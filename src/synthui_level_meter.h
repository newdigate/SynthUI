/* synthui_level_meter.h - SynthUI LevelMeter, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_LEVEL_METER_H
#define SYNTHUI_LEVEL_METER_H

#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_obj_class_t synthui_level_meter_class;

lv_obj_t *synthui_level_meter_create(lv_obj_t *parent);

void synthui_level_meter_set_value(lv_obj_t *obj, float val);
float synthui_level_meter_get_value(const lv_obj_t *obj);

void synthui_level_meter_set_peak(lv_obj_t *obj, float peak);
float synthui_level_meter_get_peak(const lv_obj_t *obj);

void synthui_level_meter_set_clip(lv_obj_t *obj, bool clip);
bool synthui_level_meter_get_clip(const lv_obj_t *obj);

void synthui_level_meter_set_segments(lv_obj_t *obj, uint8_t segments);
uint8_t synthui_level_meter_get_segments(const lv_obj_t *obj);

void synthui_level_meter_set_panel_color(lv_obj_t *obj, uint32_t rgb_hex);
uint32_t synthui_level_meter_get_panel_color(const lv_obj_t *obj);

#ifdef __cplusplus
} // extern "C"

namespace synthui {
class LevelMeter {
public:
    explicit LevelMeter(lv_obj_t *obj = nullptr) : obj_(obj) {}
    static LevelMeter create(lv_obj_t *parent) {
        return LevelMeter(synthui_level_meter_create(parent));
    }
    lv_obj_t *raw() const { return obj_; }
    operator lv_obj_t *() const { return obj_; }

    void setValue(float v) { synthui_level_meter_set_value(obj_, v); }
    float getValue() const { return synthui_level_meter_get_value(obj_); }

    void setPeak(float p) { synthui_level_meter_set_peak(obj_, p); }
    float getPeak() const { return synthui_level_meter_get_peak(obj_); }

    void setClip(bool c) { synthui_level_meter_set_clip(obj_, c); }
    bool getClip() const { return synthui_level_meter_get_clip(obj_); }

    void setSegments(uint8_t s) { synthui_level_meter_set_segments(obj_, s); }
    uint8_t getSegments() const { return synthui_level_meter_get_segments(obj_); }

    void setPanelColor(uint32_t rgb) { synthui_level_meter_set_panel_color(obj_, rgb); }
    uint32_t getPanelColor() const { return synthui_level_meter_get_panel_color(obj_); }

private:
    lv_obj_t *obj_;
};
} // namespace synthui
#endif

#endif // SYNTHUI_LEVEL_METER_H
