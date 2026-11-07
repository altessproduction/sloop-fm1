/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SLOOP menu (HOME held), in sections as the pages are (2.4): SCREEN (COLOR, ZOOM, KNOB ACCEL (2.7)), LIGHTS (LIGHTS, KEYS, NOTES),
 * AUDIO (LOWCUT, USB AUDIO, USB SERIAL (FELUCCA_CDC builds only), USB RATE (2.7 beta)), SEQ (2.7.1: STEP ADVANCE, DRUM PREVIEW), SYSTEM (2ND CORE (dualcore), HARDWARE CALIBRATION, LEARN CLEAR (2.6), ABOUT).
 * SELECT goes to the section before / after (stopping at the ends), KNOB 1..4 set the section's settings in order (the knob's colour marks
 * its row), PRESETS moves the cursor; OCT+ steps the cursor's setting round or opens it (CALIBRATION, ABOUT),
 * OCT- closes (from ABOUT: back to the section). */
/* ------------------------------------------------------------ menu --- */
/* ROOTS DUB project controls: native FM-1 menu, no external editor. */
extern void proj_roots_get(uint8_t *enabled, uint8_t gain[4]);
extern void proj_roots_set(uint8_t enabled, const uint8_t gain[4]);
extern void roots_dub_pattern_apply(uint8_t style);
extern void roots_dub_bass_apply(uint8_t style);
extern void roots_dub_skank_apply(uint8_t style);
extern void roots_dub_select_siren(void);
/* (2.7: USB SERIAL only in a build with the serial console, FELUCCA_CDC=1: the FM-1's build has none) */
enum { MI_COLOR, MI_ZOOM, MI_ACCEL, MI_LIGHTS, MI_KEYS, MI_NOTES, MI_LOWCUT, MI_USB,
#if FELUCCA_CDC
       MI_SERIAL,
#endif
       MI_RATE, MI_FRESO, MI_ADV, MI_DPREV, MI_CPU1, MI_PANEL, MI_LCLEAR, MI_ABOUT, MI_ROOTS_SUB, MI_ROOTS_BASS, MI_ROOTS_MID, MI_ROOTS_HIGH, MI_DUB_STYLE, MI_DUB_APPLY, MI_SIREN_SWEEP, MI_SIREN_TIME, MI_SIREN_RATE, MI_SIREN_DEPTH, MI_BASS_STYLE, MI_BASS_APPLY, MI_DUB_CHORD, MI_SKANK_STYLE, MI_SKANK_APPLY, MI_COUNT };
static const char *const MI_NAME[MI_COUNT] = {"COLOR", "ZOOM", "KNOB ACCEL", "LIGHTS", "KEYS", "NOTES", "SPEAKER LOWCUT", "USB AUDIO",
#if FELUCCA_CDC
                                              "USB SERIAL",
#endif
                                              "USB RATE", "FILTER RESO", "STEP ADVANCE", "DRUM PREVIEW", "2ND CORE", "HARDWARE CALIBRATION", "MIDI LEARN CLEAR", "ABOUT", "SUB", "BASS", "MID", "HIGH", "STYLE", "LOAD PATTERN", "SWEEP", "TIME", "LFO RATE", "LFO DEPTH", "BASS STYLE", "LOAD BASS", "SKANK CHORD", "SKANK STYLE", "LOAD SKANK"};
enum { MS_SCREEN, MS_LIGHTS, MS_AUDIO, MS_SEQ, MS_SYSTEM, MS_ROOTS, MS_DUBPAT, MS_SIREN, MS_DUBBASS, MS_SKANK, MS_COUNT };   /* (2.7.1: SEQ) */
static const char *const MS_NAME[MS_COUNT] = {"SCREEN", "LIGHTS", "AUDIO", "SEQ", "SYSTEM", "ROOTS", "DUB PAT", "SIREN", "DUB BASS", "SKANK"};   /* (AUDIO: and USB) */
static const uint8_t MS_FIRST[MS_COUNT + 1] = {MI_COLOR, MI_LIGHTS, MI_LOWCUT, MI_ADV, MI_CPU1, MI_ROOTS_SUB, MI_DUB_STYLE, MI_SIREN_SWEEP, MI_BASS_STYLE, MI_DUB_CHORD, MI_COUNT};   /* rows of each */

