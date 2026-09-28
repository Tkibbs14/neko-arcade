/* Neko Arcade synth: 3 effect voices + 3 music voices, integer only, 48 kHz stereo.
 * Effects are short step lists with pitch slides; music is a tiny tracker whose patterns are
 * written as note strings ("C4 . E4 . - ." : '.' holds, '-' releases). */
#include "engine.h"

enum { W_SQUARE, W_TRIANGLE, W_NOISE, W_SAW };

typedef struct {
    u8 wave, duty;           /* duty 0..255 for squares */
    u16 f0, f1;              /* start/end frequency, Hz */
    u16 ms;
    u8 v0, v1;               /* start/end volume 0..255 */
} Step;

typedef struct {
    u32 phase, inc;
    i32 inc_step;            /* added to inc every sample (slides) */
    i32 vol, vol_step;       /* 16.16 fixed volume */
    u32 left;                /* samples left in this step */
    u8 wave, duty, active;
    u32 lfsr;
    i32 noise;
    const Step *steps;
    int nsteps, step;
    /* music note state */
    i32 sustain;
    u8 releasing;
} Voice;

#define NSFX 3
#define NMUS 3
static Voice voices[NSFX + NMUS];
int audio_music_volume = 150;

static u32 hz_to_inc(u32 hz) { return (u32)(((u64)hz << 32) / AUDIO_RATE); }

/* ---------------------------------------------------------------- effects */

#define S(w, d, a, b, ms, v0, v1) { w, d, a, b, ms, v0, v1 }
static const Step fx_blip[] = { S(W_SQUARE, 64, 1320, 1320, 25, 90, 0) };
static const Step fx_ok[] = { S(W_SQUARE, 128, 880, 880, 45, 110, 60), S(W_SQUARE, 128, 1320, 1320, 80, 110, 0) };
static const Step fx_back[] = { S(W_SQUARE, 128, 660, 440, 90, 100, 0) };
static const Step fx_move[] = { S(W_SQUARE, 32, 990, 990, 20, 70, 0) };
static const Step fx_meow[] = { S(W_SAW, 0, 520, 900, 110, 40, 130), S(W_SAW, 0, 900, 620, 220, 130, 0) };
static const Step fx_meow_sad[] = { S(W_SAW, 0, 700, 760, 90, 30, 110), S(W_SAW, 0, 760, 420, 320, 110, 0) };
static const Step fx_purr[] = { S(W_NOISE, 0, 60, 60, 160, 0, 70), S(W_NOISE, 0, 55, 55, 160, 70, 20), S(W_NOISE, 0, 60, 60, 160, 20, 70), S(W_NOISE, 0, 55, 55, 200, 70, 0) };
static const Step fx_hit[] = { S(W_SQUARE, 96, 330, 200, 50, 150, 0) };
static const Step fx_wall[] = { S(W_TRIANGLE, 0, 220, 160, 40, 150, 0) };
static const Step fx_goal[] = { S(W_SQUARE, 128, 660, 660, 90, 120, 100), S(W_SQUARE, 128, 880, 880, 90, 120, 100), S(W_SQUARE, 128, 1320, 1320, 240, 120, 0) };
static const Step fx_lose[] = { S(W_SQUARE, 128, 440, 415, 120, 120, 100), S(W_SQUARE, 128, 370, 349, 140, 110, 90), S(W_SQUARE, 128, 294, 262, 300, 110, 0) };
static const Step fx_coin[] = { S(W_SQUARE, 64, 988, 988, 60, 110, 90), S(W_SQUARE, 64, 1319, 1319, 180, 110, 0) };
static const Step fx_splash[] = { S(W_NOISE, 0, 3000, 800, 220, 140, 0) };
static const Step fx_buzz[] = { S(W_SQUARE, 128, 110, 104, 220, 120, 0) };
static const Step fx_ding[] = { S(W_TRIANGLE, 0, 1760, 1760, 400, 160, 0) };
static const Step fx_pop[] = { S(W_SQUARE, 64, 400, 1200, 40, 120, 0) };
static const Step fx_whoosh[] = { S(W_NOISE, 0, 1200, 4000, 160, 20, 90), S(W_NOISE, 0, 4000, 1500, 160, 90, 0) };
static const Step fx_step[] = { S(W_NOISE, 0, 900, 700, 30, 50, 0) };
/* the dating sim: knuckles on a wooden door, a quieter one, thunder, a small chime for a moment she will keep */
static const Step fx_knock[] = { S(W_NOISE, 0, 2200, 500, 14, 170, 0), S(W_TRIANGLE, 0, 150, 90, 55, 170, 0) };
static const Step fx_knock_soft[] = { S(W_NOISE, 0, 1400, 400, 12, 70, 0), S(W_TRIANGLE, 0, 130, 85, 45, 90, 0) };
static const Step fx_thunder[] = { S(W_NOISE, 0, 240, 90, 220, 20, 160), S(W_NOISE, 0, 120, 40, 1100, 160, 0) };
static const Step fx_chime[] = { S(W_TRIANGLE, 0, 1319, 1319, 90, 100, 70), S(W_TRIANGLE, 0, 1760, 1760, 300, 100, 0) };

