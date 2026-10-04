/* SPDX-License-Identifier: MIT */
#pragma once
#include "chess_pos.h"
#include "lvgl.h"

typedef struct {
    void (*on_move)(const char *from, const char *to, char promo);  /* promo: 0 or one of q r b n */
    void (*on_resign)(void);
    void (*on_menu_forget)(void);   /* "Forget this board": wipe token, go back to pairing */
    void (*on_menu_wifi)(void);     /* "Wi-Fi settings" */
} eg_game_cb_t;

/* Build the game screen under `parent` (800x480). */
lv_obj_t *eg_game_create(lv_obj_t *parent, const eg_game_cb_t *cb);

/* Redraw from new game data. Call only when moveCount or status changed, or after a 409/422. */
void eg_game_set(lv_obj_t *screen, const eg_game_t *g);
