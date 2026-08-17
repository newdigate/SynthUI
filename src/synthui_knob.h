/* synthui_knob.h - SynthUI rotary knob, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_KNOB_H
#define SYNTHUI_KNOB_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYNTHUI_KNOB_MODE_ENDLESS = 0,
    SYNTHUI_KNOB_MODE_BOUNDED,
    SYNTHUI_KNOB_MODE_DETENTS,
    SYNTHUI_KNOB_MODE_ARC,
} synthui_knob_mode_t;

extern const lv_obj_class_t synthui_knob_class;

lv_obj_t *synthui_knob_create(lv_obj_t *parent);

/* State changes (lv_obj_add_state()/remove_state()) only trigger LVGL's
 * automatic repaint when a style differs between the old and new state.
 * This widget defines no local styles, so that check always finds SAME and
 * the repaint never fires. After changing states programmatically, call
 * lv_obj_invalidate(obj). Emits nothing itself; the input layer (vertical
 * drag, 200 px = full sweep) emits LV_EVENT_VALUE_CHANGED. */
void  synthui_knob_set_angle(lv_obj_t *obj, float deg);        /* default 0 */
void  synthui_knob_set_mode(lv_obj_t *obj, synthui_knob_mode_t m);
void  synthui_knob_set_sweep(lv_obj_t *obj, float deg);        /* default 215, clamped 30..340 */
void  synthui_knob_set_tick_count(lv_obj_t *obj, uint8_t n);   /* default 8, capped 24 */
void  synthui_knob_set_range(lv_obj_t *obj, float min_deg, float max_deg); /* default -140..140 */
void  synthui_knob_set_detent_step(lv_obj_t *obj, float deg);  /* default 35 */
float synthui_knob_get_angle(const lv_obj_t *obj);

#ifdef __cplusplus
}
#endif
#endif
