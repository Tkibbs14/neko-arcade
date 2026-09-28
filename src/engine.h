/* Neko Arcade engine: 320x180 RGB565 frame, two RetroPad players, integer math, a small synth.
 * Game code never calls the C library, so the same sources build for the M15 stick (libretro core),
 * a native test core and WebAssembly. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "gen/art_data.h"

#define SCREEN_W 320
#define SCREEN_H 180
#define AUDIO_RATE 48000
#define AUDIO_PER_FRAME 800

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#define RGB(r, g, b) ((u16)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

/* RetroPad buttons, bit = libretro joypad id. The M15 pads are mapped by label: A confirms, B backs. */
enum {
    BTN_B = 1 << 0, BTN_Y = 1 << 1, BTN_SELECT = 1 << 2, BTN_START = 1 << 3,
    BTN_UP = 1 << 4, BTN_DOWN = 1 << 5, BTN_LEFT = 1 << 6, BTN_RIGHT = 1 << 7,
    BTN_A = 1 << 8, BTN_X = 1 << 9, BTN_L = 1 << 10, BTN_R = 1 << 11,
};
#define BTN_DIRS (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)

/* Shared colours (cozy night palette). */
#define C_INK     RGB(42, 31, 51)
#define C_NIGHT   RGB(26, 22, 44)
#define C_PANEL   RGB(44, 38, 72)
#define C_PANEL2  RGB(62, 54, 98)
#define C_WHITE   RGB(255, 248, 240)
#define C_GREY    RGB(200, 192, 204)
#define C_DIM     RGB(138, 130, 148)
#define C_PINK    RGB(255, 122, 168)
#define C_ROSE    RGB(217, 70, 126)
#define C_GOLD    RGB(255, 208, 96)
#define C_MINT    RGB(122, 224, 192)
#define C_SKY     RGB(106, 168, 255)
#define C_ORANGE  RGB(255, 160, 74)
#define C_RED     RGB(255, 90, 110)
#define C_VIOLET  RGB(168, 144, 224)
#define C_MAGENTA RGB(222, 70, 170)

/* ---- frame, input ---- */
extern u16 fb[SCREEN_W * SCREEN_H];
extern u32 frame_count;
void input_update(u16 p1, u16 p2);
int btn_held(int player, u16 mask);
int btn_pressed(int player, u16 mask);
int btn_released(int player, u16 mask);
int btn_repeat(int player, u16 mask);     /* pressed, then auto-repeat while held */
int any_pressed(u16 mask);                /* either player */
int any_repeat(u16 mask);

/* ---- drawing ---- */
void cls(u16 c);
void pset(int x, int y, u16 c);
void rect(int x, int y, int w, int h, u16 c);
void rect_blend(int x, int y, int w, int h, u16 c, int alpha);   /* alpha 0..256 */
void rect_line(int x, int y, int w, int h, u16 c);
void round_box(int x, int y, int w, int h, u16 border, u16 fill);
void hline(int x, int y, int w, u16 c);
void vline(int x, int y, int h, u16 c);
void line(int x0, int y0, int x1, int y1, u16 c);
void circle_fill(int cx, int cy, int r, u16 c);
void circle_line(int cx, int cy, int r, u16 c);
void tri_fill(int x0, int y0, int x1, int y1, int x2, int y2, u16 c);
void ellipse_fill(int cx, int cy, int rx, int ry, u16 c);
void gradient_v(int x, int y, int w, int h, u16 top, u16 bottom);
void dither_blend(int x, int y, int w, int h, u16 c, int level);  /* 0..4 checker density */
u16 blend(u16 a, u16 b, int t);   /* t 0..256: 0 = a, 256 = b */

#define FLIP_X 1
void spr(int id, int x, int y, int flags);
void spr_ramp(int id, int x, int y, int flags, const u16 *ramp);
void spr_scaled(int id, int x, int y, int scale, int flags, const u16 *ramp);
void spr_tinted(int id, int x, int y, int flags, const u16 *ramp, u16 tint, int alpha);
int spr_w(int id);
int spr_h(int id);

