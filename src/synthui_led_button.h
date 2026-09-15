/* synthui_led_button.h - SynthUI LedButton, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT
 *
 * The DC "909-led-button" step key: an ivory cap sunk in a black bezel with a
 * lozenge LED across the top third.  Four boolean states and a colour; state
 * change is colour and one 2.5-unit offset, nothing resizes.
 *
 * The widget owns NO toggle logic: it emits stock LV_EVENT_CLICKED and the
 * application sets `lit` (synthui_step / synthui_panel_button's model).
 * `pressed` is a LATCH -- the drawn pressed state is (latch || LV_STATE_PRESSED),
 * so a finger sinks any key while held and the latch keeps a selected key sunk
 * afterwards (the acid_box edit cursor).  A programmatic
 * lv_obj_add_state/remove_state(LV_STATE_PRESSED) after the first render is
 * the caller's to invalidate; LVGL does not repaint a style-less widget on a
 * state change.  A scroll that starts on a held key clears LV_STATE_PRESSED
 * with no event to the key; the scroll's own invalidation normally repaints
 * it. */
#ifndef SYNTHUI_LED_BUTTON_H
#define SYNTHUI_LED_BUTTON_H

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>
#include "synthui_led_button_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_obj_class_t synthui_led_button_class;

lv_obj_t *synthui_led_button_create(lv_obj_t *parent);

void synthui_led_button_set_lit(lv_obj_t *obj, bool lit);
bool synthui_led_button_get_lit(const lv_obj_t *obj);

void synthui_led_button_set_pressed(lv_obj_t *obj, bool pressed);   /* the latch */
bool synthui_led_button_get_pressed(const lv_obj_t *obj);

void synthui_led_button_set_cue(lv_obj_t *obj, bool cue);
bool synthui_led_button_get_cue(const lv_obj_t *obj);

/* Greys the cap and LED, suppresses the halo, sets LV_STATE_DISABLED
 * (clearing any press) and clears LV_OBJ_FLAG_CLICKABLE; restored on
 * re-enable. A key disabled by lv_obj_add_state(LV_STATE_DISABLED) also
 * draws grey; the caller then owns its invalidation.  Disable on
 * LV_EVENT_PRESSED, not RELEASED, to suppress the click: LVGL decides
 * whether to send CLICKED before it sends RELEASED. */
void synthui_led_button_set_disabled(lv_obj_t *obj, bool disabled);
bool synthui_led_button_get_disabled(const lv_obj_t *obj);

void synthui_led_button_set_color(lv_obj_t *obj, synthui_led_button_color_t color);
synthui_led_button_color_t synthui_led_button_get_color(const lv_obj_t *obj);

#ifdef __cplusplus
}
#endif
#endif /* SYNTHUI_LED_BUTTON_H */
