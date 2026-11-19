/* synthui_waveform.cpp - SynthUI Waveform (see synthui_waveform.h).
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#include "synthui_waveform.h"
#include "synthui_waveform_math.h"
#include <lvgl_private.h>      /* lv_obj_t by value needs the complete type */
#include <math.h>
#include <string.h>

#define MY_CLASS (&synthui_waveform_class)

/* Palette (Waveform.dc.html), XRGB8888 with X = 0xFF. */
#define C_LANE    0xFF0E141Bu     /* lane and rail background */
#define C_OUT     0xFF0A0E13u     /* before the start / after the end of the track */
#define C_PANEL   0xFF141C26u     /* the gap between lane and rail */
#define C_WAVE    0xFFB9CFE0u
#define C_GRID    0xFF1C2733u
#define C_GRID_DN 0xFF2C3F52u     /* downbeat */
#define C_HEAD    0xFFFFC861u     /* playhead, cue, active */
#define C_WINDOW  0xFF4A3C18u     /* the view window on the rail */

#define MAX_TABLES 4
#define MAX_WIDGETS 4
#define RAIL_BUF_H (SYNTHUI_WAVEFORM_GAP + SYNTHUI_WAVEFORM_RAIL_H)

typedef struct {
    lv_obj_t obj;
    const synthui_wave_table_t *tables;
    uint8_t  ntables, channels;
    uint64_t total, valid;
    uint32_t rate;
    const synthui_wave_beat_t *beats;
    uint32_t nbeats;
    double   pos, zoom, scroll;        /* frames; frames per px; free-view left edge */
    bool     tracking;
    int64_t  cue;
    /* the lane strip: sw x sh pixels, a ring in x holding columns [g0, g0+sw) */
    uint32_t *strip, *rail;
    void     *strip_raw, *rail_raw;
    int      sw, sh;
    int64_t  g0;
    double   strip_zoom;
    uint64_t strip_valid, rail_valid;
    int64_t  strip_cue;
    bool     strip_ok, rail_ok;        /* false = redraw in full at the next compose */
    /* What changed since the last compose.  The lane changes on every frame
     * while tracking; the rail only when one of its markers moves a pixel, so
     * it is recomposed (and its area invalidated) separately: copying and
     * presenting it every frame cost 7 fps on the Nosh board. */
    bool     dirty_lane, dirty_rail;
    int      r_head, r_w0, r_w1, r_cue;   /* rail marker x positions as last composed */
    /* input: anchored on PRESSED, mapped from the TOTAL displacement (the fader's rule) */
    uint8_t  press;                    /* PRESS_NONE .. */
    int32_t  press_x;
    double   press_frame;              /* GRAB: the head frame at the press; SCROLL: the view's left edge */
    double   last_frame;               /* the last GRAB_MOVE / SEEK frame sent */
} wf_t;
enum { PRESS_NONE, PRESS_GRAB, PRESS_SCROLL, PRESS_RAIL, PRESS_CHEVRON };

static void wf_constructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void wf_destructor(const lv_obj_class_t *cls, lv_obj_t *obj);
static void wf_event(const lv_obj_class_t *cls, lv_event_t *e);

const lv_obj_class_t synthui_waveform_class = {
    .base_class     = &lv_obj_class,
    .constructor_cb = wf_constructor,
    .destructor_cb  = wf_destructor,
    .event_cb       = wf_event,
    .name           = "synthui_waveform",
    .width_def      = 640,
    .height_def     = 240,
    .instance_size  = sizeof(wf_t),
};

/* ---- platform ---------------------------------------------------------- */
static void cpu_blit(const uint32_t *src, uint32_t sp, uint32_t *dst, uint32_t dp, uint16_t w, uint16_t h)
{
    for (uint16_t y = 0; y < h; y++)
        memcpy((uint8_t *)dst + (size_t)y * dp, (const uint8_t *)src + (size_t)y * sp, (size_t)w * 4u);
}
static void *def_alloc(size_t n) { return lv_malloc(n); }
static void def_release(void *p) { lv_free(p); }
static synthui_waveform_platform_t s_plat = { def_alloc, def_release, cpu_blit };

