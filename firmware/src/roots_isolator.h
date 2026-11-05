/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * ROOTS DUB standalone fixed-point four-band isolator.
 */
#ifndef ROOTS_ISOLATOR_H
#define ROOTS_ISOLATOR_H
#include <stdint.h>
#include <string.h>
#define ROOTS_ISO_Q15 32768
#define ROOTS_ISO_MAX 131071
typedef struct {
    int32_t lp1[3], lp2[3];
    int16_t gain[4];
    uint8_t enabled;
} roots_iso_t;
/* alpha = 1-exp(-2*pi*fc/44100), Q15; split points 120, 350, 2500 Hz. */
static const int16_t roots_iso_alpha[3] = {1110, 1588, 9560};
static inline int32_t roots_iso_clamp_sample(int32_t x)
{
    return x > ROOTS_ISO_MAX ? ROOTS_ISO_MAX : x < -ROOTS_ISO_MAX ? -ROOTS_ISO_MAX : x;
}
static inline int32_t roots_iso_lp(int32_t x, int32_t *s, int32_t a)
{
    int64_t d = (int64_t)x - *s;
    *s += (int32_t)((d * a) >> 15);
    return *s;
}
static inline void roots_iso_init(roots_iso_t *s)
{
    memset(s, 0, sizeof(*s));
    s->gain[0] = s->gain[1] = s->gain[2] = s->gain[3] = ROOTS_ISO_Q15;
}
/* Disabled means bit-identical bypass; filter state is not advanced. */
static inline int32_t roots_iso_process(roots_iso_t *s, int32_t input)
{
    int32_t bands[4], x, low;
    int64_t sum = 0;
    unsigned i;
    if (!s->enabled) return input;
    x = roots_iso_clamp_sample(input);
    for (i = 0; i < 3; ++i) {
        low = roots_iso_lp(x, &s->lp1[i], roots_iso_alpha[i]);
        low = roots_iso_lp(low, &s->lp2[i], roots_iso_alpha[i]);
        bands[i] = low;
        x -= low;
    }
    bands[3] = x;
    for (i = 0; i < 4; ++i) sum += (int64_t)bands[i] * s->gain[i];
    sum >>= 15;
    if (sum > ROOTS_ISO_MAX) sum = ROOTS_ISO_MAX;
    if (sum < -ROOTS_ISO_MAX) sum = -ROOTS_ISO_MAX;
    return (int32_t)sum;
}
#endif
