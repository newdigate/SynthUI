/* synthui_rotary_knob.cpp - SynthUI RotaryKnob (notch), LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT
 *
 * Clean-room port of RotaryKnob.dc.html renderVals() (design project
 * 79ec272e, fetched 2026-08-27): 0..100 viewBox scaled to min(w,h), 0 deg =
 * 12 o'clock, clockwise. Notch variant only (NEW-20 Phase-2 scope decision);
 * geometry identical to the bench's rk_geometry, whose silicon numbers chose
 * the render strategy. Input layer carried over from synthui_knob verbatim.
 *
 * SVG-vs-LVGL divergence, accepted and golden-absorbed (bench spec section 6):
 * SVG strokes straddle the radius, LVGL borders sit inside it; the r43-centred
 * track is drawn as outer radius 44.5, width 3. */
#include "synthui_rotary_knob.h"
#include "synthui_rotary_knob_private.h"
#include "synthui_knob_math.h"
#include <math.h>

#define RK_DEG (3.14159265358979f / 180.0f)
#define MY_CLASS (&synthui_rotary_knob_class)

synthui_rotary_knob_t *synthui_rotary_knob_list = NULL;
bool synthui_rotary_gpu_enabled = false;

static void rk_constructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void rk_destructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void rk_event(const lv_obj_class_t *cls, lv_event_t *e);
static void rk_input_pressed(lv_event_t *e);
static void rk_input_pressing(lv_event_t *e);
static void rk_input_state(lv_event_t *e);

const lv_obj_class_t synthui_rotary_knob_class = {
    .base_class     = &lv_obj_class,
    .constructor_cb = rk_constructor,
    .destructor_cb  = rk_destructor,
    .event_cb       = rk_event,
    /* designators must follow lv_obj_class_private.h declaration order --
     * name declares before width_def (the old knob's note). */
    .name           = "synthui_rotary_knob",
    .width_def      = 120,
    .height_def     = 120,
    .instance_size  = sizeof(synthui_rotary_knob_t),
};

lv_obj_t *synthui_rotary_knob_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&synthui_rotary_knob_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

static void rk_constructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    synthui_rotary_knob_t *k = (synthui_rotary_knob_t *)obj;
    k->angle = 0.0f;
    k->min_deg = -150.0f; k->max_deg = 150.0f;   /* DC defaults */
    k->detent_step = 0.0f;
    k->drag_pos = 0.0f;
    k->accent = SYNTHUI_ROTARY_ACCENT_DEFAULT;
    k->mode = SYNTHUI_ROTARY_MODE_ENDLESS;
    k->theme = SYNTHUI_ROTARY_THEME_LIGHT;
    k->gpu_pending = false;
    /* head-insert into the registry */
    k->prev = NULL;
    k->next = synthui_rotary_knob_list;
    if (k->next) k->next->prev = k;
    synthui_rotary_knob_list = k;
    /* same scroll rationale as the old knob (and lv_slider): PRESSING is
     * delivered and THEN the scroll handler walks the parent chain, so a
     * vertical drag inside a scrollable container would turn AND scroll. */
    lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE |
                                            LV_OBJ_FLAG_SCROLL_CHAIN_VER));
    lv_obj_add_event_cb(obj, rk_input_pressed,  LV_EVENT_PRESSED,    NULL);
    lv_obj_add_event_cb(obj, rk_input_pressing, LV_EVENT_PRESSING,   NULL);
    lv_obj_add_event_cb(obj, rk_input_state,    LV_EVENT_PRESSED,    NULL);
    lv_obj_add_event_cb(obj, rk_input_state,    LV_EVENT_RELEASED,   NULL);
    lv_obj_add_event_cb(obj, rk_input_state,    LV_EVENT_PRESS_LOST, NULL);
}

static void rk_destructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    synthui_rotary_knob_t *k = (synthui_rotary_knob_t *)obj;
    if (k->prev) k->prev->next = k->next;
    else synthui_rotary_knob_list = k->next;
    if (k->next) k->next->prev = k->prev;
    k->prev = k->next = NULL;
}

