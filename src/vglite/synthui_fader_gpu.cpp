/* synthui_fader_gpu.cpp - see synthui_fader_gpu.h.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT
 *
 * ★★★ ONE CONTOUR PER PATH -- MEASURED ON SILICON 2026-08-29/30, and the
 * rule every path in this file follows. This GC355 (this driver + this
 * chip, empirically -- the mechanism is not identified) RENDERS ONLY THE
 * FIRST CONTOUR of a vg_lite path. Any path containing more than one
 * VLC_OP_MOVE loses every subpath after the first; vg_lite_init_path's
 * CLOSE->END fixup only rewrites a trailing CLOSE, so that is not the
 * mechanism -- treat the rule as empirical, not derived.
 * Two independent confirmations from one static SWD framebuffer capture of
 * the golden scene, BEFORE this fix:
 *  - Ticks. The software golden draws 12 visible ticks per fader at
 *    y = 143, 157, 170, 184, 197, 211, 224, 237, 251, 264, 278, 291. The GPU
 *    drew EXACTLY TWO: y = 143 and y = 156 -- tick i=0 (the first rect of
 *    the bright pass's path) and tick i=1 (the first rect of the dim
 *    pass's path). The emitting loop was correct (it emitted 4 rects for
 *    the bright pass and 9 for the dim pass at this spacing; nothing
 *    coalesces), so the loss was in rendering, not emission.
 *  - The border ring (see below). Its first contour is the OUTER rounded
 *    rect, so with the inner contour dropped it filled solid -- exactly the
 *    "solid dark cap" seen on glass, in the border colour 0x20262A.
 * This SUPERSEDES the previous claim in this file that "several disjoint
 * same-winding contours" (the old multi-rect tick batching) were safe --
 * that claim was WRONG, and the tick evidence above disproves it directly:
 * both winding-1 tick paths lost every contour after their first regardless
 * of winding or disjointness. The rotary's sibling file
 * (synthui_rotary_knob_gpu.cpp) never hit this because every path it builds
 * is already a SINGLE contour (emit_ring is a deliberate one-contour
 * keyhole; emit_track's comment records rewriting an earlier multi-subpath
 * version that rendered nondeterministically -- a DIFFERENT defect than
 * this one, but the same practical fix: don't emit more than one MOVE per
 * path). Every shape in this file that can have more than one instance
 * (ticks) is now its own path and its own vg_lite_draw call; see the tick
 * loop below.
 *
 * Structure is synthui_rotary_knob_gpu.cpp's, simplified: every shape is
 * emitted per frame into the bump arena in viewBox units x16 (S32) with the
 * fader's CURRENT cap_y baked in, and ONE matrix per fader
 * (translate(coords) * scale(u/16)) maps them to the screen -- a fader has
 * no rotation, so the rotary's two-matrix AA lesson does not arise. Ticks
 * are still run-coalesced (adjacent ticks whose rects would overlap --
 * spacing < tick_w -- are merged into one run rect rather than emitted as
 * separate overlapping rects; the union of overlapping same-x axis-aligned
 * rects is itself a rect, so this stays visually identical), but each
 * coalesced run is now its OWN path, finished and drawn immediately -- see
 * the ONE-CONTOUR-PER-PATH rule above. The coalescing logic still matters
 * for the degenerate case where spacing shrinks toward 0 near travel=0: it
 * keeps the DRAW COUNT down (fewer, wider rects) even though correctness no
 * longer depends on it the way it did when runs shared one multi-contour
 * path.
 *
 * The cap's border was originally a stroked RING -- an outer rounded
 * contour plus a reversed inner contour in one path, relying on non-zero
 * winding to cut the hole (this file's counterpart to the rotary's
 * emit_ring). Unlike the rotary's ring, which is a single simple contour
 * (one MOVE/CLOSE: the outer edge, a LINE across, the reversed inner edge,
 * CLOSE -- the keyhole technique), this file's ring was TWO nested
 * opposite-winding subpaths (two separate VLC_OP_MOVEs). On GC355 silicon
 * that geometry rendered the whole cap SOLID in the border colour and the
 * full-framebuffer checksum differed on every one of 7 boots across two
 * builds, with fd_gpu_err=0 throughout (2026-08-29) -- at the time this was
 * attributed to the same per-boot-nondeterminism class the rotary's
 * emit_track comment records for overlapping subpaths; the 2026-08-29/30
 * tick investigation above found the REAL mechanism (first-contour-only
 * rendering) and it explains this symptom exactly, deterministically, with
 * no nondeterminism required -- the "solid cap" IS the outer contour drawn
 * alone. Fixed by eliminating the ring rather than trying to make the
 * winding work: the border is now a plain filled rounded-rect PLATE drawn
 * FIRST, with the base/bands/groove inset by bw on top of it, so the border
 * shows as a margin rather than a cut hole. That fix satisfies the
 * ONE-CONTOUR-PER-PATH rule as a side effect (a filled rounded rect is a
 * single contour), which is exactly why it worked.
 *
 * The cap bands are drawn as FD_GRAD_STRIPS solid interpolated strips (see
 * near lerp_rgb below) -- solid strips are the ONLY implementation; both
 * vg_lite gradient APIs were investigated and ruled out:
 *  - Legacy vg_lite_draw_grad is GC255-only. NXP's own vglite_layer example
 *    calls it only when chip_id == 0x255 (mcuxsdk vglite_layer.c); on our
 *    GC355 it rendered solid black and produced a per-boot-varying checksum
 *    on silicon while every vg_lite_* call kept returning VG_LITE_SUCCESS.
 *  - The EXT API (vg_lite_set/update/draw_linear_grad) has a
 *    PLACEMENT-DEPENDENT ramp: vg_lite_update_linear_grad (VGLite vg_lite.c)
 *    transforms the gradient line by grad->matrix, derives the screen-space
 *    length from that, then OVERWRITES both grad->matrix (to a ramp-surface
 *    -> screen matrix) and grad->linear_grad (to (0,0)->(width,0)), and
 *    allocates the ramp image -- so the ramp must be rebuilt whenever
 *    placement changes. Our cap moves EVERY FRAME on 16 widgets, i.e. ~32
 *    rebuilds/frame, each allocating 1 KB (update_linear_grad does NOT free
 *    the previous surface -- it leaks unless explicitly cleared), with a
 *    hard ordering rule that no draw referencing a ramp may be queued while
 *    it is being rebuilt. Unsuitable. The construction this file carried
 *    before this change was additionally broken outright: grad->matrix is
 *    zero at update time, so the derived length is 0 and
 *    vg_lite_update_linear_grad returns VG_LITE_INVALID_ARGUMENT -- bands
 *    would have been skipped every frame. LVGL contains the same two
 *    mistakes and ships LV_VG_LITE_DISABLE_LINEAR_GRADIENT_EXT to route
 *    around this path -- it is NOT a safe reference for this API.
 * Strips use only the solid-fill machinery (emit_rect/finish_path/
 * vg_lite_draw) that every other shape in this compositor uses and that is
 * PROVEN working on this silicon, are deterministic by construction (no
 * ramp memory to sample), and at this widget size a band is ~10 px tall so
 * the banding is invisible.
 * No blits, so the 64-byte source-stride rule does not apply. No D-cache
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
 * PRECEDING vg_lite_draw() call returns --
 * draw_fader_clipped() resets s_used to 0 on entry (see there), and the true
 * peak is ONE fader's shapes, worst case (a full, unculled clip -- see the
 * ONE-CONTOUR-PER-PATH note in the file header and the arena arithmetic near
 * lerp_rgb below for the current tick accounting) 712 non-band words at 33
 * ticks: panel/center/groove/two gloss rects ~14 words each, rod/shadow/base
 * ~45 words each, border PLATE ~45 words (a rounded rect, same shape as base
 * -- see the file header for why this replaced a 58-word nested-contour
 * ring), ticks up to ~462 words (33 one-rect runs, each its own path with
 * its own END -- worst case, no coalescing) -- PLUS the two cap bands, each
 * now drawn as FD_GRAD_STRIPS solid strips instead of one gradient-filled
 * rect apiece: 712 + 28 * FD_GRAD_STRIPS words, 936 at the default
 * FD_GRAD_STRIPS=8 -- comfortably inside 2048 with margin.
 * Overflow is still COUNTED
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

/* Colour packing (ABGR) and the REQUIRED premultiply live in
 * ../synthui_fader_color.h, so a host test can reach them; see that header
 * for the silicon measurement that makes the premultiply mandatory. */
