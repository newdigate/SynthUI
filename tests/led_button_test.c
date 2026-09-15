/* led_button_test.c - host unit test for the pure LedButton layout, damage
 * boxes and palette (synthui_led_button_math.h).
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT
 *
 * Every guard here was shown RED against a mutant before it was trusted:
 *   press box ignoring dy      -> "press box must cover the base at both offsets"
 *   lit box omitting the halo  -> "lit box must contain the halo"
 *   cap split at 0.50          -> "cap split sits at 62 %"
 *   dots threshold at 32       -> "dots drop out below 34 px" */
#undef NDEBUG
#include "../src/synthui_led_button_math.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int approx(float a, float b) { return fabsf(a - b) < 0.01f; }

static int rect_contains(const synthui_led_button_rect_t *outer,
                         const synthui_led_button_rect_t *inner)
{
    return inner->x >= outer->x - 0.001f && inner->y >= outer->y - 0.001f &&
           inner->x + inner->w <= outer->x + outer->w + 0.001f &&
           inner->y + inner->h <= outer->y + outer->h + 0.001f;
}

int main(void)
{
    synthui_led_button_layout_t L;

    /* --- 100x100, unpressed: the DC box in pixels, 1 unit = 1 px --- */
    assert(synthui_led_button_compute_layout(100.0f, 100.0f, false, &L));
    assert(approx(L.s, 1.0f) && approx(L.ox, 0.0f) && approx(L.oy, 0.0f));
    assert(approx(L.dy, 0.0f));
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
    assert(approx(L.bezel_r, 17) && approx(L.well_r, 13) && approx(L.cap_r, 11));
    assert(approx(L.highlight_r, 5.5f) && approx(L.halo_r, 8.5f) && approx(L.led_r, 3.5f) && approx(L.base_r, 3.5f));

    /* cap split sits at 62 % and the two halves tile the cap exactly */
    assert(approx(L.cap_top.y, 9) && approx(L.cap_top.h, 49.6f));
    assert(approx(L.cap_low.y, 58.6f) && approx(L.cap_low.h, 30.4f));
    assert(approx(L.cap_top.y + L.cap_top.h, L.cap_low.y));
    assert(approx(L.cap_top.h + L.cap_low.h, L.cap.h));
    assert(approx(L.cap_top.x, L.cap.x) && approx(L.cap_top.w, L.cap.w));
    assert(approx(L.cap_low.x, L.cap.x) && approx(L.cap_low.w, L.cap.w));

    /* --- pressed: dy = 2.5, only the cap group moves --- */
    synthui_led_button_layout_t P;
    assert(synthui_led_button_compute_layout(100.0f, 100.0f, true, &P));
    assert(approx(P.dy, 2.5f));
    assert(approx(P.bezel.y, L.bezel.y) && approx(P.well.y, L.well.y));
    assert(approx(P.cap.y, 11.5f) && approx(P.cap_top.y, 11.5f) && approx(P.cap_low.y, 61.1f));
    assert(approx(P.highlight.y, 14.5f) && approx(P.led.y, 21.5f) && approx(P.halo.y, 16.5f));
    assert(approx(P.base.y, 84.5f) && approx(P.dot1.cy, 29.0f) && approx(P.dot2.cy, 29.0f));

    /* --- scaling: 200x200 doubles everything, 2.5 units -> 5 px --- */
    synthui_led_button_layout_t D;
    assert(synthui_led_button_compute_layout(200.0f, 200.0f, true, &D));
    assert(approx(D.s, 2.0f) && approx(D.dy, 5.0f));
    assert(approx(D.led.x, 52) && approx(D.led.y, 43) && approx(D.led.w, 96) && approx(D.led.h, 30));
    assert(approx(D.cap_r, 22) && approx(D.dot1.r, 3.4f));

    /* --- non-square 120x80: an 80 px key centred with 20 px side margins --- */
    synthui_led_button_layout_t N;
    assert(synthui_led_button_compute_layout(120.0f, 80.0f, false, &N));
    assert(approx(N.s, 0.8f) && approx(N.ox, 20.0f) && approx(N.oy, 0.0f));
    assert(approx(N.bezel.x, 20) && approx(N.bezel.w, 80) && approx(N.bezel.h, 80));
    assert(approx(N.led.x, 20 + 26 * 0.8f));

    /* --- dots drop out below 34 px --- */
    synthui_led_button_layout_t S;
    assert(synthui_led_button_compute_layout(33.0f, 33.0f, false, &S));
    assert(!S.dots_visible);
    assert(synthui_led_button_compute_layout(34.0f, 34.0f, false, &S));
    assert(S.dots_visible);
    assert(synthui_led_button_compute_layout(120.0f, 33.0f, false, &S));   /* min side rules */
    assert(!S.dots_visible);

    /* --- damage boxes --- */
    /* lit box == halo box, at the CURRENT press offset, and contains the LED */
    synthui_led_button_rect_t lb;
    synthui_led_button_lit_box(100.0f, 100.0f, false, &lb);
    assert(approx(lb.x, 21) && approx(lb.y, 14) && approx(lb.w, 58) && approx(lb.h, 25));
    assert(rect_contains(&lb, &L.led) && rect_contains(&lb, &L.halo));
    synthui_led_button_lit_box(100.0f, 100.0f, true, &lb);
    assert(approx(lb.y, 16.5f));
    assert(rect_contains(&lb, &P.led) && rect_contains(&lb, &P.halo));
    /* press box covers every moving layer at BOTH offsets, and lies in the key */
    synthui_led_button_rect_t pb;
    synthui_led_button_press_box(100.0f, 100.0f, &pb);
    assert(approx(pb.x, 10) && approx(pb.y, 9) && approx(pb.w, 80) && approx(pb.h, 82.5f));
    assert(rect_contains(&pb, &L.cap) && rect_contains(&pb, &P.cap));
    assert(rect_contains(&pb, &L.highlight) && rect_contains(&pb, &P.highlight));
    assert(rect_contains(&pb, &L.halo) && rect_contains(&pb, &P.halo));
    assert(rect_contains(&pb, &L.base) && rect_contains(&pb, &P.base));   /* "press box must cover the base at both offsets" */
    assert(rect_contains(&L.bezel, &pb));
    synthui_led_button_press_box(200.0f, 200.0f, &pb);
    assert(approx(pb.h, 165.0f));

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
    assert(p.bezel_color == 0x3A3A3Du && approx(p.bezel_w_units, 2.0f));

    synthui_led_button_palette(SYNTHUI_LED_BUTTON_AMBER, true, false, false, false, &p);
    assert(p.led_fill == 0xFFA41Fu && p.halo_on && p.halo_color == 0xFFA41Fu);

    synthui_led_button_palette(SYNTHUI_LED_BUTTON_RED, true, true, false, false, &p);
    assert(p.cap_top == 0xDEDCD7u && p.cap_mid == 0xCBC9C3u && p.cap_low == 0xB4B2ADu);
    assert(p.highlight_opa == 71 && p.base_opa == 128);
    assert(p.halo_on);                                   /* pressed keeps the halo */

    synthui_led_button_palette(SYNTHUI_LED_BUTTON_RED, false, false, true, false, &p);
    assert(p.bezel_color == 0xFF3B30u && approx(p.bezel_w_units, 3.5f));

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

    printf("led_button_test: all PASS\n");
    return 0;
}
