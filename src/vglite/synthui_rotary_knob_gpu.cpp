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
static vg_lite_buffer_t *s_cur_target = &s_target;   /* pass-scoped target */
static vg_lite_path_t   s_paths[3];      /* body, inner, index wedge */
static bool             s_begun = false;
static uint32_t         s_err = 0;
#define GPU_TRY(call) do { if ((call) != VG_LITE_SUCCESS) s_err++; } while (0)

/* ---- notch path build (bench rk_geometry, reduced to the one variant) ----
 * The arena holds TWO regions: [0..s_frame_base) the rotor paths, built once
 * at begin; [s_frame_base..) per-frame WELL paths (geometry varies per
 * instance: mode, min/max, focus width), bump-allocated per pending knob and
 * reset only AFTER vg_lite_finish -- safe whether the driver inlines path
 * data into the command buffer or references it until submit. Sized for
 * 16 knobs x (disc + full-annulus border or track+caps) with slack. */
#define RK_ARENA_WORDS 4096
static int32_t s_arena[RK_ARENA_WORDS];
static size_t  s_used;
static size_t  s_frame_base;
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
/* Arc segment centred at (cx0, cy0) -- same tangent construction as
 * emit_arc, translated. Translation commutes with Bezier control points, so
 * the offset applies to every emitted coordinate. Used for the track's
 * rounded end caps, which are 180-degree arcs around off-origin centres. */
static void emit_arc_at(float cx0, float cy0, float r, float a1, float a2)
{
    const float span = a2 - a1;
    int nseg = (int)ceilf(fabsf(span) / 90.0f);
    if (nseg < 1) nseg = 1;
    const float step = span / (float)nseg;
    const float d = (4.0f / 3.0f) * tanf(step * RK_DEG / 4.0f) * r;
    for (int i = 0; i < nseg; i++) {
        const float b1 = a1 + (float)i * step, b2 = b1 + step;
        float x1, y1, x2, y2;
        cpol(r, b1, &x1, &y1); x1 += cx0; y1 += cy0;
        cpol(r, b2, &x2, &y2); x2 += cx0; y2 += cy0;
        emit(VLC_OP_CUBIC);
        emit(fx(x1 + d * cosf(b1 * RK_DEG))); emit(fx(y1 + d * sinf(b1 * RK_DEG)));
        emit(fx(x2 - d * cosf(b2 * RK_DEG))); emit(fx(y2 - d * sinf(b2 * RK_DEG)));
        emit(fx(x2)); emit(fx(y2));
    }
}
/* The bounded track as ONE SIMPLE CONTOUR (winding 1 everywhere): outer arc
 * min->max, 180-degree cap arc around P(43,max), inner arc max->min
 * reversed, 180-degree cap arc around P(43,min), close. The first version
 * unioned a ring with two overlapping cap circles in one path -- winding-2
 * regions -- and that was the ONLY geometry in the system whose rendering
 * varied per boot (bounded screens only; endless/rotor bit-stable), so
 * multi-subpath overlap is treated as hardware-hostile here and avoided. */
static void emit_track(float min_deg, float max_deg)
{
    float x, y;
    cpol(44.5f, min_deg, &x, &y);
    emit(VLC_OP_MOVE); emit(fx(x)); emit(fx(y));
    emit_arc(44.5f, min_deg, max_deg);            /* outer edge */
    cpol(43.0f, max_deg, &x, &y);
    emit_arc_at(x, y, 1.5f, max_deg, max_deg + 180.0f);   /* end cap */
    emit_arc(41.5f, max_deg, min_deg);            /* inner edge, reversed */
    cpol(43.0f, min_deg, &x, &y);
    emit_arc_at(x, y, 1.5f, min_deg + 180.0f, min_deg + 360.0f); /* start cap */
    emit(VLC_OP_CLOSE);
}
static void finish_path_b(vg_lite_path_t *p, size_t start, float bound)
{
    emit(VLC_OP_END);
    memset(p, 0, sizeof(*p));
    vg_lite_init_path(p, VG_LITE_S32, VG_LITE_HIGH,
                      (uint32_t)((s_used - start) * sizeof(int32_t)),
                      &s_arena[start],
                      -bound * RK_FIX, -bound * RK_FIX,
                      bound * RK_FIX, bound * RK_FIX);
}
static void finish_path(vg_lite_path_t *p, size_t start)
{
    finish_path_b(p, start, 41.0f);
}
static bool build_paths(void)
{
    s_used = 0; s_overflow = false;
    size_t start;
    start = s_used; emit_circle(36.0f);                   finish_path(&s_paths[0], start);
    start = s_used; emit_circle(27.0f);                   finish_path(&s_paths[1], start);
    start = s_used; emit_ring(16.0f, 36.0f, -8.0f, 8.0f); finish_path(&s_paths[2], start);
    s_frame_base = s_used;               /* well paths bump-allocate above */
    /* a truncated path set is a WRONG picture that still draws (bench rule) */
    return !s_overflow;
}

