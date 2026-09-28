/* WebAssembly front for Neko Arcade: the browser page calls these each frame (see web/host.js). */
#include "engine.h"
#include "save.h"

#define EXPORT(name) __attribute__((export_name(#name)))

static i16 web_audio[4096 * 2];

EXPORT(na_init) void na_init(void) { game_init(); }
EXPORT(na_frame) void na_frame(u32 p1, u32 p2) { game_frame((u16)p1, (u16)p2); }
EXPORT(na_fb) u16 *na_fb(void) { return fb; }
EXPORT(na_audio) i16 *na_audio(int frames)
{
    if (frames > 4096) frames = 4096;
    audio_render(web_audio, frames);
    return web_audio;
}
EXPORT(na_save_ptr) void *na_save_ptr(void) { return &save; }
EXPORT(na_save_size) u32 na_save_size(void) { return sizeof(save); }

/* The page supplies a clock (performance.now) and a console for the playtest log. */
__attribute__((import_module("env"), import_name("na_time_us"))) u32 na_time_us(void);
__attribute__((import_module("env"), import_name("na_log"))) void na_log(const char *line);
u32 time_us(void) { return na_time_us(); }
void platform_log(const char *line) { na_log(line); }

/* The compiler may emit calls to these for struct copies and zeroing; there is no C library here. */
void *memset(void *d, int c, unsigned long n)
{
    unsigned char *p = d;
    while (n--) *p++ = (unsigned char)c;
    return d;
}

void *memcpy(void *d, const void *s, unsigned long n)
{
    unsigned char *p = d;
    const unsigned char *q = s;
    while (n--) *p++ = *q++;
    return d;
}

void *memmove(void *d, const void *s, unsigned long n)
{
    unsigned char *p = d;
    const unsigned char *q = s;
    if (p < q) { while (n--) *p++ = *q++; }
    else { p += n; q += n; while (n--) *--p = *--q; }
    return d;
}
