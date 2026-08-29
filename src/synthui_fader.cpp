/* synthui_fader.cpp - SynthUI Fader, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT
 *
 * Clean-room build from the written description of Fader.dc.html in the
 * design spec (evkb docs/superpowers/specs/2026-08-29-synthui-fader-design.md
 * section 4).  All geometry is evaluated in viewBox-unit space (width 100,
 * height vh = 100*H/W, u = W/100 px per unit) and rounded to px only at
 * draw time -- identically in full and delta paints, because both run this
 * one draw function.
 *
 * Delta damage (spec section 7): set_value invalidates ONE rect, the union
 * of the old and new cap extents (pure vertical motion, so the union is
 * exact).  Correctness of the partial repaint is proved per boot by the
 * consuming gate's delta-equality guard, not assumed here. */
#include "synthui_fader.h"
#include "synthui_fader_math.h"
/* lv_obj_t by value needs the complete private type (the rotary's note). */
#include <lvgl_private.h>
#include <math.h>

#define MY_CLASS (&synthui_fader_class)

typedef struct {
    lv_obj_t obj;
    float value;          /* 0..1; 1 = cap at the top */
    float press_anchor;   /* value at PRESSED (anchor-total drag) */
    float press_y;        /* screen y at PRESSED */
    uint32_t panel;
    uint8_t ticks;
    bool center;
} synthui_fader_t;

/* value-independent geometry, all in viewBox units */
typedef struct {
    float u;       /* px per unit */
    float vh;      /* viewBox height in units */
    float cap_h, top, travel;
} fd_geom_t;

static void fd_constructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void fd_event(const lv_obj_class_t *cls, lv_event_t *e);
static void fd_input_pressed(lv_event_t *e);
static void fd_input_pressing(lv_event_t *e);
static void fd_input_state(lv_event_t *e);

const lv_obj_class_t synthui_fader_class = {
    .base_class     = &lv_obj_class,
    .constructor_cb = fd_constructor,
    .event_cb       = fd_event,
    /* designators must follow lv_obj_class_private.h declaration order --
     * name declares before width_def (the rotary's note). */
    .name           = "synthui_fader",
    .width_def      = 78,       /* the DC defaults */
    .height_def     = 210,
    .instance_size  = sizeof(synthui_fader_t),
};

lv_obj_t *synthui_fader_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&synthui_fader_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

static void fd_constructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    synthui_fader_t *f = (synthui_fader_t *)obj;
    f->value = 0.5f;                       /* DC default */
    f->press_anchor = 0.5f;
    f->press_y = 0.0f;
    f->panel = SYNTHUI_FADER_PANEL_DEFAULT;
    f->ticks = 13;
    f->center = false;
    /* same scroll rationale as the rotary (and lv_slider): a vertical drag
     * inside a scrollable container would move the value AND scroll. */
    lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE |
                                            LV_OBJ_FLAG_SCROLL_CHAIN_VER));
    lv_obj_add_event_cb(obj, fd_input_pressed,  LV_EVENT_PRESSED,    NULL);
    lv_obj_add_event_cb(obj, fd_input_pressing, LV_EVENT_PRESSING,   NULL);
    lv_obj_add_event_cb(obj, fd_input_state,    LV_EVENT_PRESSED,    NULL);
    lv_obj_add_event_cb(obj, fd_input_state,    LV_EVENT_RELEASED,   NULL);
    lv_obj_add_event_cb(obj, fd_input_state,    LV_EVENT_PRESS_LOST, NULL);
}

/* false = degenerate size (avoids the W=0 division); value stays settable,
 * drawing and delta invalidation are skipped. */
static bool fd_geom(const synthui_fader_t *f, fd_geom_t *g, lv_area_t *coords)
{
    lv_obj_get_coords((lv_obj_t *)&f->obj, coords);
    const float W = (float)lv_area_get_width(coords);
    const float H = (float)lv_area_get_height(coords);
    if (W < 1.0f || H < 1.0f) return false;
    g->u = W / 100.0f;
    g->vh = 100.0f * H / W;
    g->cap_h = fmaxf(14.0f, 0.11f * g->vh);
    g->top = 0.06f * g->vh;
    g->travel = g->vh - 2.0f * g->top - g->cap_h;
    if (g->travel < 0.0f) g->travel = 0.0f;
    return true;
}

