/* synthui_rotary_palette.h - RotaryKnob.dc.html THEME/state mapping, pure.
 * LVGL-free and header-only so a host compiler can unit-test it and both the
 * sw draw and the GPU compositor share one source of truth.
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#ifndef SYNTHUI_ROTARY_PALETTE_H
#define SYNTHUI_ROTARY_PALETTE_H
#include <stdint.h>

/* set_accent() sentinel: "no accent, use the theme's index color". */
#define SYNTHUI_ROTARY_ACCENT_DEFAULT 0xFFFFFFFFu

typedef struct {
    uint32_t well, well_stroke, body, inner, index;
} synthui_rotary_palette_t;

/* Hexes verbatim from RotaryKnob.dc.html renderVals() (fetched 2026-08-27).
 * disabled wins; active/focus apply only when not disabled (the DC state enum
 * is exclusive, LVGL states are not -- same resolution the old knob used).
 * Note two DC subtleties this preserves: accent is IGNORED when disabled
 * (index -> indexOff), and the focus ring is the THEME index color, never the
 * accent (renderVals uses base.index for wellStroke). */
static inline void synthui_rotary_palette(int dark, int disabled, int active,
                                          int focus, uint32_t accent,
                                          synthui_rotary_palette_t *p)
{
    if (disabled) { active = 0; focus = 0; }
    if (!dark) {
        p->well        = disabled ? 0xe4e4eau : 0xdcdce6u;
        p->well_stroke = focus    ? 0xfcfbf6u : 0xb6b8ccu;
        p->body        = disabled ? 0x9a9caeu : (active ? 0x31356fu : 0x282b60u);
        p->inner       = disabled ? 0xa6a8b8u : (active ? 0x3d4283u : 0x333871u);
        p->index       = disabled ? 0xdcdce6u
                                  : (accent != SYNTHUI_ROTARY_ACCENT_DEFAULT
                                         ? accent : 0xfcfbf6u);
    } else {
        p->well        = disabled ? 0x101016u : 0x14141cu;
        p->well_stroke = focus    ? 0xffd24au : 0x34344au;
        p->body        = disabled ? 0x2a2a36u : (active ? 0x464c88u : 0x3c4176u);
        p->inner       = disabled ? 0x32323fu : (active ? 0x565da4u : 0x4a5090u);
        p->index       = disabled ? 0x55555fu
                                  : (accent != SYNTHUI_ROTARY_ACCENT_DEFAULT
                                         ? accent : 0xffd24au);
    }
}
#endif /* SYNTHUI_ROTARY_PALETTE_H */
