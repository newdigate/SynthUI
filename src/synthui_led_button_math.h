/* synthui_led_button_math.h - pure layout, damage boxes and palette for the
 * SynthUI LedButton.  Header-only and LVGL-free for direct host unit testing.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT
 *
 * Geometry is the spec's written description of the DC reference
 * (docs/superpowers/specs/2026-09-15-synthui-led-button-design.md section 4):
 * a 100-unit box scaled by s = min(w,h)/100 and centred; the press offset dy
 * is 2.5 units and moves every layer except the bezel and the well. */
#ifndef SYNTHUI_LED_BUTTON_MATH_H
#define SYNTHUI_LED_BUTTON_MATH_H

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

typedef struct {
    float s;              /* px per unit */
    float ox, oy;         /* centring offset of the 100-unit box, px */
    float dy;             /* press offset, px */
    bool  dots_visible;
    synthui_led_button_rect_t bezel, well, cap, cap_top, cap_low, highlight, halo, led, base;
    synthui_led_button_circle_t dot1, dot2;
    float bezel_r, well_r, cap_r, highlight_r, halo_r, led_r, base_r;   /* px */
} synthui_led_button_layout_t;

typedef struct {
    uint32_t cap_top, cap_mid, cap_low;
    uint8_t  highlight_opa, base_opa;
    uint32_t led_fill;
    uint32_t halo_color;
    bool     halo_on;
    uint32_t bezel_color;
    float    bezel_w_units;
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

static inline bool synthui_led_button_compute_layout(float w, float h, bool pressed,
                                                     synthui_led_button_layout_t *L)
{
    if (w <= 0.0f || h <= 0.0f) return false;
    const float side = w < h ? w : h;
    const float s = side / SYNTHUI_LED_BUTTON_UNIT;
    const float ox = (w - side) * 0.5f;
    const float oy = (h - side) * 0.5f;
    const float dy = pressed ? SYNTHUI_LED_BUTTON_PRESS_DY : 0.0f;
    const float cap_top_h = 80.0f * SYNTHUI_LED_BUTTON_CAP_SPLIT;

    L->s = s; L->ox = ox; L->oy = oy; L->dy = dy * s;
    L->dots_visible = side >= SYNTHUI_LED_BUTTON_DOTS_MIN_PX;

    L->bezel     = synthui_led_button_unit_rect(ox, oy, s, 0.0f, 0.0f, 100.0f, 100.0f);
    L->well      = synthui_led_button_unit_rect(ox, oy, s, 7.0f, 6.0f, 86.0f, 88.0f);
    L->cap       = synthui_led_button_unit_rect(ox, oy, s, 10.0f, 9.0f + dy, 80.0f, 80.0f);
    L->cap_top   = synthui_led_button_unit_rect(ox, oy, s, 10.0f, 9.0f + dy, 80.0f, cap_top_h);
    L->cap_low   = synthui_led_button_unit_rect(ox, oy, s, 10.0f, 9.0f + dy + cap_top_h, 80.0f, 80.0f - cap_top_h);
    L->highlight = synthui_led_button_unit_rect(ox, oy, s, 14.0f, 12.0f + dy, 72.0f, 11.0f);
    L->led       = synthui_led_button_unit_rect(ox, oy, s, 26.0f, 19.0f + dy, 48.0f, 15.0f);
    L->halo      = synthui_led_button_unit_rect(ox, oy, s,
                       26.0f - SYNTHUI_LED_BUTTON_HALO_REACH, 19.0f + dy - SYNTHUI_LED_BUTTON_HALO_REACH,
                       48.0f + 2.0f * SYNTHUI_LED_BUTTON_HALO_REACH, 15.0f + 2.0f * SYNTHUI_LED_BUTTON_HALO_REACH);
    L->base      = synthui_led_button_unit_rect(ox, oy, s, 10.0f, 82.0f + dy, 80.0f, 7.0f);
    L->dot1.cx = ox + 38.0f * s; L->dot1.cy = oy + (26.5f + dy) * s; L->dot1.r = 1.7f * s;
    L->dot2.cx = ox + 62.0f * s; L->dot2.cy = oy + (26.5f + dy) * s; L->dot2.r = 1.7f * s;

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
    return true;
}

/* Damage box for a lit/colour change: the halo box at the CURRENT press offset. */
static inline void synthui_led_button_lit_box(float w, float h, bool pressed,
                                              synthui_led_button_rect_t *out)
{
    synthui_led_button_layout_t L;
    if (!synthui_led_button_compute_layout(w, h, pressed, &L)) { out->x = out->y = out->w = out->h = 0.0f; return; }
    *out = L.halo;
}

/* Damage box for a press change: the cap group at BOTH offsets --
 * x 10..90, y 9 .. (82 + 7 + 2.5) = 91.5 units. */
static inline void synthui_led_button_press_box(float w, float h, synthui_led_button_rect_t *out)
{
    synthui_led_button_layout_t L;
    if (!synthui_led_button_compute_layout(w, h, false, &L)) { out->x = out->y = out->w = out->h = 0.0f; return; }
    *out = synthui_led_button_unit_rect(L.ox, L.oy, L.s, 10.0f, 9.0f, 80.0f, 82.5f);
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
    p->bezel_w_units = cue ? 3.5f : 2.0f;
}

#ifdef __cplusplus
}
#endif
#endif /* SYNTHUI_LED_BUTTON_MATH_H */
