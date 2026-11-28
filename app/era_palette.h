/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 David (davidaparicio2205-dotcom) */
/* The four eras' palettes (docs/VISUAL_SYSTEM.md): ui.c includes this where its colours were, visuals.c draws with
 * them. th[] is the palette now: the era's, or a morph between two while the piece changes era. */
/* ------------------------------------------------------------- palettes --- */
enum { T_BG, T_PANEL, T_LINE, T_DIM, T_TEXT, T_ACC, T_PAD, T_PAD_T, T_BELL, T_BELL_T, T_BASS, T_BASS_T, T_PULSE,
       T_PULSE_T, T_FROST, T_FROST_T, T_N };
static const uint8_t ERA_RGB[EN_NERAS][T_N][3] = {
    /* after dark: monochrome, high contrast, sodium-less: only greys and one white */
    {{10, 10, 12}, {34, 34, 38}, {58, 58, 64}, {128, 128, 134}, {228, 228, 232}, {255, 255, 255},
     {196, 196, 200}, {70, 70, 76}, {236, 236, 240}, {92, 92, 98}, {150, 150, 156}, {54, 54, 60},
     {210, 210, 214}, {60, 60, 66}, {170, 190, 210}, {70, 80, 92}},
    /* the long glow: indigo night, neon pink, cold blue */
    {{18, 10, 40}, {44, 22, 78}, {74, 48, 116}, {160, 132, 196}, {255, 236, 248}, {255, 96, 184},
     {255, 120, 196}, {90, 40, 110}, {110, 214, 255}, {44, 72, 130}, {196, 110, 255}, {70, 40, 110},
     {255, 170, 220}, {80, 50, 110}, {150, 230, 255}, {50, 90, 140}},
    /* clean signal: white, slate, one blue, one coral for the machine */
    {{236, 240, 245}, {255, 255, 255}, {200, 210, 222}, {104, 116, 134}, {18, 26, 40}, {0, 112, 224},
     {40, 120, 220}, {190, 214, 240}, {0, 150, 200}, {176, 222, 236}, {70, 84, 110}, {200, 206, 216},
     {255, 92, 70}, {255, 206, 196}, {0, 170, 170}, {180, 230, 228}},
    /* cut and paste: warm paper and cold card, ink */
    {{224, 160, 100}, {242, 206, 160}, {186, 120, 70}, {104, 64, 36}, {36, 20, 10}, {30, 70, 120},
     {244, 228, 196}, {200, 136, 84}, {40, 84, 130}, {150, 168, 182}, {120, 60, 30}, {196, 132, 84},
     {20, 30, 44}, {170, 116, 70}, {70, 120, 150}, {170, 190, 196}},
};
static uint16_t th[T_N];                         /* the palette now (morphing during a change of era) */
#define K_BG th[T_BG]
#define K_PANEL th[T_PANEL]
#define K_LINE th[T_LINE]
#define K_DIM th[T_DIM]
#define K_TEXT th[T_TEXT]
#define K_ACC th[T_ACC]
#define K_PAD th[T_PAD]
#define K_PAD_T th[T_PAD_T]
#define K_BELL th[T_BELL]
#define K_BELL_T th[T_BELL_T]
#define K_BASS th[T_BASS]
#define K_BASS_T th[T_BASS_T]
#define K_PULSE th[T_PULSE]
#define K_PULSE_T th[T_PULSE_T]
#define K_FROST th[T_FROST]
#define K_FROST_T th[T_FROST_T]

static struct {
    uint8_t era, from;                           /* the era shown, the one it is leaving */
    uint32_t t0;                                 /* when the change began */
    uint32_t sig;                                /* the palette's signature (the header and knobs redraw on it) */
    uint32_t rng;
    uint32_t frames, fps, fps_t;                 /* main-band frames drawn, and per second (Setup's header) */
    float trail[EN_NLOOP][4];                    /* the bells' last strike places (era 2), for the echo's trails */
    uint8_t intro;                               /* the boot's own animation is still running */
    uint32_t intro_t;
    float breath;                                /* the bass's breath, 0..1 */
} vis;

static int era_of(int piece) { return (piece & 7) >> 1; }   /* pieces in pairs: 1-2, 3-4, 5-6, 7-8 */
static void vis_era(int era);
