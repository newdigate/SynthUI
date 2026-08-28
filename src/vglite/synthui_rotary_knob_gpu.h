/* synthui_rotary_knob_gpu.h - opt-in GC355 rotor compositor for
 * synthui_rotary_knob. Compiled only by import_evkb_synthui(VGLITE); without
 * it the widget is fully software and this header must not be included.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_ROTARY_KNOB_GPU_H
#define SYNTHUI_ROTARY_KNOB_GPU_H
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Call AFTER vg_lite_init() succeeded (app owns the probe -- vg_lite_init
 * SPINS on absent hardware, so gate it on the chip-ID read) and AFTER the
 * LVGL display exists. Wraps + maps the framebuffer as the vg_lite target,
 * builds the notch path set once, hooks LV_EVENT_RENDER_READY, and switches
 * every synthui_rotary_knob to well-sw/rotor-gpu drawing. Returns false --
 * and changes nothing -- on any failure. */
bool synthui_rotary_gpu_begin(void *framebuffer, int32_t w, int32_t h,
                              int32_t stride_bytes);

/* Cumulative count of vg_lite_* calls that did not return VG_LITE_SUCCESS.
 * A rejected draw paints nothing while everything else looks healthy, so
 * examples must print this and hardware transcripts must show 0. */
uint32_t synthui_rotary_gpu_errors(void);

/* --- deferred (double-buffered) mode --------------------------------------
 * For displays that render off-screen and flip (lvgl_mipi_panel_create_db):
 * begin_deferred() arms the widgets (well+rotor move to the GPU) but hooks
 * NOTHING -- the app wires compose_into() as the display binding's pre-flip
 * callback, and the compositor draws into whatever back buffer it is handed
 * (buffers are wrapped and mapped lazily on first sight; two are expected).
 * This ordering -- compose BEFORE the flip request, into an off-screen
 * buffer -- is what makes the composite tear-free by construction; the
 * single-buffer begin() composites into live scanout and can flash a
 * damage-box-sized square when the scanline crosses mid-composite (seen on
 * 60 fps video). Same-frame geometry, same colors, same guards. */
bool synthui_rotary_gpu_begin_deferred(int32_t w, int32_t h,
                                       int32_t stride_bytes);
void synthui_rotary_gpu_compose_into(uint8_t *framebuffer);

#ifdef __cplusplus
}
#endif
#endif
