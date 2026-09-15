/* led_button_test.c - host unit test for the pure LedButton layout, damage
 * boxes and palette (synthui_led_button_math.h).
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT
 *
 * Every guard here was shown RED against a mutant before it was trusted,
 * and each line below names the assert that ACTUALLY failed, as measured:
 *   press_box dropping the pressed offset       -> px_contains(&press_pb, &cap_px)   (sweep, size 8, pressed)
 *   compute_layout with oy forced to 0          -> approx(M.oy, 20.0f)               (the 80x120 case)
 *   lit_box ignoring `pressed`                  -> px_contains(&lit_lb, &halo_px)    (sweep, size 20, pressed)
 *   dy_px truncated instead of rounded          -> P.dy_px == 3
 *   cap split at 0.50                           -> approx(L.cap_top.h, 49.6f)
 *   dots threshold at 32                        -> !S.dots_visible (33 px)
 *   circle_px ignoring dy_px                    -> the dot1 translation-equality assert (sweep, pressed)
 *   circle_px x2 without the "- 1"              -> dot1_px0.x2 == 39 (the circle_px(&L.dot1,0) pin)
 *   bw floor (max(1, ...)) removed               -> Bx16.bezel_bw_px == 1 (16x16 case)
 *
 * The pixel-space sweep (below) is placed BEFORE the narrow value pins so a
 * containment-breaking mutant fails there first, on a containment assert,
 * rather than on some later unrelated pin. */
#undef NDEBUG
#include "../src/synthui_led_button_math.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int approx(float a, float b) { return fabsf(a - b) < 0.01f; }

static int rect_eq(const synthui_led_button_rect_t *a, const synthui_led_button_rect_t *b)
{
    return a->x == b->x && a->y == b->y && a->w == b->w && a->h == b->h;
}

static int circ_eq(const synthui_led_button_circle_t *a, const synthui_led_button_circle_t *b)
{
    return a->cx == b->cx && a->cy == b->cy && a->r == b->r;
}

/* A rounded rect can land empty at very small sizes (x2 < x1 or y2 < y1 --
 * e.g. at 8 px the base rounds to y1=7, y2=6).  An empty area draws
 * nothing and needs no damage, so it is trivially "contained". */
static int px_empty(const synthui_led_button_px_t *r)
{
    return r->x2 < r->x1 || r->y2 < r->y1;
}

static int px_contains(const synthui_led_button_px_t *outer, const synthui_led_button_px_t *inner)
{
    return px_empty(inner) ||
           (inner->x1 >= outer->x1 && inner->y1 >= outer->y1 &&
            inner->x2 <= outer->x2 && inner->y2 <= outer->y2);
}

