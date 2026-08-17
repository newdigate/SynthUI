/* synthui_step.cpp - SynthUI sequencer step cell, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT
 *
 * Palette is the acid-box set, chosen against the #101820 ground the display
 * examples use. Geometry derives from the cell's WIDTH so one widget serves
 * the 74 px test grid and the 82 px app lane without per-size tuning. */
#include "synthui_step.h"
/* Out-of-tree widget: the class struct literal and the by-value lv_obj_t in
 * synthui_step_t both need the COMPLETE private types -- same reason as
 * synthui_knob.cpp. LVGL 9's sanctioned umbrella is lvgl_private.h. */
#include <lvgl_private.h>

#define MY_CLASS (&synthui_step_class)

typedef struct {
    lv_obj_t obj;
    bool gate, accent, slide, cursor, selected;
} synthui_step_t;

#define STEP_BG_OFF   lv_color_hex(0x1a2230)
#define STEP_BG_ON    lv_color_hex(0x39406e)
#define STEP_ACCENT   lv_color_hex(0xc2543f)
#define STEP_SLIDE    lv_color_hex(0x8f96d4)
#define STEP_CURSOR   lv_color_hex(0x3fa060)
#define STEP_SELECT   lv_color_hex(0xfdfdf9)

static void step_constructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void step_event(const lv_obj_class_t *cls, lv_event_t *e);

const lv_obj_class_t synthui_step_class = {
    .base_class     = &lv_obj_class,
    .constructor_cb = step_constructor,
    .event_cb       = step_event,
    /* Designators must be in declaration order for C++, and name declares
     * before width_def (lv_obj_class_private.h) -- see synthui_knob.cpp. */
    .name           = "synthui_step",
    /* A default size, not a required one: the app always sets its own, but a
     * zero-size widget draws nothing and looks like a broken build. */
    .width_def      = 74,
    .height_def     = 74,
    .instance_size  = sizeof(synthui_step_t),
};

lv_obj_t *synthui_step_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&synthui_step_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

static void step_constructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    synthui_step_t *s = (synthui_step_t *)obj;
    s->gate = s->accent = s->slide = s->cursor = s->selected = false;
    /* CLICKABLE is the base class default and this widget wants it -- taps
     * are its whole input story. Scrolling is not: a lane of cells sits
     * inside a plain screen, and a cell that could scroll would swallow
     * taps as drags. */
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void step_draw(synthui_step_t *s, lv_layer_t *layer)
{
    lv_area_t a;
    lv_obj_get_coords((lv_obj_t *)s, &a);
    const int32_t w = lv_area_get_width(&a);

    lv_draw_rect_dsc_t body;
    lv_draw_rect_dsc_init(&body);
    body.radius = w / 8;
    body.bg_color = s->gate ? STEP_BG_ON : STEP_BG_OFF;
    body.bg_opa = LV_OPA_COVER;
    if (s->selected) {
        body.border_color = STEP_SELECT;
        body.border_width = 2;
        body.border_opa = LV_OPA_COVER;
    }
    lv_draw_rect(layer, &body, &a);

    /* Cursor LAST-but-one in z, and inset: an outline drawn on the same edge
     * as the selected border would be hidden by it exactly when both are on
     * (the playhead crossing the edited step -- the one moment both matter). */
    if (s->cursor) {
        lv_draw_rect_dsc_t ring;
        lv_draw_rect_dsc_init(&ring);
        ring.radius = w / 8;
        ring.bg_opa = LV_OPA_TRANSP;
        ring.border_color = STEP_CURSOR;
        ring.border_width = 3;
        ring.border_opa = LV_OPA_COVER;
        lv_area_t ra = a;
        lv_area_increase(&ra, -3, -3);
        lv_draw_rect(layer, &ring, &ra);
    }

    /* Accent: square dot in the top-right, side w/4 - 2 so it scales with the
     * cell and never collides with the corner radius. */
    if (s->accent) {
        lv_draw_rect_dsc_t dot;
        lv_draw_rect_dsc_init(&dot);
        dot.radius = LV_RADIUS_CIRCLE;
        dot.bg_color = STEP_ACCENT;
        dot.bg_opa = LV_OPA_COVER;
        const lv_area_t da = { a.x2 - w / 4 - 2, a.y1 + 4,
                               a.x2 - 4,         a.y1 + w / 4 + 2 };
        lv_draw_rect(layer, &dot, &da);
    }

    /* Slide: bar along the bottom edge, reading as a tie into the next step. */
    if (s->slide) {
        lv_draw_rect_dsc_t bar;
        lv_draw_rect_dsc_init(&bar);
        bar.radius = 2;
        bar.bg_color = STEP_SLIDE;
        bar.bg_opa = LV_OPA_COVER;
        const lv_area_t ba = { a.x1 + 5, a.y2 - 8, a.x2 - 5, a.y2 - 4 };
        lv_draw_rect(layer, &bar, &ba);
    }
}

static void step_event(const lv_obj_class_t *cls, lv_event_t *e)
{
    LV_UNUSED(cls);
    if (lv_obj_event_base(MY_CLASS, e) != LV_RESULT_OK) return;
    /* Clicks need no handler here: LVGL emits LV_EVENT_CLICKED to whatever the
     * app registered, and this widget deliberately owns no selection state. */
    if (lv_event_get_code(e) == LV_EVENT_DRAW_MAIN)
        step_draw((synthui_step_t *)lv_event_get_current_target_obj(e),
                  lv_event_get_layer(e));
}

/* Every setter early-returns when nothing changed. The app's 33 ms poller
 * writes the cursor on every tick and the pattern on every edit, so without
 * this the lane would invalidate 16 cells 30 times a second for no pixels. */
void synthui_step_set(lv_obj_t *obj, bool gate, bool accent, bool slide)
{
    synthui_step_t *s = (synthui_step_t *)obj;
    if (s->gate == gate && s->accent == accent && s->slide == slide) return;
    s->gate = gate;
    s->accent = accent;
    s->slide = slide;
    lv_obj_invalidate(obj);
}

void synthui_step_set_cursor(lv_obj_t *obj, bool on)
{
    synthui_step_t *s = (synthui_step_t *)obj;
    if (s->cursor == on) return;
    s->cursor = on;
    lv_obj_invalidate(obj);
}

void synthui_step_set_selected(lv_obj_t *obj, bool on)
{
    synthui_step_t *s = (synthui_step_t *)obj;
    if (s->selected == on) return;
    s->selected = on;
    lv_obj_invalidate(obj);
}

bool synthui_step_gate(const lv_obj_t *obj)
{
    return ((const synthui_step_t *)obj)->gate;
}
