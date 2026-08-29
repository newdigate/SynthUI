/* synthui_fader_gpu.cpp - see synthui_fader_gpu.h.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT
 *
 * Structure is synthui_rotary_knob_gpu.cpp's, simplified: every shape is
 * emitted per frame into the bump arena in viewBox units x16 (S32) with the
 * fader's CURRENT cap_y baked in, and ONE matrix per fader
 * (translate(coords) * scale(u/16)) maps them to the screen -- a fader has
 * no rotation, so the rotary's two-matrix AA lesson does not arise. Ticks
 * are batched into two multi-rect paths (bright / dim); adjacent ticks
 * whose rects would overlap (spacing < tick_w) are COALESCED into one run
 * rect rather than emitted as separate overlapping rects -- the union of
 * overlapping same-x axis-aligned rects is itself a rect, so this is
 * visually identical and is what ENFORCES winding-1 everywhere, rather than
 * assuming non-overlap happens to hold. Overlapping subpaths were the
 * rotary's one source of per-boot nondeterminism (its emit_track comment);
 * unlike the rotary's fixed geometry, tick spacing here varies with vh and
 * travel and coincides at travel=0, so coalescing -- not spacing -- is what
 * keeps winding at 1.
 * The cap gradient uses cached vg_lite_ext_linear_gradient_t ramps (one per
 * band per palette state, built lazily) with a per-frame gradient matrix;
 * ramps are never rebuilt per frame (the NEW-12 per-frame-construction
 * lesson). The EXT API (vg_lite_draw_linear_grad) is used deliberately, NOT
 * the legacy vg_lite_draw_grad -- that call is the GC255 image-ramp API;
 * neither NXP's own vglite_layer example nor LVGL's vg_lite backend ever
 * exercises it on a GC355 (both gate on g_chip_id/gcFEATURE_BIT_VG_LINEAR_
 * GRADIENT_EXT and route here instead), which is what silently produced
 * solid-black cap bands and a per-boot-varying checksum on real silicon
 * while every vg_lite_* call kept returning VG_LITE_SUCCESS: vg_lite_set_
 * grad cannot report a rejected ramp, so the legacy path substitutes an
 * implicit opaque-black->white ramp with no error to see. A compile-time
 * fallback (-DFD_GPU_GRAD_STRIPS=N, see near s_grads below) draws each band
 * as N solid interpolated strips through the ordinary emit_rect/vg_lite_draw
 * path instead, for if the EXT path ever needs its own escape hatch. No
 * blits, so the 64-byte source-stride rule does not apply. No D-cache
 * maintenance, deliberately: the imxrt1176 core never enables the D-cache. */
#include "synthui_fader_gpu.h"
#include "../synthui_fader_private.h"
#include <math.h>
#include <string.h>

extern "C" {
#include "vg_lite.h"
}

#define FD_FIX 16.0f

static vg_lite_buffer_t *s_cur_target = NULL;
static bool     s_begun = false;
static uint32_t s_err = 0;
#define GPU_TRY(call) do { if ((call) != VG_LITE_SUCCESS) s_err++; } while (0)

/* ---- per-draw-call path arena --------------------------------------------
 * ★ Sized for ONE draw_fader_clipped() call, NOT one frame. The first
 * version of this file sized the arena for "16 faders" (one call each), but
 * composite_minus() calls draw_fader_clipped() once per DISJOINT CLIP PIECE
 * per fader -- two overlapping inv_areas on one fader can split into up to
 * 5 pieces, so 16 faders can mean up to ~80 calls/frame, ~37K words against
 * a 16384-word arena. A press + a drag landing in the same LVGL frame is a
 * realistic trigger. Truncating mid-path drops the VLC_OP_END, and
 * vg_lite_init_path()'s CLOSE->END fixup can then rewrite a coordinate word
 * -- corrupting the arena rather than merely mis-sizing it.
 * ★ This is fixed, not just detected, by a driver fact: vg_lite_draw() ->
 * push_data() (vg_lite_path.c ~3291, taken because vg_lite_init_path() never
 * sets VLM_PATH_GET_UPLOAD_BIT) memcpys the path words into the command
 * buffer BEFORE returning. So the arena is safe to reuse the instant the
 * PRECEDING vg_lite_draw()/vg_lite_draw_linear_grad() call returns --
 * draw_fader_clipped() resets s_used to 0 on entry (see there), and the true
 * peak is ONE fader's shapes, worst case ~722 words at 33 ticks: panel/center/one
 * grad band/groove/one gloss rect ~14 words each, rod/shadow/base ~45 words
 * each, border ring ~58 words, ticks up to ~431 words (33 emit_rects, worst
 * case not coalesced) -- comfortably inside 2048 with margin for the
 * per-band gradient rect's own finish_path call. Overflow is still COUNTED
 * and REFUSED (finish_path() returns false; a truncated path set is a wrong
 * picture that still draws -- worse here, since it is exactly what hangs
 * the Vivante front end while every call still reports SUCCESS). The
 * frame-level reset in compose_pass() (after vg_lite_finish) is kept as
 * belt-and-braces, not as the mechanism that makes this safe. */