void synthui_waveform_set_platform(const synthui_waveform_platform_t *p)
{
    s_plat.alloc   = (p && p->alloc)   ? p->alloc   : def_alloc;
    s_plat.release = (p && p->release) ? p->release : def_release;
    s_plat.blit    = (p && p->blit)    ? p->blit    : cpu_blit;
}

static wf_t *s_widgets[MAX_WIDGETS];
static void wf_released(wf_t *w, bool lost);

/* ---- helpers ----------------------------------------------------------- */
static inline int64_t floor_div(double a, double b) { return (int64_t)floor(a / b); }
static inline int ring_x(const wf_t *w, int64_t g)
{
    int64_t m = g % (int64_t)w->sw;
    return (int)(m < 0 ? m + w->sw : m);
}
static double view_start(const wf_t *w);
static void lane_area(const wf_t *w, lv_area_t *a)
{
    lv_obj_get_coords(&w->obj, a);
    a->y2 = a->y1 + w->sh - 1;
}
static void rail_area(const wf_t *w, lv_area_t *a)
{
    lv_obj_get_coords(&w->obj, a);
    a->y1 = a->y1 + w->sh;
}
/* Where the rail's markers belong now: playhead, view window edges, cue. */
static void rail_marks(const wf_t *w, int *head, int *w0, int *w1, int *cue)
{
    if (w->total == 0 || w->sw == 0) { *head = *w0 = *w1 = *cue = -1; return; }
    const double rz = (double)w->total / (double)w->sw;
    const double vs = (double)floor(view_start(w) / w->zoom) * w->zoom;
    *head = synthui_wave_frame_to_x(w->pos, 0.0, rz);
    *w0 = synthui_wave_frame_to_x(vs, 0.0, rz);
    *w1 = synthui_wave_frame_to_x(vs + (double)w->sw * w->zoom, 0.0, rz);
    if (*w0 < 0) *w0 = 0;
    if (*w1 > w->sw - 1) *w1 = w->sw - 1;
    *cue = w->cue >= 0 ? synthui_wave_frame_to_x((double)w->cue, 0.0, rz) : -1;
}
/* Everything changed. */
static void touch(wf_t *w) { w->dirty_lane = w->dirty_rail = true; lv_obj_invalidate(&w->obj); }
/* The view moved (position, scroll): the lane, and the rail only if a marker moved. */
static void touch_view(wf_t *w)
{
    if (w->sw == 0) { touch(w); return; }
    lv_area_t a;
    w->dirty_lane = true;
    lane_area(w, &a);
    lv_obj_invalidate_area(&w->obj, &a);
    int h, w0, w1, c;
    rail_marks(w, &h, &w0, &w1, &c);
    if (h != w->r_head || w0 != w->r_w0 || w1 != w->r_w1 || c != w->r_cue) {
        w->dirty_rail = true;
        rail_area(w, &a);
        lv_obj_invalidate_area(&w->obj, &a);
    }
}

static double clamp_zoom(const wf_t *w, double z)
{
    const uint32_t finest = (w->tables && w->ntables) ? w->tables[0].frames_per_pt : 128u;
    /* Before the first layout the lane has no width, so the far limit is not
     * known yet: keep what was asked for (compose clamps again once it is). */
    if (w->sw <= 0) return (z >= (double)finest) ? z : (double)finest;
    return synthui_wave_zoom_clamp(z, (double)w->total, w->sw, finest ? finest : 1u);
}

/* Is there a beat line in [f0, f1)?  0 none, 1 beat, 2 downbeat. */
static int beat_in(const wf_t *w, double f0, double f1, uint32_t stride)
{
    if (!w->beats || w->nbeats == 0 || f1 <= 0.0) return 0;
    uint32_t lo = 0, hi = w->nbeats;                 /* first beat with frame >= f0 */
    while (lo < hi) {
        const uint32_t mid = lo + (hi - lo) / 2;
        if ((double)w->beats[mid].frame < f0) lo = mid + 1; else hi = mid;
    }
    int best = 0;
    for (uint32_t i = lo; i < w->nbeats && (double)w->beats[i].frame < f1; i++) {
        if (i % stride) continue;
        if (w->beats[i].downbeat) return 2;
        best = 1;
    }
    return best;
}
static uint32_t beat_stride(const wf_t *w, double zoom)
{
    if (!w->beats || w->nbeats < 2) return 1;
    const double span = (double)w->beats[w->nbeats - 1].frame - (double)w->beats[0].frame;
    return synthui_wave_beat_stride(span / (double)(w->nbeats - 1) / zoom);
}

