/* level_meter_test.cpp - host unit test for pure level meter geometry & math.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#undef NDEBUG
#include "../src/synthui_level_meter_math.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

static bool approx_eq(float a, float b) {
    return std::fabs(a - b) < 0.05f;
}

int main() {
    using namespace synthui::level_meter;

    // 1. Layout calculation: default size W=34, H=200, N=10, value=0.6, peak=0.8
    MeterLayout layout{};
    assert(compute_layout(34.0f, 200.0f, 10, 0.6f, 0.8f, layout));
    assert(approx_eq(layout.w, 34.0f));
    assert(approx_eq(layout.h, 200.0f));
    assert(approx_eq(layout.vh, 588.0f)); // round(100 * 200 / 34)
    assert(approx_eq(layout.u, 0.34f));
    assert(layout.n == 10);
    assert(layout.lit == 6); // round(0.6 * 10)
    assert(layout.peak_idx == 7); // round(0.8 * 10) - 1

    // Geometry parameters:
    // pad = 588 * 0.02 = 11.76
    // pitch = (588 - 23.52) / 10 = 56.448
    // seg_h = 56.448 * 0.7 = 39.5136
    assert(approx_eq(layout.pad, 11.76f));
    assert(approx_eq(layout.pitch, 56.448f));
    assert(approx_eq(layout.seg_h, 39.5136f));

    // 2. Clamping of segment counts N in [3, 30]
    MeterLayout l_clamp{};
    assert(compute_layout(34.0f, 200.0f, 1, 0.5f, -1.0f, l_clamp));
    assert(l_clamp.n == 3);
    assert(compute_layout(34.0f, 200.0f, 50, 0.5f, -1.0f, l_clamp));
    assert(l_clamp.n == 30);

    // Layout dimension validation
    MeterLayout l_bad{};
    assert(!compute_layout(0.0f, 200.0f, 10, 0.5f, -1.0f, l_bad));
    assert(!compute_layout(34.0f, 0.0f, 10, 0.5f, -1.0f, l_bad));

    // Value and peak edge cases (NaN, 0, 1)
    MeterLayout l_edge{};
    float nan_val = std::numeric_limits<float>::quiet_NaN();
    assert(compute_layout(34.0f, 200.0f, 10, nan_val, nan_val, l_edge));
    assert(l_edge.lit == 0);
    assert(l_edge.peak_idx == -1);

    assert(compute_layout(34.0f, 200.0f, 10, 0.0f, 0.0f, l_edge));
    assert(l_edge.lit == 0);
    assert(l_edge.peak_idx == -1);

    assert(compute_layout(34.0f, 200.0f, 10, 1.0f, 1.0f, l_edge));
    assert(l_edge.lit == 10);
    assert(l_edge.peak_idx == 9);

    // 3. Color thresholds for N=10:
    // i=0..6: f < 0.72 -> Green
    // i=7..8: f in [0.72, 0.9) -> Amber
    // i=9: f >= 0.9 -> Red
    assert(get_segment_color(0, 10) == COLOR_GREEN);
    assert(get_segment_color(6, 10) == COLOR_GREEN); // f = 6/9 = 0.666
    assert(get_segment_color(7, 10) == COLOR_AMBER); // f = 7/9 = 0.777
    assert(get_segment_color(8, 10) == COLOR_AMBER); // f = 8/9 = 0.888
    assert(get_segment_color(9, 10) == COLOR_RED);   // f = 9/9 = 1.0

    // 4. Segment geometry verification for bottom segment (i=0) and top segment (i=9)
    SegmentGeom s0{};
    get_segment_geom(layout, 0, false, s0);
    assert(s0.on == true); // i=0 < lit(6)
    assert(s0.is_peak == false);
    assert(approx_eq(s0.slot.x, 10.0f));
    assert(approx_eq(s0.slot.w, 80.0f));
    assert(approx_eq(s0.slot.h, 39.5f));
    assert(approx_eq(s0.highlight.x, 18.0f));
    assert(approx_eq(s0.highlight.w, 64.0f));
    assert(approx_eq(s0.core_opa, 1.0f));
    assert(approx_eq(s0.hl_opa, 0.5f));
    assert(s0.color == COLOR_GREEN);

    SegmentGeom s7{}; // Peak segment
    get_segment_geom(layout, 7, false, s7);
    assert(s7.on == true); // peakIdx == 7
    assert(s7.is_peak == true); // 7 >= lit(6)
    assert(approx_eq(s7.glow_opa, 0.22f)); // isPeak glow
    assert(s7.color == COLOR_AMBER);

    SegmentGeom s9{}; // Top segment, unlit
    get_segment_geom(layout, 9, false, s9);
    assert(s9.on == false);
    assert(s9.is_peak == false);
    assert(approx_eq(s9.core_opa, 0.17f)); // ghost
    assert(approx_eq(s9.hl_opa, 0.06f));
    assert(s9.color == COLOR_RED);

    // With clip=true, top segment must turn on
    SegmentGeom s9_clipped{};
    get_segment_geom(layout, 9, true, s9_clipped);
    assert(s9_clipped.on == true);
    assert(approx_eq(s9_clipped.core_opa, 1.0f));

    // 5. Delta damage tracking:
    // When value changes from 0.6 to 0.7 (lit goes from 6 to 7),
    // segment 6 flips from OFF to ON.
    uint32_t state_old = compute_segment_states(layout, false);
    MeterLayout layout_new = layout;
    layout_new.lit = 7;
    uint32_t state_new = compute_segment_states(layout_new, false);
    assert(state_old != state_new);

    int min_flipped = -1;
    int max_flipped = -1;
    assert(get_flipped_range(state_old, state_new, layout.n, min_flipped, max_flipped));
    assert(min_flipped == 6);
    assert(max_flipped == 6);

    // Test get_flipped_bounds alias
    int min_b = -1, max_b = -1;
    assert(get_flipped_bounds(state_old, state_new, layout.n, min_b, max_b));
    assert(min_b == 6 && max_b == 6);

    // No flip case
    assert(!get_flipped_range(state_old, state_old, layout.n, min_flipped, max_flipped));

    std::printf("level_meter_test: all PASS\n");
    return 0;
}
