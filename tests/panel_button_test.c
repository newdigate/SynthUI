/* panel_button_test.c - host unit test for pure panel button geometry & math.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#undef NDEBUG
#include "../src/synthui_panel_button_math.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int approx_eq(float a, float b) { return fabsf(a - b) < 0.05f; }

int main(void)
{
    synthui_panel_button_geom_t g;

    /* 74x58 Transport button (DC default) */
    assert(synthui_panel_button_compute_geom(74.0f, 58.0f, 0.62f, &g));
    assert(approx_eq(g.vw, 100.0f));
    assert(approx_eq(g.vh, 78.0f));       /* round(100*58/74) = 78 */
    assert(approx_eq(g.u, 0.74f));
    assert(approx_eq(g.i, 2.2f));
    assert(approx_eq(g.inner_w, 95.6f));  /* 100 - 4.4 */
    assert(approx_eq(g.inner_h, 73.6f));  /* 78 - 4.4 */
    assert(approx_eq(g.sheen_h, 8.58f));  /* 78 * 0.11 */
    assert(approx_eq(g.sheen_y, 10.78f)); /* 2.2 + 8.58 */
    assert(approx_eq(g.sheen_line, 3.9f));/* 78 * 0.05 */
    assert(approx_eq(g.shadow_h, 7.8f));  /* 78 * 0.1 */
    assert(approx_eq(g.shadow_y, 68.0f)); /* 78 - 2.2 - 7.8 */
    assert(approx_eq(g.g, 48.36f));       /* min(100, 78) * 0.62 = 48.36 */
    assert(approx_eq(g.s, 0.4836f));
    assert(approx_eq(g.tx, 25.82f));      /* (100 - 48.36)/2 */
    assert(approx_eq(g.ty, 14.82f));      /* (78 - 48.36)/2 */

    /* Verify Play glyph geometry (1 triangle) */
    synthui_panel_button_glyph_geom_t gg;
    synthui_panel_button_get_glyph_geom(&g, SYNTHUI_PANEL_BUTTON_GLYPH_PLAY, &gg);
    assert(gg.num_triangles == 1);
    assert(gg.num_rects == 0);
    assert(gg.num_circles == 0);
    assert(approx_eq(gg.triangles[0].p[0].x, 25.82f + 0.4836f * 32.0f));
    assert(approx_eq(gg.triangles[0].p[0].y, 14.82f + 0.4836f * 18.0f));

    /* Verify Stop glyph geometry (1 rect) */
    synthui_panel_button_get_glyph_geom(&g, SYNTHUI_PANEL_BUTTON_GLYPH_STOP, &gg);
    assert(gg.num_triangles == 0);
    assert(gg.num_rects == 1);
    assert(gg.num_circles == 0);
    assert(approx_eq(gg.rects[0].w, 0.4836f * 44.0f));
    assert(approx_eq(gg.rects[0].h, 0.4836f * 44.0f));

    /* Verify Record glyph geometry (1 circle) */
    synthui_panel_button_get_glyph_geom(&g, SYNTHUI_PANEL_BUTTON_GLYPH_RECORD, &gg);
    assert(gg.num_triangles == 0);
    assert(gg.num_rects == 0);
    assert(gg.num_circles == 1);
    assert(approx_eq(gg.circles[0].r, 0.4836f * 30.0f));

    /* Verify Rewind glyph geometry (2 triangles) */
    synthui_panel_button_get_glyph_geom(&g, SYNTHUI_PANEL_BUTTON_GLYPH_REWIND, &gg);
    assert(gg.num_triangles == 2);

    /* Verify None glyph geometry */
    synthui_panel_button_get_glyph_geom(&g, SYNTHUI_PANEL_BUTTON_GLYPH_NONE, &gg);
    assert(gg.num_triangles == 0 && gg.num_rects == 0 && gg.num_circles == 0);

    /* Color definitions */
    assert(SYNTHUI_PANEL_BUTTON_ACCENT_GREEN == 0x48E070u);
    assert(SYNTHUI_PANEL_BUTTON_ACCENT_AMBER == 0xF0A030u);
    assert(SYNTHUI_PANEL_BUTTON_ACCENT_RED == 0xF04848u);
    assert(SYNTHUI_PANEL_BUTTON_ACCENT_BLUE == 0x90A8F0u);
    assert(SYNTHUI_PANEL_BUTTON_ACCENT_PALE == 0xD8F0F0u);
    assert(SYNTHUI_PANEL_BUTTON_ACCENT_NONE == 0x303048u);

    /* Degenerate dimensions */
    assert(!synthui_panel_button_compute_geom(0.0f, 58.0f, 0.62f, &g));
    assert(!synthui_panel_button_compute_geom(74.0f, 0.0f, 0.62f, &g));

    printf("panel_button_test: all PASS\n");
    return 0;
}