static uint32_t abgr(uint32_t hex);

/* ---- per-frame WELL paths (gpu-well spec section 3) ----------------------
 * Angle-independent by construction (drawn with m_fixed): disc r39 in the
 * well colour, then EITHER the endless border ring r(39-bw)..39 (bw 3 on
 * focus, else 1.6 -- LVGL's border-inside-radius convention) OR the bounded
 * track: ring r41.5..44.5 min->max unioned with two r1.5 cap discs at
 * P(43, min/max), one path, one colour (well_stroke -- which the palette
 * already turns into the theme index colour on focus, matching sw). */
#define RK_WELL_MAX_PATHS 2
static int build_well_paths(const synthui_rotary_knob_t *k,
                            const synthui_rotary_palette_t *pal,
                            vg_lite_path_t *paths, uint32_t *cols)
{
    int n = 0;
    size_t start;
    /* ★ ANNULUS r35..39, NOT the r39 disc, and the reason is on-glass, not
     * aesthetic (found on 60 fps video, 2026-08-28): the compositor draws
     * into the LIVE scanout buffer, so between "well drawn" and "body drawn"
     * a damaged wedge box is ENTIRELY light well colour -- and when the
     * LCDIF scanline crosses the box in that window the glass shows a white
     * square for one 60 Hz scan, beating against the ~30 fps refresh about
     * once a second. The body covers r<36 opaquely, so a disc under it buys
     * nothing; with the annulus every intermediate state is nearly the
     * final image (dark stays dark, only the thin rim and the wedge itself
     * change) and the final pixels are identical -- the body's AA rim still
     * blends over well-coloured underlay at r35..36. Structural fix for the
     * residual sub-frame wedge shimmer would be double-buffered or
     * vsync-fenced compositing; deferred, documented in the gpu-well spec. */
    start = s_used; emit_ring(35.0f, 39.0f, 0.0f, 360.0f);
    finish_path(&paths[n], start); cols[n++] = abgr(pal->well);
    if (k->mode == SYNTHUI_ROTARY_MODE_BOUNDED) {
        start = s_used;
        emit_track(k->min_deg, k->max_deg);
        finish_path_b(&paths[n], start, 47.0f);
        cols[n++] = abgr(pal->well_stroke);
    } else {
        const lv_state_t st = lv_obj_get_state((const lv_obj_t *)&k->obj);
        const bool focus = (st & LV_STATE_FOCUSED) &&
                           !(st & LV_STATE_DISABLED);
        const float bw = focus ? 3.0f : 1.6f;
        start = s_used;
        emit_ring(39.0f - bw, 39.0f, 0.0f, 360.0f);
        finish_path(&paths[n], start); cols[n++] = abgr(pal->well_stroke);
    }
    /* a truncated well is a wrong picture that still draws: count it where
     * the transcripts demand a zero (rk_gpu_err) and draw nothing */
    return s_overflow ? -1 : n;
}

/* vg_lite_color_t is ABGR -- red in the LOW byte (vglite_probe, measured). */
static uint32_t abgr(uint32_t hex)
{
    return 0xFF000000u | ((hex & 0xFFu) << 16) | (hex & 0xFF00u)
           | ((hex >> 16) & 0xFFu);
}

/* ---- one-composite-per-pixel machinery ----------------------------------
 * ★ LVGL only guarantees a damaged pixel is rendered AT LEAST once: two
 * SURVIVING (unjoined) invalid areas may overlap -- wedge boxes of adjacent
 * angles routinely do -- and LVGL's sw renderer paints the overlap twice,
 * which is harmless because sw repainting is idempotent. An SRC_OVER
 * composite of antialiased paths is NOT: the second pass re-blends the AA
 * edge pixels over the first pass's output. Found by the equality guard on
 * its FIRST silicon run (KNOB_DELTA_SEQ != FULL, rk_gpu_err=0) -- QEMU can
 * never execute this path. So each area is composited MINUS everything
 * already composited for this knob: split around the first intersecting
 * done-rect into <=4 disjoint pieces and recurse (pieces cannot intersect
 * done[0..i] again -- done[i]'s overlap with the area IS the removed piece,
 * and earlier rects did not intersect the area at all). */
