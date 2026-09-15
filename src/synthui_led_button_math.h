/* synthui_led_button_math.h - pure layout, damage boxes and palette for the
 * SynthUI LedButton.  Header-only and LVGL-free for direct host unit testing.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT
 *
 * Geometry is the spec's written description of the DC reference
 * (docs/superpowers/specs/2026-09-15-synthui-led-button-design.md section 4):
 * a 100-unit box scaled by s = min(w,h)/100 and centred.
 *
 * Spec rule: "state change is colour and one 2.5-unit offset; nothing
 * resizes."  So every rect/circle in synthui_led_button_layout_t is stored
 * at dy = 0 (unpressed), float px, ox/oy already folded in.  The press
 * offset is a SEPARATE whole-pixel integer, dy_px, rounded ONCE from
 * PRESS_DY*s, and the widget adds it AFTER each layer's own rect is
 * independently rounded to pixels (synthui_led_button_rect_px /
 * _circle_px) -- never before.  Baking dy into the float rect before
 * rounding -- the original design -- let each layer round to a DIFFERENT
 * pixel offset on press: measured at 96 px, the cap moved 2 px while the
 * LED moved 3, and the LED's height rounded 15 -> 14 (a resize, which the
 * spec rule forbids). Rounding once, after translating, makes a press a
 * pure whole-pixel translation of every layer -- never a resize. */
#ifndef SYNTHUI_LED_BUTTON_MATH_H
#define SYNTHUI_LED_BUTTON_MATH_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "synthui_led_button_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SYNTHUI_LED_BUTTON_UNIT        100.0f
#define SYNTHUI_LED_BUTTON_PRESS_DY    2.5f    /* units */
#define SYNTHUI_LED_BUTTON_HALO_REACH  5.0f    /* units the halo extends past the LED */
#define SYNTHUI_LED_BUTTON_CAP_SPLIT   0.62f   /* top->mid over the first 62 % of the cap */
#define SYNTHUI_LED_BUTTON_DOTS_MIN_PX 34.0f   /* the sheet: below 34 px the dots drop out */

/* 8-bit opacities from the DC alphas (x255, rounded) */
#define SYNTHUI_LED_BUTTON_WELL_OPA        230   /* 0.90 */
#define SYNTHUI_LED_BUTTON_HIGHLIGHT_OPA   140   /* 0.55 */
#define SYNTHUI_LED_BUTTON_HIGHLIGHT_OPA_P  71   /* 0.28 pressed */
#define SYNTHUI_LED_BUTTON_BASE_OPA        217   /* 0.85 */
#define SYNTHUI_LED_BUTTON_BASE_OPA_P      128   /* 0.50 pressed */
#define SYNTHUI_LED_BUTTON_DOT_OPA          56   /* 0.22 */
#define SYNTHUI_LED_BUTTON_HALO_OPA         97   /* 0.38 */

typedef struct { float x, y, w, h; } synthui_led_button_rect_t;
typedef struct { float cx, cy, r; } synthui_led_button_circle_t;

/* Inclusive pixel area relative to the widget's top-left (LVGL convention). */
typedef struct { int32_t x1, y1, x2, y2; } synthui_led_button_px_t;

