/* synthui_seven_segment_types.h - types and constants for SynthUI SevenSegment.
 * Header-only and LVGL-free for host testing.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_SEVEN_SEGMENT_TYPES_H
#define SYNTHUI_SEVEN_SEGMENT_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYNTHUI_SEVEN_SEGMENT_MAX_CHARS 16

/* Standard DC reference accent colors (0xRRGGBB) */
#define SYNTHUI_SEVEN_SEGMENT_COLOR_BLUE_ON     0xDCECFF
#define SYNTHUI_SEVEN_SEGMENT_COLOR_BLUE_GLOW   0x90A8F0
#define SYNTHUI_SEVEN_SEGMENT_COLOR_CYAN_ON     0xD8F0F0
#define SYNTHUI_SEVEN_SEGMENT_COLOR_CYAN_GLOW   0x78C8D8
#define SYNTHUI_SEVEN_SEGMENT_COLOR_AMBER_ON    0xFFE0A8
#define SYNTHUI_SEVEN_SEGMENT_COLOR_AMBER_GLOW  0xF0A030
#define SYNTHUI_SEVEN_SEGMENT_COLOR_RED_ON      0xFFC8C0
#define SYNTHUI_SEVEN_SEGMENT_COLOR_RED_GLOW    0xE04848

#define SYNTHUI_SEVEN_SEGMENT_COLOR_DEFAULT_ON   SYNTHUI_SEVEN_SEGMENT_COLOR_BLUE_ON
#define SYNTHUI_SEVEN_SEGMENT_COLOR_DEFAULT_GLOW SYNTHUI_SEVEN_SEGMENT_COLOR_BLUE_GLOW

/* Segment bitmasks (a-g, dot, colon) */
#define SYNTHUI_SEG_A     (1u << 0)
#define SYNTHUI_SEG_B     (1u << 1)
#define SYNTHUI_SEG_C     (1u << 2)
#define SYNTHUI_SEG_D     (1u << 3)
#define SYNTHUI_SEG_E     (1u << 4)
#define SYNTHUI_SEG_F     (1u << 5)
#define SYNTHUI_SEG_G     (1u << 6)
#define SYNTHUI_SEG_DOT   (1u << 7)
#define SYNTHUI_SEG_COLON (1u << 8)

#ifdef __cplusplus
}
#endif
#endif /* SYNTHUI_SEVEN_SEGMENT_TYPES_H */