static float fd_cap_y(const fd_geom_t *g, float value)
{
    return g->top + (1.0f - value) * g->travel;
}

/* Cap extent (spec section 7): union of the stroked body and the offset
 * shadow -- x 3.2..94 units, y capY-0.8 .. capY+capH+2.5 -- rounded outward
 * and inflated 2 px.  This is the ONLY damage a value change produces. */
static bool fd_cap_extent(const synthui_fader_t *f, float value, lv_area_t *a)
{
    lv_area_t coords; fd_geom_t g;
    if (!fd_geom(f, &g, &coords)) return false;
    const float cy = fd_cap_y(&g, value);
    a->x1 = coords.x1 + (int32_t)floorf(3.2f * g.u) - 2;
    a->x2 = coords.x1 + (int32_t)ceilf(94.0f * g.u) + 2;
    a->y1 = coords.y1 + (int32_t)floorf((cy - 0.8f) * g.u) - 2;
    a->y2 = coords.y1 + (int32_t)ceilf((cy + g.cap_h + 2.5f) * g.u) + 2;
    return true;
}

void synthui_fader_set_value(lv_obj_t *obj, float v01)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_fader_t *f = (synthui_fader_t *)obj;
    if (!(v01 == v01)) return;             /* NaN ignored (spec section 11) */
    if (v01 < 0.0f) v01 = 0.0f;
    if (v01 > 1.0f) v01 = 1.0f;
    if (f->value == v01) return;
    lv_area_t a_old, a_new;
    const bool ok = fd_cap_extent(f, f->value, &a_old) &&
                    fd_cap_extent(f, v01, &a_new);
    f->value = v01;
    if (!ok) return;                       /* degenerate size: value only */
    lv_area_t un = { LV_MIN(a_old.x1, a_new.x1), LV_MIN(a_old.y1, a_new.y1),
                     LV_MAX(a_old.x2, a_new.x2), LV_MAX(a_old.y2, a_new.y2) };
    lv_obj_invalidate_area(obj, &un);
}

float synthui_fader_get_value(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    return ((const synthui_fader_t *)obj)->value;
}

#define FD_SETTER(obj, field, val) do { \
    synthui_fader_t *f = (synthui_fader_t *)obj; \
    if (f->field == (val)) return; \
    f->field = (val); \
    lv_obj_invalidate(obj); } while (0)

void synthui_fader_set_ticks(lv_obj_t *obj, uint8_t n)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (n < 2) n = 2;
    if (n > 33) n = 33;
    FD_SETTER(obj, ticks, n);
}
void synthui_fader_set_center(lv_obj_t *obj, bool on)
{ LV_ASSERT_OBJ(obj, MY_CLASS); FD_SETTER(obj, center, on); }
void synthui_fader_set_panel(lv_obj_t *obj, uint32_t rgb)
{ LV_ASSERT_OBJ(obj, MY_CLASS); FD_SETTER(obj, panel, rgb); }

/* ---- palette (spec section 5) -- one pure function so a future second
 * engine shares it (the rotary's two-engines-one-palette seam, prepared
 * but not built) ---- */
typedef struct {
    uint32_t cap_top, cap_mid, cap_low, ticks, center;
    lv_opa_t gloss_opa;
} synthui_fader_palette_t;

static void fd_palette(bool disabled, bool pressed, synthui_fader_palette_t *p)
{
    p->cap_top = disabled ? 0xD2D5D4 : (pressed ? 0xFFFFFF : 0xF4F5F4);
    p->cap_mid = disabled ? 0xB4B8B8 : 0xDCDEDD;
    p->cap_low = disabled ? 0x9AA0A1 : (pressed ? 0xC8CBCA : 0xB6BABA);
    p->ticks   = disabled ? 0xC8CDD0 : 0xE8EEF0;
    p->center  = disabled ? 0x8F9598 : 0x20262A;
    p->gloss_opa = disabled ? 77 : 191;    /* 0.30 / 0.75 of 255 */
}