#define FD_ARENA_WORDS 2048
static int32_t s_arena[FD_ARENA_WORDS];
static size_t  s_used;
static bool    s_overflow;

static void emit(int32_t w)
{
    if (s_used < FD_ARENA_WORDS) s_arena[s_used++] = w;
    else s_overflow = true;
}
static int32_t fx(float f) { return (int32_t)lroundf(f * FD_FIX); }

/* axis-aligned rect contour, unit coords */
static void emit_rect(float x, float y, float w, float h)
{
    emit(VLC_OP_MOVE); emit(fx(x));     emit(fx(y));
    emit(VLC_OP_LINE); emit(fx(x + w)); emit(fx(y));
    emit(VLC_OP_LINE); emit(fx(x + w)); emit(fx(y + h));
    emit(VLC_OP_LINE); emit(fx(x));     emit(fx(y + h));
    emit(VLC_OP_CLOSE);
}
/* rounded rect: four quarter-circle corners as single cubics (r is small --
 * <= 2 units -- so one cubic per 90 degrees is visually exact). k = c*r,
 * c = 0.5523 (4/3*(sqrt(2)-1)). Clockwise from the top-left arc end. */
static void emit_round_rect(float x, float y, float w, float h, float r)
{
    const float k = 0.5523f * r;
    emit(VLC_OP_MOVE);  emit(fx(x + r));         emit(fx(y));
    emit(VLC_OP_LINE);  emit(fx(x + w - r));     emit(fx(y));
    emit(VLC_OP_CUBIC); emit(fx(x + w - r + k)); emit(fx(y));
                        emit(fx(x + w));         emit(fx(y + r - k));
                        emit(fx(x + w));         emit(fx(y + r));
    emit(VLC_OP_LINE);  emit(fx(x + w));         emit(fx(y + h - r));
    emit(VLC_OP_CUBIC); emit(fx(x + w));         emit(fx(y + h - r + k));
                        emit(fx(x + w - r + k)); emit(fx(y + h));
                        emit(fx(x + w - r));     emit(fx(y + h));
    emit(VLC_OP_LINE);  emit(fx(x + r));         emit(fx(y + h));
    emit(VLC_OP_CUBIC); emit(fx(x + r - k));     emit(fx(y + h));
                        emit(fx(x));             emit(fx(y + h - r + k));
                        emit(fx(x));             emit(fx(y + h - r));
    emit(VLC_OP_LINE);  emit(fx(x));             emit(fx(y + r));
    emit(VLC_OP_CUBIC); emit(fx(x));             emit(fx(y + r - k));
                        emit(fx(x + r - k));     emit(fx(y));
                        emit(fx(x + r));         emit(fx(y));
    emit(VLC_OP_CLOSE);
}
/* border ring: outer rounded contour + reversed inner sharp contour in one
 * path -- non-zero winding makes the donut, exactly like the rotary's
 * emit_ring. The two contours never overlap, so winding is 1 everywhere. */
static void emit_border_ring(float x, float y, float w, float h, float r,
                             float bw)
{
    emit_round_rect(x, y, w, h, r);
    /* inner, counter-clockwise (reversed) */
    const float ix = x + bw, iy = y + bw, iw = w - 2.0f * bw,
                ih = h - 2.0f * bw;
    emit(VLC_OP_MOVE); emit(fx(ix));      emit(fx(iy));
    emit(VLC_OP_LINE); emit(fx(ix));      emit(fx(iy + ih));
    emit(VLC_OP_LINE); emit(fx(ix + iw)); emit(fx(iy + ih));
    emit(VLC_OP_LINE); emit(fx(ix + iw)); emit(fx(iy));
    emit(VLC_OP_CLOSE);
}

/* Returns false when the path was truncated by the arena (see FD_ARENA_WORDS
 * above); the caller must not draw in that case. A truncated path has no
 * VLC_OP_END and is a WRONG PICTURE THAT STILL DRAWS -- worse here than
 * elsewhere, since unterminated path data is exactly what hangs the Vivante
 * front end while every vg_lite_* call keeps returning VG_LITE_SUCCESS (the
 * Phase-1 finding). Bounds are padded ~1 viewBox unit on every side: the
 * driver derives its tessellation window from this box (rounded, not exact
 * -- vg_lite_path.c's ts_is_fullscreen logic), and an exact bound can land a
 * half-pixel short at a tile boundary. */
