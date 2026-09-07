/* synthui_seven_segment_math.h - pure geometry arithmetic for SynthUI SevenSegment.
 * Header-only and LVGL-free for direct host unit testing.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_SEVEN_SEGMENT_MATH_H
#define SYNTHUI_SEVEN_SEGMENT_MATH_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "synthui_seven_segment_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    float x, y;
} synthui_seven_segment_point_t;

typedef struct {
    synthui_seven_segment_point_t p[3];
} synthui_seven_segment_triangle_t;

typedef struct {
    float cx, cy, r;
} synthui_seven_segment_circle_t;

typedef struct {
    char ch;
    float x;    /* Local X start in viewbox units */
    float w;    /* Width in viewbox units (76 for digit, 44 for punctuation) */
} synthui_seven_segment_cell_geom_t;

typedef struct {
    float h;
    float u;            /* Scale factor: h / 112.0f */
    float slant_deg;
    float shear;        /* tan(slant_deg) */
    float overhang;     /* 112.0f * shear */
    float total_view_w; /* Total viewbox width */
    int num_cells;
    synthui_seven_segment_cell_geom_t cells[SYNTHUI_SEVEN_SEGMENT_MAX_CHARS];
} synthui_seven_segment_geom_t;

typedef struct {
    int num_verts;
    synthui_seven_segment_point_t verts[6];
    int num_triangles;
    synthui_seven_segment_triangle_t triangles[4];
} synthui_seven_segment_poly_geom_t;

typedef struct {
    bool is_colon;
    int num_circles;
    synthui_seven_segment_circle_t circles[2];
} synthui_seven_segment_punct_geom_t;

