/* synthui_level_meter.cpp - SynthUI LevelMeter, LVGL 9 custom widget.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#include "synthui_level_meter.h"
#include "synthui_level_meter_math.h"
#include <lvgl_private.h>
#include <cmath>
#include <algorithm>

#define MY_CLASS (&synthui_level_meter_class)

typedef struct {
    lv_obj_t obj;
    float value;
    float peak;
    uint8_t segments;
    bool clip;
    uint32_t panel_color;
} synthui_level_meter_t;

static void meter_constructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void meter_destructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void meter_event(const lv_obj_class_t *cls, lv_event_t *e);
static void meter_draw(synthui_level_meter_t *meter, lv_layer_t *layer);

const lv_obj_class_t synthui_level_meter_class = {
    .base_class     = &lv_obj_class,
    .constructor_cb = meter_constructor,
    .destructor_cb  = meter_destructor,
    .event_cb       = meter_event,
    .name           = "synthui_level_meter",
    .width_def      = 34,
    .height_def     = 200,
    .instance_size  = sizeof(synthui_level_meter_t),
};

lv_obj_t *synthui_level_meter_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&synthui_level_meter_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

static void meter_constructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    synthui_level_meter_t *meter = (synthui_level_meter_t *)obj;
    meter->value = synthui::level_meter::DEFAULT_VALUE;       /* 0.6f */
    meter->peak = synthui::level_meter::DEFAULT_PEAK;         /* 0.8f */
    meter->segments = synthui::level_meter::DEFAULT_SEGMENTS; /* 10 */
    meter->clip = false;
    meter->panel_color = synthui::level_meter::COLOR_PANEL_DEF; /* 0x6D7A85 */
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void meter_destructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    LV_UNUSED(obj);
}

static void meter_event(const lv_obj_class_t *cls, lv_event_t *e)
{
    LV_UNUSED(cls);
    if (lv_obj_event_base(MY_CLASS, e) != LV_RESULT_OK) return;
    if (lv_event_get_code(e) == LV_EVENT_DRAW_MAIN) {
        meter_draw((synthui_level_meter_t *)lv_event_get_current_target_obj(e),
                   lv_event_get_layer(e));
    }
}

