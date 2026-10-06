/* SPDX-License-Identifier: MIT */
#pragma once
#include "chess_pos.h"
#include "lvgl.h"

typedef struct {
    void (*on_move)(const char *from, const char *to, char promo);  /* promo: 0 or one of q r b n */
    void (*on_resign)(void);
    void (*on_menu_forget)(void);   /* "Forget this board": wipe token, go back to pairing */
    void (*on_menu_wifi)(void);     /* "Wi-Fi settings" */
    void (*on_menu_home)(void);     /* "Home": back to the home screen; the game keeps going */
    void (*on_clock_zero)(void);    /* a running clock hit zero: fetch the game now so the server settles it */
} eg_game_cb_t;

/* Build the game screen under `parent` (800x480). */
lv_obj_t *eg_game_create(lv_obj_t *parent, const eg_game_cb_t *cb);

/* Redraw from new game data. Call only when moveCount or status changed, or after a 409/422. */
void eg_game_set(lv_obj_t *screen, const eg_game_t *g);

#define EG_GAME_AVATAR_PX 40   /* opponent picture on the game screen */
void eg_game_refresh_avatar(lv_obj_t *screen);   /* after eg_avatar_load fetched the opponent's picture */
void eg_game_show_result(lv_obj_t *screen);   /* the game-over card (it also shows by itself when a game ends) */
void eg_game_set_you(lv_obj_t *screen, const char *name, const char *pfp);   /* your initial and picture on your card */

/* Fresh clock numbers from any response for the game on screen (call on every poll, not only on changes). */
void eg_game_clock(lv_obj_t *screen, const eg_game_t *g);
