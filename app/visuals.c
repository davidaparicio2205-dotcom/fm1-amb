/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 David (davidaparicio2205-dotcom) */
/* The four eras' screens (docs/VISUAL_SYSTEM.md). Part of the unity build: ui.c includes it before its screen code.
 *
 * Each era is a palette (the header and the knobs take it too) and a scene in the main band (240 x 144), drawn whole
 * every second frame (30 a second). The scene is the instrument itself, drawn the era's way, so nothing the screen
 * showed before is lost: the chord and its name, the scale with its root, the four bell loops with their notes and
 * playheads, the three drums' patterns and steps, the four layers' levels, the way to the next chord (or "held").
 * How the sound moves it:
 *   pad      slow fields: fog, reflections, glows, signal lines, paper strips (each voice its own, by its loudness)
 *   bells    points of light on their loops (13, 17, 23, 29 steps): a glint when one rings, a trail as long as the echo
 *   bass     a slow breathing of the whole scene (its level, on a 9 s breath)
 *   kick     a tight pulse of one line or edge, 70 ms, never a flash; the snare a second, smaller one
 *   echo     trails and persistence; reverb: haze
 *   arc      over minutes: grain, haze and the glitches' share (the arc's full half is the machine's half)
 * A new era fades in through an ordered dither over 0.9 s (the old era's ground thinning out) while the palette
 * morphs. Float, no libm (fastmath.h); the scenes' per-pixel work is limited to the glows. */


#define ERA_FADE_MS 900u

static float era_mix(void)                       /* 0 the old era .. 1 the new (eased) */
{
    uint32_t dt = plat_ms() - vis.t0;
    float t;
    if (vis.era == vis.from || dt >= ERA_FADE_MS)
        return 1.0f;
    t = (float)dt * (1.0f / (float)ERA_FADE_MS);
    return t * t * (3.0f - 2.0f * t);
}

static uint16_t rgb3(const uint8_t *c) { return RGB(c[0], c[1], c[2]); }
static void palette_now(void)
{
    float m = era_mix();
    int k, t = (int)(m * 16.0f + 0.5f);
    uint32_t h = 2166136261u;
    for (k = 0; k < T_N; k++) {
        th[k] = mix(rgb3(ERA_RGB[vis.from][k]), rgb3(ERA_RGB[vis.era][k]), t);
        h = (h ^ th[k]) * 16777619u;
    }
    vis.sig = h;
}

static void vis_era(int era)                     /* a piece of another era: the morph begins */
{
    if (era == vis.era)
        return;
    vis.from = (uint8_t)(era_mix() < 0.5f ? vis.from : vis.era);
    vis.era = (uint8_t)era;
    vis.t0 = plat_ms();
}

/* ------------------------------------------------------- drawing helpers --- */
static uint32_t vrand(void)
{
    vis.rng = vis.rng * 1664525u + 1013904223u;
    return vis.rng >> 8;
}

