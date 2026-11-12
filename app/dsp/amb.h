/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The instrument: a generative ambient garden. Four layers draw on one pool of notes (a scale:
 * the root plus up to eleven more pitch classes) and one chord that drifts slowly through it:
 *
 *   PAD     four breathing voices: each swells in, hangs and fades on its own cycle (unequal lengths,
 *           as the loops of Music for Airports), taking its note of the chord as it starts, so the
 *           chord forms and dissolves. Two colours, by piece: a filtered Rhodes (FM, blooming brighter
 *           as it swells) or Voices (saws through two formants, as "2/1"); then a soft low-pass and an
 *           ensemble chorus. The chord moves every DRIFT (20 s .. 5 min): voices on shared
 *           notes hold, the others breathe in anew.
 *   BELLS   soft two-operator FM bells played by four tape loops of unequal length (13, 17, 23, 29
 *           quarter notes), each holding a short phrase that shifts against the others, each its own
 *           shade of bell. EVOLVE changes a note now and then; DENSITY sets how many notes a loop holds.
 *   BASS    a sine and its sub-octave on the chord's root (now and then its fifth), gliding.
 *   PULSE   a soft thump, a closed hat and an open hat, each on its own Euclidean pattern (16, 12 and 7
 *           sixteenths): polyrhythms. HITS sets how many; the patterns turn and change around it.
 *
 * Over it all, the ARC: a slow tide, minutes long, between sparse (fewer bells, a darker vowel, a quieter
 * pulse, more reverb) and full; MOTION sets how far it goes. And rests: with EVOLVE, the pulse
 * sits out a few bars, the bass a chord, a bell loop a pass.
 * Then, after Discreet Music's chain, an Echoplex (160 ms, fed back) and a tape loop (to ~5.6 s, two
 * heads, each repeat darker), and Dattorro's plate (from FoMni), fed generously: the space is most of
 * what is heard.
 * FREEZE holds the chord and lets the plate ring on forever.
 *
 * Threads: en_render() runs in the audio interrupt. Everything else posts commands, which the render
 * drains at the start of a block; the state below marked (UI) is written by the render and only read
 * elsewhere. Float only, no libm: fastmath.h. */
#pragma once
#include <stdint.h>

#define EN_SR 44100.0f
#define EN_NLOOP 4                     /* bell tape loops */
#define EN_LOOP_MAX 29
#define EN_NDRUM 3                     /* pulse: thump, closed hat, open hat */
#define EN_NPAD 4                      /* pad voices */

/* modes: pools relative to the root (bit i: root + i semitones) */
enum { MODE_IONIAN, MODE_DORIAN, MODE_PHRYGIAN, MODE_LYDIAN, MODE_MIXOLYDIAN, MODE_AEOLIAN, MODE_PENT_MAJ,
       MODE_PENT_MIN, MODE_MAJ7, EN_NMODES };
extern const char *const EN_MODE_NAME[EN_NMODES];
extern const uint16_t EN_MODE_POOL[EN_NMODES];
extern const char *const EN_NOTE_NAME[12];
extern const uint8_t EN_LOOP_LEN[EN_NLOOP];
extern const uint8_t EN_DRUM_N[EN_NDRUM];

/* parameters: value ranges in en_param_info() */
enum {
    P_PAD, P_BELLS, P_BASS, P_PULSE,                  /* layer levels */
    P_DENSITY, P_EVOLVE, P_DRIFT, P_HITS,             /* motion */
    P_ECHO, P_ETIME, P_REVERB, P_SIZE,                /* space */
    P_BRIGHT, P_SWELL, P_BTONE, P_MOTION,             /* tone */
    P_TUNE, P_MIDI, P_TEMPO, P_ROOT, P_MODE, P_BOCT,  /* setup, panel encoders, bell octave */
    P_PADTONE,                                        /* the pad's colour, set by the piece: Rhodes, Voices */
    P_BASSOCT, P_GLIDE, P_FIFTH,                      /* bass: its octave, its glide, how often it leans on the fifth */
    P_SWING, P_HATS,                                  /* pulse: swing, the hats' colour (the pulse has no page now) */
    P_GRAINS, P_GSIZE, P_GREV, P_GRAND,               /* the granulator: grains a second (0 off), their size, reverse %, random % */
    P_NPARAMS
};
typedef struct {
    const char *name;
    int16_t lo, hi, def;
} en_param_t;
const en_param_t *en_param_info(int i);
void en_param_text(int i, int v, char *buf);   /* the value as the screen shows it (10 bytes) */

/* commands (main loop -> render) */
void en_init(void);
void en_set(int param, int value);
void en_pool(uint32_t pool);                   /* the scale, relative to the root (bit 0 always set) */
void en_play(int on);                          /* the garden grows / rests (the tails ring on) */
void en_next(void);                            /* the next chord: on the next bar (now while resting) */
void en_freeze(int on);
void en_seed(uint32_t s);                      /* stir some entropy into the generators */
void en_panic(void);                           /* everything quiet, now */
void en_regrow(void);                          /* a new piece: fresh loops, the home chord */
/* MIDI clock in (as FoMni): ticks drive the 1/16 grid (6 a step) while they keep coming */
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_CONTINUE, EN_CLK_STOP };
void en_clock(int msg);

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* MIDI out, from the render: the app sends it (USB); status, data 1, data 2 */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2);