typedef struct {
    float s, ox, oy;
    int32_t dy_px;          /* press offset in whole px: pressed ? lroundf(PRESS_DY * s) : 0 */
    bool  dots_visible;
    /* ALL rects and circles at dy = 0 (unpressed), float px incl. ox/oy.
       The widget adds dy_px AFTER rounding to: cap, cap_top, cap_low, highlight, halo, led, dots, base.
       bezel and well never move. */
    synthui_led_button_rect_t bezel, well, cap, cap_top, cap_low, highlight, halo, led, base;
    synthui_led_button_circle_t dot1, dot2;
    float bezel_r, well_r, cap_r, highlight_r, halo_r, led_r, base_r;   /* px */
    /* The bezel deliberately keeps the SAME (0,0,100,100) r17 path for both
     * the normal stroke and the "cue" stroke: the cue is a WIDER border
     * drawn on the identical rect/radius (LVGL draws borders inside the
     * area, so the wider cue stroke is clamped inward rather than growing
     * the ring past the bezel).  At side <= 34 px, max(1, lroundf(2.0*s))
     * and max(1, lroundf(3.5*s)) both round to 1 px, so on the smallest
     * legal key the cue reduces to a colour change only -- there is no
     * width left to show it. */
    int32_t bezel_bw_px;    /* max(1, lroundf(2.0  * s)) */
    int32_t cue_bw_px;      /* max(1, lroundf(3.5  * s)) */
    int32_t halo_bw_px;     /* max(1, lroundf(2 * HALO_REACH * s)) */
} synthui_led_button_layout_t;

typedef struct {
    uint32_t cap_top, cap_mid, cap_low;
    uint8_t  highlight_opa, base_opa;
    uint32_t led_fill;
    uint32_t halo_color;
    bool     halo_on;
    uint32_t bezel_color;
} synthui_led_button_palette_t;

static inline uint32_t synthui_led_button_color_on(synthui_led_button_color_t c)
{
    switch (c) {
    case SYNTHUI_LED_BUTTON_AMBER: return SYNTHUI_LED_BUTTON_AMBER_ON;
    case SYNTHUI_LED_BUTTON_GREEN: return SYNTHUI_LED_BUTTON_GREEN_ON;
    case SYNTHUI_LED_BUTTON_BLUE:  return SYNTHUI_LED_BUTTON_BLUE_ON;
    default:                       return SYNTHUI_LED_BUTTON_RED_ON;
    }
}

static inline uint32_t synthui_led_button_color_off(synthui_led_button_color_t c)
{
    switch (c) {
    case SYNTHUI_LED_BUTTON_AMBER: return SYNTHUI_LED_BUTTON_AMBER_OFF;
    case SYNTHUI_LED_BUTTON_GREEN: return SYNTHUI_LED_BUTTON_GREEN_OFF;
    case SYNTHUI_LED_BUTTON_BLUE:  return SYNTHUI_LED_BUTTON_BLUE_OFF;
    default:                       return SYNTHUI_LED_BUTTON_RED_OFF;
    }
}

static inline synthui_led_button_rect_t synthui_led_button_unit_rect(
    float ox, float oy, float s, float x, float y, float w, float h)
{
    synthui_led_button_rect_t r;
    r.x = ox + x * s; r.y = oy + y * s; r.w = w * s; r.h = h * s;
    return r;
}

static inline int32_t synthui_led_button_bw_px(float units, float s)
{
    int32_t px = (int32_t)lroundf(units * s);
    return px < 1 ? 1 : px;
}

/* The ONE float->pixel conversion; the widget must use these, so host tests
 * exercise the real rounding.  Inclusive: x1/y1 round the top-left corner,
 * x2/y2 round the bottom-right corner and step back one pixel. dy_px is
 * added AFTER rounding, to the already-rounded corner. */
static inline synthui_led_button_px_t synthui_led_button_rect_px(
    const synthui_led_button_rect_t *r, int32_t dy_px)
{
    synthui_led_button_px_t px;
    px.x1 = (int32_t)lroundf(r->x);
    px.y1 = (int32_t)lroundf(r->y) + dy_px;
    px.x2 = (int32_t)lroundf(r->x + r->w) - 1;
    px.y2 = (int32_t)lroundf(r->y + r->h) - 1 + dy_px;
    return px;
}

static inline synthui_led_button_px_t synthui_led_button_circle_px(
    const synthui_led_button_circle_t *c, int32_t dy_px)
{
    synthui_led_button_px_t px;
    px.x1 = (int32_t)lroundf(c->cx - c->r);
    px.y1 = (int32_t)lroundf(c->cy - c->r) + dy_px;
    px.x2 = (int32_t)lroundf(c->cx + c->r) - 1;
    px.y2 = (int32_t)lroundf(c->cy + c->r) - 1 + dy_px;
    return px;
}