static inline uint16_t *pxp(int32_t x, int32_t y)
{
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1)
        return 0;
    return &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
}
/* a colour over the pixel, a 0..256 */
static void px_mix(int32_t x, int32_t y, uint16_t c, int a)
{
    uint16_t *q = pxp(x, y);
    uint32_t b;
    int r, g, bl;
    if (!q || a <= 0)
        return;
    if (a >= 256) {
        *q = swap16(c);
        return;
    }
    b = swap16(*q);
    r = (int)(b >> 11), g = (int)((b >> 5) & 63u), bl = (int)(b & 31u);
    r += (((int)(c >> 11) - r) * a) >> 8;
    g += (((int)((c >> 5) & 63u) - g) * a) >> 8;
    bl += (((int)(c & 31u) - bl) * a) >> 8;
    *q = swap16((uint32_t)(r << 11 | g << 5 | bl));
}
/* light added to the pixel (a glow, a bloom): r, g, b 0..255, saturating */
static void px_add(int32_t x, int32_t y, int r, int g, int b)
{
    uint16_t *q = pxp(x, y);
    uint32_t p;
    int R, G, B;
    if (!q)
        return;
    p = swap16(*q);
    R = (int)(p >> 11) + ((r + 4) >> 3), G = (int)((p >> 5) & 63u) + ((g + 2) >> 2), B = (int)(p & 31u) + ((b + 4) >> 3);
    *q = swap16((uint32_t)((R > 31 ? 31 : R) << 11 | (G > 63 ? 63 : G) << 5 | (B > 31 ? 31 : B)));
}
/* a soft round glow, added: the light falls off as (1 - d^2/r^2)^2 (no square root), k its strength 0..1 */
static void glow(float cx, float cy, float r, const uint8_t *c, float k)
{
    int32_t x0 = (int32_t)(cx - r), x1 = (int32_t)(cx + r), y0 = (int32_t)(cy - r), y1 = (int32_t)(cy + r), x, y;
    float ir2 = 1.0f / (r * r);
    if (k <= 0.01f || r < 1.0f)
        return;
    if (y0 < 0)
        y0 = 0;
    if (y1 > (int32_t)cv_h - 1)
        y1 = (int32_t)cv_h - 1;
    for (y = y0; y <= y1; y++) {
        float dy = (float)y - cy, dy2 = dy * dy;
        for (x = x0; x <= x1; x++) {
            float dx = (float)x - cx, u = 1.0f - (dx * dx + dy2) * ir2, a;
            if (u <= 0.0f)
                continue;
            a = u * u * k;
            px_add(x, y, (int)((float)c[0] * a), (int)((float)c[1] * a), (int)((float)c[2] * a));
        }
    }
}
static void vgrad(int32_t y0, int32_t h, uint16_t c0, uint16_t c1)   /* a vertical gradient, row by row */
{
    int32_t y;
    for (y = 0; y < h; y++)
        cv_rect(0, y0 + y, (int32_t)cv_w, 1, mix(c0, c1, (int)(y * 16 / (h > 1 ? h - 1 : 1))));
}
static void hline_a(int32_t x0, int32_t x1, int32_t y, uint16_t c, int a)
{
    int32_t x;
    for (x = x0; x <= x1; x++)
        px_mix(x, y, c, a);
}
static void grain(int n, uint16_t c, int a)      /* film grain: n random specks */
{
    int i;
    for (i = 0; i < n; i++) {
        uint32_t r = vrand();
        px_mix((int32_t)(r % 240u), (int32_t)((r >> 8) % 144u), c, a + (int)(r >> 20 & 31u));
    }
}

/* ----------------------------------------------- the shared instruments --- */
/* the four layers' levels: x, bottom, height; round (glow) or square ends */
static void meters_at(int32_t x0, int32_t yb, int32_t h, int round)
{
    float lv[4], bm = 0.0f, dm = 0.0f;
    uint16_t C[4], T[4];
    int i;
    C[0] = K_PAD, C[1] = K_BELL, C[2] = K_BASS, C[3] = K_PULSE;
    T[0] = K_PAD_T, T[1] = K_BELL_T, T[2] = K_BASS_T, T[3] = K_PULSE_T;
    for (i = 0; i < EN_NLOOP; i++)
        bm = fm_maxf(bm, en_loop_glow[i]);
    for (i = 0; i < EN_NDRUM; i++)
        dm = fm_maxf(dm, en_drum_glow[i]);
    lv[0] = en_pad_level * (float)proj.par[P_PAD] * 0.01f;
    lv[1] = bm * (float)proj.par[P_BELLS] * 0.01f;
    lv[2] = en_bass_level * (float)proj.par[P_BASS] * 0.01f;
    lv[3] = dm * (float)proj.par[P_PULSE] * 0.01f;
    for (i = 0; i < 4; i++) {
        int32_t x = x0 + i * 9, f = (int32_t)(fm_clampf(lv[i], 0.0f, 1.0f) * (float)(h - 4) + 0.5f);
        if (round) {
            cv_round(x, yb - h, 5, h, 2, T[i]);
            if (f > 0)
                cv_round(x, yb - f - 2, 5, f + 2, 2, C[i]);
        } else {
            cv_rect(x + 2, yb - h, 1, h, T[i]);
            if (f > 0)
                cv_rect(x, yb - f, 5, f, C[i]);
        }
    }
}

/* the three drums as 4 x 4 steps each (a bar), the step now marked; style 0 squares, 1 dots, 2 offset print */
static void drums_at(int32_t x0, int32_t y0, int32_t dy, int style)
{
    int d, s;
    for (d = 0; d < EN_NDRUM; d++) {
        uint32_t p = en_drum_pat[d];
        int g = q16(en_drum_glow[d]);
        for (s = 0; s < 16; s++) {
            int32_t x = x0 + (s & 3) * 9, y = y0 + d * dy + (s >> 2) * 9;
            int hit = (int)(p >> s & 1u), cur = en_playing && s == en_drum_pos[d];
            uint16_t c = hit ? (en_pulse_rest ? K_LINE : cur ? mix(K_PULSE, K_ACC, g) : K_PULSE) : (cur ? K_DIM : K_LINE);
            if (style == 1) {
                cv_disc(x + 3, y + 3, hit ? (cur ? 3 : 2) : (cur ? 2 : 1), c);
            } else if (style == 2) {
                if (hit)
                    cv_rect(x + 1, y + 1, 7, 7, K_PULSE_T);
                cv_rect(x, y, hit ? 7 : 3, hit ? 7 : 3, c);
            } else {
                if (hit)
                    cv_rect(x, y, 7, 7, c);
                else
                    cv_rect(x + 3, y + 3, cur ? 3 : 1, cur ? 3 : 1, c);
                if (cur && hit)
                    cv_rect(x - 1, y - 1, 9, 1, K_ACC), cv_rect(x - 1, y + 7, 9, 1, K_ACC);
            }
        }
    }
}

