/* synthui_led_button.cpp - SynthUI LedButton, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#include "synthui_led_button.h"
#include "synthui_led_button_math.h"
#include <lvgl_private.h>

#define MY_CLASS (&synthui_led_button_class)

typedef struct {
    lv_obj_t obj;
    synthui_led_button_color_t color;
    bool lit;
    bool pressed;      /* the latch */
    bool cue;
    bool disabled;
} synthui_led_button_t;

static void led_constructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void led_destructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void led_event(const lv_obj_class_t *cls, lv_event_t *e);
static void led_draw(synthui_led_button_t *b, lv_layer_t *layer);

const lv_obj_class_t synthui_led_button_class = {
    .base_class     = &lv_obj_class,
    .constructor_cb = led_constructor,
    .destructor_cb  = led_destructor,
    .event_cb       = led_event,
    /* designators follow lv_obj_class_private.h declaration order -- name
     * declares before width_def (the rotary's note). */
    .name           = "synthui_led_button",
    .width_def      = 96,       /* the DC default size */
    .height_def     = 96,
    .instance_size  = sizeof(synthui_led_button_t),
};

lv_obj_t *synthui_led_button_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&synthui_led_button_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

static void led_constructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    synthui_led_button_t *b = (synthui_led_button_t *)obj;
    b->color = SYNTHUI_LED_BUTTON_RED;
    b->lit = b->pressed = b->cue = b->disabled = false;
    /* CLICKABLE is the base default and taps are the whole input story;
     * a scrollable key would swallow taps as drags (synthui_step's reasoning). */
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void led_destructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    LV_UNUSED(obj);
}

/* The DRAWN pressed state reads LV_STATE_PRESSED directly, not a flag kept
 * from events: lv_obj_add_state(obj, LV_STATE_PRESSED) sends no
 * LV_EVENT_PRESSED, and the spec requires that state to draw exactly like the
 * latch (the gate scene pins it with key 15). */
static bool led_drawn_pressed(const synthui_led_button_t *b)
{
    return b->pressed || lv_obj_has_state((const lv_obj_t *)b, LV_STATE_PRESSED);
}

/* Like led_drawn_pressed: the drawn disabled state reads LV_STATE_DISABLED
 * directly, so a key disabled by lv_obj_add_state(LV_STATE_DISABLED) alone
 * (bypassing the setter) draws grey the same as one disabled through
 * synthui_led_button_set_disabled(). */
static bool led_drawn_disabled(const synthui_led_button_t *b)
{
    return b->disabled || lv_obj_has_state((const lv_obj_t *)b, LV_STATE_DISABLED);
}

/* --- pixel helpers.  ALL float->pixel rounding goes through the math
 * header's rect_px / circle_px (the conversion the host sweep tests); this
 * file only offsets the widget-relative result by the object's coords. --- */
static void led_px_to_area(lv_area_t *out, const lv_area_t *c, const synthui_led_button_px_t *px)
{
    out->x1 = c->x1 + px->x1;
    out->y1 = c->y1 + px->y1;
    out->x2 = c->x1 + px->x2;
    out->y2 = c->y1 + px->y2;
}

static void led_area(lv_area_t *out, const lv_area_t *c, const synthui_led_button_rect_t *r, int32_t dy_px)
{
    const synthui_led_button_px_t px = synthui_led_button_rect_px(r, dy_px);
    led_px_to_area(out, c, &px);
}

static void led_circle_area(lv_area_t *out, const lv_area_t *c, const synthui_led_button_circle_t *k, int32_t dy_px)
{
    const synthui_led_button_px_t px = synthui_led_button_circle_px(k, dy_px);
    led_px_to_area(out, c, &px);
}

/* Every rect drawn here carries base.obj: on the software path the ONLY
 * thing LVGL reads it for is LV_EVENT_DRAW_TASK_ADDED (lv_draw.c, behind
 * LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS), which is how synthui_led_button_test
 * counts draw tasks per setter (NEW-50).  lv_draw_rect_dsc_init leaves it
 * NULL, and a widget whose tasks cannot be attributed cannot be
 * instrumented.  Pixel-neutral: the goldens prove it. */
static void led_dsc_init(lv_draw_rect_dsc_t *d, synthui_led_button_t *b)
{
    lv_draw_rect_dsc_init(d);
    d->base.obj = (lv_obj_t *)b;
}

static void led_invalidate_px(lv_obj_t *obj, const synthui_led_button_px_t *px)
{
    lv_area_t c, a;
    lv_obj_get_coords(obj, &c);
    led_px_to_area(&a, &c, px);
    lv_obj_invalidate_area(obj, &a);
}

static void led_invalidate_lit_box(lv_obj_t *obj)
{
    synthui_led_button_t *b = (synthui_led_button_t *)obj;
    lv_area_t c;
    lv_obj_get_coords(obj, &c);
    synthui_led_button_px_t px;
    synthui_led_button_lit_box((float)lv_area_get_width(&c), (float)lv_area_get_height(&c),
                               led_drawn_pressed(b), &px);
    led_invalidate_px(obj, &px);
}

