/* synthui_fader.h - SynthUI Fader, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_FADER_H
#define SYNTHUI_FADER_H

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The DC panel greys (Fader.dc.html panel options); set_panel takes any
 * rgb, so these are a convenience, not an enum. */
#define SYNTHUI_FADER_PANEL_DEFAULT 0x6D7A85u
#define SYNTHUI_FADER_PANEL_DARK    0x5B6570u
#define SYNTHUI_FADER_PANEL_LIGHT   0x7D8994u
#define SYNTHUI_FADER_PANEL_DEEP    0x4A535Cu

extern const lv_obj_class_t synthui_fader_class;

lv_obj_t *synthui_fader_create(lv_obj_t *parent);

/* Programmatic state changes (lv_obj_add_state/remove_state) need a manual
 * lv_obj_invalidate(): no local styles, so LVGL's style-diff repaint never
 * fires (same contract as the rotary).  LV_STATE_PRESSED selects the active
 * cap palette, LV_STATE_DISABLED the disabled palette.  The input layer
 * (1:1 anchor-total vertical drag over the travel length) emits
 * LV_EVENT_VALUE_CHANGED. */
void  synthui_fader_set_value(lv_obj_t *obj, float v01);   /* clamp 0..1; NaN ignored; delta-invalidates */
float synthui_fader_get_value(const lv_obj_t *obj);
void  synthui_fader_set_ticks(lv_obj_t *obj, uint8_t n);   /* clamp 2..33, default 13 */
void  synthui_fader_set_center(lv_obj_t *obj, bool on);    /* default false */
void  synthui_fader_set_panel(lv_obj_t *obj, uint32_t rgb);/* default SYNTHUI_FADER_PANEL_DEFAULT */

#ifdef __cplusplus
}
#endif
#endif