static const struct { const Step *s; int n; } fx_table[SFX_COUNT] = {
#define E(a) { a, (int)(sizeof(a) / sizeof(a[0])) }
    E(fx_blip), E(fx_ok), E(fx_back), E(fx_move), E(fx_meow), E(fx_meow_sad), E(fx_purr), E(fx_hit),
    E(fx_wall), E(fx_goal), E(fx_lose), E(fx_coin), E(fx_splash), E(fx_buzz), E(fx_ding), E(fx_pop),
    E(fx_whoosh), E(fx_step), E(fx_knock), E(fx_knock_soft), E(fx_thunder), E(fx_chime),
#undef E
};

static void voice_step(Voice *v)
{
    const Step *s = &v->steps[v->step];
    u32 n = (u32)s->ms * (AUDIO_RATE / 1000);
    v->wave = s->wave;
    v->duty = s->duty;
    v->inc = hz_to_inc(s->f0);
    v->inc_step = n ? (i32)(((i64)hz_to_inc(s->f1) - (i64)v->inc) / (i64)n) : 0;
    v->vol = (i32)s->v0 << 16;
    v->vol_step = n ? (((i32)s->v1 - (i32)s->v0) << 16) / (i32)n : 0;
    v->left = n;
    v->active = 1;
}

static void voice_play(Voice *v, const Step *steps, int n)
{
    v->steps = steps;
    v->nsteps = n;
    v->step = 0;
    v->releasing = 0;
    if (!v->lfsr)
        v->lfsr = 0xACE1u;
    voice_step(v);
}

void sfx(int id)
{
    if (id < 0 || id >= SFX_COUNT)
        return;
    /* free voice, else the one closest to finishing */
    Voice *best = &voices[0];
    for (int i = 0; i < NSFX; i++) {
        if (!voices[i].active) { best = &voices[i]; break; }
        if (voices[i].left < best->left) best = &voices[i];
    }
    voice_play(best, fx_table[id].s, fx_table[id].n);
}

void sfx_voice(int pitch)
{
    static Step blip;
    /* A4 * 2^(pitch/12), from a semitone table */
    static const u16 semis[12] = { 440, 466, 494, 523, 554, 587, 622, 659, 698, 740, 784, 831 };
    int oct = 0;
    while (pitch < 0) { pitch += 12; oct--; }
    while (pitch >= 12) { pitch -= 12; oct++; }
    u32 f = semis[pitch];
    f = oct >= 0 ? f << oct : f >> -oct;
    blip.wave = W_SQUARE; blip.duty = 96;
    blip.f0 = (u16)f; blip.f1 = (u16)(f + f / 16);
    blip.ms = 28; blip.v0 = 55; blip.v1 = 0;
    voice_play(&voices[NSFX - 1], &blip, 1);
}

/* ---------------------------------------------------------------- music */

typedef struct {
    int step_samples;          /* samples per 16th note */
    const char *chan[NMUS];    /* patterns */
    u8 wave[NMUS], duty[NMUS], vol[NMUS];
} Tune;

/* Cozy lo-fi loop for Nia's desk: Fmaj7 - Em7 - Dm7 - Cmaj7. */
static const Tune tune_menu = { 48000 * 9 / 60, {
    "A4 . . . C5 . E5 . . . C5 . A4 . G4 . "
    "G4 . . . B4 . D5 . . . B4 . G4 . E4 . "
    "F4 . . . A4 . C5 . . . A4 . F4 . D4 . "
    "E4 . . . G4 . B4 . . . G4 . E4 . . . ",
    "F2 . . . . . . . C3 . . . . . . . "
    "E2 . . . . . . . B2 . . . . . . . "
    "D2 . . . . . . . A2 . . . . . . . "
    "C2 . . . . . . . G2 . . . E2 . . . ",
    "- . F3 . - . A3 . - . F3 . - . A3 . "
    "- . E3 . - . G3 . - . E3 . - . G3 . "
    "- . D3 . - . F3 . - . D3 . - . F3 . "
    "- . C3 . - . E3 . - . C3 . - . G3 . " },
    { W_SQUARE, W_TRIANGLE, W_TRIANGLE }, { 64, 0, 0 }, { 52, 120, 60 } };

