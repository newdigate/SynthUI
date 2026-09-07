/* synthui_slide_toggle.cpp - SynthUI SlideToggle, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#include "synthui_slide_toggle.h"
#include "synthui_slide_toggle_math.h"
#include <lvgl_private.h>
#include <cmath>
#include <algorithm>

#define MY_CLASS (&synthui_slide_toggle_class)

typedef struct {
    lv_obj_t obj;
    int32_t positions;
    int32_t value;
    synthui_slide_toggle_glyph_t left_glyph;
    synthui_slide_toggle_glyph_t right_glyph;
    uint32_t panel_color;
    bool disabled;
} synthui_slide_toggle_t;

static void slide_toggle_constructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void slide_toggle_destructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void slide_toggle_event(const lv_obj_class_t *cls, lv_event_t *e);
static void slide_toggle_draw(synthui_slide_toggle_t *toggle, lv_layer_t *layer);

const lv_obj_class_t synthui_slide_toggle_class = {
    .base_class     = &lv_obj_class,
    .constructor_cb = slide_toggle_constructor,
    .destructor_cb  = slide_toggle_destructor,
    .event_cb       = slide_toggle_event,
    .name           = "synthui_slide_toggle",
    .width_def      = 150,
    .height_def     = 52,
    .instance_size  = sizeof(synthui_slide_toggle_t),
};

lv_obj_t *synthui_slide_toggle_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&synthui_slide_toggle_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

static void slide_toggle_constructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    synthui_slide_toggle_t *toggle = (synthui_slide_toggle_t *)obj;
    toggle->positions = 2;
    toggle->value = 0;
    toggle->left_glyph = SYNTHUI_SLIDE_TOGGLE_GLYPH_SAW;
    toggle->right_glyph = SYNTHUI_SLIDE_TOGGLE_GLYPH_SQUARE;
    toggle->panel_color = SYNTHUI_SLIDE_TOGGLE_COLOR_PANEL_DEFAULT;
    toggle->disabled = false;
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void slide_toggle_destructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    LV_UNUSED(obj);
}

static void slide_toggle_event(const lv_obj_class_t *cls, lv_event_t *e)
{
    LV_UNUSED(cls);
    if (lv_obj_event_base(MY_CLASS, e) != LV_RESULT_OK) return;
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    synthui_slide_toggle_t *toggle = (synthui_slide_toggle_t *)obj;

    if (code == LV_EVENT_DRAW_MAIN) {
        slide_toggle_draw(toggle, lv_event_get_layer(e));
    } else if (code == LV_EVENT_CLICKED) {
        if (!toggle->disabled) {
            int32_t next_val = (toggle->value + 1) % toggle->positions;
            synthui_slide_toggle_set_value(obj, next_val);
            lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
        }
    }
}

static void slide_toggle_draw(synthui_slide_toggle_t *toggle, lv_layer_t *layer)
{
    lv_area_t coords;
    lv_obj_get_coords((lv_obj_t *)toggle, &coords);
    const int32_t w = lv_area_get_width(&coords);
    const int32_t h = lv_area_get_height(&coords);
    if (w <= 0 || h <= 0) return;

    synthui::slide_toggle::SlideToggleGeom g;
    if (!synthui::slide_toggle::compute_geom((float)w, (float)h,
                                             toggle->positions, toggle->value,
                                             toggle->left_glyph, toggle->right_glyph,
                                             toggle->panel_color, toggle->disabled, g)) {
        return;
    }

    /* Sub-element 1: Panel background rect */
    lv_area_t panel_isect;
    if (_lv_area_intersect(&panel_isect, &coords, &layer->_clip_area)) {
        lv_draw_rect_dsc_t panel_dsc;
        lv_draw_rect_dsc_init(&panel_dsc);
        panel_dsc.bg_color = lv_color_hex(g.panel_color);
        panel_dsc.bg_opa = LV_OPA_COVER;
        panel_dsc.radius = 0;
        lv_draw_rect(layer, &panel_dsc, &coords);
    }

    /* Sub-element 2: Left glyph (if has_left) */
    if (g.has_left) {
        lv_area_t left_bbox;
        left_bbox.x1 = coords.x1;
        left_bbox.y1 = coords.y1;
        left_bbox.x2 = coords.x1 + (int32_t)lroundf(22.0f * g.u);
        left_bbox.y2 = coords.y2;

        lv_area_t left_isect;
        if (_lv_area_intersect(&left_isect, &left_bbox, &layer->_clip_area)) {
            synthui::slide_toggle::GlyphPath path = synthui::slide_toggle::get_glyph_path(g.left_glyph);
            int32_t stroke_w = (int32_t)lroundf(2.2f * g.left_scale * g.u);
            if (stroke_w < 1) stroke_w = 1;

            lv_draw_line_dsc_t line_dsc;
            lv_draw_line_dsc_init(&line_dsc);
            line_dsc.color = lv_color_hex(g.glyph_color);
            line_dsc.opa = LV_OPA_COVER;
            line_dsc.width = stroke_w;
            line_dsc.round_start = 0;
            line_dsc.round_end = 0;

            for (int i = 0; i < path.num_points - 1; i++) {
                line_dsc.p1.x = (lv_value_precise_t)lroundf((float)coords.x1 + (g.left_tx + path.points[i].x * g.left_scale) * g.u);
                line_dsc.p1.y = (lv_value_precise_t)lroundf((float)coords.y1 + (g.left_ty + path.points[i].y * g.left_scale) * g.u);
                line_dsc.p2.x = (lv_value_precise_t)lroundf((float)coords.x1 + (g.left_tx + path.points[i + 1].x * g.left_scale) * g.u);
                line_dsc.p2.y = (lv_value_precise_t)lroundf((float)coords.y1 + (g.left_ty + path.points[i + 1].y * g.left_scale) * g.u);
                lv_draw_line(layer, &line_dsc);
            }
        }
    }

    /* Sub-element 3: Right glyph (if has_right) */
    if (g.has_right) {
        lv_area_t right_bbox;
        right_bbox.x1 = coords.x1 + (int32_t)lroundf(78.0f * g.u);
        right_bbox.y1 = coords.y1;
        right_bbox.x2 = coords.x2;
        right_bbox.y2 = coords.y2;

        lv_area_t right_isect;
        if (_lv_area_intersect(&right_isect, &right_bbox, &layer->_clip_area)) {
            synthui::slide_toggle::GlyphPath path = synthui::slide_toggle::get_glyph_path(g.right_glyph);
            int32_t stroke_w = (int32_t)lroundf(2.2f * g.right_scale * g.u);
            if (stroke_w < 1) stroke_w = 1;

            lv_draw_line_dsc_t line_dsc;
            lv_draw_line_dsc_init(&line_dsc);
            line_dsc.color = lv_color_hex(g.glyph_color);
            line_dsc.opa = LV_OPA_COVER;
            line_dsc.width = stroke_w;
            line_dsc.round_start = 0;
            line_dsc.round_end = 0;

            for (int i = 0; i < path.num_points - 1; i++) {
                line_dsc.p1.x = (lv_value_precise_t)lroundf((float)coords.x1 + (g.right_tx + path.points[i].x * g.right_scale) * g.u);
                line_dsc.p1.y = (lv_value_precise_t)lroundf((float)coords.y1 + (g.right_ty + path.points[i].y * g.right_scale) * g.u);
                line_dsc.p2.x = (lv_value_precise_t)lroundf((float)coords.x1 + (g.right_tx + path.points[i + 1].x * g.right_scale) * g.u);
                line_dsc.p2.y = (lv_value_precise_t)lroundf((float)coords.y1 + (g.right_ty + path.points[i + 1].y * g.right_scale) * g.u);
                lv_draw_line(layer, &line_dsc);
            }
        }
    }

    /* Sub-element 4: Housing rect */
    lv_area_t housing_area;
    housing_area.x1 = coords.x1 + (int32_t)lroundf(g.hx * g.u);
    housing_area.y1 = coords.y1 + (int32_t)lroundf(g.hy * g.u);
    housing_area.x2 = coords.x1 + (int32_t)lroundf((g.hx + g.hw) * g.u) - 1;
    housing_area.y2 = coords.y1 + (int32_t)lroundf((g.hy + g.hh) * g.u) - 1;

    lv_area_t housing_isect;
    if (_lv_area_intersect(&housing_isect, &housing_area, &layer->_clip_area)) {
        lv_draw_rect_dsc_t h_dsc;
        lv_draw_rect_dsc_init(&h_dsc);
        h_dsc.bg_color = lv_color_hex(SYNTHUI_SLIDE_TOGGLE_COLOR_HOUSING);
        h_dsc.bg_opa = LV_OPA_COVER;
        int32_t hr = (int32_t)lroundf(1.6f * g.u);
        h_dsc.radius = hr < 1 ? 1 : hr;
        if (housing_area.x1 <= housing_area.x2 && housing_area.y1 <= housing_area.y2) {
            lv_draw_rect(layer, &h_dsc, &housing_area);
        }
    }

    /* Sub-element 5: Well rect */
    lv_area_t well_area;
    well_area.x1 = coords.x1 + (int32_t)lroundf(g.well_x * g.u);
    well_area.y1 = coords.y1 + (int32_t)lroundf(g.well_y * g.u);
    well_area.x2 = coords.x1 + (int32_t)lroundf((g.well_x + g.well_w) * g.u) - 1;
    well_area.y2 = coords.y1 + (int32_t)lroundf((g.well_y + g.well_h) * g.u) - 1;

    lv_area_t well_isect;
    if (_lv_area_intersect(&well_isect, &well_area, &layer->_clip_area)) {
        lv_draw_rect_dsc_t w_dsc;
        lv_draw_rect_dsc_init(&w_dsc);
        w_dsc.bg_color = lv_color_hex(SYNTHUI_SLIDE_TOGGLE_COLOR_WELL);
        w_dsc.bg_opa = LV_OPA_COVER;
        w_dsc.radius = 0;
        if (well_area.x1 <= well_area.x2 && well_area.y1 <= well_area.y2) {
            lv_draw_rect(layer, &w_dsc, &well_area);
        }
    }

    /* Sub-element 6: Knob */
    const int32_t kx1 = coords.x1 + (int32_t)lroundf(g.knob_x * g.u);
    const int32_t ky1 = coords.y1 + (int32_t)lroundf(g.knob_y * g.u);
    const int32_t kx2 = coords.x1 + (int32_t)lroundf((g.knob_x + g.knob_w) * g.u) - 1;
    const int32_t ky2 = coords.y1 + (int32_t)lroundf((g.knob_y + g.knob_h) * g.u) - 1;

    const int32_t sx1 = coords.x1 + (int32_t)lroundf((g.knob_x + 1.5f) * g.u);
    const int32_t sy1 = coords.y1 + (int32_t)lroundf((g.knob_y + 2.0f) * g.u);
    const int32_t sx2 = coords.x1 + (int32_t)lroundf((g.knob_x + 1.5f + g.knob_w) * g.u) - 1;
    const int32_t sy2 = coords.y1 + (int32_t)lroundf((g.knob_y + 2.0f + g.knob_h) * g.u) - 1;

    lv_area_t knob_bbox;
    knob_bbox.x1 = std::min(kx1, sx1);
    knob_bbox.y1 = std::min(ky1, sy1);
    knob_bbox.x2 = std::max(kx2, sx2);
    knob_bbox.y2 = std::max(ky2, sy2);

    lv_area_t knob_isect;
    if (_lv_area_intersect(&knob_isect, &knob_bbox, &layer->_clip_area)) {
        int32_t kr = (int32_t)lroundf(1.2f * g.u);
        if (kr < 1) kr = 1;

        /* 1. Knob shadow: fill #000000, opacity 0.45 (115/255) */
        lv_area_t shadow_area = { sx1, sy1, sx2, sy2 };
        lv_draw_rect_dsc_t s_dsc;
        lv_draw_rect_dsc_init(&s_dsc);
        s_dsc.bg_color = lv_color_hex(0x000000);
        s_dsc.bg_opa = 115;
        s_dsc.radius = kr;
        if (shadow_area.x1 <= shadow_area.x2 && shadow_area.y1 <= shadow_area.y2) {
            lv_draw_rect(layer, &s_dsc, &shadow_area);
        }

        /* 2. Knob body: fill knob_fill, stroke #0a0a0b 1px */
        lv_area_t body_area = { kx1, ky1, kx2, ky2 };
        lv_draw_rect_dsc_t b_dsc;
        lv_draw_rect_dsc_init(&b_dsc);
        b_dsc.bg_color = lv_color_hex(g.knob_fill);
        b_dsc.bg_opa = LV_OPA_COVER;
        b_dsc.border_color = lv_color_hex(SYNTHUI_SLIDE_TOGGLE_COLOR_KNOB_BORDER);
        int32_t bw = (int32_t)lroundf(1.0f * g.u);
        b_dsc.border_width = bw < 1 ? 1 : bw;
        b_dsc.border_opa = LV_OPA_COVER;
        b_dsc.radius = kr;
        if (body_area.x1 <= body_area.x2 && body_area.y1 <= body_area.y2) {
            lv_draw_rect(layer, &b_dsc, &body_area);
        }

        /* 3. Knob top highlight: fill #ffffff, opacity knob_hl_opa */
        int32_t hly2 = coords.y1 + (int32_t)lroundf((g.knob_y + g.knob_top) * g.u) - 1;
        lv_area_t hl_area = { kx1, ky1, kx2, hly2 };
        lv_draw_rect_dsc_t hl_dsc;
        lv_draw_rect_dsc_init(&hl_dsc);
        hl_dsc.bg_color = lv_color_hex(0xFFFFFF);
        hl_dsc.bg_opa = (lv_opa_t)lroundf(g.knob_hl_opa * 255.0f);
        hl_dsc.radius = kr;
        if (hl_area.x1 <= hl_area.x2 && hl_area.y1 <= hl_area.y2) {
            lv_draw_rect(layer, &hl_dsc, &hl_area);
        }

        /* 4. Knob vertical ridges: 4 lines with round caps */
        int32_t rw = (int32_t)lroundf(g.ridge_w * g.u);
        if (rw < 1) rw = 1;

        lv_draw_line_dsc_t r_dsc;
        lv_draw_line_dsc_init(&r_dsc);
        r_dsc.color = lv_color_hex(g.ridge_color);
        r_dsc.opa = LV_OPA_COVER;
        r_dsc.width = rw;
        r_dsc.round_start = 1;
        r_dsc.round_end = 1;

        const int32_t ry1 = coords.y1 + (int32_t)lroundf((g.knob_y + g.ridge_y1) * g.u);
        const int32_t ry2 = coords.y1 + (int32_t)lroundf((g.knob_y + g.ridge_y2) * g.u);

        for (int i = 0; i < 4; i++) {
            int32_t rx = coords.x1 + (int32_t)lroundf((g.knob_x + g.ridge_x[i]) * g.u);
            r_dsc.p1.x = (lv_value_precise_t)rx;
            r_dsc.p1.y = (lv_value_precise_t)ry1;
            r_dsc.p2.x = (lv_value_precise_t)rx;
            r_dsc.p2.y = (lv_value_precise_t)ry2;
            lv_draw_line(layer, &r_dsc);
        }
    }
}

