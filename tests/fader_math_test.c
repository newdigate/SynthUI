/* Host-compiled unit test for the fader's pure drag math -- no LVGL, no
 * target.  Build: tests/run.sh
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#undef NDEBUG          /* assertions must survive a -DNDEBUG build; BEFORE every
                        * include, so no header can pull in assert.h first */
#include "../src/synthui_fader_math.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int approx_eq(float a, float b) { return fabsf(a - b) < 0.001f; }

int main(void)
{
    /* sign: an upward drag (positive dy_up_px) increases the value */
    assert(approx_eq(synthui_fader_drag(0.5f,  40.0f, 160.0f), 0.75f));
    assert(approx_eq(synthui_fader_drag(0.5f, -40.0f, 160.0f), 0.25f));

    /* scaling: the TRAVEL, not a hardcoded stroke, sets the sweep -- a
     * 200 px travel needs 100 px of drag for half the range */
    assert(approx_eq(synthui_fader_drag(0.0f, 160.0f, 160.0f), 1.0f));
    assert(approx_eq(synthui_fader_drag(0.0f, 100.0f, 200.0f), 0.5f));

    /* clamping at both rails */
    assert(approx_eq(synthui_fader_drag(0.9f,  500.0f, 160.0f), 1.0f));
    assert(approx_eq(synthui_fader_drag(0.1f, -500.0f, 160.0f), 0.0f));

    /* overshoot past the rail, then a smaller displacement from the SAME
     * anchor: verifies clamp-then-remap arithmetic under the anchor-fixed
     * calling convention (spec section 8).  The accumulator-vs-total
     * behavioural contrast lives at the widget layer (anchor set only on
     * PRESSED) and cannot be expressed against this pure signature. */
    assert(approx_eq(synthui_fader_drag(0.0f, 200.0f, 160.0f), 1.0f));
    assert(approx_eq(synthui_fader_drag(0.0f,  80.0f, 160.0f), 0.5f));

    /* degenerate travel: anchor unchanged (spec section 11) */
    assert(approx_eq(synthui_fader_drag(0.3f, 50.0f,  0.0f), 0.3f));
    assert(approx_eq(synthui_fader_drag(0.3f, 50.0f, -5.0f), 0.3f));

    /* NaN displacement: anchor unchanged */
    assert(approx_eq(synthui_fader_drag(0.3f, nanf(""), 160.0f), 0.3f));

    /* no motion */
    assert(approx_eq(synthui_fader_drag(0.42f, 0.0f, 160.0f), 0.42f));

    printf("fader_math: all PASS\n");
    return 0;
}
