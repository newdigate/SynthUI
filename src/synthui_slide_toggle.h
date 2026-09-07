/* synthui_slide_toggle.h - SynthUI SlideToggle, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_SLIDE_TOGGLE_H
#define SYNTHUI_SLIDE_TOGGLE_H

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>
#include "synthui_slide_toggle_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_obj_class_t synthui_slide_toggle_class;

lv_obj_t *synthui_slide_toggle_create(lv_obj_t *parent);

void synthui_slide_toggle_set_value(lv_obj_t *obj, int32_t value);
int32_t synthui_slide_toggle_get_value(const lv_obj_t *obj);

void synthui_slide_toggle_set_positions(lv_obj_t *obj, int32_t positions);
int32_t synthui_slide_toggle_get_positions(const lv_obj_t *obj);

void synthui_slide_toggle_set_left_glyph(lv_obj_t *obj, synthui_slide_toggle_glyph_t glyph);
synthui_slide_toggle_glyph_t synthui_slide_toggle_get_left_glyph(const lv_obj_t *obj);

void synthui_slide_toggle_set_right_glyph(lv_obj_t *obj, synthui_slide_toggle_glyph_t glyph);
synthui_slide_toggle_glyph_t synthui_slide_toggle_get_right_glyph(const lv_obj_t *obj);

void synthui_slide_toggle_set_panel_color(lv_obj_t *obj, uint32_t color);
uint32_t synthui_slide_toggle_get_panel_color(const lv_obj_t *obj);

void synthui_slide_toggle_set_disabled(lv_obj_t *obj, bool disabled);
bool synthui_slide_toggle_get_disabled(const lv_obj_t *obj);

#ifdef __cplusplus
} // extern "C"

namespace synthui {
class SlideToggle {
public:
    explicit SlideToggle(lv_obj_t *obj = nullptr) : obj_(obj) {}
    static SlideToggle create(lv_obj_t *parent) {
        return SlideToggle(synthui_slide_toggle_create(parent));
    }
    lv_obj_t *raw() const { return obj_; }
    operator lv_obj_t *() const { return obj_; }

    void setValue(int32_t val) { synthui_slide_toggle_set_value(obj_, val); }
    int32_t getValue() const { return synthui_slide_toggle_get_value(obj_); }

    void setPositions(int32_t pos) { synthui_slide_toggle_set_positions(obj_, pos); }
    int32_t getPositions() const { return synthui_slide_toggle_get_positions(obj_); }

    void setLeftGlyph(synthui_slide_toggle_glyph_t g) { synthui_slide_toggle_set_left_glyph(obj_, g); }
    synthui_slide_toggle_glyph_t getLeftGlyph() const { return synthui_slide_toggle_get_left_glyph(obj_); }

    void setRightGlyph(synthui_slide_toggle_glyph_t g) { synthui_slide_toggle_set_right_glyph(obj_, g); }
    synthui_slide_toggle_glyph_t getRightGlyph() const { return synthui_slide_toggle_get_right_glyph(obj_); }

    void setPanelColor(uint32_t c) { synthui_slide_toggle_set_panel_color(obj_, c); }
    uint32_t getPanelColor() const { return synthui_slide_toggle_get_panel_color(obj_); }

    void setDisabled(bool d) { synthui_slide_toggle_set_disabled(obj_, d); }
    bool isDisabled() const { return synthui_slide_toggle_get_disabled(obj_); }

private:
    lv_obj_t *obj_;
};
} // namespace synthui
#endif

#endif /* SYNTHUI_SLIDE_TOGGLE_H */
