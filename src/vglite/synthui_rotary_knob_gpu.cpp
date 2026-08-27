/* synthui_rotary_knob_gpu.cpp - see synthui_rotary_knob_gpu.h.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT
 *
 * The bench's proven ordering (rotary_knob_bench, silicon 2026-08-27):
 * LVGL renders every damaged area in software (the widget paints its WELL
 * there), then LV_EVENT_RENDER_READY -- sent from refr_invalid_areas, which
 * structurally cannot fire on an empty refresh -- composites each pending
 * rotor straight onto the framebuffer and retires with ONE vg_lite_finish.
 * Paths are built once in centred viewBox units x16 (S32); per frame only
 * translate(center)*rotate(angle)*scale(S/16) changes. No blits anywhere, so
 * the GC355's 64-byte source-stride rule does not apply to this TU.
 * No D-cache maintenance, deliberately: the imxrt1176 core never enables the
 * D-cache (lvgl_mipi_panel.cpp's invariant). An rt1062 port makes this site
 * and that flush ONE change, not two. */
#include "synthui_rotary_knob_gpu.h"
#include "../synthui_rotary_knob_private.h"
#include <math.h>
#include <string.h>

extern "C" {
#include "vg_lite.h"
}

#define RK_DEG (3.14159265358979f / 180.0f)
#define RK_FIX 16.0f

static vg_lite_buffer_t s_target;
static vg_lite_path_t   s_paths[3];      /* body, inner, index wedge */
static bool             s_begun = false;
static uint32_t         s_err = 0;
#define GPU_TRY(call) do { if ((call) != VG_LITE_SUCCESS) s_err++; } while (0)

/* ---- notch path build (bench rk_geometry, reduced to the one variant) ---- */
#define RK_ARENA_WORDS 256
static int32_t s_arena[RK_ARENA_WORDS];
static size_t  s_used;
static bool    s_overflow;

static void emit(int32_t w)
{
    if (s_used < RK_ARENA_WORDS) s_arena[s_used++] = w;
    else s_overflow = true;
}
static int32_t fx(float f) { return (int32_t)lroundf(f * RK_FIX); }
/* centred polar, viewBox units (no scale -- the matrix scales) */
static void cpol(float r, float deg, float *x, float *y)
{
    *x = r * sinf(deg * RK_DEG);
    *y = -r * cosf(deg * RK_DEG);
}
/* cubics for the arc r, a1 -> a2; current point already at (r, a1).
 * k = (4/3)tan(step/4); a negative span flips sign via tan, so the ring's
 * reversed inner arc needs no special case. */
static void emit_arc(float r, float a1, float a2)
{
    const float span = a2 - a1;
    int nseg = (int)ceilf(fabsf(span) / 90.0f);
    if (nseg < 1) nseg = 1;
    const float step = span / (float)nseg;
    const float d = (4.0f / 3.0f) * tanf(step * RK_DEG / 4.0f) * r;
    for (int i = 0; i < nseg; i++) {
        const float b1 = a1 + (float)i * step, b2 = b1 + step;
        float x1, y1, x2, y2;
        cpol(r, b1, &x1, &y1);
        cpol(r, b2, &x2, &y2);
        /* unit tangent of p(t)=(r sin t, -r cos t) is (cos t, sin t) */
        emit(VLC_OP_CUBIC);
        emit(fx(x1 + d * cosf(b1 * RK_DEG))); emit(fx(y1 + d * sinf(b1 * RK_DEG)));
        emit(fx(x2 - d * cosf(b2 * RK_DEG))); emit(fx(y2 - d * sinf(b2 * RK_DEG)));
        emit(fx(x2)); emit(fx(y2));
    }
}
static void emit_circle(float r)
{
    float x, y;
    cpol(r, 0.0f, &x, &y);
    emit(VLC_OP_MOVE); emit(fx(x)); emit(fx(y));
    emit_arc(r, 0.0f, 360.0f);
    emit(VLC_OP_CLOSE);
}
static void emit_ring(float r0, float r1, float a1, float a2)
{
    float x, y;
    cpol(r1, a1, &x, &y);
    emit(VLC_OP_MOVE); emit(fx(x)); emit(fx(y));
    emit_arc(r1, a1, a2);
    cpol(r0, a2, &x, &y);
    emit(VLC_OP_LINE); emit(fx(x)); emit(fx(y));
    emit_arc(r0, a2, a1);               /* reversed inner edge closes it */
    emit(VLC_OP_CLOSE);
}
static void finish_path(vg_lite_path_t *p, size_t start)
{
    emit(VLC_OP_END);
    memset(p, 0, sizeof(*p));
    vg_lite_init_path(p, VG_LITE_S32, VG_LITE_HIGH,
                      (uint32_t)((s_used - start) * sizeof(int32_t)),
                      &s_arena[start],
                      -41.0f * RK_FIX, -41.0f * RK_FIX,
                      41.0f * RK_FIX, 41.0f * RK_FIX);
}
static bool build_paths(void)
{
    s_used = 0; s_overflow = false;
    size_t start;
    start = s_used; emit_circle(36.0f);                   finish_path(&s_paths[0], start);
    start = s_used; emit_circle(27.0f);                   finish_path(&s_paths[1], start);
    start = s_used; emit_ring(16.0f, 36.0f, -8.0f, 8.0f); finish_path(&s_paths[2], start);
    /* a truncated path set is a WRONG picture that still draws (bench rule) */
    return !s_overflow;
}