#include "../synthui_fader_color.h"

static uint32_t abgr_a(uint32_t hex, uint32_t a)
{
    return synthui_fd_abgr_a(hex, a);
}

/* ---- cap-band gradient: solid strips (the ONLY implementation -- see the
 * file header for why both vg_lite gradient APIs were ruled out). Each band
 * is drawn as FD_GRAD_STRIPS solid horizontal strips, linearly
 * RGB-interpolated between the band's two endpoint colours (strip i,
 * 0-based, at t = (i + 0.5) / FD_GRAD_STRIPS), through the SAME emit_rect +
 * finish_path + vg_lite_draw machinery as every other solid shape in this
 * file, packed with abgr_a() (defined above) -- NOT a ramp/image packing,
 * since this is an ordinary solid fill. */

/* Quality/cost knob: strips per band. A band is ~10 px tall on this panel,
 * so 8 strips is ~1.3 px each -- imperceptible banding -- at the cost of one
 * extra vg_lite_draw() per strip; see the arena arithmetic below. */
#define FD_GRAD_STRIPS 8
#if FD_GRAD_STRIPS < 2
#error "FD_GRAD_STRIPS must be >= 2"
#endif

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

/* Arena arithmetic -- see FD_ARENA_WORDS above for the worst-case words of a
 * full (unculled -- the cull in draw_fader_clipped only ever REDUCES words
 * used, so the bound below assumes a full-fader clip where nothing is
 * culled) draw_fader_clipped() call excluding the cap bands, now that ticks
 * are ONE CONTOUR PER PATH (see the file header). s_used is a bump
 * allocator that is NOT reclaimed between shapes within one call -- only at
 * the top of the next draw_fader_clipped() call -- so drawing each tick run
 * separately does not shrink the per-call peak; it only adds one
 * VLC_OP_END word per run in place of one END per PASS:
 *  - panel/center-line/groove/two gloss rects: 5 x 14 words (an emit_rect
 *    contour is 13 words -- MOVE+2, three LINEs at 3 words each, CLOSE --
 *    plus finish_path's own END) = 70
 *  - rod/shadow/base: 3 x 45 words (a rounded rect -- MOVE+2, four
 *    LINE+CUBIC pairs, CLOSE, END) = 135
 *  - border PLATE: 1 x 45 words = 45
 *  - ticks: worst case is the full 33-tick clamp (synthui_fader_set_ticks)
 *    with NO coalescing at all, so every tick is its own one-rect run:
 *    33 emit_rects x 13 words = 429, PLUS one END per run (33, not 2 --
 *    the one-contour-per-path change is exactly what turned "one END per
 *    PASS" into "one END per RUN") = 462
 *  Subtotal: 70 + 135 + 45 + 462 = 712.
 * The two cap bands add 2 * FD_GRAD_STRIPS strip rects at 14 words each
 * (13-word emit_rect contour + END), each its own finish_path()/
 * vg_lite_draw() call since each strip needs its own colour. Worst case:
 * 712 + 28 * FD_GRAD_STRIPS words. At the default FD_GRAD_STRIPS=8:
 * 712 + 224 = 936, comfortably inside FD_ARENA_WORDS (2048, ~54% margin);
 * FD_GRAD_STRIPS could rise to 47 before 2048 is exceeded ((2048-712)/28 =
 * 47.7), well past the documented headroom of 16. Asserted, not just
 * stated, so a future N this arena can't hold fails to compile instead of
 * silently truncating (finish_path()'s overflow guard still catches it at
 * runtime either way, but a build-time check is cheaper than a bench
 * boot). */