UI_SMALL static int mi_cpu1_have(void)                      /* the second core is running (cpu1.c; the host tests have no cpu1.c) */
{
#if FELUCCA_CPU1
    return cpu1_state == CPU1_OK;
#else
    return 0;
#endif
}
static const char *const LIGHTS_NAME[LIGHTS_N] = {"OFF", "LOW", "MID", "HIGH"};   /* every button lit, the labels readable */
static uint8_t dub_pattern_style, dub_bass_style, dub_skank_style;
/* SIREN page macros: K1 sweep depth (E1), K2 sweep time (E2), K3 LFO rate (E3), K4 LFO depth (E4). */
static const char *const DUB_PATTERN_NAME[10] = {"ONE DROP", "STEPPERS", "ROCKERS", "ROCKSTEADY", "RUB A DUB", "DUB BREAK", "NYABINGHI", "SKA", "EARLY REGGAE", "HALF TIME"};
static const char *const DUB_BASS_NAME[8] = {"ROOTS", "WALKING", "STEPPERS", "ROCKERS", "ONE DROP", "DUB WALK", "ROCKERS 2", "SKA BASS"};
static const char *const DUB_SKANK_NAME[6] = {"OFFBEAT", "DOUBLE", "ROCKERS", "HALF STEP", "UPSTROKE", "SKA CHOP"};
static const char *const DUB_CHORD_NAME[10] = {"OFF", "TRIAD", "7TH", "9TH", "SUS4", "POWER", "SKANK", "SK7", "SK9", "SKSUS"};
static const char *const KEYS_NAME[KEYS_N] = {"OFF", "C KEYS", "WHITE KEYS", "ALL KEYS"};      /* keys lit too, at the LIGHTS level */
#define MI_Y0 26                                   /* the first row, under the section tabs */
#define MI_DY 50                                   /* a row: the label, the value in large type */

UI_SMALL static uint32_t mi_sec(uint32_t i)                 /* the section of item i */
{
    uint32_t k = 0;
    while (k + 1u < MS_COUNT && i >= MS_FIRST[k + 1u])
        k++;
    return k;
}
UI_SMALL static uint32_t mi_row(uint32_t i) { return i - MS_FIRST[mi_sec(i)]; }   /* its row: KNOB 1 + row sets it */