static inline uint16_t synthui_seven_segment_get_char_mask(char ch)
{
    switch (ch) {
    case '0': return SYNTHUI_SEG_A | SYNTHUI_SEG_B | SYNTHUI_SEG_C | SYNTHUI_SEG_D | SYNTHUI_SEG_E | SYNTHUI_SEG_F;
    case '1': return SYNTHUI_SEG_B | SYNTHUI_SEG_C;
    case '2': return SYNTHUI_SEG_A | SYNTHUI_SEG_B | SYNTHUI_SEG_G | SYNTHUI_SEG_E | SYNTHUI_SEG_D;
    case '3': return SYNTHUI_SEG_A | SYNTHUI_SEG_B | SYNTHUI_SEG_G | SYNTHUI_SEG_C | SYNTHUI_SEG_D;
    case '4': return SYNTHUI_SEG_F | SYNTHUI_SEG_G | SYNTHUI_SEG_B | SYNTHUI_SEG_C;
    case '5': return SYNTHUI_SEG_A | SYNTHUI_SEG_F | SYNTHUI_SEG_G | SYNTHUI_SEG_C | SYNTHUI_SEG_D;
    case '6': return SYNTHUI_SEG_A | SYNTHUI_SEG_F | SYNTHUI_SEG_G | SYNTHUI_SEG_E | SYNTHUI_SEG_C | SYNTHUI_SEG_D;
    case '7': return SYNTHUI_SEG_A | SYNTHUI_SEG_B | SYNTHUI_SEG_C;
    case '8': return SYNTHUI_SEG_A | SYNTHUI_SEG_B | SYNTHUI_SEG_C | SYNTHUI_SEG_D | SYNTHUI_SEG_E | SYNTHUI_SEG_F | SYNTHUI_SEG_G;
    case '9': return SYNTHUI_SEG_A | SYNTHUI_SEG_B | SYNTHUI_SEG_C | SYNTHUI_SEG_D | SYNTHUI_SEG_F | SYNTHUI_SEG_G;
    case '-': return SYNTHUI_SEG_G;
    case '_': return SYNTHUI_SEG_D;
    case ' ': return 0;
    case '.': return SYNTHUI_SEG_DOT;
    case ':': return SYNTHUI_SEG_COLON;
    case 'A': case 'a': return SYNTHUI_SEG_A | SYNTHUI_SEG_B | SYNTHUI_SEG_C | SYNTHUI_SEG_E | SYNTHUI_SEG_F | SYNTHUI_SEG_G;
    case 'B': case 'b': return SYNTHUI_SEG_C | SYNTHUI_SEG_D | SYNTHUI_SEG_E | SYNTHUI_SEG_F | SYNTHUI_SEG_G;
    case 'C':           return SYNTHUI_SEG_A | SYNTHUI_SEG_D | SYNTHUI_SEG_E | SYNTHUI_SEG_F;
    case 'c':           return SYNTHUI_SEG_D | SYNTHUI_SEG_E | SYNTHUI_SEG_G;
    case 'D': case 'd': return SYNTHUI_SEG_B | SYNTHUI_SEG_C | SYNTHUI_SEG_D | SYNTHUI_SEG_E | SYNTHUI_SEG_G;
    case 'E': case 'e': return SYNTHUI_SEG_A | SYNTHUI_SEG_D | SYNTHUI_SEG_E | SYNTHUI_SEG_F | SYNTHUI_SEG_G;
    case 'F': case 'f': return SYNTHUI_SEG_A | SYNTHUI_SEG_E | SYNTHUI_SEG_F | SYNTHUI_SEG_G;
    case 'P': case 'p': return SYNTHUI_SEG_A | SYNTHUI_SEG_B | SYNTHUI_SEG_E | SYNTHUI_SEG_F | SYNTHUI_SEG_G;
    case 'L': case 'l': return SYNTHUI_SEG_D | SYNTHUI_SEG_E | SYNTHUI_SEG_F;
    case 'O': case 'o': return SYNTHUI_SEG_C | SYNTHUI_SEG_D | SYNTHUI_SEG_E | SYNTHUI_SEG_G;
    case 'R': case 'r': return SYNTHUI_SEG_E | SYNTHUI_SEG_G;
    case 'N': case 'n': return SYNTHUI_SEG_C | SYNTHUI_SEG_E | SYNTHUI_SEG_G;
    case 'U':           return SYNTHUI_SEG_B | SYNTHUI_SEG_C | SYNTHUI_SEG_D | SYNTHUI_SEG_E | SYNTHUI_SEG_F;
    case 'u':           return SYNTHUI_SEG_C | SYNTHUI_SEG_D | SYNTHUI_SEG_E;
    case 'H': case 'h': return SYNTHUI_SEG_B | SYNTHUI_SEG_C | SYNTHUI_SEG_E | SYNTHUI_SEG_F | SYNTHUI_SEG_G;
    case 'T': case 't': return SYNTHUI_SEG_D | SYNTHUI_SEG_E | SYNTHUI_SEG_F | SYNTHUI_SEG_G;
    case 'S': case 's': return SYNTHUI_SEG_A | SYNTHUI_SEG_F | SYNTHUI_SEG_G | SYNTHUI_SEG_C | SYNTHUI_SEG_D;
    case 'Q': case 'q': return SYNTHUI_SEG_A | SYNTHUI_SEG_B | SYNTHUI_SEG_C | SYNTHUI_SEG_F | SYNTHUI_SEG_G;
    case 'J': case 'j': return SYNTHUI_SEG_B | SYNTHUI_SEG_C | SYNTHUI_SEG_D;
    case 'Y': case 'y': return SYNTHUI_SEG_B | SYNTHUI_SEG_C | SYNTHUI_SEG_D | SYNTHUI_SEG_F | SYNTHUI_SEG_G;
    default:  return 0;
    }
}

static inline bool synthui_seven_segment_compute_layout(const char *text, float h, float slant_deg,
                                                        synthui_seven_segment_geom_t *g)
{
    if (!text || h <= 0.0f) return false;

    g->h = h;
    g->u = h / 112.0f;
    g->slant_deg = slant_deg;
    const float rad = slant_deg * (float)M_PI / 180.0f;
    g->shear = tanf(rad);
    g->overhang = 112.0f * g->shear;

    int count = 0;
    float cur_x = 0.0f;
    for (size_t i = 0; text[i] != '\0' && count < SYNTHUI_SEVEN_SEGMENT_MAX_CHARS; i++) {
        char ch = text[i];
        float w = (ch == '.' || ch == ':') ? 44.0f : 76.0f;
        g->cells[count].ch = ch;
        g->cells[count].x = cur_x;
        g->cells[count].w = w;
        cur_x += w;
        count++;
    }
    g->num_cells = count;
    g->total_view_w = cur_x + g->overhang;
    return true;
}

