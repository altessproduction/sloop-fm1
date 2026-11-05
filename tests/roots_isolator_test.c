/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "roots_isolator.h"
#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL: %s\\n", m); return 1; } } while (0)
int main(void)
{
    roots_iso_t iso;
    int32_t x, y;
    unsigned i;
    roots_iso_init(&iso);
    for (i = 0; i < 1000; ++i) {
        x = (int32_t)(i * 7919u) - 3950000;
        CHECK(roots_iso_process(&iso, x) == x, "bypass bit-identical");
    }
    iso.enabled = 1;
    for (i = 0; i < 20000; ++i) {
        x = (i & 1u) ? 12000 : -12000;
        y = roots_iso_process(&iso, x);
        CHECK(y <= ROOTS_ISO_MAX && y >= -ROOTS_ISO_MAX, "output bounded");
    }
    roots_iso_init(&iso); iso.enabled = 1;
    for (i = 0; i < 50000; ++i) y = roots_iso_process(&iso, 10000);
    CHECK(abs(y - 10000) <= 2, "unity gains preserve settled DC");
    roots_iso_init(&iso); iso.enabled = 1;
    iso.gain[0] = iso.gain[1] = iso.gain[2] = iso.gain[3] = 0;
    for (i = 0; i < 1000; ++i)
        CHECK(roots_iso_process(&iso, (i & 1u) ? 20000 : -20000) == 0, "all bands muted");
    roots_iso_init(&iso); iso.enabled = 1;
    iso.gain[0] = iso.gain[1] = iso.gain[2] = 0; iso.gain[3] = 32767;
    for (i = 0; i < 50000; ++i) y = roots_iso_process(&iso, 10000);
    CHECK(abs(y) <= 10, "TOPS rejects settled DC");
    roots_iso_init(&iso); iso.enabled = 1;
    for (i = 0; i < 50000; ++i) {
        x = (i & 1u) ? 131071 : -131071;
        y = roots_iso_process(&iso, x);
        CHECK(y <= ROOTS_ISO_MAX && y >= -ROOTS_ISO_MAX, "full-scale stress bounded");
    }
    puts("ROOTS DUB isolator: bypass, unity/DC, mute, band rejection and stress OK");
    return 0;
}
