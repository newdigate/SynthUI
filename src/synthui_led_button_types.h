/* synthui_led_button_types.h - types and constants for SynthUI LedButton.
 * Header-only and LVGL-free for host testing.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_LED_BUTTON_TYPES_H
#define SYNTHUI_LED_BUTTON_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYNTHUI_LED_BUTTON_RED = 0,   /* DC default */
    SYNTHUI_LED_BUTTON_AMBER,
    SYNTHUI_LED_BUTTON_GREEN,
    SYNTHUI_LED_BUTTON_BLUE,
} synthui_led_button_color_t;

/* DC LED table, 0xRRGGBB: {on, off} per colour. */
#define SYNTHUI_LED_BUTTON_RED_ON     0xFF3B30u
#define SYNTHUI_LED_BUTTON_RED_OFF    0x5E2B28u
#define SYNTHUI_LED_BUTTON_AMBER_ON   0xFFA41Fu
#define SYNTHUI_LED_BUTTON_AMBER_OFF  0x5C3D18u
#define SYNTHUI_LED_BUTTON_GREEN_ON   0x4BE060u
#define SYNTHUI_LED_BUTTON_GREEN_OFF  0x254A2Bu
#define SYNTHUI_LED_BUTTON_BLUE_ON    0x5AA8FFu
#define SYNTHUI_LED_BUTTON_BLUE_OFF   0x22384Fu

/* Chrome, 0xRRGGBB. */
#define SYNTHUI_LED_BUTTON_BEZEL       0x1C1C1Eu
#define SYNTHUI_LED_BUTTON_BEZEL_STROKE 0x3A3A3Du
#define SYNTHUI_LED_BUTTON_CUE_STROKE  0xFF3B30u
#define SYNTHUI_LED_BUTTON_WELL        0x101012u
#define SYNTHUI_LED_BUTTON_BASE        0x8E8B84u
#define SYNTHUI_LED_BUTTON_LED_DISABLED 0x4A4A4Cu

#ifdef __cplusplus
}
#endif
#endif /* SYNTHUI_LED_BUTTON_TYPES_H */
