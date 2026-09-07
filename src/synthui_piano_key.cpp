/* synthui_piano_key.cpp - SynthUI PianoKey, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#include "synthui_piano_key.h"
#include "synthui_piano_key_math.h"
#include <lvgl_private.h>
#include <cmath>
#include <algorithm>

#define MY_CLASS (&synthui_piano_key_class)

typedef struct {
    lv_obj_t obj;
    synthui_piano_key_type_t type;
    bool lit;
    bool pressed;
    float zone_top;
    float pad_height;
} synthui_piano_key_t;

static void key_constructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void key_destructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void key_event(const lv_obj_class_t *cls, lv_event_t *e);
static void key_draw(synthui_piano_key_t *key, lv_layer_t *layer);

const lv_obj_class_t synthui_piano_key_class = {
    .base_class     = &lv_obj_class,
    .constructor_cb = key_constructor,
    .destructor_cb  = key_destructor,
    .event_cb       = key_event,
    .name           = "synthui_piano_key",
    .width_def      = 46,
    .height_def     = 158,
    .instance_size  = sizeof(synthui_piano_key_t),
};

lv_obj_t *synthui_piano_key_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&synthui_piano_key_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

static void key_constructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    synthui_piano_key_t *key = (synthui_piano_key_t *)obj;
    key->type = SYNTHUI_PIANO_KEY_WHITE;
    key->lit = false;
    key->pressed = false;
    key->zone_top = 0.58f;
    key->pad_height = 48.0f;
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void key_destructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    LV_UNUSED(obj);
}

static void key_event(const lv_obj_class_t *cls, lv_event_t *e)
{
    LV_UNUSED(cls);
    if (lv_obj_event_base(MY_CLASS, e) != LV_RESULT_OK) return;
    if (lv_event_get_code(e) == LV_EVENT_DRAW_MAIN) {
        key_draw((synthui_piano_key_t *)lv_event_get_current_target_obj(e),
                 lv_event_get_layer(e));
    }
}

static void key_draw(synthui_piano_key_t *key, lv_layer_t *layer)
{
    lv_area_t coords;
    lv_obj_get_coords((lv_obj_t *)key, &coords);
    const int32_t w = lv_area_get_width(&coords);
    const int32_t h = lv_area_get_height(&coords);
    if (w <= 0 || h <= 0) return;

    synthui::piano_key::KeyGeom g;
    if (!synthui::piano_key::compute_geom((float)w, (float)h, key->type, key->lit, key->pressed,
                                          key->zone_top, key->pad_height, g)) {
        return;
    }

    /* Sub-element 1: Key body rect & border & shade band */
    lv_area_t body_isect;
    if (_lv_area_intersect(&body_isect, &coords, &layer->_clip_area)) {
        int32_t split32 = coords.x1 + (int32_t)lroundf(32.0f * g.u);
        if (split32 > coords.x2) split32 = coords.x2;

        /* Left sub-rect: x from x1 to x1 + round(32*u), grad body_a to body_b */
        lv_area_t left_area;
        left_area.x1 = coords.x1;
        left_area.x2 = split32;
        left_area.y1 = coords.y1;
        left_area.y2 = coords.y2;

        lv_draw_rect_dsc_t left_dsc;
        lv_draw_rect_dsc_init(&left_dsc);
        left_dsc.bg_opa = LV_OPA_COVER;
        left_dsc.bg_grad.dir = LV_GRAD_DIR_HOR;
        left_dsc.bg_grad.stops_count = 2;
        left_dsc.bg_grad.stops[0].color = lv_color_hex(g.body_a);
        left_dsc.bg_grad.stops[0].opa = LV_OPA_COVER;
        left_dsc.bg_grad.stops[0].frac = 0;
        left_dsc.bg_grad.stops[1].color = lv_color_hex(g.body_b);
        left_dsc.bg_grad.stops[1].opa = LV_OPA_COVER;
        left_dsc.bg_grad.stops[1].frac = 255;
        left_dsc.radius = 0;
        if (left_area.x1 <= left_area.x2 && left_area.y1 <= left_area.y2) {
            lv_draw_rect(layer, &left_dsc, &left_area);
        }

        /* Right sub-rect: x from x1 + round(32*u) to x2, grad body_b to body_c */
        lv_area_t right_area;
        right_area.x1 = split32;
        right_area.x2 = coords.x2;
        right_area.y1 = coords.y1;
        right_area.y2 = coords.y2;

        lv_draw_rect_dsc_t right_dsc;
        lv_draw_rect_dsc_init(&right_dsc);
        right_dsc.bg_opa = LV_OPA_COVER;
        right_dsc.bg_grad.dir = LV_GRAD_DIR_HOR;
        right_dsc.bg_grad.stops_count = 2;
        right_dsc.bg_grad.stops[0].color = lv_color_hex(g.body_b);
        right_dsc.bg_grad.stops[0].opa = LV_OPA_COVER;
        right_dsc.bg_grad.stops[0].frac = 0;
        right_dsc.bg_grad.stops[1].color = lv_color_hex(g.body_c);
        right_dsc.bg_grad.stops[1].opa = LV_OPA_COVER;
        right_dsc.bg_grad.stops[1].frac = 255;
        right_dsc.radius = 0;
        if (right_area.x1 <= right_area.x2 && right_area.y1 <= right_area.y2) {
            lv_draw_rect(layer, &right_dsc, &right_area);
        }

        /* Right shade band: x from 78 to 100 (x1 + round(78*u) to x2), black with shade_a */
        int32_t split78 = coords.x1 + (int32_t)lroundf(78.0f * g.u);
        if (split78 > coords.x2) split78 = coords.x2;

        lv_area_t shade_area;
        shade_area.x1 = split78;
        shade_area.x2 = coords.x2;
        shade_area.y1 = coords.y1;
        shade_area.y2 = coords.y2;

        lv_draw_rect_dsc_t shade_dsc;
        lv_draw_rect_dsc_init(&shade_dsc);
        shade_dsc.bg_color = lv_color_hex(0x000000);
        shade_dsc.bg_opa = (lv_opa_t)lroundf(g.shade_a * 255.0f);
        shade_dsc.radius = 0;
        if (shade_area.x1 <= shade_area.x2 && shade_area.y1 <= shade_area.y2) {
            lv_draw_rect(layer, &shade_dsc, &shade_area);
        }

        /* Key border stroke: perimeter rect */
        lv_draw_rect_dsc_t border_dsc;
        lv_draw_rect_dsc_init(&border_dsc);
        border_dsc.bg_opa = LV_OPA_TRANSP;
        border_dsc.border_color = lv_color_hex(g.edge_c);
        int32_t edge_w_px = (int32_t)lroundf(g.edge_w * g.u);
        border_dsc.border_width = edge_w_px < 1 ? 1 : edge_w_px;
        border_dsc.border_opa = LV_OPA_COVER;
        int32_t body_r_px = (int32_t)lroundf(g.body_r * g.u);
        border_dsc.radius = body_r_px < 1 ? 1 : body_r_px;
        lv_draw_rect(layer, &border_dsc, &coords);
    }

    /* Sub-element 2: LED indicator */
    int32_t led_x1, led_y1, led_x2, led_y2;
    synthui::piano_key::compute_led_dirty_area(g, coords.x1, coords.y1, led_x1, led_y1, led_x2, led_y2);
    lv_area_t led_bbox = { led_x1, led_y1, led_x2, led_y2 };
    lv_area_t led_isect;
    if (_lv_area_intersect(&led_isect, &led_bbox, &layer->_clip_area)) {
        const int32_t cx_px = coords.x1 + (int32_t)lroundf(g.led_cx * g.u);
        const int32_t cy_px = coords.y1 + (int32_t)lroundf(g.led_cy * g.u);

        /* Bloom halo circle if lit */
        if (key->lit) {
            const int32_t bloom_r = (int32_t)lroundf(g.led_bloom_r * g.u);
            lv_area_t bloom_area;
            bloom_area.x1 = cx_px - bloom_r;
            bloom_area.y1 = cy_px - bloom_r;
            bloom_area.x2 = cx_px + bloom_r - 1;
            bloom_area.y2 = cy_px + bloom_r - 1;

            lv_draw_rect_dsc_t bloom_dsc;
            lv_draw_rect_dsc_init(&bloom_dsc);
            bloom_dsc.bg_color = lv_color_hex(SYNTHUI_PIANO_KEY_COLOR_LED_BLOOM);
            bloom_dsc.bg_opa = 87;
            bloom_dsc.radius = LV_RADIUS_CIRCLE;
            lv_draw_rect(layer, &bloom_dsc, &bloom_area);
        }

        /* Core circle */
        const int32_t core_r = (int32_t)lroundf(g.led_r * g.u);
        lv_area_t core_area;
        core_area.x1 = cx_px - core_r;
        core_area.y1 = cy_px - core_r;
        core_area.x2 = cx_px + core_r - 1;
        core_area.y2 = cy_px + core_r - 1;

        lv_draw_rect_dsc_t core_dsc;
        lv_draw_rect_dsc_init(&core_dsc);
        core_dsc.bg_color = lv_color_hex(g.led_fill);
        core_dsc.bg_opa = LV_OPA_COVER;
        core_dsc.radius = LV_RADIUS_CIRCLE;
        lv_draw_rect(layer, &core_dsc, &core_area);
    }

    /* Sub-element 3: Ridged pad */
    lv_area_t pad_bbox;
    pad_bbox.x1 = coords.x1 + (int32_t)floorf(19.0f * g.u) - 2;
    pad_bbox.y1 = coords.y1 + (int32_t)floorf(g.pad_y * g.u) - 2;
    pad_bbox.x2 = coords.x1 + (int32_t)ceilf((19.0f + 1.5f + g.pad_w) * g.u) + 2;
    pad_bbox.y2 = coords.y1 + (int32_t)ceilf((g.pad_y + 2.0f + g.pad_h) * g.u) + 2;

    lv_area_t pad_isect;
    if (_lv_area_intersect(&pad_isect, &pad_bbox, &layer->_clip_area)) {
        int32_t pad_r = (int32_t)lroundf(2.5f * g.u);
        if (pad_r < 1) pad_r = 1;

        /* 1. Pad cast shadow */
        lv_area_t shadow_area;
        shadow_area.x1 = coords.x1 + (int32_t)lroundf((19.0f + 1.5f) * g.u);
        shadow_area.y1 = coords.y1 + (int32_t)lroundf((g.pad_y + 2.0f) * g.u);
        shadow_area.x2 = coords.x1 + (int32_t)lroundf((19.0f + 1.5f + g.pad_w) * g.u) - 1;
        shadow_area.y2 = coords.y1 + (int32_t)lroundf((g.pad_y + 2.0f + g.pad_h) * g.u) - 1;

        lv_draw_rect_dsc_t shadow_dsc;
        lv_draw_rect_dsc_init(&shadow_dsc);
        shadow_dsc.bg_color = lv_color_hex(0x000000);
        shadow_dsc.bg_opa = 89;
        shadow_dsc.radius = pad_r;
        if (shadow_area.x1 <= shadow_area.x2 && shadow_area.y1 <= shadow_area.y2) {
            lv_draw_rect(layer, &shadow_dsc, &shadow_area);
        }

        /* 2. Pad face */
        const int32_t x_pad1 = coords.x1 + (int32_t)lroundf(19.0f * g.u);
        const int32_t x_pad2 = coords.x1 + (int32_t)lroundf((19.0f + g.pad_w) * g.u) - 1;
        const int32_t y_pad1 = coords.y1 + (int32_t)lroundf(g.pad_y * g.u);
        const int32_t y_pad2 = coords.y1 + (int32_t)lroundf((g.pad_y + g.pad_h) * g.u) - 1;

        int32_t pad_split_x = coords.x1 + (int32_t)lroundf((19.0f + 0.40f * g.pad_w) * g.u);
        if (pad_split_x > x_pad2) pad_split_x = x_pad2;

        /* Left face sub-rect: #ececeb to #c4c4c1 */
        lv_area_t pad_left;
        pad_left.x1 = x_pad1;
        pad_left.x2 = pad_split_x;
        pad_left.y1 = y_pad1;
        pad_left.y2 = y_pad2;

        lv_draw_rect_dsc_t pad_left_dsc;
        lv_draw_rect_dsc_init(&pad_left_dsc);
        pad_left_dsc.bg_opa = LV_OPA_COVER;
        pad_left_dsc.bg_grad.dir = LV_GRAD_DIR_HOR;
        pad_left_dsc.bg_grad.stops_count = 2;
        pad_left_dsc.bg_grad.stops[0].color = lv_color_hex(SYNTHUI_PIANO_KEY_COLOR_PAD_A);
        pad_left_dsc.bg_grad.stops[0].opa = LV_OPA_COVER;
        pad_left_dsc.bg_grad.stops[0].frac = 0;
        pad_left_dsc.bg_grad.stops[1].color = lv_color_hex(SYNTHUI_PIANO_KEY_COLOR_PAD_B);
        pad_left_dsc.bg_grad.stops[1].opa = LV_OPA_COVER;
        pad_left_dsc.bg_grad.stops[1].frac = 255;
        pad_left_dsc.radius = 0;
        if (pad_left.x1 <= pad_left.x2 && pad_left.y1 <= pad_left.y2) {
            lv_draw_rect(layer, &pad_left_dsc, &pad_left);
        }

        /* Right face sub-rect: #c4c4c1 to #8f918f */
        lv_area_t pad_right;
        pad_right.x1 = pad_split_x;
        pad_right.x2 = x_pad2;
        pad_right.y1 = y_pad1;
        pad_right.y2 = y_pad2;

        lv_draw_rect_dsc_t pad_right_dsc;
        lv_draw_rect_dsc_init(&pad_right_dsc);
        pad_right_dsc.bg_opa = LV_OPA_COVER;
        pad_right_dsc.bg_grad.dir = LV_GRAD_DIR_HOR;
        pad_right_dsc.bg_grad.stops_count = 2;
        pad_right_dsc.bg_grad.stops[0].color = lv_color_hex(SYNTHUI_PIANO_KEY_COLOR_PAD_B);
        pad_right_dsc.bg_grad.stops[0].opa = LV_OPA_COVER;
        pad_right_dsc.bg_grad.stops[0].frac = 0;
        pad_right_dsc.bg_grad.stops[1].color = lv_color_hex(SYNTHUI_PIANO_KEY_COLOR_PAD_C);
        pad_right_dsc.bg_grad.stops[1].opa = LV_OPA_COVER;
        pad_right_dsc.bg_grad.stops[1].frac = 255;
        pad_right_dsc.radius = 0;
        if (pad_right.x1 <= pad_right.x2 && pad_right.y1 <= pad_right.y2) {
            lv_draw_rect(layer, &pad_right_dsc, &pad_right);
        }

        /* 3. Pad border stroke: radius 2.5*u, width max(1, 1.4*u), color #3a3c3c */
        lv_area_t pad_area;
        pad_area.x1 = x_pad1;
        pad_area.y1 = y_pad1;
        pad_area.x2 = x_pad2;
        pad_area.y2 = y_pad2;

        lv_draw_rect_dsc_t pad_border_dsc;
        lv_draw_rect_dsc_init(&pad_border_dsc);
        pad_border_dsc.bg_opa = LV_OPA_TRANSP;
        pad_border_dsc.border_color = lv_color_hex(SYNTHUI_PIANO_KEY_COLOR_PAD_EDGE);
        int32_t pad_bw = (int32_t)lroundf(1.4f * g.u);
        pad_border_dsc.border_width = pad_bw < 1 ? 1 : pad_bw;
        pad_border_dsc.border_opa = LV_OPA_COVER;
        pad_border_dsc.radius = pad_r;
        if (pad_area.x1 <= pad_area.x2 && pad_area.y1 <= pad_area.y2) {
            lv_draw_rect(layer, &pad_border_dsc, &pad_area);
        }

        /* 4. 4 Ridges */
        int32_t ridge_w_px = (int32_t)lroundf(g.ridge_w * g.u);
        if (ridge_w_px < 1) ridge_w_px = 1;

        lv_draw_line_dsc_t line_dsc;
        lv_draw_line_dsc_init(&line_dsc);
        line_dsc.color = lv_color_hex(SYNTHUI_PIANO_KEY_COLOR_PAD_RIDGE);
        line_dsc.opa = LV_OPA_COVER;
        line_dsc.width = ridge_w_px;
        line_dsc.round_start = 1;
        line_dsc.round_end = 1;

        for (int i = 0; i < 4; i++) {
            const int32_t ry_px = coords.y1 + (int32_t)lroundf((g.pad_y + g.ridge_y[i]) * g.u);
            line_dsc.p1.x = (lv_value_precise_t)lroundf((float)coords.x1 + g.ridge_x1 * g.u);
            line_dsc.p1.y = (lv_value_precise_t)ry_px;
            line_dsc.p2.x = (lv_value_precise_t)lroundf((float)coords.x1 + g.ridge_x2 * g.u);
            line_dsc.p2.y = (lv_value_precise_t)ry_px;
            lv_draw_line(layer, &line_dsc);
        }
    }
}

