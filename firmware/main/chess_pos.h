/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define EG_MAX_LEGAL 256

/* One position plus what the API told us about the game. Plain data, no LVGL. */
typedef struct {
    char id[40];
    char board[64];          /* index = row*8 + file, row 0 = rank 8. 'K','q',... or 0 for empty */
    bool you_white;
    bool white_to_move;
    bool your_turn;
    bool in_progress;
    char status[16];         /* IN_PROGRESS, WHITE_WON, BLACK_WON, DRAW, ABANDONED */
    int  move_count;
    char last_from[3], last_to[3];
    char opponent[32];
    bool opp_ai;             /* playing the computer */
    char opp_difficulty[16]; /* beginner | intermediate | advanced | expert */
    char legal[EG_MAX_LEGAL][6];  /* UCI: "e2e4", "e7e8q" */
    int  legal_n;
    char moves[160][8];      /* last 160 SAN moves, oldest first */
    int  moves_n;
    int  moves_first_ply;    /* ply number (0 = white's first move) of moves[0] */
    bool timed;                      /* timeControl set: clock fields below are live */
    char time_control[8];            /* "5+0" etc., empty when untimed */
    int32_t you_ms, opp_ms, inc_ms;  /* time left at the moment of the response */
    char running;                    /* 'y' your clock, 'o' theirs, 0 neither */
    char end_reason[12];             /* checkmate | draw | resign | timeout | abandoned, empty in progress */
} eg_game_t;

/* Fill g->board and the turn flag from a FEN string. Returns false on a malformed FEN. */
bool eg_parse_fen(eg_game_t *g, const char *fen);

/* "e4" -> 0..63 (row*8+file). -1 if invalid. */
int eg_sq_index(const char *sq);

/* True if the piece at idx belongs to the side the owner plays. */
bool eg_is_yours(const eg_game_t *g, int idx);

/* True if some legal move goes from -> to. Sets *promo when it is a promotion. */
bool eg_move_legal(const eg_game_t *g, const char *from, const char *to, bool *promo);

/* True if the side to move is in check. Cheap heuristic: finds the king and
 * tests attackers. Used only for the red highlight. */
bool eg_in_check(const eg_game_t *g, int *king_idx);
