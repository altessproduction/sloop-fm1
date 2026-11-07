/* SPDX-License-Identifier: GPL-3.0-only
 * ROOTS DUB monophonic siren instrument.
 *
 * A playable note drives a pitch sweep and tempo-independent LFO. The normal
 * track delay/reverb sends provide the dub echo path, so the siren is part of
 * the firmware mixer and sequencer rather than a browser-only oscillator.
 */
static const char *const N_SIREN_WAVE[] = {"SINE", "TRI", "SAW", "PULSE"};

static void siren_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[0] = 0;
    v->ph[1] = 0;
    v->s[0] = 1 << 24;           /* sweep envelope, Q24 */
    v->s[1] = v->s[2] = 0;      /* resonant SVF history */
}

static void siren_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t i, ph = v->ph[0], lfo = v->ph[1];
    uint32_t lfo_inc = 10000u + (uint32_t)p[P_E3] * 7000u;
    uint32_t sweep_samples = FS / 10u + ((uint32_t)p[P_E2] * FS * 4u) / 127u;
    int32_t sweep = v->s[0], ic1 = v->s[1], ic2 = v->s[2];
    int32_t sweep_step = (int32_t)((1u << 24) / sweep_samples);
    int32_t sweep_semi = p[P_E1] * 16;
    int32_t tune = p[P_E0] * 16;
    int32_t lfo_depth = (p[P_E4] * 192) / 127; /* up to +/- 12 semitones */
    tsvf_t filter;
    if (sweep_step < 1)
        sweep_step = 1;
    tsvf_coef(&filter, p[P_E5] << 8, p[P_E6]);
    for (i = 0; i < n; i++) {
        int32_t lfo_pitch = (osc_sine(lfo) * lfo_depth) >> 15;
        int32_t sweep_q15 = sweep >> 9;
        int32_t pitch = clamp(m->pitch16 + tune + ((sweep_semi * sweep_q15) >> 15) + lfo_pitch, 0, 2047);
        uint32_t inc = pitch_inc((uint32_t)pitch);
        int32_t x, y;
        switch ((uint32_t)p[P_E7]) {
        case 1: x = osc_tri(ph); break;
        case 2: x = osc_saw(ph, inc); break;
        case 3: x = osc_pulse(ph, inc, 0x80000000u); break;
        default: x = osc_sine(ph); break;
        }
        y = tsvf_lp(&filter, x, &ic1, &ic2);
        out[i] += voice_amp(y, m, i);
        ph += inc;
        lfo += lfo_inc;
        sweep = sweep > sweep_step ? sweep - sweep_step : 0;
    }
    v->ph[0] = ph;
    v->ph[1] = lfo;
    v->s[0] = sweep;
    v->s[1] = ic1;
    v->s[2] = ic2;
}

/* E0 tune, E1 sweep depth, E2 sweep time, E3 LFO rate, E4 LFO depth,
 * E5 filter cutoff, E6 resonance, E7 waveform. Echo is the track DLY send. */
static const preset_t SIREN_PRESETS[] = {
    {"DUB SIREN", {0, 24, 78, 22, 42, 112, 78, 0}, {0, 80, 100, 24}, 0, 1, FX(0, 0, 82, 22)},
    {"AIR HORN", {0, 7, 28, 0, 8, 120, 28, 3}, {0, 55, 100, 24}, 0, 1, FX(0, 0, 48, 12)},
    {"FALLING DUB", {0, -24, 92, 16, 24, 96, 68, 1}, {0, 72, 100, 32}, 0, 1, FX(0, 0, 92, 28)},
    {"RISING TONE", {0, 30, 110, 8, 18, 88, 55, 2}, {0, 90, 100, 36}, 0, 1, FX(0, 0, 76, 30)},
    {"WOBBLE SIREN", {0, 12, 54, 82, 96, 78, 90, 0}, {0, 80, 100, 28}, 0, 1, FX(0, 0, 100, 35)},
};

static const engine_t ENG_SIREN = {
    .name = "SIREN",
    .page_title = {"PITCH", "MOD"},
    .edit = {
        {"TUNE", F_SEMI, -24, 24, 0, 0, 0},
        {"SWEEP", F_SEMI, -48, 48, 0, 0, 0},
        {"TIME", F_TIME, 0, 127, 78, 0, 0},
        {"LFO", F_LFOHZ, 0, 127, 22, 0, 0},
        {"DEPTH", F_PCT, 0, 127, 42, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 112, 0, 0},
        {"RES", F_PCT, 0, 127, 78, 0, 0},
        {"WAVE", F_ENUM, 0, 3, 0, N_SIREN_WAVE, 0},
    },
    .presets = SIREN_PRESETS,
    .npresets = NELEM(SIREN_PRESETS),
    .fil_page = 0,
    .note_on = siren_note_on,
    .render = siren_render,
    .color = 0xD6A5,
    .macro = {P_E1, P_E2, P_E3, P_E4},
    .poly = 1,
};