/* Min/max of the track over [f0, f1), from the table that suits `zoom`.
 * False when nothing valid lies in the range. */
static bool extent(const wf_t *w, double f0, double f1, double zoom, int8_t *mn, int8_t *mx)
{
    if (!w->tables || w->ntables == 0 || f1 <= 0.0 || f0 >= (double)w->valid) return false;
    uint32_t fpp[MAX_TABLES];
    for (int i = 0; i < w->ntables; i++) fpp[i] = w->tables[i].frames_per_pt;
    const synthui_wave_table_t *t = &w->tables[synthui_wave_pick_table(zoom, fpp, w->ntables)];
    if (t->frames_per_pt == 0) return false;
    if (f0 < 0.0) f0 = 0.0;
    if (f1 > (double)w->valid) f1 = (double)w->valid;
    uint64_t usable = w->valid / t->frames_per_pt;   /* whole points inside the loaded part */
    if (usable > t->count) usable = t->count;
    const int64_t first = (int64_t)(f0 / (double)t->frames_per_pt);
    int64_t last = (int64_t)ceil(f1 / (double)t->frames_per_pt) - 1;
    if (last < first) last = first;
    return synthui_wave_column(t->pts, (uint32_t)usable, w->channels, first, last, mn, mx) != 0;
}

/* ---- the lane strip ---------------------------------------------------- */
static void strip_col(wf_t *w, int64_t g, uint32_t stride)
{
    const double f0 = (double)g * w->strip_zoom, f1 = f0 + w->strip_zoom;
    uint32_t base = C_LANE;
    if (w->total == 0 || f1 <= 0.0 || f0 >= (double)w->total) {
        base = w->total == 0 ? C_LANE : C_OUT;
    } else {
        const int b = beat_in(w, f0 < 0.0 ? 0.0 : f0, f1, stride);
        if (b) base = b == 2 ? C_GRID_DN : C_GRID;
    }
    int y0 = 1, y1 = 0;                              /* empty */
    int8_t mn, mx;
    if (w->total && extent(w, f0, f1, w->strip_zoom, &mn, &mx)) synthui_wave_column_px(mn, mx, w->sh, &y0, &y1);
    const bool cue = w->cue >= 0 && (double)w->cue >= f0 && (double)w->cue < f1;
    uint32_t *p = w->strip + ring_x(w, g);
    if (cue) {
        for (int y = 0; y < w->sh; y++, p += w->sw) *p = C_HEAD;
    } else {
        for (int y = 0; y < w->sh; y++, p += w->sw) *p = (y >= y0 && y <= y1) ? C_WAVE : base;
    }
}
static void strip_range(wf_t *w, int64_t ga, int64_t gb)       /* [ga, gb), clipped to the strip's columns */
{
    if (ga < w->g0) ga = w->g0;
    if (gb > w->g0 + w->sw) gb = w->g0 + w->sw;
    const uint32_t stride = beat_stride(w, w->strip_zoom);
    for (int64_t g = ga; g < gb; g++) strip_col(w, g, stride);
}
static double view_start(const wf_t *w)
{   /* (declared above) */
    return synthui_wave_view_start(w->pos, w->sw, w->zoom, w->tracking, w->scroll, (double)w->total);
}
static void strip_sync(wf_t *w)
{
    const int64_t want = floor_div(view_start(w), w->zoom);
    const int64_t d = want - w->g0;
    if (!w->strip_ok || w->zoom != w->strip_zoom || d >= w->sw || d <= -(int64_t)w->sw) {
        w->strip_zoom = w->zoom;
        w->g0 = want;
        strip_range(w, want, want + w->sw);
        w->strip_ok = true;
        w->strip_valid = w->valid;
        w->strip_cue = w->cue;
        return;
    }
    if (d > 0) { const int64_t old_end = w->g0 + w->sw; w->g0 = want; strip_range(w, old_end, old_end + d); }
    else if (d < 0) { const int64_t old = w->g0; w->g0 = want; strip_range(w, want, old); }
    if (w->valid != w->strip_valid) {                /* the load moved: redraw what it uncovered */
        const uint64_t a = w->valid < w->strip_valid ? w->valid : w->strip_valid;
        const uint64_t b = w->valid < w->strip_valid ? w->strip_valid : w->valid;
        strip_range(w, floor_div((double)a, w->zoom) - 1, floor_div((double)b, w->zoom) + 2);
        w->strip_valid = w->valid;
    }
    if (w->cue != w->strip_cue) {
        if (w->strip_cue >= 0) { const int64_t g = floor_div((double)w->strip_cue, w->zoom); strip_range(w, g, g + 1); }
        if (w->cue >= 0)       { const int64_t g = floor_div((double)w->cue, w->zoom);       strip_range(w, g, g + 1); }
        w->strip_cue = w->cue;
    }
}

