/* synthui_piano_key_math.h - pure geometry arithmetic for SynthUI PianoKey.
 * Header-only and LVGL-free for direct host unit testing.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_PIANO_KEY_MATH_H
#define SYNTHUI_PIANO_KEY_MATH_H

#include "synthui_piano_key_types.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

#ifdef __cplusplus
namespace synthui::piano_key {

struct Point {
    float x;
    float y;
};

struct Rect {
    float x;
    float y;
    float w;
    float h;
};

struct KeyGeom {
    float w;
    float h;
    float vw;
    float vh;
    float u;
    synthui_piano_key_type_t type;
    bool lit;
    bool pressed;
    float zone_top;
    float pad_height;

    float dy;
    float body_r;
    float edge_w;
    uint32_t body_a;
    uint32_t body_b;
    uint32_t body_c;
    uint32_t edge_c;
    float shade_a;

    float led_cx;
    float led_cy;
    float led_r;
    float led_bloom_r;
    uint32_t led_fill;

    float pad_x;
    float pad_y;
    float pad_w;
    float pad_h;
    float ridge_w;
    float ridge_y[4];
    float ridge_x1;
    float ridge_x2;
};

inline bool compute_geom(float w, float h,
                         synthui_piano_key_type_t type,
                         bool lit, bool pressed,
                         float zone_top, float pad_height,
                         KeyGeom &g) {
    if (std::isnan(w) || std::isnan(h) || w < 1.0f || h < 1.0f) {
        return false;
    }

    g.w = w;
    g.h = h;
    g.vw = 100.0f;
    g.vh = std::round(100.0f * h / w);
    g.u = w / 100.0f;
    g.type = type;
    g.lit = lit;
    g.pressed = pressed;
    g.zone_top = (zone_top <= 0.0f) ? (type == SYNTHUI_PIANO_KEY_WHITE ? 0.58f : 0.10f) : zone_top;
    g.pad_height = pad_height;

    g.dy = pressed ? (g.vh * 0.012f) : 0.0f;

    if (type == SYNTHUI_PIANO_KEY_BLACK) {
        g.body_r = 3.0f;
        g.edge_w = 1.6f;
        g.edge_c = SYNTHUI_PIANO_KEY_COLOR_BLACK_EDGE;
        g.shade_a = 0.25f;
        g.body_a = SYNTHUI_PIANO_KEY_COLOR_BLACK_A;
        g.body_b = SYNTHUI_PIANO_KEY_COLOR_BLACK_B;
        g.body_c = SYNTHUI_PIANO_KEY_COLOR_BLACK_C;
    } else {
        g.body_r = 4.0f;
        g.edge_w = 1.8f;
        g.edge_c = SYNTHUI_PIANO_KEY_COLOR_WHITE_EDGE;
        g.shade_a = 0.07f;
        if (pressed) {
            g.body_a = SYNTHUI_PIANO_KEY_COLOR_WHITE_A_PRESSED;
            g.body_b = SYNTHUI_PIANO_KEY_COLOR_WHITE_B_PRESSED;
            g.body_c = SYNTHUI_PIANO_KEY_COLOR_WHITE_C_PRESSED;
        } else {
            g.body_a = SYNTHUI_PIANO_KEY_COLOR_WHITE_A_UNPRESSED;
            g.body_b = SYNTHUI_PIANO_KEY_COLOR_WHITE_B_UNPRESSED;
            g.body_c = SYNTHUI_PIANO_KEY_COLOR_WHITE_C_UNPRESSED;
        }
    }

    // LED
    g.led_cx = 50.0f;
    g.led_cy = g.vh * (g.zone_top + 0.055f) + g.dy;
    g.led_r = std::min(14.0f, g.vh * 0.032f + 4.0f);
    g.led_bloom_r = 1.75f * g.led_r;
    g.led_fill = lit ? SYNTHUI_PIANO_KEY_COLOR_LED_LIT : SYNTHUI_PIANO_KEY_COLOR_LED_UNLIT;

    // Pad
    g.pad_x = 19.0f;
    g.pad_w = 62.0f;
    g.pad_y = g.vh * (g.zone_top + 0.14f) + g.dy;
    g.pad_h = (pad_height > 0.0f) ? (pad_height * (100.0f / w)) : (g.vh * 0.94f - (g.vh * (g.zone_top + 0.14f)));

    g.ridge_y[0] = g.pad_h * 0.22f;
    g.ridge_y[1] = g.pad_h * 0.42f;
    g.ridge_y[2] = g.pad_h * 0.62f;
    g.ridge_y[3] = g.pad_h * 0.82f;
    g.ridge_x1 = g.pad_x + g.pad_w * 0.16f;
    g.ridge_x2 = g.pad_x + g.pad_w * 0.84f;
    g.ridge_w = std::max(1.0f, g.pad_h * 0.035f);

    return true;
}

inline void compute_led_dirty_area(const KeyGeom &g, int32_t x0, int32_t y0,
                                   int32_t &x1, int32_t &y1, int32_t &x2, int32_t &y2) {
    const float r_px = std::ceil(g.led_bloom_r * g.u) + 2.0f;
    const float cx_px = static_cast<float>(x0) + g.led_cx * g.u;
    const float cy_px = static_cast<float>(y0) + g.led_cy * g.u;
    x1 = static_cast<int32_t>(std::floor(cx_px - r_px));
    y1 = static_cast<int32_t>(std::floor(cy_px - r_px));
    x2 = static_cast<int32_t>(std::ceil(cx_px + r_px));
    y2 = static_cast<int32_t>(std::ceil(cy_px + r_px));
}

inline void compute_key_dirty_area(const KeyGeom &g, int32_t x0, int32_t y0,
                                   int32_t &x1, int32_t &y1, int32_t &x2, int32_t &y2) {
    x1 = x0;
    y1 = y0;
    x2 = x0 + static_cast<int32_t>(std::round(g.w)) - 1;
    y2 = y0 + static_cast<int32_t>(std::round(g.h)) - 1;
}

} // namespace synthui::piano_key
#endif

#endif /* SYNTHUI_PIANO_KEY_MATH_H */
