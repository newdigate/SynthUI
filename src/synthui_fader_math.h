/* synthui_fader_math.h - pure drag arithmetic for the fader's input layer.
 * Header-only and LVGL-free so a host compiler can unit-test it directly;
 * synthui_fader.cpp includes it for the real widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_FADER_MATH_H
#define SYNTHUI_FADER_MATH_H

/* 1:1 ANCHOR-TOTAL mapping (spec 2026-08-29 section 8): the widget anchors
 * value and press-y on PRESSED, and every PRESSING event maps the TOTAL
 * displacement -- dy_up_px = press_y - current_y (positive = finger moved
 * up) -- over the travel length.  Dragging the full travel sweeps the full
 * 0..1 range; the cap never jumps to the finger.  There is no per-poll
 * accumulator here on purpose: total mapping is what makes an overshoot
 * behave like a physical cap (see the host test's overshoot case).
 * A degenerate travel (<= 0) and a NaN displacement return the anchor. */
static inline float synthui_fader_drag(float anchor, float dy_up_px,
                                       float travel_px)
{
    if (!(dy_up_px == dy_up_px) || travel_px <= 0.0f) return anchor;
    float v = anchor + dy_up_px / travel_px;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return v;
}

#endif /* SYNTHUI_FADER_MATH_H */