/* ---- the overview rail: the whole track across the widget's width ------- */
static void rail_cols(wf_t *w, int xa, int xb)
{
    if (xa < 0) xa = 0;
    if (xb > w->sw) xb = w->sw;
    const double z = w->total ? (double)w->total / (double)w->sw : 1.0;
    for (int x = xa; x < xb; x++) {
        int y0 = 1, y1 = 0;
        int8_t mn, mx;
        if (w->total && extent(w, (double)x * z, (double)(x + 1) * z, z, &mn, &mx))
            synthui_wave_column_px(mn, mx, SYNTHUI_WAVEFORM_RAIL_H, &y0, &y1);
        uint32_t *p = w->rail + x;
        for (int y = 0; y < SYNTHUI_WAVEFORM_GAP; y++, p += w->sw) *p = C_PANEL;
        for (int y = 0; y < SYNTHUI_WAVEFORM_RAIL_H; y++, p += w->sw) *p = (y >= y0 && y <= y1) ? C_WAVE : C_LANE;
    }
}
static void rail_sync(wf_t *w)
{
    if (!w->rail_ok) {
        rail_cols(w, 0, w->sw);
        w->rail_ok = true;
    } else if (w->valid != w->rail_valid && w->total) {
        const uint64_t a = w->valid < w->rail_valid ? w->valid : w->rail_valid;
        const uint64_t b = w->valid < w->rail_valid ? w->rail_valid : w->valid;
        rail_cols(w, (int)(a * (uint64_t)w->sw / w->total) - 1, (int)(b * (uint64_t)w->sw / w->total) + 2);
    }
    w->rail_valid = w->valid;
}

/* ---- overlays, drawn straight into the display buffer after the blits --- */
typedef struct { uint32_t *px; uint32_t pitch; int w, h; } surf_t;   /* pitch in pixels */
static void vline(const surf_t *s, int x, int y0, int y1, int thick, uint32_t c)
{
    for (int t = 0; t < thick; t++) {
        const int xx = x + t;
        if (xx < 0 || xx >= s->w) continue;
        for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < s->h; y++) s->px[(uint32_t)y * s->pitch + (uint32_t)xx] = c;
    }
}
static void hline(const surf_t *s, int x0, int x1, int y, uint32_t c)
{
    if (y < 0 || y >= s->h) return;
    for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < s->w; x++) s->px[(uint32_t)y * s->pitch + (uint32_t)x] = c;
}
/* Downward triangle, 13 wide and 7 tall, its point at (x, y+6). */
static void head_flag(const surf_t *s, int x, int y)
{
    for (int r = 0; r < 7; r++) hline(s, x - 6 + r, x + 6 - r + 1, y + r, C_HEAD);
}
/* Triangle 12 wide and 24 tall at the lane's edge, POINTING at the side the
 * playhead went off: its tip is on the edge side, its flat side inward. */
static void chevron(const surf_t *s, int dir)
{
    const int cy = s->h / 2;
    for (int r = -12; r <= 12; r++) {
        const int len = 12 - (r < 0 ? -r : r);
        if (dir < 0) hline(s, 4 + 12 - len, 4 + 12, cy + r, C_HEAD);                 /* tip at x = 4 */
        else         hline(s, s->w - 17, s->w - 17 + len, cy + r, C_HEAD);            /* tip at x = w - 5 */
    }
}