static inline bool synthui_led_button_compute_layout(float w, float h, bool pressed,
                                                     synthui_led_button_layout_t *L)
{
    if (w <= 0.0f || h <= 0.0f) return false;
    const float side = w < h ? w : h;
    const float s = side / SYNTHUI_LED_BUTTON_UNIT;
    const float ox = (w - side) * 0.5f;
    const float oy = (h - side) * 0.5f;

    L->s = s; L->ox = ox; L->oy = oy;
    L->dy_px = pressed ? (int32_t)lroundf(SYNTHUI_LED_BUTTON_PRESS_DY * s) : 0;
    L->dots_visible = side >= SYNTHUI_LED_BUTTON_DOTS_MIN_PX;

    L->bezel = synthui_led_button_unit_rect(ox, oy, s, 0.0f, 0.0f, 100.0f, 100.0f);
    L->well  = synthui_led_button_unit_rect(ox, oy, s, 7.0f, 6.0f, 86.0f, 88.0f);

    /* cap/cap_top/cap_low share their boundary floats bit-for-bit (rather
     * than each independently re-deriving the split point from unit space)
     * so the two halves round to pixels that meet with NO gap or overlap at
     * any scale -- see led_button_test's cap-adjacency sweep. */
    {
        const float cap_y = oy + 9.0f * s;
        const float cap_h = 80.0f * s;
        const float cap_bottom = cap_y + cap_h;
        const float cap_top_h = cap_h * SYNTHUI_LED_BUTTON_CAP_SPLIT;
        const float cap_low_y = cap_y + cap_top_h;

        L->cap.x = ox + 10.0f * s; L->cap.y = cap_y; L->cap.w = 80.0f * s; L->cap.h = cap_h;
        L->cap_top.x = L->cap.x; L->cap_top.y = cap_y;     L->cap_top.w = L->cap.w; L->cap_top.h = cap_top_h;
        L->cap_low.x = L->cap.x; L->cap_low.y = cap_low_y; L->cap_low.w = L->cap.w; L->cap_low.h = cap_bottom - cap_low_y;
    }

    L->highlight = synthui_led_button_unit_rect(ox, oy, s, 14.0f, 12.0f, 72.0f, 11.0f);
    L->led       = synthui_led_button_unit_rect(ox, oy, s, 26.0f, 19.0f, 48.0f, 15.0f);
    L->halo      = synthui_led_button_unit_rect(ox, oy, s,
                       26.0f - SYNTHUI_LED_BUTTON_HALO_REACH, 19.0f - SYNTHUI_LED_BUTTON_HALO_REACH,
                       48.0f + 2.0f * SYNTHUI_LED_BUTTON_HALO_REACH, 15.0f + 2.0f * SYNTHUI_LED_BUTTON_HALO_REACH);
    L->base      = synthui_led_button_unit_rect(ox, oy, s, 10.0f, 82.0f, 80.0f, 7.0f);
    L->dot1.cx = ox + 38.0f * s; L->dot1.cy = oy + 26.5f * s; L->dot1.r = 1.7f * s;
    L->dot2.cx = ox + 62.0f * s; L->dot2.cy = oy + 26.5f * s; L->dot2.r = 1.7f * s;

    /* SVG radii are the path's; a stroke adds half its width outside, so the
     * bezel (rx 16, stroke 2) and the halo (rx 3.5, stroke 10) carry the OUTER
     * radius here because LVGL draws borders inside the area. */
    L->bezel_r = 17.0f * s;
    L->well_r = 13.0f * s;
    L->cap_r = 11.0f * s;
    L->highlight_r = 5.5f * s;
    L->halo_r = (3.5f + SYNTHUI_LED_BUTTON_HALO_REACH) * s;
    L->led_r = 3.5f * s;
    L->base_r = 3.5f * s;

    L->bezel_bw_px = synthui_led_button_bw_px(2.0f, s);
    L->cue_bw_px   = synthui_led_button_bw_px(3.5f, s);
    L->halo_bw_px  = synthui_led_button_bw_px(2.0f * SYNTHUI_LED_BUTTON_HALO_REACH, s);
    return true;
}