/* ★ TWO MATRICES, and the split is load-bearing for the equality guard.
 * The body/inner circles are 4-segment Bezier approximations: geometrically
 * rotation-invariant, but the Bezier radius-error pattern ROTATES with the
 * matrix, so a disc drawn at rotate(a) has slightly different edge AA than
 * the same disc at rotate(b). Found on silicon by the delta equality guard:
 * the diff between a delta-sequence render and a fresh render was exactly
 * the two disc-edge circles, full circumference, on both knobs -- rim AA
 * painted by an earlier composite at an earlier angle. Drawing the discs
 * UNROTATED (rotation is a geometric no-op for them) makes their pixels a
 * function of position and size alone, which is what the wedge-delta damage
 * model requires. Only the wedge path takes the rotated matrix. The sw
 * renderer has always had this property (it draws discs as axis-aligned
 * circles and rotates only the wedge arc). */
typedef struct {
    const vg_lite_matrix_t *m_fixed;   /* translate * scale: well + discs */
    const vg_lite_matrix_t *m_rot;     /* translate * rotate * scale: wedge */
    const uint32_t *col;               /* rotor colours (body, inner, wedge) */
    const vg_lite_path_t *wpaths;      /* per-frame well paths (m_fixed) */
    const uint32_t *wcols;
    int wn;
} rk_gpu_draw_ctx_t;

static void composite_minus(const rk_gpu_draw_ctx_t *ctx, lv_area_t area,
                            const lv_area_t *done, int ndone)
{
    for (int i = 0; i < ndone; i++) {
        lv_area_t ix;
        if (!lv_area_intersect(&ix, &area, &done[i])) continue;
        lv_area_t piece;
        if (area.y1 < ix.y1) {   /* strip above the overlap */
            piece.x1 = area.x1; piece.y1 = area.y1;
            piece.x2 = area.x2; piece.y2 = ix.y1 - 1;
            composite_minus(ctx, piece, done + i + 1, ndone - i - 1);
        }
        if (ix.y2 < area.y2) {   /* strip below */
            piece.x1 = area.x1; piece.y1 = ix.y2 + 1;
            piece.x2 = area.x2; piece.y2 = area.y2;
            composite_minus(ctx, piece, done + i + 1, ndone - i - 1);
        }
        if (area.x1 < ix.x1) {   /* left of the overlap, overlap-height */
            piece.x1 = area.x1; piece.y1 = ix.y1;
            piece.x2 = ix.x1 - 1; piece.y2 = ix.y2;
            composite_minus(ctx, piece, done + i + 1, ndone - i - 1);
        }
        if (ix.x2 < area.x2) {   /* right of the overlap */
            piece.x1 = ix.x2 + 1; piece.y1 = ix.y1;
            piece.x2 = area.x2; piece.y2 = ix.y2;
            composite_minus(ctx, piece, done + i + 1, ndone - i - 1);
        }
        return;                  /* the overlap itself is already composited */
    }
    /* lv_area x2/y2 inclusive; the driver's right/bottom exclusive. */
    GPU_TRY(vg_lite_set_scissor(area.x1, area.y1, area.x2 + 1, area.y2 + 1));
    /* the sw painter's order: well first, then the rotor over it */
    for (int p = 0; p < ctx->wn; p++)
        GPU_TRY(vg_lite_draw(s_cur_target, (vg_lite_path_t *)&ctx->wpaths[p],
                             VG_LITE_FILL_NON_ZERO,
                             (vg_lite_matrix_t *)ctx->m_fixed,
                             VG_LITE_BLEND_SRC_OVER, ctx->wcols[p]));
    for (int p = 0; p < 3; p++)
        GPU_TRY(vg_lite_draw(s_cur_target, &s_paths[p], VG_LITE_FILL_NON_ZERO,
                             (vg_lite_matrix_t *)(p == 2 ? ctx->m_rot
                                                         : ctx->m_fixed),
                             VG_LITE_BLEND_SRC_OVER, ctx->col[p]));
}

static void compose_pass(void);

static void render_ready_cb(lv_event_t *e)
{
    /* ★ SCISSOR TO THE DISPLAY'S ACTUAL RENDERED AREAS, per knob -- not to
     * anything the widget recorded. The sw pass has just repainted
     * ground+well inside exactly disp->inv_areas[] (however LVGL chose to
     * JOIN what was invalidated -- a join can be a strict superset of any
     * rect the widget knows about, and an unscissored full-rotor redraw
     * would re-blend the body rim's AA over its own previous blend and
     * drift toward pure body colour). Compositing the rotor scissored to
     * each rendered area that touches the knob recreates a fresh render
     * there and touches nothing else: exact for wedge-delta damage, full
     * invalidations, and partial overlaps from OTHER objects alike (the
     * Phase-2 "double-composite AA" caveat dies here). The areas are still
     * populated at RENDER_READY -- it is sent from refr_invalid_areas,
     * before refr_finish resets inv_p -- and joined slots are flagged, not
     * rendered, so they are skipped.
     * Spec: 2026-08-27-rotary-knob-delta-damage-design.md section 4. */
    LV_UNUSED(e);
    s_cur_target = &s_target;
    compose_pass();
}