/* the way to the next chord, or "held" (frozen), at x..x+w */
static void drift_at(int32_t x, int32_t y, int32_t w)
{
    if (en_frozen) {
        cv_text(x + w - text_w(&FONT_XS, "held"), y - 5, &FONT_XS, "held", K_FROST);
    } else {
        int32_t f = (int32_t)(en_drift * (float)w + 0.5f);
        cv_rect(x, y + 2, w, 1, K_LINE);
        if (f > 0)
            cv_rect(x, y + 1, f, 3, K_PAD);
    }
}

/* the scale as a row of twelve from C: in it a mark, the chord's notes bright, the root underlined */
static uint32_t chord_mask(void)
{
    uint32_t m = 0;
    int i;
    for (i = 0; i < EN_NPAD; i++)
        m |= 1u << (en_chord[i] % 12u);
    return m;
}
static void scale_row(int32_t x0, int32_t y, int32_t step, int round)
{
    uint32_t ap = pool_abs(), ch = chord_mask();
    int i;
    for (i = 0; i < 12; i++) {
        int32_t x = x0 + i * step;
        int in = (int)(ap >> i & 1u), c = (int)(ch >> i & 1u);
        uint16_t col = c ? mix(K_PAD_T, K_PAD, 8 + q16(en_pad_level) / 2) : in ? mix(K_PAD_T, K_PAD, 6) : K_LINE;
        if (round)
            cv_disc(x, y, c ? 3 : in ? 2 : 1, col);
        else
            cv_rect(x - (c ? 2 : 1), y - (c ? 2 : 1), c ? 5 : 3, c ? 5 : 3, col);
        if (i == proj.par[P_ROOT])
            cv_rect(x - 3, y + 5, 7, 1, K_TEXT);
    }
}
static void chord_text(int32_t x, int32_t y, const felucca_font_t *f, uint16_t c)
{
    char nm[16];
    chord_now(nm);
    cv_text(x, y, f, nm, en_frozen ? K_FROST : c);
}

static float sine(float ph) { return fm_sin_turns(ph); }   /* (a turn is 1) */
static float slow(float hz, float ph) { return sine(fm_fabsf((float)plat_ms() * 0.001f * hz + ph)); }

/* --------------------------------------------- era 1: after dark (mono) --- */
/* A city at night through a windscreen: four rows of lights going by (the bell loops; each light a note, the
 * playhead a white tick, a strike a star-shaped glint whose streak is as long as the echo), the horizon a hairline
 * the kick tightens, and under it the wet road: the pad's four voices as reflections that shimmer down into the
 * dark, the bass breathing in the haze at the horizon. Grain over all, more of it as the arc thins out. */