static void compose(wf_t *w, uint8_t *buf)
{
    lv_display_t *disp = lv_obj_get_display(&w->obj);
    const int32_t hres = lv_display_get_horizontal_resolution(disp), vres = lv_display_get_vertical_resolution(disp);
    lv_area_t a;
    lv_obj_get_coords(&w->obj, &a);
    if (a.x1 < 0 || a.y1 < 0 || a.x1 + w->sw > hres || a.y1 + w->sh + RAIL_BUF_H > vres) return;   /* partly off-screen: not supported */
    const uint32_t pitch = (uint32_t)hres * 4u;
    uint32_t *lane = (uint32_t *)(buf + (size_t)a.y1 * pitch) + a.x1;

    w->zoom = clamp_zoom(w, w->zoom);
    strip_sync(w);
    rail_sync(w);

    uint32_t *railp = (uint32_t *)((uint8_t *)lane + (size_t)w->sh * pitch);
    const double vs = (double)w->g0 * w->strip_zoom;

    if (w->dirty_lane) {
        /* the ring: strip columns [x0, sw) are the lane's left part */
        const int x0 = ring_x(w, w->g0), w1 = w->sw - x0;
        s_plat.blit(w->strip + x0, (uint32_t)w->sw * 4u, lane, pitch, (uint16_t)w1, (uint16_t)w->sh);
        if (x0) s_plat.blit(w->strip, (uint32_t)w->sw * 4u, lane + w1, pitch, (uint16_t)x0, (uint16_t)w->sh);
        if (w->total) {                                  /* playhead */
            const surf_t ls = { lane, (uint32_t)hres, w->sw, w->sh };
            const int hx = w->tracking ? w->sw / 2 : synthui_wave_frame_to_x(w->pos, vs, w->strip_zoom);
            if (hx < 0) chevron(&ls, -1);
            else if (hx >= w->sw) chevron(&ls, +1);
            else { vline(&ls, hx, 0, w->sh - 1, 2, C_HEAD); head_flag(&ls, hx, 0); }
        }
        w->dirty_lane = false;
    }
    if (w->dirty_rail) {
        s_plat.blit(w->rail, (uint32_t)w->sw * 4u, railp, pitch, (uint16_t)w->sw, (uint16_t)RAIL_BUF_H);
        rail_marks(w, &w->r_head, &w->r_w0, &w->r_w1, &w->r_cue);
        if (w->total) {                                  /* view window, cue and playhead */
            const surf_t rs = { railp + (uint32_t)SYNTHUI_WAVEFORM_GAP * (uint32_t)hres, (uint32_t)hres, w->sw, SYNTHUI_WAVEFORM_RAIL_H };
            const int wx0 = w->r_w0, wx1 = w->r_w1;
            if (wx1 > wx0) {
                hline(&rs, wx0, wx1, 0, C_WINDOW); hline(&rs, wx0, wx1, rs.h - 1, C_WINDOW);
                vline(&rs, wx0, 0, rs.h - 1, 1, C_WINDOW); vline(&rs, wx1, 0, rs.h - 1, 1, C_WINDOW);
            }
            if (w->r_cue >= 0) vline(&rs, w->r_cue, 0, 9, 1, C_HEAD);
            vline(&rs, w->r_head, 0, rs.h - 1, 2, C_HEAD);
        }
        w->dirty_rail = false;
    }
}

void synthui_waveform_compose_into(uint8_t *buf)
{
    if (!buf) return;
    for (int i = 0; i < MAX_WIDGETS; i++) {
        wf_t *w = s_widgets[i];
        if (!w || !(w->dirty_lane || w->dirty_rail) || !w->strip || !w->rail) continue;
        if (lv_obj_get_screen(&w->obj) != lv_display_get_screen_active(lv_obj_get_display(&w->obj))) continue;
        /* hidden itself, or inside something hidden: its dirty flags wait until it is shown
         * (showing it invalidates it, DRAW_MAIN then asks for both parts) */
        if (!lv_obj_is_visible(&w->obj)) continue;
        compose(w, buf);
    }
}

