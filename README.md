# SynthUI

Synth control-surface widgets for LVGL on the i.MX RT1176 / MIMXRT1170-EVKB —
knobs, faders, lamps, LED buttons, level meters, piano keys, seven-segment
displays, toggles — for the synth firmware in the rt1176-evkb tree (acid bass
voice, transport, step sequencer).

Status: reference material plus three widgets — `src/synthui_rotary_knob`
(two-engine: LVGL-sw + optional GC355 compositor in `src/vglite/`),
`src/synthui_step`, and `src/synthui_fader` (LVGL-sw, delta rendering).
Drag math is host-tested in `tests/`, run via `tests/run.sh`.

## Layout

- `reference/dc/` — the DC component set: 9 leaf components and 8 sheets
  (`.dc.html`), the generated `dc-runtime` `support.js`, and the sprite art
  the sheets embed. The flat layout is load-bearing: sheets reference art by
  bare relative path (one sheet uses `uploads/strip.png`).
There is no `reference/rebirth/` here. ReBirth RB-338 material recovered from
the 1997–2000 mod archive (single mod `mellow.rbm`) was part of this repo while
it was local-only, and was **removed from history before the first push**: its
rights are unclear, the MIT licence below cannot speak for it, and publishing it
would have done precisely what the provenance rules in the next section exist to
prevent. It is kept as a local design reference and `.gitignore`d, so a
reappearing `reference/rebirth/` is a deliberate decision, not a restoration.

## Provenance rules

- `reference/` is design reference. It is **never compiled**.
- Nothing under `reference/` may be converted into C arrays, fonts, or
  sprite data in `src/` without an explicit rights decision recorded here
  first. This bit hardest for the local ReBirth material above — recovered
  community mod art, rights unclear — which is why it is not in this repo.
- Clean-room vector rebuilds from *written descriptions* are the intended
  path — the same firewall discipline as the rt1176-evkb tree's license audit.
  the `src/` widgets are built that way and derive from the DC set, not from
  the ReBirth material.
- The MIT LICENSE covers this repo's own content: the DC component set,
  `support.js`, and all `src/` code.

## Relationship to the evkb tree

Sibling library under `$TEENSY_LIB_ROOT` (default `~/Development`), like
`LVGL`, `Audio`, and `MipiDisplay`. Imported with `import_evkb_synthui()` and
pinned by SHA in `evkb.cmake`; resolution is local-first, so a checkout here
wins over the pin. The consuming example is `examples/display/synthui_knob_test`,
whose QEMU gate records one golden checksum per knob mode.

Design spec: rt1176-evkb
`docs/superpowers/specs/2026-08-15-synthui-repo-design.md`.