void synthui_slide_toggle_set_value(lv_obj_t *obj, int32_t value)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_slide_toggle_t *toggle = (synthui_slide_toggle_t *)obj;
    int32_t clamped = std::max<int32_t>(0, std::min<int32_t>(toggle->positions - 1, value));
    if (toggle->value == clamped) return;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    const float w = (float)lv_area_get_width(&coords);
    const float h = (float)lv_area_get_height(&coords);

    synthui::slide_toggle::SlideToggleGeom g;
    if (synthui::slide_toggle::compute_geom(w, h, toggle->positions, toggle->value,
                                            toggle->left_glyph, toggle->right_glyph,
                                            toggle->panel_color, toggle->disabled, g)) {
        int32_t x1, y1, x2, y2;
        synthui::slide_toggle::compute_knob_dirty_area(g, coords.x1, coords.y1, x1, y1, x2, y2);
        toggle->value = clamped;
        lv_area_t dirty = { x1, y1, x2, y2 };
        lv_obj_invalidate_area(obj, &dirty);
    } else {
        toggle->value = clamped;
        lv_obj_invalidate(obj);
    }
}

int32_t synthui_slide_toggle_get_value(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_slide_toggle_t *toggle = (const synthui_slide_toggle_t *)obj;
    return toggle->value;
}