/* ---- input ------------------------------------------------------------- */
lv_event_code_t synthui_waveform_event(synthui_wave_ev_t which)
{
    static lv_event_code_t codes[5];
    static bool registered = false;
    if (!registered) {
        for (int i = 0; i < 5; i++) codes[i] = (lv_event_code_t)lv_event_register_id();
        registered = true;
    }
    return codes[(unsigned)which < 5u ? which : 0];
}
static void send(wf_t *w, synthui_wave_ev_t ev, double frame)
{
    w->last_frame = frame;
    lv_obj_send_event(&w->obj, synthui_waveform_event(ev), ev == SYNTHUI_WAVE_EV_TRACK_REQUEST ? NULL : &frame);
}
/* The press point relative to the widget; false when there is no pointer. */
static bool local_point(const wf_t *w, int32_t *x, int32_t *y)
{
    lv_indev_t *indev = lv_indev_active();
    if (!indev) return false;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_area_t a;
    lv_obj_get_coords(&w->obj, &a);
    *x = p.x - a.x1; *y = p.y - a.y1;
    return true;
}
static double rail_frame(const wf_t *w, int32_t x)
{
    if (x < 0) x = 0;
    if (x > w->sw - 1) x = w->sw - 1;
    return (double)((uint64_t)x * w->total / (uint64_t)w->sw);
}
static void wf_pressed(wf_t *w)
{
    int32_t x, y;
    w->press = PRESS_NONE;
    if (w->total == 0 || w->sw == 0 || !local_point(w, &x, &y)) return;
    w->press_x = x;
    if (y >= w->sh + SYNTHUI_WAVEFORM_GAP) {
        w->press = PRESS_RAIL;
        send(w, SYNTHUI_WAVE_EV_SEEK, rail_frame(w, x));
    } else if (y < w->sh) {
        if (w->tracking) {
            w->press = PRESS_GRAB;
            w->press_frame = w->pos;
            send(w, SYNTHUI_WAVE_EV_GRAB_BEGIN, w->pos);
        } else {
            const double vs = view_start(w);
            const int hx = synthui_wave_frame_to_x(w->pos, vs, w->zoom);
            const bool off_left = hx < 0, off_right = hx >= w->sw;
            const bool near_mid = y > w->sh / 2 - 40 && y < w->sh / 2 + 40;
            if (near_mid && ((off_left && x < 48) || (off_right && x >= w->sw - 48))) {
                w->press = PRESS_CHEVRON;
            } else {
                w->press = PRESS_SCROLL;
                w->press_frame = vs;
            }
        }
    }
}
static void wf_pressing(wf_t *w)
{
    int32_t x, y;
    if (w->press == PRESS_NONE || w->press == PRESS_CHEVRON || !local_point(w, &x, &y)) return;
    const int dx = (int)(x - w->press_x);
    if (w->press == PRESS_GRAB) {
        const double f = synthui_wave_grab_frame(w->press_frame, dx, w->zoom, (double)w->total);
        if (f != w->last_frame) send(w, SYNTHUI_WAVE_EV_GRAB_MOVE, f);
    } else if (w->press == PRESS_SCROLL) {
        const double want = synthui_wave_view_start(w->pos, w->sw, w->zoom, 0,
                                                    w->press_frame - (double)dx * w->zoom, (double)w->total);
        if (want != w->scroll) { w->scroll = want; touch_view(w); }
    } else if (w->press == PRESS_RAIL) {
        const double f = rail_frame(w, x);
        if (f != w->last_frame) send(w, SYNTHUI_WAVE_EV_SEEK, f);
    }
}
static void wf_released(wf_t *w, bool lost)
{
    const uint8_t was = w->press;
    w->press = PRESS_NONE;                               /* first: a handler may re-enter */
    if (was == PRESS_GRAB) send(w, SYNTHUI_WAVE_EV_GRAB_END, w->last_frame);
    else if (was == PRESS_CHEVRON && !lost) send(w, SYNTHUI_WAVE_EV_TRACK_REQUEST, 0.0);
}

