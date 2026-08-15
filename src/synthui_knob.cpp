/* synthui_knob.cpp - SynthUI rotary knob, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT
 *
 * Clean-room port of the DC Knob's renderVals() MATH (SynthUI
 * reference/dc/Knob.dc.html): geometry in a 0..100 viewBox scaled to
 * min(w,h).  v1 shading is deliberately flat (spec 2026-08-15-synthui-knob
 * section 5): one native vertical gradient on the face; the crescent's SVG
 * gradient is replaced by angle-driven luminance (section 5.3). */
#include "synthui_knob.h"
/* Out-of-tree widget: the class struct literal and the by-value lv_obj_t in
 * synthui_knob_t both need the COMPLETE private types.  LVGL 9's sanctioned
 * umbrella for that is lvgl_private.h (LV_USE_PRIVATE_API stays 0). */
#include <lvgl_private.h>
#include <math.h>

#define KNOB_DEG (3.14159265358979f / 180.0f)

typedef struct {
    lv_obj_t obj;
    float angle, sweep, min_deg, max_deg, detent_step;
    uint8_t tick_count;
    synthui_knob_mode_t mode;
} synthui_knob_t;

static void knob_constructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void knob_event(const lv_obj_class_t *cls, lv_event_t *e);

const lv_obj_class_t synthui_knob_class = {
    .base_class     = &lv_obj_class,
    .constructor_cb = knob_constructor,
    .event_cb       = knob_event,
    .width_def      = 120,
    .height_def     = 120,
    .instance_size  = sizeof(synthui_knob_t),
};

lv_obj_t *synthui_knob_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&synthui_knob_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

static void knob_constructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    synthui_knob_t *k = (synthui_knob_t *)obj;
    k->angle = 0.0f; k->sweep = 215.0f;
    k->min_deg = -140.0f; k->max_deg = 140.0f;
    k->detent_step = 35.0f; k->tick_count = 8;
    k->mode = SYNTHUI_KNOB_MODE_ENDLESS;
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

#define KNOB_SETTER(field, expr) do { \
    synthui_knob_t *k = (synthui_knob_t *)obj; \
    k->field = (expr); \
    lv_obj_invalidate(obj); } while (0)

void synthui_knob_set_angle(lv_obj_t *obj, float deg) { KNOB_SETTER(angle, deg); }
void synthui_knob_set_mode(lv_obj_t *obj, synthui_knob_mode_t m) { KNOB_SETTER(mode, m); }
void synthui_knob_set_sweep(lv_obj_t *obj, float deg)
{   /* renderVals(): Math.max(30, Math.min(340, sweep)) */
    KNOB_SETTER(sweep, deg < 30.0f ? 30.0f : (deg > 340.0f ? 340.0f : deg));
}
void synthui_knob_set_tick_count(lv_obj_t *obj, uint8_t n) { KNOB_SETTER(tick_count, n > 24 ? 24 : n); }
void synthui_knob_set_detent_step(lv_obj_t *obj, float deg) { KNOB_SETTER(detent_step, deg); }
void synthui_knob_set_range(lv_obj_t *obj, float min_deg, float max_deg)
{
    synthui_knob_t *k = (synthui_knob_t *)obj;
    k->min_deg = min_deg; k->max_deg = max_deg;
    lv_obj_invalidate(obj);
}
float synthui_knob_get_angle(const lv_obj_t *obj) { return ((const synthui_knob_t *)obj)->angle; }

static void knob_draw(synthui_knob_t *k, lv_layer_t *layer); /* Task 5 */

static void knob_event(const lv_obj_class_t *cls, lv_event_t *e)
{
    LV_UNUSED(cls);
    if (lv_obj_event_base(&synthui_knob_class, e) != LV_RESULT_OK) return;
    if (lv_event_get_code(e) == LV_EVENT_DRAW_MAIN)
        knob_draw((synthui_knob_t *)lv_event_get_current_target_obj(e),
                  lv_event_get_layer(e));
}

/* Task 2 stub -- replaced by the full port in Task 5. */
static void knob_draw(synthui_knob_t *k, lv_layer_t *layer)
{
    LV_UNUSED(k); LV_UNUSED(layer);
}