static bool finish_path(vg_lite_path_t *p, size_t start, float x0, float y0,
                        float x1, float y1)
{
    emit(VLC_OP_END);
    /* s_overflow is PER-CALL (reset at the top of draw_fader_clipped, along
     * with s_used) -- a frame-sticky flag would leave a split-brain state
     * (s_used==0 says "arena free" while every later shape in the frame is
     * silently refused) and, for this widget, that means stale pixels on
     * every LATER fader that frame even though each had the whole arena
     * free. Per-call confines the damage to the one piece that genuinely
     * didn't fit. Counted here, not just detected: every refusal is a real
     * error. */
    if (s_overflow) { s_err++; return false; }
    memset(p, 0, sizeof(*p));
    vg_lite_init_path(p, VG_LITE_S32, VG_LITE_HIGH,
                      (uint32_t)((s_used - start) * sizeof(int32_t)),
                      &s_arena[start],
                      (x0 - 1.0f) * FD_FIX, (y0 - 1.0f) * FD_FIX,
                      (x1 + 1.0f) * FD_FIX, (y1 + 1.0f) * FD_FIX);
    return true;
}

/* vg_lite_color_t is ABGR -- red in the LOW byte (vglite_probe, measured);
 * `a` carries the sw path's opa values so the two looks stay close. */
static uint32_t abgr_a(uint32_t hex, uint32_t a)
{
    return (a << 24) | ((hex & 0xFFu) << 16) | (hex & 0xFF00u)
           | ((hex >> 16) & 0xFFu);
}

/* ---- gradient strategy: EXT linear gradient (default) or solid strips ---
 * Default (FD_GPU_GRAD_STRIPS undefined): vg_lite_draw_linear_grad, the
 * gcFEATURE_VG_LINEAR_GRADIENT_EXT API both NXP's own vglite_layer example
 * and LVGL's vg_lite backend route a GC355 through (see the file header).
 * Its ramp is a vg_lite_color_ramp_t array of straight (non-packed) float
 * R/G/B/A components in [0,1] -- unlike the legacy vg_lite_linear_gradient_t
 * image ramp, there is no packed word and therefore no byte-order to get
 * wrong decoding this file's 0xRRGGBB hex constants into it; see ramp_set
 * below.
 * -DFD_GPU_GRAD_STRIPS=N (N >= 2): escape hatch for if the EXT path ever
 * needs its own bench-side workaround -- draws each band as N solid
 * horizontal strips (linearly RGB-interpolated between the band's two
 * colours, strip i at (i+0.5)/N) through the SAME emit_rect + finish_path +
 * vg_lite_draw machinery as every other solid shape in this file, which is
 * proven correct on this hardware (only the gradient calls were ever
 * implicated). Costs N draws per band instead of 1; FD_ARENA_WORDS is
 * asserted (below) to cover it up to N=16 with margin. */

/* vg_lite_color_ramp_t stop/color for the EXT ramp -- straight floats, full
 * opacity (this widget's bands are never translucent). */
#ifndef FD_GPU_GRAD_STRIPS
static void ramp_set(vg_lite_color_ramp_t *r, float stop, uint32_t hex)
{
    r->stop  = stop;
    r->red   = (float)((hex >> 16) & 0xFFu) / 255.0f;
    r->green = (float)((hex >> 8)  & 0xFFu) / 255.0f;
    r->blue  = (float)(hex & 0xFFu) / 255.0f;
    r->alpha = 1.0f;
}

/* ---- cached gradient ramps: one per band per palette state --------------
 * The ramp's line parameters are the CANONICAL unit vector (0,0)->(0,1) --
 * position- and size-independent, so ONE cached object serves every frame
 * and every fader instance for this (band, palette-state); only the grad
 * MATRIX changes per frame (the ramp itself must not be rebuilt per frame --
 * the NEW-12 lesson). draw_fader_clipped() maps the canonical line to the
 * actual band's screen position/height with translate+scale on a COPY of
 * the fader matrix (see the band loop below) -- the EXT-API equivalent of
 * the legacy image-ramp trick of translate+rotate(90)+scale(h/256), minus
 * the rotate (the canonical line already points down) and minus the /256
 * (EXT takes a literal line length via grad_param, not a normalized
 * 256-texel ramp index). Key: band 0 = capTop->capMid, band 1 =
 * capMid->capLow; state index 0 idle / 1 active / 2 disabled. */