/* state for the screen (UI) */
extern volatile uint8_t en_playing, en_frozen, en_ext;
extern volatile uint8_t en_chord[EN_NPAD];     /* the chord's notes (MIDI), pad voices' targets */
extern volatile uint8_t en_chord_root;         /* the chord's root, MIDI pitch class 0..11 */
extern volatile uint8_t en_chord_n;            /* its distinct tones: 3 or 4 */
extern volatile float en_drift;                /* 0..1: how far to the next chord */
extern volatile float en_pad_level, en_bass_level;
extern volatile uint8_t en_loop_pos[EN_NLOOP];
extern volatile int8_t en_loop_note[EN_NLOOP][EN_LOOP_MAX];   /* -1 rest, else the slot's offset */
extern volatile float en_loop_glow[EN_NLOOP];  /* 0..1: a bell just rang from this loop */
extern volatile uint8_t en_drum_pos[EN_NDRUM];
extern volatile uint32_t en_drum_pat[EN_NDRUM];/* bit i: a hit on step i */
extern volatile float en_drum_glow[EN_NDRUM];
extern volatile uint32_t en_bars;              /* sixteenths played since start (for tests) */
extern volatile float en_arc;                  /* the arc, -1 sparse .. +1 full (times Motion) */
extern volatile uint8_t en_pulse_rest;         /* the pulse is sitting a few bars out */
extern volatile uint8_t en_pad_note[EN_NPAD];  /* the note each voice sings now (MIDI) */
extern volatile float en_pad_amp[EN_NPAD];     /* and how loud */

/* the pure parts, for the tests and the screen */
uint32_t en_euclid(int k, int n, int rot);     /* Bjorklund's pattern, as a bitmask (bit 0 = step 0) */
int en_scale(uint32_t pool, int *steps);       /* the pool's pitch classes (relative), ascending; count */
/* the chord on scale degree deg: its tones (MIDI pitch classes, absolute), 3 or 4 of them; count */
int en_chord_tones(uint32_t pool, int root, int deg, int *pcs);
/* voice leading: new pad notes for the tones, each voice moving as little as possible, all in lo..hi */
void en_voice_lead(const int *from, const int *pcs, int ntones, int *to, int lo, int hi);
const char *en_chord_name(const int *pcs, int n, char *buf);   /* "Em7", "Dsus4", "C" ... */

/* POCKET SCION. The Instruo Pocket Scion plays the FM-1 over MIDI: its five voices come on MIDI channels 1..5, and
 * every piece answers it. The first four pieces are made for its four presets (Garden, Mycelium, Flora, Earth: a
 * quiet bed, the Scion's notes on top); the other four keep the music of AMB-1's (its generative loops go on) with
 * the Scion's notes added. A note on channel c rings a voice
 * of its own (bell family, pan and colour by channel, voicing by preset); CC 71..75 move that voice's brightness.
 * The generative garden underneath goes on as a quiet bed (the Pad, the Bass), or not at all. */
#define EN_NSCION 5                    /* the Scion's voices (MIDI channels 1..5) */
#define EN_NSCION_PRESETS 8            /* every piece answers the Scion (a voicing each) */
#define EN_NSCION_BED 4                /* pieces 0..3 are its four presets' own: no loops, their notes only */
void en_scion(int preset);             /* the piece playing (its voicing), or -1 for none */
void en_note(int ch, int note, int vel);   /* a Scion note on: voice 0..4 (channel - 1), MIDI note, velocity 1..127 */
void en_cc(int voice, int val);        /* a voice's brightness, 0..127 (en_mod to Bright, at full depth) */
/* Where a CC can be sent (the player's map, the Setup button's second page): a Scion voice's own sound, or one of the
 * garden's settings, moved up and down around where its knob is */
enum { EN_D_OFF, EN_D_BRIGHT, EN_D_LEVEL, EN_D_DECAY, EN_D_PAN, EN_D_PITCH,   /* the voice's: its tone, volume, length, place, pitch */
       EN_D_ECHO, EN_D_REVERB, EN_D_GRAINS, EN_D_GSIZE, EN_D_GREV, EN_D_GRAND,
       EN_D_PAD, EN_D_BASS, EN_D_BELLS, EN_D_TONE, EN_NDEST };
extern const char *const EN_DEST_NAME[EN_NDEST];
void en_mod(int voice, int dest, int val, int amount);   /* a CC's value 0..127 (64: no change) for the voice's map, depth 0..100 */
extern volatile float en_sc_glow[EN_NSCION];       /* 0..1: this voice just rang (UI) */
extern volatile uint8_t en_sc_count[EN_NSCION];    /* notes received, wrapping: the UI sees how many are new */
extern volatile uint8_t en_sc_note[EN_NSCION];     /* the last note of each voice (MIDI) */

/* the pieces: recipes for every parameter (Tune and MIDI out included, but they are the player's) */
#define EN_NPIECES 8
const char *en_piece_name(int i);
void en_piece(int i, int16_t *par, uint16_t *pool);
