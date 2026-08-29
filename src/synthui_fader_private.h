/* synthui_fader_private.h - shared between the widget core and the
 * optional GPU compositor TU (src/vglite/). NOT part of the public API.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_FADER_PRIVATE_H
#define SYNTHUI_FADER_PRIVATE_H

#include "synthui_fader.h"
/* lv_obj_t by value needs the complete private type (the rotary's note). */
#include <lvgl_private.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct synthui_fader_t {
    lv_obj_t obj;
    float value;          /* 0..1; 1 = cap at the top */
    float press_anchor;   /* value at PRESSED (anchor-total drag) */
    float press_y;        /* screen y at PRESSED */
    uint32_t panel;
    uint8_t ticks;
    bool center;
    /* Set by DRAW_MAIN when the GPU hook is enabled: "my draw was SKIPPED
     * this frame; I still need full compositing" -- nothing was painted
     * for this instance, so the compositor must cover the whole footprint
     * (see the fd_draw gate comment). Cleared by the compositor. Never set
     * for hidden/other-screen objects, because DRAW_MAIN does not run for
     * them (the rotary's contract). */
    bool gpu_pending;
    struct synthui_fader_t *prev, *next;   /* instance registry */
} synthui_fader_t;

/* Registry of live instances (constructor links, destructor unlinks) and the
 * one flag the GPU TU flips on successful synthui_fader_gpu_begin_deferred().
 * Both are defined in the core so the core never references GPU symbols; a
 * build without src/vglite/ simply leaves the flag false forever.
 * Invariant the compositor relies on: no instance may be created or
 * destroyed while a compose pass is walking this list. True today because
 * LVGL is single-threaded and compositing runs inside the render/flip
 * path, but nothing enforces it. */
extern synthui_fader_t *synthui_fader_list;
extern bool synthui_fader_gpu_enabled;

/* Value-independent geometry in viewBox units (width 100, vh = 100*H/W,
 * u = px per unit). false = degenerate size; nothing may be drawn. */
typedef struct {
    float u, vh, cap_h, top, travel;
} synthui_fader_geom_t;
bool  synthui_fader_geom(const synthui_fader_t *f, synthui_fader_geom_t *g,
                         lv_area_t *coords);
float synthui_fader_cap_y(const synthui_fader_geom_t *g, float value);

/* Resolve the palette for one instance from its LVGL state (shared by the
 * sw draw and the compositor so the two engines cannot disagree on color). */
typedef struct {
    uint32_t cap_top, cap_mid, cap_low, ticks, center;
    lv_opa_t gloss_opa;
} synthui_fader_palette_t;
void synthui_fader_palette(const synthui_fader_t *f,
                           synthui_fader_palette_t *p);

#ifdef __cplusplus
}
#endif
#endif