static vg_lite_ext_linear_gradient_t s_grads[2][3];
static bool s_grad_ready[2][3];

static vg_lite_ext_linear_gradient_t *grad_get(int band, int stidx,
                                               uint32_t c_top, uint32_t c_bot)
{
    if (!s_grad_ready[band][stidx]) {
        vg_lite_ext_linear_gradient_t *g = &s_grads[band][stidx];
        memset(g, 0, sizeof(*g));
        vg_lite_color_ramp_t ramp[2];
        ramp_set(&ramp[0], 0.0f, c_top);
        ramp_set(&ramp[1], 1.0f, c_bot);
        const vg_lite_linear_gradient_parameter_t line =
            { 0.0f, 0.0f, 0.0f, 1.0f };
        /* pre_mult=1 and PAD spread match LVGL's lv_vg_lite_grad.c
         * linear_ext_grad_create() exactly; PAD is also the closest EXT
         * equivalent of the legacy ramp's implicit tail-clamp (its texels
         * past stop 255 held the last colour). pre_mult is a no-op here
         * either way since every stop above is alpha=1.0. */
        const bool ok =
            vg_lite_set_linear_grad(g, 2, ramp, line,
                                    VG_LITE_GRADIENT_SPREAD_PAD, 1)
                == VG_LITE_SUCCESS
            && vg_lite_update_linear_grad(g) == VG_LITE_SUCCESS;
        /* Only cache as ready when BOTH calls succeeded -- caching a broken
         * ramp on a failed set/update would draw a stale or garbage ramp
         * forever while counting the error exactly once. vg_lite_clear_
         * linear_grad frees grad->image if its handle is set -- without it,
         * the next frame's memset wipes the handle and leaks the backing
         * ramp image: VLC_GRADIENT_BUFFER_WIDTH x 1 = 1024x1 (4 KB, not the
         * 256x1/1 KB this comment used to claim before the ext-API switch --
         * vg_lite.h:101) per band per frame. */
        if (!ok) { s_err++; vg_lite_clear_linear_grad(g); return NULL; }
        s_grad_ready[band][stidx] = true;
    }
    return &s_grads[band][stidx];
}
#else /* FD_GPU_GRAD_STRIPS */
#if FD_GPU_GRAD_STRIPS < 2
#error "FD_GPU_GRAD_STRIPS must be >= 2"
#endif
/* Each strip's fill colour is built with abgr_a() (defined above), the same
 * packer every solid shape in this file uses. */
static uint32_t lerp_rgb(uint32_t c0, uint32_t c1, float t)
{
    const int r0 = (int)((c0 >> 16) & 0xFFu), g0 = (int)((c0 >> 8) & 0xFFu),
              b0 = (int)(c0 & 0xFFu);
    const int r1 = (int)((c1 >> 16) & 0xFFu), g1 = (int)((c1 >> 8) & 0xFFu),
              b1 = (int)(c1 & 0xFFu);
    const uint32_t r = (uint32_t)lroundf((float)r0 + (float)(r1 - r0) * t);
    const uint32_t g = (uint32_t)lroundf((float)g0 + (float)(g1 - g0) * t);
    const uint32_t b = (uint32_t)lroundf((float)b0 + (float)(b1 - b0) * t);
    return (r << 16) | (g << 8) | b;
}

/* Arena arithmetic for the strips fallback -- see FD_ARENA_WORDS above for
 * the 722-word derivation of a full draw_fader_clipped() call, of which the
 * two ext-gradient band rects contribute 28 words (2 rects * 14 words: an
 * emit_rect contour is 13 words -- MOVE+2, three LINEs at 3 words each,
 * CLOSE -- plus finish_path's own VLC_OP_END). Replacing those 2 rects with
 * 2*FD_GPU_GRAD_STRIPS strip rects (same 14 words each: each strip is its
 * own finish_path()/vg_lite_draw() call) gives a worst case of
 * (722 - 28) + 28*FD_GPU_GRAD_STRIPS = 694 + 28*FD_GPU_GRAD_STRIPS words.
 * At N=16: 694 + 448 = 1142, comfortably inside FD_ARENA_WORDS (2048, ~44%
 * margin); N could rise to 48 before 2048 is exceeded. Asserted, not just
 * stated, so a future N this arena can't hold fails to compile instead of
 * silently truncating (finish_path()'s overflow guard still catches it at
 * runtime either way, but a build-time check is cheaper than a bench boot). */
static_assert(694 + 28 * FD_GPU_GRAD_STRIPS <= FD_ARENA_WORDS,
             "FD_GPU_GRAD_STRIPS too large for FD_ARENA_WORDS");
