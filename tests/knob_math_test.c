/* Host-compiled unit test for the pure drag math -- no LVGL, no target.
 * Build: cc -o /tmp/knob_math_test tests/knob_math_test.c && /tmp/knob_math_test
 * (or tests/run.sh)
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#undef NDEBUG          /* assertions must survive a -DNDEBUG build; BEFORE every
                        * include, so no header can pull in assert.h first */
#include "../src/synthui_knob_math.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int approx_eq(float a, float b) { return fabsf(a - b) < 0.01f; }

int main(void)
{
    /* --- continuous drag ------------------------------------------------- */
    assert(approx_eq(synthui_knob_drag(-140.0f, -140.0f, 140.0f, -200), 140.0f));
    assert(approx_eq(synthui_knob_drag(   0.0f, -140.0f, 140.0f, -200), 140.0f));
    assert(approx_eq(synthui_knob_drag(-140.0f, -140.0f, 140.0f, -100), 0.0f));
    assert(approx_eq(synthui_knob_drag(-100.0f, -140.0f, 140.0f, 200), -140.0f));
    assert(approx_eq(synthui_knob_drag(17.0f, -140.0f, 140.0f, 0), 17.0f));
    /* The span, not a hardcoded 280, scales the delta: an asymmetric range
     * of 270 deg moves 135 deg for half a stroke. Without this case a
     * hardcoded sweep would pass every other assertion here. */
    assert(approx_eq(synthui_knob_drag(0.0f, 0.0f, 270.0f, -100), 135.0f));

    /* --- ITERATIVE: the real call pattern, one poll at a time ------------
     * The widget feeds this its own previous output ~100x/second. A slow
     * drag MUST traverse; the version that snapped inside this function
     * returned 0.0 here forever, and that defect is exactly what a
     * single-call test cannot see. */
    float pos = -140.0f;
    for (int i = 0; i < 40; i++)
        pos = synthui_knob_drag(pos, -140.0f, 140.0f, -5);
    assert(approx_eq(pos, 140.0f));                    /* 40 x 5px = 200px */
    assert(approx_eq(synthui_knob_snap(pos, -140.0f, 140.0f, 35.0f), 140.0f));

    /* --- snapping, anchored on min_deg like the DRAWN lattice ------------ */
    assert(approx_eq(synthui_knob_snap(  0.0f, -140.0f, 140.0f, 35.0f),   0.0f));
    assert(approx_eq(synthui_knob_snap( 20.0f, -140.0f, 140.0f, 35.0f),  35.0f));
    assert(approx_eq(synthui_knob_snap(  5.0f, -140.0f, 140.0f, 35.0f),   0.0f));
    assert(approx_eq(synthui_knob_snap( 17.3f, -140.0f, 140.0f,  0.0f),  17.3f));
    /* NON-ALIGNED range: min is NOT a multiple of the step. Marks sit at
     * -100,-70,-40,-10,+20,... so +25 must snap to +20, NOT to +30 (which is
     * what anchoring the lattice at zero would give -- a pointer resting
     * between drawn detents). */
    assert(approx_eq(synthui_knob_snap( 25.0f, -100.0f, 100.0f, 30.0f),  20.0f));
    assert(approx_eq(synthui_knob_snap( 99.0f, -100.0f, 100.0f, 30.0f), 100.0f));

    printf("knob_math: all PASS\n");
    return 0;
}
