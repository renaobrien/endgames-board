/* SPDX-License-Identifier: MIT */
#include "chess_pos.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

int eg_sq_index(const char *sq)
{
    if (!sq || sq[0] < 'a' || sq[0] > 'h' || sq[1] < '1' || sq[1] > '8') return -1;
    return (8 - (sq[1] - '0')) * 8 + (sq[0] - 'a');
}

bool eg_parse_fen(eg_game_t *g, const char *fen)
{
    memset(g->board, 0, sizeof g->board);
    int row = 0, file = 0;
    const char *p = fen;
    for (; *p && *p != ' '; p++) {
        if (*p == '/') { row++; file = 0; }
        else if (isdigit((unsigned char)*p)) file += *p - '0';
        else if (row < 8 && file < 8) g->board[row * 8 + file++] = *p;
        else return false;
    }
    if (row != 7) return false;
    if (*p == ' ') p++;
    g->white_to_move = (*p != 'b');
    return true;
}

bool eg_is_yours(const eg_game_t *g, int idx)
{
    char c = g->board[idx];
    if (!c) return false;
    return g->you_white ? isupper((unsigned char)c) != 0 : islower((unsigned char)c) != 0;
}

bool eg_move_legal(const eg_game_t *g, const char *from, const char *to, bool *promo)
{
    bool found = false;
    if (promo) *promo = false;
    for (int i = 0; i < g->legal_n; i++) {
        if (strncmp(g->legal[i], from, 2) == 0 && strncmp(g->legal[i] + 2, to, 2) == 0) {
            found = true;
            if (promo && g->legal[i][4]) *promo = true;
        }
    }
    return found;
}

static bool attacked_by(const eg_game_t *g, int target, bool by_white)
{
    int tr = target / 8, tf = target % 8;
    static const int kn[8][2] = {{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
    for (int i = 0; i < 8; i++) {
        int r = tr + kn[i][0], f = tf + kn[i][1];
        if (r < 0 || r > 7 || f < 0 || f > 7) continue;
        char c = g->board[r * 8 + f];
        if (c == (by_white ? 'N' : 'n')) return true;
    }
    int pr = tr + (by_white ? 1 : -1);   /* a white pawn attacks from the row below (higher index) */
    for (int df = -1; df <= 1; df += 2) {
        int f = tf + df;
        if (pr >= 0 && pr < 8 && f >= 0 && f < 8 && g->board[pr * 8 + f] == (by_white ? 'P' : 'p')) return true;
    }
    for (int dr = -1; dr <= 1; dr++)
        for (int df = -1; df <= 1; df++) {
            if (!dr && !df) continue;
            int r = tr + dr, f = tf + df;
            if (r < 0 || r > 7 || f < 0 || f > 7) continue;
            if (g->board[r * 8 + f] == (by_white ? 'K' : 'k')) return true;
            bool diag = dr && df;
            for (r = tr + dr, f = tf + df; r >= 0 && r < 8 && f >= 0 && f < 8; r += dr, f += df) {
                char c = g->board[r * 8 + f];
                if (!c) continue;
                char u = (char)tolower((unsigned char)c);
                bool mine = by_white ? isupper((unsigned char)c) : islower((unsigned char)c);
                if (mine && (u == 'q' || (diag ? u == 'b' : u == 'r'))) return true;
                break;
            }
        }
    return false;
}

bool eg_in_check(const eg_game_t *g, int *king_idx)
{
    char k = g->white_to_move ? 'K' : 'k';
    for (int i = 0; i < 64; i++) {
        if (g->board[i] == k) {
            if (king_idx) *king_idx = i;
            return attacked_by(g, i, !g->white_to_move);
        }
    }
    return false;
}