int main(void)
{
    /* --- pixel-space sweep: containment invariants across scale, aspect
     * ratio and press state.  This replaces reliance on the old float
     * rect_contains: every box here is the ACTUAL rounded pixel area the
     * widget would draw. --- */
    {
        static const float nonsquare[][2] = {
            {120.0f, 80.0f}, {80.0f, 120.0f}, {150.0f, 100.0f}, {56.0f, 56.0f}
        };
        int shape;
        for (shape = 0; shape < 393 + 4; ++shape) {
            float sw, sh;
            if (shape < 393) { sw = sh = (float)(8 + shape); }
            else             { sw = nonsquare[shape - 393][0]; sh = nonsquare[shape - 393][1]; }

            int pr;
            for (pr = 0; pr < 2; ++pr) {
                bool pressed = pr != 0;
                synthui_led_button_layout_t Lx;
                assert(synthui_led_button_compute_layout(sw, sh, pressed, &Lx));

                synthui_led_button_px_t press_pb, lit_lb;
                synthui_led_button_press_box(sw, sh, &press_pb);
                synthui_led_button_lit_box(sw, sh, pressed, &lit_lb);

                const synthui_led_button_px_t bounds = { 0, 0, (int32_t)sw - 1, (int32_t)sh - 1 };

                synthui_led_button_px_t cap_px   = synthui_led_button_rect_px(&Lx.cap, Lx.dy_px);
                synthui_led_button_px_t top_px   = synthui_led_button_rect_px(&Lx.cap_top, Lx.dy_px);
                synthui_led_button_px_t low_px   = synthui_led_button_rect_px(&Lx.cap_low, Lx.dy_px);
                synthui_led_button_px_t hi_px    = synthui_led_button_rect_px(&Lx.highlight, Lx.dy_px);
                synthui_led_button_px_t halo_px  = synthui_led_button_rect_px(&Lx.halo, Lx.dy_px);
                synthui_led_button_px_t led_px   = synthui_led_button_rect_px(&Lx.led, Lx.dy_px);
                synthui_led_button_px_t base_px  = synthui_led_button_rect_px(&Lx.base, Lx.dy_px);
                synthui_led_button_px_t d1_px    = synthui_led_button_circle_px(&Lx.dot1, Lx.dy_px);
                synthui_led_button_px_t d2_px    = synthui_led_button_circle_px(&Lx.dot2, Lx.dy_px);
                synthui_led_button_px_t bezel_px = synthui_led_button_rect_px(&Lx.bezel, 0);
                synthui_led_button_px_t well_px  = synthui_led_button_rect_px(&Lx.well, 0);

                /* every moving layer, at this press state, lies in press_box */
                assert(px_contains(&press_pb, &cap_px));
                assert(px_contains(&press_pb, &top_px));
                assert(px_contains(&press_pb, &low_px));
                assert(px_contains(&press_pb, &hi_px));
                assert(px_contains(&press_pb, &halo_px));
                assert(px_contains(&press_pb, &led_px));
                assert(px_contains(&press_pb, &base_px));
                assert(px_contains(&press_pb, &d1_px));
                assert(px_contains(&press_pb, &d2_px));

                /* led and halo lie inside lit_box(pressed) */
                assert(px_contains(&lit_lb, &led_px));
                assert(px_contains(&lit_lb, &halo_px));

                /* lit_box lies inside press_box */
                assert(px_contains(&press_pb, &lit_lb));

                /* everything drawn lies inside the widget */
                assert(px_contains(&bounds, &press_pb));
                assert(px_contains(&bounds, &lit_lb));
                assert(px_contains(&bounds, &bezel_px));
                assert(px_contains(&bounds, &well_px));
                assert(px_contains(&bounds, &cap_px));
                assert(px_contains(&bounds, &base_px));

                /* cap_top and cap_low meet exactly and together span cap --
                 * an exact equality (not a containment check), so it is
                 * unaffected by emptiness. */
                assert(top_px.y2 + 1 == low_px.y1);
                assert(top_px.y1 == cap_px.y1);
                assert(low_px.y2 == cap_px.y2);

                /* pressed: every moving layer TRANSLATES by dy_px, whole
                 * pixels, nothing else -- x1/x2 unchanged, y1/y2 each +
                 * dy_px, checked directly against the dy=0 pixel area
                 * (containment alone cannot see a wrong-but-still-inside
                 * position).  bezel/well are drawn at 0, so no check here. */
                if (pressed) {
                    const synthui_led_button_px_t cap0  = synthui_led_button_rect_px(&Lx.cap, 0);
                    const synthui_led_button_px_t top0  = synthui_led_button_rect_px(&Lx.cap_top, 0);
                    const synthui_led_button_px_t low0  = synthui_led_button_rect_px(&Lx.cap_low, 0);
                    const synthui_led_button_px_t hi0   = synthui_led_button_rect_px(&Lx.highlight, 0);
                    const synthui_led_button_px_t halo0 = synthui_led_button_rect_px(&Lx.halo, 0);
                    const synthui_led_button_px_t led0  = synthui_led_button_rect_px(&Lx.led, 0);
                    const synthui_led_button_px_t base0 = synthui_led_button_rect_px(&Lx.base, 0);
                    const synthui_led_button_px_t d1_0  = synthui_led_button_circle_px(&Lx.dot1, 0);
                    const synthui_led_button_px_t d2_0  = synthui_led_button_circle_px(&Lx.dot2, 0);

                    assert(cap_px.x1 == cap0.x1 && cap_px.x2 == cap0.x2 && cap_px.y1 == cap0.y1 + Lx.dy_px && cap_px.y2 == cap0.y2 + Lx.dy_px);
                    assert(top_px.x1 == top0.x1 && top_px.x2 == top0.x2 && top_px.y1 == top0.y1 + Lx.dy_px && top_px.y2 == top0.y2 + Lx.dy_px);
                    assert(low_px.x1 == low0.x1 && low_px.x2 == low0.x2 && low_px.y1 == low0.y1 + Lx.dy_px && low_px.y2 == low0.y2 + Lx.dy_px);
                    assert(hi_px.x1 == hi0.x1 && hi_px.x2 == hi0.x2 && hi_px.y1 == hi0.y1 + Lx.dy_px && hi_px.y2 == hi0.y2 + Lx.dy_px);
                    assert(halo_px.x1 == halo0.x1 && halo_px.x2 == halo0.x2 && halo_px.y1 == halo0.y1 + Lx.dy_px && halo_px.y2 == halo0.y2 + Lx.dy_px);
                    assert(led_px.x1 == led0.x1 && led_px.x2 == led0.x2 && led_px.y1 == led0.y1 + Lx.dy_px && led_px.y2 == led0.y2 + Lx.dy_px);
                    assert(base_px.x1 == base0.x1 && base_px.x2 == base0.x2 && base_px.y1 == base0.y1 + Lx.dy_px && base_px.y2 == base0.y2 + Lx.dy_px);
                    assert(d1_px.x1 == d1_0.x1 && d1_px.x2 == d1_0.x2 && d1_px.y1 == d1_0.y1 + Lx.dy_px && d1_px.y2 == d1_0.y2 + Lx.dy_px);
                    assert(d2_px.x1 == d2_0.x1 && d2_px.x2 == d2_0.x2 && d2_px.y1 == d2_0.y1 + Lx.dy_px && d2_px.y2 == d2_0.y2 + Lx.dy_px);
                }
            }
        }
    }

    /* --- 100x100, unpressed: the DC box in pixels, 1 unit = 1 px --- */
    synthui_led_button_layout_t L;
    assert(synthui_led_button_compute_layout(100.0f, 100.0f, false, &L));
    assert(approx(L.s, 1.0f) && approx(L.ox, 0.0f) && approx(L.oy, 0.0f));
    assert(L.dy_px == 0);
    assert(L.dots_visible);
    assert(approx(L.bezel.x, 0) && approx(L.bezel.y, 0) && approx(L.bezel.w, 100) && approx(L.bezel.h, 100));
    assert(approx(L.well.x, 7) && approx(L.well.y, 6) && approx(L.well.w, 86) && approx(L.well.h, 88));
    assert(approx(L.cap.x, 10) && approx(L.cap.y, 9) && approx(L.cap.w, 80) && approx(L.cap.h, 80));
    assert(approx(L.highlight.x, 14) && approx(L.highlight.y, 12) && approx(L.highlight.w, 72) && approx(L.highlight.h, 11));
    assert(approx(L.led.x, 26) && approx(L.led.y, 19) && approx(L.led.w, 48) && approx(L.led.h, 15));
    assert(approx(L.halo.x, 21) && approx(L.halo.y, 14) && approx(L.halo.w, 58) && approx(L.halo.h, 25));
    assert(approx(L.base.x, 10) && approx(L.base.y, 82) && approx(L.base.w, 80) && approx(L.base.h, 7));
    assert(approx(L.dot1.cx, 38) && approx(L.dot1.cy, 26.5f) && approx(L.dot1.r, 1.7f));
    assert(approx(L.dot2.cx, 62) && approx(L.dot2.cy, 26.5f) && approx(L.dot2.r, 1.7f));
    /* dot1's pixel area: cx 38, cy 26.5, r 1.7 -> x1 lroundf(36.3)=36,
     * y1 lroundf(24.8)=25, x2 lroundf(39.7)-1=39, y2 lroundf(28.2)-1=27. */
    {
        synthui_led_button_px_t dot1_px0 = synthui_led_button_circle_px(&L.dot1, 0);
        assert(dot1_px0.x1 == 36 && dot1_px0.y1 == 25 && dot1_px0.x2 == 39 && dot1_px0.y2 == 27);
    }
    assert(approx(L.bezel_r, 17) && approx(L.well_r, 13) && approx(L.cap_r, 11));
    assert(approx(L.highlight_r, 5.5f) && approx(L.halo_r, 8.5f) && approx(L.led_r, 3.5f) && approx(L.base_r, 3.5f));
    assert(L.bezel_bw_px == 2 && L.cue_bw_px == 4 && L.halo_bw_px == 10);

    /* cap split sits at 62 % and the two halves tile the cap exactly */
    assert(approx(L.cap_top.y, 9) && approx(L.cap_top.h, 49.6f));
    assert(approx(L.cap_low.y, 58.6f) && approx(L.cap_low.h, 30.4f));
    assert(approx(L.cap_top.y + L.cap_top.h, L.cap_low.y));
    assert(approx(L.cap_top.h + L.cap_low.h, L.cap.h));
    assert(approx(L.cap_top.x, L.cap.x) && approx(L.cap_top.w, L.cap.w));
    assert(approx(L.cap_low.x, L.cap.x) && approx(L.cap_low.w, L.cap.w));

    /* --- pressed: dy_px = 3 (lroundf(2.5) rounds away from zero); a press
     * TRANSLATES -- every rect/circle is bit-identical to the unpressed one,
     * the offset lives only in dy_px. --- */
    synthui_led_button_layout_t P;
    assert(synthui_led_button_compute_layout(100.0f, 100.0f, true, &P));
    assert(P.dy_px == 3);
    assert(rect_eq(&P.bezel, &L.bezel) && rect_eq(&P.well, &L.well));
    assert(rect_eq(&P.cap, &L.cap) && rect_eq(&P.cap_top, &L.cap_top) && rect_eq(&P.cap_low, &L.cap_low));
    assert(rect_eq(&P.highlight, &L.highlight) && rect_eq(&P.halo, &L.halo) && rect_eq(&P.led, &L.led));
    assert(rect_eq(&P.base, &L.base));
    assert(circ_eq(&P.dot1, &L.dot1) && circ_eq(&P.dot2, &L.dot2));

    /* --- 96x96 pressed: dy_px = lroundf(2.4) = 2 (the case that split the
     * layers under the old float-dy design) --- */
    synthui_led_button_layout_t Qz;
    assert(synthui_led_button_compute_layout(96.0f, 96.0f, true, &Qz));
    assert(Qz.dy_px == 2);

    /* --- scaling: 200x200 doubles everything; rects stay at dy = 0, dy_px
     * scales to 5 --- */
    synthui_led_button_layout_t D;
    assert(synthui_led_button_compute_layout(200.0f, 200.0f, true, &D));
    assert(approx(D.s, 2.0f) && D.dy_px == 5);
    assert(approx(D.led.x, 52) && approx(D.led.y, 38) && approx(D.led.w, 96) && approx(D.led.h, 30));
    assert(approx(D.cap_r, 22) && approx(D.dot1.r, 3.4f));

    /* --- border widths in whole px --- */
    assert(approx(L.s, 1.0f));   /* L is still the 100x100 unpressed layout */
    assert(L.bezel_bw_px == 2 && L.cue_bw_px == 4 && L.halo_bw_px == 10);
    synthui_led_button_layout_t W;
    assert(synthui_led_button_compute_layout(32.0f, 32.0f, false, &W));
    assert(W.bezel_bw_px == 1 && W.cue_bw_px == 1 && W.halo_bw_px == 3);

    /* the 1 px floor itself: at 16x16 the raw bezel width rounds to 0
     * (lroundf(2.0*0.16)=lroundf(0.32)=0) and must be floored to 1; cue and
     * halo are already >= 1 without the floor at this size (lroundf(0.56)=1,
     * lroundf(1.6)=2).  8x8 floors even harder (lroundf(0.16)=0). */
    synthui_led_button_layout_t Bx16;
    assert(synthui_led_button_compute_layout(16.0f, 16.0f, false, &Bx16));
    assert(Bx16.bezel_bw_px == 1 && Bx16.cue_bw_px == 1 && Bx16.halo_bw_px == 2);
    synthui_led_button_layout_t Bx8;
    assert(synthui_led_button_compute_layout(8.0f, 8.0f, false, &Bx8));
    assert(Bx8.bezel_bw_px == 1);

    /* --- non-square: an 80 px key centred with side margins on whichever
     * axis is longer --- */
    synthui_led_button_layout_t N;
    assert(synthui_led_button_compute_layout(120.0f, 80.0f, false, &N));
    assert(approx(N.s, 0.8f) && approx(N.ox, 20.0f) && approx(N.oy, 0.0f));
    assert(approx(N.bezel.x, 20) && approx(N.bezel.w, 80) && approx(N.bezel.h, 80));
    assert(approx(N.led.x, 20 + 26 * 0.8f));

    synthui_led_button_layout_t M;
    assert(synthui_led_button_compute_layout(80.0f, 120.0f, false, &M));
    assert(approx(M.s, 0.8f) && approx(M.ox, 0.0f) && approx(M.oy, 20.0f));
    assert(approx(M.bezel.y, 20));
    assert(approx(M.led.y, 20 + 19 * 0.8f));

    /* --- dots drop out below 34 px --- */
    synthui_led_button_layout_t S;
    assert(synthui_led_button_compute_layout(33.0f, 33.0f, false, &S));
    assert(!S.dots_visible);
    assert(synthui_led_button_compute_layout(34.0f, 34.0f, false, &S));
    assert(S.dots_visible);
    assert(synthui_led_button_compute_layout(120.0f, 33.0f, false, &S));   /* min side rules */
    assert(!S.dots_visible);

    /* --- damage boxes, pixel space, at 100x100 --- */
    synthui_led_button_px_t lb;
    synthui_led_button_lit_box(100.0f, 100.0f, false, &lb);
    assert(lb.x1 == 21 && lb.y1 == 14 && lb.x2 == 78 && lb.y2 == 38);
    synthui_led_button_lit_box(100.0f, 100.0f, true, &lb);
    assert(lb.x1 == 21 && lb.y1 == 17 && lb.x2 == 78 && lb.y2 == 41);

    synthui_led_button_px_t pb;
    synthui_led_button_press_box(100.0f, 100.0f, &pb);
    assert(pb.x1 == 10 && pb.y1 == 9 && pb.x2 == 89 && pb.y2 == 91);

    /* --- colour table --- */
    assert(synthui_led_button_color_on(SYNTHUI_LED_BUTTON_RED)   == 0xFF3B30u);
    assert(synthui_led_button_color_off(SYNTHUI_LED_BUTTON_RED)  == 0x5E2B28u);
    assert(synthui_led_button_color_on(SYNTHUI_LED_BUTTON_AMBER) == 0xFFA41Fu);
    assert(synthui_led_button_color_off(SYNTHUI_LED_BUTTON_AMBER)== 0x5C3D18u);
    assert(synthui_led_button_color_on(SYNTHUI_LED_BUTTON_GREEN) == 0x4BE060u);
    assert(synthui_led_button_color_off(SYNTHUI_LED_BUTTON_GREEN)== 0x254A2Bu);
    assert(synthui_led_button_color_on(SYNTHUI_LED_BUTTON_BLUE)  == 0x5AA8FFu);
    assert(synthui_led_button_color_off(SYNTHUI_LED_BUTTON_BLUE) == 0x22384Fu);
    assert(synthui_led_button_color_on((synthui_led_button_color_t)99) == 0xFF3B30u);   /* out of range -> red */

    /* --- palette --- */
    synthui_led_button_palette_t p;
    synthui_led_button_palette(SYNTHUI_LED_BUTTON_RED, false, false, false, false, &p);
    assert(p.cap_top == 0xF7F5F1u && p.cap_mid == 0xE8E6E1u && p.cap_low == 0xC9C7C1u);
    assert(p.highlight_opa == 140 && p.base_opa == 217);
    assert(p.led_fill == 0x5E2B28u && !p.halo_on);
    assert(p.bezel_color == 0x3A3A3Du);

    synthui_led_button_palette(SYNTHUI_LED_BUTTON_AMBER, true, false, false, false, &p);
    assert(p.led_fill == 0xFFA41Fu && p.halo_on && p.halo_color == 0xFFA41Fu);

    synthui_led_button_palette(SYNTHUI_LED_BUTTON_RED, true, true, false, false, &p);
    assert(p.cap_top == 0xDEDCD7u && p.cap_mid == 0xCBC9C3u && p.cap_low == 0xB4B2ADu);
    assert(p.highlight_opa == 71 && p.base_opa == 128);
    assert(p.halo_on);                                   /* pressed keeps the halo */

    synthui_led_button_palette(SYNTHUI_LED_BUTTON_RED, false, false, true, false, &p);
    assert(p.bezel_color == 0xFF3B30u);

    synthui_led_button_palette(SYNTHUI_LED_BUTTON_GREEN, true, false, true, true, &p);
    assert(p.cap_top == 0xE2E1DEu && p.cap_mid == 0xD2D1CEu && p.cap_low == 0xBCBBB8u);
    assert(p.led_fill == 0x4A4A4Cu && !p.halo_on);       /* disabled: neutral LED, no halo, even when lit */
    assert(p.highlight_opa == 140 && p.base_opa == 217);  /* disabled is not pressed */
    assert(p.bezel_color == 0xFF3B30u);                  /* cue still shows on a disabled key */

    synthui_led_button_palette(SYNTHUI_LED_BUTTON_RED, true, true, false, true, &p);
    assert(p.cap_top == 0xE2E1DEu && p.highlight_opa == 71 && p.base_opa == 128);   /* disabled + pressed: disabled cap, pressed opacities */

    /* --- degenerate sizes --- */
    assert(!synthui_led_button_compute_layout(0.0f, 100.0f, false, &L));
    assert(!synthui_led_button_compute_layout(100.0f, -1.0f, false, &L));

    synthui_led_button_px_t zb;
    synthui_led_button_lit_box(0.0f, 100.0f, false, &zb);
    assert(zb.x1 == 0 && zb.y1 == 0 && zb.x2 == 0 && zb.y2 == 0);
    synthui_led_button_press_box(0.0f, 100.0f, &zb);
    assert(zb.x1 == 0 && zb.y1 == 0 && zb.x2 == 0 && zb.y2 == 0);

    printf("led_button_test: all PASS\n");
    return 0;
}
