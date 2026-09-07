/* synthui_panel_button_math.h - pure geometry arithmetic for SynthUI PanelButton.
 * Header-only and LVGL-free for direct host unit testing.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_PANEL_BUTTON_MATH_H
#define SYNTHUI_PANEL_BUTTON_MATH_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "synthui_panel_button_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float x, y;
} synthui_panel_button_point_t;

typedef struct {
    synthui_panel_button_point_t p[3];
} synthui_panel_button_triangle_t;

typedef struct {
    float x, y, w, h;
} synthui_panel_button_rect_t;

typedef struct {
    float cx, cy, r;
} synthui_panel_button_circle_t;

typedef struct {
    float w, h;
    float vw, vh, u;
    float i;
    float inner_w, inner_h;
    float sheen_h, sheen_y, sheen_line;
    float shadow_y, shadow_h;
    float g, s, tx, ty;
    float glow_w;
} synthui_panel_button_geom_t;

typedef struct {
    uint8_t num_triangles;
    uint8_t num_rects;
    uint8_t num_circles;
    synthui_panel_button_triangle_t triangles[2];
    synthui_panel_button_rect_t rects[1];
    synthui_panel_button_circle_t circles[1];
} synthui_panel_button_glyph_geom_t;

static inline bool synthui_panel_button_compute_geom(float w, float h, float glyph_scale,
                                                     synthui_panel_button_geom_t *g)
{
    if (w <= 0.0f || h <= 0.0f) return false;
    if (glyph_scale <= 0.0f) glyph_scale = 0.62f;

    g->w = w;
    g->h = h;
    g->vw = 100.0f;
    g->vh = roundf((100.0f * h) / w);
    g->u = w / 100.0f;

    g->i = 2.2f;
    g->inner_w = g->vw - (g->i * 2.0f);
    g->inner_h = g->vh - (g->i * 2.0f);

    g->sheen_h = g->vh * 0.11f;
    g->sheen_y = g->i + g->sheen_h;
    g->sheen_line = g->vh * 0.05f;

    g->shadow_h = g->vh * 0.1f;
    g->shadow_y = g->vh - g->i - g->shadow_h;

    const float min_dim = g->vw < g->vh ? g->vw : g->vh;
    g->g = min_dim * glyph_scale;
    g->s = g->g / 100.0f;
    g->tx = (g->vw - g->g) * 0.5f;
    g->ty = (g->vh - g->g) * 0.5f;
    g->glow_w = 14.0f * g->s;

    return true;
}

static inline void synthui_panel_button_get_glyph_geom(const synthui_panel_button_geom_t *g,
                                                       synthui_panel_button_glyph_t glyph,
                                                       synthui_panel_button_glyph_geom_t *out)
{
    out->num_triangles = 0;
    out->num_rects = 0;
    out->num_circles = 0;

    const float tx = g->tx;
    const float ty = g->ty;
    const float s = g->s;

    switch (glyph) {
    case SYNTHUI_PANEL_BUTTON_GLYPH_PLAY:
        out->num_triangles = 1;
        out->triangles[0].p[0] = (synthui_panel_button_point_t){ tx + s * 32.0f, ty + s * 18.0f };
        out->triangles[0].p[1] = (synthui_panel_button_point_t){ tx + s * 84.0f, ty + s * 50.0f };
        out->triangles[0].p[2] = (synthui_panel_button_point_t){ tx + s * 32.0f, ty + s * 82.0f };
        break;

    case SYNTHUI_PANEL_BUTTON_GLYPH_STOP:
        out->num_rects = 1;
        out->rects[0] = (synthui_panel_button_rect_t){ tx + s * 28.0f, ty + s * 28.0f, s * 44.0f, s * 44.0f };
        break;

    case SYNTHUI_PANEL_BUTTON_GLYPH_RECORD:
        out->num_circles = 1;
        out->circles[0] = (synthui_panel_button_circle_t){ tx + s * 50.0f, ty + s * 50.0f, s * 30.0f };
        break;

    case SYNTHUI_PANEL_BUTTON_GLYPH_REWIND:
        out->num_triangles = 2;
        out->triangles[0].p[0] = (synthui_panel_button_point_t){ tx + s * 50.0f, ty + s * 22.0f };
        out->triangles[0].p[1] = (synthui_panel_button_point_t){ tx + s * 20.0f, ty + s * 50.0f };
        out->triangles[0].p[2] = (synthui_panel_button_point_t){ tx + s * 50.0f, ty + s * 78.0f };
        out->triangles[1].p[0] = (synthui_panel_button_point_t){ tx + s * 80.0f, ty + s * 22.0f };
        out->triangles[1].p[1] = (synthui_panel_button_point_t){ tx + s * 50.0f, ty + s * 50.0f };
        out->triangles[1].p[2] = (synthui_panel_button_point_t){ tx + s * 80.0f, ty + s * 78.0f };
        break;

    case SYNTHUI_PANEL_BUTTON_GLYPH_FORWARD:
        out->num_triangles = 2;
        out->triangles[0].p[0] = (synthui_panel_button_point_t){ tx + s * 50.0f, ty + s * 22.0f };
        out->triangles[0].p[1] = (synthui_panel_button_point_t){ tx + s * 80.0f, ty + s * 50.0f };
        out->triangles[0].p[2] = (synthui_panel_button_point_t){ tx + s * 50.0f, ty + s * 78.0f };
        out->triangles[1].p[0] = (synthui_panel_button_point_t){ tx + s * 20.0f, ty + s * 22.0f };
        out->triangles[1].p[1] = (synthui_panel_button_point_t){ tx + s * 50.0f, ty + s * 50.0f };
        out->triangles[1].p[2] = (synthui_panel_button_point_t){ tx + s * 20.0f, ty + s * 78.0f };
        break;

    case SYNTHUI_PANEL_BUTTON_GLYPH_UP:
        out->num_triangles = 1;
        out->triangles[0].p[0] = (synthui_panel_button_point_t){ tx + s * 50.0f, ty + s * 30.0f };
        out->triangles[0].p[1] = (synthui_panel_button_point_t){ tx + s * 74.0f, ty + s * 66.0f };
        out->triangles[0].p[2] = (synthui_panel_button_point_t){ tx + s * 26.0f, ty + s * 66.0f };
        break;

    case SYNTHUI_PANEL_BUTTON_GLYPH_DOWN:
        out->num_triangles = 1;
        out->triangles[0].p[0] = (synthui_panel_button_point_t){ tx + s * 50.0f, ty + s * 70.0f };
        out->triangles[0].p[1] = (synthui_panel_button_point_t){ tx + s * 74.0f, ty + s * 34.0f };
        out->triangles[0].p[2] = (synthui_panel_button_point_t){ tx + s * 26.0f, ty + s * 34.0f };
        break;

    case SYNTHUI_PANEL_BUTTON_GLYPH_BAR:
        out->num_rects = 1;
        out->rects[0] = (synthui_panel_button_rect_t){ tx + s * 18.0f, ty + s * 42.0f, s * 64.0f, s * 16.0f };
        break;

    case SYNTHUI_PANEL_BUTTON_GLYPH_DOT:
        out->num_circles = 1;
        out->circles[0] = (synthui_panel_button_circle_t){ tx + s * 50.0f, ty + s * 50.0f, s * 16.0f };
        break;

    case SYNTHUI_PANEL_BUTTON_GLYPH_NONE:
    default:
        break;
    }
}

#ifdef __cplusplus
}
#endif
#endif /* SYNTHUI_PANEL_BUTTON_MATH_H */
