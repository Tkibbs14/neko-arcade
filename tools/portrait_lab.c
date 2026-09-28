/* Contact sheet of the dating-sim portraits: each character in ten tells, rendered by the game's own code
 * (engine.c, cast.c, portrait.c) into build/portrait_lab.ppm. Built and converted by tools/portraitlab.sh. */
#include <stdio.h>
#include <string.h>
#include "../src/engine.h"
#include "../src/cast.h"
#include "../src/portrait.h"

u32 time_us(void) { return 0; }
void platform_log(const char *line) { (void)line; }

typedef struct { int expr, ears, tail, pose, gest, blush, aside, outfit, frames; const char *label; } State;

int main(void)
{
    static const State st[] = {
        { EX_NEUTRAL, EM_UP, TM_SWAY, PO_MID, GE_NONE, 0, 0, 0, 40, "neutral" },
        { EX_HAPPY, EM_PERK, TM_CURL, PO_CLOSE, GE_NONE, 1, 0, 0, 60, "pleased" },
        { EX_FLUSTERED, EM_BACK, TM_FLICK, PO_MID, GE_NONE, 1, 1, 0, 9, "flustered" },
        { EX_ANNOYED, EM_SWIVEL, TM_LOW, PO_FAR, GE_NONE, 0, 0, 0, 90, "annoyed" },
        { EX_SAD, EM_DROOP, TM_TUCK, PO_MID, GE_NONE, 0, 1, 0, 90, "guarded" },
        { EX_TIRED, EM_CURL, TM_SWAY, PO_MID, -2, 0, 0, 0, 30, "gesture" },
        { EX_NEUTRAL, EM_STILL, TM_STILL, PO_LEAN, GE_NONE, 1, 0, 0, 90, "still" },
        { EX_SMUG, EM_UP, TM_WRAP, PO_MID, GE_NONE, 0, 0, 0, 90, "wrap" },
        { EX_HAPPY, EM_UP, TM_SWAY, PO_MID, GE_NONE, 0, 0, 1, 40, "outfit 2" },
        { EX_SPARKLE, EM_PERK, TM_WAG, PO_MID, GE_NONE, 1, 0, 2, 40, "outfit 3" },
    };
    static const int gesture_of[3] = { GE_FRINGE, GE_CHAIN, GE_GLASSES };
    enum { N = sizeof st / sizeof st[0], CW = 150, CH = 170 };
    static u8 sheet[3 * CH][N * CW][3];
    for (int who = 0; who < 3; who++)
        for (int k = 0; k < N; k++) {
            Portrait p;
            portrait_init(&p, who, st[k].outfit);
            int gest = st[k].gest == -2 ? gesture_of[who] : st[k].gest;
            portrait_cue(&p, st[k].expr, st[k].ears, st[k].tail, st[k].pose, gest, st[k].blush, st[k].aside);
            for (int f = 0; f < st[k].frames; f++) {
                frame_count++;
                portrait_update(&p);
            }
            gradient_v(0, 0, SCREEN_W, SCREEN_H, RGB(58, 46, 86), RGB(30, 24, 48));
            portrait_draw(&p);
            text(4, 4, st[k].label, C_WHITE);
            for (int y = 0; y < CH; y++)
                for (int x = 0; x < CW; x++) {
                    int sx = x + 146, sy = y + 4;
                    u16 c = (sy < SCREEN_H && sx < SCREEN_W) ? fb[sy * SCREEN_W + sx] : 0;
                    if (x < 60 && y < 12)             /* the label, from the frame's top-left corner */
                        c = fb[(y + 2) * SCREEN_W + x + 2];
                    u8 *o = sheet[who * CH + y][k * CW + x];
                    o[0] = (u8)(((c >> 11) & 31) * 255 / 31);
                    o[1] = (u8)(((c >> 5) & 63) * 255 / 63);
                    o[2] = (u8)((c & 31) * 255 / 31);
                }
        }
    FILE *f = fopen("build/portrait_lab.ppm", "wb");
    fprintf(f, "P6 %d %d 255\n", N * CW, 3 * CH);
    fwrite(sheet, 1, sizeof sheet, f);
    fclose(f);
    printf("portrait_lab: %d states x 3 characters -> build/portrait_lab.ppm\n", N);
    return 0;
}