#endif /* FD_GPU_GRAD_STRIPS */

/* ---- one-composite-per-pixel machinery (the rotary's, verbatim shape) ---
 * LVGL only guarantees a damaged pixel is rendered AT LEAST once; two
 * surviving inv areas may overlap, and an SRC_OVER composite of antialiased
 * paths is not idempotent (found by the knob's equality guard on its FIRST
 * silicon run). Each area is composited MINUS everything already composited
 * for this fader. */
typedef struct {
    const synthui_fader_t *f;
    const synthui_fader_geom_t *g;
    const synthui_fader_palette_t *pal;
    const vg_lite_matrix_t *m;      /* unit*16 -> screen */
    float cap_y;                    /* units, this frame */
    int stidx;                      /* palette state index for grad cache */
} fd_gpu_ctx_t;

static void draw_fader_clipped(const fd_gpu_ctx_t *c, const lv_area_t *clip);

static void composite_minus(const fd_gpu_ctx_t *ctx, lv_area_t area,
                            const lv_area_t *done, int ndone)
{
    for (int i = 0; i < ndone; i++) {
        lv_area_t ix;
        if (!lv_area_intersect(&ix, &area, &done[i])) continue;
        lv_area_t piece;
        if (area.y1 < ix.y1) {
            piece.x1 = area.x1; piece.y1 = area.y1;
            piece.x2 = area.x2; piece.y2 = ix.y1 - 1;
            composite_minus(ctx, piece, done + i + 1, ndone - i - 1);
        }
        if (ix.y2 < area.y2) {
            piece.x1 = area.x1; piece.y1 = ix.y2 + 1;
            piece.x2 = area.x2; piece.y2 = area.y2;
            composite_minus(ctx, piece, done + i + 1, ndone - i - 1);
        }
        if (area.x1 < ix.x1) {
            piece.x1 = area.x1; piece.y1 = ix.y1;
            piece.x2 = ix.x1 - 1; piece.y2 = ix.y2;
            composite_minus(ctx, piece, done + i + 1, ndone - i - 1);
        }
        if (ix.x2 < area.x2) {
            piece.x1 = ix.x2 + 1; piece.y1 = ix.y1;
            piece.x2 = area.x2; piece.y2 = ix.y2;
            composite_minus(ctx, piece, done + i + 1, ndone - i - 1);
        }
        return;
    }
    /* lv_area x2/y2 inclusive; the driver's right/bottom exclusive. */
    GPU_TRY(vg_lite_set_scissor(area.x1, area.y1, area.x2 + 1, area.y2 + 1));
    draw_fader_clipped(ctx, &area);
}

/* Draw the fader's FULL content, clipped by the scissor already set.
 * Geometry mirrors the sw path's fd_draw (spec 2026-08-29 base, section 4);
 * pixel parity is NOT required (two golden sets, never reconciled) but the
 * shapes and draw order are the same so the looks stay close. */