void synthui_piano_key_set_type(lv_obj_t *obj, synthui_piano_key_type_t type)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_piano_key_t *key = (synthui_piano_key_t *)obj;
    if (key->type == type) return;
    key->type = type;
    lv_obj_invalidate(obj);
}

synthui_piano_key_type_t synthui_piano_key_get_type(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_piano_key_t *key = (const synthui_piano_key_t *)obj;
    return key->type;
}

void synthui_piano_key_set_lit(lv_obj_t *obj, bool lit)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_piano_key_t *key = (synthui_piano_key_t *)obj;
    if (key->lit == lit) return;
    key->lit = lit;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    const float w = (float)lv_area_get_width(&coords);
    const float h = (float)lv_area_get_height(&coords);

    synthui::piano_key::KeyGeom g;
    if (synthui::piano_key::compute_geom(w, h, key->type, key->lit, key->pressed,
                                          key->zone_top, key->pad_height, g)) {
        int32_t x1, y1, x2, y2;
        synthui::piano_key::compute_led_dirty_area(g, coords.x1, coords.y1, x1, y1, x2, y2);
        lv_area_t dirty = { x1, y1, x2, y2 };
        lv_obj_invalidate_area(obj, &dirty);
    } else {
        lv_obj_invalidate(obj);
    }
}

