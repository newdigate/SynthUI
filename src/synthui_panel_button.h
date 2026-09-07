/* synthui_panel_button.h - SynthUI PanelButton, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_PANEL_BUTTON_H
#define SYNTHUI_PANEL_BUTTON_H

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>
#include "synthui_panel_button_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_obj_class_t synthui_panel_button_class;

lv_obj_t *synthui_panel_button_create(lv_obj_t *parent);

void synthui_panel_button_set_on(lv_obj_t *obj, bool on);
bool synthui_panel_button_get_on(const lv_obj_t *obj);

void synthui_panel_button_set_glyph(lv_obj_t *obj, synthui_panel_button_glyph_t glyph);
synthui_panel_button_glyph_t synthui_panel_button_get_glyph(const lv_obj_t *obj);

void synthui_panel_button_set_accent(lv_obj_t *obj, uint32_t rgb_hex);
uint32_t synthui_panel_button_get_accent(const lv_obj_t *obj);

void synthui_panel_button_set_glyph_scale(lv_obj_t *obj, float scale);
float synthui_panel_button_get_glyph_scale(const lv_obj_t *obj);

#ifdef __cplusplus
}
#endif
#endif /* SYNTHUI_PANEL_BUTTON_H */
