/* synthui_knob.cpp - SynthUI rotary knob, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT
 *
 * Clean-room port of the DC Knob's renderVals() MATH (SynthUI
 * reference/dc/Knob.dc.html): geometry in a 0..100 viewBox scaled to
 * min(w,h).  v1 shading is deliberately flat (spec 2026-08-15-synthui-knob
 * section 5): one native vertical gradient on the face; the crescent's SVG
 * gradient is replaced by angle-driven luminance (section 5.3). */
#include "synthui_knob.h"
/* Out-of-tree widget: the class struct literal and the by-value lv_obj_t in
 * synthui_knob_t both need the COMPLETE private types.  LVGL 9's sanctioned
 * umbrella for that is lvgl_private.h (LV_USE_PRIVATE_API stays 0). */
#include <lvgl_private.h>
#include <math.h>

#define KNOB_DEG (3.14159265358979f / 180.0f)
#define MY_CLASS (&synthui_knob_class)

typedef struct {
    lv_obj_t obj;
    float angle, sweep, min_deg, max_deg, detent_step;
    uint8_t tick_count;
    synthui_knob_mode_t mode;
} synthui_knob_t;

static void knob_constructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void knob_event(const lv_obj_class_t *cls, lv_event_t *e);

const lv_obj_class_t synthui_knob_class = {
    .base_class     = &lv_obj_class,
    .constructor_cb = knob_constructor,
    .event_cb       = knob_event,
    /* upstream's class-literal template lists .name last; C++ requires
     * designators in declaration order, and name declares before width_def
     * (lv_obj_class_private.h), so it has to go here instead. */
    .name           = "synthui_knob",
    .width_def      = 120,
    .height_def     = 120,
    .instance_size  = sizeof(synthui_knob_t),
};

lv_obj_t *synthui_knob_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&synthui_knob_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

static void knob_constructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    synthui_knob_t *k = (synthui_knob_t *)obj;
    k->angle = 0.0f; k->sweep = 215.0f;
    k->min_deg = -140.0f; k->max_deg = 140.0f;
    k->detent_step = 35.0f; k->tick_count = 8;
    k->mode = SYNTHUI_KNOB_MODE_ENDLESS;
    lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
}

#define KNOB_SETTER(obj, field, val) do { \
    synthui_knob_t *k = (synthui_knob_t *)obj; \
    if (k->field == (val)) return; \
    k->field = (val); \
    lv_obj_invalidate(obj); } while (0)

void synthui_knob_set_angle(lv_obj_t *obj, float deg)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    KNOB_SETTER(obj, angle, deg);
}
void synthui_knob_set_mode(lv_obj_t *obj, synthui_knob_mode_t m)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    KNOB_SETTER(obj, mode, m);
}
void synthui_knob_set_sweep(lv_obj_t *obj, float deg)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    /* renderVals(): Math.max(30, Math.min(340, sweep)) */
    KNOB_SETTER(obj, sweep, deg < 30.0f ? 30.0f : (deg > 340.0f ? 340.0f : deg));
}
void synthui_knob_set_tick_count(lv_obj_t *obj, uint8_t n)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    KNOB_SETTER(obj, tick_count, n > 24 ? 24 : n);
}
void synthui_knob_set_detent_step(lv_obj_t *obj, float deg)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    /* Floor at 1 deg: prevents pathological draw-loop counts (Task 5
     * iterates min..max by this step). */
    KNOB_SETTER(obj, detent_step, deg < 1.0f ? 1.0f : deg);
}
void synthui_knob_set_range(lv_obj_t *obj, float min_deg, float max_deg)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_knob_t *k = (synthui_knob_t *)obj;
    if (k->min_deg == min_deg && k->max_deg == max_deg) return;
    k->min_deg = min_deg; k->max_deg = max_deg;
    lv_obj_invalidate(obj);
}
float synthui_knob_get_angle(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    return ((const synthui_knob_t *)obj)->angle;
}

static void knob_draw(synthui_knob_t *k, lv_layer_t *layer); /* Task 5 */

static void knob_event(const lv_obj_class_t *cls, lv_event_t *e)
{
    LV_UNUSED(cls);
    if (lv_obj_event_base(MY_CLASS, e) != LV_RESULT_OK) return;
    /* Touch handling lands later; must invalidate on LV_EVENT_PRESSED/RELEASED/PRESS_LOST/FOCUSED/DEFOCUSED then. */
    if (lv_event_get_code(e) == LV_EVENT_DRAW_MAIN)
        knob_draw((synthui_knob_t *)lv_event_get_current_target_obj(e),
                  lv_event_get_layer(e));
}

