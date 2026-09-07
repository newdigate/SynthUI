#!/bin/sh
# Builds and runs the host test suite; exits non-zero on any failure.
# Copyright (c) 2026 Nicholas Newdigate
# SPDX-License-Identifier: MIT
set -eu
cd "$(dirname "$0")/.."
# mktemp, not a fixed /tmp name: two checkouts (or two concurrent runs) would
# otherwise race on one binary, and a fixed path writes outside the repo.
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT INT TERM HUP
cc -Wall -Wextra -Werror -o "$out/knob_math_test" tests/knob_math_test.c
"$out/knob_math_test"
cc -Wall -Wextra -Werror -o "$out/rotary_palette_test" tests/rotary_palette_test.c
"$out/rotary_palette_test"
cc -Wall -Wextra -Werror -o "$out/fader_math_test" tests/fader_math_test.c
"$out/fader_math_test"
cc -Wall -Wextra -Werror -o "$out/fader_color_test" tests/fader_color_test.c
"$out/fader_color_test"
cc -Wall -Wextra -Werror -o "$out/lamp_test" tests/lamp_test.c
"$out/lamp_test"
cc -Wall -Wextra -Werror -o "$out/panel_button_test" tests/panel_button_test.c
"$out/panel_button_test"
cc -Wall -Wextra -Werror -o "$out/seven_segment_test" tests/seven_segment_test.c
"$out/seven_segment_test"
c++ -std=c++17 -Wall -Wextra -Werror -o "$out/level_meter_test" tests/level_meter_test.cpp
"$out/level_meter_test"