static void led_invalidate_press_box(lv_obj_t *obj)
{
    lv_area_t c;
    lv_obj_get_coords(obj, &c);
    synthui_led_button_px_t px;
    synthui_led_button_press_box((float)lv_area_get_width(&c), (float)lv_area_get_height(&c), &px);
    led_invalidate_px(obj, &px);
}

/* A finger went down or came up.  The press box does not depend on the press
 * state, so the invalidation is correct whether or not the indev has already
 * flipped LV_STATE_PRESSED when this event arrives; the draw that follows reads
 * the settled state.  A latched key does not move, so nothing is invalidated. */
static void led_on_press_edge(lv_obj_t *obj)
{
    const synthui_led_button_t *b = (const synthui_led_button_t *)obj;
    if (b->pressed) return;
    led_invalidate_press_box(obj);
}

static void led_event(const lv_obj_class_t *cls, lv_event_t *e)
{
    LV_UNUSED(cls);
    if (lv_obj_event_base(MY_CLASS, e) != LV_RESULT_OK) return;
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    switch (lv_event_get_code(e)) {
    case LV_EVENT_DRAW_MAIN:
        led_draw((synthui_led_button_t *)obj, lv_event_get_layer(e));
        break;
    /* Transient press: the key sinks while a finger is down.  INDEV_RESET is
     * in this group too: the base lv_obj event clears LV_STATE_PRESSED on it
     * with no invalidation of its own (lv_obj.c), so a held key would
     * otherwise stay drawn sunk after the indev is reset out from under it. */
    case LV_EVENT_PRESSED:
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
    case LV_EVENT_INDEV_RESET:
        led_on_press_edge(obj);
        break;
    default:
        break;
    }
}

static void led_fill(lv_layer_t *layer, synthui_led_button_t *b, const lv_area_t *a,
                     uint32_t hex, lv_opa_t opa, int32_t radius)
{
    lv_draw_rect_dsc_t d;
    led_dsc_init(&d, b);
    d.bg_color = lv_color_hex(hex);
    d.bg_opa = opa;
    d.radius = radius;
    lv_draw_rect(layer, &d, a);
}

static void led_grad(lv_layer_t *layer, synthui_led_button_t *b, const lv_area_t *a,
                     uint32_t top, uint32_t bottom, int32_t radius)
{
    lv_draw_rect_dsc_t d;
    led_dsc_init(&d, b);
    d.bg_opa = LV_OPA_COVER;
    d.bg_grad.dir = LV_GRAD_DIR_VER;
    d.bg_grad.stops_count = 2;
    d.bg_grad.stops[0].color = lv_color_hex(top);
    d.bg_grad.stops[0].opa = LV_OPA_COVER;
    d.bg_grad.stops[0].frac = 0;
    d.bg_grad.stops[1].color = lv_color_hex(bottom);
    d.bg_grad.stops[1].opa = LV_OPA_COVER;
    d.bg_grad.stops[1].frac = 255;
    d.radius = radius;
    lv_draw_rect(layer, &d, a);
}

