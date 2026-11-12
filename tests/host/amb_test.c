/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The instrument on the host: the pure parts (Euclid, scales, chords, voice leading, names) and the
 * render (sound, no NaN, bounded, the loops evolve, the chord drifts, freeze holds). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "amb.h"

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

static uint32_t midi_count;
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    (void)st;
    (void)d1;
    (void)d2;
    midi_count++;
}

static uint32_t pat(const char *s)
{
    uint32_t m = 0;
    int i;
    for (i = 0; s[i]; i++)
        if (s[i] == 'x')
            m |= 1u << i;
    return m;
}

static int popcount(uint32_t m)
{
    int c = 0;
    while (m) {
        c += (int)(m & 1u);
        m >>= 1;
    }
    return c;
}

static int32_t buf[2 * 256];
static float peak, sum2;
static int bad;
static void run(float secs)
{
    int blocks = (int)(secs * 44100.0f / 256.0f), b, j;
    for (b = 0; b < blocks; b++) {
        en_render(buf, 256, 2048);
        for (j = 0; j < 512; j++) {
            float x = (float)buf[j] / 8388608.0f;
            if (x != x)
                bad++;
            if (fabsf(x) > peak)
                peak = fabsf(x);
            sum2 += x * x;
        }
    }
}

int main(void)
{
    int pcs[4], n, i;
    char name[16];

    /* Euclid: the classic patterns */
    CHECK(en_euclid(3, 8, 0) == pat("x..x..x."), "E(3,8) %x", en_euclid(3, 8, 0));
    CHECK(en_euclid(5, 8, 0) == pat("x.xx.xx."), "E(5,8) %x", en_euclid(5, 8, 0));
    CHECK(en_euclid(4, 16, 0) == pat("x...x...x...x..."), "E(4,16)");
    CHECK(en_euclid(0, 16, 0) == 0, "E(0,16)");
    CHECK(en_euclid(7, 7, 0) == 0x7Fu, "E(7,7)");
    for (i = 0; i <= 12; i++)
        CHECK(popcount(en_euclid(i, 12, 5)) == i, "E(%d,12) has %d hits", i, popcount(en_euclid(i, 12, 5)));
    CHECK(en_euclid(1, 7, 3) == 1u << 3, "rotation");

    /* scales and chords */
    {
        int st[12];
        CHECK(en_scale(EN_MODE_POOL[MODE_DORIAN], st) == 7 && st[2] == 3 && st[6] == 10, "Dorian steps");
        CHECK(en_scale(EN_MODE_POOL[MODE_PENT_MIN], st) == 5, "pentatonic minor has 5");
        CHECK(en_scale(0, st) == 1, "the root is always in");
    }
    n = en_chord_tones(EN_MODE_POOL[MODE_IONIAN], 0, 0, pcs);
    CHECK(n == 4 && pcs[0] == 0 && pcs[1] == 4 && pcs[2] == 7 && pcs[3] == 11, "C Ionian I = Cmaj7");
    CHECK(!strcmp(en_chord_name(pcs, n, name), "CM7"), "name %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_DORIAN], 2, 0, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "Dm7"), "D Dorian i: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_DORIAN], 2, 3, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "G7"), "D Dorian IV: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_PENT_MAJ], 0, 0, pcs);
    CHECK(n == 3, "pentatonic chords have three tones (%d)", n);
    n = en_chord_tones(1u | 1u << 4 | 1u << 7 | 1u << 9, 0, 0, pcs);   /* four notes: no doubled tones */
    CHECK(n == 3 && pcs[0] != pcs[1] && pcs[1] != pcs[2] && pcs[0] != pcs[2], "4-note pool: distinct");
    n = en_chord_tones(1u | 1u << 4 | 1u << 7, 0, 1, pcs);
    CHECK(n == 3, "3-note pool: all three (%d)", n);

    /* voice leading: small moves, in range, no unisons */
    {
        int from[4] = {50, 57, 62, 66}, to[4], cm7[4] = {0, 4, 7, 11}, f7[4] = {5, 9, 0, 4}, tot = 0;
        en_voice_lead(from, cm7, 4, to, 48, 77);
        for (i = 0; i < 4; i++) {
            CHECK(to[i] >= 48 && to[i] <= 77, "range %d", to[i]);
            tot += abs(to[i] - from[i]);
        }
        CHECK(tot <= 8, "Cmaj7 from D-ish: moved %d", tot);
        memcpy(from, to, sizeof from);
        en_voice_lead(from, f7, 4, to, 48, 77);
        tot = 0;
        for (i = 0; i < 4; i++)
            tot += abs(to[i] - from[i]);
        CHECK(tot <= 6, "CM7 -> FM7 moved %d", tot);
        CHECK(to[0] != to[1] && to[1] != to[2] && to[2] != to[3] && to[0] != to[3], "no unisons");
    }

    /* texts */
    {
        char t[12];
        en_param_text(P_DRIFT, 0, t);
        CHECK(!strcmp(t, "20s"), "drift 0: %s", t);
        en_param_text(P_DRIFT, 100, t);
        CHECK(!strcmp(t, "5:00"), "drift 100: %s", t);
        en_param_text(P_ETIME, 3, t);
        CHECK(!strcmp(t, "1 bar"), "etime: %s", t);
        en_param_text(P_BOCT, 1, t);
        CHECK(!strcmp(t, "+1"), "boct: %s", t);
    }

    /* the render: silent at rest, sound when growing, never NaN, bounded */
    en_init();
    run(1.0f);
    CHECK(peak == 0.0f, "silent before play (%f)", (double)peak);
    en_play(1);
    peak = sum2 = 0.0f;
    run(20.0f);
    CHECK(!bad, "NaN in the output");
    CHECK(peak > 0.05f && peak < 0.99f, "peak %f", (double)peak);
    printf("  20 s playing: peak %.3f, rms %.4f, bars %u\n", (double)peak,
           (double)sqrtf(sum2 / (20.0f * 44100.0f * 2.0f)), en_bars / 16u);
    CHECK(en_bars / 16u >= 5u, "the grid ran (%u sixteenths)", en_bars);
    CHECK(en_pad_level > 0.5f, "the pad swelled (%f)", (double)en_pad_level);

    /* the chord drifts: with Drift at its shortest, a few changes in two minutes */
    {
        int changes = 0, last = en_chord_root * 16 + en_chord[0] % 16, k;
        en_set(P_DRIFT, 0);
        for (k = 0; k < 120; k++) {
            int now;
            run(1.0f);
            now = en_chord_root * 16 + en_chord[0] % 16;
            changes += now != last;
            last = now;
        }
        CHECK(changes >= 3, "chord changes in 2 min at Drift 0: %d", changes);
        printf("  chord changes in 2 min: %d\n", changes);
    }

    /* a chord change is heard: within 2 s no sounding voice sings a note outside the new chord */
    {
        int k, i, bad_s = 0, changes = 0, last = -1;
        en_set(P_DRIFT, 0);
        for (k = 0; k < 90 * 4; k++) {
            int sig = en_chord[0] | en_chord[1] << 7 | en_chord[2] << 14 | en_chord[3] << 21;
            static int since = -1;
            run(0.25f);
            if (sig != last) {
                if (last >= 0) {
                    changes++;
                    since = 0;
                }
                last = sig;
            } else if (since >= 0 && ++since == 8) {   /* 2 s after a change */
                uint32_t pcs = 0;
                for (i = 0; i < EN_NPAD; i++)
                    pcs |= 1u << (en_chord[i] % 12);
                for (i = 0; i < EN_NPAD; i++)
                    if (en_pad_amp[i] > 0.05f && !(pcs >> (en_pad_note[i] % 12) & 1u))
                        bad_s++;
                since = -1;
            }
        }
        CHECK(changes >= 3, "chord changes in 90 s at Drift 0: %d", changes);
        CHECK(!bad_s, "%d voices still off the new chord 2 s after a change", bad_s);
    }

    /* the loops evolve: with Evolve at 100, the phrases change */
    {
        int8_t before[EN_NLOOP][EN_LOOP_MAX];
        int diff = 0, l;
        en_set(P_EVOLVE, 100);
        en_set(P_DENSITY, 80);
        memcpy(before, (const void *)en_loop_note, sizeof before);
        run(90.0f);
        for (l = 0; l < EN_NLOOP; l++)
            for (i = 0; i < EN_LOOP_MAX; i++)
                diff += before[l][i] != en_loop_note[l][i];
        CHECK(diff > 0, "the loops evolved");
        for (l = 0; l < EN_NLOOP; l++)
            for (i = EN_LOOP_LEN[l]; i < EN_LOOP_MAX; i++)
                CHECK(en_loop_note[l][i] == -1, "loop %d slot %d past its end", l, i);
    }

    /* freeze: the chord holds */
    {
        int held = en_chord[0] + 128 * en_chord[1], k;
        en_freeze(1);
        for (k = 0; k < 60; k++) {
            run(1.0f);
            CHECK(en_chord[0] + 128 * en_chord[1] == held, "the chord moved while frozen");
        }
        en_freeze(0);
    }

    /* a new key while playing (a piece, the root, the scale): within 3 s no voice still sings the old
     * chord (a voice only takes its note at its next breath otherwise, up to a minute later) */
    {
        int k, stale = 0;
        en_freeze(0);
        en_set(P_ROOT, 6);                       /* a tritone away: nothing in common to hide behind */
        en_pool(EN_MODE_POOL[MODE_LYDIAN]);
        en_regrow();
        run(3.0f);
        for (k = 0; k < EN_NPAD; k++)
            if (en_pad_amp[k] > 0.02f && en_pad_note[k] != en_chord[k]) {
                printf("  voice %d sings %d, the chord has %d (amp %.2f)\n", k, en_pad_note[k], en_chord[k],
                       (double)en_pad_amp[k]);
                stale++;
            }
        CHECK(!stale, "%d voices still in the old key after 3 s", stale);
        en_set(P_ROOT, 2);
    }

    /* a pool change: the chord's tones are in it */
    {
        uint32_t pent = EN_MODE_POOL[MODE_PENT_MIN];
        en_pool(pent);
        run(0.1f);
        for (i = 0; i < 4; i++)
            CHECK(pent >> ((en_chord[i] - 2 + 120) % 12) & 1u, "chord note %d not in the pool", en_chord[i]);
    }

    /* every setting at its extremes: still bounded */
    {
        int p;
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->hi);
        en_set(P_MIDI, 1);
        peak = 0.0f;
        run(30.0f);
        CHECK(!bad && peak < 1.0f, "all at max: peak %f", (double)peak);
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->lo);
        en_set(P_TEMPO, 30);
        run(10.0f);
        CHECK(!bad, "all at min: NaN");
    }

    /* Bell tone and Bell oct are heard: brighter (zero crossings) and higher with them, bells alone */
    {
        float zc[3];
        int k;
        static const int TONE[3] = {0, 100, 0}, OCT[3] = {0, 0, 2};
        for (k = 0; k < 3; k++) {
            int b2, j;
            long z = 0;
            float prev = 0.0f;
            en_init();
            en_set(P_PAD, 0);
            en_set(P_BASS, 0);
            en_set(P_PULSE, 0);
            en_set(P_ECHO, 0);
            en_set(P_REVERB, 0);
            en_set(P_DENSITY, 100);
            en_set(P_BTONE, TONE[k]);
            en_set(P_BOCT, OCT[k]);
            en_play(1);
            for (b2 = 0; b2 < 20 * 44100 / 256; b2++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 256; j++) {
                    float x = (float)buf[2 * j];
                    z += (x > 0.0f) != (prev > 0.0f);
                    prev = x;
                }
            }
            zc[k] = (float)z;
        }
        CHECK(zc[1] > 2.5f * zc[0], "Bell tone 100 not brighter: %.0f vs %.0f", (double)zc[1], (double)zc[0]);
        CHECK(zc[2] > 1.8f * zc[0], "Bell oct +2 not higher: %.0f vs %.0f", (double)zc[2], (double)zc[0]);
        en_init();
        en_play(1);
    }

    /* Pocket Scion: a note on a channel rings a voice of its own; the generative bed can be left out */
    {
        static float el_, er_, zc_;
        static int zn_;
        float ch_l[EN_NSCION], ch_r[EN_NSCION], dec_e[EN_NSCION_PRESETS], q0, q1;
        int k, j, b2;
        uint32_t m0;
        /* a helper in the block: render secs, energy per side in el_, er_, zero crossings in zc_ */
#define SC_QUIET()                                                                                                 \
    do {                                                                                                           \
        en_render(buf, 256, 2048); /* what the last block left queued (a play) is taken first */                   \
        en_playing = 0; /* bells still ringing from before would ring on through en_init */                        \
        en_panic();                                                                                                \
        en_render(buf, 256, 2048);                                                                                 \
        en_playing = 0;                                                                                            \
        en_init();                                                                                                 \
        en_set(P_PAD, 0);                                                                                          \
        en_set(P_BASS, 0);                                                                                         \
        en_set(P_PULSE, 0);                                                                                        \
        en_set(P_DENSITY, 0);                                                                                      \
        en_set(P_ECHO, 0);                                                                                         \
        en_set(P_REVERB, 0);                                                                                       \
        en_set(P_MIDI, 1);                                                                                         \
        en_regrow(); /* as a piece change: the loops grow again at Density 0 */                                    \
        en_play(1);                                                                                                \
    } while (0)
#define SC_RUN(secs)                                                                                               \
    do {                                                                                                           \
        float prev_ = 0.0f;                                                                                        \
        el_ = er_ = zc_ = 0.0f;                                                                                    \
        for (b2 = 0; b2 < (int)((secs) * 44100.0f / 256.0f); b2++) {                                               \
            en_render(buf, 256, 2048);                                                                             \
            for (j = 0; j < 256; j++) {                                                                            \
                float l_ = (float)buf[2 * j] / 8388608.0f, r_ = (float)buf[2 * j + 1] / 8388608.0f;                \
                if (l_ != l_ || r_ != r_)                                                                          \
                    bad++;                                                                                         \
                el_ += l_ * l_;                                                                                    \
                er_ += r_ * r_;                                                                                    \
                zc_ += (l_ > 0.0f) != (prev_ > 0.0f);                                                              \
                prev_ = l_;                                                                                        \
            }                                                                                                      \
        }                                                                                                          \
    } while (0)

        /* silence without notes, sound with one */
        SC_QUIET();
        en_scion(0);
        SC_RUN(1.0f);
        CHECK(el_ + er_ < 1e-6f, "Scion bed left out: not silent (%g)", (double)(el_ + er_));
        m0 = midi_count;
        en_note(2, 62, 100);
        SC_RUN(0.5f);
        CHECK(el_ + er_ > 1e-3f, "a Scion note makes no sound (%g)", (double)(el_ + er_));
        CHECK(midi_count > m0, "a Scion note sends no MIDI out");
        CHECK(en_sc_count[2] == 1 && en_sc_note[2] == 62 && en_sc_glow[2] > 0.1f, "Scion state: %d %d %f", en_sc_count[2],
              en_sc_note[2], (double)en_sc_glow[2]);
        SC_RUN(15.0f);
        SC_RUN(0.5f);
        CHECK(el_ + er_ < 1e-5f, "a Scion note rings on (%g)", (double)(el_ + er_));
        /* velocity 0 is a note off: nothing */
        en_note(1, 60, 0);
        SC_RUN(0.3f);
        CHECK(el_ + er_ < 1e-6f && en_sc_count[1] == 0, "velocity 0 rang");

        /* each channel its own place: 1 left .. 5 right */
        for (k = 0; k < EN_NSCION; k++) {
            SC_QUIET();
            en_scion(0);
            en_note(k, 64, 100);
            SC_RUN(1.0f);
            ch_l[k] = el_;
            ch_r[k] = er_;
        }
        CHECK(ch_l[0] > 1.5f * ch_r[0], "channel 1 not left");
        CHECK(ch_r[4] > 1.5f * ch_l[4], "channel 5 not right");
        CHECK(ch_l[2] > 0.6f * ch_r[2] && ch_r[2] > 0.6f * ch_l[2], "channel 3 not centred");

        /* the presets' voicings: Earth's drops are short, Flora's long (energy 2..4 s after the note) */
        for (k = 0; k < EN_NSCION_PRESETS; k++) {
            SC_QUIET();
            en_scion(k);
            en_note(2, 60, 100);
            SC_RUN(2.0f);
            SC_RUN(2.0f);
            dec_e[k] = el_ + er_;
        }
        CHECK(dec_e[3] < 0.35f * dec_e[2], "Earth does not die faster than Flora: %g vs %g", (double)dec_e[3],
              (double)dec_e[2]);

        /* a voice's CC: brighter with 127 than with 0 (zero crossings) */
        SC_QUIET();
        en_scion(0);
        en_cc(0, 0);
        en_note(0, 60, 100);
        SC_RUN(2.0f);
        q0 = zc_;
        SC_QUIET();
        en_scion(0);
        en_cc(0, 127);
        en_note(0, 60, 100);
        SC_RUN(2.0f);
        q1 = zc_;
        CHECK(q1 > 1.15f * q0, "CC 127 not brighter than CC 0: %.0f vs %.0f", (double)q1, (double)q0);
        zn_ = 0;
        (void)zn_;

        /* a burst (a ratcheting Scion), at the extremes: bounded, no NaN */
        SC_QUIET();
        en_scion(1);
        en_set(P_BELLS, 100);
        en_set(P_BTONE, 100);
        peak = 0.0f;
        bad = 0;
        for (k = 0; k < 400; k++) {
            en_note(k % EN_NSCION, 24 + (k * 7) % 90, 1 + (k * 13) % 127);
            en_cc(k % EN_NSCION, k % 128);
            run(0.02f);
        }
        run(5.0f);
        CHECK(!bad && peak < 1.0f, "a burst of Scion notes: bad %d peak %f", bad, (double)peak);
        run(25.0f);
        peak = 0.0f;
        run(1.0f);
        CHECK(peak < 0.01f, "the burst's tails do not die (%f)", (double)peak);
        bad = 0;

        /* the granulator: off by default; with Granules a cloud of the recent past is added; at its extremes it stays sane */
        {
            float base_e, e1;
            int sz, rv, rn;
            SC_QUIET();
            en_scion(0);
            en_note(2, 60, 100);
            SC_RUN(6.0f);
            base_e = el_ + er_;
            SC_QUIET();
            en_scion(0);
            en_set(P_GRAINS, 100);
            en_set(P_GSIZE, 60);
            en_set(P_GRAND, 60);
            en_note(2, 60, 100);
            SC_RUN(6.0f);
            e1 = el_ + er_;
            CHECK(e1 > 1.2f * base_e, "Granules adds nothing: %g vs %g", (double)e1, (double)base_e);
            for (sz = 0; sz <= 100; sz += 100)
                for (rv = 0; rv <= 100; rv += 100)
                    for (rn = 0; rn <= 100; rn += 100) {
                        SC_QUIET();
                        en_scion(1);
                        en_set(P_BELLS, 100);
                        en_set(P_GRAINS, 100);
                        en_set(P_GSIZE, sz);
                        en_set(P_GREV, rv);
                        en_set(P_GRAND, rn);
                        bad = 0;
                        peak = 0.0f;
                        for (k = 0; k < 120; k++) {
                            en_note(k % EN_NSCION, 24 + (k * 11) % 90, 1 + (k * 17) % 127);
                            run(0.05f);
                        }
                        run(4.0f);
                        CHECK(!bad && peak < 1.0f, "granulator size %d rev %d rand %d: bad %d peak %f", sz, rv, rn, bad, (double)peak);
                    }
            /* and it stops: Granules 0, the grains end, a quiet room */
            SC_QUIET();
            en_scion(0);
            en_set(P_GRAINS, 100);
            en_note(2, 60, 100);
            SC_RUN(1.0f);
            en_set(P_GRAINS, 0);
            SC_RUN(20.0f);
            SC_RUN(1.0f);
            CHECK(el_ + er_ < 1e-5f, "the grains go on after Granules 0 (%g)", (double)(el_ + er_));
        }

        /* the CC map: a voice's tone, level, length, place and pitch; a setting moved around its knob; Off, and depth 0 */
        {
            float e0, e2, l4, r4;
            SC_QUIET();
            en_scion(0);
            en_note(0, 60, 100);
            SC_RUN(6.0f);
            e0 = el_ + er_;
            q0 = zc_;
            SC_QUIET();
            en_scion(0);
            en_mod(0, EN_D_LEVEL, 0, 100);
            en_note(0, 60, 100);
            SC_RUN(6.0f);
            CHECK(el_ + er_ < 0.05f * e0, "CC to Level at 0 does not silence the voice");
            SC_QUIET();
            en_scion(0);
            en_mod(0, EN_D_LEVEL, 127, 100);
            en_note(0, 60, 100);
            SC_RUN(6.0f);
            CHECK(el_ + er_ > 1.8f * e0, "CC to Level at 127 does not raise the voice (%g vs %g)", (double)(el_ + er_), (double)e0);
            SC_QUIET();
            en_scion(0);
            en_mod(0, EN_D_DECAY, 127, 100);
            en_note(0, 60, 100);
            SC_RUN(6.0f);
            e2 = el_ + er_;
            CHECK(e2 > 1.3f * e0, "CC to Decay does not lengthen the ring (%g vs %g)", (double)e2, (double)e0);
            SC_QUIET();
            en_scion(0);
            en_mod(0, EN_D_PITCH, 127, 100);
            en_note(0, 60, 100);
            SC_RUN(2.0f);
            q1 = zc_;
            SC_QUIET();
            en_scion(0);
            en_note(0, 60, 100);
            SC_RUN(2.0f);
            CHECK(q1 > 1.06f * zc_, "CC to Pitch does not raise the note: %.0f vs %.0f", (double)q1, (double)zc_);
            SC_QUIET();
            en_scion(0);
            en_mod(4, EN_D_PAN, 0, 100);        /* voice 5 is on the right: pushed all the way left */
            en_note(4, 64, 100);
            SC_RUN(1.0f);
            l4 = el_;
            r4 = er_;
            CHECK(l4 > 1.5f * r4, "CC to Pan does not move voice 5 to the left");
            SC_QUIET();
            en_scion(0);
            en_mod(0, EN_D_GRAINS, 127, 100);   /* a setting: Granules, from its knob (0) up to 100 */
            en_note(0, 60, 100);
            SC_RUN(6.0f);
            CHECK(el_ + er_ > 1.05f * e0, "CC to Granules adds no grains (%g vs %g)", (double)(el_ + er_), (double)e0);
            SC_QUIET();
            en_scion(0);
            en_mod(0, EN_D_LEVEL, 0, 0);        /* depth 0: no change */
            en_mod(0, EN_D_OFF, 0, 100);        /* Off: nothing */
            en_note(0, 60, 100);
            SC_RUN(6.0f);
            CHECK(el_ + er_ > 0.9f * e0 && el_ + er_ < 1.1f * e0, "depth 0 or Off changed the voice (%g vs %g)", (double)(el_ + er_), (double)e0);
            CHECK(!strcmp(EN_DEST_NAME[EN_D_GRAINS], "Grains") && !strcmp(EN_DEST_NAME[EN_D_OFF], "Off"), "destination names");
        }

        /* the pieces: the four Scion presets and their keys */
        {
            int16_t pr[P_NPARAMS];
            uint16_t pl;
            static const int ROOT[EN_NSCION_BED] = {2, 9, 5, 2};
            for (k = 0; k < EN_NSCION_BED; k++) {
                en_piece(k, pr, &pl);
                CHECK(pr[P_ROOT] == ROOT[k] && pr[P_DENSITY] == 0 && pr[P_PULSE] == 0, "Scion piece %d recipe", k);
                CHECK(pl == EN_MODE_POOL[pr[P_MODE]], "Scion piece %d pool", k);
            }
            CHECK(!strcmp(en_piece_name(0), "Garden") && !strcmp(en_piece_name(3), "Earth"), "Scion piece names");
            CHECK(!strcmp(en_piece_name(4), "Posidonia") && !strcmp(en_piece_name(5), "Night") &&
                      !strcmp(en_piece_name(6), "Lighthouse") && !strcmp(en_piece_name(7), "Shoreline"),
                  "scene piece names");
            for (k = EN_NSCION_BED; k < EN_NSCION_PRESETS; k++) {     /* the four with AMB-1's music keep it: loops and a pulse */
                en_piece(k, pr, &pl);
                CHECK(pr[P_DENSITY] > 0 && pr[P_TEMPO] > 0, "piece %d keeps AMB-1's music", k);
            }
            /* and every piece's voicing rings the Scion's note */
            for (k = 0; k < EN_NSCION_PRESETS; k++) {
                SC_QUIET();
                en_scion(k);
                en_note(2, 64, 100);
                SC_RUN(1.0f);
                CHECK(el_ + er_ > 1e-3f, "piece %d: a Scion note makes no sound", k);
            }
            en_piece(2, pr, &pl);
            CHECK(pl == (1u | 1u << 4 | 1u << 7 | 1u << 11), "Flora is F maj7: F A C E");
        }

        /* the four-note Maj7 pool and the chromatic one: chords stay in the pool and the engine stays sane */
        {
            static const uint32_t POOLS[2] = {1u | 1u << 4 | 1u << 7 | 1u << 11, 0xFFFu};
            int p2;
            for (p2 = 0; p2 < 2; p2++) {
                int bad_chord = 0, changes = 0, last = -1;
                SC_QUIET();
                en_set(P_PAD, 80);
                en_set(P_ROOT, 5);
                en_set(P_DRIFT, 0);
                en_pool(POOLS[p2]);
                en_regrow();
                bad = 0;
                peak = 0.0f;
                for (k = 0; k < 360; k++) {
                    run(1.0f);
                    for (i = 0; i < 4; i++)
                        if (!(POOLS[p2] >> ((en_chord[i] - 5 + 120) % 12) & 1u))
                            bad_chord++;
                    if (last != en_chord[0] + 128 * en_chord[1]) {
                        changes++;
                        last = en_chord[0] + 128 * en_chord[1];
                    }
                }
                CHECK(!bad_chord, "pool %d: %d chord notes outside the pool", p2, bad_chord);
                CHECK(!bad && peak > 0.001f && peak < 1.0f, "pool %d: bad %d peak %f", p2, bad, (double)peak);
                CHECK(changes > 2, "pool %d: the chord never moves (%d)", p2, changes);
            }
        }
        en_init();
        en_play(1);
    }
#undef SC_QUIET
#undef SC_RUN

    /* stop: the tails die away */
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, en_param_info(i)->def);
    en_play(0);
    run(40.0f);
    peak = 0.0f;
    run(1.0f);
    CHECK(peak < 0.01f, "quiet after stopping (%f)", (double)peak);
    CHECK(midi_count > 50u, "MIDI out (%u)", midi_count);

    printf(fails ? "amb: %d FAILED\n" : "amb: ok\n", fails);
    return fails != 0;
}
