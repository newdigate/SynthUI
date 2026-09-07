/* synthui_lamp_math.h - pure geometry arithmetic for SynthUI Lamp.
 * Header-only and LVGL-free for direct host unit testing.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_LAMP_MATH_H
#define SYNTHUI_LAMP_MATH_H

#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYNTHUI_LAMP_SHAPE_ROUND = 0,
    SYNTHUI_LAMP_SHAPE_BAR,
    SYNTHUI_LAMP_SHAPE_PILL,
} synthui_lamp_shape_t;

typedef struct {
    float w, h;
    float vw, vh, u;
    float inner_w, inner_h, top_line;
    float cx, cy, r;
    float bx, by, bw, bh, br;
    float glow_w, glow_r;
} synthui_lamp_geom_t;

static inline bool synthui_lamp_compute_geom(float w, float h, synthui_lamp_shape_t shape,
                                             synthui_lamp_geom_t *g)
{
    if (w <= 0.0f || h <= 0.0f) return false;
    g->w = w;
    g->h = h;
    g->vw = 100.0f;
    g->vh = roundf((100.0f * h) / w);
    g->u = w / 100.0f;

    g->inner_w = g->vw - 3.2f;
    g->inner_h = g->vh - 3.2f;
    g->top_line = g->vh * 0.06f;
    if (g->top_line < 1.2f) g->top_line = 1.2f;

    const float m = 12.0f;
    const float min_dim = g->vw < g->vh ? g->vw : g->vh;

    g->cx = g->vw * 0.5f;
    g->cy = g->vh * 0.5f;
    g->r = (min_dim * 0.5f) - m;
    g->glow_w = min_dim * 0.22f;
    g->glow_r = g->r + (g->glow_w * 0.5f);

    g->bx = m;
    g->by = g->vh * 0.26f;
    g->bw = g->vw - (m * 2.0f);
    g->bh = g->vh * 0.48f;
    g->br = (shape == SYNTHUI_LAMP_SHAPE_PILL) ? (g->vh * 0.24f) : 2.0f;

    return true;
}

#ifdef __cplusplus
}
#endif
#endif
