/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 David (davidaparicio2205-dotcom) */
/* The offline render harness: the firmware's own engine (app/dsp/amb.c, compiled for the host with -DEN_HOST, the
 * same file the device builds) plays one piece for a while, and the output is measured as it streams: peak, a
 * true-peak estimate, an approximate integrated loudness (BS.1770 K-weighting, gated), DC, the limiter's work, NaN and
 * denormals, the average spectrum (centroid, five bands, tonal peaks in 2..6 kHz), and, for a layer alone, the kick's
 * fundamental, the bass's attack and sustain, and how alike the pulse's bars are. Also the engine's host CPU time.
 *
 *   render PIECE SECONDS [--wav OUT.wav] [--solo pad|bells|bass|pulse] [--mode M] [--master Q12] [--json OUT.json]
 *
 * The metrics go to stdout (and OUT.json) as one JSON object. tools/render_all.sh runs every piece; tools/metrics.py
 * makes the table and checks the targets (docs/ERA_BIBLE.md, Acceptance). */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st, (void)d1, (void)d2; }

#define SR 44100
#define NFFT 8192
#define PI 3.14159265358979323846

/* ---------------------------------------------------------------- FFT --- */
static void fft(double *re, double *im, int n)
{
    int i, j, k, m;
    for (i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i];
            re[i] = re[j], re[j] = t;
            t = im[i], im[i] = im[j], im[j] = t;
        }
    }
    for (m = 2; m <= n; m <<= 1) {
        double a = -2.0 * PI / m, wr = cos(a), wi = sin(a);
        for (k = 0; k < n; k += m) {
            double cr = 1.0, ci = 0.0;
            for (j = 0; j < m / 2; j++) {
                double xr = re[k + j + m / 2] * cr - im[k + j + m / 2] * ci;
                double xi = re[k + j + m / 2] * ci + im[k + j + m / 2] * cr, t;
                re[k + j + m / 2] = re[k + j] - xr;
                im[k + j + m / 2] = im[k + j] - xi;
                re[k + j] += xr;
                im[k + j] += xi;
                t = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = t;
            }
        }
    }
}

/* ------------------------------------------------- K-weighting (BS.1770) --- */
typedef struct { double b0, b1, b2, a1, a2, z1, z2; } bq_t;
static double bq(bq_t *f, double x)
{
    double y = f->b0 * x + f->z1;
    f->z1 = f->b1 * x - f->a1 * y + f->z2;
    f->z2 = f->b2 * x - f->a2 * y;
    return y;
}
static void kweight(bq_t *shelf, bq_t *hp)       /* the two stages at 44.1 kHz (as pyloudnorm derives them) */
{
    double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
    double K = tan(PI * f0 / SR), Vh = pow(10.0, G / 20.0), Vb = pow(Vh, 0.4996667741545416);
    double a0 = 1.0 + K / Q + K * K;
    shelf->b0 = (Vh + Vb * K / Q + K * K) / a0;
    shelf->b1 = 2.0 * (K * K - Vh) / a0;
    shelf->b2 = (Vh - Vb * K / Q + K * K) / a0;
    shelf->a1 = 2.0 * (K * K - 1.0) / a0;
    shelf->a2 = (1.0 - K / Q + K * K) / a0;
    f0 = 38.13547087602444, Q = 0.5003270373238773;
    K = tan(PI * f0 / SR);
    a0 = 1.0 + K / Q + K * K;
    hp->b0 = 1.0 / a0, hp->b1 = -2.0 / a0, hp->b2 = 1.0 / a0;
    hp->a1 = 2.0 * (K * K - 1.0) / a0;
    hp->a2 = (1.0 - K / Q + K * K) / a0;
    shelf->z1 = shelf->z2 = hp->z1 = hp->z2 = 0.0;
}

/* ------------------------------------------------------------ the state --- */
static const char *LAYER[4] = {"pad", "bells", "bass", "pulse"};
static double spec[NFFT / 2 + 1], win[NFFT];
static double fre[NFFT], fim[NFFT];
static float ring[NFFT];
static int nring, nspec;
static double *blocks;                           /* 100 ms mean squares (K-weighted, both channels summed) */
static int nblocks;
static float *envl;                              /* 10 ms RMS of the mono sum, for the layer measures */
static int nenv;

