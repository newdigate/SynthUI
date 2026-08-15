# SynthUI

Synth control-surface widgets for LVGL on the i.MX RT1176 / MIMXRT1170-EVKB —
knobs, faders, lamps, LED buttons, level meters, piano keys, seven-segment
displays, toggles — for the synth firmware in the rt1176-evkb tree (acid bass
voice, transport, step sequencer).

Status: reference material plus the first widget (`src/synthui_knob`, v1:
rendering only, no touch).

## Layout

- `reference/dc/` — the DC component set: 9 leaf components and 8 sheets
  (`.dc.html`), the generated `dc-runtime` `support.js`, and the sprite art
  the sheets embed. The flat layout is load-bearing: sheets reference art by
  bare relative path (one sheet uses `uploads/strip.png`).
- `reference/rebirth/` — ReBirth RB-338 material recovered from the
  1997–2000 mod archive (single mod `mellow.rbm`): the component guide and
  its companion images.

## Provenance rules

- `reference/` is design reference. It is **never compiled**.
- Nothing under `reference/` may be converted into C arrays, fonts, or
  sprite data in `src/` without an explicit rights decision recorded here
  first. This bites hardest for `reference/rebirth/`: recovered community
  mod art, rights unclear.
- Clean-room vector rebuilds from the guide's *written descriptions* are the
  intended path — the same firewall discipline as the rt1176-evkb tree's
  license audit.
- The MIT LICENSE covers this repo's own content: the DC component set,
  `support.js`, and all future `src/` code. It does not speak for
  `reference/rebirth/`.

## Relationship to the evkb tree

Sibling library under `$TEENSY_LIB_ROOT` (default `~/Development`), like
`LVGL`, `Audio`, and `MipiDisplay`. Local-only for now: no remote, and no
`evkb.cmake` import macro until the first consuming example.

Design spec: rt1176-evkb
`docs/superpowers/specs/2026-08-15-synthui-repo-design.md`.
