/* piano_key_test.cpp - host unit test for pure piano key geometry & math.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#undef NDEBUG
#include "../src/synthui_piano_key_math.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static bool approx_eq(float a, float b) {
    return std::fabs(a - b) < 0.05f;
}

int main() {
    using namespace synthui::piano_key;

    // 1. Default white key geometry: w=46, h=158, type=white, lit=false, pressed=false, zone=0.5, padHeight=46
    KeyGeom gw{};
    assert(compute_geom(46.0f, 158.0f, SYNTHUI_PIANO_KEY_WHITE, false, false, 0.5f, 46.0f, gw));
    assert(approx_eq(gw.w, 46.0f));
    assert(approx_eq(gw.h, 158.0f));
    assert(approx_eq(gw.vw, 100.0f));
    assert(approx_eq(gw.vh, 343.0f)); // round(100 * 158 / 46) = round(343.478) = 343
    assert(approx_eq(gw.u, 0.46f));
    assert(gw.type == SYNTHUI_PIANO_KEY_WHITE);
    assert(!gw.lit);
    assert(!gw.pressed);
    assert(approx_eq(gw.dy, 0.0f));
    assert(approx_eq(gw.body_r, 4.0f));
    assert(approx_eq(gw.edge_w, 1.8f));
    assert(gw.edge_c == SYNTHUI_PIANO_KEY_COLOR_WHITE_EDGE);
    assert(approx_eq(gw.shade_a, 0.07f));
    assert(gw.body_a == SYNTHUI_PIANO_KEY_COLOR_WHITE_A_UNPRESSED);
    assert(gw.body_b == SYNTHUI_PIANO_KEY_COLOR_WHITE_B_UNPRESSED);
    assert(gw.body_c == SYNTHUI_PIANO_KEY_COLOR_WHITE_C_UNPRESSED);

    // LED coordinates: cx=50, ledY = vh * (zone + 0.055) = 343 * 0.555 = 190.365
    // ledR = min(14, 343 * 0.032 + 4) = min(14, 14.976) = 14
    assert(approx_eq(gw.led_cx, 50.0f));
    assert(approx_eq(gw.led_cy, 190.365f));
    assert(approx_eq(gw.led_r, 14.0f));
    assert(approx_eq(gw.led_bloom_r, 24.5f)); // 1.75 * 14 = 24.5
    assert(gw.led_fill == SYNTHUI_PIANO_KEY_COLOR_LED_UNLIT);

    // Pad geometry: padTop = vh * (zone + 0.14) = 343 * 0.64 = 219.52
    // padH = padHeight * 100 / w = 46 * 100 / 46 = 100
    assert(approx_eq(gw.pad_x, 19.0f));
    assert(approx_eq(gw.pad_w, 62.0f));
    assert(approx_eq(gw.pad_y, 219.52f));
    assert(approx_eq(gw.pad_h, 100.0f));

    // 4 Ridges on pad:
    // ridge fractions: 0.22, 0.42, 0.62, 0.82 of padH
    assert(approx_eq(gw.ridge_y[0], 22.0f));
    assert(approx_eq(gw.ridge_y[1], 42.0f));
    assert(approx_eq(gw.ridge_y[2], 62.0f));
    assert(approx_eq(gw.ridge_y[3], 82.0f));
    assert(approx_eq(gw.ridge_x1, 28.92f)); // 19 + 62 * 0.16 = 28.92
    assert(approx_eq(gw.ridge_x2, 71.08f)); // 19 + 62 * 0.84 = 71.08
    assert(approx_eq(gw.ridge_w, 3.5f));   // padH * 0.035 = 3.5

    // 2. White key pressed & lit
    KeyGeom gwp{};
    assert(compute_geom(46.0f, 158.0f, SYNTHUI_PIANO_KEY_WHITE, true, true, 0.5f, 46.0f, gwp));
    assert(gwp.lit);
    assert(gwp.pressed);
    assert(approx_eq(gwp.dy, 343.0f * 0.012f)); // 4.116
    assert(approx_eq(gwp.led_cy, 190.365f + 4.116f));
    assert(approx_eq(gwp.pad_y, 219.52f + 4.116f));
    assert(gwp.led_fill == SYNTHUI_PIANO_KEY_COLOR_LED_LIT);
    assert(gwp.body_a == SYNTHUI_PIANO_KEY_COLOR_WHITE_A_PRESSED);
    assert(gwp.body_b == SYNTHUI_PIANO_KEY_COLOR_WHITE_B_PRESSED);
    assert(gwp.body_c == SYNTHUI_PIANO_KEY_COLOR_WHITE_C_PRESSED);

    // 3. Black key default geometry: w=30, h=76, type=black, zone=0.1, padHeight=46
    KeyGeom gb{};
    assert(compute_geom(30.0f, 76.0f, SYNTHUI_PIANO_KEY_BLACK, false, false, 0.1f, 46.0f, gb));
    assert(approx_eq(gb.vh, 253.0f)); // round(100 * 76 / 30) = round(253.333) = 253
    assert(approx_eq(gb.u, 0.30f));
    assert(approx_eq(gb.body_r, 3.0f));
    assert(approx_eq(gb.edge_w, 1.6f));
    assert(gb.edge_c == SYNTHUI_PIANO_KEY_COLOR_BLACK_EDGE);
    assert(approx_eq(gb.shade_a, 0.25f));
    assert(gb.body_a == SYNTHUI_PIANO_KEY_COLOR_BLACK_A);
    assert(gb.body_b == SYNTHUI_PIANO_KEY_COLOR_BLACK_B);
    assert(gb.body_c == SYNTHUI_PIANO_KEY_COLOR_BLACK_C);

    // Black key LED: cy = 253 * (0.1 + 0.055) = 39.215
    // ledR = min(14, 253 * 0.032 + 4) = min(14, 12.096) = 12.096
    assert(approx_eq(gb.led_cy, 39.215f));
    assert(approx_eq(gb.led_r, 12.096f));

    // 4. Delta damage bounding box:
    int32_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    compute_led_dirty_area(gw, 100, 200, x1, y1, x2, y2);
    int32_t dmg_w = x2 - x1 + 1;
    int32_t dmg_h = y2 - y1 + 1;
    assert(dmg_w > 0 && dmg_h > 0);
    assert((uint32_t)(dmg_w * dmg_h) <= 1500u); // Tight LED bloom bbox

    // 5. Invalid input checks
    KeyGeom bad{};
    assert(!compute_geom(0.0f, 100.0f, SYNTHUI_PIANO_KEY_WHITE, false, false, 0.5f, 40.0f, bad));
    assert(!compute_geom(50.0f, 0.0f, SYNTHUI_PIANO_KEY_WHITE, false, false, 0.5f, 40.0f, bad));

    std::printf("piano_key_test: all PASS\n");
    return 0;
}
