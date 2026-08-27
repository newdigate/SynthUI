/* synthui_rotary_knob_private.h - shared between the widget core and the
 * optional GPU compositor TU (src/vglite/). NOT part of the public API.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_ROTARY_KNOB_PRIVATE_H
#define SYNTHUI_ROTARY_KNOB_PRIVATE_H

#include "synthui_rotary_knob.h"
/* lv_obj_t by value needs the complete private type (old knob's note). */
#include <lvgl_private.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct synthui_rotary_knob_t {
    lv_obj_t obj;
    float angle, min_deg, max_deg, detent_step;
    float drag_pos;               /* unsnapped accumulator (knob_math) */
    uint32_t accent;              /* SYNTHUI_ROTARY_ACCENT_DEFAULT = none */
    synthui_rotary_mode_t  mode;
    synthui_rotary_theme_t theme;
    /* Set by DRAW_MAIN when the GPU hook is enabled: "my well was painted
     * this frame, my rotor still needs compositing". Cleared by the
     * compositor. Never set for hidden/other-screen objects, because
     * DRAW_MAIN does not run for them. */
    bool gpu_pending;
    struct synthui_rotary_knob_t *prev, *next;   /* instance registry */
} synthui_rotary_knob_t;

/* Registry of live instances (constructor links, destructor unlinks) and the
 * one flag the GPU TU flips on successful synthui_rotary_gpu_begin(). Both
 * are defined in the core so the core never references GPU symbols; a build
 * without src/vglite/ simply leaves the flag false forever. */
extern synthui_rotary_knob_t *synthui_rotary_knob_list;
extern bool synthui_rotary_gpu_enabled;

/* Resolve the palette for one instance from its LVGL state (shared by the sw
 * draw and the compositor so the two engines cannot disagree on color). */
void synthui_rotary_knob_palette(const synthui_rotary_knob_t *k,
                                 synthui_rotary_palette_t *p);

#ifdef __cplusplus
}
#endif
#endif