/* vg_lite_color_t is ABGR -- red in the LOW byte (vglite_probe, measured). */
static uint32_t abgr(uint32_t hex)
{
    return 0xFF000000u | ((hex & 0xFFu) << 16) | (hex & 0xFF00u)
           | ((hex >> 16) & 0xFFu);
}

static void render_ready_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    bool drew = false;
    for (synthui_rotary_knob_t *k = synthui_rotary_knob_list; k; k = k->next) {
        if (!k->gpu_pending) continue;
        k->gpu_pending = false;
        lv_area_t coords; lv_obj_get_coords(&k->obj, &coords);
        const float W = (float)lv_area_get_width(&coords);
        const float H = (float)lv_area_get_height(&coords);
        const float S = (W < H ? W : H) / 100.0f;
        synthui_rotary_palette_t pal;
        synthui_rotary_knob_palette(k, &pal);
        const uint32_t col[3] = { abgr(pal.body), abgr(pal.inner),
                                  abgr(pal.index) };
        vg_lite_matrix_t m; vg_lite_identity(&m);
        vg_lite_translate((float)coords.x1 + W * 0.5f,
                          (float)coords.y1 + H * 0.5f, &m);
        vg_lite_rotate(k->angle, &m);
        vg_lite_scale(S / RK_FIX, S / RK_FIX, &m);
        for (int p = 0; p < 3; p++)
            GPU_TRY(vg_lite_draw(&s_target, &s_paths[p], VG_LITE_FILL_NON_ZERO,
                                 &m, VG_LITE_BLEND_SRC_OVER, col[p]));
        drew = true;
    }
    /* Retire before anyone (checksums, scanout readers) touches the
     * framebuffer -- reading earlier races the hardware (vglite_probe). */
    if (drew) GPU_TRY(vg_lite_finish());
}

bool synthui_rotary_gpu_begin(void *framebuffer, int32_t w, int32_t h,
                              int32_t stride_bytes)
{
    if (s_begun) return true;
    lv_display_t *disp = lv_display_get_default();
    if (disp == NULL || framebuffer == NULL) return false;
    if (!build_paths()) return false;
    memset(&s_target, 0, sizeof(s_target));
    s_target.width   = w;
    s_target.height  = h;
    s_target.stride  = stride_bytes;
    s_target.tiled   = VG_LITE_LINEAR;
    s_target.format  = VG_LITE_BGRA8888;   /* = panel XRGB8888 memory order */
    s_target.memory  = framebuffer;
    s_target.address = (uint32_t)(uintptr_t)framebuffer;
    /* REGISTER the target with the driver or every draw "succeeds" and
     * changes nothing (vglite_probe, measured on silicon). */
    if (vg_lite_map(&s_target, VG_LITE_MAP_USER_MEMORY, 0) != VG_LITE_SUCCESS) {
        s_err++;
        return false;
    }
    lv_display_add_event_cb(disp, render_ready_cb, LV_EVENT_RENDER_READY, NULL);
    synthui_rotary_gpu_enabled = true;
    s_begun = true;
    return true;
}

uint32_t synthui_rotary_gpu_errors(void) { return s_err; }
