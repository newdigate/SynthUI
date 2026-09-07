/* synthui_lamp.cpp - SynthUI Lamp, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#include "synthui_lamp.h"
#include "synthui_lamp_math.h"
#include <lvgl_private.h>
#include <math.h>

#define MY_CLASS (&synthui_lamp_class)

typedef struct {
    lv_obj_t obj;
    uint32_t color;
    synthui_lamp_shape_t shape;
    bool on;
} synthui_lamp_t;

static void lamp_constructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void lamp_destructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void lamp_event(const lv_obj_class_t *cls, lv_event_t *e);
static void lamp_draw(synthui_lamp_t *lamp, lv_layer_t *layer);

const lv_obj_class_t synthui_lamp_class = {
    .base_class     = &lv_obj_class,
    .constructor_cb = lamp_constructor,
    .destructor_cb  = lamp_destructor,
    .event_cb       = lamp_event,
    .name           = "synthui_lamp",
    .width_def      = 48,
    .height_def     = 48,
    .instance_size  = sizeof(synthui_lamp_t),
};

lv_obj_t *synthui_lamp_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&synthui_lamp_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

static void lamp_constructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    synthui_lamp_t *lamp = (synthui_lamp_t *)obj;
    lamp->color = SYNTHUI_LAMP_COLOR_DEFAULT;
    lamp->shape = SYNTHUI_LAMP_SHAPE_ROUND;
    lamp->on = true;
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void lamp_destructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    LV_UNUSED(obj);
}

static void lamp_event(const lv_obj_class_t *cls, lv_event_t *e)
{
    LV_UNUSED(cls);
    if (lv_obj_event_base(MY_CLASS, e) != LV_RESULT_OK) return;
    if (lv_event_get_code(e) == LV_EVENT_DRAW_MAIN) {
        lamp_draw((synthui_lamp_t *)lv_event_get_current_target_obj(e),
                  lv_event_get_layer(e));
    }
}

static void lamp_draw(synthui_lamp_t *lamp, lv_layer_t *layer)
{
    lv_area_t a;
    lv_obj_get_coords((lv_obj_t *)lamp, &a);
    const int32_t w = lv_area_get_width(&a);
    const int32_t h = lv_area_get_height(&a);
    if (w <= 0 || h <= 0) return;

    synthui_lamp_geom_t g;
    if (!synthui_lamp_compute_geom((float)w, (float)h, lamp->shape, &g)) return;

    const lv_state_t st = lv_obj_get_state((const lv_obj_t *)lamp);
    const bool disabled = (st & LV_STATE_DISABLED) != 0;

    /* 1. Bezel: #303048, radius round(1.5 * u), min 1 */
    int32_t bezel_r = (int32_t)roundf(1.5f * g.u);
    if (bezel_r < 1) bezel_r = 1;
    lv_draw_rect_dsc_t bezel_dsc;
    lv_draw_rect_dsc_init(&bezel_dsc);
    bezel_dsc.bg_color = lv_color_hex(0x303048);
    bezel_dsc.bg_opa = LV_OPA_COVER;
    bezel_dsc.radius = bezel_r;
    lv_draw_rect(layer, &bezel_dsc, &a);

    /* 2. Well: #181830, inset by round(1.6 * u), radius 0 */
    const int32_t well_inset = (int32_t)roundf(1.6f * g.u);
    lv_area_t well_area;
    well_area.x1 = a.x1 + well_inset;
    well_area.y1 = a.y1 + well_inset;
    well_area.x2 = a.x2 - well_inset;
    well_area.y2 = a.y2 - well_inset;
    if (well_area.x1 > well_area.x2 || well_area.y1 > well_area.y2) return;

    lv_draw_rect_dsc_t well_dsc;
    lv_draw_rect_dsc_init(&well_dsc);
    well_dsc.bg_color = lv_color_hex(0x181830);
    well_dsc.bg_opa = LV_OPA_COVER;
    well_dsc.radius = 0;
    lv_draw_rect(layer, &well_dsc, &well_area);

    const lv_color_t color = lv_color_hex(lamp->color);

    /* 3. Glow (if on and not disabled): radius/inset per shape, color, opacity 76 (30%) */
    if (lamp->on && !disabled) {
        lv_draw_rect_dsc_t glow_dsc;
        lv_draw_rect_dsc_init(&glow_dsc);
        glow_dsc.bg_color = color;
        glow_dsc.bg_opa = 76;

        if (lamp->shape == SYNTHUI_LAMP_SHAPE_ROUND) {
            const int32_t gr = (int32_t)roundf(g.glow_r * g.u);
            const int32_t cx_px = a.x1 + (int32_t)roundf(g.cx * g.u);
            const int32_t cy_px = a.y1 + (int32_t)roundf(g.cy * g.u);
            lv_area_t glow_area;
            glow_area.x1 = cx_px - gr;
            glow_area.y1 = cy_px - gr;
            glow_area.x2 = cx_px + gr - 1;
            glow_area.y2 = cy_px + gr - 1;
            glow_dsc.radius = LV_RADIUS_CIRCLE;
            lv_draw_rect(layer, &glow_dsc, &glow_area);
        } else {
            const int32_t bx_px = a.x1 + (int32_t)roundf(g.bx * g.u);
            const int32_t by_px = a.y1 + (int32_t)roundf(g.by * g.u);
            const int32_t bw_px = (int32_t)roundf(g.bw * g.u);
            const int32_t bh_px = (int32_t)roundf(g.bh * g.u);
            const int32_t glow_expand = (int32_t)roundf((g.glow_w * 0.5f) * g.u);
            const int32_t glow_br = (int32_t)roundf((g.br + g.glow_w * 0.5f) * g.u);

            lv_area_t glow_area;
            glow_area.x1 = bx_px - glow_expand;
            glow_area.y1 = by_px - glow_expand;
            glow_area.x2 = bx_px + bw_px - 1 + glow_expand;
            glow_area.y2 = by_px + bh_px - 1 + glow_expand;
            glow_dsc.radius = glow_br;
            lv_draw_rect(layer, &glow_dsc, &glow_area);
        }
    }

    /* 4. Core: radius per shape, color, opacity 255 (if on) or 76 (if off) or 38 (if disabled) */
    lv_opa_t core_opa;
    if (disabled) {
        core_opa = 38;
    } else if (lamp->on) {
        core_opa = 255;
    } else {
        core_opa = 76;
    }

    lv_draw_rect_dsc_t core_dsc;
    lv_draw_rect_dsc_init(&core_dsc);
    core_dsc.bg_color = color;
    core_dsc.bg_opa = core_opa;

    if (lamp->shape == SYNTHUI_LAMP_SHAPE_ROUND) {
        const int32_t cr = (int32_t)roundf(g.r * g.u);
        const int32_t cx_px = a.x1 + (int32_t)roundf(g.cx * g.u);
        const int32_t cy_px = a.y1 + (int32_t)roundf(g.cy * g.u);
        lv_area_t core_area;
        core_area.x1 = cx_px - cr;
        core_area.y1 = cy_px - cr;
        core_area.x2 = cx_px + cr - 1;
        core_area.y2 = cy_px + cr - 1;
        core_dsc.radius = LV_RADIUS_CIRCLE;
        lv_draw_rect(layer, &core_dsc, &core_area);
    } else {
        const int32_t bx_px = a.x1 + (int32_t)roundf(g.bx * g.u);
        const int32_t by_px = a.y1 + (int32_t)roundf(g.by * g.u);
        const int32_t bw_px = (int32_t)roundf(g.bw * g.u);
        const int32_t bh_px = (int32_t)roundf(g.bh * g.u);
        lv_area_t core_area;
        core_area.x1 = bx_px;
        core_area.y1 = by_px;
        core_area.x2 = bx_px + bw_px - 1;
        core_area.y2 = by_px + bh_px - 1;
        core_dsc.radius = (int32_t)roundf(g.br * g.u);
        lv_draw_rect(layer, &core_dsc, &core_area);
    }

    /* 5. Top highlight line: #7890D8, opacity 89 (35%) or 51 (20% if disabled) */
    int32_t top_h = (int32_t)roundf(g.top_line * g.u);
    if (top_h < 1) top_h = 1;
    int32_t top_y2 = well_area.y1 + top_h - 1;
    if (top_y2 > well_area.y2) top_y2 = well_area.y2;

    lv_area_t top_area;
    top_area.x1 = well_area.x1;
    top_area.y1 = well_area.y1;
    top_area.x2 = well_area.x2;
    top_area.y2 = top_y2;

    lv_draw_rect_dsc_t top_dsc;
    lv_draw_rect_dsc_init(&top_dsc);
    top_dsc.bg_color = lv_color_hex(0x7890D8);
    top_dsc.bg_opa = disabled ? 51 : 89;
    top_dsc.radius = 0;
    lv_draw_rect(layer, &top_dsc, &top_area);
}

void synthui_lamp_set_on(lv_obj_t *obj, bool on)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_lamp_t *lamp = (synthui_lamp_t *)obj;
    if (lamp->on == on) return;
    lamp->on = on;
    lv_obj_invalidate(obj);
}

bool synthui_lamp_get_on(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_lamp_t *lamp = (const synthui_lamp_t *)obj;
    return lamp->on;
}

void synthui_lamp_set_shape(lv_obj_t *obj, synthui_lamp_shape_t shape)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_lamp_t *lamp = (synthui_lamp_t *)obj;
    if (lamp->shape == shape) return;
    lamp->shape = shape;
    lv_obj_invalidate(obj);
}

synthui_lamp_shape_t synthui_lamp_get_shape(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_lamp_t *lamp = (const synthui_lamp_t *)obj;
    return lamp->shape;
}

void synthui_lamp_set_color(lv_obj_t *obj, uint32_t rgb_hex)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_lamp_t *lamp = (synthui_lamp_t *)obj;
    if (lamp->color == rgb_hex) return;
    lamp->color = rgb_hex;
    lv_obj_invalidate(obj);
}

uint32_t synthui_lamp_get_color(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_lamp_t *lamp = (const synthui_lamp_t *)obj;
    return lamp->color;
}
