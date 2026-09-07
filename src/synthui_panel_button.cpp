/* synthui_panel_button.cpp - SynthUI PanelButton, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#include "synthui_panel_button.h"
#include "synthui_panel_button_math.h"
#include <lvgl_private.h>
#include <math.h>

#define MY_CLASS (&synthui_panel_button_class)

typedef struct {
    lv_obj_t obj;
    uint32_t accent;
    synthui_panel_button_glyph_t glyph;
    float glyph_scale;
    bool on;
} synthui_panel_button_t;

static void btn_constructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void btn_destructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void btn_event(const lv_obj_class_t *cls, lv_event_t *e);
static void btn_draw(synthui_panel_button_t *btn, lv_layer_t *layer);

const lv_obj_class_t synthui_panel_button_class = {
    .base_class     = &lv_obj_class,
    .constructor_cb = btn_constructor,
    .destructor_cb  = btn_destructor,
    .event_cb       = btn_event,
    .name           = "synthui_panel_button",
    .width_def      = 74,
    .height_def     = 58,
    .instance_size  = sizeof(synthui_panel_button_t),
};

lv_obj_t *synthui_panel_button_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&synthui_panel_button_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

static void btn_constructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    synthui_panel_button_t *btn = (synthui_panel_button_t *)obj;
    btn->accent = SYNTHUI_PANEL_BUTTON_ACCENT_DEFAULT;
    btn->glyph = SYNTHUI_PANEL_BUTTON_GLYPH_PLAY;
    btn->glyph_scale = 0.62f;
    btn->on = false;
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void btn_destructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    LV_UNUSED(obj);
}

static void btn_event(const lv_obj_class_t *cls, lv_event_t *e)
{
    LV_UNUSED(cls);
    if (lv_obj_event_base(MY_CLASS, e) != LV_RESULT_OK) return;
    if (lv_event_get_code(e) == LV_EVENT_DRAW_MAIN) {
        btn_draw((synthui_panel_button_t *)lv_event_get_current_target_obj(e),
                 lv_event_get_layer(e));
    }
}

static void btn_draw(synthui_panel_button_t *btn, lv_layer_t *layer)
{
    lv_area_t a;
    lv_obj_get_coords((lv_obj_t *)btn, &a);
    const int32_t w = lv_area_get_width(&a);
    const int32_t h = lv_area_get_height(&a);
    if (w <= 0 || h <= 0) return;

    synthui_panel_button_geom_t g;
    if (!synthui_panel_button_compute_geom((float)w, (float)h, btn->glyph_scale, &g)) return;

    const lv_state_t st = lv_obj_get_state((const lv_obj_t *)btn);
    const bool disabled = (st & LV_STATE_DISABLED) != 0;

    /* 1. Bezel: #303048, radius round(2.0 * u), min 1 */
    int32_t bezel_r = (int32_t)lroundf(2.0f * g.u);
    if (bezel_r < 1) bezel_r = 1;
    lv_draw_rect_dsc_t bezel_dsc;
    lv_draw_rect_dsc_init(&bezel_dsc);
    bezel_dsc.bg_color = lv_color_hex(0x303048);
    bezel_dsc.bg_opa = LV_OPA_COVER;
    bezel_dsc.radius = bezel_r;
    lv_draw_rect(layer, &bezel_dsc, &a);

    /* 2. Body Face: inset by round(2.2 * u), vertical 2-stop gradient */
    const int32_t inset_px = (int32_t)lroundf(g.i * g.u);
    lv_area_t body_area;
    body_area.x1 = a.x1 + inset_px;
    body_area.y1 = a.y1 + inset_px;
    body_area.x2 = a.x2 - inset_px;
    body_area.y2 = a.y2 - inset_px;
    if (body_area.x1 > body_area.x2 || body_area.y1 > body_area.y2) return;

    const uint32_t top_c = btn->on ? 0xA0B4F4u : 0x90A8F0u;
    const uint32_t bot_c = 0x486090u;
    lv_draw_rect_dsc_t body_dsc;
    lv_draw_rect_dsc_init(&body_dsc);
    body_dsc.bg_opa = disabled ? LV_OPA_50 : LV_OPA_COVER;
    body_dsc.bg_grad.dir = LV_GRAD_DIR_VER;
    body_dsc.bg_grad.stops_count = 2;
    body_dsc.bg_grad.stops[0].color = lv_color_hex(top_c);
    body_dsc.bg_grad.stops[0].opa = LV_OPA_COVER;
    body_dsc.bg_grad.stops[0].frac = 0;
    body_dsc.bg_grad.stops[1].color = lv_color_hex(bot_c);
    body_dsc.bg_grad.stops[1].opa = LV_OPA_COVER;
    body_dsc.bg_grad.stops[1].frac = 255;
    body_dsc.radius = 0;
    lv_draw_rect(layer, &body_dsc, &body_area);

    /* 3. Sheen Band: #D8D8F0, opacity 191 (75%) or 102 (disabled) */
    const int32_t sheen_h_px = (int32_t)lroundf(g.sheen_h * g.u);
    if (sheen_h_px > 0) {
        lv_area_t sheen_area = body_area;
        sheen_area.y2 = body_area.y1 + sheen_h_px - 1;
        if (sheen_area.y2 > body_area.y2) sheen_area.y2 = body_area.y2;
        lv_draw_rect_dsc_t sheen_dsc;
        lv_draw_rect_dsc_init(&sheen_dsc);
        sheen_dsc.bg_color = lv_color_hex(0xD8D8F0);
        sheen_dsc.bg_opa = disabled ? 102 : 191;
        sheen_dsc.radius = 0;
        lv_draw_rect(layer, &sheen_dsc, &sheen_area);
    }

    /* 4. Sheen Line: #F0F0F0, opacity 230 (90%) or 128 (disabled) */
    int32_t sheen_line_h = (int32_t)lroundf(g.sheen_line * g.u);
    if (sheen_line_h < 1) sheen_line_h = 1;
    const int32_t sheen_y_px = a.y1 + (int32_t)lroundf(g.sheen_y * g.u);
    lv_area_t line_area = body_area;
    line_area.y1 = sheen_y_px;
    line_area.y2 = sheen_y_px + sheen_line_h - 1;
    if (line_area.y2 <= body_area.y2 && line_area.y1 >= body_area.y1) {
        lv_draw_rect_dsc_t line_dsc;
        lv_draw_rect_dsc_init(&line_dsc);
        line_dsc.bg_color = lv_color_hex(0xF0F0F0);
        line_dsc.bg_opa = disabled ? 128 : 230;
        line_dsc.radius = 0;
        lv_draw_rect(layer, &line_dsc, &line_area);
    }

    /* 5. Shadow Band: #303048, opacity 128 (50%) or 76 (disabled) */
    const int32_t shadow_h_px = (int32_t)lroundf(g.shadow_h * g.u);
    if (shadow_h_px > 0) {
        const int32_t shadow_y_px = a.y1 + (int32_t)lroundf(g.shadow_y * g.u);
        lv_area_t shadow_area = body_area;
        shadow_area.y1 = shadow_y_px;
        if (shadow_area.y1 <= body_area.y2) {
            lv_draw_rect_dsc_t shadow_dsc;
            lv_draw_rect_dsc_init(&shadow_dsc);
            shadow_dsc.bg_color = lv_color_hex(0x303048);
            shadow_dsc.bg_opa = disabled ? 76 : 128;
            shadow_dsc.radius = 0;
            lv_draw_rect(layer, &shadow_dsc, &shadow_area);
        }
    }

    /* 6. Wash Overlay (if on and not disabled): accent, opacity 31 (12%) */
    const lv_color_t accent_color = lv_color_hex(btn->accent);
    if (btn->on && !disabled) {
        lv_draw_rect_dsc_t wash_dsc;
        lv_draw_rect_dsc_init(&wash_dsc);
        wash_dsc.bg_color = accent_color;
        wash_dsc.bg_opa = 31;
        wash_dsc.radius = 0;
        lv_draw_rect(layer, &wash_dsc, &body_area);
    }

    /* Get glyph geometry */
    synthui_panel_button_glyph_geom_t gg;
    synthui_panel_button_get_glyph_geom(&g, btn->glyph, &gg);

    const int32_t glow_w_px = (int32_t)lroundf(g.glow_w * g.u);
    const int32_t half_glow_px = (int32_t)lroundf((g.glow_w * 0.5f) * g.u);

    /* 7. Glyph Glow (if on and not disabled): accent, opacity 82 (32%) */
    if (btn->on && !disabled && glow_w_px > 0) {
        /* Triangles: draw lines along edges with rounded caps */
        for (int i = 0; i < gg.num_triangles; i++) {
            for (int e_idx = 0; e_idx < 3; e_idx++) {
                int next = (e_idx + 1) % 3;
                lv_draw_line_dsc_t line_dsc;
                lv_draw_line_dsc_init(&line_dsc);
                line_dsc.color = accent_color;
                line_dsc.opa = 82;
                line_dsc.width = glow_w_px;
                line_dsc.round_start = 1;
                line_dsc.round_end = 1;
                line_dsc.p1.x = (lv_value_precise_t)lroundf(a.x1 + gg.triangles[i].p[e_idx].x * g.u);
                line_dsc.p1.y = (lv_value_precise_t)lroundf(a.y1 + gg.triangles[i].p[e_idx].y * g.u);
                line_dsc.p2.x = (lv_value_precise_t)lroundf(a.x1 + gg.triangles[i].p[next].x * g.u);
                line_dsc.p2.y = (lv_value_precise_t)lroundf(a.y1 + gg.triangles[i].p[next].y * g.u);
                lv_draw_line(layer, &line_dsc);
            }
        }

        /* Rectangles: expand by half_glow */
        for (int i = 0; i < gg.num_rects; i++) {
            lv_area_t glow_r;
            glow_r.x1 = a.x1 + (int32_t)lroundf(gg.rects[i].x * g.u) - half_glow_px;
            glow_r.y1 = a.y1 + (int32_t)lroundf(gg.rects[i].y * g.u) - half_glow_px;
            glow_r.x2 = a.x1 + (int32_t)lroundf((gg.rects[i].x + gg.rects[i].w) * g.u) - 1 + half_glow_px;
            glow_r.y2 = a.y1 + (int32_t)lroundf((gg.rects[i].y + gg.rects[i].h) * g.u) - 1 + half_glow_px;
            lv_draw_rect_dsc_t glow_rect_dsc;
            lv_draw_rect_dsc_init(&glow_rect_dsc);
            glow_rect_dsc.bg_color = accent_color;
            glow_rect_dsc.bg_opa = 82;
            glow_rect_dsc.radius = half_glow_px > 1 ? half_glow_px : 1;
            lv_draw_rect(layer, &glow_rect_dsc, &glow_r);
        }

        /* Circles: expand radius by half_glow */
        for (int i = 0; i < gg.num_circles; i++) {
            const int32_t cx_px = a.x1 + (int32_t)lroundf(gg.circles[i].cx * g.u);
            const int32_t cy_px = a.y1 + (int32_t)lroundf(gg.circles[i].cy * g.u);
            const int32_t r_px = (int32_t)lroundf(gg.circles[i].r * g.u) + half_glow_px;
            lv_area_t glow_c;
            glow_c.x1 = cx_px - r_px;
            glow_c.y1 = cy_px - r_px;
            glow_c.x2 = cx_px + r_px - 1;
            glow_c.y2 = cy_px + r_px - 1;
            lv_draw_rect_dsc_t glow_circ_dsc;
            lv_draw_rect_dsc_init(&glow_circ_dsc);
            glow_circ_dsc.bg_color = accent_color;
            glow_circ_dsc.bg_opa = 82;
            glow_circ_dsc.radius = LV_RADIUS_CIRCLE;
            lv_draw_rect(layer, &glow_circ_dsc, &glow_c);
        }
    }

    /* 8. Glyph Core: on ? accent : #303048 */
    const lv_color_t glyph_color = btn->on ? accent_color : lv_color_hex(0x303048);
    const lv_opa_t glyph_opa = disabled ? 102 : LV_OPA_COVER;

    /* Triangles */
    for (int i = 0; i < gg.num_triangles; i++) {
        lv_draw_triangle_dsc_t tri_dsc;
        lv_draw_triangle_dsc_init(&tri_dsc);
        tri_dsc.color = glyph_color;
        tri_dsc.opa = glyph_opa;
        for (int p_idx = 0; p_idx < 3; p_idx++) {
            tri_dsc.p[p_idx].x = (lv_value_precise_t)lroundf(a.x1 + gg.triangles[i].p[p_idx].x * g.u);
            tri_dsc.p[p_idx].y = (lv_value_precise_t)lroundf(a.y1 + gg.triangles[i].p[p_idx].y * g.u);
        }
        lv_draw_triangle(layer, &tri_dsc);
    }

    /* Rectangles */
    for (int i = 0; i < gg.num_rects; i++) {
        lv_area_t r_area;
        r_area.x1 = a.x1 + (int32_t)lroundf(gg.rects[i].x * g.u);
        r_area.y1 = a.y1 + (int32_t)lroundf(gg.rects[i].y * g.u);
        r_area.x2 = a.x1 + (int32_t)lroundf((gg.rects[i].x + gg.rects[i].w) * g.u) - 1;
        r_area.y2 = a.y1 + (int32_t)lroundf((gg.rects[i].y + gg.rects[i].h) * g.u) - 1;
        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);
        rect_dsc.bg_color = glyph_color;
        rect_dsc.bg_opa = glyph_opa;
        rect_dsc.radius = 0;
        lv_draw_rect(layer, &rect_dsc, &r_area);
    }

    /* Circles */
    for (int i = 0; i < gg.num_circles; i++) {
        const int32_t cx_px = a.x1 + (int32_t)lroundf(gg.circles[i].cx * g.u);
        const int32_t cy_px = a.y1 + (int32_t)lroundf(gg.circles[i].cy * g.u);
        const int32_t r_px = (int32_t)lroundf(gg.circles[i].r * g.u);
        lv_area_t circ_area;
        circ_area.x1 = cx_px - r_px;
        circ_area.y1 = cy_px - r_px;
        circ_area.x2 = cx_px + r_px - 1;
        circ_area.y2 = cy_px + r_px - 1;
        lv_draw_rect_dsc_t circ_dsc;
        lv_draw_rect_dsc_init(&circ_dsc);
        circ_dsc.bg_color = glyph_color;
        circ_dsc.bg_opa = glyph_opa;
        circ_dsc.radius = LV_RADIUS_CIRCLE;
        lv_draw_rect(layer, &circ_dsc, &circ_area);
    }
}