static void scene_dark(void)
{
    static const uint8_t W[3] = {255, 255, 255}, G[3] = {150, 150, 158};
    const int32_t L0 = 58, L1 = 192, HZ = 100;
    int l, s, i;
    float kick = en_drum_glow[0], sn = en_drum_glow[2], echo = (float)proj.par[P_ECHO] * 0.01f, b = vis.breath;
    vgrad(0, 144, RGB(6, 6, 8), RGB(22, 22, 26));
    glow(125.0f, (float)HZ, 70.0f + 20.0f * b, G, 0.10f + 0.22f * en_bass_level * b);   /* the haze, breathing */
    for (l = 0; l < EN_NLOOP; l++) {             /* the rows of lights */
        int len = EN_LOOP_LEN[l], pos = en_loop_pos[l];
        int32_t y = 40 + l * 14;
        float g = en_loop_glow[l];
        hline_a(L0, L1, y, K_LINE, 150);
        for (s = 0; s < len; s++) {
            int32_t x = L0 + (int32_t)((float)s * (float)(L1 - L0) / (float)(len - 1) + 0.5f);
            if (en_loop_note[l][s] >= 0) {       /* a light: a point and its halo */
                glow((float)x, (float)y, 4.0f, G, 0.5f);
                cv_rect(x - 1, y - 1, 2, 2, s == pos && en_playing ? K_ACC : K_PAD);
            }
        }
        if (en_playing) {
            int32_t x = L0 + (int32_t)((float)pos * (float)(L1 - L0) / (float)(len - 1) + 0.5f), k;
            cv_rect(x, y - 3, 1, 7, K_ACC);
            if (g > 0.05f) {                     /* the glint: a star, and the streak behind it (the echo) */
                int32_t arm = (int32_t)(3.0f + 9.0f * g), tr = (int32_t)(4.0f + 60.0f * echo * g);
                for (k = 1; k <= arm; k++) {
                    int a = (int)(220.0f * g * (1.0f - (float)k / (float)(arm + 1)));
                    px_mix(x + k, y, K_ACC, a), px_mix(x - k, y, K_ACC, a);
                    if (k <= arm / 2)
                        px_mix(x, y + k, K_ACC, a), px_mix(x, y - k, K_ACC, a);
                }
                for (k = 1; k <= tr; k++)
                    px_mix(x - arm - k, y, K_PAD, (int)(120.0f * g * (1.0f - (float)k / (float)(tr + 1))));
                glow((float)x, (float)y, 6.0f + 6.0f * g, W, 0.35f * g);
            }
        }
    }
    /* the horizon: a hairline; the kick draws it tight and bright for a moment, the snare a second line */
    hline_a(L0 - 6, L1 + 4, HZ, K_DIM, 160 + (int)(96.0f * kick));
    if (kick > 0.3f)
        hline_a(L0 - 6 + (int32_t)(30.0f * (1.0f - kick)), L1 + 4 - (int32_t)(30.0f * (1.0f - kick)), HZ + 1, K_ACC,
                (int)(200.0f * kick));
    if (sn > 0.3f)
        hline_a(L0 + 20, L1 - 20, HZ - 3, K_PAD, (int)(140.0f * sn));
    /* the wet road: each pad voice a reflection, by its place in the field and its loudness */
    for (i = 0; i < EN_NPAD; i++) {
        float a = en_pad_amp[i] * (float)proj.par[P_PAD] * 0.02f;
        int32_t xc = 70 + i * 34 + (en_pad_note[i] % 7) * 2, y;
        if (a < 0.02f)
            continue;
        for (y = HZ + 3; y < 142; y++) {
            float d = (float)(y - HZ) * (1.0f / 42.0f), w = slow(0.35f + 0.07f * (float)i, (float)y * 0.045f);
            int32_t x = xc + (int32_t)(2.2f * w * (0.3f + d));
            int al = (int)(fm_clampf(a, 0.0f, 1.0f) * 240.0f * (1.0f - d) * (0.65f + 0.35f * slow(1.3f, (float)y * 0.11f)));
            px_mix(x, y, K_PAD, al), px_mix(x + 1, y, K_PAD, al * 2 / 3);
            if (!(y & 3))
                px_mix(x - 1, y, K_PAD, al / 3);
        }
    }
    grain(260 + (int)(240.0f * fm_maxf(-en_arc, 0.0f)), K_TEXT, 20);
    grain(160, RGB(0, 0, 0), 60);
    /* the instrument's plain facts, in the margins */
    chord_text(58, 2, &FONT_B, K_TEXT);
    scale_row(62, 24, 9, 0);
    meters_at(8, 136, 92, 0);
    drums_at(200, 16, 42, 0);
    drift_at(200, 4, 33);
}

/* ------------------------------------------- era 2: the long glow (neon) --- */
/* A night window full of out-of-focus light: the pad's four voices as big soft glows (pink and cold blue, drifting
 * slowly, as bright as they sing), haze rising from the bottom with the bass's breath, and in front of it the garden
 * itself in neon: the scale's ring, the four loops round it, each strike a bloom with a fading trail of the last
 * strikes (the echo), the kick a ring of light that tightens on the outside. */