static_assert(712 + 28 * FD_GRAD_STRIPS <= FD_ARENA_WORDS,
             "FD_GRAD_STRIPS too large for FD_ARENA_WORDS");

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
    lv_area_t coords;               /* screen-space obj footprint (compose_pass) */
} fd_gpu_ctx_t;

static void draw_fader_clipped(const fd_gpu_ctx_t *c, const lv_area_t *clip);

/* ---- clip culling (pure optimisation) -------------------------------------
 * draw_fader_clipped's scissor already restricts every pixel written to
 * *clip, so skipping a shape whose screen bbox misses *clip entirely cannot
 * change a single output pixel -- it only avoids submitting a draw that
 * would paint nothing. During the cap-drag animation *clip is a narrow strip
 * around the moving cap, so this is what makes drawing each tick run as its
 * own vg_lite_draw() call (the ONE-CONTOUR-PER-PATH fix, file header)
 * affordable: most runs fall outside that strip and are never emitted.
 * x0/y0/x1/y1 are in the shape's own VIEWBOX UNITS (pre-matrix, matching
 * every emit_*() call in this file). Converted to screen pixels via the
 * same coords.y1 + y * g->u the sw path uses (synthui_fader.cpp's
 * fd_cap_extent), rounded OUTWARD -- floor the low edge, ceil the high edge
 * -- and inflated by 1 px on every side before testing. Deliberately
 * conservative: a cull that is even slightly too tight would drop a
 * partially-visible shape, and lv_area_t's y2/x2 are INCLUSIVE, so an exact
 * (non-inflated) bound would be one pixel short of a shape whose true edge
 * lands exactly on clip's boundary. When in doubt this returns true (draw
 * it) rather than false. */