/* ---- class ------------------------------------------------------------- */
static void buffers(wf_t *w)
{
    const int sw = (int)lv_obj_get_width(&w->obj);
    const int sh = (int)lv_obj_get_height(&w->obj) - RAIL_BUF_H;
    if (sw == w->sw && sh == w->sh && w->strip) return;
    if (w->strip_raw) s_plat.release(w->strip_raw);
    if (w->rail_raw) s_plat.release(w->rail_raw);
    w->strip_raw = w->rail_raw = NULL; w->strip = w->rail = NULL;
    w->sw = w->sh = 0;
    if (sw < 16 || sh < 16) return;
    /* 64-byte aligned: the blit may be a DMA engine */
    w->strip_raw = s_plat.alloc((size_t)sw * (size_t)sh * 4u + 64u);
    w->rail_raw  = s_plat.alloc((size_t)sw * (size_t)RAIL_BUF_H * 4u + 64u);
    if (!w->strip_raw || !w->rail_raw) {
        if (w->strip_raw) s_plat.release(w->strip_raw);
        if (w->rail_raw) s_plat.release(w->rail_raw);
        w->strip_raw = w->rail_raw = NULL;
        return;
    }
    w->strip = (uint32_t *)(((uintptr_t)w->strip_raw + 63u) & ~(uintptr_t)63u);
    w->rail  = (uint32_t *)(((uintptr_t)w->rail_raw + 63u) & ~(uintptr_t)63u);
    w->sw = sw; w->sh = sh;
    w->strip_ok = w->rail_ok = false;
    w->dirty_lane = w->dirty_rail = true;
}

lv_obj_t *synthui_waveform_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(&synthui_waveform_class, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

static void wf_constructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    wf_t *w = (wf_t *)obj;
    w->zoom = 128.0;
    w->tracking = true;
    w->cue = w->strip_cue = -1;
    w->channels = 2;
    w->dirty_lane = w->dirty_rail = true;
    w->r_head = w->r_w0 = w->r_w1 = w->r_cue = -1;
    for (int i = 0; i < MAX_WIDGETS; i++) if (!s_widgets[i]) { s_widgets[i] = w; break; }
    lv_obj_remove_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN));
}

static void wf_destructor(const lv_obj_class_t *cls, lv_obj_t *obj)
{
    LV_UNUSED(cls);
    wf_t *w = (wf_t *)obj;
    for (int i = 0; i < MAX_WIDGETS; i++) if (s_widgets[i] == w) s_widgets[i] = NULL;
    if (w->strip_raw) s_plat.release(w->strip_raw);
    if (w->rail_raw) s_plat.release(w->rail_raw);
}

static void wf_event(const lv_obj_class_t *cls, lv_event_t *e)
{
    LV_UNUSED(cls);
    if (lv_obj_event_base(MY_CLASS, e) != LV_RESULT_OK) return;
    wf_t *w = (wf_t *)lv_event_get_current_target(e);
    switch (lv_event_get_code(e)) {
    case LV_EVENT_SIZE_CHANGED:
        buffers(w);
        break;
    case LV_EVENT_COVER_CHECK:
        /* opaque: LVGL need not draw what lies under the widget */
        lv_event_set_cover_res(e, LV_COVER_RES_COVER);
        break;
    case LV_EVENT_DRAW_MAIN:
        /* LVGL is redrawing this area for a reason of its own: whatever it
         * leaves here is replaced by the compose hook. */
        if (!w->strip) buffers(w);
        {   /* recompose whichever part LVGL's clip area touches */
            const lv_layer_t *layer = lv_event_get_layer(e);
            lv_area_t la, ra, x;
            lane_area(w, &la); rail_area(w, &ra);
            if (!layer || lv_area_intersect(&x, &layer->_clip_area, &la)) w->dirty_lane = true;
            if (!layer || lv_area_intersect(&x, &layer->_clip_area, &ra)) w->dirty_rail = true;
        }
        break;
    case LV_EVENT_PRESSED:    wf_pressed(w); break;
    case LV_EVENT_PRESSING:   wf_pressing(w); break;
    case LV_EVENT_RELEASED:   wf_released(w, false); break;
    case LV_EVENT_PRESS_LOST: wf_released(w, true); break;
    case LV_EVENT_DELETE:     wf_released(w, true); break;   /* a grab in progress still gets its GRAB_END */
    default:
        break;
    }
}

