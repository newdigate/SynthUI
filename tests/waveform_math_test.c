/* Host-compiled unit test for the waveform widget's pure maths -- no LVGL, no
 * target.  Build: tests/run.sh
 * Copyright (c) 2026 Nicholas Newdigate
 * SPDX-License-Identifier: MIT */
#undef NDEBUG
#include "../src/synthui_waveform_math.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int near(double a, double b, double tol) { return fabs(a - b) <= tol; }

int main(void)
{
    /* --- which peak table: the coarsest that still has a point per pixel --- */
    static const uint32_t fpp[3] = { 128, 2048, 32768 };
    assert(synthui_wave_pick_table(100.0, fpp, 3) == 0);    /* closer than the finest: finest */
    assert(synthui_wave_pick_table(128.0, fpp, 3) == 0);
    assert(synthui_wave_pick_table(2047.0, fpp, 3) == 0);
    assert(synthui_wave_pick_table(2048.0, fpp, 3) == 1);
    assert(synthui_wave_pick_table(40000.0, fpp, 3) == 2);
    assert(synthui_wave_pick_table(40000.0, fpp, 0) == 0);

    /* --- zoom limits: finest table .. whole track across the lane --- */
    const double total = 13230000.0;                        /* 5 min at 44.1 kHz */
    assert(near(synthui_wave_zoom_clamp(50.0, total, 1104, 128), 128.0, 1e-9));
    assert(near(synthui_wave_zoom_clamp(1e9, total, 1104, 128), total / 1104.0, 1e-6));
    assert(near(synthui_wave_zoom_clamp(5000.0, total, 1104, 128), 5000.0, 1e-9));
    assert(near(synthui_wave_zoom_clamp(1e9, 0.0, 1104, 128), 128.0, 1e-9));      /* no track */
    assert(near(synthui_wave_zoom_clamp(1e9, 1000.0, 1104, 128), 128.0, 1e-9));   /* shorter than a lane */
    assert(near(synthui_wave_zoom_clamp(1e9, total, 0, 128), 128.0, 1e-9));       /* no lane */
    assert(near(synthui_wave_zoom_clamp(NAN, total, 1104, 128), 128.0, 1e-9));

    /* --- view start: tracking centres the head; free view is clamped --- */
    assert(near(synthui_wave_view_start(0.0, 1104, 128.0, 1, 99999.0, total), -70656.0, 1e-9));
    assert(near(synthui_wave_view_start(1e6, 1104, 128.0, 1, 0.0, total), 1e6 - 70656.0, 1e-9));
    assert(near(synthui_wave_view_start(1e6, 1104, 128.0, 0, 5e5, total), 5e5, 1e-9));
    assert(near(synthui_wave_view_start(1e6, 1104, 128.0, 0, -1e9, total), -552.0 * 128.0, 1e-9));
    assert(near(synthui_wave_view_start(1e6, 1104, 128.0, 0, 1e12, total), total - 552.0 * 128.0, 1e-9));

    /* --- pixel <-> frame --- */
    srand(1);
    for (int i = 0; i < 1000; i++) {
        const double vs = (double)(rand() % 1000000) - 100000.0, z = 128.0 + (double)(rand() % 20000);
        const int x = rand() % 1104;
        assert(abs(synthui_wave_frame_to_x(synthui_wave_x_to_frame(x, vs, z), vs, z) - x) <= 1);
    }
    assert(synthui_wave_frame_to_x(-500.0, 0.0, 128.0) < 0);
    assert(synthui_wave_frame_to_x(1e9, 0.0, 128.0) >= 1104);
    assert(synthui_wave_frame_to_x(1e30, 0.0, 128.0) > 0 && synthui_wave_frame_to_x(-1e30, 0.0, 128.0) < 0);  /* no int overflow */

    /* --- one column from a run of points; L and R share the lane --- */
    static const int8_t st[8] = { -10, 20, -30, 5,   -1, 40, -2, 3 };
    int8_t mn = 0, mx = 0;
    assert(synthui_wave_column(st, 2, 2, 0, 1, &mn, &mx) == 1 && mn == -30 && mx == 40);
    assert(synthui_wave_column(st, 2, 2, 1, 1, &mn, &mx) == 1 && mn == -2 && mx == 40);
    assert(synthui_wave_column(st, 2, 2, -5, -1, &mn, &mx) == 0);          /* before the data */
    assert(synthui_wave_column(st, 2, 2, 2, 9, &mn, &mx) == 0);            /* after it */
    assert(synthui_wave_column(st, 2, 2, 0, 99, &mn, &mx) == 1 && mn == -30 && mx == 40);   /* clamped */
    assert(synthui_wave_column(st, 2, 2, 1, 0, &mn, &mx) == 0);            /* empty range */
    static const int8_t mono[6] = { -7, 9,  -50, 2,  -1, 100 };
    assert(synthui_wave_column(mono, 3, 1, 1, 2, &mn, &mx) == 1 && mn == -50 && mx == 100);
    assert(synthui_wave_column(NULL, 3, 1, 0, 2, &mn, &mx) == 0);
    assert(synthui_wave_column(mono, 0, 1, 0, 2, &mn, &mx) == 0);

    /* --- column extent in pixels: up is positive, never less than 1 px --- */
    int y0 = -1, y1 = -1;
    synthui_wave_column_px(-128, 127, 340, &y0, &y1);
    assert(y0 >= 0 && y0 <= 1 && y1 >= 338 && y1 <= 339);
    synthui_wave_column_px(0, 0, 340, &y0, &y1);
    assert(y0 == 170 && y1 == 170);
    synthui_wave_column_px(0, 64, 340, &y0, &y1);                         /* a positive peak is drawn ABOVE the centre */
    assert(y1 == 170 && y0 < 170 && y0 >= 80 && y0 <= 90);
    synthui_wave_column_px(-128, 127, 0, &y0, &y1);                       /* degenerate lane */
    assert(y0 == 0 && y1 == 0);

    /* --- beat grid thinning: lines at least 6 px apart --- */
    assert(synthui_wave_beat_stride(12.0) == 1);
    assert(synthui_wave_beat_stride(6.0) == 1);
    assert(synthui_wave_beat_stride(5.9) == 2);
    assert(synthui_wave_beat_stride(1.6) == 4);
    {
        const uint32_t s = synthui_wave_beat_stride(0.01);
        assert((s & (s - 1)) == 0 && (double)s * 0.01 >= 6.0 && (double)(s / 2) * 0.01 < 6.0);
    }
    assert(synthui_wave_beat_stride(0.0) >= 1 && synthui_wave_beat_stride(-3.0) >= 1 && synthui_wave_beat_stride(NAN) >= 1);

    /* --- grab: finger right = earlier in the track; clamped to the track --- */
    assert(near(synthui_wave_grab_frame(1000.0, 10, 128.0, total), 0.0, 1e-9));
    assert(near(synthui_wave_grab_frame(100000.0, -10, 128.0, total), 101280.0, 1e-9));
    assert(near(synthui_wave_grab_frame(100000.0, 10, 128.0, total), 98720.0, 1e-9));
    assert(near(synthui_wave_grab_frame(total - 5.0, -100, 128.0, total), total - 1.0, 1e-9));
    assert(near(synthui_wave_grab_frame(500.0, -10, 128.0, 0.0), 0.0, 1e-9));   /* no track */

    printf("waveform_math_test: all assertions passed\n");
    return 0;
}