static void scene_glow(void)
{
    static const uint8_t PINK[3] = {255, 70, 170}, BLUE[3] = {70, 170, 255}, WHITE[3] = {255, 220, 245},
                         VIOLET[3] = {150, 60, 255};
    const float cx = 126.0f, cy = 76.0f, b = vis.breath;
    int i, l, s;
    float kick = en_drum_glow[0], rev = (float)proj.par[P_REVERB] * 0.01f;
    vgrad(0, 144, RGB(14, 8, 34), RGB(58, 16, 64));
    glow(120.0f, 160.0f, 120.0f, VIOLET, 0.25f + 0.35f * en_bass_level * b);   /* the haze: the bass's breath */
    for (i = 0; i < EN_NPAD; i++) {              /* the pad: four slow glows */
        float a = en_pad_amp[i] * (float)proj.par[P_PAD] * 0.012f, t = (float)plat_ms() * 0.001f;
        float x = cx + 62.0f * sine(fm_fabsf(t * (0.011f + 0.004f * (float)i) + 0.23f * (float)i)) - 31.0f + (float)(i - 2) * 14.0f;
        float y = cy + 30.0f * sine(fm_fabsf(t * (0.017f + 0.003f * (float)i) + 0.61f * (float)i)) - 15.0f;
        if (a > 0.02f)
            glow(x, y, 30.0f + 12.0f * a + 8.0f * rev, (i & 1) ? BLUE : PINK, 0.22f + 0.5f * fm_minf(a, 1.0f));
    }
    for (l = 0; l < EN_NLOOP; l++) {             /* the loops, in neon */
        int len = EN_LOOP_LEN[l], pos = en_loop_pos[l];
        float r = (float)LOOP_R[l] - 2.0f, g = en_loop_glow[l];
        for (s = 0; s < len; s++) {
            int32_t x, y;
            polar((int32_t)cx, (int32_t)cy, r, s, len, &x, &y);
            if (en_loop_note[l][s] >= 0)
                cv_disc(x, y, 1, mix(K_BELL_T, K_BELL, 10));
            else
                px_mix(x, y, K_LINE, 200);
        }
        if (en_playing) {
            int32_t x, y, k;
            polar((int32_t)cx, (int32_t)cy, r, pos, len, &x, &y);
            if (g > 0.6f && vis.trail[l][0] != (float)(x * 256 + y)) {   /* a new strike: the trail moves on */
                for (k = 3; k > 0; k--)
                    vis.trail[l][k] = vis.trail[l][k - 1];
                vis.trail[l][0] = (float)(x * 256 + y);
            }
            for (k = 1; k < 4; k++) {            /* the trail: the echo's repeats, fading */
                int32_t tv = (int32_t)vis.trail[l][k];
                float e = (float)proj.par[P_ECHO] * 0.01f * (1.0f - 0.28f * (float)k);
                if (tv > 0)
                    glow((float)(tv >> 8), (float)(tv & 255), 5.0f, BLUE, 0.35f * e);
            }
            cv_disc(x, y, 1 + q16(g) / 8, mix(K_BELL, K_ACC, q16(g)));
            glow((float)x, (float)y, 5.0f + 9.0f * g, WHITE, 0.6f * g);
        }
    }
    {   /* the scale's ring and the chord's name in the middle */
        uint32_t ap = pool_abs(), ch = chord_mask();
        char nm[16];
        for (i = 0; i < 12; i++) {
            int32_t x, y;
            polar((int32_t)cx, (int32_t)cy, 23.0f, i, 12, &x, &y);
            if (i == proj.par[P_ROOT])
                disc((float)x, (float)y, 4.6f, K_TEXT);
            if (ch >> i & 1u)
                disc((float)x, (float)y, 3.4f, mix(K_PAD_T, K_PAD, 8 + q16(en_pad_level) / 2));
            else if (ap >> i & 1u)
                disc((float)x, (float)y, 2.4f, mix(K_PAD_T, K_PAD, 8));
            else
                px_mix(x, y, K_LINE, 256);
        }
        chord_now(nm);
        text_c((int32_t)cx, (int32_t)cy - 8, text_w(&FONT_B, nm) <= 34 ? &FONT_B : &FONT_XS, nm, en_frozen ? K_FROST : K_TEXT);
    }
    if (kick > 0.05f) {                          /* the kick: a ring of light closing in, 70 ms */
        float r = 64.0f + 6.0f * (1.0f - kick);
        int k;
        for (k = 0; k < 96; k++) {
            int32_t x, y;
            polar((int32_t)cx, (int32_t)cy, r, k, 96, &x, &y);
            px_add(x, y, (int)(255.0f * kick), (int)(80.0f * kick), (int)(190.0f * kick));
        }
    }
    meters_at(8, 136, 92, 1);
    drums_at(202, 16, 42, 1);
    drift_at(202, 4, 32);
}

/* ------------------------------------------- era 3: clean signal (digital) --- */
/* A clean white screen with a soft grid and scanlines: the four loops as rows of cells (a sequencer: a note a filled
 * cell, the playhead's cell framed, a strike lit), the pad's voices as four signal lines (a wave a voice, as tall as
 * it sings), the scale as a row of squares. The machine comes in with the arc's full half, as the sound does: slices
 * of the screen shift sideways on the pulse's hits, and a coral cursor marks the beat. */
