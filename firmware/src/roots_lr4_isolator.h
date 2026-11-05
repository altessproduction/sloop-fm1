/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * ROOTS DUB LR4 four-band isolator, fixed 44.1 kHz / Q30.
 */
#ifndef ROOTS_LR4_ISOLATOR_H
#define ROOTS_LR4_ISOLATOR_H
#include <stdint.h>
#include <string.h>

#define ROOTS_LR4_Q30 INT64_C(1073741824)
#define ROOTS_LR4_MAX 131071

typedef struct {
    int32_t x1, x2, y1, y2;
} roots_lr4_biquad_state_t;

typedef struct {
    roots_lr4_biquad_state_t lp[2], hp[2];
    int32_t gain[4]; /* Q15, unity = 32768 */
    uint8_t enabled;
} roots_lr4_t;

/* Normalized bilinear Butterworth biquad coefficients, Q30, 44.1 kHz.
 * Rows are b0,b1,b2,a1,a2. a1/a2 are subtracted in the recurrence.
 */
typedef struct { int32_t b0,b1,b2,a1,a2; } roots_lr4_coeff_t;
static const roots_lr4_coeff_t roots_lr4_lp_coeff[3] = {
    {77527,155055,77527,-2121522935,1048091221},
    {644651,1289302,644651,-2071791055,1000627836},
    {27032647,54065294,27032647,-1614580579,648969343}
};
static const roots_lr4_coeff_t roots_lr4_hp_coeff[3] = {
    {1060838995,-2121677990,1060838995,-2121522935,1048091221},
    {1036540179,-2073080358,1036540179,-2071791055,1000627836},
    {834322936,-1668645873,834322936,-1614580579,648969343}
};

static inline int32_t roots_lr4_sat(int64_t x)
{
    return x > ROOTS_LR4_MAX ? ROOTS_LR4_MAX :
           x < -ROOTS_LR4_MAX ? -ROOTS_LR4_MAX : (int32_t)x;
}

/* Direct form I: 64-bit accumulator, Q30 coefficients, integer audio states. */
static inline int32_t roots_lr4_biquad(int32_t x,
        roots_lr4_biquad_state_t *s, const roots_lr4_coeff_t *c)
{
    int64_t acc = (int64_t)c->b0*x + (int64_t)c->b1*s->x1 +
                  (int64_t)c->b2*s->x2 - (int64_t)c->a1*s->y1 -
                  (int64_t)c->a2*s->y2;
    int32_t y = roots_lr4_sat(acc >> 30);
    s->x2=s->x1; s->x1=x; s->y2=s->y1; s->y1=y;
    return y;
}

static inline void roots_lr4_init(roots_lr4_t *s)
{
    memset(s,0,sizeof(*s));
    s->gain[0]=s->gain[1]=s->gain[2]=s->gain[3]=32768;
}

/* One instance per channel. Bypass is bit-identical and freezes all states. */
static inline int32_t roots_lr4_process(roots_lr4_t *s, int32_t input)
{
    int32_t rest=input, bands[4];
    int64_t mix=0;
    unsigned i,j;
    if (!s->enabled) return input;
    rest=roots_lr4_sat(rest);
    for (i=0;i<3;i++) {
        int32_t lo=rest, hi=rest;
        for (j=0;j<2;j++) {
            lo=roots_lr4_biquad(lo,&s->lp[i].lp[j],&roots_lr4_lp_coeff[i]);
            hi=roots_lr4_biquad(hi,&s->hp[i].hp[j],&roots_lr4_hp_coeff[i]);
        }
        bands[i]=lo;
        rest=hi;
    }
    bands[3]=rest;
    for (i=0;i<4;i++) mix+=(int64_t)bands[i]*s->gain[i];
    return roots_lr4_sat(mix >> 15);
}
#endif