void synthui_panel_button_set_on(lv_obj_t *obj, bool on)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_panel_button_t *btn = (synthui_panel_button_t *)obj;
    if (btn->on == on) return;
    btn->on = on;
    lv_obj_invalidate(obj);
}

bool synthui_panel_button_get_on(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_panel_button_t *btn = (const synthui_panel_button_t *)obj;
    return btn->on;
}

void synthui_panel_button_set_glyph(lv_obj_t *obj, synthui_panel_button_glyph_t glyph)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_panel_button_t *btn = (synthui_panel_button_t *)obj;
    if (btn->glyph == glyph) return;
    btn->glyph = glyph;
    lv_obj_invalidate(obj);
}

synthui_panel_button_glyph_t synthui_panel_button_get_glyph(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_panel_button_t *btn = (const synthui_panel_button_t *)obj;
    return btn->glyph;
}

void synthui_panel_button_set_accent(lv_obj_t *obj, uint32_t rgb_hex)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_panel_button_t *btn = (synthui_panel_button_t *)obj;
    if (btn->accent == rgb_hex) return;
    btn->accent = rgb_hex;
    lv_obj_invalidate(obj);
}

uint32_t synthui_panel_button_get_accent(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_panel_button_t *btn = (const synthui_panel_button_t *)obj;
    return btn->accent;
}

void synthui_panel_button_set_glyph_scale(lv_obj_t *obj, float scale)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_panel_button_t *btn = (synthui_panel_button_t *)obj;
    if (btn->glyph_scale == scale) return;
    btn->glyph_scale = scale;
    lv_obj_invalidate(obj);
}

float synthui_panel_button_get_glyph_scale(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_panel_button_t *btn = (const synthui_panel_button_t *)obj;
    return btn->glyph_scale;
}
