/* synthui_seven_segment.cpp - SynthUI SevenSegment, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#include "synthui_seven_segment.h"
#include "synthui_seven_segment_math.h"
#include <lvgl_private.h>
#include <math.h>
#include <string.h>

#define MY_CLASS (&synthui_seven_segment_class)

typedef struct {
    lv_obj_t obj;
    char text[SYNTHUI_SEVEN_SEGMENT_MAX_CHARS + 1];
    uint32_t on_color;
    uint32_t glow_color;
    float slant;
    bool ghost;
} synthui_seven_segment_t;

static void seg_constructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void seg_destructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void seg_event(const lv_obj_class_t *cls, lv_event_t *e);
static void seg_draw(synthui_seven_segment_t *seg, lv_layer_t *layer);

const lv_obj_class_t synthui_seven_segment_class = {
    .base_class     = &lv_obj_class,
    .constructor_cb = seg_constructor,
    .destructor_cb  = seg_destructor,
    .event_cb       = seg_event,
    .name           = "synthui_seven_segment",
    .width_def      = 180,
    .height_def     = 56,
    .instance_size  = sizeof(synthui_seven_segment_t),
};

lv_obj_t *synthui_seven_segment_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&synthui_seven_segment_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

static void seg_constructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    synthui_seven_segment_t *seg = (synthui_seven_segment_t *)obj;
    strncpy(seg->text, "888", SYNTHUI_SEVEN_SEGMENT_MAX_CHARS);
    seg->text[SYNTHUI_SEVEN_SEGMENT_MAX_CHARS] = '\0';
    seg->on_color = SYNTHUI_SEVEN_SEGMENT_COLOR_DEFAULT_ON;
    seg->glow_color = SYNTHUI_SEVEN_SEGMENT_COLOR_DEFAULT_GLOW;
    seg->slant = 6.0f;
    seg->ghost = true;
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void seg_destructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    LV_UNUSED(obj);
}

static void seg_event(const lv_obj_class_t *cls, lv_event_t *e)
{
    LV_UNUSED(cls);
    if (lv_obj_event_base(MY_CLASS, e) != LV_RESULT_OK) return;
    if (lv_event_get_code(e) == LV_EVENT_DRAW_MAIN) {
        seg_draw((synthui_seven_segment_t *)lv_event_get_current_target_obj(e),
                 lv_event_get_layer(e));
    }
}

static void seg_draw(synthui_seven_segment_t *seg, lv_layer_t *layer)
{
    lv_area_t a;
    lv_obj_get_coords((lv_obj_t *)seg, &a);
    const int32_t w = lv_area_get_width(&a);
    const int32_t h = lv_area_get_height(&a);
    if (w <= 0 || h <= 0) return;

    synthui_seven_segment_geom_t g;
    if (!synthui_seven_segment_compute_layout(seg->text, (float)h, seg->slant, &g)) return;

    const lv_state_t st = lv_obj_get_state((const lv_obj_t *)seg);
    const bool disabled = (st & LV_STATE_DISABLED) != 0;

    const lv_color_t on_color = lv_color_hex(seg->on_color);
    const lv_color_t glow_color = lv_color_hex(seg->glow_color);

    const lv_opa_t lit_core_opa = disabled ? LV_OPA_50 : LV_OPA_COVER;
    const lv_opa_t lit_glow_opa = disabled ? 36 : 71;
    const lv_opa_t ghost_opa = disabled ? 14 : 28;
    const lv_opa_t punct_glow_opa = disabled ? 38 : 76;

    const int32_t glow_stroke_w = (int32_t)lroundf(7.0f * g.u);

    /* Pass 1: Cell backgrounds (#181830 well) across all cells.
     * Rendering all backgrounds before any foreground prevents cell i+1's
     * background from clipping the sheared top overhang / glow of cell i. */
    for (int ci = 0; ci < g.num_cells; ci++) {
        const synthui_seven_segment_cell_geom_t *cell = &g.cells[ci];

        lv_area_t cell_area;
        cell_area.x1 = a.x1 + (int32_t)floorf(cell->x * g.u);
        cell_area.y1 = a.y1;
        cell_area.x2 = a.x1 + (int32_t)ceilf((cell->x + cell->w + g.overhang) * g.u);
        cell_area.y2 = a.y2;

        lv_area_t intersect;
        if (!_lv_area_intersect(&intersect, &cell_area, &layer->_clip_area)) {
            continue;
        }

        lv_draw_rect_dsc_t bg_dsc;
        lv_draw_rect_dsc_init(&bg_dsc);
        bg_dsc.bg_color = lv_color_hex(0x181830);
        bg_dsc.bg_opa = LV_OPA_COVER;
        bg_dsc.radius = 0;
        lv_draw_rect(layer, &bg_dsc, &intersect);
    }

    /* Pass 2: Foreground segments (punctuation, ghost, glow stroke, core) */
    for (int ci = 0; ci < g.num_cells; ci++) {
        const synthui_seven_segment_cell_geom_t *cell = &g.cells[ci];

        /* Screen bounds of this cell */
        lv_area_t cell_area;
        cell_area.x1 = a.x1 + (int32_t)floorf(cell->x * g.u);
        cell_area.y1 = a.y1;
        cell_area.x2 = a.x1 + (int32_t)ceilf((cell->x + cell->w + g.overhang) * g.u);
        cell_area.y2 = a.y2;

        /* Clip check: skip if cell is outside layer clip area */
        lv_area_t intersect;
        if (!_lv_area_intersect(&intersect, &cell_area, &layer->_clip_area)) {
            continue;
        }

        const uint16_t mask = synthui_seven_segment_get_char_mask(cell->ch);

        /* Handle punctuation ('.' and ':') */
        if (cell->ch == '.' || cell->ch == ':') {
            synthui_seven_segment_punct_geom_t pg;
            synthui_seven_segment_get_punct_geom(cell, cell->ch, &pg);

            for (int pi = 0; pi < pg.num_circles; pi++) {
                const float sheared_cx = cell->x + pg.circles[pi].cx + (112.0f - pg.circles[pi].cy) * g.shear;
                const int32_t cx_px = a.x1 + (int32_t)lroundf(sheared_cx * g.u);
                const int32_t cy_px = a.y1 + (int32_t)lroundf(pg.circles[pi].cy * g.u);
                const int32_t r_px = (int32_t)lroundf(pg.circles[pi].r * g.u);

                /* Glow circle */
                if (glow_stroke_w > 0) {
                    const int32_t gr_px = r_px + (glow_stroke_w / 2);
                    lv_area_t glow_circ = { cx_px - gr_px, cy_px - gr_px, cx_px + gr_px, cy_px + gr_px };
                    lv_draw_rect_dsc_t gdsc;
                    lv_draw_rect_dsc_init(&gdsc);
                    gdsc.bg_color = glow_color;
                    gdsc.bg_opa = punct_glow_opa;
                    gdsc.radius = LV_RADIUS_CIRCLE;
                    lv_draw_rect(layer, &gdsc, &glow_circ);
                }

                /* Core circle */
                lv_area_t core_circ = { cx_px - r_px, cy_px - r_px, cx_px + r_px, cy_px + r_px };
                lv_draw_rect_dsc_t cdsc;
                lv_draw_rect_dsc_init(&cdsc);
                cdsc.bg_color = on_color;
                cdsc.bg_opa = lit_core_opa;
                cdsc.radius = LV_RADIUS_CIRCLE;
                lv_draw_rect(layer, &cdsc, &core_circ);
            }
            continue;
        }

        /* 2. Ghost unlit segments (if enabled) */
        if (seg->ghost) {
            for (int s = 0; s < 7; s++) {
                if ((mask & (1u << s)) != 0) continue; /* skip lit segments */

                synthui_seven_segment_poly_geom_t poly;
                synthui_seven_segment_get_segment_geom(cell, s, g.shear, &poly);

                lv_draw_triangle_dsc_t tdsc;
                lv_draw_triangle_dsc_init(&tdsc);
                tdsc.color = glow_color;
                tdsc.opa = ghost_opa;

                for (int ti = 0; ti < 4; ti++) {
                    tdsc.p[0].x = a.x1 + (int32_t)lroundf(poly.triangles[ti].p[0].x * g.u);
                    tdsc.p[0].y = a.y1 + (int32_t)lroundf(poly.triangles[ti].p[0].y * g.u);
                    tdsc.p[1].x = a.x1 + (int32_t)lroundf(poly.triangles[ti].p[1].x * g.u);
                    tdsc.p[1].y = a.y1 + (int32_t)lroundf(poly.triangles[ti].p[1].y * g.u);
                    tdsc.p[2].x = a.x1 + (int32_t)lroundf(poly.triangles[ti].p[2].x * g.u);
                    tdsc.p[2].y = a.y1 + (int32_t)lroundf(poly.triangles[ti].p[2].y * g.u);
                    lv_draw_triangle(layer, &tdsc);
                }
            }
        }

        /* 3. Lit segments: glow stroke pass */
        if (glow_stroke_w > 0) {
            for (int s = 0; s < 7; s++) {
                if ((mask & (1u << s)) == 0) continue;

                synthui_seven_segment_poly_geom_t poly;
                synthui_seven_segment_get_segment_geom(cell, s, g.shear, &poly);

                for (int vi = 0; vi < 6; vi++) {
                    int next = (vi + 1) % 6;
                    lv_draw_line_dsc_t ldsc;
                    lv_draw_line_dsc_init(&ldsc);
                    ldsc.color = glow_color;
                    ldsc.opa = lit_glow_opa;
                    ldsc.width = glow_stroke_w;
                    ldsc.round_start = 1;
                    ldsc.round_end = 1;
                    ldsc.p1.x = a.x1 + (int32_t)lroundf(poly.verts[vi].x * g.u);
                    ldsc.p1.y = a.y1 + (int32_t)lroundf(poly.verts[vi].y * g.u);
                    ldsc.p2.x = a.x1 + (int32_t)lroundf(poly.verts[next].x * g.u);
                    ldsc.p2.y = a.y1 + (int32_t)lroundf(poly.verts[next].y * g.u);
                    lv_draw_line(layer, &ldsc);
                }
            }
        }

        /* 4. Lit segments: core pass */
        for (int s = 0; s < 7; s++) {
            if ((mask & (1u << s)) == 0) continue;

            synthui_seven_segment_poly_geom_t poly;
            synthui_seven_segment_get_segment_geom(cell, s, g.shear, &poly);

            lv_draw_triangle_dsc_t tdsc;
            lv_draw_triangle_dsc_init(&tdsc);
            tdsc.color = on_color;
            tdsc.opa = lit_core_opa;

            for (int ti = 0; ti < 4; ti++) {
                tdsc.p[0].x = a.x1 + (int32_t)lroundf(poly.triangles[ti].p[0].x * g.u);
                tdsc.p[0].y = a.y1 + (int32_t)lroundf(poly.triangles[ti].p[0].y * g.u);
                tdsc.p[1].x = a.x1 + (int32_t)lroundf(poly.triangles[ti].p[1].x * g.u);
                tdsc.p[1].y = a.y1 + (int32_t)lroundf(poly.triangles[ti].p[1].y * g.u);
                tdsc.p[2].x = a.x1 + (int32_t)lroundf(poly.triangles[ti].p[2].x * g.u);
                tdsc.p[2].y = a.y1 + (int32_t)lroundf(poly.triangles[ti].p[2].y * g.u);
                lv_draw_triangle(layer, &tdsc);
            }
        }
    }
}

