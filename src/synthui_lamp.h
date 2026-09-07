/* synthui_lamp.h - SynthUI Lamp, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_LAMP_H
#define SYNTHUI_LAMP_H

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>
#include "synthui_lamp_math.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SYNTHUI_LAMP_MATH_H
typedef enum {
    SYNTHUI_LAMP_SHAPE_ROUND = 0,   /* circular lamp (DC default) */
    SYNTHUI_LAMP_SHAPE_BAR,         /* rectangular bar with rounded corners (r=2) */
    SYNTHUI_LAMP_SHAPE_PILL,        /* pill with fully rounded ends (r=vh*0.24) */
} synthui_lamp_shape_t;
#endif

/* Standard DC reference lamp colors (0xRRGGBB) */
#define SYNTHUI_LAMP_COLOR_PINK     0xF078B0  /* DC default */
#define SYNTHUI_LAMP_COLOR_RED      0xE04848
#define SYNTHUI_LAMP_COLOR_GREEN    0x48E070
#define SYNTHUI_LAMP_COLOR_AMBER    0xF0A030
#define SYNTHUI_LAMP_COLOR_BLUE     0x90A8F0
#define SYNTHUI_LAMP_COLOR_CYAN     0xD8F0F0
#define SYNTHUI_LAMP_COLOR_DEFAULT  SYNTHUI_LAMP_COLOR_PINK

extern const lv_obj_class_t synthui_lamp_class;

lv_obj_t *synthui_lamp_create(lv_obj_t *parent);

void synthui_lamp_set_on(lv_obj_t *obj, bool on);
bool synthui_lamp_get_on(const lv_obj_t *obj);

void synthui_lamp_set_shape(lv_obj_t *obj, synthui_lamp_shape_t shape);
synthui_lamp_shape_t synthui_lamp_get_shape(const lv_obj_t *obj);

void synthui_lamp_set_color(lv_obj_t *obj, uint32_t rgb_hex);
uint32_t synthui_lamp_get_color(const lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif /* SYNTHUI_LAMP_H */