static void draw_fader_clipped(const fd_gpu_ctx_t *c, const lv_area_t *clip)
{
    (void)clip;
    /* Reset the per-draw-call arena HERE, not once per frame: vg_lite_draw()
     * copies (memcpy, push_data) every path's words into the command buffer
     * before returning (see FD_ARENA_WORDS above), so nothing from a prior
     * call to this function -- a different clip piece, a different fader --
     * is still needed once that prior call's vg_lite_draw()s have returned.
     * This is what shrinks the real peak from "the whole frame" (up to ~80
     * calls) to "one fader's shapes" (~722 words worst case). s_overflow is
     * reset alongside s_used -- both are PER-CALL state now, not per-frame:
     * a truncation in one piece must not silently refuse every later shape
     * in this call, and must not poison unrelated calls later in the frame
     * (see finish_path's comment). */
    s_used = 0;
    s_overflow = false;
    const synthui_fader_geom_t *g = c->g;
    const synthui_fader_palette_t *pal = c->pal;
    const float ch = g->cap_h, cy = c->cap_y, bw = 1.6f;
    vg_lite_path_t p;
    size_t start;

    /* panel */
    start = s_used; emit_rect(0.0f, 0.0f, 100.0f, g->vh);
    if (finish_path(&p, start, 0.0f, 0.0f, 100.0f, g->vh))
        GPU_TRY(vg_lite_draw(s_cur_target, &p, VG_LITE_FILL_NON_ZERO,
                             (vg_lite_matrix_t *)c->m, VG_LITE_BLEND_SRC_OVER,
                             abgr_a(c->f->panel, 0xFFu)));

    /* ticks: two multi-rect paths (bright i%4==0 at 158, dim 87); tick_w in
     * units, drawn as thin rects centred on the tick y. Adjacent ticks in
     * the SAME pass whose rects would overlap (spacing = travel/(n-1) <
     * tick_w) are COALESCED into one run rect -- this is what ENFORCES
     * winding-1, since spacing shrinks below tick_w at large vh/small n and
     * collapses to 0 at travel=0 (the file header's coalescing note).
     * ★ The union-of-overlapping-rects-is-a-rect argument (so `run_y1 = y1`
     * is equivalent to `run_y1 = max(run_y1, y1)`) depends on ty being
     * MONOTONIC in i, which holds because synthui_fader_geom() clamps
     * travel >= 0 -- without that clamp a negative travel would walk ty
     * backwards and a later, shorter run could be silently dropped instead
     * of extending it. `y0 <= run_y1` (not `<`) is deliberate too: it
     * coalesces exactly-touching rects, avoiding a coincident-edge
     * tessellation case rather than merely a strictly-overlapping one. */
    const float tick_w = fmaxf(1.4f, 0.012f * g->vh);
    const int n = c->f->ticks;
    for (int pass = 0; pass < 2; pass++) {
        start = s_used;
        int emitted = 0;
        float run_y0 = 0.0f, run_y1 = 0.0f;
        for (int i = 0; i < n; i++) {
            const bool bright = (i % 4 == 0);
            if (bright != (pass == 0)) continue;
            const float ty = g->top + ch * 0.5f
                             + (float)i * g->travel / (float)(n - 1);
            const float y0 = ty - tick_w * 0.5f, y1 = ty + tick_w * 0.5f;
            if (emitted && y0 <= run_y1) { run_y1 = y1; continue; } /* extend */
            if (emitted) emit_rect(8.0f, run_y0, 84.0f, run_y1 - run_y0);
            run_y0 = y0; run_y1 = y1; emitted++;
        }
        if (emitted) emit_rect(8.0f, run_y0, 84.0f, run_y1 - run_y0);
        if (!emitted) { s_used = start; continue; }
        if (finish_path(&p, start, 8.0f, 0.0f, 92.0f, g->vh))
            GPU_TRY(vg_lite_draw(s_cur_target, &p, VG_LITE_FILL_NON_ZERO,
                                 (vg_lite_matrix_t *)c->m, VG_LITE_BLEND_SRC_OVER,
                                 abgr_a(pal->ticks, pass == 0 ? 158u : 87u)));
    }

    /* rod */
    start = s_used;
    emit_round_rect(46.5f, g->top + ch * 0.5f - 2.0f, 7.0f,
                    g->travel + 4.0f, 1.5f);
    if (finish_path(&p, start, 46.5f, 0.0f, 53.5f, g->vh))
        GPU_TRY(vg_lite_draw(s_cur_target, &p, VG_LITE_FILL_NON_ZERO,
                             (vg_lite_matrix_t *)c->m, VG_LITE_BLEND_SRC_OVER,
                             abgr_a(0x14181Bu, 0xFFu)));

    /* center-detent line */
    if (c->f->center) {
        const float cyl = g->top + ch * 0.5f + g->travel * 0.5f;
        start = s_used; emit_rect(4.0f, cyl - 1.2f, 92.0f, 2.4f);
        if (finish_path(&p, start, 4.0f, cyl - 1.2f, 96.0f, cyl + 1.2f))
            GPU_TRY(vg_lite_draw(s_cur_target, &p, VG_LITE_FILL_NON_ZERO,
                                 (vg_lite_matrix_t *)c->m, VG_LITE_BLEND_SRC_OVER,
                                 abgr_a(pal->center, 0xFFu)));
    }

    /* cap: shadow, base, grad x2, groove, gloss x2, border ring */
    start = s_used; emit_round_rect(6.0f, cy + 2.5f, 88.0f, ch, 2.0f);
    if (finish_path(&p, start, 6.0f, cy, 94.0f, cy + ch + 3.0f))
        GPU_TRY(vg_lite_draw(s_cur_target, &p, VG_LITE_FILL_NON_ZERO,
                             (vg_lite_matrix_t *)c->m, VG_LITE_BLEND_SRC_OVER,
                             abgr_a(0x1B1F22u, 115u)));

    start = s_used; emit_round_rect(4.0f, cy, 88.0f, ch, 2.0f);
    if (finish_path(&p, start, 4.0f, cy, 92.0f, cy + ch))
        GPU_TRY(vg_lite_draw(s_cur_target, &p, VG_LITE_FILL_NON_ZERO,
                             (vg_lite_matrix_t *)c->m, VG_LITE_BLEND_SRC_OVER,
                             abgr_a(pal->cap_mid, 0xFFu)));

    /* gradient bands, inset inside the border. */
    const struct { float y0, h; int band; uint32_t top, bot; } bands[2] = {
        { cy + bw,          0.46f * ch - bw, 0, pal->cap_top, pal->cap_mid },
        { cy + 0.46f * ch,  0.54f * ch - bw, 1, pal->cap_mid, pal->cap_low },
    };
#ifndef FD_GPU_GRAD_STRIPS
    /* EXT linear gradient: the grad's own matrix is a COPY of the fader
     * matrix with translate+scale composed on top, mapping the ramp's
     * canonical (0,0)->(0,1) line onto this band's actual screen position
     * and height (see grad_get's comment) -- no rotate needed (the line
     * already points down) and no /256 (the length is literal, not a
     * normalized ramp-texel index). The PATH's own matrix (passed to
     * vg_lite_draw_linear_grad below) stays the plain fader matrix c->m,
     * same as every other shape in this function -- only the gradient's
     * OWN sampling matrix gets the extra translate+scale. */
    for (int b = 0; b < 2; b++) {
        vg_lite_ext_linear_gradient_t *gr = grad_get(bands[b].band, c->stidx,
                                                      bands[b].top, bands[b].bot);
        if (gr == NULL) continue;
        start = s_used;
        emit_rect(4.0f + bw, bands[b].y0, 88.0f - 2.0f * bw, bands[b].h);
        if (!finish_path(&p, start, 4.0f + bw, bands[b].y0,
                         92.0f - bw, bands[b].y0 + bands[b].h))
            continue;
        vg_lite_matrix_t *gm = vg_lite_get_linear_grad_matrix(gr);
        *gm = *(vg_lite_matrix_t *)c->m;
        vg_lite_translate((4.0f + bw) * FD_FIX, bands[b].y0 * FD_FIX, gm);
        vg_lite_scale(1.0f, bands[b].h * FD_FIX, gm);
        GPU_TRY(vg_lite_draw_linear_grad(s_cur_target, &p, VG_LITE_FILL_NON_ZERO,
                                         (vg_lite_matrix_t *)c->m, gr,
                                         0, VG_LITE_BLEND_SRC_OVER,
                                         VG_LITE_FILTER_LINEAR));
    }
#else
    /* Escape hatch: N solid horizontal strips per band, linearly
     * RGB-interpolated, through the ordinary solid-fill draw site. */
    for (int b = 0; b < 2; b++) {
        for (int s = 0; s < FD_GPU_GRAD_STRIPS; s++) {
            const float t = ((float)s + 0.5f) / (float)FD_GPU_GRAD_STRIPS;
            const uint32_t col = lerp_rgb(bands[b].top, bands[b].bot, t);
            const float sy0 = bands[b].y0
                             + bands[b].h * (float)s / (float)FD_GPU_GRAD_STRIPS;
            const float sy1 = bands[b].y0
                             + bands[b].h * (float)(s + 1) / (float)FD_GPU_GRAD_STRIPS;
            start = s_used;
            emit_rect(4.0f + bw, sy0, 88.0f - 2.0f * bw, sy1 - sy0);
            if (finish_path(&p, start, 4.0f + bw, sy0, 92.0f - bw, sy1))
                GPU_TRY(vg_lite_draw(s_cur_target, &p, VG_LITE_FILL_NON_ZERO,
                                     (vg_lite_matrix_t *)c->m, VG_LITE_BLEND_SRC_OVER,
                                     abgr_a(col, 0xFFu)));
        }
    }
#endif

    start = s_used; emit_rect(4.0f, cy + 0.43f * ch, 88.0f, 0.14f * ch);
    if (finish_path(&p, start, 4.0f, cy, 92.0f, cy + ch))
        GPU_TRY(vg_lite_draw(s_cur_target, &p, VG_LITE_FILL_NON_ZERO,
                             (vg_lite_matrix_t *)c->m, VG_LITE_BLEND_SRC_OVER,
                             abgr_a(0x20262Au, 0xFFu)));

    const float gh = fmaxf(1.5f, 0.12f * ch);
    for (int s = 0; s < 2; s++) {
        const float gy = cy + (s ? 0.68f : 0.16f) * ch;
        start = s_used; emit_rect(9.0f, gy, 78.0f, gh);
        if (finish_path(&p, start, 9.0f, gy, 87.0f, gy + gh))
            GPU_TRY(vg_lite_draw(s_cur_target, &p, VG_LITE_FILL_NON_ZERO,
                                 (vg_lite_matrix_t *)c->m, VG_LITE_BLEND_SRC_OVER,
                                 abgr_a(0xFFFFFFu, pal->gloss_opa)));
    }

    start = s_used; emit_border_ring(4.0f, cy, 88.0f, ch, 2.0f, bw);
    if (finish_path(&p, start, 4.0f, cy, 92.0f, cy + ch))
        GPU_TRY(vg_lite_draw(s_cur_target, &p, VG_LITE_FILL_NON_ZERO,
                             (vg_lite_matrix_t *)c->m, VG_LITE_BLEND_SRC_OVER,
                             abgr_a(0x20262Au, 0xFFu)));
}