/* Bouncy arcade loop for paw hockey. */
static const Tune tune_hockey = { 48000 * 7 / 60, {
    "C5 . E5 . G5 . E5 . C5 . G4 . A4 . C5 . "
    "D5 . F5 . A5 . F5 . D5 . A4 . B4 . D5 . "
    "E5 . G5 . C6 . G5 . E5 . C5 . D5 . E5 . "
    "F5 . E5 . D5 . C5 . B4 . G4 . C5 . - . ",
    "C3 . C3 . G2 . G2 . A2 . A2 . E2 . E2 . "
    "D3 . D3 . A2 . A2 . G2 . G2 . G2 . G2 . "
    "C3 . C3 . G2 . G2 . A2 . A2 . E2 . E2 . "
    "F2 . F2 . G2 . G2 . C3 . G2 . C3 . - . ",
    "C4 E4 G4 E4 C4 E4 G4 E4 A3 C4 E4 C4 A3 C4 E4 C4 "
    "D4 F4 A4 F4 D4 F4 A4 F4 G3 B3 D4 B3 G3 B3 D4 B3 "
    "C4 E4 G4 E4 C4 E4 G4 E4 A3 C4 E4 C4 A3 C4 E4 C4 "
    "F3 A3 C4 A3 G3 B3 D4 B3 C4 E4 G4 E4 C4 . - . " },
    { W_SQUARE, W_TRIANGLE, W_SQUARE }, { 128, 0, 32 }, { 46, 130, 22 } };

/* Sneaky minor loop for the detective. */
static const Tune tune_mystery = { 48000 * 10 / 60, {
    "A4 . . . - . C5 . B4 . . . G#4 . . . "
    "A4 . . . - . E5 . D5 . C5 . B4 . . . "
    "A4 . . . - . C5 . B4 . . . G#4 . . . "
    "E4 . F4 . E4 . D#4 . E4 . . . - . . . ",
    "A2 . - . A2 . - . E2 . - . E2 . - . "
    "F2 . - . F2 . - . E2 . - . E2 . - . "
    "A2 . - . A2 . - . E2 . - . E2 . - . "
    "D2 . - . D2 . - . E2 . - . E2 . - . ",
    "- . - . A3 . - . - . - . B3 . - . "
    "- . - . C4 . - . - . - . B3 . - . "
    "- . - . A3 . - . - . - . B3 . - . "
    "- . - . F3 . - . - . - . G#3 . - . " },
    { W_TRIANGLE, W_TRIANGLE, W_SQUARE }, { 0, 0, 32 }, { 80, 120, 26 } };

/* Busy cafe shuffle. */
static const Tune tune_cafe = { 48000 * 8 / 60, {
    "G4 . B4 . D5 . B4 . C5 . A4 . F#4 . A4 . "
    "G4 . B4 . D5 . G5 . F#5 . D5 . A4 . . . "
    "E5 . C5 . A4 . C5 . D5 . B4 . G4 . B4 . "
    "C5 . A4 . F#4 . D4 . G4 . . . - . . . ",
    "G2 . D3 . G2 . D3 . D2 . A2 . D2 . A2 . "
    "G2 . D3 . G2 . D3 . D2 . A2 . D2 . F#2 . "
    "C3 . G2 . C3 . G2 . G2 . D3 . G2 . D3 . "
    "D2 . A2 . D2 . A2 . G2 . D3 . G2 . - . ",
    "- G3 - B3 - G3 - B3 - F#3 - A3 - F#3 - A3 "
    "- G3 - B3 - G3 - B3 - F#3 - A3 - F#3 - A3 "
    "- E3 - G3 - E3 - G3 - D3 - G3 - D3 - G3 "
    "- F#3 - A3 - F#3 - A3 - G3 - B3 - . - . " },
    { W_SQUARE, W_TRIANGLE, W_SQUARE }, { 64, 0, 128 }, { 44, 120, 18 } };

