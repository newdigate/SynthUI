/* synthui_level_meter_math.h - pure geometry arithmetic for SynthUI LevelMeter.
 * Header-only and LVGL-free for direct host unit testing.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_LEVEL_METER_MATH_H
#define SYNTHUI_LEVEL_METER_MATH_H

#include "synthui_level_meter_types.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace synthui::level_meter {

inline float clamp(float val, float min_val, float max_val) {
    if (std::isnan(val)) return min_val;
    return std::max(min_val, std::min(max_val, val));
}

inline bool compute_layout(float w, float h, uint8_t segments, float value, float peak, MeterLayout &l) {
    if (w < 1.0f || h < 1.0f) return false;
    l.w = w;
    l.h = h;
    l.vh = std::round(100.0f * h / w);
    l.u = w / 100.0f;
    l.n = std::max<uint8_t>(MIN_SEGMENTS, std::min<uint8_t>(MAX_SEGMENTS, segments));
    const float val_clamped = clamp(value, 0.0f, 1.0f);
    l.lit = static_cast<int32_t>(std::round(val_clamped * l.n));
    if (!std::isnan(peak) && peak > 0.0f) {
        int32_t p_idx = static_cast<int32_t>(std::round(clamp(peak, 0.0f, 1.0f) * l.n)) - 1;
        l.peak_idx = std::max<int32_t>(-1, std::min<int32_t>(l.n - 1, p_idx));
    } else {
        l.peak_idx = -1;
    }
    l.pad = l.vh * 0.02f;
    l.pitch = (l.vh - l.pad * 2.0f) / l.n;
    l.seg_h = l.pitch * 0.7f;
    return true;
}

inline uint32_t get_segment_color(uint8_t i, uint8_t n) {
    if (n <= 1) return COLOR_GREEN;
    float f = static_cast<float>(i) / static_cast<float>(n - 1);
    if (f >= 0.9f) return COLOR_RED;
    if (f >= 0.72f) return COLOR_AMBER;
    return COLOR_GREEN;
}

inline void get_segment_geom(const MeterLayout &l, uint8_t i, bool clip, SegmentGeom &s) {
    s.color = get_segment_color(i, l.n);
    float y = l.pad + (l.n - 1 - i) * l.pitch + (l.pitch - l.seg_h) * 0.5f;
    s.on = (static_cast<int32_t>(i) < l.lit) ||
           (static_cast<int32_t>(i) == l.peak_idx) ||
           (clip && i >= l.n - 1);
    s.is_peak = (static_cast<int32_t>(i) == l.peak_idx && static_cast<int32_t>(i) >= l.lit);

    s.slot.x = 10.0f;
    s.slot.w = 80.0f;
    s.slot.y = std::round(y * 10.0f) / 10.0f;
    s.slot.h = std::round(l.seg_h * 10.0f) / 10.0f;
    s.slot.rx = l.seg_h * 0.34f;

    s.glow_w = s.on ? l.seg_h * 0.7f : 0.0f;
    s.glow_opa = s.on ? (s.is_peak ? 0.22f : 0.34f) : 0.0f;
    s.core_opa = s.on ? 1.0f : 0.17f;

    s.highlight.x = 18.0f;
    s.highlight.w = 64.0f;
    s.highlight.y = std::round((y + l.seg_h * 0.18f) * 10.0f) / 10.0f;
    s.highlight.h = std::max(0.8f, l.seg_h * 0.16f);
    s.highlight.rx = l.seg_h * 0.08f;
    s.hl_opa = s.on ? 0.5f : 0.06f;
}

inline uint32_t compute_segment_states(const MeterLayout &l, bool clip = false) {
    uint32_t mask = 0;
    for (uint8_t i = 0; i < l.n && i < 32; ++i) {
        bool on = (static_cast<int32_t>(i) < l.lit) ||
                  (static_cast<int32_t>(i) == l.peak_idx) ||
                  (clip && i >= l.n - 1);
        if (on) {
            mask |= (1u << i);
        }
    }
    return mask;
}

inline bool get_flipped_range(uint32_t state_old, uint32_t state_new, uint8_t n, int &min_flipped, int &max_flipped) {
    uint32_t diff = state_old ^ state_new;
    if (n < 32) {
        diff &= (1u << n) - 1;
    }
    if (diff == 0) {
        return false;
    }
    int min_idx = -1;
    int max_idx = -1;
    for (int i = 0; i < n && i < 32; ++i) {
        if (diff & (1u << i)) {
            if (min_idx == -1) min_idx = i;
            max_idx = i;
        }
    }
    min_flipped = min_idx;
    max_flipped = max_idx;
    return true;
}

inline bool get_flipped_bounds(uint32_t state_old, uint32_t state_new, uint8_t n, int &min_flipped, int &max_flipped) {
    return get_flipped_range(state_old, state_new, n, min_flipped, max_flipped);
}

} // namespace synthui::level_meter

#endif // SYNTHUI_LEVEL_METER_MATH_H