/* item i's value as shown, and its colour */
UI_SMALL static const char *mi_value(uint32_t i, uint16_t *c)
{
    *c = C_HI;
    switch (i) {
    case MI_DUB_STYLE: return DUB_PATTERN_NAME[dub_pattern_style % 10u];
    case MI_DUB_APPLY: return "OCT+ TO LOAD";
    case MI_SIREN_SWEEP: case MI_SIREN_TIME: case MI_SIREN_RATE: case MI_SIREN_DEPTH: {
        static char b[16];
        if (is_drum(TSEL)) { *c = C_DIM; return "SELECT SYNTH"; }
        const char *unit;
        uint32_t k = i - MI_SIREN_SWEEP;
        const param_desc_t *d = &ENGINES[ENGI_SIREN]->edit[k + 1u];
        param_format(d, TSEL->p[P_E1 + k], b, &unit);
        if (unit[0]) {
            uint32_t n = str_len(b);
            str_cpy(b + n, " ", sizeof b - n);
            n = str_len(b);
            str_cpy(b + n, unit, sizeof b - n);
        }
        return b;
    }
    case MI_DUB_CHORD: return DUB_CHORD_NAME[(uint32_t)clamp(TSEL->p[P_CHORD], 0, 9)];
    case MI_BASS_STYLE: return DUB_BASS_NAME[dub_bass_style % 8u];
    case MI_BASS_APPLY: return "OCT+ TO LOAD";
    case MI_SKANK_STYLE: return DUB_SKANK_NAME[dub_skank_style % 6u];
    case MI_SKANK_APPLY: return "OCT+ TO LOAD";
    case MI_ROOTS_SUB: case MI_ROOTS_BASS: case MI_ROOTS_MID: case MI_ROOTS_HIGH: {
        static char b[5]; uint8_t enabled, gain[4];
        proj_roots_get(&enabled, gain); *c = enabled ? C_HI : C_DIM;
        fmt_int(b, gain[i - MI_ROOTS_SUB]); return b;
    }
    case MI_COLOR:                                    /* (2.7) your theme, by its name */
        if (theme_shown()) {
            static char b[9];
            uint32_t k;
            for (k = 0; k < 8u && theme.name[k]; k++)
                b[k] = theme.name[k];
            b[k] = 0;
            return k ? b : "MY THEME";
        }
        return PALETTES[settings.palette % NPALETTES].name;
    case MI_ZOOM: return settings.zoom ? "ON" : "OFF";
    case MI_ACCEL: return knob_accel_off ? "OFF" : "ON";   /* (2.7) the knobs speed up with a fast turn */
    case MI_ADV: return step_adv_off ? "OFF" : "ON";      /* (2.7.1) the STEP page's cursor moves on when the keys are let go */
    case MI_DPREV: return drum_preview ? "ON" : "OFF";     /* (2.7.1) a drum sound picked with a knob is heard */
    case MI_LIGHTS: return LIGHTS_NAME[lights_lvl % LIGHTS_N];
    case MI_KEYS:
        *c = lights_lvl ? C_HI : C_DIM;               /* (needs LIGHTS) */
        return KEYS_NAME[lights_keys % KEYS_N];
    case MI_NOTES: return lights_notes ? "ON" : "OFF";
    case MI_LOWCUT: return settings.lowcut ? "ON" : "OFF";
    case MI_USB: return usb_full ? "FULL" : "MASTER";
#if FELUCCA_CDC
    case MI_SERIAL: return usb_serial ? "ON" : "OFF";
#endif
    case MI_RATE: {                                   /* (2.7 beta) the rates the USB audio input offers (usb.c) */
        static const char *const R[3] = {"AUTO", "44.1 KHZ", "48 KHZ"};
        return R[usb_rate % 3u];
    }
    case MI_FRESO: {                                  /* (2.7.2) the track filter's resonance, 0 (clean) .. 10 (fx.c) */
        static char b[4];
        if (!flt_reso) {
            *c = C_DIM;
            return "OFF";
        }
        fmt_int(b, flt_reso > 10u ? 10 : flt_reso);
        return b;
    }
    case MI_CPU1:                                     /* the render on both cores (fx.c); live, the sound is the same */
        *c = mi_cpu1_have() ? C_HI : C_DIM;
        return cpu1_off ? "OFF" : "ON";
    case MI_LCLEAR: {                                 /* (2.6) the CCs MIDI LEARN holds; OCT+ clears them */
        static char b[8];
        uint32_t n = ml_count();
        if (!n) {
            *c = C_DIM;
            return "NONE";
        }
        fmt_int(b, (int32_t)n);
        str_cpy(b + str_len(b), " CC", 4);
        return b;
    }
    default:
        *c = C_DIM;
        return "";                                    /* (an action: OCT+ opens it) */
    }
}