static void scene_signal(void)
{
    const int32_t X0 = 58, X1 = 194;
    int l, s, i;
    float kick = en_drum_glow[0], sn = en_drum_glow[2], mach = fm_clampf(en_arc * 2.5f, 0.0f, 1.0f), b = vis.breath;
    int32_t y;
    cv_rect(0, 0, 240, 144, mix(K_BG, K_PANEL, (int)(6.0f * b * en_bass_level)));   /* the breath: a little light */
    for (y = 6; y < 144; y += 12)                /* the grid */
        cv_rect(X0, y, X1 - X0, 1, kick > 0.4f ? K_DIM : K_LINE);
    for (s = X0; s <= X1; s += 17)
        cv_rect(s, 6, 1, 132, K_LINE);
    for (i = 0; i < EN_NPAD; i++) {              /* the pad: signal lines */
        float a = en_pad_amp[i] * (float)proj.par[P_PAD] * 0.01f, f = 0.05f + 0.012f * (float)(en_pad_note[i] % 12);
        float ph = (float)plat_ms() * 0.0004f * (1.0f + 0.2f * (float)i);
        int32_t yc = 98 + i * 10, x, last = yc;
        if (a < 0.02f)
            continue;
        for (x = X0; x <= X1; x++) {
            int32_t yy = yc + (int32_t)(4.5f * fm_minf(a, 1.0f) * sine(fm_fabsf((float)x * f + ph)));
            int32_t y0 = yy < last ? yy : last, y1 = yy < last ? last : yy;
            cv_rect(x, y0, 1, y1 - y0 + 1, mix(K_PAD_T, K_PAD, 10 - 2 * (i & 1)));
            last = yy;
        }
    }
    for (l = 0; l < EN_NLOOP; l++) {             /* the loops: rows of cells */
        int len = EN_LOOP_LEN[l], pos = en_loop_pos[l];
        float w = (float)(X1 - X0) / (float)len, g = en_loop_glow[l];
        int32_t yr = 34 + l * 14;
        for (s = 0; s < len; s++) {
            int32_t x = X0 + (int32_t)((float)s * w), cw = (int32_t)((float)(s + 1) * w) - (int32_t)((float)s * w) - 1;
            int note = en_loop_note[l][s] >= 0, cur = en_playing && s == pos;
            if (note)
                cv_rect(x, yr, cw, 9, cur ? mix(K_BELL, K_PULSE, q16(g) * mach > 8.0f ? 16 : q16(g)) : K_BELL);
            else
                cv_rect(x, yr + 4, cw, 1, K_BELL_T);
            if (cur) {
                cv_rect(x - 1, yr - 2, cw + 2, 1, K_TEXT);
                cv_rect(x - 1, yr + 10, cw + 2, 1, K_TEXT);
            }
        }
    }
    for (y = 0; y < 144; y += 2)                 /* scanlines */
        hline_a(0, 239, y, RGB(0, 0, 0), 9);
    if (mach > 0.05f && en_playing) {            /* the machine's half: slices shift on the pulse's hits */
        float hit = fm_maxf(kick, fm_maxf(sn, en_drum_glow[1] * 0.6f));
        if (hit > 0.5f) {
            uint32_t h = (uint32_t)en_bars * 2654435761u;
            int k, n = 1 + (int)(mach * 3.0f);
            for (k = 0; k < n; k++) {
                int32_t ys = 30 + (int32_t)((h >> (5 + k * 7)) % 100u), hs = 2 + (int32_t)((h >> (11 + k)) % 6u);
                int32_t dx = ((int32_t)((h >> (3 + k * 5)) % 13u) - 6) * (int32_t)(hit * mach * 2.0f + 0.5f), r;
                for (r = ys; r < ys + hs && r < 144; r++) {
                    uint16_t *row = &cv_px[(uint32_t)(r + cv_oy) * cv_w];
                    if (dx > 0)
                        memmove(row + X0 + dx, row + X0, (size_t)(X1 - X0 - dx) * 2u);
                    else if (dx < 0)
                        memmove(row + X0, row + X0 - dx, (size_t)(X1 - X0 + dx) * 2u);
                    if (k == 0)
                        hline_a(X0, X1, r, K_PULSE, (int)(60.0f * hit * mach));
                }
            }
        }
        if (en_drum_glow[0] > 0.2f)              /* the beat's cursor */
            cv_rect(X1 + 2, 34, 2, 52, mix(K_BG, K_PULSE, q16(en_drum_glow[0] * mach)));
    }
    chord_text(X0, 2, &FONT_B, K_TEXT);
    scale_row(X0 + 66, 12, 6, 0);
    meters_at(10, 136, 92, 0);
    drums_at(202, 16, 42, 0);
    drift_at(202, 4, 32);
}

