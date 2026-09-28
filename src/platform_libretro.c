/* libretro front for Neko Arcade: the M15 stick (picoarch) and the native test host both load this. */
#include "libretro.h"
#include "engine.h"
#include "save.h"
#include "nekolm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static retro_environment_t env_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static bool use_bitmasks;
static i16 audio_buf[AUDIO_PER_FRAME * 2];

/* ---------------------------------------------------------------- the playtest log
 * logs/nekoarcade.log on the card (or next to the launcher file), one flushed line per event, stamped with the
 * seconds since the game started; rotated to .prev past 256 KB. NEKO_LOG=- sends it to stderr (the PC tests). */
static FILE *plog;
static int plog_tried;
static char rom_dir[400];
static u32 t_boot;

static void plog_open(void)
{
    plog_tried = 1;
    const char *env = getenv("NEKO_LOG");
    if (env && !strcmp(env, "-")) {
        plog = stderr;
        return;
    }
    const char *dirs[2] = { "/mnt/sdcard/logs", rom_dir };
    for (int i = 0; i < 2 && !plog; i++) {
        if (!dirs[i][0])
            continue;
        char path[450], prev[460];
        snprintf(path, sizeof path, "%s/nekoarcade.log", dirs[i]);
        FILE *f = fopen(path, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fclose(f);
            if (size > 256 * 1024) {
                snprintf(prev, sizeof prev, "%s.prev", path);
                remove(prev);
                rename(path, prev);
            }
        }
        plog = fopen(path, "a");
    }
}

void platform_log(const char *line)
{
    if (!plog_tried)
        plog_open();
    if (!plog)
        return;
    u32 t = time_us() - t_boot;
    fprintf(plog, "%s[%5u.%03u] %s\n", plog == stderr ? "[nekoarcade] " : "", t / 1000000, t / 1000 % 1000, line);
    fflush(plog);
}

void retro_set_environment(retro_environment_t cb)
{
    env_cb = cb;
    bool no_game = true;
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
}
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }

void retro_get_system_info(struct retro_system_info *info)
{
    info->library_name = "Neko Arcade";
    info->library_version = "0.1";
    info->valid_extensions = "neko";
    info->need_fullpath = true;
    info->block_extract = true;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    info->geometry.base_width = SCREEN_W;
    info->geometry.base_height = SCREEN_H;
    info->geometry.max_width = SCREEN_W;
    info->geometry.max_height = SCREEN_H;
    info->geometry.aspect_ratio = 0;    /* width/height = 16:9 */
    info->timing.fps = 60;
    info->timing.sample_rate = AUDIO_RATE;
}

void retro_init(void)
{
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
    if (env_cb) {
        env_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
        use_bitmasks = env_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL);
    }
}

void retro_deinit(void) {}

/* The CPU's own description, once per session: which instruction-set extensions the model could use. */
static void log_cpuinfo(void)
{
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f)
        return;
    char line[200], out[230];
    static const char *const keys[] = { "cpu model", "BogoMIPS", "isa", "ASEs implemented", "model name", "flags" };
    int logged = 0;
    while (fgets(line, sizeof line, f) && logged < 6) {
        if (line[0] == 10)
            break;                                   /* the first processor's block is enough */
        for (int k = 0; k < 6; k++)
            if (!strncmp(line, keys[k], strlen(keys[k]))) {
                for (char *c = line; *c; c++)
                    if (*c == 10 || *c == 13) { *c = 0; break; }      /* drop the line end */
                snprintf(out, sizeof out, "cpu %.200s", line);
                platform_log(out);
                logged++;
                break;
            }
    }
    fclose(f);
}

bool retro_load_game(const struct retro_game_info *game)
{
    t_boot = time_us();
    if (game && game->path) {                        /* the launcher's folder, the log's fallback home */
        snprintf(rom_dir, sizeof rom_dir, "%s", game->path);
        char *slash = strrchr(rom_dir, '/');
        if (slash) *slash = 0; else rom_dir[0] = 0;
    }
    static const struct retro_input_descriptor desc[] = {
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "Confirm / action" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "Back / dash" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Special" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Special 2" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Pause" },
        { 0 },
    };
    if (env_cb)
        env_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, (void *)desc);
    log_cpuinfo();
    game_init();
    return true;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
    (void)type; (void)info; (void)num;
    return false;
}

void retro_unload_game(void) {}
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
void retro_reset(void) { game_init(); }

static u16 read_pad(unsigned port)
{
    if (!input_state_cb)
        return 0;
    if (use_bitmasks)
        return (u16)input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
    u16 m = 0;
    for (unsigned id = 0; id < 12; id++)
        if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, id))
            m |= (u16)(1u << id);
    return m;
}

void retro_run(void)
{
    if (input_poll_cb)
        input_poll_cb();
    game_frame(read_pad(0), read_pad(1));
    if (video_cb)
        video_cb(fb, SCREEN_W, SCREEN_H, SCREEN_W * sizeof(u16));
    audio_render(audio_buf, AUDIO_PER_FRAME);
    if (audio_batch_cb)
        audio_batch_cb(audio_buf, AUDIO_PER_FRAME);
    /* stutter as the player sees it: time between the frontend's calls (16.7 ms when smooth) */
    static u32 last_run, gaps, gap_max, runs;
    u32 now = time_us();
    if (last_run) {
        u32 d = now - last_run;
        if (d > 20000) gaps++;
        if (d > gap_max) gap_max = d;
    }
    last_run = now;
    if (++runs >= 600) {
        char b[96];
        snprintf(b, sizeof b, "frames frames_over_20ms=%u longest_frame_us=%u", gaps, gap_max);
        platform_log(b);
        gaps = gap_max = runs = 0;
    }
}

size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { (void)data; (void)size; return false; }
bool retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) { (void)index; (void)enabled; (void)code; }

void *retro_get_memory_data(unsigned id) { return id == RETRO_MEMORY_SAVE_RAM ? (void *)&save : NULL; }
size_t retro_get_memory_size(unsigned id) { return id == RETRO_MEMORY_SAVE_RAM ? sizeof(save) : 0; }

u32 time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u32)tv.tv_sec * 1000000u + (u32)tv.tv_usec;
}
