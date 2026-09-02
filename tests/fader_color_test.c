/* Host-compiled unit test for the fader's GPU colour packing -- no LVGL, no
 * vg_lite, no target.  Build: tests/run.sh
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT
 *
 * ★ WHY THIS EXISTS. The premultiply in synthui_fd_abgr_a is REQUIRED by
 * measured silicon behaviour (this GC355's VG_LITE_BLEND_SRC_OVER is the
 * PREMULTIPLIED operator S + D*(1-Sa) -- see the header). Nothing else in the
 * tree can catch its removal: the QEMU gate runs the SOFTWARE engine, so it
 * is blind to this code path, and the GPU goldens live only in a hardware
 * transcript that needs a hand-pressed reset. Without this file the property
 * is guarded by nobody, and "simplifying" the packing back to an unscaled
 * pack would go unnoticed until someone looked at the glass. */
#undef NDEBUG          /* assertions must survive a -DNDEBUG build; BEFORE every
                        * include, so no header can pull in assert.h first */
#include "../src/synthui_fader_color.h"
#include <assert.h>
#include <stdio.h>

/* The packing as it was BEFORE the premultiply fix. Present so the test can
 * assert the two DISAGREE where it matters -- an arm that proves the pin
 * fires, rather than a test equally happy with the defect. */
static uint32_t unscaled_pack(uint32_t hex, uint32_t a)
{
    return (a << 24) | ((hex & 0xFFu) << 16) | (hex & 0xFF00u)
           | ((hex >> 16) & 0xFFu);
}

int main(void)
{
    int checks = 0;

    /* 1. CHANNEL ORDER: ABGR -- red in the LOW byte, alpha in the high one.
     *    Opaque pure red must land at 0xFF0000FF, not 0xFFFF0000. */
    assert(synthui_fd_abgr_a(0xFF0000u, 0xFFu) == 0xFF0000FFu); checks++;
    assert(synthui_fd_abgr_a(0x00FF00u, 0xFFu) == 0xFF00FF00u); checks++;
    assert(synthui_fd_abgr_a(0x0000FFu, 0xFFu) == 0xFFFF0000u); checks++;

    /* 2. THE OPAQUE PATH IS UNTOUCHED. Eight of the fader's ten call sites
     *    pass a=255; if premultiplying moved any of them, every opaque
     *    golden in the widget would shift for no reason. Exhaustive over the
     *    red axis and strided over the other two. */
    for (uint32_t r = 0; r < 256; r++)
        for (uint32_t g = 0; g < 256; g += 17)
            for (uint32_t b = 0; b < 256; b += 17) {
                const uint32_t hex = (r << 16) | (g << 8) | b;
                assert(synthui_fd_abgr_a(hex, 0xFFu) == unscaled_pack(hex, 0xFFu));
                checks++;
            }

    /* 3. ENDPOINTS ARE EXACT. a=255 must be the identity (that is what makes
     *    check 2 hold) and a=0 must annihilate -- no off-by-one drift from
     *    the rounding term at either rail. */
    for (uint32_t v = 0; v < 256; v++) {
        assert(synthui_fd_pm(v, 255u) == v); checks++;
        assert(synthui_fd_pm(v, 0u) == 0u);  checks++;
    }

    /* 4. THE PIN FIRES. The three partial-alpha draws in the compositor must
     *    DIFFER from the unscaled packing -- otherwise this whole file is
     *    consistent with the fix having been reverted. */
    const struct { uint32_t hex, a; } partial[] = {
        { 0x1B1F22u, 115u },   /* cap shadow */
        { 0xFFFFFFu, 191u },   /* gloss, enabled  (synthui_fader.cpp:189) */
        { 0xFFFFFFu,  77u },   /* gloss, disabled (synthui_fader.cpp:189) */
    };
    for (unsigned i = 0; i < 3; i++) {
        assert(synthui_fd_abgr_a(partial[i].hex, partial[i].a)
               != unscaled_pack(partial[i].hex, partial[i].a));
        checks++;
    }

    /* 5. THE ACTUAL VALUES. White at opa 191 must premultiply to 191 in every
     *    colour channel -- the saturating 0xFF..FF is precisely the defect
     *    that made the gloss read as a blown highlight instead of a sheen. */
    assert(synthui_fd_abgr_a(0xFFFFFFu, 191u) == 0xBFBFBFBFu); checks++;
    assert(synthui_fd_abgr_a(0xFFFFFFu,  77u) == 0x4D4D4D4Du); checks++;
    assert(synthui_fd_abgr_a(0x1B1F22u, 115u) == 0x730F0E0Cu); checks++;

    /* 6. ALPHA IS PRESERVED VERBATIM. The hardware's alpha row is
     *    Sa + Da*(1-Sa); scaling the alpha byte here would double-apply it. */
    for (uint32_t a = 0; a < 256; a++) {
        assert((synthui_fd_abgr_a(0x808080u, a) >> 24) == a); checks++;
    }

    /* 7. NO CHANNEL EXCEEDS ITS ALPHA. A premultiplied colour with a channel
     *    above its own alpha is not representable and would clip on the
     *    hardware; this is the invariant that distinguishes premultiplied
     *    from unscaled packing across the whole domain. */
    for (uint32_t a = 0; a < 256; a += 5)
        for (uint32_t v = 0; v < 256; v += 5) {
            assert(synthui_fd_pm(v, a) <= a); checks++;
        }

    printf("fader_color: all PASS (%d checks)\n", checks);
    return 0;
}