UI_SMALL static void draw_menu(void)
{
    uint32_t i, pass, sec = mi_sec(ui.menu_sel % MI_COUNT);
    uint32_t sig = ui.menu * 7u + ui.menu_sel * 131u + settings.palette * 1009u + settings.lowcut * 7919u +
                   settings.zoom * 104729u + lights_lvl * 1299709u + lights_keys * 15485863u + ml_count() * 2654435u +
                   lights_notes * 32452843u + usb_full * 49979687u + usb_serial * 86028121u + usb_rate * 15486719u + theme_shown() * 32452867u + theme.crc + cpu1_off * 122949829u +
                   knob_accel_off * 179424673u + drum_preview * 236887691u + step_adv_off * 373587883u + flt_reso * 512927357u + (uint32_t)TSEL->p[P_CHORD] * 32452867u + (uint32_t)TSEL->eng_req * 49979687u;
    { uint8_t re, rg[4]; proj_roots_get(&re, rg); sig += re * 977u + rg[0] * 101u + rg[1] * 103u + rg[2] * 107u + rg[3] * 109u; }
    if (!ui.force && sig == ui.menu_sig)
        return;
    ui.menu_sig = sig;
    if (ui.force)                                   /* head + rule + two bands cover rows 0..229 */
        lcd_fill(0, H_HEAD + 1 + 124 + 95, 240, 240 - (H_HEAD + 1 + 124 + 95), C_BLACK);
    cv_begin(240, H_HEAD, C_BLACK);
    cv_text(4, 1, &FONT_S, ui.menu == 2 ? "ABOUT" : "MENU", C_HI);
    if (ui.menu != 2) {                             /* the section and its number, as a page's "ENV DEST 2/2" */
        char t[16];
        str_cpy(t, MS_NAME[sec], sizeof t);
        str_cpy(t + str_len(t), " ", 2);
        fmt_int(t + str_len(t), (int32_t)sec + 1);
        str_cpy(t + str_len(t), "/", 2);
        fmt_int(t + str_len(t), MS_COUNT);
        cv_text(236 - text_w(&FONT_S, t), 1, &FONT_S, t, C_GRAY);
    }
    cv_blit(0, Y_HEAD);
    lcd_fill(0, H_HEAD, 240, 1, C_LINE);
    for (pass = 0; pass < 2u; pass++) {             /* the canvas holds 124 rows: draw in two bands */
        cv_begin(240, pass ? 95u : 124u, C_BLACK);
        cv_oy = pass ? -124 : 0;
        if (ui.menu == 2) {
            cv_text(4, 4, &FONT_L, "SLOOP", C_WHITE);
            cv_rect(96, 10, 8, 4, TE_COL[0]), cv_rect(96, 16, 12, 4, TE_COL[1]);   /* the sail */
            cv_rect(96, 22, 16, 4, TE_COL[2]), cv_rect(96, 28, 20, 4, TE_COL[3]);
            cv_text(4, 36, &FONT_S, "BASED ON FELUCCA", C_AMB);
            cv_text(4, 54, &FONT_S, FELUCCA_VERSION, C_HI);
            cv_text(236 - text_w(&FONT_S, __DATE__), 54, &FONT_S, __DATE__, C_GRAY);   /* build date */
            cv_text(cv_text(4, 72, &FONT_S, "LEO KUROSHITA", C_HI) + 8, 72, &FONT_S, "@KUROGEDELIC", C_AMB);
            cv_text(4, 88, &FONT_S, "H\xDCGELTON INSTRUMENTS", C_HI);   /* Latin-1 U-umlaut */
            cv_text(4, 104, &FONT_S, "HUGELTON.COM", C_AMB);
#if FELUCCA_CPU1
            {
                char sv[12];
                cpu1_supplies(sv);                  /* (2.7) SYSVDD, VDC14 now: 1.38V 1.60V (fm1_sys.h) */
                cv_text(236 - text_w(&FONT_S, sv), 104, &FONT_S, sv, C_GRAY);
            }
#endif
            cv_text(4, 119, &FONT_S, "GPL-3.0, NO WARRANTY", C_HI);
#if FELUCCA_CPU1                                    /* (the host tests include this file without cpu1.c) */
            cv_text(236 - text_w(&FONT_S, cpu1_label()), 119, &FONT_S, cpu1_label(), C_AMB);   /* 2 CORES / 1 CORE (cpu1.c) */
#endif
            cv_text(4, 132, &FONT_S, "GITHUB.COM/ISOD89/SLOOP-FM1", C_AMB);   /* (the source of this firmware) */
            cv_text(4, 146, &FONT_S, "FONT: TERMINUS (OFL)", C_DIM);
            cv_text(4, 159, &FONT_S, "SAMPLES: VERSILIAN (CC0)", C_DIM);
            cv_text(4, 172, &FONT_S, "+ SONIC PI (CC0)", C_DIM);
            cv_text(4, 185, &FONT_S, "PHASE: CRISPYZEBRA (GPL)", C_DIM);
            cv_text(4, 198, &FONT_S, "VOICE: REF. KLATTSCH (MIT)", C_DIM);
        } else {
            {   /* Keep the active tab visible as ROOTS DUB adds pages. The old
                 * fixed row overflowed the 240 px display with ten sections. */
                int32_t cw = text_w(&FONT_S, MS_NAME[sec]);
                int32_t cx = (240 - cw) / 2;
                cv_rect(cx - 3, 3, cw + 6, 18, C_LINE);
                cv_text(cx, 4, &FONT_S, MS_NAME[sec], C_WHITE);
                if (sec > 0u) {
                    int32_t pw = text_w(&FONT_S, MS_NAME[sec - 1u]);
                    cv_text(cx - pw - 12, 4, &FONT_S, MS_NAME[sec - 1u], C_DIM);
                }
                if (sec + 1u < MS_COUNT)
                    cv_text(cx + cw + 12, 4, &FONT_S, MS_NAME[sec + 1u], C_DIM);
            }
            for (i = MS_FIRST[sec]; i < MS_FIRST[sec + 1u]; i++) {
                uint32_t r = i - MS_FIRST[sec];
                int32_t y = MI_Y0 + (int32_t)r * MI_DY, xv;
                int cur = i == ui.menu_sel;
                uint16_t vc;
                const char *v = mi_value(i, &vc);
                char k[4] = {'K', (char)('1' + r), 0, 0};
                cv_rect(4, y + 2, 4, MI_DY - 8, cur ? C_WHITE : TE_COL[r & 3u]);   /* the knob's colour */
                xv = cv_text(14, y, &FONT_S, k, TE_COL[r & 3u]);
                cv_text(xv + 6, y, &FONT_S, MI_NAME[i], cur ? C_WHITE : C_GRAY);
                if (v[0] && text_w(&FONT_L, v) <= 222) {
                    xv = cv_text(14, y + 16, &FONT_L, v, cur && vc == C_HI ? C_WHITE : vc);
                } else if (v[0]) {                      /* (too wide for the large type: WHITE KEYS) */
                    xv = cv_text(14, y + 24, &FONT_S, v, cur && vc == C_HI ? C_WHITE : vc);
                } else {
                    xv = cv_text(14, y + 24, &FONT_S, "OCT+ OPENS", cur ? C_WHITE : C_DIM);
                }
                if (i == MI_COLOR) {                    /* the palette's colours */
                    uint32_t q;
                    for (q = 0; q < 5u; q++)
                        cv_rect(xv + 10 + (int32_t)q * 14, y + 28, 10, 10, pal[q]);
                }
                if (i == MI_RATE && usb_rate != usb_rate_now())
                    cv_text(xv + 8, y + 30, &FONT_S, "RESTART", C_AMB);   /* (usb.c: at the next start) */
#if FELUCCA_CDC
                if (i == MI_SERIAL && usb_serial != usb_cdc_now())
                    cv_text(xv + 8, y + 30, &FONT_S, "RESTART", C_AMB);   /* (usb.c: at the next start) */
#endif
                if (i == MI_CPU1 && !mi_cpu1_have())
                    cv_text(xv + 8, y + 30, &FONT_S, "NOT RUNNING", C_AMB);   /* (cpu1.c: ABOUT says 1 CORE) */
            }
            if (MS_FIRST[sec + 1u] - MS_FIRST[sec] <= 3u) {   /* (SYSTEM has four rows with 2ND CORE: its fourth
                                                               * row, ABOUT, takes the help's place) */
                cv_text(4, MI_Y0 + 3 * MI_DY + 6, &FONT_S, "SELECT SECTION  KNOB SETS", C_DIM);
                cv_text(4, MI_Y0 + 3 * MI_DY + 22, &FONT_S, "OCT+ OK   OCT- CLOSE", C_DIM);   /* (rows .. 218) */
            }
        }
        cv_oy = 0;
        cv_blit(0, H_HEAD + 1 + pass * 124u);
    }
}

