/* SPDX-License-Identifier: MIT */
#pragma once
#include "chess_pos.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EG_API_BASE "https://endgam.es/.netlify/functions"

typedef enum { EG_OK, EG_NO_GAME, EG_UNAUTHORIZED, EG_CONFLICT, EG_ILLEGAL, EG_EXPIRED, EG_PENDING, EG_ERROR } eg_result_t;

typedef struct {
    char code[8];
    char poll_secret[96];
    char claim_url[64];
} eg_pairing_t;

eg_result_t eg_api_pair_start(eg_pairing_t *out);
/* EG_PENDING until claimed, EG_OK with token filled, EG_EXPIRED after 10 minutes. */
eg_result_t eg_api_pair_poll(const char *poll_secret, char token_out[96]);

/* GET /board-game. EG_NO_GAME when game is null. EG_UNAUTHORIZED means wipe the token. */
eg_result_t eg_api_game(const char *token, eg_game_t *out);
/* POST /board-move. On EG_CONFLICT / EG_ILLEGAL, *out holds the server's current game: redraw from it. */
eg_result_t eg_api_move(const char *token, const eg_game_t *g, const char *from, const char *to, char promo, eg_game_t *out);

/* GET /board-pieces. Fills set_id and the 12 URLs (index: color*6 + type, color 0=w 1=b, type order k q r b n p). */
eg_result_t eg_api_pieces(const char *token, char set_id[48], char urls[12][160]);

/* Download a URL into PSRAM. Caller frees with heap_caps_free. Returns NULL on failure. */
void *eg_http_download(const char *url, size_t *len);