static double fin(double x) { return isfinite(x) ? x : -99.0; }   /* JSON has no inf or nan */

static int cmpd(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y;
}

static double band_db(double lo, double hi, double tot)
{
    double s = 0.0;
    int k;
    for (k = 1; k <= NFFT / 2; k++) {
        double f = (double)k * SR / NFFT;
        if (f >= lo && f < hi)
            s += spec[k];
    }
    return tot > 0.0 ? 10.0 * log10(s / tot + 1e-30) : -99.0;
}

int main(int argc, char **argv)
{
    int piece, mode = -1, solo = -1, i, master = 1024, a;
    double secs;
    const char *wav = 0, *json = 0;
    FILE *wf = 0, *jf = 0;
    int16_t par[P_NPARAMS];
    uint16_t pool;
    long frames, done = 0;
    static int32_t buf[2 * 256];
    double peak = 0.0, tpeak = 0.0, sum_l = 0.0, sum_r = 0.0, sq = 0.0;
    float hist[2][3] = {{0}};
    bq_t ks[2], kh[2];
    double kacc = 0.0;
    int kn = 0;
    float eacc = 0.0f;
    int en = 0;
    clock_t c0, c1;
    if (argc < 3) {
        fprintf(stderr, "usage: render PIECE SECONDS [--wav F] [--solo L] [--mode M] [--master Q12] [--json F]\n");
        return 2;
    }
    piece = atoi(argv[1]);
    secs = atof(argv[2]);
    for (a = 3; a < argc; a++) {
        if (!strcmp(argv[a], "--wav") && a + 1 < argc)
            wav = argv[++a];
        else if (!strcmp(argv[a], "--json") && a + 1 < argc)
            json = argv[++a];
        else if (!strcmp(argv[a], "--mode") && a + 1 < argc)
            mode = atoi(argv[++a]);
        else if (!strcmp(argv[a], "--master") && a + 1 < argc)
            master = atoi(argv[++a]);
        else if (!strcmp(argv[a], "--solo") && a + 1 < argc) {
            const char *s = argv[++a];
            for (i = 0; i < 4; i++)
                if (!strcmp(s, LAYER[i]))
                    solo = i;
        }
    }
    frames = (long)(secs * SR);
    blocks = calloc((size_t)(frames / 4410 + 2), sizeof *blocks);
    envl = calloc((size_t)(frames / 441 + 2), sizeof *envl);
    for (i = 0; i < NFFT; i++)
        win[i] = 0.5 - 0.5 * cos(2.0 * PI * i / NFFT);
    kweight(&ks[0], &kh[0]);
    kweight(&ks[1], &kh[1]);

    /* the piece, as the firmware sets it up (project_apply, then a regrow) */
    en_init();
    en_piece(piece, par, &pool);
    if (mode >= 0) {
        par[P_MODE] = (int16_t)mode;
        pool = EN_MODE_POOL[mode];
    }
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, par[i]);
    if (solo >= 0)
        for (i = 0; i < 4; i++)
            if (i != solo)
                en_set(P_PAD + i, 0);
    en_pool(pool);
    en_regrow();
    en_play(1);
    memset(&en_host, 0, sizeof en_host);

    if (wav && (wf = fopen(wav, "wb")) != 0) {   /* a 16-bit stereo WAV (the header's sizes filled in at the end) */
        static const uint8_t H[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0,
                                      1, 0, 2, 0, 0x44, 0xAC, 0, 0, 0x10, 0xB1, 2, 0, 4, 0, 16, 0, 'd', 'a', 't', 'a'};
        fwrite(H, 1, 44, wf);
    }
    c0 = clock();
    double eng = 0.0;
    while (done < frames) {
        int n = 256, j;
        clock_t t0 = clock();
        en_render(buf, (uint32_t)n, (uint32_t)master);
        eng += (double)(clock() - t0);
        for (j = 0; j < n; j++) {
            float l = (float)buf[2 * j] / 8388608.0f, r = (float)buf[2 * j + 1] / 8388608.0f, m = 0.5f * (l + r);
            int c;
            float xs[2] = {l, r};
            double kl, kr;
            for (c = 0; c < 2; c++) {            /* the true peak: 4x, Catmull-Rom between the last samples */
                float p0 = hist[c][0], p1 = hist[c][1], p2 = hist[c][2], p3 = xs[c];
                int t;
                for (t = 1; t < 4; t++) {
                    float u = (float)t * 0.25f;
                    float y = 0.5f * (2.0f * p1 + (-p0 + p2) * u + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * u * u +
                                      (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * u * u * u);
                    if (fabs(y) > tpeak)
                        tpeak = fabs(y);
                }
                hist[c][0] = p1, hist[c][1] = p2, hist[c][2] = p3;
                if (fabs(xs[c]) > peak)
                    peak = fabs(xs[c]);
            }
            if (fabs(l) > tpeak)
                tpeak = fabs(l);
            if (fabs(r) > tpeak)
                tpeak = fabs(r);
            sum_l += l, sum_r += r, sq += (double)m * m;
            kl = bq(&kh[0], bq(&ks[0], l));
            kr = bq(&kh[1], bq(&ks[1], r));
            kacc += kl * kl + kr * kr;
            if (++kn == 4410) {
                blocks[nblocks++] = kacc / kn;
                kacc = 0.0, kn = 0;
            }
            eacc += m * m;
            if (++en == 441) {
                envl[nenv++] = sqrtf(eacc / 441.0f);
                eacc = 0.0f, en = 0;
            }
            ring[nring++] = m;
            if (nring == NFFT) {
                int k;
                for (k = 0; k < NFFT; k++)
                    fre[k] = ring[k] * win[k], fim[k] = 0.0;
                fft(fre, fim, NFFT);
                for (k = 0; k <= NFFT / 2; k++)
                    spec[k] += fre[k] * fre[k] + fim[k] * fim[k];
                nspec++;
                nring = 0;
            }
            if (wf) {
                int16_t s[2];
                s[0] = (int16_t)lrintf(fmaxf(-1.0f, fminf(1.0f, l)) * 32767.0f);
                s[1] = (int16_t)lrintf(fmaxf(-1.0f, fminf(1.0f, r)) * 32767.0f);
                fwrite(s, 2, 2, wf);
            }
        }
        done += n;
    }
    c1 = clock();
    if (wf) {
        uint32_t data = (uint32_t)(done * 4), riff = data + 36;
        fseek(wf, 4, SEEK_SET);
        fwrite(&riff, 4, 1, wf);
        fseek(wf, 40, SEEK_SET);
        fwrite(&data, 4, 1, wf);
        fclose(wf);
    }

    /* ---- the measures */
    double lufs, tot = 0.0, cen = 0.0, harsh_db = -99.0, harsh_rel = -99.0, harsh_f = 0.0, kick_f = 0.0;
    int harsh_n = 0, k;
    {   /* gated loudness: blocks of 400 ms, 75 % overlap, -70 LUFS absolute, then -10 LU relative */
        int nb = nblocks - 3, g = 0;
        double s = 0.0, thr;
        double *z = calloc((size_t)(nb > 0 ? nb : 1), sizeof *z);
        for (i = 0; i < nb; i++)
            z[i] = 0.25 * (blocks[i] + blocks[i + 1] + blocks[i + 2] + blocks[i + 3]);
        for (i = 0; i < nb; i++)
            if (-0.691 + 10.0 * log10(z[i] + 1e-30) > -70.0)
                s += z[i], g++;
        thr = g ? -0.691 + 10.0 * log10(s / g) - 10.0 : -70.0;
        s = 0.0, g = 0;
        for (i = 0; i < nb; i++)
            if (-0.691 + 10.0 * log10(z[i] + 1e-30) > thr)
                s += z[i], g++;
        lufs = g ? -0.691 + 10.0 * log10(s / g) : -99.0;
        free(z);
    }
    for (k = 1; k <= NFFT / 2; k++) {
        tot += spec[k];
        cen += spec[k] * k * (double)SR / NFFT;
    }
    cen = tot > 0.0 ? cen / tot : 0.0;
    {   /* tonal peaks, 2..6 kHz: a local maximum of the average spectrum that stands above the median of the bins
         * around it (a fifth of an octave, its own +-2 bins left out) by more than 12 dB. Sustained notes have such
         * peaks by nature; one is HARSH when it is also loud: within 30 dB of the loudest bin above 250 Hz
         * (docs/ERA_BIBLE.md, Acceptance) */
        double mx = 0.0, wnd[400], rel;
        for (k = (int)(250.0 * NFFT / SR); k <= NFFT / 2; k++)
            mx = fmax(mx, spec[k]);
        for (k = (int)(2000.0 * NFFT / SR); k <= (int)(6000.0 * NFFT / SR); k++) {
            int lo = (int)(k / 1.12), hi = (int)(k * 1.12), n = 0, q;
            double med, prom;
            if (spec[k] < spec[k - 1] || spec[k] < spec[k + 1] || spec[k] < mx * 1e-5)
                continue;
            for (q = lo; q <= hi && n < 400; q++)
                if (q < k - 2 || q > k + 2)
                    wnd[n++] = spec[q];
            qsort(wnd, (size_t)n, sizeof wnd[0], cmpd);
            med = wnd[n / 2];
            prom = 10.0 * log10(spec[k] / (med + 1e-30));
            rel = 10.0 * log10(spec[k] / mx);
            if (prom > 12.0 && rel > harsh_rel)  /* the loudest peak that stands out */
                harsh_rel = rel, harsh_db = prom, harsh_f = (double)k * SR / NFFT;
            if (prom > 12.0 && rel > -30.0)      /* harsh: it stands out, and it is loud */
                harsh_n++;
        }
    }
    if (solo == 3) {   /* the kick: the strongest bin from 25 to 120 Hz, refined by the neighbours' parabola */
        int b = 0;
        for (k = (int)(25.0 * NFFT / SR); k <= (int)(120.0 * NFFT / SR); k++)
            if (!b || spec[k] > spec[b])
                b = k;
        {
            double y0 = log(spec[b - 1] + 1e-30), y1 = log(spec[b] + 1e-30), y2 = log(spec[b + 1] + 1e-30);
            double d = 0.5 * (y0 - y2) / (y0 - 2.0 * y1 + y2);
            kick_f = ((double)b + d) * SR / NFFT;
        }
    }
    /* the bass (alone): every rise of its 10 ms envelope by 6 dB or more, its 10..90 % time; and how much of the peak is
     * left 2 s after it (a pluck falls away, a drone holds) */
    double att_min = -1.0, att_med = -1.0, sus_med = -1.0;
    int rises = 0;
    if (solo == 2) {
        double att[512], sus[512];
        int t = 0;
        float emax = 0.0f;
        {   /* the 10 ms frames smoothed over 100 ms (the bass's period is up to 40 ms), and the rises that matter: to at
             * least a tenth of the loudest */
            float *sm = calloc((size_t)nenv + 1, sizeof *sm), acc = 0.0f;
            for (i = 0; i < nenv; i++) {
                acc += envl[i] * envl[i] - (i >= 10 ? envl[i - 10] * envl[i - 10] : 0.0f);
                sm[i] = sqrtf(fmaxf(acc, 0.0f) / 10.0f);
                emax = fmaxf(emax, sm[i]);
            }
            memcpy(envl, sm, (size_t)nenv * sizeof *sm);
            free(sm);
        }
        while (t < nenv - 1 && rises < 512) {
            int lo = t, hi;
            while (lo + 1 < nenv && envl[lo + 1] <= envl[lo])
                lo++;
            hi = lo;
            while (hi + 1 < nenv && envl[hi + 1] >= envl[hi] * 0.999f)
                hi++;
            if (hi > lo && envl[hi] > 0.1f * emax && envl[hi] > envl[lo] * 2.0f) {
                float a10 = envl[lo] + 0.1f * (envl[hi] - envl[lo]), a90 = envl[lo] + 0.9f * (envl[hi] - envl[lo]);
                int p = lo, q;
                while (p < hi && envl[p] < a10)
                    p++;
                q = p;
                while (q < hi && envl[q] < a90)
                    q++;
                att[rises] = (double)(q - p) * 0.01;
                sus[rises] = hi + 200 < nenv ? envl[hi + 200] / envl[hi] : 1.0;
                rises++;
            }
            t = hi + 1;
        }
        if (rises) {
            att_min = att[0];
            for (i = 1; i < rises; i++)
                att_min = fmin(att_min, att[i]);
            qsort(att, (size_t)rises, sizeof att[0], cmpd);
            qsort(sus, (size_t)rises, sizeof sus[0], cmpd);
            att_med = att[rises / 2];
            sus_med = sus[rises / 2];
        }
    }
    /* the pulse (alone): its energy in each sixteenth of each bar; each bar against the second (the first fills the
     * reverb), as the cosine of the two vectors: 1 = the same pattern, the same balance */
    double bar_sim = -1.0;
    int nbars = 0;
    if (solo == 3) {
        double step = (double)SR * 60.0 / ((double)par[P_TEMPO] * 4.0) / 441.0;   /* in envelope frames */
        double ref[16] = {0}, v[16];
        nbars = (int)(nenv / (16.0 * step));
        bar_sim = 1.0;
        for (i = 1; i < nbars; i++) {
            int s;
            double dot = 0.0, na = 0.0, nb2 = 0.0;
            for (s = 0; s < 16; s++) {
                int f0 = (int)((i * 16 + s) * step), f1 = (int)((i * 16 + s + 1) * step), f;
                v[s] = 0.0;
                for (f = f0; f < f1 && f < nenv; f++)
                    v[s] += (double)envl[f] * envl[f];
            }
            if (i == 1) {
                memcpy(ref, v, sizeof ref);
                continue;
            }
            for (s = 0; s < 16; s++)
                dot += ref[s] * v[s], na += ref[s] * ref[s], nb2 += v[s] * v[s];
            if (na > 0.0 && nb2 > 0.0)
                bar_sim = fmin(bar_sim, dot / sqrt(na * nb2));
        }
    }

    {
        double rt = (double)(c1 - c0) / CLOCKS_PER_SEC, et = eng / CLOCKS_PER_SEC;
        char out[4096];
        int n = snprintf(out, sizeof out,
            "{\"piece\": %d, \"name\": \"%s\", \"solo\": \"%s\", \"mode\": %d, \"seconds\": %.0f, \"master_q12\": %d,\n"
            " \"peak_dbfs\": %.2f, \"true_peak_dbfs\": %.2f, \"lufs\": %.2f, \"rms_dbfs\": %.2f,\n"
            " \"dc_l\": %.6f, \"dc_r\": %.6f, \"limiter_max_in\": %.3f, \"limiter_sat_frac\": %.6f,\n"
            " \"nonfinite\": %u, \"denormal\": %u,\n"
            " \"centroid_hz\": %.0f, \"band_20_45\": %.1f, \"band_45_250\": %.1f, \"band_250_2k\": %.1f,"
            " \"band_2k_6k\": %.1f, \"band_6k_16k\": %.1f,\n"
            " \"tonal_peak_prom_db\": %.1f, \"tonal_peak_rel_db\": %.1f, \"tonal_peak_hz\": %.0f, \"harsh_peaks\": %d,\n"
            " \"kick_hz\": %.1f, \"bass_attack_min_s\": %.2f, \"bass_attack_med_s\": %.2f, \"bass_sustain_2s\": %.2f,"
            " \"bass_rises\": %d,\n"
            " \"bar_similarity\": %.4f, \"bars\": %d,\n"
            " \"engine_cpu_pct_host\": %.3f, \"wall_s\": %.2f}\n",
            piece, en_piece_name(piece), solo >= 0 ? LAYER[solo] : "mix", par[P_MODE], secs, master,
            20.0 * log10(peak + 1e-12), 20.0 * log10(tpeak + 1e-12), fin(lufs), 10.0 * log10(sq / (double)done + 1e-30),
            sum_l / (double)done, sum_r / (double)done, (double)en_host.pre_max,
            (double)en_host.sat / (double)(2 * done), en_host.nonfinite, en_host.denorm, cen,
            band_db(20, 45, tot), band_db(45, 250, tot), band_db(250, 2000, tot), band_db(2000, 6000, tot),
            band_db(6000, 16000, tot), fin(harsh_db), fin(harsh_rel), harsh_f, harsh_n, fin(kick_f), att_min, att_med, fin(sus_med),
            rises, fin(bar_sim), nbars, 100.0 * et / secs, rt);
        fwrite(out, 1, (size_t)n, stdout);
        if (json && (jf = fopen(json, "w")) != 0) {
            fwrite(out, 1, (size_t)n, jf);
            fclose(jf);
        }
    }
    (void)nspec;
    return 0;
}