UI_SMALL static void enc_drop(void)                             /* knob turns nobody takes */
{
    uint32_t k;
    for (k = 0; k < NE; k++)
        panel_enc(k);
}

UI_SMALL static void menu_close(void)
{
    if (song.playing || transport_req)
        settings_later = 1;                            /* (a flash write stops the audio: once stopped) */
    else
        settings_save();                               /* palette / panel table, if changed */
    ui.menu = 0;
    ui.force = 1;
    go_home();
}

/* item i: a knob turned s (> 0 right, < 0 left), or 0: OCT+ (steps round, toggles, or opens) */
UI_SMALL static void mi_set(uint32_t i, int32_t s)
{
    switch (i) {
    case MI_DUB_STYLE:
        dub_pattern_style = (uint8_t)((dub_pattern_style + (s < 0 ? 9u : 1u)) % 10u);
        ui.force = 1;
        break;
    case MI_DUB_APPLY:
        if (!s) {
            roots_dub_pattern_apply(dub_pattern_style);
            ui_message("DUB PATTERN LOADED");
            ui.force = 1;
        }
        break;
    case MI_BASS_STYLE:
        dub_bass_style = (uint8_t)((dub_bass_style + (s < 0 ? 7u : 1u)) % 8u); ui.force = 1; break;
    case MI_BASS_APPLY:
        if (!s) { if (is_drum(TSEL)) ui_message("SELECT A SYNTH TRACK"); else { roots_dub_bass_apply(dub_bass_style); ui_message("DUB BASS LOADED"); } ui.force = 1; }
        break;
    case MI_SKANK_STYLE:
        dub_skank_style = (uint8_t)((dub_skank_style + (s < 0 ? 5u : 1u)) % 6u); ui.force = 1; break;
    case MI_SKANK_APPLY:
        if (!s) { if (is_drum(TSEL)) ui_message("SELECT A SYNTH TRACK"); else { roots_dub_skank_apply(dub_skank_style); ui_message("DUB SKANK LOADED"); } ui.force = 1; }
        break;
    case MI_DUB_CHORD:
        if (is_drum(TSEL)) { ui_message("SELECT A SYNTH TRACK"); break; }
        if (TSEL->p[P_CHORD] < 6 || TSEL->p[P_CHORD] > 9)
            TSEL->p[P_CHORD] = (int16_t)(s < 0 ? 9 : 6);
        else
            TSEL->p[P_CHORD] = (int16_t)(6 + ((TSEL->p[P_CHORD] - 6 + (s < 0 ? 3 : 1)) % 4));
        ui.force = 1;
        break;
    case MI_SIREN_SWEEP: case MI_SIREN_TIME: case MI_SIREN_RATE: case MI_SIREN_DEPTH: {
        uint32_t k = i - MI_SIREN_SWEEP;
        uint32_t id = P_E1 + k;
        const param_desc_t *d = &ENGINES[ENGI_SIREN]->edit[k + 1u];
        int32_t v;
        if (!s)
            break;
        if (is_drum(TSEL)) { ui_message("SELECT A SYNTH TRACK"); break; }
        if (TSEL->eng_req != ENGI_SIREN)
            roots_dub_select_siren();
        v = clamp(TSEL->p[id] + accel(EN_K1 + k, s, d->max - d->min), d->min, d->max);
        TSEL->p[id] = (int16_t)v;
        ui.lock_par = (uint8_t)id;
        ui.force = 1;
        break;
    }
    case MI_ROOTS_SUB: case MI_ROOTS_BASS: case MI_ROOTS_MID: case MI_ROOTS_HIGH: {
        uint8_t enabled, gain[4]; uint32_t band = i - MI_ROOTS_SUB;
        proj_roots_get(&enabled, gain);
        gain[band] = (uint8_t)clamp((int32_t)gain[band] + (s > 0 ? 1 : s < 0 ? -1 : 0), 0, 127);
        proj_roots_set(enabled, gain); ui.force = 1; break;
    }
    case MI_COLOR: {                                   /* both ways round; OCT+ the next. 2.7: your theme after MONO
                                                        * (the palette number stays the last built-in one: an older
                                                        * SLOOP shows that) */
        uint32_t n = NPALETTES + (theme_valid ? 1u : 0u), at = theme_shown() ? NPALETTES : settings.palette % NPALETTES;
        at = (at + (s < 0 ? n - 1u : 1u)) % n;
        theme_on = (uint8_t)(at == NPALETTES);
        if (!theme_on)
            settings.palette = at;
        palette_set(settings.palette);
        ui.force = 1;
        break;
    }
    case MI_ACCEL:                                     /* (2.7) right ON, left OFF; OCT+ toggles (ui_input.c accel) */
        knob_accel_off = (uint8_t)(s > 0 ? 0u : s < 0 ? 1u : !knob_accel_off);
        break;
    case MI_ADV:                                       /* (2.7.1) right ON, left OFF; OCT+ toggles (ui_input.c seq_entry) */
        step_adv_off = (uint8_t)(s > 0 ? 0u : s < 0 ? 1u : !step_adv_off);
        break;
    case MI_DPREV:                                     /* (2.7.1) right ON, left OFF; OCT+ toggles (ui_studio.c drum_hear) */
        drum_preview = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !drum_preview);
        break;
    case MI_ZOOM:
    case MI_LOWCUT: {                                  /* right ON, left OFF; OCT+ toggles */
        uint32_t *v = i == MI_LOWCUT ? &settings.lowcut : &settings.zoom;
        *v = s > 0 ? 1u : s < 0 ? 0u : !*v;
        fx_lowcut = (uint8_t)(settings.lowcut != 0);
        break;
    }
    case MI_USB:                                       /* right FULL, left MASTER */
        usb_full = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !usb_full);
        break;
