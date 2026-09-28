/* Persistent save: 4 KB, exposed as libretro SRAM on the stick and kept in localStorage in the browser.
 * The frontend fills it after the game starts, so call save_check() before reading it. */
#pragma once
#include "engine.h"

#define SAVE_MAGIC 0x4F4B454Eu   /* "NEKO" */
#define SAVE_VERSION 1

typedef struct {
    u8 learned;                 /* 0..255: how well this kitten has learned the house rules */
    u8 trust;                   /* 0..255 */
    i8 brain[24];               /* small neural net weights (see demo_kittens.c) */
    u8 hunger, energy, fun;     /* needs 0..255 */
    u8 name_idx;
} KittenSave;

typedef struct {
    u32 magic;
    u16 version;
    u16 boots;
    u32 play_frames;
    /* Rival! (Mako) */
    u16 rival_wins, rival_losses;
    u16 rival_shots[3];         /* your lifetime shots at her top / middle / bottom: Mako remembers */
    u8 rival_best_margin, rival_streak;
    /* Whisker Detective (Shio) */
    u8 cases_solved;
    u8 case_done[16];
    u8 wrong_accusations;
    /* Nyan Cafe */
    u16 cafe_best, cafe_runs;
    u8 cafe_stars;
    /* Nia's foster kittens */
    KittenSave kittens[3];
    u8 kitten_days;
    /* Nekomura */
    u8 hearts[8];               /* friendship per villager, 0..100 */
    u8 gifts_given[8];
    u8 last_gift_item[8];
    u8 village_days;
    u8 fish_caught;
    /* which builds Nia has introduced (bit per demo) */
    u8 intro_seen;
    u8 reserved[4096 - 120];
} Save;

extern Save save;
void save_check(void);          /* initialise if the save is empty or from another version */
