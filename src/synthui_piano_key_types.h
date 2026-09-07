/* synthui_piano_key_types.h - types and constants for SynthUI PianoKey.
 * Header-only and LVGL-free for host testing.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_PIANO_KEY_TYPES_H
#define SYNTHUI_PIANO_KEY_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYNTHUI_PIANO_KEY_WHITE = 0,
    SYNTHUI_PIANO_KEY_BLACK = 1,
} synthui_piano_key_type_t;

/* Colors */
#define SYNTHUI_PIANO_KEY_COLOR_LED_LIT          0xFF2A20u
#define SYNTHUI_PIANO_KEY_COLOR_LED_UNLIT        0x5C1C17u
#define SYNTHUI_PIANO_KEY_COLOR_LED_BLOOM        0xFF2A20u

#define SYNTHUI_PIANO_KEY_COLOR_WHITE_A_UNPRESSED 0xF6F5F2u
#define SYNTHUI_PIANO_KEY_COLOR_WHITE_B_UNPRESSED 0xEAE8E3u
#define SYNTHUI_PIANO_KEY_COLOR_WHITE_C_UNPRESSED 0xC7C5C0u

#define SYNTHUI_PIANO_KEY_COLOR_WHITE_A_PRESSED   0xDCDAD5u
#define SYNTHUI_PIANO_KEY_COLOR_WHITE_B_PRESSED   0xCFCDC8u
#define SYNTHUI_PIANO_KEY_COLOR_WHITE_C_PRESSED   0xB2B0ABu

#define SYNTHUI_PIANO_KEY_COLOR_WHITE_EDGE       0x4A4A48u

#define SYNTHUI_PIANO_KEY_COLOR_BLACK_A          0x2B2B2Bu
#define SYNTHUI_PIANO_KEY_COLOR_BLACK_B          0x141414u
#define SYNTHUI_PIANO_KEY_COLOR_BLACK_C          0x0B0B0Bu
#define SYNTHUI_PIANO_KEY_COLOR_BLACK_EDGE       0x000000u

#define SYNTHUI_PIANO_KEY_COLOR_PAD_A            0xECECEBu
#define SYNTHUI_PIANO_KEY_COLOR_PAD_B            0xC4C4C1u
#define SYNTHUI_PIANO_KEY_COLOR_PAD_C            0x8F918Fu
#define SYNTHUI_PIANO_KEY_COLOR_PAD_EDGE         0x3A3C3Cu
#define SYNTHUI_PIANO_KEY_COLOR_PAD_RIDGE        0x7D807Eu

#ifdef __cplusplus
}
#endif
#endif /* SYNTHUI_PIANO_KEY_TYPES_H */