void synthui_seven_segment_set_text(lv_obj_t *obj, const char *text)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (!text) return;
    synthui_seven_segment_t *seg = (synthui_seven_segment_t *)obj;

    if (strncmp(seg->text, text, SYNTHUI_SEVEN_SEGMENT_MAX_CHARS) == 0) return;

    const size_t prev_len = strlen(seg->text);
    const size_t new_len = strlen(text);

    if (prev_len != new_len) {
        /* Layout length changed -> full invalidation */
        strncpy(seg->text, text, SYNTHUI_SEVEN_SEGMENT_MAX_CHARS);
        seg->text[SYNTHUI_SEVEN_SEGMENT_MAX_CHARS] = '\0';
        lv_obj_invalidate(obj);
        return;
    }

    /* Check if character cell widths changed (punctuation vs non-punctuation).
     * If punctuation status changes, cell coordinate positions shift -> full invalidation. */
    bool layout_shifted = false;
    for (size_t i = 0; i < prev_len; i++) {
        bool prev_punct = (seg->text[i] == '.' || seg->text[i] == ':');
        bool new_punct = (text[i] == '.' || text[i] == ':');
        if (prev_punct != new_punct) {
            layout_shifted = true;
            break;
        }
    }

    if (layout_shifted) {
        strncpy(seg->text, text, SYNTHUI_SEVEN_SEGMENT_MAX_CHARS);
        seg->text[SYNTHUI_SEVEN_SEGMENT_MAX_CHARS] = '\0';
        lv_obj_invalidate(obj);
        return;
    }

    /* Same length and identical cell layout: perform cell-level delta invalidation */
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const int32_t h = lv_area_get_height(&a);

    synthui_seven_segment_geom_t g;
    synthui_seven_segment_compute_layout(seg->text, (float)h, seg->slant, &g);

    const int32_t margin = (int32_t)ceilf(8.0f * g.u);

    for (size_t i = 0; i < prev_len && (int)i < g.num_cells; i++) {
        if (seg->text[i] != text[i]) {
            const synthui_seven_segment_cell_geom_t *cell = &g.cells[i];
            lv_area_t dirty;
            dirty.x1 = a.x1 + (int32_t)floorf(cell->x * g.u) - margin;
            dirty.y1 = a.y1;
            dirty.x2 = a.x1 + (int32_t)ceilf((cell->x + cell->w + g.overhang) * g.u) + margin;
            dirty.y2 = a.y2;
            lv_obj_invalidate_area(obj, &dirty);
        }
    }

    strncpy(seg->text, text, SYNTHUI_SEVEN_SEGMENT_MAX_CHARS);
    seg->text[SYNTHUI_SEVEN_SEGMENT_MAX_CHARS] = '\0';
}

