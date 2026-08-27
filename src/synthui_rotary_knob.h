/* synthui_rotary_knob.h - SynthUI RotaryKnob (notch), LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_ROTARY_KNOB_H
#define SYNTHUI_ROTARY_KNOB_H

#include <lvgl.h>
#include "synthui_rotary_palette.h"   /* SYNTHUI_ROTARY_ACCENT_DEFAULT */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYNTHUI_ROTARY_MODE_ENDLESS = 0,   /* well ring */
    SYNTHUI_ROTARY_MODE_BOUNDED,       /* well disc + min..max arc track */
} synthui_rotary_mode_t;

typedef enum {
    SYNTHUI_ROTARY_THEME_LIGHT = 0,
    SYNTHUI_ROTARY_THEME_DARK,
} synthui_rotary_theme_t;

extern const lv_obj_class_t synthui_rotary_knob_class;

lv_obj_t *synthui_rotary_knob_create(lv_obj_t *parent);

/* Programmatic state changes (lv_obj_add_state/remove_state) need a manual
 * lv_obj_invalidate(): no local styles, so LVGL's style-diff repaint never
 * fires (same contract as the old knob). The input layer (vertical drag,
 * 200 px = full sweep, unsnapped accumulator) emits LV_EVENT_VALUE_CHANGED. */
void  synthui_rotary_knob_set_angle(lv_obj_t *obj, float deg);       /* default 0 */
void  synthui_rotary_knob_set_mode(lv_obj_t *obj, synthui_rotary_mode_t m);
void  synthui_rotary_knob_set_theme(lv_obj_t *obj, synthui_rotary_theme_t t);
/* rgb hex (0xRRGGBB); SYNTHUI_ROTARY_ACCENT_DEFAULT reverts to theme index. */
void  synthui_rotary_knob_set_accent(lv_obj_t *obj, uint32_t rgb_hex);
void  synthui_rotary_knob_set_range(lv_obj_t *obj, float min_deg, float max_deg); /* default -150..150 */
void  synthui_rotary_knob_set_detent_step(lv_obj_t *obj, float deg); /* <=0 = continuous (default 0) */
float synthui_rotary_knob_get_angle(const lv_obj_t *obj);

#ifdef __cplusplus
}
#endif
#endif