/* ---- API --------------------------------------------------------------- */
void synthui_waveform_set_tables(lv_obj_t *obj, const synthui_wave_table_t *tables, uint8_t n,
                                 uint8_t channels, uint64_t total_frames, uint32_t sample_rate)
{
    wf_t *w = (wf_t *)obj;
    wf_released(w, true);                                /* a new track ends a grab on the old one */
    w->tables = (tables && n) ? tables : NULL;
    w->ntables = w->tables ? (n > MAX_TABLES ? MAX_TABLES : n) : 0;
    w->channels = channels == 1 ? 1 : 2;
    w->total = w->tables ? total_frames : 0;
    w->rate = sample_rate;
    if (w->valid > w->total) w->valid = w->total;
    w->strip_ok = w->rail_ok = false;
    touch(w);
}
void synthui_waveform_clear(lv_obj_t *obj)
{
    wf_t *w = (wf_t *)obj;
    wf_released(w, true);                                /* a grab in progress still gets its GRAB_END */
    w->tables = NULL; w->ntables = 0; w->total = w->valid = 0;
    w->beats = NULL; w->nbeats = 0; w->cue = -1; w->pos = 0.0; w->scroll = 0.0;
    w->press = PRESS_NONE;
    w->strip_ok = w->rail_ok = false;
    touch(w);
}
void synthui_waveform_set_valid_frames(lv_obj_t *obj, uint64_t frames)
{
    wf_t *w = (wf_t *)obj;
    if (frames > w->total) frames = w->total;
    if (frames == w->valid) return;
    w->valid = frames;
    touch(w);
}
void synthui_waveform_set_position(lv_obj_t *obj, double frame)
{
    wf_t *w = (wf_t *)obj;
    if (!(frame > 0.0)) frame = 0.0;
    if (w->total && frame > (double)(w->total - 1)) frame = (double)(w->total - 1);
    if (frame == w->pos) return;
    w->pos = frame;
    touch_view(w);
}
void synthui_waveform_set_zoom(lv_obj_t *obj, double frames_per_px)
{
    wf_t *w = (wf_t *)obj;
    const double z = clamp_zoom(w, frames_per_px);
    if (z == w->zoom) return;
    if (!w->tracking) {                                  /* keep the view's centre where it is */
        const double centre = w->scroll + (double)(w->sw / 2) * w->zoom;
        w->scroll = centre - (double)(w->sw / 2) * z;
    }
    w->zoom = z;
    touch(w);
}
double synthui_waveform_get_zoom(const lv_obj_t *obj) { return ((const wf_t *)obj)->zoom; }
void synthui_waveform_zoom_step(lv_obj_t *obj, int dir)
{
    const wf_t *w = (const wf_t *)obj;
    synthui_waveform_set_zoom(obj, dir > 0 ? w->zoom * 0.5 : w->zoom * 2.0);
}
void synthui_waveform_zoom_fit(lv_obj_t *obj) { synthui_waveform_set_zoom(obj, 1.0e18); }
void synthui_waveform_set_tracking(lv_obj_t *obj, bool on)
{
    wf_t *w = (wf_t *)obj;
    if (on == w->tracking) return;
    if (!on) w->scroll = w->pos - (double)(w->sw / 2) * w->zoom;   /* the free view starts where tracking left it */
    w->tracking = on;
    touch(w);
}
bool synthui_waveform_get_tracking(const lv_obj_t *obj) { return ((const wf_t *)obj)->tracking; }
void synthui_waveform_set_cue(lv_obj_t *obj, int64_t frame)
{
    wf_t *w = (wf_t *)obj;
    if (frame < 0) frame = -1;
    if (frame == w->cue) return;
    w->cue = frame;
    touch(w);
}
void synthui_waveform_set_beats(lv_obj_t *obj, const synthui_wave_beat_t *beats, uint32_t n)
{
    wf_t *w = (wf_t *)obj;
    w->beats = (beats && n) ? beats : NULL;
    w->nbeats = w->beats ? n : 0;
    w->strip_ok = false;
    touch(w);
}
double synthui_waveform_get_view_start(const lv_obj_t *obj) { return view_start((const wf_t *)obj); }
uint32_t synthui_waveform_grid_columns(const lv_obj_t *obj)
{
    const wf_t *w = (const wf_t *)obj;
    if (!w->strip_ok || w->total == 0) return 0;
    const uint32_t stride = beat_stride(w, w->strip_zoom);
    uint32_t n = 0;
    for (int64_t g = w->g0; g < w->g0 + w->sw; g++) {
        const double f0 = (double)g * w->strip_zoom, f1 = f0 + w->strip_zoom;
        if (f1 > 0.0 && f0 < (double)w->total && beat_in(w, f0 < 0.0 ? 0.0 : f0, f1, stride)) n++;
    }
    return n;
}
