/* synthui_slide_toggle_math.h - pure geometry arithmetic for SynthUI SlideToggle.
 * Header-only and LVGL-free for direct host unit testing.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_SLIDE_TOGGLE_MATH_H
#define SYNTHUI_SLIDE_TOGGLE_MATH_H

#include "synthui_slide_toggle_types.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

#ifdef __cplusplus
namespace synthui::slide_toggle {

struct Point {
    float x;
    float y;
};

struct GlyphPath {
    int num_points;
    Point points[10];
};

struct SlideToggleGeom {
    float w;
    float h;
    float vw;
    float vh;
    float u;
    int32_t positions;
    int32_t value;
    bool disabled;
    synthui_slide_toggle_glyph_t left_glyph;
    synthui_slide_toggle_glyph_t right_glyph;
    uint32_t panel_color;

    bool has_left;
    bool has_right;
    float hx;
    float hy;
    float hw;
    float hh;
    float well_x;
    float well_y;
    float well_w;
    float well_h;
    float knob_w;
    float knob_h;
    float knob_x;
    float knob_y;
    float knob_top;
    float knob_hl_opa;
    uint32_t knob_fill;
    uint32_t glyph_color;
    uint32_t ridge_color;
    float ridge_w;
    float ridge_x[4];
    float ridge_y1;
    float ridge_y2;

    float left_tx;
    float left_ty;
    float left_scale;
    float right_tx;
    float right_ty;
    float right_scale;
};

inline GlyphPath get_glyph_path(synthui_slide_toggle_glyph_t glyph) {
    GlyphPath p{};
    switch (glyph) {
    case SYNTHUI_SLIDE_TOGGLE_GLYPH_SAW:
        p.num_points = 6;
        p.points[0] = {2.0f, 15.0f};
        p.points[1] = {7.0f, 6.0f};
        p.points[2] = {7.0f, 15.0f};
        p.points[3] = {12.0f, 6.0f};
        p.points[4] = {12.0f, 15.0f};
        p.points[5] = {17.0f, 6.0f};
        break;
    case SYNTHUI_SLIDE_TOGGLE_GLYPH_SQUARE:
        p.num_points = 8;
        p.points[0] = {2.0f, 15.0f};
        p.points[1] = {2.0f, 7.0f};
        p.points[2] = {7.0f, 7.0f};
        p.points[3] = {7.0f, 15.0f};
        p.points[4] = {12.0f, 15.0f};
        p.points[5] = {12.0f, 7.0f};
        p.points[6] = {17.0f, 7.0f};
        p.points[7] = {17.0f, 15.0f};
        break;
    case SYNTHUI_SLIDE_TOGGLE_GLYPH_TRI:
        p.num_points = 5;
        p.points[0] = {2.0f, 15.0f};
        p.points[1] = {6.5f, 6.0f};
        p.points[2] = {11.0f, 15.0f};
        p.points[3] = {15.5f, 6.0f};
        p.points[4] = {18.0f, 11.0f};
        break;
    case SYNTHUI_SLIDE_TOGGLE_GLYPH_PULSE:
        p.num_points = 9;
        p.points[0] = {2.0f, 15.0f};
        p.points[1] = {5.0f, 15.0f};
        p.points[2] = {5.0f, 7.0f};
        p.points[3] = {8.0f, 7.0f};
        p.points[4] = {8.0f, 15.0f};
        p.points[5] = {13.0f, 15.0f};
        p.points[6] = {13.0f, 7.0f};
        p.points[7] = {16.0f, 7.0f};
        p.points[8] = {16.0f, 15.0f};
        break;
    case SYNTHUI_SLIDE_TOGGLE_GLYPH_NONE:
    default:
        p.num_points = 0;
        break;
    }
    return p;
}

inline bool compute_geom(float w, float h,
                         int32_t positions, int32_t value,
                         synthui_slide_toggle_glyph_t left, synthui_slide_toggle_glyph_t right,
                         uint32_t panel_color, bool disabled,
                         SlideToggleGeom &g) {
    if (std::isnan(w) || std::isnan(h) || w < 1.0f || h < 1.0f) {
        return false;
    }

    g.w = w;
    g.h = h;
    g.vw = 100.0f;
    g.vh = std::round(100.0f * h / w);
    g.u = w / 100.0f;

    g.positions = std::max<int32_t>(2, std::min<int32_t>(4, positions));
    g.value = std::max<int32_t>(0, std::min<int32_t>(g.positions - 1, value));
    g.disabled = disabled;
    g.left_glyph = left;
    g.right_glyph = right;
    g.panel_color = panel_color;

    g.has_left = (left != SYNTHUI_SLIDE_TOGGLE_GLYPH_NONE);
    g.has_right = (right != SYNTHUI_SLIDE_TOGGLE_GLYPH_NONE);

    const float gx = 22.0f;
    g.hx = g.has_left ? gx : 3.0f;
    const float hRight = g.has_right ? (100.0f - gx) : 97.0f;
    g.hw = hRight - g.hx;
    g.hh = g.vh * 0.72f;
    g.hy = (g.vh - g.hh) / 2.0f;

    const float inset = std::max(1.6f, g.vh * 0.06f);
    g.well_x = g.hx + inset;
    g.well_y = g.hy + inset;
    g.well_w = g.hw - inset * 2.0f;
    g.well_h = g.hh - inset * 2.0f;

    g.knob_w = g.well_w / static_cast<float>(g.positions);
    g.knob_h = g.well_h;
    g.knob_x = g.well_x + static_cast<float>(g.value) * g.knob_w;
    g.knob_y = g.well_y;
    g.knob_top = g.knob_h * 0.16f;

    g.knob_fill = disabled ? SYNTHUI_SLIDE_TOGGLE_COLOR_KNOB_DISABLED : SYNTHUI_SLIDE_TOGGLE_COLOR_KNOB_ACTIVE;
    g.glyph_color = disabled ? SYNTHUI_SLIDE_TOGGLE_COLOR_GLYPH_DISABLED : SYNTHUI_SLIDE_TOGGLE_COLOR_GLYPH_ACTIVE;
    g.ridge_color = disabled ? SYNTHUI_SLIDE_TOGGLE_COLOR_RIDGE_DISABLED : SYNTHUI_SLIDE_TOGGLE_COLOR_RIDGE_ACTIVE;
    g.knob_hl_opa = disabled ? 0.05f : 0.12f;

    g.ridge_w = std::max(0.8f, g.knob_w * 0.055f);
    const float ridge_f[4] = {0.26f, 0.44f, 0.62f, 0.80f};
    for (int i = 0; i < 4; i++) {
        g.ridge_x[i] = std::round(g.knob_w * ridge_f[i] * 10.0f) / 10.0f;
    }
    g.ridge_y1 = g.knob_h * 0.14f;
    g.ridge_y2 = g.knob_h * 0.86f;

    const float gs = (g.vh * 0.62f) / 20.0f;
    g.left_scale = gs;
    g.left_tx = 1.0f;
    g.left_ty = (g.vh - 20.0f * gs) / 2.0f;

    g.right_scale = gs;
    g.right_tx = 100.0f - gx + 4.0f;
    g.right_ty = (g.vh - 20.0f * gs) / 2.0f;

    return true;
}

inline void compute_knob_dirty_area(const SlideToggleGeom &g, int32_t x0, int32_t y0,
                                    int32_t &x1, int32_t &y1, int32_t &x2, int32_t &y2) {
    /* Covers entire slider well travel zone including knob shadow (+1.5 x, +2.0 y) */
    x1 = x0 + static_cast<int32_t>(std::floor(g.well_x * g.u)) - 1;
    y1 = y0 + static_cast<int32_t>(std::floor(g.well_y * g.u)) - 1;
    x2 = x0 + static_cast<int32_t>(std::ceil((g.well_x + g.well_w + 1.5f) * g.u)) + 1;
    y2 = y0 + static_cast<int32_t>(std::ceil((g.well_y + g.well_h + 2.0f) * g.u)) + 1;
}

} // namespace synthui::slide_toggle
#endif

#endif /* SYNTHUI_SLIDE_TOGGLE_MATH_H */
