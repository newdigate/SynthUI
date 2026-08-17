/* synthui_knob_math.h - pure drag arithmetic for the knob's input layer.
 * Header-only and LVGL-free so a host compiler can unit-test it directly;
 * synthui_knob.cpp includes it for the real widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_KNOB_MATH_H
#define SYNTHUI_KNOB_MATH_H
#include <math.h>

/* Full sweep of (max_deg - min_deg) per this many pixels of vertical drag.
 * Sized against the RK055HDMIPI4MA0: 720x1280 on a 5.5" diagonal is ~267 DPI,
 * so 200 px is a ~19 mm thumb stroke for the whole range. */
#define SYNTHUI_KNOB_DRAG_FULL_PX 200.0f

/* Continuous drag position, NO quantisation. dy_px is SCREEN-space vertical
 * delta (LVGL vect.y: positive = downward); upward drag increases the angle,
 * the touch-synth convention.
 *
 * SNAPPING IS DELIBERATELY NOT DONE HERE, and this is the whole design. The
 * caller feeds this function its own previous output once per indev poll
 * (~10 ms). Quantise that accumulator and every per-poll delta smaller than
 * half a detent rounds straight back to where it started -- forever. Measured
 * on the pitch knob (step 280/24 deg): a 480 px drag, more than twice the
 * full-sweep stroke, moves it ZERO degrees below ~4.2 px/poll and slams to
 * the rail above it. The knob keeps the unsnapped position and snaps only
 * what it DISPLAYS -- see synthui_knob_snap. */
static inline float synthui_knob_drag(float pos_deg, float min_deg,
                                      float max_deg, int dy_px)
{
    const float span = max_deg - min_deg;
    float a = pos_deg - (float)dy_px * (span / SYNTHUI_KNOB_DRAG_FULL_PX);
    if (a < min_deg) a = min_deg;
    if (a > max_deg) a = max_deg;
    return a;
}

/* Quantise a position onto the widget's DRAWN detent lattice. That lattice
 * starts at min_deg (the draw loop places marks at min_deg + i*detent_step),
 * NOT at zero: anchoring at zero agrees only when min_deg happens to be an
 * exact multiple of detent_step, and leaves the pointer resting between drawn
 * marks whenever it is not (range -100..100, step 30 snaps to +30 while marks
 * sit at -10/+20/+50). detent_step <= 0 means continuous. */
static inline float synthui_knob_snap(float pos_deg, float min_deg,
                                      float max_deg, float detent_step)
{
    if (detent_step <= 0.0f) return pos_deg;
    float a = min_deg + roundf((pos_deg - min_deg) / detent_step) * detent_step;
    if (a < min_deg) a = min_deg;   /* snap may not escape the range */
    if (a > max_deg) a = max_deg;
    return a;
}

#endif /* SYNTHUI_KNOB_MATH_H */