static bool fd_bbox_visible(const fd_gpu_ctx_t *c, const lv_area_t *clip,
                            float x0, float y0, float x1, float y1)
{
    const int32_t sx0 = c->coords.x1 + (int32_t)floorf(x0 * c->g->u) - 1;
    const int32_t sx1 = c->coords.x1 + (int32_t)ceilf(x1 * c->g->u) + 1;
    const int32_t sy0 = c->coords.y1 + (int32_t)floorf(y0 * c->g->u) - 1;
    const int32_t sy1 = c->coords.y1 + (int32_t)ceilf(y1 * c->g->u) + 1;
    return sx0 <= clip->x2 && sx1 >= clip->x1 && sy0 <= clip->y2 && sy1 >= clip->y1;
}

/* Emit, finish and draw one coalesced tick run as its own single-contour
 * path (the ONE-CONTOUR-PER-PATH rule, file header) -- culled against *clip
 * FIRST, before anything is emitted into the arena, since the scissor
 * already confines output there and a culled run costs nothing. x range
 * 8..92 units matches the tick rect emitted below (emit_rect(8, y, 84, h)). */
static void fd_draw_tick_run(const fd_gpu_ctx_t *c, const lv_area_t *clip,
                             vg_lite_path_t *p, float y0, float y1,
                             uint32_t color, uint32_t opa)
{
    if (!fd_bbox_visible(c, clip, 8.0f, y0, 92.0f, y1)) return;
    const size_t start = s_used;
    emit_rect(8.0f, y0, 84.0f, y1 - y0);
    if (finish_path(p, start, 8.0f, y0, 92.0f, y1))
        GPU_TRY(vg_lite_draw(s_cur_target, p, VG_LITE_FILL_NON_ZERO,
                             (vg_lite_matrix_t *)c->m, VG_LITE_BLEND_SRC_OVER,
                             abgr_a(color, opa)));
}

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
    /* Reset the per-draw-call arena HERE, not once per frame: vg_lite_draw()
     * copies (memcpy, push_data) every path's words into the command buffer
     * before returning (see FD_ARENA_WORDS above), so nothing from a prior
     * call to this function -- a different clip piece, a different fader --
     * is still needed once that prior call's vg_lite_draw()s have returned.
     * This is what shrinks the real peak from "the whole frame" (up to ~80
     * calls) to "one fader's shapes" (712 + 28*FD_GRAD_STRIPS words worst
     * case, see the arena arithmetic near lerp_rgb above). s_overflow is
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

    /* ticks: bright (i%4==0, opa 158) and dim (opa 87) passes, tick_w in
     * units, drawn as thin rects centred on the tick y. Adjacent ticks in
     * the SAME pass whose rects would overlap (spacing = travel/(n-1) <
     * tick_w) are COALESCED into one run rect -- the union of overlapping
     * same-x axis-aligned rects is itself a rect, so this stays visually
     * identical while cutting the draw count (matters more now that each
     * run is its own vg_lite_draw() call -- see below). Coalescing no
     * longer has anything to do with winding (the file header's
     * ONE-CONTOUR-PER-PATH finding: winding was never the mechanism).
     * ★ The union-of-overlapping-rects-is-a-rect argument (so `run_y1 = y1`
     * is equivalent to `run_y1 = max(run_y1, y1)`) depends on ty being
     * MONOTONIC in i, which holds because synthui_fader_geom() clamps
     * travel >= 0 -- without that clamp a negative travel would walk ty
     * backwards and a later, shorter run could be silently dropped instead
     * of extending it. `y0 <= run_y1` (not `<`) is deliberate too: it
     * coalesces exactly-touching rects, avoiding a coincident-edge
     * tessellation case rather than merely a strictly-overlapping one.
     * ★ EACH RUN IS ITS OWN PATH, finished and drawn the moment it ends
     * (fd_draw_tick_run below), instead of accumulating every run in one
     * pass into a single multi-MOVE path -- that accumulation is exactly
     * the shape of path this silicon drops subpaths from (file header).
     * A run is culled (via fd_bbox_visible) before it is even emitted: the
     * scissor already confines every draw to *clip, so a run whose box
     * misses *clip entirely can be skipped for free -- during the cap-drag
     * animation *clip is a narrow strip, so typically only the 2-3 runs
     * nearest the cap survive out of up to 33. */
    const float tick_w = fmaxf(1.4f, 0.012f * g->vh);
    const int n = c->f->ticks;
    for (int pass = 0; pass < 2; pass++) {
        bool have_run = false;
        float run_y0 = 0.0f, run_y1 = 0.0f;
        const uint32_t opa = (pass == 0) ? 158u : 87u;
        for (int i = 0; i < n; i++) {
            const bool bright = (i % 4 == 0);
            if (bright != (pass == 0)) continue;
            const float ty = g->top + ch * 0.5f
                             + (float)i * g->travel / (float)(n - 1);
            const float y0 = ty - tick_w * 0.5f, y1 = ty + tick_w * 0.5f;
            if (have_run && y0 <= run_y1) { run_y1 = y1; continue; } /* extend */
            if (have_run) fd_draw_tick_run(c, clip, &p, run_y0, run_y1,
                                           pal->ticks, opa);
            run_y0 = y0; run_y1 = y1; have_run = true;
        }
        if (have_run) fd_draw_tick_run(c, clip, &p, run_y0, run_y1,
                                       pal->ticks, opa);
    }

    /* rod */
    start = s_used;
    emit_round_rect(46.5f, g->top + ch * 0.5f - 2.0f, 7.0f,
                    g->travel + 4.0f, 1.5f);
    if (finish_path(&p, start, 46.5f, 0.0f, 53.5f, g->vh))
        GPU_TRY(vg_lite_draw(s_cur_target, &p, VG_LITE_FILL_NON_ZERO,
                             (vg_lite_matrix_t *)c->m, VG_LITE_BLEND_SRC_OVER,
                             abgr_a(0x14181Bu, 0xFFu)));

    /* center-detent line -- culled like the ticks (fd_bbox_visible): fixed
     * at cap-centre travel regardless of the cap's current position, so it
     * is very often outside a narrow drag-animation clip. */
    if (c->f->center) {
        const float cyl = g->top + ch * 0.5f + g->travel * 0.5f;
        if (fd_bbox_visible(c, clip, 4.0f, cyl - 1.2f, 96.0f, cyl + 1.2f)) {
            start = s_used; emit_rect(4.0f, cyl - 1.2f, 92.0f, 2.4f);
            if (finish_path(&p, start, 4.0f, cyl - 1.2f, 96.0f, cyl + 1.2f))
                GPU_TRY(vg_lite_draw(s_cur_target, &p, VG_LITE_FILL_NON_ZERO,
                                     (vg_lite_matrix_t *)c->m, VG_LITE_BLEND_SRC_OVER,
                                     abgr_a(pal->center, 0xFFu)));
        }
    }

    /* cap: shadow, border plate, base, band strips x 2*FD_GRAD_STRIPS,
     * groove (inset), gloss x2. */
    start = s_used; emit_round_rect(6.0f, cy + 2.5f, 88.0f, ch, 2.0f);
    if (finish_path(&p, start, 6.0f, cy, 94.0f, cy + ch + 3.0f))
        GPU_TRY(vg_lite_draw(s_cur_target, &p, VG_LITE_FILL_NON_ZERO,
                             (vg_lite_matrix_t *)c->m, VG_LITE_BLEND_SRC_OVER,
                             abgr_a(0x1B1F22u, 115u)));

    /* ★ border PLATE, not a stroked ring: a plain filled rounded rect drawn
     * UNDER everything else, with the base/bands/groove inset by bw so the
     * border shows as a margin. This replaces a nested outer-rounded +
     * reversed-inner two-subpath ring that relied on non-zero winding to cut
     * the hole -- on GC355 silicon that geometry rendered the WHOLE CAP
     * SOLID in this same border colour, with a full-framebuffer checksum
     * that differed on every one of 7 boots across two builds and
     * fd_gpu_err=0 throughout (2026-08-29). See the file header for the
     * full account and the winding rule this file now follows; the rotary's
     * emit_track comment records the same hardware hostility to overlapping
     * subpaths. */
    start = s_used; emit_round_rect(4.0f, cy, 88.0f, ch, 2.0f);
    if (finish_path(&p, start, 4.0f, cy, 92.0f, cy + ch))
        GPU_TRY(vg_lite_draw(s_cur_target, &p, VG_LITE_FILL_NON_ZERO,
                             (vg_lite_matrix_t *)c->m, VG_LITE_BLEND_SRC_OVER,
                             abgr_a(0x20262Au, 0xFFu)));

    start = s_used; emit_round_rect(4.0f + bw, cy + bw, 88.0f - 2.0f * bw,
                                    ch - 2.0f * bw, 2.0f);
    if (finish_path(&p, start, 4.0f + bw, cy + bw, 92.0f - bw, cy + ch - bw))
        GPU_TRY(vg_lite_draw(s_cur_target, &p, VG_LITE_FILL_NON_ZERO,
                             (vg_lite_matrix_t *)c->m, VG_LITE_BLEND_SRC_OVER,
                             abgr_a(pal->cap_mid, 0xFFu)));

    /* cap bands, inset inside the border -- solid strips (see file header
     * and the FD_GRAD_STRIPS block above for why). */
    const struct { float y0, h; uint32_t top, bot; } bands[2] = {
        { cy + bw,          0.46f * ch - bw, pal->cap_top, pal->cap_mid },
        { cy + 0.46f * ch,  0.54f * ch - bw, pal->cap_mid, pal->cap_low },
    };
    for (int b = 0; b < 2; b++) {
        for (int s = 0; s < FD_GRAD_STRIPS; s++) {
            const float t = ((float)s + 0.5f) / (float)FD_GRAD_STRIPS;
            const uint32_t col = lerp_rgb(bands[b].top, bands[b].bot, t);
            const float sy0 = bands[b].y0
                             + bands[b].h * (float)s / (float)FD_GRAD_STRIPS;
            const float sy1 = bands[b].y0
                             + bands[b].h * (float)(s + 1) / (float)FD_GRAD_STRIPS;
            start = s_used;
            emit_rect(4.0f + bw, sy0, 88.0f - 2.0f * bw, sy1 - sy0);
            if (finish_path(&p, start, 4.0f + bw, sy0, 92.0f - bw, sy1))
                GPU_TRY(vg_lite_draw(s_cur_target, &p, VG_LITE_FILL_NON_ZERO,
                                     (vg_lite_matrix_t *)c->m, VG_LITE_BLEND_SRC_OVER,
                                     abgr_a(col, 0xFFu)));
        }
    }

    /* groove: inset to stay inside the border plate, not full cap width. */
    start = s_used; emit_rect(4.0f + bw, cy + 0.43f * ch, 88.0f - 2.0f * bw,
                              0.14f * ch);
    if (finish_path(&p, start, 4.0f + bw, cy, 92.0f - bw, cy + ch))
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
        vg_lite_matrix_t m; vg_lite_identity(&m);
        vg_lite_translate((float)coords.x1, (float)coords.y1, &m);
        vg_lite_scale(g.u / FD_FIX, g.u / FD_FIX, &m);
        const fd_gpu_ctx_t ctx = { f, &g, &pal, &m,
                                   synthui_fader_cap_y(&g, f->value), coords };
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
     * shape's finish_path() before this point. The only paths that can skip
     * finish_path() entirely are culled tick runs and a culled centre line
     * (fd_bbox_visible, checked BEFORE any emit() call), so a cull can never
     * be hiding a trip it caused itself. */
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
