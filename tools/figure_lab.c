/* Contact sheet of the full-body figures (Thin Walls title and pick screens): each character in her three outfits
 * and a few tells, rendered by the game's own code into build/figure_lab.ppm. Built by tools/figurelab.sh. */
#include <stdio.h>
#include "../src/engine.h"
#include "../src/cast.h"
#include "../src/portrait.h"

u32 time_us(void) { return 0; }
void platform_log(const char *line) { (void)line; }

typedef struct { int outfit, expr, ears, tail, dim, frames; const char *label; } State;

int main(void)
{
    static const State st[] = {
        { 0, EX_NEUTRAL, EM_UP, TM_SWAY, 0, 40, "outfit 1" },
        { 1, EX_HAPPY, EM_PERK, TM_CURL, 0, 60, "outfit 2" },
        { 2, EX_SMUG, EM_UP, TM_WAG, 0, 50, "outfit 3" },
        { 0, EX_FLUSTERED, EM_BACK, TM_WRAP, 0, 90, "wrap" },
        { 0, EX_SAD, EM_DROOP, TM_LOW, 0, 90, "low" },
        { 0, EX_NEUTRAL, EM_UP, TM_SWAY, 120, 40, "dimmed" },
    };
    enum { N = sizeof st / sizeof st[0], CW = 76, CH = 140 };
    static u8 sheet[3 * CH][N * CW][3];
    for (int who = 0; who < 3; who++)
        for (int k = 0; k < N; k++) {
            Portrait p;
            portrait_init(&p, who, st[k].outfit);
            portrait_cue(&p, st[k].expr, st[k].ears, st[k].tail, -1, GE_NONE, -1, 0);
            for (int f = 0; f < st[k].frames; f++) {
                frame_count++;
                portrait_update(&p);
            }
            gradient_v(0, 0, SCREEN_W, SCREEN_H, RGB(128, 110, 100), RGB(90, 78, 76));
            rect(0, 128, SCREEN_W, 52, RGB(104, 46, 58));
            figure_draw(&p, 8, 6, 256, st[k].dim);
            text(2, 132, st[k].label, C_WHITE);
            for (int y = 0; y < CH; y++)
                for (int x = 0; x < CW; x++) {
                    u16 c = fb[y * SCREEN_W + x];
                    u8 *o = sheet[who * CH + y][k * CW + x];
                    o[0] = (u8)(((c >> 11) & 31) * 255 / 31);
                    o[1] = (u8)(((c >> 5) & 63) * 255 / 63);
                    o[2] = (u8)((c & 31) * 255 / 31);
                }
        }
    FILE *f = fopen("build/figure_lab.ppm", "wb");
    fprintf(f, "P6 %d %d 255", N * CW, 3 * CH);
    fputc(10, f);
    fwrite(sheet, 1, sizeof sheet, f);
    fclose(f);
    printf("figure_lab: %d states x 3 characters -> build/figure_lab.ppm", N);
    puts("");
    return 0;
}
