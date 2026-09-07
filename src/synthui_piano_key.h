/* synthui_piano_key.h - SynthUI PianoKey, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_PIANO_KEY_H
#define SYNTHUI_PIANO_KEY_H

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>
#include "synthui_piano_key_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_obj_class_t synthui_piano_key_class;

lv_obj_t *synthui_piano_key_create(lv_obj_t *parent);

void synthui_piano_key_set_type(lv_obj_t *obj, synthui_piano_key_type_t type);
synthui_piano_key_type_t synthui_piano_key_get_type(const lv_obj_t *obj);

void synthui_piano_key_set_lit(lv_obj_t *obj, bool lit);
bool synthui_piano_key_get_lit(const lv_obj_t *obj);

void synthui_piano_key_set_pressed(lv_obj_t *obj, bool pressed);
bool synthui_piano_key_get_pressed(const lv_obj_t *obj);

void synthui_piano_key_set_zone_top(lv_obj_t *obj, float zone_top);
float synthui_piano_key_get_zone_top(const lv_obj_t *obj);

void synthui_piano_key_set_pad_height(lv_obj_t *obj, float pad_height);
float synthui_piano_key_get_pad_height(const lv_obj_t *obj);

#ifdef __cplusplus
} // extern "C"

namespace synthui {
class PianoKey {
public:
    explicit PianoKey(lv_obj_t *obj = nullptr) : obj_(obj) {}
    static PianoKey create(lv_obj_t *parent) {
        return PianoKey(synthui_piano_key_create(parent));
    }
    lv_obj_t *raw() const { return obj_; }
    operator lv_obj_t *() const { return obj_; }

    void setType(synthui_piano_key_type_t type) { synthui_piano_key_set_type(obj_, type); }
    synthui_piano_key_type_t getType() const { return synthui_piano_key_get_type(obj_); }

    void setLit(bool lit) { synthui_piano_key_set_lit(obj_, lit); }
    bool isLit() const { return synthui_piano_key_get_lit(obj_); }

    void setPressed(bool pressed) { synthui_piano_key_set_pressed(obj_, pressed); }
    bool isPressed() const { return synthui_piano_key_get_pressed(obj_); }

    void setZoneTop(float zone_top) { synthui_piano_key_set_zone_top(obj_, zone_top); }
    float getZoneTop() const { return synthui_piano_key_get_zone_top(obj_); }

    void setPadHeight(float pad_height) { synthui_piano_key_set_pad_height(obj_, pad_height); }
    float getPadHeight() const { return synthui_piano_key_get_pad_height(obj_); }

private:
    lv_obj_t *obj_;
};
} // namespace synthui
#endif

#endif /* SYNTHUI_PIANO_KEY_H */