static void meter_draw(synthui_level_meter_t *meter, lv_layer_t *layer)
{
    lv_area_t coords;
    lv_obj_get_coords((lv_obj_t *)meter, &coords);
    const int32_t w_px = lv_area_get_width(&coords);
    const int32_t h_px = lv_area_get_height(&coords);
    if (w_px <= 0 || h_px <= 0) return;

    synthui::level_meter::MeterLayout l;
    if (!synthui::level_meter::compute_layout((float)w_px, (float)h_px,
                                              meter->segments, meter->value, meter->peak, l)) {
        return;
    }

    /* Pass 1: Panel background rect with panel_color clipped to layer->_clip_area */
    lv_area_t bg_area;
    if (!_lv_area_intersect(&bg_area, &coords, &layer->_clip_area)) {
        return;
    }

    lv_draw_rect_dsc_t bg_dsc;
    lv_draw_rect_dsc_init(&bg_dsc);
    bg_dsc.bg_color = lv_color_hex(meter->panel_color);
    bg_dsc.bg_opa = LV_OPA_COVER;
    bg_dsc.radius = 0;
    lv_draw_rect(layer, &bg_dsc, &bg_area);

    /* Pass 2: For each segment intersecting layer->_clip_area */
    const float u = l.u;
    const float glow_margin_units = 0.35f * l.seg_h;
    const int32_t glow_margin_px = (int32_t)lroundf(glow_margin_units * u);

    for (uint8_t i = 0; i < l.n; i++) {
        synthui::level_meter::SegmentGeom s;
        synthui::level_meter::get_segment_geom(l, i, meter->clip, s);

        /* Slot pixel coordinates */
        lv_area_t slot_area;
        slot_area.x1 = coords.x1 + (int32_t)lroundf(s.slot.x * u);
        slot_area.y1 = coords.y1 + (int32_t)lroundf(s.slot.y * u);
        slot_area.x2 = coords.x1 + (int32_t)lroundf((s.slot.x + s.slot.w) * u) - 1;
        slot_area.y2 = coords.y1 + (int32_t)lroundf((s.slot.y + s.slot.h) * u) - 1;

        /* Segment bounding box including glow */
        lv_area_t seg_bbox;
        seg_bbox.x1 = slot_area.x1 - glow_margin_px;
        seg_bbox.y1 = slot_area.y1 - glow_margin_px;
        seg_bbox.x2 = slot_area.x2 + glow_margin_px;
        seg_bbox.y2 = slot_area.y2 + glow_margin_px;

        /* Clip check: if segment bounding box does not intersect layer->_clip_area, skip */
        lv_area_t intersect;
        if (!_lv_area_intersect(&intersect, &seg_bbox, &layer->_clip_area)) {
            continue;
        }

        const int32_t slot_rx_px = (int32_t)lroundf(s.slot.rx * u);
        const lv_color_t seg_color = lv_color_hex(s.color);

        /* 1. Slot background (#12161A, cover, radius rx * u) */
        lv_draw_rect_dsc_t slot_dsc;
        lv_draw_rect_dsc_init(&slot_dsc);
        slot_dsc.bg_color = lv_color_hex(synthui::level_meter::COLOR_SLOT_BG);
        slot_dsc.bg_opa = LV_OPA_COVER;
        slot_dsc.radius = slot_rx_px;
        lv_draw_rect(layer, &slot_dsc, &slot_area);

        /* 2. Glow (if on): inflated rounded rect by 0.35 * seg_h * u, color, opacity 56 (is_peak) or 87 (normal) */
        if (s.on) {
            lv_draw_rect_dsc_t glow_dsc;
            lv_draw_rect_dsc_init(&glow_dsc);
            glow_dsc.bg_color = seg_color;
            glow_dsc.bg_opa = s.is_peak ? 56 : 87;
            glow_dsc.radius = slot_rx_px + glow_margin_px;
            lv_draw_rect(layer, &glow_dsc, &seg_bbox);
        }

        /* 3. Core segment: color, opacity 255 (on) or 43 (unlit 17%) */
        lv_draw_rect_dsc_t core_dsc;
        lv_draw_rect_dsc_init(&core_dsc);
        core_dsc.bg_color = seg_color;
        core_dsc.bg_opa = s.on ? LV_OPA_COVER : 43;
        core_dsc.radius = slot_rx_px;
        lv_draw_rect(layer, &core_dsc, &slot_area);

        /* 4. Highlight strip: #FFFFFF, opacity 128 (on) or 15 (unlit 6%) */
        lv_area_t hl_area;
        hl_area.x1 = coords.x1 + (int32_t)lroundf(s.highlight.x * u);
        hl_area.y1 = coords.y1 + (int32_t)lroundf(s.highlight.y * u);
        hl_area.x2 = coords.x1 + (int32_t)lroundf((s.highlight.x + s.highlight.w) * u) - 1;
        hl_area.y2 = coords.y1 + (int32_t)lroundf((s.highlight.y + s.highlight.h) * u) - 1;

        lv_draw_rect_dsc_t hl_dsc;
        lv_draw_rect_dsc_init(&hl_dsc);
        hl_dsc.bg_color = lv_color_hex(synthui::level_meter::COLOR_HIGHLIGHT);
        hl_dsc.bg_opa = s.on ? 128 : 15;
        hl_dsc.radius = (int32_t)lroundf(s.highlight.rx * u);
        lv_draw_rect(layer, &hl_dsc, &hl_area);
    }
}

