/* synthui_waveform_math.h - pure arithmetic for the waveform widget: which
 * peak table to draw from, the zoom limits, frame <-> pixel mapping, reducing
 * peak points to one pixel column, beat-grid thinning and drag mapping.
 * Header-only and LVGL-free so a host compiler can unit-test it directly
 * (tests/waveform_math_test.c); synthui_waveform.cpp includes it.
 *
 * Positions are in source FRAMES as doubles (a 9 minute track is 24 million
 * frames; a float cannot hold that).  Zoom is frames per pixel.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_WAVEFORM_MATH_H
#define SYNTHUI_WAVEFORM_MATH_H

#include <stdint.h>
#include <stddef.h>

/* The coarsest table that still has at least one point per pixel, so a column
 * never has to reduce more points than the next-finer table's ratio.  fpp[] is
 * frames per point, ascending (finest first).  Zoomed in past the finest table
 * the finest is still the answer (its points are then wider than a pixel). */
static inline int synthui_wave_pick_table(double frames_per_px, const uint32_t *fpp, int n)
{
    int pick = 0;
    for (int i = 1; i < n; i++)
        if ((double)fpp[i] <= frames_per_px) pick = i;
    return pick;
}

/* Zoom limits: no closer than one finest-table point per pixel, no further
 * out than the whole track across the lane.  NaN and degenerate inputs give
 * the closest zoom. */
static inline double synthui_wave_zoom_clamp(double frames_per_px, double total_frames,
                                             int lane_w, uint32_t finest_fpp)
{
    const double lo = (double)finest_fpp;
    double hi = (lane_w > 0 && total_frames > 0.0) ? total_frames / (double)lane_w : lo;
    if (hi < lo) hi = lo;
    if (!(frames_per_px >= lo)) return lo;          /* also catches NaN */
    if (frames_per_px > hi) return hi;
    return frames_per_px;
}

/* Frame at the lane's left edge.  Tracking: the head sits at the lane's
 * centre, so this is negative near the start (empty lane before frame 0).
 * Free view: scroll_frame, clamped so the track's start and end can each be
 * brought to the centre but no further. */
static inline double synthui_wave_view_start(double pos, int lane_w, double frames_per_px,
                                             int tracking, double scroll_frame, double total_frames)
{
    const double half = (double)(lane_w / 2) * frames_per_px;
    if (tracking) return pos - half;
    if (scroll_frame < -half) return -half;
    if (scroll_frame > total_frames - half) return total_frames - half;
    return scroll_frame;
}

static inline double synthui_wave_x_to_frame(int x, double view_start, double frames_per_px)
{
    return view_start + (double)x * frames_per_px;
}

/* May be < 0 or >= the lane width; saturates instead of overflowing int. */
static inline int synthui_wave_frame_to_x(double frame, double view_start, double frames_per_px)
{
    if (!(frames_per_px > 0.0)) return 0;
    const double x = (frame - view_start) / frames_per_px;
    if (x >= 1.0e9) return 1000000000;
    if (x <= -1.0e9) return -1000000000;
    return (int)(x < 0.0 ? x - 0.5 : x + 0.5);
}

/* Reduce points [first,last] of a table to one column.  A point is
 * {min,max} per channel (APV1: mono 2 bytes, stereo 4); both channels share
 * the lane, so the column is the min of mins and the max of maxes.  The range
 * is clamped to [0,count).  Returns 0 (and leaves *mn, *mx alone) when
 * nothing of the range is inside the table. */
static inline int synthui_wave_column(const int8_t *pts, uint32_t count, uint8_t channels,
                                      int64_t first, int64_t last, int8_t *mn, int8_t *mx)
{
    if (!pts || count == 0) return 0;
    if (first < 0) first = 0;
    if (last >= (int64_t)count) last = (int64_t)count - 1;
    if (first > last) return 0;
    const int per = channels == 2 ? 4 : 2;
    int lo = 127, hi = -128;
    for (int64_t i = first; i <= last; i++) {
        const int8_t *p = pts + (size_t)i * (size_t)per;
        for (int c = 0; c < per; c += 2) {
            if (p[c] < lo) lo = p[c];
            if (p[c + 1] > hi) hi = p[c + 1];
        }
    }
    *mn = (int8_t)lo;
    *mx = (int8_t)hi;
    return 1;
}

/* Column extent in pixels inside a lane of height h: *y0 <= *y1, the centre
 * line at h/2, positive samples drawn above it, never less than 1 px tall. */
static inline void synthui_wave_column_px(int8_t mn, int8_t mx, int h, int *y0, int *y1)
{
    if (h <= 0) { *y0 = 0; *y1 = 0; return; }
    const int mid = h / 2;
    const int up = (int)mx * mid, dn = (int)mn * mid;        /* rounded to nearest, either sign */
    int top = mid - (up >= 0 ? up + 64 : up - 64) / 128;     /* mx = 127  -> 1 */
    int bot = mid - (dn >= 0 ? dn + 64 : dn - 64) / 128;     /* mn = -128 -> h, clamped below */
    if (top < 0) top = 0;
    if (bot > h - 1) bot = h - 1;
    if (bot < top) bot = top;
    *y0 = top;
    *y1 = bot;
}

/* Draw every stride-th beat so that lines are at least 6 px apart:
 * 1, 2, 4, 8, ...  beat_px is the spacing of consecutive beats in pixels. */
static inline uint32_t synthui_wave_beat_stride(double beat_px)
{
    uint32_t s = 1;
    if (!(beat_px > 0.0)) return 1u << 30;          /* nothing sensible to draw: one line at most */
    while ((double)s * beat_px < 6.0 && s < (1u << 30)) s <<= 1;
    return s;
}

/* The frame a drag has pulled the head to: the finger moved dx_px since the
 * grab (right = positive), and dragging right pulls EARLIER audio under the
 * head, as on a record.  Clamped to the track. */
static inline double synthui_wave_grab_frame(double frame_at_grab, int dx_px, double frames_per_px,
                                             double total_frames)
{
    double f = frame_at_grab - (double)dx_px * frames_per_px;
    if (f > total_frames - 1.0) f = total_frames - 1.0;
    if (!(f > 0.0)) f = 0.0;
    return f;
}

#endif /* SYNTHUI_WAVEFORM_MATH_H */
