/* seven_segment_test.c - host unit test for pure seven segment geometry & math.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#undef NDEBUG
#include "../src/synthui_seven_segment_math.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int approx_eq(float a, float b) { return fabsf(a - b) < 0.05f; }

int main(void)
{
    synthui_seven_segment_geom_t g;

    /* 1. Normal layout: "140.0" at height 96, default slant 6.0 deg */
    assert(synthui_seven_segment_compute_layout("140.0", 96.0f, 6.0f, &g));
    assert(g.num_cells == 5);
    assert(approx_eq(g.h, 96.0f));
    assert(approx_eq(g.u, 96.0f / 112.0f));
    assert(approx_eq(g.slant_deg, 6.0f));
    assert(approx_eq(g.shear, tanf(6.0f * (float)M_PI / 180.0f)));
    assert(approx_eq(g.overhang, 112.0f * g.shear));

    /* Cell advances: '1', '4', '0' -> 76; '.' -> 44; '0' -> 76 */
    assert(approx_eq(g.cells[0].w, 76.0f));
    assert(approx_eq(g.cells[1].w, 76.0f));
    assert(approx_eq(g.cells[2].w, 76.0f));
    assert(approx_eq(g.cells[3].w, 44.0f)); /* Centered '.' */
    assert(approx_eq(g.cells[4].w, 76.0f));
    assert(approx_eq(g.cells[0].x, 0.0f));
    assert(approx_eq(g.cells[1].x, 76.0f));
    assert(approx_eq(g.cells[2].x, 152.0f));
    assert(approx_eq(g.cells[3].x, 228.0f));
    assert(approx_eq(g.cells[4].x, 272.0f));

    /* Total width in viewbox units = 348 + overhang */
    assert(approx_eq(g.total_view_w, 272.0f + 76.0f + g.overhang));

    /* 2. Character bitmasks */
    assert(synthui_seven_segment_get_char_mask('0') == (SYNTHUI_SEG_A | SYNTHUI_SEG_B | SYNTHUI_SEG_C | SYNTHUI_SEG_D | SYNTHUI_SEG_E | SYNTHUI_SEG_F));
    assert(synthui_seven_segment_get_char_mask('1') == (SYNTHUI_SEG_B | SYNTHUI_SEG_C));
    assert(synthui_seven_segment_get_char_mask('8') == (SYNTHUI_SEG_A | SYNTHUI_SEG_B | SYNTHUI_SEG_C | SYNTHUI_SEG_D | SYNTHUI_SEG_E | SYNTHUI_SEG_F | SYNTHUI_SEG_G));
    assert(synthui_seven_segment_get_char_mask('-') == SYNTHUI_SEG_G);
    assert(synthui_seven_segment_get_char_mask(' ') == 0);
    assert(synthui_seven_segment_get_char_mask('.') == SYNTHUI_SEG_DOT);
    assert(synthui_seven_segment_get_char_mask(':') == SYNTHUI_SEG_COLON);

    /* 3. Punctuation geometry centering */
    synthui_seven_segment_punct_geom_t pg;
    synthui_seven_segment_get_punct_geom(&g.cells[3], '.', &pg);
    assert(pg.is_colon == false);
    assert(pg.num_circles == 1);
    assert(approx_eq(pg.circles[0].cx, 22.0f)); /* Centered in 44-width cell */
    assert(approx_eq(pg.circles[0].cy, 100.0f));
    assert(approx_eq(pg.circles[0].r, 7.0f));

    synthui_seven_segment_cell_geom_t colon_cell;
    colon_cell.x = 100.0f;
    colon_cell.w = 44.0f;
    colon_cell.ch = ':';
    synthui_seven_segment_get_punct_geom(&colon_cell, ':', &pg);
    assert(pg.is_colon == true);
    assert(pg.num_circles == 2);
    assert(approx_eq(pg.circles[0].cx, 22.0f));
    assert(approx_eq(pg.circles[0].cy, 38.0f));
    assert(approx_eq(pg.circles[0].r, 6.0f));
    assert(approx_eq(pg.circles[1].cx, 22.0f));
    assert(approx_eq(pg.circles[1].cy, 76.0f));
    assert(approx_eq(pg.circles[1].r, 6.0f));

    /* 4. Hexagon segment geometry for 'a' (horizontal) */
    synthui_seven_segment_poly_geom_t sg;
    synthui_seven_segment_get_segment_geom(&g.cells[0], 0 /* seg a */, g.shear, &sg);
    assert(sg.num_triangles == 4);
    assert(sg.num_verts == 6);
    /* Vertex 0 unslanted would be (15, 12). Sheared: 15 + (112 - 12) * shear */
    assert(approx_eq(sg.verts[0].y, 12.0f));
    assert(approx_eq(sg.verts[0].x, 15.0f + (112.0f - 12.0f) * g.shear));

    /* 5. Clamping strings > 16 chars */
    assert(synthui_seven_segment_compute_layout("01234567890123456789", 96.0f, 6.0f, &g));
    assert(g.num_cells == SYNTHUI_SEVEN_SEGMENT_MAX_CHARS);

    /* 6. Degenerate inputs */
    assert(!synthui_seven_segment_compute_layout("140", 0.0f, 6.0f, &g));
    assert(!synthui_seven_segment_compute_layout("140", -10.0f, 6.0f, &g));
    assert(!synthui_seven_segment_compute_layout(NULL, 96.0f, 6.0f, &g));

    /* 7. Color constants */
    assert(SYNTHUI_SEVEN_SEGMENT_COLOR_BLUE_ON == 0xDCECFF);
    assert(SYNTHUI_SEVEN_SEGMENT_COLOR_BLUE_GLOW == 0x90A8F0);
    assert(SYNTHUI_SEVEN_SEGMENT_COLOR_CYAN_ON == 0xD8F0F0);
    assert(SYNTHUI_SEVEN_SEGMENT_COLOR_AMBER_ON == 0xFFE0A8);
    assert(SYNTHUI_SEVEN_SEGMENT_COLOR_RED_ON == 0xFFC8C0);

    printf("seven_segment_test: all PASS\n");
    return 0;
}