/* ---------------------------------------------- era 4: cut and paste --- */
/* A collage on warm paper: card tiles, cold against warm, that cut to each other on the groove's own hits (which tile
 * is cold is a function of the step, so the cuts are irregular and the same every bar), freezes where the pattern
 * rests. In the tiles: the chord's name in two typefaces, the loops as torn strips, the pad's voices as paper bars,
 * the bass as halftone dots that swell with its breath, the scale as a cut strip. */
static void scene_collage(void)
{
    static const int16_t TILE[6][4] = {          /* x, y, w, h: overlapping cards */
        {54, 4, 92, 42}, {132, 26, 64, 54}, {56, 52, 74, 46}, {120, 86, 76, 54}, {60, 104, 66, 36}, {150, 4, 46, 20}};
    const int step = en_drum_pos[0];
    const uint32_t pat = en_drum_pat[0] | en_drum_pat[2];
    uint32_t h = ((uint32_t)step * 2654435761u) >> 7;
    int k, l, s, i;
    float b = vis.breath;
    /* the cut: on a hit, the step decides which two tiles turn cold; between hits, it holds (the freeze) */
    static uint8_t cold = 0x05, last_step = 0xFF;
    if (en_playing && step != last_step) {
        last_step = (uint8_t)step;
        if (pat >> step & 1u)
            cold = (uint8_t)(1u << (h % 6u) | 1u << ((h >> 4) % 6u));
    }
    cv_rect(0, 0, 240, 144, K_BG);
    for (k = 0; k < 180; k++) {                  /* the paper's fibres */
        uint32_t r = vrand();
        px_mix((int32_t)(r % 240u), (int32_t)((r >> 8) % 144u), K_LINE, 50);
    }
    for (k = 0; k < 6; k++) {
        int c = (int)(cold >> k & 1u);
        cv_rect(TILE[k][0] + 3, TILE[k][1] + 3, TILE[k][2], TILE[k][3], mix(K_BG, K_TEXT, 3));   /* its shadow */
        cv_rect(TILE[k][0], TILE[k][1], TILE[k][2], TILE[k][3], c ? K_BELL_T : K_PANEL);
    }
    {   /* the bass: halftone dots in tile 3, as big as its breath */
        int32_t x, y;
        float rr = 0.6f + 2.4f * en_bass_level * (0.55f + 0.45f * b);
        for (y = TILE[3][1] + 4; y < TILE[3][1] + TILE[3][3] - 2; y += 7)
            for (x = TILE[3][0] + 4 + ((y >> 3) & 1) * 3; x < TILE[3][0] + TILE[3][2] - 2; x += 7)
                disc((float)x, (float)y, rr, (cold >> 3 & 1u) ? K_FROST : K_BASS);
    }
    for (i = 0; i < EN_NPAD; i++) {              /* the pad: paper bars in tile 2 */
        float a = fm_minf(en_pad_amp[i] * (float)proj.par[P_PAD] * 0.012f, 1.0f);
        int32_t hgt = (int32_t)(a * 36.0f), x = TILE[2][0] + 8 + i * 16;
        if (hgt > 0) {
            cv_rect(x + 2, TILE[2][1] + 42 - hgt + 2, 10, hgt, mix(K_PAD_T, K_TEXT, 4));
            cv_rect(x, TILE[2][1] + 42 - hgt, 10, hgt, (cold >> 2 & 1u) ? K_BELL : K_PAD_T);
        }
    }
    for (l = 0; l < EN_NLOOP; l++) {             /* the loops: torn strips across tile 1 */
        int len = EN_LOOP_LEN[l], pos = en_loop_pos[l];
        int32_t y = TILE[1][1] + 8 + l * 12, x0 = TILE[1][0] + 4, w = TILE[1][2] - 8;
        float g = en_loop_glow[l];
        for (s = 0; s < len; s++) {
            int32_t x = x0 + s * w / len, cw = (s + 1) * w / len - s * w / len;
            int note = en_loop_note[l][s] >= 0, cur = en_playing && s == pos;
            if (note)
                cv_rect(x, y - ((s * 7 + l) % 3), cw - (cw > 2), 6, cur ? mix(K_BELL, K_ACC, q16(g)) : K_BELL);
            else if (!(s & 1))
                cv_rect(x, y + 2, cw, 1, K_DIM);
            if (cur)
                cv_rect(x, y + 7, cw, 2, K_TEXT);
        }
    }
    {   /* the chord's name in tile 0: two typefaces, letter by letter (a ransom note) */
        char nm[16], one[2] = {0, 0};
        int32_t x = TILE[0][0] + 6;
        chord_now(nm);
        for (i = 0; nm[i]; i++) {
            one[0] = nm[i];
            x = cv_text(x, TILE[0][1] + ((i & 1) ? 12 : 6), (i & 1) ? &FONT_B : &FONT_M, one,
                        en_frozen ? K_FROST : ((cold & 1u) && (i & 1) ? K_ACC : K_TEXT)) + 1;
        }
    }
    scale_row(TILE[4][0] + 6, TILE[4][1] + 14, 5, 0);   /* the scale: a strip in tile 4 */
    drift_at(TILE[5][0] + 4, TILE[5][1] + 8, TILE[5][2] - 8);
    meters_at(10, 136, 92, 0);
    drums_at(204, 16, 42, 2);
}

