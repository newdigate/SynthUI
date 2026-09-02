/* synthui_fader_color.h - the fader GPU compositor's colour packing.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT
 *
 * Split out of synthui_fader_gpu.cpp so the premultiplication property below
 * is reachable by a HOST test (tests/fader_color_test.c) rather than only by
 * a silicon boot. Same pattern as synthui_fader_math.h: the test includes
 * THIS header, so it exercises the real function and cannot drift from a
 * mirrored copy. Pure integer arithmetic -- no vg_lite, no LVGL, no target. */
#ifndef SYNTHUI_FADER_COLOR_H
#define SYNTHUI_FADER_COLOR_H

#include <stdint.h>

/* vg_lite_color_t is ABGR -- red in the LOW byte (vglite_probe, measured);
 * `a` carries the sw path's opa values so the two looks stay close.
 *
 * ★★ RGB IS PREMULTIPLIED BY `a`, and that is REQUIRED, not stylistic --
 * measured on silicon 2026-09-02 by display/vglite_conformance's
 * color/premultiplied-srcover and blend/srcover-arithmetic cases (two boots,
 * both reporting model=B, and agreeing). This GC355 implements
 * VG_LITE_BLEND_SRC_OVER as
 *
 *     S + D*(1-Sa)                       <- the PREMULTIPLIED operator
 *
 * which is what inc/vg_lite.h:461 literally gives it, DESPITE :458 filing
 * mode 1 under "Non-premultiplied Blending modes" (the header's names and
 * formulas are inverted against each other: :481 gives mode 11 the
 * non-premultiplied S*Sa + D*(1-Sa), and :137 aliases THAT one
 * PREMULTIPLY_SRC_OVER). The conformance cases were built to admit both
 * readings and report which, rather than trusting either half of the header.
 *
 * Feeding that operator an UNSCALED RGB makes the source contribute at FULL
 * intensity whatever its alpha -- so the gloss below (white at opa 191) came
 * out SATURATED instead of reading as a sheen, and the cap shadow (a=115) was
 * added rather than blended. Premultiplying here makes the hardware compute
 * S*Sa + D*(1-Sa), which is exactly what LVGL's software path does with the
 * same colour and opa (synthui_fader.cpp:390 hands it bg_color 0xFFFFFF +
 * bg_opa = gloss_opa), so the two engines agree by construction.
 *
 * Fixed HERE rather than by switching to blend mode 11: mode 1's behaviour is
 * MEASURED on this silicon and mode 11's is not, and this header's naming is
 * demonstrably unreliable. Do not "simplify" this to the untested mode.
 *
 * The rounding is exact at both endpoints, which is what keeps the eight
 * opaque call sites bit-identical: at a=255, (v*255 + 127)/255 == v for every
 * v in 0..255 (the remainder 127 never reaches 255); at a=0 it is 0. So this
 * change moves ONLY the three partial-alpha draws -- the tick runs, the cap
 * shadow and the gloss. Destination alpha is 255 everywhere the fader draws
 * (it composites over an opaque panel), where premultiplied and
 * non-premultiplied D coincide, so D needs no such treatment. */
static inline uint32_t synthui_fd_pm(uint32_t v, uint32_t a)
{
    return (v * a + 127u) / 255u;
}

static inline uint32_t synthui_fd_abgr_a(uint32_t hex, uint32_t a)
{
    const uint32_t r = synthui_fd_pm((hex >> 16) & 0xFFu, a);
    const uint32_t g = synthui_fd_pm((hex >> 8) & 0xFFu, a);
    const uint32_t b = synthui_fd_pm(hex & 0xFFu, a);
    return (a << 24) | (b << 16) | (g << 8) | r;
}

#endif /* SYNTHUI_FADER_COLOR_H */