#define RK_SETTER(obj, field, val) do { \
    synthui_rotary_knob_t *k = (synthui_rotary_knob_t *)obj; \
    if (k->field == (val)) return; \
    k->field = (val); \
    lv_obj_invalidate(obj); } while (0)

/* Bbox of the +/-8 deg index wedge (ring sector r16..36) at `deg`, padded
 * 4 px for AA. 5 samples across the 16 deg span at both radii bound the arc
 * to <0.1 px at every supported size, and the pad is constant because AA
 * is. NOTCH-ONLY property: the discs are rotationally invariant, so this
 * box is the EXACT damage of an angle change (delta-damage spec section 2).
 * A future variant whose rotor is not rotationally symmetric outside the
 * wedge must NOT take the delta path -- the equality guard in
 * synthui_knob_test is what catches that mistake. */
static void wedge_bbox(const synthui_rotary_knob_t *k, float deg,
                       lv_area_t *a)
{
    lv_area_t coords; lv_obj_get_coords((lv_obj_t *)&k->obj, &coords);
    const float W = (float)lv_area_get_width(&coords);
    const float H = (float)lv_area_get_height(&coords);
    const float S = (W < H ? W : H) / 100.0f;
    const float cx = (float)coords.x1 + W * 0.5f;
    const float cy = (float)coords.y1 + H * 0.5f;
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    for (int i = 0; i <= 4; i++) {
        const float d = deg - 8.0f + 4.0f * (float)i;
        for (int r = 0; r < 2; r++) {
            const float rad = (r ? 36.0f : 16.0f) * S;
            const float x = cx + rad * sinf(d * RK_DEG);
            const float y = cy - rad * cosf(d * RK_DEG);
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
        }
    }
    a->x1 = (int32_t)minx - 4; a->y1 = (int32_t)miny - 4;
    a->x2 = (int32_t)maxx + 4; a->y2 = (int32_t)maxy + 4;
}

void synthui_rotary_knob_set_angle(lv_obj_t *obj, float deg)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_rotary_knob_t *k = (synthui_rotary_knob_t *)obj;
    if (k->angle == deg) return;
    /* Wedge-delta damage, BOTH engines: an angle change moves ONLY the
     * index wedge (the discs are rotationally invariant), so damage the old
     * and new wedge boxes instead of the whole control. sw: LVGL clips the
     * draw callback, exact by construction. gpu: the compositor re-renders
     * well+rotor scissored to the rendered areas -- proven pixel-identical
     * to a fresh full render by the delta equality guard, three boots
     * bit-stable at 42.4 fps on the all-16 worst case (2026-08-28).
     * ★ That guard is LOAD-BEARING history, not ceremony: the gpu delta
     * path failed it TWICE before shipping -- rotated-Bezier disc AA, then
     * a boot-random fill inversion that turned out to be the OLD
     * multi-subpath track poisoning tessellation state (see emit_track in
     * the gpu TU). Any change here or in the compositor must hold the guard
     * on silicon across REPEATED boots -- single-boot evidence was what let
     * the second defect hide. */
    lv_area_t a;
    wedge_bbox(k, k->angle, &a);
    lv_obj_invalidate_area(obj, &a);
    wedge_bbox(k, deg, &a);
    k->angle = deg;
    lv_obj_invalidate_area(obj, &a);
}
void synthui_rotary_knob_set_mode(lv_obj_t *obj, synthui_rotary_mode_t m)
{ LV_ASSERT_OBJ(obj, MY_CLASS); RK_SETTER(obj, mode, m); }
void synthui_rotary_knob_set_theme(lv_obj_t *obj, synthui_rotary_theme_t t)
{ LV_ASSERT_OBJ(obj, MY_CLASS); RK_SETTER(obj, theme, t); }
void synthui_rotary_knob_set_accent(lv_obj_t *obj, uint32_t rgb_hex)
{ LV_ASSERT_OBJ(obj, MY_CLASS); RK_SETTER(obj, accent, rgb_hex); }
void synthui_rotary_knob_set_detent_step(lv_obj_t *obj, float deg)
{ LV_ASSERT_OBJ(obj, MY_CLASS); RK_SETTER(obj, detent_step, deg < 0.0f ? 0.0f : deg); }
void synthui_rotary_knob_set_range(lv_obj_t *obj, float min_deg, float max_deg)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_rotary_knob_t *k = (synthui_rotary_knob_t *)obj;
    if (k->min_deg == min_deg && k->max_deg == max_deg) return;
    k->min_deg = min_deg; k->max_deg = max_deg;
    lv_obj_invalidate(obj);
}
float synthui_rotary_knob_get_angle(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    return ((const synthui_rotary_knob_t *)obj)->angle;
}

