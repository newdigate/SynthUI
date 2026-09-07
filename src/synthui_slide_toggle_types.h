/* synthui_slide_toggle_types.h - types and constants for SynthUI SlideToggle.
 * Header-only and LVGL-free for host testing.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_SLIDE_TOGGLE_TYPES_H
#define SYNTHUI_SLIDE_TOGGLE_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYNTHUI_SLIDE_TOGGLE_GLYPH_NONE   = 0,
    SYNTHUI_SLIDE_TOGGLE_GLYPH_SAW    = 1,
    SYNTHUI_SLIDE_TOGGLE_GLYPH_SQUARE = 2,
    SYNTHUI_SLIDE_TOGGLE_GLYPH_TRI    = 3,
    SYNTHUI_SLIDE_TOGGLE_GLYPH_PULSE  = 4,
} synthui_slide_toggle_glyph_t;

/* Colors from SlideToggle.dc.html */
#define SYNTHUI_SLIDE_TOGGLE_COLOR_PANEL_DEFAULT 0xB9BCBCu
#define SYNTHUI_SLIDE_TOGGLE_COLOR_HOUSING       0x0F0F10u
#define SYNTHUI_SLIDE_TOGGLE_COLOR_WELL          0x1D1D1Fu
#define SYNTHUI_SLIDE_TOGGLE_COLOR_KNOB_ACTIVE   0x2C2E2Fu
#define SYNTHUI_SLIDE_TOGGLE_COLOR_KNOB_DISABLED 0x3A3C3Du
#define SYNTHUI_SLIDE_TOGGLE_COLOR_KNOB_BORDER   0x0A0A0Bu
#define SYNTHUI_SLIDE_TOGGLE_COLOR_GLYPH_ACTIVE  0x232526u
#define SYNTHUI_SLIDE_TOGGLE_COLOR_GLYPH_DISABLED 0x8B8E8Eu
#define SYNTHUI_SLIDE_TOGGLE_COLOR_RIDGE_ACTIVE  0x6A6E70u
#define SYNTHUI_SLIDE_TOGGLE_COLOR_RIDGE_DISABLED 0x54585Au

#ifdef __cplusplus
}
#endif
#endif /* SYNTHUI_SLIDE_TOGGLE_TYPES_H */