/* Pastoral village walk. */
static const Tune tune_village = { 48000 * 9 / 60, {
    "E5 . D5 . C5 . D5 . E5 . E5 . E5 . . . "
    "D5 . D5 . D5 . . . E5 . G5 . G5 . . . "
    "E5 . D5 . C5 . D5 . E5 . E5 . E5 . C5 . "
    "D5 . D5 . E5 . D5 . C5 . . . - . . . ",
    "C3 . . . G2 . . . C3 . . . G2 . . . "
    "G2 . . . D3 . . . C3 . . . G2 . . . "
    "C3 . . . G2 . . . A2 . . . E2 . . . "
    "F2 . . . G2 . . . C3 . . . G2 . . . ",
    "- . C4 . - . E4 . - . C4 . - . E4 . "
    "- . B3 . - . D4 . - . C4 . - . E4 . "
    "- . C4 . - . E4 . - . A3 . - . C4 . "
    "- . A3 . - . B3 . - . C4 . - . . . " },
    { W_TRIANGLE, W_TRIANGLE, W_SQUARE }, { 0, 0, 32 }, { 90, 110, 22 } };

/* Gentle lullaby for the kittens. */
static const Tune tune_kittens = { 48000 * 12 / 60, {
    "C5 . E5 . G5 . E5 . F5 . D5 . B4 . . . "
    "A4 . C5 . E5 . C5 . D5 . B4 . G4 . . . "
    "C5 . E5 . G5 . C6 . B5 . G5 . E5 . . . "
    "F5 . E5 . D5 . B4 . C5 . . . - . . . ",
    "C3 . . . . . . . G2 . . . . . . . "
    "A2 . . . . . . . G2 . . . . . . . "
    "C3 . . . . . . . E2 . . . . . . . "
    "F2 . . . G2 . . . C3 . . . . . . . ",
    "- . - . - . - . - . - . - . - . "
    "- . - . - . - . - . - . - . - . "
    "- . - . - . - . - . - . - . - . "
    "- . - . - . - . - . - . - . - . " },
    { W_TRIANGLE, W_TRIANGLE, W_TRIANGLE }, { 0, 0, 0 }, { 90, 100, 0 } };

/* Thin Walls: a slow sweet loop, D major, I - vi - IV - V (Dmaj7 - Bm7 - Gmaj7 - A). */
static const Tune tune_date = { 48000 * 11 / 60, {
    "F#5 . . . E5 . D5 . A4 . . . C#5 . D5 . "
    "D5 . . . C#5 . B4 . F#4 . . . A4 . B4 . "
    "B4 . . . A4 . G4 . D5 . . . F#5 . E5 . "
    "E5 . . . D5 . C#5 . A4 . . . - . . . ",
    "D2 . . . . . . . A2 . . . . . . . "
    "B1 . . . . . . . F#2 . . . . . . . "
    "G1 . . . . . . . D2 . . . . . . . "
    "A1 . . . . . . . E2 . . . C#2 . . . ",
    "- . A3 . - . C#4 . - . F#3 . - . A3 . "
    "- . F#3 . - . A3 . - . D3 . - . F#3 . "
    "- . D3 . - . F#3 . - . B3 . - . D4 . "
    "- . C#3 . - . E3 . - . G3 . - . A3 . " },
    { W_SQUARE, W_TRIANGLE, W_TRIANGLE }, { 48, 0, 0 }, { 44, 110, 56 } };

static const Tune *const tunes[MUS_COUNT] = { &tune_menu, &tune_hockey, &tune_cafe, &tune_mystery, &tune_village, &tune_kittens,
                                              &tune_date };

#define MAX_STEPS 128
static i8 pat[NMUS][MAX_STEPS];     /* >0 midi note, 0 hold, -1 release */
static int pat_len;
static const Tune *cur_tune;
static int mus_pos, mus_sample;

static int parse_note(const char *s, int *adv)
{
    static const i8 base[7] = { 9, 11, 0, 2, 4, 5, 7 };   /* A B C D E F G */
    int i = 0;
    while (s[i] == ' ') i++;
    if (!s[i]) { *adv = i; return -2; }
    if (s[i] == '.') { *adv = i + 1; return 0; }
    if (s[i] == '-') { *adv = i + 1; return -1; }
    int n = base[s[i] - 'A'];
    i++;
    if (s[i] == '#') { n++; i++; }
    int oct = s[i++] - '0';
    *adv = i;
    return 12 * (oct + 1) + n;
}

void music(int track)
{
    for (int c = 0; c < NMUS; c++)
        voices[NSFX + c].active = 0;
    cur_tune = (track >= 0 && track < MUS_COUNT) ? tunes[track] : 0;
    if (!cur_tune)
        return;
    pat_len = 0;
    for (int c = 0; c < NMUS; c++) {
        const char *s = cur_tune->chan[c];
        int n = 0, adv;
        for (;;) {
            int v = parse_note(s, &adv);
            if (v == -2 || n >= MAX_STEPS) break;
            pat[c][n++] = (i8)v;
            s += adv;
        }
        if (n > pat_len) pat_len = n;
    }
    mus_pos = 0;
    mus_sample = 0;
}