void synthui_rotary_knob_palette(const synthui_rotary_knob_t *k,
                                 synthui_rotary_palette_t *p)
{
    const lv_state_t st = lv_obj_get_state((const lv_obj_t *)&k->obj);
    synthui_rotary_palette(k->theme == SYNTHUI_ROTARY_THEME_DARK,
                           (st & LV_STATE_DISABLED) != 0,
                           (st & LV_STATE_PRESSED)  != 0,
                           (st & LV_STATE_FOCUSED)  != 0,
                           k->accent, p);
}

/* ---- input layer: verbatim port of synthui_knob's ---- */

/* Seeded HERE, not in the constructor, so a knob whose angle was set
 * programmatically between touches starts the next drag from where it is
 * actually pointing. */
static void rk_input_pressed(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    synthui_rotary_knob_t *k = (synthui_rotary_knob_t *)obj;
    k->drag_pos = k->angle;
}

/* drag_pos is UNSNAPPED and the snap applies only to what is displayed --
 * quantising the accumulator is a knob that cannot be turned slowly (the
 * knob_math header carries the measurement). */
static void rk_input_pressing(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    lv_indev_t *indev = lv_event_get_indev(e);
    if (indev == NULL) return;
    lv_point_t vect;
    lv_indev_get_vect(indev, &vect);
    if (vect.y == 0) return;

    synthui_rotary_knob_t *k = (synthui_rotary_knob_t *)obj;
    k->drag_pos = synthui_knob_drag(k->drag_pos, k->min_deg, k->max_deg, vect.y);
    /* detent snap applies whenever a step is set (the old DETENTS mode is a
     * behavior here, not a visual mode) */
    const float next = synthui_knob_snap(k->drag_pos, k->min_deg, k->max_deg,
                                         k->detent_step);
    if (next == k->angle) return;    /* mid-detent: moved, nothing to show */
    synthui_rotary_knob_set_angle(obj, next);
    lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
}

/* Palette changes on PRESSED/RELEASED/PRESS_LOST; no local styles, so LVGL's
 * style refresh finds SAME and never repaints on its own. */
static void rk_input_state(lv_event_t *e)
{
    lv_obj_invalidate(lv_event_get_current_target_obj(e));
}

/* ---- drawing ---- */
static void draw_disc(lv_layer_t *l, float x, float y, float rpx,
                      const lv_draw_rect_dsc_t *dsc)
{
    lv_area_t a = { (int32_t)lroundf(x - rpx), (int32_t)lroundf(y - rpx),
                    (int32_t)lroundf(x + rpx), (int32_t)lroundf(y + rpx) };
    lv_draw_rect(l, dsc, &a);
}

/* Annulus sector; radius names the OUTER edge, width extends inward. The
 * fold is the old knob's lesson: LVGL's sw arc clamps a negative start to 0
 * and renders a truncated wedge, so fold the start into [0,360) and carry
 * the span. */
