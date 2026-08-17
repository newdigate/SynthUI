/* synthui_step.h - one step cell of a sequencer lane, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT
 *
 * The library's second widget. Five INDEPENDENT visual states -- gate (fill),
 * accent (dot, top-right), slide (bar, bottom edge), cursor (inset ring) and
 * selected (outline) -- because a cell is routinely several at once: the
 * playhead sits on a gated, accented step while the user has a different one
 * selected for editing.
 *
 * Input is stock LVGL clicking; the widget adds no input code of its own and
 * emits LV_EVENT_CLICKED. Which cell is "selected" is the APPLICATION's
 * business (one cell per lane), so the widget only draws what it is told. */
#ifndef SYNTHUI_STEP_H
#define SYNTHUI_STEP_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_obj_class_t synthui_step_class;

lv_obj_t *synthui_step_create(lv_obj_t *parent);

/* The three PATTERN bits, set together: they come from one sequencer step and
 * the app writes them back as one transaction, so a combined setter is the
 * shape that matches the caller and repaints once instead of three times. */
void synthui_step_set(lv_obj_t *obj, bool gate, bool accent, bool slide);

/* The two VIEW bits, independent of the pattern and of each other. */
void synthui_step_set_cursor(lv_obj_t *obj, bool on);
void synthui_step_set_selected(lv_obj_t *obj, bool on);

bool synthui_step_gate(const lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif /* SYNTHUI_STEP_H */