/* Text: proportional 5x7 font, 10 px lines. Inline codes: ^0-^9 change colour (^0 = the colour
 * passed in), glyphs \x01 heart \x02 star \x03 note \x04 paw \x05 cursor \x06 more. */
#define LINE_H 10
int text(int x, int y, const char *s, u16 c);
int text_n(int x, int y, const char *s, int n, u16 c);
int text_w(const char *s);
void text_sh(int x, int y, const char *s, u16 c, u16 shadow);
void text_center(int cx, int y, const char *s, u16 c, u16 shadow);
void text_big(int x, int y, const char *s, int scale, u16 c, u16 shadow);
int text_big_w(const char *s, int scale);
/* Word wrap: fills starts[]/lens[] (byte offsets into s), returns the number of lines. */
int wrap_text(const char *s, int maxw, int *starts, int *lens, int maxlines);
/* Draw s word-wrapped to width w (at most maxlines rows); returns the rows drawn. */
int text_wrapped(int x, int y, int w, const char *s, u16 c, int maxlines);

/* ---- math, random ---- */
u32 rnd(void);
int rnd_range(int lo, int hi);    /* inclusive */
void rnd_mix(u32 v);              /* stir entropy in (button timing) */
int isin(int a);                  /* a: 256 per turn, result -256..256 */
int icos(int a);
int iabs(int v);
int imin(int a, int b);
int imax(int a, int b);
int iclamp(int v, int lo, int hi);
u32 isqrt(u32 v);
int iatan2(int y, int x);         /* 0..255 per turn */

/* ---- strings, memory ---- */
int str_len(const char *s);
int str_eq(const char *a, const char *b);
char *str_cpy(char *dst, const char *src);   /* returns the end */
char *str_int(char *dst, int v);
void mem_set(void *d, int v, u32 n);
void mem_copy(void *d, const void *s, u32 n);

/* ---- audio ---- */
enum {
    SFX_BLIP, SFX_OK, SFX_BACK, SFX_MOVE, SFX_MEOW, SFX_MEOW_SAD, SFX_PURR, SFX_HIT, SFX_WALL,
    SFX_GOAL, SFX_LOSE, SFX_COIN, SFX_SPLASH, SFX_BUZZ, SFX_DING, SFX_POP, SFX_WHOOSH, SFX_STEP,
    SFX_COUNT
};
enum { MUS_NONE = -1, MUS_MENU, MUS_HOCKEY, MUS_CAFE, MUS_MYSTERY, MUS_VILLAGE, MUS_KITTENS, MUS_COUNT };
void sfx(int id);
void sfx_voice(int pitch);        /* one dialogue blip; pitch in semitones around A4 */
void music(int track);
void audio_render(i16 *out, int frames);   /* interleaved stereo */
extern int audio_music_volume;             /* 0..256 */

/* ---- scenes ---- */
typedef struct {
    void (*enter)(void);
    void (*update)(void);
    void (*draw)(void);
    const char *name;                 /* for the playtest log */
} Scene;
void scene_set(const Scene *s);
void scene_fade_to(const Scene *s);
extern const Scene scene_menu, scene_rival, scene_detective, scene_cafe, scene_kittens, scene_village;

/* ---- entry points used by the platform layers ---- */
void game_init(void);
void game_frame(u16 p1, u16 p2);
/* Microseconds from the platform clock for time-slicing work, or 0 where there is none (then callers use
 * fixed step counts). */
u32 time_us(void);

/* ---- playtest diagnostics */
void platform_log(const char *line);      /* one line to the playtest log (the stick: logs/nekoarcade.log) */
char *log_buf(void);                      /* a 256-byte scratch buffer for building a log line */
extern int perf_overlay;                  /* SELECT toggles the on-screen performance overlay */
extern const Scene *current_scene(void);
