/* synthui_seven_segment.h - SynthUI SevenSegment, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_SEVEN_SEGMENT_H
#define SYNTHUI_SEVEN_SEGMENT_H

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>
#include "synthui_seven_segment_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_obj_class_t synthui_seven_segment_class;

lv_obj_t *synthui_seven_segment_create(lv_obj_t *parent);

/* Text display (clamped to SYNTHUI_SEVEN_SEGMENT_MAX_CHARS) */
void synthui_seven_segment_set_text(lv_obj_t *obj, const char *text);
const char *synthui_seven_segment_get_text(const lv_obj_t *obj);

/* Accent styling */
void synthui_seven_segment_set_accent(lv_obj_t *obj, uint32_t on_hex, uint32_t glow_hex);
uint32_t synthui_seven_segment_get_accent_on(const lv_obj_t *obj);
uint32_t synthui_seven_segment_get_accent_glow(const lv_obj_t *obj);

/* Ghost unlit segments (default true) */
void synthui_seven_segment_set_ghost(lv_obj_t *obj, bool ghost);
bool synthui_seven_segment_get_ghost(const lv_obj_t *obj);

/* Slant angle in degrees (default 6.0 deg, range 0..14) */
void synthui_seven_segment_set_slant(lv_obj_t *obj, float slant_deg);
float synthui_seven_segment_get_slant(const lv_obj_t *obj);

#ifdef __cplusplus
}
#endif
#endif /* SYNTHUI_SEVEN_SEGMENT_H */
