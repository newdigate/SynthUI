/* slide_toggle_test.cpp - host unit test for pure slide toggle geometry & math.
 * Header-only, LVGL-free for direct host verification.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#undef NDEBUG
#include "../src/synthui_slide_toggle_math.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static bool approx_eq(float a, float b) {
    return std::fabs(a - b) < 0.05f;
}

int main() {
    using namespace synthui::slide_toggle;

    // 1. Default 2-position toggle geometry: w=150, h=52, saw/square, panel=#b9bcbc
    SlideToggleGeom g{};
    assert(compute_geom(150.0f, 52.0f, 2, 0,
                        SYNTHUI_SLIDE_TOGGLE_GLYPH_SAW,
                        SYNTHUI_SLIDE_TOGGLE_GLYPH_SQUARE,
                        SYNTHUI_SLIDE_TOGGLE_COLOR_PANEL_DEFAULT, false, g));
    assert(approx_eq(g.w, 150.0f));
    assert(approx_eq(g.h, 52.0f));
    assert(approx_eq(g.vw, 100.0f));
    assert(approx_eq(g.vh, 35.0f)); // round(100 * 52 / 150) = round(34.6667) = 35
    assert(approx_eq(g.u, 1.5f));
    assert(g.positions == 2);
    assert(g.value == 0);
    assert(!g.disabled);
    assert(g.has_left);
    assert(g.has_right);
    assert(approx_eq(g.hx, 22.0f));
    assert(approx_eq(g.hw, 56.0f)); // 78 - 22 = 56
    assert(approx_eq(g.hh, 25.2f)); // 35 * 0.72 = 25.2
    assert(approx_eq(g.hy, 4.9f));  // (35 - 25.2) / 2 = 4.9

    // Inset = max(1.6, 35 * 0.06) = max(1.6, 2.1) = 2.1
    assert(approx_eq(g.well_x, 24.1f));
    assert(approx_eq(g.well_y, 7.0f));
    assert(approx_eq(g.well_w, 51.8f)); // 56 - 4.2 = 51.8
    assert(approx_eq(g.well_h, 21.0f)); // 25.2 - 4.2 = 21.0

    // Knob W = 51.8 / 2 = 25.9, Knob H = 21.0
    assert(approx_eq(g.knob_w, 25.9f));
    assert(approx_eq(g.knob_h, 21.0f));
    assert(approx_eq(g.knob_x, 24.1f));
    assert(approx_eq(g.knob_y, 7.0f));
    assert(approx_eq(g.knob_top, 21.0f * 0.16f)); // 3.36
    assert(approx_eq(g.knob_hl_opa, 0.12f));
    assert(g.knob_fill == SYNTHUI_SLIDE_TOGGLE_COLOR_KNOB_ACTIVE);
    assert(g.glyph_color == SYNTHUI_SLIDE_TOGGLE_COLOR_GLYPH_ACTIVE);
    assert(g.ridge_color == SYNTHUI_SLIDE_TOGGLE_COLOR_RIDGE_ACTIVE);
    assert(approx_eq(g.ridge_w, 25.9f * 0.055f)); // 1.4245 >= 0.8
    assert(approx_eq(g.ridge_y1, 21.0f * 0.14f));
    assert(approx_eq(g.ridge_y2, 21.0f * 0.86f));

    // 2. Position 1 (right toggle position)
    SlideToggleGeom g_pos1{};
    assert(compute_geom(150.0f, 52.0f, 2, 1,
                        SYNTHUI_SLIDE_TOGGLE_GLYPH_SAW,
                        SYNTHUI_SLIDE_TOGGLE_GLYPH_SQUARE,
                        SYNTHUI_SLIDE_TOGGLE_COLOR_PANEL_DEFAULT, false, g_pos1));
    assert(g_pos1.value == 1);
    assert(approx_eq(g_pos1.knob_x, 24.1f + 25.9f)); // well_x + 1 * knob_w

    // 3. Clamping of positions and value
    SlideToggleGeom g_clamp{};
    assert(compute_geom(150.0f, 52.0f, 1, -2,
                        SYNTHUI_SLIDE_TOGGLE_GLYPH_NONE,
                        SYNTHUI_SLIDE_TOGGLE_GLYPH_NONE,
                        0xD6D4CFu, true, g_clamp));
    assert(g_clamp.positions == 2); // clamped min 2
    assert(g_clamp.value == 0);     // clamped min 0
    assert(!g_clamp.has_left);
    assert(!g_clamp.has_right);
    assert(approx_eq(g_clamp.hx, 3.0f));
    assert(approx_eq(g_clamp.hw, 94.0f)); // 97 - 3 = 94
    assert(g_clamp.disabled);
    assert(g_clamp.knob_fill == SYNTHUI_SLIDE_TOGGLE_COLOR_KNOB_DISABLED);
    assert(g_clamp.glyph_color == SYNTHUI_SLIDE_TOGGLE_COLOR_GLYPH_DISABLED);
    assert(g_clamp.ridge_color == SYNTHUI_SLIDE_TOGGLE_COLOR_RIDGE_DISABLED);
    assert(approx_eq(g_clamp.knob_hl_opa, 0.05f));

    SlideToggleGeom g_clamp_max{};
    assert(compute_geom(150.0f, 52.0f, 10, 10,
                        SYNTHUI_SLIDE_TOGGLE_GLYPH_TRI,
                        SYNTHUI_SLIDE_TOGGLE_GLYPH_PULSE,
                        0x6D7A85u, false, g_clamp_max));
    assert(g_clamp_max.positions == 4); // clamped max 4
    assert(g_clamp_max.value == 3);     // clamped max positions - 1 = 3

    // 4. Glyph paths
    GlyphPath saw = get_glyph_path(SYNTHUI_SLIDE_TOGGLE_GLYPH_SAW);
    assert(saw.num_points == 6);
    assert(approx_eq(saw.points[0].x, 2.0f) && approx_eq(saw.points[0].y, 15.0f));
    assert(approx_eq(saw.points[1].x, 7.0f) && approx_eq(saw.points[1].y, 6.0f));
    assert(approx_eq(saw.points[2].x, 7.0f) && approx_eq(saw.points[2].y, 15.0f));
    assert(approx_eq(saw.points[3].x, 12.0f) && approx_eq(saw.points[3].y, 6.0f));
    assert(approx_eq(saw.points[4].x, 12.0f) && approx_eq(saw.points[4].y, 15.0f));
    assert(approx_eq(saw.points[5].x, 17.0f) && approx_eq(saw.points[5].y, 6.0f));

    GlyphPath square = get_glyph_path(SYNTHUI_SLIDE_TOGGLE_GLYPH_SQUARE);
    assert(square.num_points == 8);
    assert(approx_eq(square.points[0].x, 2.0f) && approx_eq(square.points[0].y, 15.0f));
    assert(approx_eq(square.points[1].x, 2.0f) && approx_eq(square.points[1].y, 7.0f));
    assert(approx_eq(square.points[7].x, 17.0f) && approx_eq(square.points[7].y, 15.0f));

    GlyphPath tri = get_glyph_path(SYNTHUI_SLIDE_TOGGLE_GLYPH_TRI);
    assert(tri.num_points == 5);
    assert(approx_eq(tri.points[0].x, 2.0f) && approx_eq(tri.points[0].y, 15.0f));
    assert(approx_eq(tri.points[1].x, 6.5f) && approx_eq(tri.points[1].y, 6.0f));
    assert(approx_eq(tri.points[2].x, 11.0f) && approx_eq(tri.points[2].y, 15.0f));
    assert(approx_eq(tri.points[3].x, 15.5f) && approx_eq(tri.points[3].y, 6.0f));
    assert(approx_eq(tri.points[4].x, 18.0f) && approx_eq(tri.points[4].y, 11.0f));

    GlyphPath pulse = get_glyph_path(SYNTHUI_SLIDE_TOGGLE_GLYPH_PULSE);
    assert(pulse.num_points == 9);
    assert(approx_eq(pulse.points[0].x, 2.0f) && approx_eq(pulse.points[0].y, 15.0f));
    assert(approx_eq(pulse.points[1].x, 5.0f) && approx_eq(pulse.points[1].y, 15.0f));
    assert(approx_eq(pulse.points[2].x, 5.0f) && approx_eq(pulse.points[2].y, 7.0f));
    assert(approx_eq(pulse.points[8].x, 16.0f) && approx_eq(pulse.points[8].y, 15.0f));

    GlyphPath none = get_glyph_path(SYNTHUI_SLIDE_TOGGLE_GLYPH_NONE);
    assert(none.num_points == 0);

    // 5. Delta damage dirty area calculation
    int32_t dx1, dy1, dx2, dy2;
    compute_knob_dirty_area(g, 100, 200, dx1, dy1, dx2, dy2);
    int32_t dmg_w = dx2 - dx1 + 1;
    int32_t dmg_h = dy2 - dy1 + 1;
    assert(dmg_w > 0 && dmg_h > 0);
    assert(dmg_w * dmg_h <= 15000);
    // Bounding box must contain well travel and knob shadow
    assert(dx1 <= 100 + static_cast<int32_t>(g.well_x * g.u));
    assert(dy1 <= 200 + static_cast<int32_t>(g.well_y * g.u));
    assert(dx2 >= 100 + static_cast<int32_t>((g.well_x + g.well_w + 1.5f) * g.u));
    assert(dy2 >= 200 + static_cast<int32_t>((g.well_y + g.well_h + 2.0f) * g.u));

    // 6. Degenerate dimension guard
    SlideToggleGeom g_bad{};
    assert(!compute_geom(0.0f, 52.0f, 2, 0,
                         SYNTHUI_SLIDE_TOGGLE_GLYPH_SAW,
                         SYNTHUI_SLIDE_TOGGLE_GLYPH_SQUARE,
                         0, false, g_bad));

    printf("PASS: synthui_slide_toggle host unit tests\n");
    return 0;
}