#if FELUCCA_CDC
    case MI_SERIAL:
        usb_serial = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !usb_serial);
        break;
#endif
    case MI_RATE:                                      /* (2.7 beta) AUTO 44.1 48: the knob steps (stops at the ends),
                                                        * OCT+ round; at the next start (usb.c usb_rate_mode) */
        if (s > 0 && usb_rate < 2u)
            usb_rate++;
        else if (s < 0 && usb_rate > 0u)
            usb_rate--;
        else if (!s)
            usb_rate = (uint8_t)((usb_rate + 1u) % 3u);
        break;
    case MI_FRESO:                                     /* (2.7.2) 0 (OFF/clean) .. 10, stops at the ends; OCT+ steps round */
        if (s > 0 && flt_reso < 10u)
            flt_reso++;
        else if (s < 0 && flt_reso > 0u)
            flt_reso--;
        else if (!s)
            flt_reso = (uint8_t)((flt_reso >= 10u ? 0u : flt_reso + 1u));
        break;
    case MI_CPU1:                                      /* right ON, left OFF; at once (the next block renders so) */
        cpu1_off = (uint8_t)(s > 0 ? 0u : s < 0 ? 1u : !cpu1_off);
        cpu1_units = cpu1_off ? 0 : -1;
        if (!cpu1_off && !cpu1_on())                   /* 2.7: core 1 starts only at power-on */
            ui_message("ON AT NEXT POWER-ON");
        break;
    case MI_NOTES:
        lights_notes = (uint8_t)(s > 0 ? 1u : s < 0 ? 0u : !lights_notes);
        break;
    case MI_LIGHTS:
    case MI_KEYS: {                                    /* brighter / dimmer (stops at the ends); OCT+ steps round */
        uint8_t *v = i == MI_LIGHTS ? &lights_lvl : &lights_keys;
        uint32_t n = i == MI_LIGHTS ? LIGHTS_N : KEYS_N;
        if (s > 0 && *v + 1u < n)
            (*v)++;
        else if (s < 0 && *v > 0u)
            (*v)--;
        else if (!s)
            *v = (uint8_t)((*v + 1u) % n);
        if (i == MI_KEYS && lights_keys && !lights_lvl)
            lights_lvl = LIGHTS_LOW;                   /* keys lit need a level: the lowest */
        break;
    }
    case MI_LCLEAR:                                    /* (2.6) every learned CC: OCT+ */
        if (!s)
            ui_message(ml_clear_all() ? "LEARN CLEARED" : "NOTHING LEARNED");
        break;
    case MI_PANEL:                                     /* actions: OCT+ only */
        if (!s) {
            panel_setup();
            ui.force = 1;
        }
        break;
    case MI_ABOUT:
        if (!s) {
            ui.menu = 2;
            ui.force = 1;
        }
        break;
    default:
        break;
    }
}

