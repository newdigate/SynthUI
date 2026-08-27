/* rotary_palette_test.c - pins the RotaryKnob.dc.html THEME/state mapping.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#include <stdio.h>
#include "../src/synthui_rotary_palette.h"

static int fails = 0;
#define CHECK(what, got, want) do { \
    if ((got) != (want)) { \
        printf("FAIL %s: got %06x want %06x\n", what, (unsigned)(got), (unsigned)(want)); \
        fails++; \
    } } while (0)

/* One row of the DC table: theme, state flags, accent in; five hexes out. */
static void row(const char *name, int dark, int dis, int act, int foc,
                uint32_t accent, uint32_t well, uint32_t stroke,
                uint32_t body, uint32_t inner, uint32_t index_)
{
    synthui_rotary_palette_t p;
    synthui_rotary_palette(dark, dis, act, foc, accent, &p);
    char buf[64];
    snprintf(buf, sizeof buf, "%s well", name);   CHECK(buf, p.well, well);
    snprintf(buf, sizeof buf, "%s stroke", name); CHECK(buf, p.well_stroke, stroke);
    snprintf(buf, sizeof buf, "%s body", name);   CHECK(buf, p.body, body);
    snprintf(buf, sizeof buf, "%s inner", name);  CHECK(buf, p.inner, inner);
    snprintf(buf, sizeof buf, "%s index", name);  CHECK(buf, p.index, index_);
}

int main(void)
{
    const uint32_t N = SYNTHUI_ROTARY_ACCENT_DEFAULT;
    /* light */
    row("l/idle",     0,0,0,0, N, 0xdcdce6,0xb6b8cc, 0x282b60,0x333871,0xfcfbf6);
    row("l/active",   0,0,1,0, N, 0xdcdce6,0xb6b8cc, 0x31356f,0x3d4283,0xfcfbf6);
    row("l/focus",    0,0,0,1, N, 0xdcdce6,0xfcfbf6, 0x282b60,0x333871,0xfcfbf6);
    row("l/disabled", 0,1,0,0, N, 0xe4e4ea,0xb6b8cc, 0x9a9cae,0xa6a8b8,0xdcdce6);
    row("l/accent",   0,0,0,0, 0xffd24a, 0xdcdce6,0xb6b8cc, 0x282b60,0x333871,0xffd24a);
    /* accent is IGNORED when disabled (renderVals: off ? indexOff : accent) */
    row("l/acc+dis",  0,1,0,0, 0xffd24a, 0xe4e4ea,0xb6b8cc, 0x9a9cae,0xa6a8b8,0xdcdce6);
    /* focus ring is the THEME index, not the accent (renderVals: base.index) */
    row("l/acc+foc",  0,0,0,1, 0x5be0a0, 0xdcdce6,0xfcfbf6, 0x282b60,0x333871,0x5be0a0);
    /* dark */
    row("d/idle",     1,0,0,0, N, 0x14141c,0x34344a, 0x3c4176,0x4a5090,0xffd24a);
    row("d/active",   1,0,1,0, N, 0x14141c,0x34344a, 0x464c88,0x565da4,0xffd24a);
    row("d/focus",    1,0,0,1, N, 0x14141c,0xffd24a, 0x3c4176,0x4a5090,0xffd24a);
    row("d/disabled", 1,1,0,0, N, 0x101016,0x34344a, 0x2a2a36,0x32323f,0x55555f);
    /* disabled wins over active AND focus (DC state is exclusive; LVGL is not) */
    row("l/dis+all",  0,1,1,1, 0xff6a52, 0xe4e4ea,0xb6b8cc, 0x9a9cae,0xa6a8b8,0xdcdce6);
    if (fails) { printf("%d FAILURES\n", fails); return 1; }
    printf("rotary_palette_test: all rows match the DC table\n");
    return 0;
}