/* ------------------------------------------------------------ the band --- */
/* the change of era: the old era's ground over the new one in an ordered dither, thinning out */
static void era_veil(void)
{
    static const uint8_t BAYER[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    float m = era_mix();
    int32_t x, y;
    uint16_t c;
    int lim;
    if (m >= 1.0f)
        return;
    c = swap16(rgb3(ERA_RGB[vis.from][T_BG]));
    lim = (int)((1.0f - m) * 16.0f + 0.5f);
    for (y = 0; y < 144; y++) {
        uint16_t *row = &cv_px[(uint32_t)(y + cv_oy) * cv_w];
        for (x = 0; x < 240; x++)
            if (BAYER[(y & 3) * 4 + (x & 3)] < lim)
                row[x] = c;
    }
}

/* the boot's own animation (1.8 s): the firmware's name drawn in by a line of light, in the era's colours */
#define INTRO_MS 1800u
static void draw_intro(uint32_t dt)
{
    static const uint8_t W[3] = {255, 255, 255};
    const char *nm = APP_NAME;
    float t = (float)dt * (1.0f / (float)INTRO_MS), sweep = fm_clampf(t * 1.6f, 0.0f, 1.0f);
    int32_t w = text_w(&FONT_M, nm), x0 = 120 - w / 2, xs = x0 - 10 + (int32_t)((float)(w + 20) * sweep), y;
    cv_rect(0, 0, 240, 144, K_BG);
    cv_text(x0, 52, &FONT_M, nm, K_TEXT);
    for (y = 52; y < 80; y++)                    /* what the light has not reached yet stays dark */
        cv_rect(xs, y, 240 - xs, 1, K_BG);
    hline_a(x0 - 10, xs, 84, K_ACC, 220);
    glow((float)xs, 84.0f, 10.0f, W, 0.8f * (1.0f - sweep * 0.6f));
    if (t > 0.6f) {
        int a = (int)(fm_clampf((t - 0.6f) * 3.0f, 0.0f, 1.0f) * 16.0f);
        text_c(120, 92, &FONT_XS, en_era_name(vis.era), mix(K_BG, K_DIM, a));
    }
}

/* the main band: the era's scene, then the change's veil; a moment of the piece's era name on a change */
static void draw_scene(void)
{
    switch (vis.era) {
    case 0: scene_dark(); break;
    case 1: scene_glow(); break;
    case 2: scene_signal(); break;
    default: scene_collage(); break;
    }
    era_veil();
}

static void vis_tick(void)                       /* once a frame: the breath, the frame rate */
{
    uint32_t now = plat_ms();
    vis.breath = 0.5f + 0.5f * slow(1.0f / 9.0f, 0.0f);
    if (now - vis.fps_t >= 1000u) {
        vis.fps = vis.frames * 1000u / (now - vis.fps_t);
        vis.frames = 0;
        vis.fps_t = now;
    }
}

static void vis_init(void)
{
    vis.era = vis.from = (uint8_t)era_of(proj.piece);
    vis.t0 = plat_ms() - ERA_FADE_MS;
    vis.rng = 0x1F2E3D4Cu;
    vis.intro = 1;
    vis.intro_t = plat_ms();
    vis.fps_t = plat_ms();
    palette_now();
}

/* the first screen at boot (APP_SPLASH: before the project is loaded, so in no era's colours yet): the name in white
 * on the dark, a hairline under it. The animation proper is the intro, once the screen is the app's */
void app_splash(void)
{
    lcd_fill(0, 0, 240, 240, RGB(8, 8, 10));
    cv_begin(240, 64, RGB(8, 8, 10));
    text_c(120, 8, &FONT_M, APP_NAME, RGB(236, 236, 240));
    cv_rect(80, 40, 80, 1, RGB(90, 90, 98));
    text_c(120, 44, &FONT_XS, APP_TAGLINE, RGB(130, 130, 138));
    cv_blit(0, 88);
}