bool synthui_piano_key_get_lit(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_piano_key_t *key = (const synthui_piano_key_t *)obj;
    return key->lit;
}

void synthui_piano_key_set_pressed(lv_obj_t *obj, bool pressed)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_piano_key_t *key = (synthui_piano_key_t *)obj;
    if (key->pressed == pressed) return;
    key->pressed = pressed;
    lv_obj_invalidate(obj);
}

bool synthui_piano_key_get_pressed(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_piano_key_t *key = (const synthui_piano_key_t *)obj;
    return key->pressed;
}

void synthui_piano_key_set_zone_top(lv_obj_t *obj, float zone_top)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (std::isnan(zone_top)) return;
    synthui_piano_key_t *key = (synthui_piano_key_t *)obj;
    if (key->zone_top == zone_top) return;
    key->zone_top = zone_top;
    lv_obj_invalidate(obj);
}

float synthui_piano_key_get_zone_top(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_piano_key_t *key = (const synthui_piano_key_t *)obj;
    return key->zone_top;
}

void synthui_piano_key_set_pad_height(lv_obj_t *obj, float pad_height)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (std::isnan(pad_height)) return;
    synthui_piano_key_t *key = (synthui_piano_key_t *)obj;
    if (key->pad_height == pad_height) return;
    key->pad_height = pad_height;
    lv_obj_invalidate(obj);
}

float synthui_piano_key_get_pad_height(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_piano_key_t *key = (const synthui_piano_key_t *)obj;
    return key->pad_height;
}