/* One composite pass over every pending instance, into *s_cur_target. */
static void compose_pass(void)
{
    lv_display_t *disp = lv_display_get_default();
    if (disp == NULL) return;   /* nothing to composite into (rotary's guard) */
    bool drew = false;
    for (synthui_fader_t *f = synthui_fader_list; f; f = f->next) {
        if (!f->gpu_pending) continue;
        f->gpu_pending = false;
        lv_area_t coords; synthui_fader_geom_t g;
        if (!synthui_fader_geom(f, &g, &coords)) continue;
        synthui_fader_palette_t pal;
        synthui_fader_palette(f, &pal);
        const lv_state_t st = lv_obj_get_state(&f->obj);
        const int stidx = (st & LV_STATE_DISABLED) ? 2
                        : (st & LV_STATE_PRESSED)  ? 1 : 0;
        vg_lite_matrix_t m; vg_lite_identity(&m);
        vg_lite_translate((float)coords.x1, (float)coords.y1, &m);
        vg_lite_scale(g.u / FD_FIX, g.u / FD_FIX, &m);
        const fd_gpu_ctx_t ctx = { f, &g, &pal, &m,
                                   synthui_fader_cap_y(&g, f->value), stidx };
        lv_area_t done[LV_INV_BUF_SIZE];
        int ndone = 0;
        for (uint32_t i = 0; i < disp->inv_p; i++) {
            if (disp->inv_area_joined[i]) continue;    /* merged, not drawn */
            lv_area_t clip;
            if (!lv_area_intersect(&clip, &disp->inv_areas[i], &coords))
                continue;                              /* not this fader */
            composite_minus(&ctx, clip, done, ndone);
            done[ndone++] = clip;
            drew = true;
        }
    }
    if (drew) {
        GPU_TRY(vg_lite_set_scissor(-1, -1, -1, -1));  /* disable */
        /* Retire before anyone (checksums, scanout) touches the buffer. */
        GPU_TRY(vg_lite_finish());
    }
    /* Reclaim the arena -- only AFTER finish, in case the driver references
     * (rather than copied) the path data until submit. Belt-and-braces only:
     * s_overflow is counted and reset PER CALL in finish_path()/
     * draw_fader_clipped() now, so there is nothing left to count here --
     * every emit() that ever set s_overflow was already followed by its own
     * shape's finish_path() before this point (the one branch in
     * draw_fader_clipped that skips finish_path() -- the empty-pass
     * `s_used = start; continue;` in the tick loop -- can only run when that
     * pass emitted nothing, so it can't be hiding a trip it caused itself). */
    s_used = 0;
    s_overflow = false;
}