typedef struct {
    lv_color_t ring, tick, detent, stop, track, arc,
               face_from, face_to, cres_from, cres_to, pointer, cap;
    uint8_t gopa;                    /* SVG group alpha: 166 disabled, else 255 */
} knob_palette_t;

/* Hexes verbatim from renderVals(); DC 'state' maps onto LVGL states
 * (disabled wins, then active/pressed; focus only recolors the ring). */
static void knob_palette(lv_state_t st, knob_palette_t *p)
{
    const bool dis = st & LV_STATE_DISABLED;
    const bool act = (st & LV_STATE_PRESSED) && !dis;
    const bool foc = (st & LV_STATE_FOCUSED) && !dis;
    const lv_color_t ink = lv_color_hex(dis ? 0x8b8b93 : 0x2b2e5c);
    p->ring      = foc ? lv_color_hex(0x5b62b8) : ink;
    p->tick      = lv_color_hex(dis ? 0xb6b6bd : 0x3a3d6b);
    p->detent    = lv_color_hex(dis ? 0xb0b0b8 : 0x6f74bd);
    p->stop      = lv_color_hex(dis ? 0x8e8e98 : 0xc2543f);
    p->track     = lv_color_hex(dis ? 0xdcdce1 : 0xd8d9e8);
    p->arc       = lv_color_hex(dis ? 0xa8a8b2 : 0x5b62b8);
    p->face_from = lv_color_hex(dis ? 0xf4f4f4 : 0xfcfbf6);
    p->face_to   = lv_color_hex(dis ? 0xe6e6e9 : 0xe7e7f1);
    p->cres_from = lv_color_hex(dis ? 0xc8c8cf : (act ? 0x9aa0e0 : 0x8f96d4));
    p->cres_to   = lv_color_hex(dis ? 0x8e8e98 : 0x282b60);
    p->pointer   = lv_color_hex(dis ? 0xeeeef0 : 0xfdfdf9);
    p->cap       = lv_color_hex(dis ? 0xf2f2f4 : 0xfdfdf9);
    p->gopa      = dis ? 166 : 255;
}

/* P(r, th): 0 deg = 12 o'clock, clockwise -- the DC convention. */
static void polar(float cx, float cy, float S, float r, float deg,
                  lv_point_precise_t *out)
{
    out->x = (lv_value_precise_t)(cx + r * S * sinf(deg * KNOB_DEG));
    out->y = (lv_value_precise_t)(cy - r * S * cosf(deg * KNOB_DEG));
}

static void draw_ray(lv_layer_t *layer, float cx, float cy, float S,
                     float r1, float r2, float deg,
                     lv_color_t color, float w, uint8_t opa)
{
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d);
    polar(cx, cy, S, r1, deg, &d.p1);
    polar(cx, cy, S, r2, deg, &d.p2);
    d.color = color; d.opa = opa;
    d.width = (int32_t)lroundf(w * S); if (d.width < 1) d.width = 1;
    lv_draw_line(layer, &d);
}

/* LVGL arcs are annulus sectors and measure from 3 o'clock: lv = dc - 90. */
static void draw_arc_seg(lv_layer_t *layer, float cx, float cy, float S,
                         float r, float w, float a1, float a2,
                         lv_color_t color, uint8_t opa)
{
    lv_draw_arc_dsc_t a; lv_draw_arc_dsc_init(&a);
    a.center.x = (int32_t)lroundf(cx); a.center.y = (int32_t)lroundf(cy);
    a.radius = (uint16_t)lroundf(r * S);
    a.width  = (int32_t)lroundf(w * S); if (a.width < 1) a.width = 1;
    a.start_angle = (lv_value_precise_t)(a1 - 90.0f);
    a.end_angle   = (lv_value_precise_t)(a2 - 90.0f);
    a.color = color; a.opa = opa;
    lv_draw_arc(layer, &a);
}

static void draw_disc(lv_layer_t *layer, float x, float y, float rpx,
                      const lv_draw_rect_dsc_t *dsc)
{
    lv_area_t a = { (int32_t)lroundf(x - rpx), (int32_t)lroundf(y - rpx),
                    (int32_t)lroundf(x + rpx), (int32_t)lroundf(y + rpx) };
    lv_draw_rect(layer, (lv_draw_rect_dsc_t *)dsc, &a);
}

