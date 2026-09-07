/* lamp_test.c - host unit test for pure lamp geometry & math.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#undef NDEBUG
#include "../src/synthui_lamp_math.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int approx_eq(float a, float b) { return fabsf(a - b) < 0.05f; }

int main(void)
{
    synthui_lamp_geom_t g;

    /* 48x48 square round lamp (DC default) */
    assert(synthui_lamp_compute_geom(48.0f, 48.0f, SYNTHUI_LAMP_SHAPE_ROUND, &g));
    assert(approx_eq(g.vw, 100.0f));
    assert(approx_eq(g.vh, 100.0f));
    assert(approx_eq(g.u, 0.48f));
    assert(approx_eq(g.cx, 50.0f));
    assert(approx_eq(g.cy, 50.0f));
    assert(approx_eq(g.r, 38.0f));       /* 50 - 12 */
    assert(approx_eq(g.glow_w, 22.0f));  /* 100 * 0.22 */
    assert(approx_eq(g.glow_r, 49.0f));  /* 38 + 11 */

    /* 72x38 pill lamp */
    assert(synthui_lamp_compute_geom(72.0f, 38.0f, SYNTHUI_LAMP_SHAPE_PILL, &g));
    assert(approx_eq(g.vw, 100.0f));
    assert(approx_eq(g.vh, 53.0f));      /* round(100*38/72) = 53 */
    assert(approx_eq(g.u, 0.72f));
    assert(approx_eq(g.bx, 12.0f));
    assert(approx_eq(g.bw, 76.0f));      /* 100 - 24 */
    assert(approx_eq(g.br, 12.72f));     /* 53 * 0.24 */

    /* 52x20 bar lamp */
    assert(synthui_lamp_compute_geom(52.0f, 20.0f, SYNTHUI_LAMP_SHAPE_BAR, &g));
    assert(approx_eq(g.br, 2.0f));       /* bar corner radius is fixed at 2 */

    /* Degenerate dimensions */
    assert(!synthui_lamp_compute_geom(0.0f, 48.0f, SYNTHUI_LAMP_SHAPE_ROUND, &g));
    assert(!synthui_lamp_compute_geom(48.0f, 0.0f, SYNTHUI_LAMP_SHAPE_ROUND, &g));

    printf("lamp_test: all PASS\n");
    return 0;
}