uint32_t synthui_fader_gpu_errors(void) { return s_err; }

/* ---- deferred (double-buffered) mode ---- */
static int32_t s_def_w, s_def_h, s_def_stride;
static vg_lite_buffer_t s_def_targets[2];
static void *s_def_ptrs[2];
static int s_def_n = 0;

bool synthui_fader_gpu_begin_deferred(int32_t w, int32_t h,
                                      int32_t stride_bytes)
{
    if (s_begun) return true;
    s_def_w = w; s_def_h = h; s_def_stride = stride_bytes;
    s_def_n = 0;
    s_used = 0; s_overflow = false;
#ifndef FD_GPU_GRAD_STRIPS
    memset(s_grad_ready, 0, sizeof(s_grad_ready));
#endif
    synthui_fader_gpu_enabled = true;
    s_begun = true;
    return true;
}

void synthui_fader_gpu_compose_into(uint8_t *framebuffer)
{
    if (!synthui_fader_gpu_enabled || framebuffer == NULL) return;
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
        /* REGISTER the target with the driver or every draw "succeeds" and
         * changes nothing (vglite_probe, measured on silicon). */
        if (vg_lite_map(t, VG_LITE_MAP_USER_MEMORY, 0) != VG_LITE_SUCCESS) {
            s_err++;
            return;
        }
        s_def_ptrs[s_def_n++] = framebuffer;
    }
    s_cur_target = &s_def_targets[i];
    compose_pass();
    s_cur_target = NULL;
}