/* menu: SELECT the section, KNOB 1..3 its settings, PRESETS the cursor, OCT+ ok, OCT- close (ABOUT -> the section) */
UI_SMALL static void menu_input(uint32_t pressed)
{
    int32_t s;
    uint32_t k, ok = (pressed >> panel.btn[B_OCTUP]) & 1u, back = (pressed >> panel.btn[B_OCTDN]) & 1u;
    uint32_t sel = ui.menu_sel % MI_COUNT, sec = mi_sec(sel), n = MS_FIRST[sec + 1u] - MS_FIRST[sec];
    if (back) {
        if (ui.menu == 2)
            ui.menu = 1, ui.force = 1;
        else
            menu_close();
        return;
    }
    if (ui.menu != 1) {
        enc_drop();
        return;
    }
    if ((s = panel_enc(EN_SELECT)) != 0) {             /* the section before / after (stops at the ends) */
        int32_t to = clamp((int32_t)sec + (s > 0 ? 1 : -1), 0, MS_COUNT - 1);
        if ((uint32_t)to != sec) {
            ui.menu_sel = MS_FIRST[to];
            if ((uint32_t)to == MS_SIREN) {
                if (is_drum(TSEL))
                    ui_message("SELECT A SYNTH TRACK");
                else if (TSEL->eng_req != ENGI_SIREN) {
                    roots_dub_select_siren();
                    ui_message("DUB SIREN READY");
                }
            }
            ui.force = 1;
        }
        enc_drop();
        return;
    }
    if ((s = panel_enc(EN_PRESET)) != 0)               /* the cursor, round the section's rows */
        ui.menu_sel = (uint8_t)(MS_FIRST[sec] + (sel - MS_FIRST[sec] + (s > 0 ? 1u : n - 1u)) % n);
    for (k = 0; k < 4u; k++)
        if ((s = panel_enc(EN_K1 + k)) != 0 && k < n) {
            ui.menu_sel = (uint8_t)(MS_FIRST[sec] + k);   /* (the cursor follows the knob turned) */
            mi_set(MS_FIRST[sec] + k, s);
        }
    if (ok && sec == MS_ROOTS) {
        uint8_t enabled, gain[4];
        proj_roots_get(&enabled, gain);
        proj_roots_set((uint8_t)!enabled, gain);
        ui_message(enabled ? "ISOLATOR BYPASS" : "ISOLATOR ON");
        ui.force = 1;
    } else if (ok)
        mi_set(ui.menu_sel % MI_COUNT, 0);
    enc_drop();                                        /* swallow the rest while the menu is up */
}