static bool get_segment_range_bbox(const lv_area_t *coords, const synthui::level_meter::MeterLayout *l,
                                   uint8_t min_i, uint8_t max_i, lv_area_t *out_bbox)
{
    if (min_i > max_i || max_i >= l->n) return false;

    const float u = l->u;
    const float glow_margin_units = 0.35f * l->seg_h;
    const int32_t glow_margin_px = (int32_t)lroundf(glow_margin_units * u);

    /* Segment 0 is at bottom (largest y), segment n-1 is at top (smallest y).
     * max_i has the top-most y, and min_i has the bottom-most y. */
    synthui::level_meter::SegmentGeom s_top;
    synthui::level_meter::get_segment_geom(*l, max_i, false, s_top);

    synthui::level_meter::SegmentGeom s_bot;
    synthui::level_meter::get_segment_geom(*l, min_i, false, s_bot);

    int32_t top_y1 = coords->y1 + (int32_t)lroundf(s_top.slot.y * u) - glow_margin_px;
    int32_t bot_y2 = coords->y1 + (int32_t)lroundf((s_bot.slot.y + s_bot.slot.h) * u) - 1 + glow_margin_px;

    /* Expand horizontally to full widget width, with 1px vertical safety slack */
    out_bbox->x1 = coords->x1;
    out_bbox->x2 = coords->x2;
    out_bbox->y1 = top_y1 - 1;
    out_bbox->y2 = bot_y2 + 1;

    /* Clamp to widget bounds */
    if (out_bbox->y1 < coords->y1) out_bbox->y1 = coords->y1;
    if (out_bbox->y2 > coords->y2) out_bbox->y2 = coords->y2;
    if (out_bbox->x1 < coords->x1) out_bbox->x1 = coords->x1;
    if (out_bbox->x2 > coords->x2) out_bbox->x2 = coords->x2;

    return (out_bbox->y1 <= out_bbox->y2 && out_bbox->x1 <= out_bbox->x2);
}

static inline bool is_segment_flipped(const synthui::level_meter::MeterLayout &l_old,
                                      const synthui::level_meter::MeterLayout &l_new,
                                      bool clip_old, bool clip_new, uint8_t i)
{
    bool on_old = (static_cast<int32_t>(i) < l_old.lit) ||
                  (static_cast<int32_t>(i) == l_old.peak_idx) ||
                  (clip_old && i >= l_old.n - 1);
    bool on_new = (static_cast<int32_t>(i) < l_new.lit) ||
                  (static_cast<int32_t>(i) == l_new.peak_idx) ||
                  (clip_new && i >= l_new.n - 1);
    if (on_old != on_new) return true;
    if (on_old) {
        bool peak_old = (static_cast<int32_t>(i) == l_old.peak_idx && static_cast<int32_t>(i) >= l_old.lit);
        bool peak_new = (static_cast<int32_t>(i) == l_new.peak_idx && static_cast<int32_t>(i) >= l_new.lit);
        if (peak_old != peak_new) return true;
    }
    return false;
}

void synthui_level_meter_set_value(lv_obj_t *obj, float val)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (std::isnan(val)) return;
    if (val < 0.0f) val = 0.0f;
    if (val > 1.0f) val = 1.0f;
    synthui_level_meter_t *meter = (synthui_level_meter_t *)obj;
    if (meter->value == val) return;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    const float w = (float)lv_area_get_width(&coords);
    const float h = (float)lv_area_get_height(&coords);

    synthui::level_meter::MeterLayout l_old, l_new;
    bool ok_old = synthui::level_meter::compute_layout(w, h, meter->segments, meter->value, meter->peak, l_old);
    bool ok_new = synthui::level_meter::compute_layout(w, h, meter->segments, val, meter->peak, l_new);

    meter->value = val;
    if (!ok_old || !ok_new) return;

    int min_flipped = -1;
    int max_flipped = -1;
    for (uint8_t i = 0; i < l_old.n; i++) {
        if (is_segment_flipped(l_old, l_new, meter->clip, meter->clip, i)) {
            if (min_flipped == -1) min_flipped = i;
            max_flipped = i;
        }
    }

    if (min_flipped != -1) {
        lv_area_t dirty;
        if (get_segment_range_bbox(&coords, &l_new, (uint8_t)min_flipped, (uint8_t)max_flipped, &dirty)) {
            lv_obj_invalidate_area(obj, &dirty);
        }
    }
}