static void knob_draw(synthui_knob_t *k, lv_layer_t *layer)
{
    lv_obj_t *obj = &k->obj;
    lv_area_t coords; lv_obj_get_coords(obj, &coords);
    const float W = (float)lv_area_get_width(&coords);
    const float H = (float)lv_area_get_height(&coords);
    const float S = (W < H ? W : H) / 100.0f;
    const float cx = (float)coords.x1 + W * 0.5f;
    const float cy = (float)coords.y1 + H * 0.5f;

    knob_palette_t pal; knob_palette(lv_obj_get_state(obj), &pal);
    const uint8_t g = pal.gopa;

    /* tick ring -- fixed, never rotates; decorated modes keep only the top
     * orientation marker (renderVals' effTicks) */
    const uint8_t nticks =
        (k->mode == SYNTHUI_KNOB_MODE_ENDLESS) ? k->tick_count : 1;
    for (uint8_t i = 0; i < nticks; i++) {
        const float th = (float)i * 360.0f / (float)nticks;
        const bool major = (i == 0);
        draw_ray(layer, cx, cy, S, 37.5f, major ? 49.0f : 45.5f, th,
                 pal.tick, major ? 3.4f : 2.4f, g);
    }

    if (k->mode == SYNTHUI_KNOB_MODE_DETENTS && k->detent_step > 0.0f)
        for (float a = k->min_deg; a <= k->max_deg + 0.001f; a += k->detent_step)
            draw_ray(layer, cx, cy, S, 37.5f, 44.0f, a, pal.detent, 2.0f, g);

    if (k->mode == SYNTHUI_KNOB_MODE_BOUNDED ||
        k->mode == SYNTHUI_KNOB_MODE_DETENTS) {
        draw_ray(layer, cx, cy, S, 37.0f, 49.0f, k->min_deg, pal.stop, 3.4f, g);
        draw_ray(layer, cx, cy, S, 37.0f, 49.0f, k->max_deg, pal.stop, 3.4f, g);
    }

    if (k->mode == SYNTHUI_KNOB_MODE_ARC) {
        float v = k->angle;
        if (v < k->min_deg) v = k->min_deg;
        if (v > k->max_deg) v = k->max_deg;
        draw_arc_seg(layer, cx, cy, S, 38.5f, 3.2f, k->min_deg, k->max_deg,
                     pal.track, g);
        if (v > k->min_deg)
            draw_arc_seg(layer, cx, cy, S, 38.5f, 3.2f, k->min_deg, v,
                         pal.arc, g);
    }

    /* face: r=33 disc, vertical faceFrom->faceTo, ring border 2.6 */
    {
        lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
        d.radius = LV_RADIUS_CIRCLE;
        d.bg_opa = g;
        d.bg_grad.dir = LV_GRAD_DIR_VER;
        d.bg_grad.stops[0].color = pal.face_from;
        d.bg_grad.stops[0].opa = 255; d.bg_grad.stops[0].frac = 0;
        d.bg_grad.stops[1].color = pal.face_to;
        d.bg_grad.stops[1].opa = 255; d.bg_grad.stops[1].frac = 255;
        d.bg_grad.stops_count = 2;
        d.border_color = pal.ring; d.border_opa = g;
        d.border_width = (int32_t)lroundf(2.6f * S);
        if (d.border_width < 1) d.border_width = 1;
        draw_disc(layer, cx, cy, 33.0f * S, &d);
    }

    /* crescent: annulus r=21..30 spanning sweep centred on angle; solid color,
     * angle-driven luminance (spec 5.3) -- lightest pointing at the top-left
     * light (-45 deg), darkest opposite */
    {
        const float t = (1.0f - cosf((k->angle + 45.0f) * KNOB_DEG)) * 0.5f;
        const lv_color_t c = lv_color_mix(pal.cres_to, pal.cres_from,
                                          (uint8_t)lroundf(255.0f * (1.0f - t)));
        draw_arc_seg(layer, cx, cy, S, 30.0f, 9.0f,
                     k->angle - k->sweep * 0.5f, k->angle + k->sweep * 0.5f,
                     c, g);
    }

    /* pointer dot at P(25.5, angle), r=4 */
    {
        lv_point_precise_t pt; polar(cx, cy, S, 25.5f, k->angle, &pt);
        lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
        d.radius = LV_RADIUS_CIRCLE; d.bg_color = pal.pointer; d.bg_opa = g;
        draw_disc(layer, (float)pt.x, (float)pt.y, 4.0f * S, &d);
    }

    /* cap: r=20 at SVG opacity 0.55 (140/255), scaled by group alpha */
    {
        lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
        d.radius = LV_RADIUS_CIRCLE; d.bg_color = pal.cap;
        d.bg_opa = (uint8_t)((140u * g) >> 8);
        draw_disc(layer, cx, cy, 20.0f * S, &d);
    }
}
