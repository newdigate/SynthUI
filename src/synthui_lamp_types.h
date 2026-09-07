/* synthui_lamp_types.h - SynthUI Lamp types and shape definitions.
 * Pure C header without LVGL dependency.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_LAMP_TYPES_H
#define SYNTHUI_LAMP_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYNTHUI_LAMP_SHAPE_ROUND = 0,   /* circular lamp (DC default) */
    SYNTHUI_LAMP_SHAPE_BAR,         /* rectangular bar with rounded corners (r=2) */
    SYNTHUI_LAMP_SHAPE_PILL,        /* pill with fully rounded ends (r=vh*0.24) */
} synthui_lamp_shape_t;

#ifdef __cplusplus
}
#endif

#endif /* SYNTHUI_LAMP_TYPES_H */