float synthui_level_meter_get_value(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_level_meter_t *meter = (const synthui_level_meter_t *)obj;
    return meter->value;
}

void synthui_level_meter_set_peak(lv_obj_t *obj, float peak)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (std::isnan(peak)) return;
    if (peak < -1.0f) peak = -1.0f;
    if (peak > 1.0f) peak = 1.0f;
    synthui_level_meter_t *meter = (synthui_level_meter_t *)obj;
    if (meter->peak == peak) return;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    const float w = (float)lv_area_get_width(&coords);
    const float h = (float)lv_area_get_height(&coords);

    synthui::level_meter::MeterLayout l_old, l_new;
    bool ok_old = synthui::level_meter::compute_layout(w, h, meter->segments, meter->value, meter->peak, l_old);
    bool ok_new = synthui::level_meter::compute_layout(w, h, meter->segments, meter->value, peak, l_new);

    meter->peak = peak;
    if (!ok_old || !ok_new) return;

    int32_t old_p = l_old.peak_idx;
    int32_t new_p = l_new.peak_idx;
    if (old_p == new_p) return;

    if (old_p >= 0 && old_p < l_old.n) {
        lv_area_t dirty_old;
        if (get_segment_range_bbox(&coords, &l_old, (uint8_t)old_p, (uint8_t)old_p, &dirty_old)) {
            lv_obj_invalidate_area(obj, &dirty_old);
        }
    }
    if (new_p >= 0 && new_p < l_new.n) {
        lv_area_t dirty_new;
        if (get_segment_range_bbox(&coords, &l_new, (uint8_t)new_p, (uint8_t)new_p, &dirty_new)) {
            lv_obj_invalidate_area(obj, &dirty_new);
        }
    }
}

float synthui_level_meter_get_peak(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_level_meter_t *meter = (const synthui_level_meter_t *)obj;
    return meter->peak;
}

void synthui_level_meter_set_clip(lv_obj_t *obj, bool clip)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_level_meter_t *meter = (synthui_level_meter_t *)obj;
    if (meter->clip == clip) return;
    meter->clip = clip;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    const float w = (float)lv_area_get_width(&coords);
    const float h = (float)lv_area_get_height(&coords);

    synthui::level_meter::MeterLayout l;
    if (!synthui::level_meter::compute_layout(w, h, meter->segments, meter->value, meter->peak, l)) return;

    lv_area_t dirty;
    if (get_segment_range_bbox(&coords, &l, (uint8_t)(l.n - 1), (uint8_t)(l.n - 1), &dirty)) {
        lv_obj_invalidate_area(obj, &dirty);
    }
}

bool synthui_level_meter_get_clip(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_level_meter_t *meter = (const synthui_level_meter_t *)obj;
    return meter->clip;
}

void synthui_level_meter_set_segments(lv_obj_t *obj, uint8_t segments)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    if (segments < synthui::level_meter::MIN_SEGMENTS) segments = synthui::level_meter::MIN_SEGMENTS;
    if (segments > synthui::level_meter::MAX_SEGMENTS) segments = synthui::level_meter::MAX_SEGMENTS;
    synthui_level_meter_t *meter = (synthui_level_meter_t *)obj;
    if (meter->segments == segments) return;
    meter->segments = segments;
    lv_obj_invalidate(obj);
}

uint8_t synthui_level_meter_get_segments(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_level_meter_t *meter = (const synthui_level_meter_t *)obj;
    return meter->segments;
}

void synthui_level_meter_set_panel_color(lv_obj_t *obj, uint32_t rgb_hex)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    synthui_level_meter_t *meter = (synthui_level_meter_t *)obj;
    if (meter->panel_color == rgb_hex) return;
    meter->panel_color = rgb_hex;
    lv_obj_invalidate(obj);
}

uint32_t synthui_level_meter_get_panel_color(const lv_obj_t *obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    const synthui_level_meter_t *meter = (const synthui_level_meter_t *)obj;
    return meter->panel_color;
}