static inline void synthui_seven_segment_get_segment_geom(const synthui_seven_segment_cell_geom_t *cell,
                                                          int seg_idx, float shear,
                                                          synthui_seven_segment_poly_geom_t *poly)
{
    poly->num_verts = 6;
    poly->num_triangles = 4;

    const float t = 6.0f; /* T / 2 = 12 / 2 */
    float base_x = 0.0f, base_y = 0.0f, L = 0.0f;
    bool is_vert = false;

    /* Viewbox coordinates from SevenSegment.dc.html */
    switch (seg_idx) {
    case 0: /* a: h(15, 12, 34) */ base_x = 15.0f; base_y = 12.0f; L = 34.0f; is_vert = false; break;
    case 1: /* b: v(58, 15, 38) */ base_x = 58.0f; base_y = 15.0f; L = 38.0f; is_vert = true;  break;
    case 2: /* c: v(58, 59, 38) */ base_x = 58.0f; base_y = 59.0f; L = 38.0f; is_vert = true;  break;
    case 3: /* d: h(15, 100, 34)*/ base_x = 15.0f; base_y = 100.0f; L = 34.0f; is_vert = false; break;
    case 4: /* e: v(12, 59, 38) */ base_x = 12.0f; base_y = 59.0f; L = 38.0f; is_vert = true;  break;
    case 5: /* f: v(12, 15, 38) */ base_x = 12.0f; base_y = 15.0f; L = 38.0f; is_vert = true;  break;
    case 6: /* g: h(15, 56, 34) */ base_x = 15.0f; base_y = 56.0f; L = 34.0f; is_vert = false; break;
    default: return;
    }

    synthui_seven_segment_point_t raw[6];
    if (!is_vert) {
        /* h(x, y, L) */
        raw[0] = (synthui_seven_segment_point_t){ base_x, base_y };
        raw[1] = (synthui_seven_segment_point_t){ base_x + t, base_y - t };
        raw[2] = (synthui_seven_segment_point_t){ base_x + L - t, base_y - t };
        raw[3] = (synthui_seven_segment_point_t){ base_x + L, base_y };
        raw[4] = (synthui_seven_segment_point_t){ base_x + L - t, base_y + t };
        raw[5] = (synthui_seven_segment_point_t){ base_x + t, base_y + t };
    } else {
        /* v(x, y, L) */
        raw[0] = (synthui_seven_segment_point_t){ base_x, base_y };
        raw[1] = (synthui_seven_segment_point_t){ base_x + t, base_y + t };
        raw[2] = (synthui_seven_segment_point_t){ base_x + t, base_y + L - t };
        raw[3] = (synthui_seven_segment_point_t){ base_x, base_y + L };
        raw[4] = (synthui_seven_segment_point_t){ base_x - t, base_y + L - t };
        raw[5] = (synthui_seven_segment_point_t){ base_x - t, base_y + t };
    }

    /* Apply cell offset and bottom-aligned shear */
    for (int i = 0; i < 6; i++) {
        float x_local = cell->x + raw[i].x;
        float y_local = raw[i].y;
        poly->verts[i].x = x_local + (112.0f - y_local) * shear;
        poly->verts[i].y = y_local;
    }

    /* Decompose 6-gon into 4 triangles:
     * Quad 1: verts 0, 1, 4, 5 -> (0, 1, 5) and (1, 4, 5)
     * Quad 2: verts 1, 2, 3, 4 -> (1, 2, 4) and (2, 3, 4) */
    poly->triangles[0] = (synthui_seven_segment_triangle_t){ { poly->verts[0], poly->verts[1], poly->verts[5] } };
    poly->triangles[1] = (synthui_seven_segment_triangle_t){ { poly->verts[1], poly->verts[4], poly->verts[5] } };
    poly->triangles[2] = (synthui_seven_segment_triangle_t){ { poly->verts[1], poly->verts[2], poly->verts[4] } };
    poly->triangles[3] = (synthui_seven_segment_triangle_t){ { poly->verts[2], poly->verts[3], poly->verts[4] } };
}

static inline void synthui_seven_segment_get_punct_geom(const synthui_seven_segment_cell_geom_t *cell,
                                                        char ch, synthui_seven_segment_punct_geom_t *pg)
{
    const float center_x = cell->w * 0.5f; /* 22.0f for width 44 */

    if (ch == ':') {
        pg->is_colon = true;
        pg->num_circles = 2;
        pg->circles[0] = (synthui_seven_segment_circle_t){ center_x, 38.0f, 6.0f };
        pg->circles[1] = (synthui_seven_segment_circle_t){ center_x, 76.0f, 6.0f };
    } else {
        pg->is_colon = false;
        pg->num_circles = 1;
        pg->circles[0] = (synthui_seven_segment_circle_t){ center_x, 100.0f, 7.0f };
    }
}

#ifdef __cplusplus
}
#endif
#endif /* SYNTHUI_SEVEN_SEGMENT_MATH_H */
