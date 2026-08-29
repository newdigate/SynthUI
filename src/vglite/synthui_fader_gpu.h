/* synthui_fader_gpu.h - opt-in GC355 compositor for synthui_fader.
 * Compiled only by import_evkb_synthui(VGLITE); without it the widget is
 * fully software and this header must not be included.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_FADER_GPU_H
#define SYNTHUI_FADER_GPU_H
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DEFERRED (double-buffered) mode ONLY -- the fader never had a live-scanout
 * compositor and never will (the rotary's scanout-flash finding; gpu spec
 * section 4). Call AFTER vg_lite_init() succeeded (app owns the chip-ID
 * probe -- vg_lite_init SPINS on absent hardware) and AFTER the LVGL display
 * exists. Arms the widgets (all drawing moves to the GPU); the app wires
 * compose_into() as the panel binding's pre-flip callback. Idempotent
 * (a second call is a no-op returning true); it records the target
 * dimensions and always succeeds -- unlike the rotary's begin(), there is no
 * up-front buffer to map or path set to build, so it has no failure path.
 * Per-frame failures (a rejected vg_lite_* call, or an overflowed per-call
 * path arena) are instead counted in synthui_fader_gpu_errors().
 *
 * ★ The compositor's per-call scissoring only clamps the tessellation
 * window when the driver's ts_is_fullscreen test is false (vg_lite_draw(),
 * vg_lite_path.c) -- which requires a scissor SMALLER than the target, which
 * in turn requires vg_lite_init() to have been called with tessellation
 * dimensions SMALLER than the panel (the examples use 256x256 against a
 * 720x1280 panel). Calling vg_lite_init() with the panel's own size defeats
 * per-fader scissoring here. */
bool synthui_fader_gpu_begin_deferred(int32_t w, int32_t h,
                                      int32_t stride_bytes);
void synthui_fader_gpu_compose_into(uint8_t *framebuffer);

/* Cumulative count of vg_lite_* calls that did not return VG_LITE_SUCCESS.
 * A rejected draw paints nothing while everything else looks healthy, so
 * examples must print this and hardware transcripts must show 0. */
uint32_t synthui_fader_gpu_errors(void);

#ifdef __cplusplus
}
#endif
#endif