const char *synthui_seven_segment_get_text(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_seven_segment_t *seg = (const synthui_seven_segment_t *)obj;
    return seg->text;
}

void synthui_seven_segment_set_accent(lv_obj_t *obj, uint32_t on_hex, uint32_t glow_hex)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_seven_segment_t *seg = (synthui_seven_segment_t *)obj;
    if (seg->on_color == on_hex && seg->glow_color == glow_hex) return;
    seg->on_color = on_hex;
    seg->glow_color = glow_hex;
    lv_obj_invalidate(obj);
}

uint32_t synthui_seven_segment_get_accent_on(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_seven_segment_t *seg = (const synthui_seven_segment_t *)obj;
    return seg->on_color;
}

uint32_t synthui_seven_segment_get_accent_glow(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_seven_segment_t *seg = (const synthui_seven_segment_t *)obj;
    return seg->glow_color;
}

void synthui_seven_segment_set_ghost(lv_obj_t *obj, bool ghost)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_seven_segment_t *seg = (synthui_seven_segment_t *)obj;
    if (seg->ghost == ghost) return;
    seg->ghost = ghost;
    lv_obj_invalidate(obj);
}

bool synthui_seven_segment_get_ghost(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_seven_segment_t *seg = (const synthui_seven_segment_t *)obj;
    return seg->ghost;
}

void synthui_seven_segment_set_slant(lv_obj_t *obj, float slant_deg)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (slant_deg < 0.0f) slant_deg = 0.0f;
    if (slant_deg > 14.0f) slant_deg = 14.0f;
    synthui_seven_segment_t *seg = (synthui_seven_segment_t *)obj;
    if (fabsf(seg->slant - slant_deg) < 0.01f) return;
    seg->slant = slant_deg;
    lv_obj_invalidate(obj);
}

float synthui_seven_segment_get_slant(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_seven_segment_t *seg = (const synthui_seven_segment_t *)obj;
    return seg->slant;
}