static void led_draw(synthui_led_button_t *b, lv_layer_t *layer)
{
    lv_area_t c;
    lv_obj_get_coords((lv_obj_t *)b, &c);
    const int32_t w = lv_area_get_width(&c);
    const int32_t h = lv_area_get_height(&c);
    if (w <= 0 || h <= 0) return;

    const bool pressed = led_drawn_pressed(b);
    synthui_led_button_layout_t L;
    if (!synthui_led_button_compute_layout((float)w, (float)h, pressed, &L)) return;
    synthui_led_button_palette_t P;
    synthui_led_button_palette(b->color, b->lit, pressed, b->cue, led_drawn_disabled(b), &P);

    lv_area_t a;
    const int32_t dy = L.dy_px;   /* whole pixels, added AFTER rounding: a press never resizes a layer */

    /* 1. bezel (never moves): fill + border, the SVG stroke drawn inside the extent */
    led_area(&a, &c, &L.bezel, 0);
    {
        lv_draw_rect_dsc_t d;
        led_dsc_init(&d, b);
        d.bg_color = lv_color_hex(SYNTHUI_LED_BUTTON_BEZEL);
        d.bg_opa = LV_OPA_COVER;
        d.radius = synthui_led_button_radius_px(L.bezel_r);
        d.border_color = lv_color_hex(P.bezel_color);
        d.border_width = P.cue_border ? L.cue_bw_px : L.bezel_bw_px;  /* same source as bezel_color */
        d.border_opa = LV_OPA_COVER;
        d.border_side = LV_BORDER_SIDE_FULL;
        lv_draw_rect(layer, &d, &a);
    }

    /* 2. well (never moves) */
    led_area(&a, &c, &L.well, 0);
    led_fill(layer, b, &a, SYNTHUI_LED_BUTTON_WELL, SYNTHUI_LED_BUTTON_WELL_OPA, synthui_led_button_radius_px(L.well_r));

    /* 3. cap: solid mid under two 2-stop halves (LV_GRADIENT_MAX_STOPS is 2).
     * LVGL rounds all four corners of EACH half, not just the outer two, so
     * the inner-corner wedges show the solid mid fill through a gradient
     * that at that point is within a few colour levels of mid, and the two
     * halves double-composite the cap's outer antialiased edge on top of the
     * solid layer's.  Both are deterministic, visually negligible, and
     * pinned by the golden -- not an approximation being waved away. */
    led_area(&a, &c, &L.cap, dy);
    led_fill(layer, b, &a, P.cap_mid, LV_OPA_COVER, synthui_led_button_radius_px(L.cap_r));
    led_area(&a, &c, &L.cap_top, dy);
    led_grad(layer, b, &a, P.cap_top, P.cap_mid, synthui_led_button_radius_px(L.cap_r));
    led_area(&a, &c, &L.cap_low, dy);
    led_grad(layer, b, &a, P.cap_mid, P.cap_low, synthui_led_button_radius_px(L.cap_r));

    /* 4. highlight */
    led_area(&a, &c, &L.highlight, dy);
    led_fill(layer, b, &a, 0xFFFFFFu, P.highlight_opa, synthui_led_button_radius_px(L.highlight_r));

    /* 5. halo: a 10-unit border on the LED grown by 5; the LED fill covers
     * the inner half, which is what SVG's stroke-over-fill produces */
    if (P.halo_on) {
        led_area(&a, &c, &L.halo, dy);
        lv_draw_rect_dsc_t d;
        led_dsc_init(&d, b);
        d.bg_opa = LV_OPA_TRANSP;
        d.radius = synthui_led_button_radius_px(L.halo_r);
        d.border_color = lv_color_hex(P.halo_color);
        d.border_width = L.halo_bw_px;
        d.border_opa = SYNTHUI_LED_BUTTON_HALO_OPA;
        d.border_side = LV_BORDER_SIDE_FULL;
        lv_draw_rect(layer, &d, &a);
    }

    /* 6. LED */
    led_area(&a, &c, &L.led, dy);
    led_fill(layer, b, &a, P.led_fill, LV_OPA_COVER, synthui_led_button_radius_px(L.led_r));

    /* 7. moulding dots (dropped below 34 px) */
    if (L.dots_visible) {
        led_circle_area(&a, &c, &L.dot1, dy);
        led_fill(layer, b, &a, 0x000000u, SYNTHUI_LED_BUTTON_DOT_OPA, LV_RADIUS_CIRCLE);
        led_circle_area(&a, &c, &L.dot2, dy);
        led_fill(layer, b, &a, 0x000000u, SYNTHUI_LED_BUTTON_DOT_OPA, LV_RADIUS_CIRCLE);
    }

    /* 8. base */
    led_area(&a, &c, &L.base, dy);
    led_fill(layer, b, &a, SYNTHUI_LED_BUTTON_BASE, P.base_opa, synthui_led_button_radius_px(L.base_r));
}

/* --- setters: early-return on no change, invalidate only the box painted --- */

void synthui_led_button_set_lit(lv_obj_t *obj, bool lit)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_led_button_t *b = (synthui_led_button_t *)obj;
    if (b->lit == lit) return;
    b->lit = lit;
    led_invalidate_lit_box(obj);
}

bool synthui_led_button_get_lit(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    return ((const synthui_led_button_t *)obj)->lit;
}

void synthui_led_button_set_pressed(lv_obj_t *obj, bool pressed)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_led_button_t *b = (synthui_led_button_t *)obj;
    if (b->pressed == pressed) return;
    b->pressed = pressed;
    if (lv_obj_has_state(obj, LV_STATE_PRESSED)) return;   /* finger still down: nothing moves */
    led_invalidate_press_box(obj);
}

bool synthui_led_button_get_pressed(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    return ((const synthui_led_button_t *)obj)->pressed;
}

void synthui_led_button_set_cue(lv_obj_t *obj, bool cue)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_led_button_t *b = (synthui_led_button_t *)obj;
    if (b->cue == cue) return;
    b->cue = cue;
    lv_obj_invalidate(obj);   /* the bezel ring is the outer edge on four sides */
}

bool synthui_led_button_get_cue(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    return ((const synthui_led_button_t *)obj)->cue;
}

void synthui_led_button_set_disabled(lv_obj_t *obj, bool disabled)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_led_button_t *b = (synthui_led_button_t *)obj;
    if (b->disabled == disabled) return;
    b->disabled = disabled;
    if (disabled) {
        lv_obj_remove_state(obj, LV_STATE_PRESSED);
        lv_obj_add_state(obj, LV_STATE_DISABLED);
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_remove_state(obj, LV_STATE_DISABLED);
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_invalidate(obj);   /* covers the press-state repaint too */
}

bool synthui_led_button_get_disabled(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    return ((const synthui_led_button_t *)obj)->disabled;
}

void synthui_led_button_set_color(lv_obj_t *obj, synthui_led_button_color_t color)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_led_button_t *b = (synthui_led_button_t *)obj;
    if (b->color == color) return;
    b->color = color;
    led_invalidate_lit_box(obj);
}

synthui_led_button_color_t synthui_led_button_get_color(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    return ((const synthui_led_button_t *)obj)->color;
}