static void draw_arc_seg(lv_layer_t *layer, float cx, float cy, float S,
                         float r_outer, float w, float a1, float a2,
                         uint32_t hex, bool rounded)
{
    lv_draw_arc_dsc_t a; lv_draw_arc_dsc_init(&a);
    a.center.x = (int32_t)lroundf(cx); a.center.y = (int32_t)lroundf(cy);
    a.radius = (uint16_t)lroundf(r_outer * S);
    a.width  = (int32_t)lroundf(w * S); if (a.width < 1) a.width = 1;
    float span = a2 - a1;
    if (span <= 0.0f) return;
    if (span > 360.0f) span = 360.0f;
    float s0 = fmodf(a1 - 90.0f, 360.0f);   /* LVGL measures from 3 o'clock */
    if (s0 < 0.0f) s0 += 360.0f;
    a.start_angle = (lv_value_precise_t)s0;
    a.end_angle   = (lv_value_precise_t)(s0 + span);
    a.color = lv_color_hex(hex); a.opa = LV_OPA_COVER;
    a.rounded = rounded ? 1 : 0;
    lv_draw_arc(layer, &a);
}

static void rk_draw(synthui_rotary_knob_t *k, lv_layer_t *layer)
{
    /* GPU mode: the compositor draws EVERYTHING (well + rotor) per rendered
     * area -- moving the well off the CPU is the measured 30 fps lever (the
     * clipped well repaints were the entire 38 ms residual at delta damage,
     * gpu-well spec section 1). DRAW_MAIN only marks the instance; LVGL
     * paints the screen ground beneath. sw mode is untouched, so every QEMU
     * golden and the sw delta guards stand. */
    if (synthui_rotary_gpu_enabled) { k->gpu_pending = true; return; }

    lv_obj_t *obj = &k->obj;
    lv_area_t coords; lv_obj_get_coords(obj, &coords);
    const float W = (float)lv_area_get_width(&coords);
    const float H = (float)lv_area_get_height(&coords);
    const float S = (W < H ? W : H) / 100.0f;
    const float cx = (float)coords.x1 + W * 0.5f;
    const float cy = (float)coords.y1 + H * 0.5f;

    synthui_rotary_palette_t pal;
    synthui_rotary_knob_palette(k, &pal);
    const lv_state_t st = lv_obj_get_state(obj);
    const bool focus = (st & LV_STATE_FOCUSED) && !(st & LV_STATE_DISABLED);

    /* well: r39 disc; ring mode strokes it (focus: index color, w3),
     * bounded mode leaves it strokeless and adds the r43-centred track */
    {
        lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
        d.radius = LV_RADIUS_CIRCLE;
        d.bg_color = lv_color_hex(pal.well); d.bg_opa = LV_OPA_COVER;
        if (k->mode != SYNTHUI_ROTARY_MODE_BOUNDED) {
            d.border_color = lv_color_hex(pal.well_stroke);
            d.border_opa = LV_OPA_COVER;
            d.border_width = (int32_t)lroundf((focus ? 3.0f : 1.6f) * S);
            if (d.border_width < 1) d.border_width = 1;
        }
        draw_disc(layer, cx, cy, 39.0f * S, &d);
    }
    if (k->mode == SYNTHUI_ROTARY_MODE_BOUNDED)
        draw_arc_seg(layer, cx, cy, S, 44.5f, 3.0f, k->min_deg, k->max_deg,
                     pal.well_stroke, true);

    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.radius = LV_RADIUS_CIRCLE; d.bg_opa = LV_OPA_COVER;
    d.bg_color = lv_color_hex(pal.body);
    draw_disc(layer, cx, cy, 36.0f * S, &d);
    d.bg_color = lv_color_hex(pal.inner);
    draw_disc(layer, cx, cy, 27.0f * S, &d);
    draw_arc_seg(layer, cx, cy, S, 36.0f, 20.0f,
                 k->angle - 8.0f, k->angle + 8.0f, pal.index, false);
}

static void rk_event(const lv_obj_class_t *cls, lv_event_t *e)
{
    LV_UNUSED(cls);
    if (lv_obj_event_base(MY_CLASS, e) != LV_RESULT_OK) return;
    if (lv_event_get_code(e) == LV_EVENT_DRAW_MAIN)
        rk_draw((synthui_rotary_knob_t *)lv_event_get_current_target_obj(e),
                lv_event_get_layer(e));
}
