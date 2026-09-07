/* synthui_level_meter_types.h - types and constants for SynthUI LevelMeter.
 * Header-only and LVGL-free for host testing.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_LEVEL_METER_TYPES_H
#define SYNTHUI_LEVEL_METER_TYPES_H

#include <cstdint>
#include <cstdbool>

namespace synthui::level_meter {

constexpr uint8_t MIN_SEGMENTS = 3;
constexpr uint8_t MAX_SEGMENTS = 30;
constexpr uint8_t DEFAULT_SEGMENTS = 10;

constexpr uint32_t COLOR_RED       = 0xFF3B30;
constexpr uint32_t COLOR_AMBER     = 0xFFA41F;
constexpr uint32_t COLOR_GREEN     = 0x3BE03B;
constexpr uint32_t COLOR_SLOT_BG   = 0x12161A;
constexpr uint32_t COLOR_HIGHLIGHT = 0xFFFFFF;
constexpr uint32_t COLOR_PANEL_DEF = 0x6D7A85;

constexpr float DEFAULT_VALUE = 0.6f;
constexpr float DEFAULT_PEAK  = 0.8f;

struct Point {
    float x;
    float y;
};

struct Rect {
    float x;
    float y;
    float w;
    float h;
    float rx;
};

struct SegmentGeom {
    Rect slot;
    Rect highlight;
    uint32_t color;
    bool on;
    bool is_peak;
    float glow_w;
    float glow_opa;
    float core_opa;
    float hl_opa;
};

struct MeterLayout {
    float w;
    float h;
    float vh;
    float u;
    float pad;
    float pitch;
    float seg_h;
    uint8_t n;
    int32_t lit;
    int32_t peak_idx;
};

} // namespace synthui::level_meter

#endif // SYNTHUI_LEVEL_METER_TYPES_H