void synthui_slide_toggle_set_positions(lv_obj_t *obj, int32_t positions)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    int32_t clamped = std::max<int32_t>(2, std::min<int32_t>(4, positions));
    synthui_slide_toggle_t *toggle = (synthui_slide_toggle_t *)obj;
    if (toggle->positions == clamped) return;
    toggle->positions = clamped;
    if (toggle->value >= clamped) {
        toggle->value = clamped - 1;
    }
    lv_obj_invalidate(obj);
}

int32_t synthui_slide_toggle_get_positions(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_slide_toggle_t *toggle = (const synthui_slide_toggle_t *)obj;
    return toggle->positions;
}

void synthui_slide_toggle_set_left_glyph(lv_obj_t *obj, synthui_slide_toggle_glyph_t glyph)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_slide_toggle_t *toggle = (synthui_slide_toggle_t *)obj;
    if (toggle->left_glyph == glyph) return;
    toggle->left_glyph = glyph;
    lv_obj_invalidate(obj);
}

synthui_slide_toggle_glyph_t synthui_slide_toggle_get_left_glyph(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_slide_toggle_t *toggle = (const synthui_slide_toggle_t *)obj;
    return toggle->left_glyph;
}

void synthui_slide_toggle_set_right_glyph(lv_obj_t *obj, synthui_slide_toggle_glyph_t glyph)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_slide_toggle_t *toggle = (synthui_slide_toggle_t *)obj;
    if (toggle->right_glyph == glyph) return;
    toggle->right_glyph = glyph;
    lv_obj_invalidate(obj);
}

synthui_slide_toggle_glyph_t synthui_slide_toggle_get_right_glyph(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_slide_toggle_t *toggle = (const synthui_slide_toggle_t *)obj;
    return toggle->right_glyph;
}

void synthui_slide_toggle_set_panel_color(lv_obj_t *obj, uint32_t color)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_slide_toggle_t *toggle = (synthui_slide_toggle_t *)obj;
    if (toggle->panel_color == color) return;
    toggle->panel_color = color;
    lv_obj_invalidate(obj);
}

uint32_t synthui_slide_toggle_get_panel_color(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_slide_toggle_t *toggle = (const synthui_slide_toggle_t *)obj;
    return toggle->panel_color;
}

void synthui_slide_toggle_set_disabled(lv_obj_t *obj, bool disabled)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_slide_toggle_t *toggle = (synthui_slide_toggle_t *)obj;
    if (toggle->disabled == disabled) return;
    toggle->disabled = disabled;
    if (disabled) {
        lv_obj_add_state(obj, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(obj, LV_STATE_DISABLED);
    }
    lv_obj_invalidate(obj);
}

bool synthui_slide_toggle_get_disabled(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_slide_toggle_t *toggle = (const synthui_slide_toggle_t *)obj;
    return toggle->disabled;
}