/* ---- input layer (spec section 8) ---- */
static void fd_input_pressed(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    synthui_fader_t *f = (synthui_fader_t *)obj;
    lv_indev_t *indev = lv_event_get_indev(e);
    if (indev == NULL) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    f->press_anchor = f->value;
    f->press_y = (float)p.y;
}

static void fd_input_pressing(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    synthui_fader_t *f = (synthui_fader_t *)obj;
    lv_indev_t *indev = lv_event_get_indev(e);
    if (indev == NULL) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_area_t coords; fd_geom_t g;
    if (!fd_geom(f, &g, &coords)) return;
    const float next = synthui_fader_drag(f->press_anchor,
                                          f->press_y - (float)p.y,
                                          g.travel * g.u);
    if (next == f->value) return;
    synthui_fader_set_value(obj, next);
    lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
}

/* Press/release changes ONLY cap colors (spec section 3), so the state
 * repaint is the cap extent, not the whole widget. */
static void fd_input_state(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    synthui_fader_t *f = (synthui_fader_t *)obj;
    lv_area_t a;
    if (fd_cap_extent(f, f->value, &a)) lv_obj_invalidate_area(obj, &a);
}

/* ---- drawing (spec sections 4 & 6) ---- */

/* Rect in unit space; x2/y2 are LVGL-inclusive, hence the -1. */
static void fd_rect(lv_layer_t *layer, const lv_draw_rect_dsc_t *d,
                    float x0, float y0, float x, float y, float w, float h,
                    float u)
{
    lv_area_t a = { (int32_t)lroundf(x0 + x * u),
                    (int32_t)lroundf(y0 + y * u),
                    (int32_t)lroundf(x0 + (x + w) * u) - 1,
                    (int32_t)lroundf(y0 + (y + h) * u) - 1 };
    lv_draw_rect(layer, d, &a);
}

static void fd_grad_rect(lv_layer_t *layer, float x0, float y0, float x,
                         float y, float w, float h, float u,
                         uint32_t c_top, uint32_t c_bot)
{
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_COVER;
    d.bg_grad.dir = LV_GRAD_DIR_VER;
    d.bg_grad.stops_count = 2;
    d.bg_grad.stops[0].color = lv_color_hex(c_top);
    d.bg_grad.stops[0].opa = LV_OPA_COVER;
    d.bg_grad.stops[0].frac = 0;
    d.bg_grad.stops[1].color = lv_color_hex(c_bot);
    d.bg_grad.stops[1].opa = LV_OPA_COVER;
    d.bg_grad.stops[1].frac = 255;
    fd_rect(layer, &d, x0, y0, x, y, w, h, u);
}

static void fd_hline(lv_layer_t *layer, float x0, float y0, float xa,
                     float xb, float y, float w_units, float u,
                     uint32_t hex, lv_opa_t opa)
{
    lv_draw_line_dsc_t l; lv_draw_line_dsc_init(&l);
    l.color = lv_color_hex(hex);
    l.opa = opa;
    l.width = (int32_t)lroundf(w_units * u);
    if (l.width < 1) l.width = 1;
    l.p1.x = x0 + xa * u; l.p1.y = y0 + y * u;
    l.p2.x = x0 + xb * u; l.p2.y = y0 + y * u;
    lv_draw_line(layer, &l);
}

