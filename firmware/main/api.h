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

/* ---------- home, new games, sets ---------- */

#define EG_HOME_MAX_GAMES 20
#define EG_HOME_MAX_SETS 16

typedef struct {
    char id[40];
    char opponent[33];
    bool opponent_ai;
    char difficulty[16];        /* beginner | intermediate | advanced | expert, computer games only */
    char your_color;            /* 'w' or 'b' */
    bool your_turn;
    int move_count;
} eg_home_game_t;

typedef struct { char id[48]; char name[33]; char preview_k[160], preview_n[160]; } eg_set_t;   /* preview: white king and knight PNGs */

#define EG_HOME_MAX_INCOMING 10
typedef struct { char id[40]; char name[33]; int elo; } eg_incoming_t;   /* a direct challenge waiting for your answer */

typedef struct {
    char name[33];
    int elo;                    /* -1 before the first ranked game */
    int n_incoming;
    eg_incoming_t incoming[EG_HOME_MAX_INCOMING];
    int n_games;
    eg_home_game_t games[EG_HOME_MAX_GAMES];
    char active_set[48];
    int n_sets;
    eg_set_t sets[EG_HOME_MAX_SETS];
} eg_home_t;

/* GET /board-home. EG_ERROR also covers a server that doesn't have it yet (404). */
eg_result_t eg_api_home(const char *token, eg_home_t *out);
/* GET /board-game?gameId= */
eg_result_t eg_api_game_id(const char *token, const char *game_id, eg_game_t *out);
/* POST /board-new-game {mode:"ai"}. difficulty: beginner|intermediate|advanced|expert; color: white|black|random */
eg_result_t eg_api_new_ai(const char *token, const char *difficulty, const char *color, eg_game_t *out);
/* POST /board-new-game {mode:"challenge"}. Fills the link to show as a QR. */
eg_result_t eg_api_new_challenge(const char *token, const char *color, char url[160]);
/* POST /board-resign */
eg_result_t eg_api_resign(const char *token, const char *game_id, eg_game_t *out);
/* POST /board-set */
eg_result_t eg_api_set(const char *token, const char *set_id);

/* ---------- people: quick match and direct challenges ---------- */

typedef enum { EG_QM_IDLE, EG_QM_WAITING, EG_QM_MATCHED } eg_qm_status_t;
/* action "join" or "cancel" POSTs /board-quick-match; NULL GETs the current status (what you poll).
 * EG_OK fills *status, and *game when matched. EG_NO_GAME: matched, but the game didn't come back
 * (it is still in progress and shows up in board-home). */
eg_result_t eg_api_quick_match(const char *token, const char *action, eg_qm_status_t *status, eg_game_t *game);

#define EG_USERS_MAX 10
typedef struct { char id[40]; char name[33]; int elo; } eg_user_t;
/* GET /board-users?q=  q needs at least 2 characters (shorter returns no users). */
eg_result_t eg_api_users(const char *token, const char *q, eg_user_t users[EG_USERS_MAX], int *n);

/* POST /board-new-game {mode:"challenge", opponentId, first}. first: me | computer (they move first) | random.
 * EG_CONFLICT: you already have a pending challenge with this player. */
eg_result_t eg_api_challenge_player(const char *token, const char *opponent_id, const char *first);

/* POST /board-challenge-respond {id, accept}. Accept: EG_OK and *out is the new game (EG_NO_GAME if it didn't come back).
 * Decline: EG_OK. EG_CONFLICT: the challenge expired, is gone, or was already answered. */
eg_result_t eg_api_challenge_respond(const char *token, const char *id, bool accept, eg_game_t *out);

/* GET /board-leaderboard */
#define EG_RANK_MAX 50
typedef struct {
    int n;
    struct { int rank; char name[33]; int elo; bool you; } rows[EG_RANK_MAX];
    int you_rank, you_elo;     /* -1 when unranked */
} eg_rank_t;
eg_result_t eg_api_leaderboard(const char *token, eg_rank_t *out);

/* Download a URL into PSRAM. Caller frees with heap_caps_free. Returns NULL on failure. */
void *eg_http_download(const char *url, size_t *len);
