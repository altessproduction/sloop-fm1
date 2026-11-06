/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * ROOTS DUB LR4 four-band isolator, fixed 44.1 kHz / Q30.
 */
#ifndef ROOTS_LR4_ISOLATOR_H
#define ROOTS_LR4_ISOLATOR_H
#include <stdint.h>

#define ROOTS_LR4_Q30 INT64_C(1073741824)
#define ROOTS_LR4_MAX 1048575
#define ROOTS_LR4_MAX_Q8 (ROOTS_LR4_MAX * 256)

typedef struct {
    int32_t x1, x2, y1, y2; /* Q8 audio states */
} roots_lr4_biquad_state_t;

typedef struct {
    roots_lr4_biquad_state_t lp[3][2], hp[3][2];
    roots_lr4_biquad_state_t allpass[3]; /* SUB@350, SUB@2500, BASS@2500 */
    int32_t gain[4]; /* Q15, unity = 32768 */
    uint16_t wet; /* Q15 dry/wet transition, 64-sample ramp */
    uint8_t enabled, running;
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
    return x > ROOTS_LR4_MAX_Q8 ? ROOTS_LR4_MAX_Q8 :
           x < -ROOTS_LR4_MAX_Q8 ? -ROOTS_LR4_MAX_Q8 : (int32_t)x;
}

/* Direct form I: Q30 coefficients, Q8 states, 64-bit accumulator. */
static inline int32_t roots_lr4_biquad(int32_t x,
        roots_lr4_biquad_state_t *s, const roots_lr4_coeff_t *c)
{
    int64_t acc = (int64_t)c->b0*x + (int64_t)c->b1*s->x1 +
                  (int64_t)c->b2*s->x2 - (int64_t)c->a1*s->y1 -
                  (int64_t)c->a2*s->y2;
    int32_t y = roots_lr4_sat(acc >> 30); /* remain in Q8 between sections */
    s->x2=s->x1; s->x1=x; s->y2=s->y1; s->y1=y;
    return y;
}

static const roots_lr4_coeff_t roots_lr4_ap_coeff[3] = {
    {1048091221,-2121522935,1073741824,-2121522935,1048091221},
    {1000627836,-2071791055,1073741824,-2071791055,1000627836},
    {648969343,-1614580579,1073741824,-1614580579,648969343}
};

/* Sum of a fourth-order Linkwitz-Riley LP/HP pair is one second-order all-pass. */
static inline int32_t roots_lr4_allpass(int32_t x,
        roots_lr4_biquad_state_t *state, const roots_lr4_coeff_t *c)
{
    return roots_lr4_biquad(x,state,c);
}

static inline void roots_lr4_init(roots_lr4_t *s)
{
    unsigned i, j;
    for (i=0;i<3;i++) {
        for (j=0;j<2;j++) {
            s->lp[i][j].x1=s->lp[i][j].x2=s->lp[i][j].y1=s->lp[i][j].y2=0;
            s->hp[i][j].x1=s->hp[i][j].x2=s->hp[i][j].y1=s->hp[i][j].y2=0;
        }
        s->allpass[i].x1=s->allpass[i].x2=s->allpass[i].y1=s->allpass[i].y2=0;
    }
    s->gain[0]=s->gain[1]=s->gain[2]=s->gain[3]=32768;
    s->wet=0; s->enabled=0; s->running=0;
}

/* One instance per channel. Bypass is bit-identical at rest. Enable/disable
 * crossfades over 64 samples; re-enable starts with clean filter histories. */
static inline int32_t roots_lr4_process(roots_lr4_t *s, int32_t input)
{
    int32_t input_q8=roots_lr4_sat((int64_t)input * 256);
    int32_t rest=input_q8, bands[4], filtered, out;
    int64_t mix=0;
    unsigned i,j;
    if (!s->enabled && s->wet == 0u) { s->running=0u; return input; }
    if (!s->running) {
        for (i=0;i<3;i++) {
            for (j=0;j<2;j++) {
                s->lp[i][j].x1=s->lp[i][j].x2=s->lp[i][j].y1=s->lp[i][j].y2=0;
                s->hp[i][j].x1=s->hp[i][j].x2=s->hp[i][j].y1=s->hp[i][j].y2=0;
            }
            s->allpass[i].x1=s->allpass[i].x2=s->allpass[i].y1=s->allpass[i].y2=0;
        }
        s->running=1u;
    }
    rest=input_q8;
    for (i=0;i<3;i++) {
        int32_t lo=rest, hi=rest;
        for (j=0;j<2;j++) {
            lo=roots_lr4_biquad(lo,&s->lp[i][j],&roots_lr4_lp_coeff[i]);
            hi=roots_lr4_biquad(hi,&s->hp[i][j],&roots_lr4_hp_coeff[i]);
        }
        bands[i]=lo;
        rest=hi;
    }
    bands[3]=rest;
    bands[0]=roots_lr4_allpass(bands[0],&s->allpass[0],&roots_lr4_ap_coeff[1]);
    bands[0]=roots_lr4_allpass(bands[0],&s->allpass[1],&roots_lr4_ap_coeff[2]);
    bands[1]=roots_lr4_allpass(bands[1],&s->allpass[2],&roots_lr4_ap_coeff[2]);
    for (i=0;i<4;i++) mix+=(int64_t)bands[i]*s->gain[i];
    filtered=roots_lr4_sat(mix >> 15);
    if (s->enabled) s->wet = s->wet > 32255u ? 32768u : (uint16_t)(s->wet + 512u);
    else s->wet = s->wet < 512u ? 0u : (uint16_t)(s->wet - 512u);
    if (s->wet == 0u) { s->running=0u; return input; }
    out=roots_lr4_sat(((int64_t)input_q8*(32768u-s->wet) + (int64_t)filtered*s->wet) >> 15);
    return out >> 8;
}
#endif