/* One composite pass over every pending instance, into *s_cur_target. */
static void compose_pass(void)
{
    lv_display_t *disp = lv_display_get_default();
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
        vg_lite_matrix_t m_fixed; vg_lite_identity(&m_fixed);
        vg_lite_translate((float)coords.x1 + W * 0.5f,
                          (float)coords.y1 + H * 0.5f, &m_fixed);
        vg_lite_matrix_t m_rot = m_fixed;
        vg_lite_rotate(k->angle, &m_rot);
        vg_lite_scale(S / RK_FIX, S / RK_FIX, &m_fixed);
        vg_lite_scale(S / RK_FIX, S / RK_FIX, &m_rot);
        vg_lite_path_t wpaths[RK_WELL_MAX_PATHS];
        uint32_t wcols[RK_WELL_MAX_PATHS];
        int wn = build_well_paths(k, &pal, wpaths, wcols);
        if (wn < 0) { s_err++; wn = 0; }   /* truncated well: error, not draw */
        const rk_gpu_draw_ctx_t ctx = { &m_fixed, &m_rot, col,
                                        wpaths, wcols, wn };
        lv_area_t done[LV_INV_BUF_SIZE];
        int ndone = 0;
        for (uint32_t i = 0; i < disp->inv_p; i++) {
            if (disp->inv_area_joined[i]) continue;    /* merged, not drawn */
            lv_area_t clip;
            if (!lv_area_intersect(&clip, &disp->inv_areas[i], &coords))
                continue;                              /* not this knob */
            /* one composite per pixel: subtract what this knob already got */
            composite_minus(&ctx, clip, done, ndone);
            done[ndone++] = clip;
            drew = true;
        }
    }
    if (drew) {
        GPU_TRY(vg_lite_set_scissor(-1, -1, -1, -1));  /* disable */
        /* Retire before anyone (checksums, scanout readers) touches the
         * framebuffer -- reading earlier races the hardware (vglite_probe). */
        GPU_TRY(vg_lite_finish());
    }
    /* Reclaim the per-frame well paths -- only AFTER finish, in case the
     * driver references (rather than copied) the path data until submit. */
    s_used = s_frame_base;
    s_overflow = false;
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

/* ---- deferred (double-buffered) mode: see the header ---- */
static int32_t s_def_w, s_def_h, s_def_stride;
static vg_lite_buffer_t s_def_targets[2];
static void *s_def_ptrs[2];
static int s_def_n = 0;

bool synthui_rotary_gpu_begin_deferred(int32_t w, int32_t h,
                                       int32_t stride_bytes)
{
    if (s_begun) return true;
    if (!build_paths()) return false;
    s_def_w = w; s_def_h = h; s_def_stride = stride_bytes;
    s_def_n = 0;
    synthui_rotary_gpu_enabled = true;
    s_begun = true;
    return true;
}

void synthui_rotary_gpu_compose_into(uint8_t *framebuffer)
{
    if (!synthui_rotary_gpu_enabled || framebuffer == NULL) return;
    /* lazy wrap+map: a flip display alternates between exactly two buffers */
    int i;
    for (i = 0; i < s_def_n; i++)
        if (s_def_ptrs[i] == framebuffer) break;
    if (i == s_def_n) {
        if (s_def_n >= 2) { s_err++; return; }   /* a third buffer is a bug */
        vg_lite_buffer_t *t = &s_def_targets[s_def_n];
        memset(t, 0, sizeof(*t));
        t->width   = s_def_w;
        t->height  = s_def_h;
        t->stride  = s_def_stride;
        t->tiled   = VG_LITE_LINEAR;
        t->format  = VG_LITE_BGRA8888;   /* = panel XRGB8888 memory order */
        t->memory  = framebuffer;
        t->address = (uint32_t)(uintptr_t)framebuffer;
        if (vg_lite_map(t, VG_LITE_MAP_USER_MEMORY, 0) != VG_LITE_SUCCESS) {
            s_err++;
            return;
        }
        s_def_ptrs[s_def_n++] = framebuffer;
    }
    s_cur_target = &s_def_targets[i];
    compose_pass();
    s_cur_target = &s_target;
}