/* Damage box for a lit/colour change: the halo box at the CURRENT press offset. */
static inline void synthui_led_button_lit_box(float w, float h, bool pressed,
                                              synthui_led_button_px_t *out)
{
    synthui_led_button_layout_t L;
    if (!synthui_led_button_compute_layout(w, h, pressed, &L)) {
        out->x1 = out->y1 = out->x2 = out->y2 = 0;
        return;
    }
    *out = synthui_led_button_rect_px(&L.halo, L.dy_px);
}

/* Damage box for a press change: the union of every moving layer at dy 0
 * and at the pressed dy_px, computed from rect_px -- never from a
 * hand-written unit constant. */
static inline void synthui_led_button_press_box(float w, float h, synthui_led_button_px_t *out)
{
    synthui_led_button_layout_t L;
    if (!synthui_led_button_compute_layout(w, h, true, &L)) {
        out->x1 = out->y1 = out->x2 = out->y2 = 0;
        return;
    }
    const synthui_led_button_px_t cap0       = synthui_led_button_rect_px(&L.cap, 0);
    const synthui_led_button_px_t highlight0 = synthui_led_button_rect_px(&L.highlight, 0);
    const synthui_led_button_px_t halo0      = synthui_led_button_rect_px(&L.halo, 0);
    const synthui_led_button_px_t cap_p      = synthui_led_button_rect_px(&L.cap, L.dy_px);
    const synthui_led_button_px_t base_p     = synthui_led_button_rect_px(&L.base, L.dy_px);

    out->x1 = cap0.x1;
    out->x2 = cap0.x2;
    int32_t y1 = cap0.y1;
    if (highlight0.y1 < y1) y1 = highlight0.y1;
    if (halo0.y1 < y1) y1 = halo0.y1;
    out->y1 = y1;
    out->y2 = cap_p.y2 > base_p.y2 ? cap_p.y2 : base_p.y2;
}

static inline void synthui_led_button_palette(synthui_led_button_color_t color,
                                              bool lit, bool pressed, bool cue, bool disabled,
                                              synthui_led_button_palette_t *p)
{
    if (disabled)      { p->cap_top = 0xE2E1DEu; p->cap_mid = 0xD2D1CEu; p->cap_low = 0xBCBBB8u; }
    else if (pressed)  { p->cap_top = 0xDEDCD7u; p->cap_mid = 0xCBC9C3u; p->cap_low = 0xB4B2ADu; }
    else               { p->cap_top = 0xF7F5F1u; p->cap_mid = 0xE8E6E1u; p->cap_low = 0xC9C7C1u; }
    p->highlight_opa = pressed ? SYNTHUI_LED_BUTTON_HIGHLIGHT_OPA_P : SYNTHUI_LED_BUTTON_HIGHLIGHT_OPA;
    p->base_opa      = pressed ? SYNTHUI_LED_BUTTON_BASE_OPA_P : SYNTHUI_LED_BUTTON_BASE_OPA;
    p->led_fill      = disabled ? SYNTHUI_LED_BUTTON_LED_DISABLED
                                : (lit ? synthui_led_button_color_on(color) : synthui_led_button_color_off(color));
    p->halo_color    = synthui_led_button_color_on(color);
    p->halo_on       = lit && !disabled;
    p->bezel_color   = cue ? SYNTHUI_LED_BUTTON_CUE_STROKE : SYNTHUI_LED_BUTTON_BEZEL_STROKE;
}

#ifdef __cplusplus
}
#endif
#endif /* SYNTHUI_LED_BUTTON_MATH_H */