static void fd_draw(synthui_fader_t *f, lv_layer_t *layer)
{
    lv_area_t c; fd_geom_t g;
    if (!fd_geom(f, &g, &c)) return;
    const float u = g.u;
    const float x0 = (float)c.x1, y0 = (float)c.y1;
    const lv_state_t st = lv_obj_get_state(&f->obj);
    synthui_fader_palette_t pal;
    fd_palette((st & LV_STATE_DISABLED) != 0,
               (st & LV_STATE_PRESSED) != 0, &pal);

    /* panel */
    {
        lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
        d.bg_color = lv_color_hex(f->panel); d.bg_opa = LV_OPA_COVER;
        lv_draw_rect(layer, &d, &c);
    }

    /* ticks: x 8..92, every 4th brighter (0.62 vs 0.34 -> 158 vs 87) */
    {
        const float tick_w = fmaxf(1.4f, 0.012f * g.vh);
        const int n = f->ticks;
        for (int i = 0; i < n; i++) {
            const float ty = g.top + g.cap_h * 0.5f
                             + (float)i * g.travel / (float)(n - 1);
            fd_hline(layer, x0, y0, 8.0f, 92.0f, ty, tick_w, u,
                     pal.ticks, (i % 4 == 0) ? 158 : 87);
        }
    }

    /* rod (the slot): x 46.5 w 7, r 1.5, spanning the cap-center travel +-2 */
    {
        lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
        d.bg_color = lv_color_hex(0x14181B); d.bg_opa = LV_OPA_COVER;
        d.radius = (int32_t)lroundf(1.5f * u);
        fd_rect(layer, &d, x0, y0, 46.5f, g.top + g.cap_h * 0.5f - 2.0f,
                7.0f, g.travel + 4.0f, u);
    }

    /* center-detent line (option) */
    if (f->center)
        fd_hline(layer, x0, y0, 4.0f, 96.0f,
                 g.top + g.cap_h * 0.5f + g.travel * 0.5f, 2.4f, u,
                 pal.center, LV_OPA_COVER);

    /* cap */
    {
        const float cy = fd_cap_y(&g, f->value);
        const float ch = g.cap_h;
        const float bw = 1.6f;             /* body stroke, units */

        /* shadow: solid dark rect at +2/+2.5, 45% (the DC uses no blur) */
        lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
        d.bg_color = lv_color_hex(0x1B1F22); d.bg_opa = 115;
        d.radius = (int32_t)lroundf(2.0f * u);
        fd_rect(layer, &d, x0, y0, 6.0f, cy + 2.5f, 88.0f, ch, u);

        /* body base: capMid fill + stroke; the gradients are inset inside
         * the border with square corners -- the corner pixels keep capMid,
         * sub-pixel at r ~= 1.6 px (spec section 6) */
        lv_draw_rect_dsc_init(&d);
        d.bg_color = lv_color_hex(pal.cap_mid); d.bg_opa = LV_OPA_COVER;
        d.radius = (int32_t)lroundf(2.0f * u);
        d.border_color = lv_color_hex(0x20262A);
        d.border_opa = LV_OPA_COVER;
        d.border_width = (int32_t)lroundf(bw * u);
        if (d.border_width < 1) d.border_width = 1;
        fd_rect(layer, &d, x0, y0, 4.0f, cy, 88.0f, ch, u);

        /* 3-stop gradient as two stacked 2-stop rects, split at 0.46 */
        fd_grad_rect(layer, x0, y0, 4.0f + bw, cy + bw,
                     88.0f - 2.0f * bw, 0.46f * ch - bw, u,
                     pal.cap_top, pal.cap_mid);
        fd_grad_rect(layer, x0, y0, 4.0f + bw, cy + 0.46f * ch,
                     88.0f - 2.0f * bw, 0.54f * ch - bw, u,
                     pal.cap_mid, pal.cap_low);

        /* groove across the middle: y 0.43..0.57 of capH */
        lv_draw_rect_dsc_init(&d);
        d.bg_color = lv_color_hex(0x20262A); d.bg_opa = LV_OPA_COVER;
        fd_rect(layer, &d, x0, y0, 4.0f, cy + 0.43f * ch, 88.0f,
                0.14f * ch, u);

        /* two gloss strips */
        const float gh = fmaxf(1.5f, 0.12f * ch);
        lv_draw_rect_dsc_init(&d);
        d.bg_color = lv_color_hex(0xFFFFFF); d.bg_opa = pal.gloss_opa;
        fd_rect(layer, &d, x0, y0, 9.0f, cy + 0.16f * ch, 78.0f, gh, u);
        fd_rect(layer, &d, x0, y0, 9.0f, cy + 0.68f * ch, 78.0f, gh, u);
    }
}

static void fd_event(const lv_obj_class_t *cls, lv_event_t *e)
{
    LV_UNUSED(cls);
    if (lv_obj_event_base(MY_CLASS, e) != LV_RESULT_OK) return;
    if (lv_event_get_code(e) == LV_EVENT_DRAW_MAIN)
        fd_draw((synthui_fader_t *)lv_event_get_current_target_obj(e),
                lv_event_get_layer(e));
}