static u32 midi_hz(int note)
{
    static const u16 a4_semis[12] = { 440, 466, 494, 523, 554, 587, 622, 659, 698, 740, 784, 831 };
    int d = note - 69, oct = 0;
    while (d < 0) { d += 12; oct--; }
    while (d >= 12) { d -= 12; oct++; }
    u32 f = a4_semis[d];
    return oct >= 0 ? f << oct : f >> -oct;
}

static void music_tick(void)
{
    for (int c = 0; c < NMUS; c++) {
        Voice *v = &voices[NSFX + c];
        int e = mus_pos < pat_len ? pat[c][mus_pos] : 0;
        if (e > 0) {
            v->wave = cur_tune->wave[c];
            v->duty = cur_tune->duty[c];
            v->inc = hz_to_inc(midi_hz(e));
            v->inc_step = 0;
            v->vol = (i32)cur_tune->vol[c] << 16;
            v->sustain = v->vol * 3 / 5;
            v->vol_step = -(v->vol - v->sustain) / (cur_tune->step_samples * 2);
            v->left = 0xFFFFFFFFu;
            v->steps = 0;
            v->releasing = 0;
            v->active = cur_tune->vol[c] > 0;
            if (!v->lfsr) v->lfsr = 0xBEEFu;
        } else if (e < 0 && v->active) {
            v->releasing = 1;
            v->vol_step = -(v->vol / (AUDIO_RATE / 40) + 1);
        }
    }
    mus_pos = (mus_pos + 1) % (pat_len ? pat_len : 1);
}

/* ---------------------------------------------------------------- mixer */

static inline i32 voice_sample(Voice *v)
{
    i32 amp = v->vol >> 16;
    i32 s;
    u32 ph = v->phase;
    switch (v->wave) {
    case W_SQUARE:
        s = (ph >> 24) < v->duty ? amp : -amp;
        if (!v->duty) s = (ph >> 31) ? amp : -amp;
        break;
    case W_TRIANGLE: {
        i32 t = (i32)(ph >> 23);                 /* 0..511 */
        s = (t < 256 ? t - 128 : 383 - t) * amp / 128;
        break;
    }
    case W_SAW:
        s = ((i32)(ph >> 24) - 128) * amp / 128;
        break;
    default: {                                   /* noise, clocked at the voice frequency */
        u32 next = ph + v->inc;
        if (next < ph) {
            u32 l = v->lfsr;
            u32 bit = (l ^ (l >> 2) ^ (l >> 3) ^ (l >> 5)) & 1;
            v->lfsr = (l >> 1) | (bit << 15);
            v->noise = (v->lfsr & 1) ? amp : -amp;
        }
        s = v->noise;
        if (s > amp) s = amp;
        if (s < -amp) s = -amp;
        break;
    }
    }
    v->phase += v->inc;
    return s;
}

void audio_render(i16 *out, int frames)
{
    for (int i = 0; i < frames; i++) {
        if (cur_tune && pat_len) {
            if (mus_sample <= 0) {
                music_tick();
                mus_sample += cur_tune->step_samples;
            }
            mus_sample--;
        }
        i32 fx = 0, mu = 0;
        for (int k = 0; k < NSFX + NMUS; k++) {
            Voice *v = &voices[k];
            if (!v->active)
                continue;
            i32 s = voice_sample(v);
            if (k < NSFX) fx += s; else mu += s;
            v->inc = (u32)((i32)v->inc + v->inc_step);
            v->vol += v->vol_step;
            if (k >= NSFX) {
                if (!v->releasing && v->vol < v->sustain) { v->vol = v->sustain; v->vol_step = 0; }
                if (v->vol <= 0) { v->vol = 0; v->active = 0; }
                continue;
            }
            if (v->vol < 0) v->vol = 0;
            if (--v->left == 0) {
                if (++v->step < v->nsteps) voice_step(v);
                else v->active = 0;
            }
        }
        i32 mix = fx * 48 + ((mu * audio_music_volume) >> 8) * 48;
        if (mix > 32767) mix = 32767;
        if (mix < -32768) mix = -32768;
        out[2 * i] = (i16)mix;
        out[2 * i + 1] = (i16)mix;
    }
}
