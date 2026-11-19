/* synthui_waveform.h - SynthUI Waveform, LVGL 9 custom widget.
 *
 * A scrolling min/max waveform lane with a fixed centre playhead (or a free
 * view), a beat grid, a cue marker, zoom, and an overview rail of the whole
 * track underneath.  The widget draws from peak tables the application owns;
 * it reads no files and plays no audio.
 *
 * How it draws.  While tracking, the whole lane changes on every frame, and
 * on the boards this targets a CPU redraw of the lane is far too slow.  So
 * the widget keeps the lane in a strip of its own (a ring in x: scrolling
 * draws only the columns that enter) and copies it into the display buffer
 * from a compose hook, with a blit the platform supplies:
 *
 *     synthui_waveform_set_platform(&p);                    // alloc + blit
 *     lvgl_mipi_panel_set_preflip_cb(synthui_waveform_compose_into);
 *
 * LVGL itself draws nothing for the widget (it reports itself as covering its
 * area), so nothing may be placed on top of it.  Measured on the Nosh board:
 * 30 fps, the pipeline's ceiling (evkb examples/display/waveform_draw_bench).
 *
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_WAVEFORM_H
#define SYNTHUI_WAVEFORM_H

#include <lvgl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One peak table.  A point is {int8 min, int8 max} per channel (the APV1
 * layout: 2 bytes mono, 4 bytes stereo) and summarises frames_per_pt frames.
 * The application may raise `count` while a track loads. */
typedef struct {
    const int8_t *pts;
    uint32_t count;
    uint32_t frames_per_pt;
} synthui_wave_table_t;

typedef struct {
    uint32_t frame;
    uint8_t  downbeat;
} synthui_wave_beat_t;

#define SYNTHUI_WAVEFORM_RAIL_H 64      /* overview rail height, px */
#define SYNTHUI_WAVEFORM_GAP    8       /* between the lane and the rail */

/* What the widget needs from the board.  NULL members (or no call at all)
 * select the defaults: lv_malloc / lv_free and a CPU row copy.  The lane strip
 * is lane_w * lane_h * 4 bytes (about 1.5 MB at 1120 x 336), which the LVGL
 * heap will not hold on most configurations. */
typedef struct {
    void *(*alloc)(size_t bytes);
    void  (*release)(void *p);
    /* Copy w x h XRGB8888 pixels; pitches in bytes. */
    void  (*blit)(const uint32_t *src, uint32_t src_pitch, uint32_t *dst, uint32_t dst_pitch,
                  uint16_t w, uint16_t h);
} synthui_waveform_platform_t;
void synthui_waveform_set_platform(const synthui_waveform_platform_t *p);   /* before the first create */

/* Compose every visible waveform into the buffer LVGL has just rendered
 * (XRGB8888, pitch = the display's horizontal resolution * 4).  Register it
 * as the display port's pre-flip hook. */
void synthui_waveform_compose_into(uint8_t *buf);

extern const lv_obj_class_t synthui_waveform_class;
lv_obj_t *synthui_waveform_create(lv_obj_t *parent);

/* Tables finest first, n = 1..4, channels 1 or 2.  The widget keeps the
 * POINTERS (to the array of tables and to each table's points). */
void   synthui_waveform_set_tables(lv_obj_t *obj, const synthui_wave_table_t *tables, uint8_t n,
                                   uint8_t channels, uint64_t total_frames, uint32_t sample_rate);
void   synthui_waveform_clear(lv_obj_t *obj);                           /* no track */
void   synthui_waveform_set_valid_frames(lv_obj_t *obj, uint64_t frames);  /* how much is loaded so far */
void   synthui_waveform_set_position(lv_obj_t *obj, double frame);
void   synthui_waveform_set_zoom(lv_obj_t *obj, double frames_per_px);  /* clamped: finest table .. whole track */
double synthui_waveform_get_zoom(const lv_obj_t *obj);
void   synthui_waveform_zoom_step(lv_obj_t *obj, int dir);              /* +1 = in (x0.5), -1 = out (x2) */
void   synthui_waveform_zoom_fit(lv_obj_t *obj);
void   synthui_waveform_set_tracking(lv_obj_t *obj, bool on);           /* on: head fixed at the centre */
bool   synthui_waveform_get_tracking(const lv_obj_t *obj);
void   synthui_waveform_set_cue(lv_obj_t *obj, int64_t frame);          /* < 0 = none */
void   synthui_waveform_set_beats(lv_obj_t *obj, const synthui_wave_beat_t *beats, uint32_t n);  /* pointer kept; sorted */

/* Diagnostic: how many lane columns currently carry a beat line. */
uint32_t synthui_waveform_grid_columns(const lv_obj_t *obj);

#ifdef __cplusplus
}
#endif
#endif /* SYNTHUI_WAVEFORM_H */
