/* SPDX-License-Identifier: MIT */
/* Game screen: board on the left (56 px squares), 292 px panel on the right:
 * Back and Menu (Resign) on top, the opponent's card, status and last move, the move list, your card.
 * Looks like the website's v2 game screen: sunset palette, pink/yellow last-move tint,
 * magenta check glow, mint legal-move dots, cyan edge. */
#include "ui_game.h"
#include "fonts.h"
#include "pieces.h"
#include "theme.h"
#include "ui_screens.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SQ 56
#define BOARD (8 * SQ)        /* 448: leaves room for the cyan frame and pink offset shadow, like the website */
#define BOARD_X 16
#define BOARD_Y 16
#define PANEL_X 496          /* right panel */
#define RAIL_W 292
#define PAD 16

typedef struct {
    eg_game_t g;
    eg_game_cb_t cb;
    bool flipped;            /* true: black at the bottom */
    int selected;            /* game square index, or -1 */
    char pending_from[3], pending_to[3];
    /* Everything per-square is indexed by screen cell (row*8+col). */
    lv_obj_t *cells[64], *tint[64], *piece[64], *mark[64];
    lv_obj_t *avatar, *avatar_txt, *opp_name, *opp_sub, *opp_caps, *you_caps, *status, *last, *you_card;
    lv_obj_t *btn_back, *btn_menu, *you_name, *you_sub;
    lv_obj_t *promo, *menu, *drawer_list;
    bool sent;               /* our move is on its way; board already shows it */
    /* clocks (timed games) */
    lv_obj_t *clk_you, *clk_opp;
    uint32_t clk_at;         /* lv_tick when you_ms / opp_ms were received */
    bool zero_sent;          /* asked the server to settle a clock that reached zero */
} ui_t;

static ui_t *U(lv_obj_t *screen) { return (ui_t *)lv_obj_get_user_data(screen); }

/* Game square <-> screen cell. Flipping rotates the board 180 degrees, so the map is its own inverse. */
static int cell_of(const ui_t *u, int sq) { return u->flipped ? 63 - sq : sq; }

static void sq_name(int idx, char out[3])
{
    out[0] = (char)('a' + idx % 8);
    out[1] = (char)('0' + 8 - idx / 8);
    out[2] = 0;
}

static lv_obj_t *mk_label(lv_obj_t *p, const lv_font_t *f, lv_color_t c, const char *txt)
{
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_label_set_text(l, txt);
    return l;
}

static void plain(lv_obj_t *o)
{
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}


/* ---------- board ---------- */

static void clear_marks(ui_t *u)
{
    for (int i = 0; i < 64; i++) {
        lv_obj_add_flag(u->mark[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_border_width(u->mark[i], 0, 0);
        lv_obj_set_style_bg_opa(u->mark[i], LV_OPA_TRANSP, 0);
    }
}

static void show_targets(ui_t *u)
{
    clear_marks(u);
    if (u->selected < 0) return;
    char from[3];
    sq_name(u->selected, from);

    lv_obj_t *m = u->mark[cell_of(u, u->selected)];
    lv_obj_clear_flag(m, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_border_width(m, 3, 0);
    lv_obj_set_style_border_color(m, EG_BOARD_EDGE, 0);

    for (int i = 0; i < u->g.legal_n; i++) {
        if (strncmp(u->g.legal[i], from, 2) != 0) continue;
        int t = eg_sq_index(u->g.legal[i] + 2);
        if (t < 0) continue;
        lv_obj_t *d = u->mark[cell_of(u, t)];
        lv_obj_clear_flag(d, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        if (u->g.board[t]) {                      /* capture: ring */
            lv_obj_set_style_border_width(d, 3, 0);
            lv_obj_set_style_border_color(d, EG_MINT, 0);
        } else {                                  /* quiet move: dot */
            lv_obj_set_style_bg_color(d, EG_MINT, 0);
            lv_obj_set_style_bg_opa(d, LV_OPA_80, 0);
        }
    }
}

static void redraw_board(ui_t *u)
{
    int king = -1;
    bool check = u->g.in_progress && eg_in_check(&u->g, &king);
    int lf = eg_sq_index(u->g.last_from), lt = eg_sq_index(u->g.last_to);

    for (int cell = 0; cell < 64; cell++) {
        int idx = cell_of(u, cell);          /* same map both ways */
        bool dark = ((cell / 8) + (cell % 8)) % 2 == 1;
        lv_obj_set_style_bg_color(u->cells[cell], dark ? EG_BOARD_DARK : EG_BOARD_LIGHT, 0);

        lv_obj_t *t = u->tint[cell];
        if (check && idx == king) {
            lv_obj_set_style_bg_color(t, EG_MAGENTA, 0);
            lv_obj_set_style_bg_opa(t, LV_OPA_60, 0);
        } else if (idx == lf || idx == lt) {          /* last move: both squares clearly lit */
            lv_obj_set_style_bg_color(t, EG_YELLOW, 0);
            lv_obj_set_style_bg_opa(t, dark ? LV_OPA_50 : LV_OPA_60, 0);
        } else {
            lv_obj_set_style_bg_opa(t, LV_OPA_TRANSP, 0);
        }
        /* destination gets a yellow ring (website: --lastmove-ring) */
        lv_obj_set_style_border_width(t, (idx == lt && !(check && idx == king)) ? 4 : 0, 0);
        lv_obj_set_style_border_color(t, EG_LASTMOVE_RING, 0);

        char c = u->g.board[idx];
        /* your pieces always use the light art, theirs the dark art, whatever color you actually play */
        bool mine = c && (isupper((unsigned char)c) != 0) == u->g.you_white;
        const void *src = c ? eg_piece_src(mine ? 'w' : 'b', (char)tolower((unsigned char)c)) : NULL;
        if (src) {
            lv_image_set_src(u->piece[cell], src);
            lv_obj_clear_flag(u->piece[cell], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(u->piece[cell], LV_OBJ_FLAG_HIDDEN);
        }
    }
    show_targets(u);
}

/* Slide the piece that just moved from its old square, so the opponent's move is easy to follow. */
static void anim_tx(void *o, int32_t v) { lv_obj_set_style_translate_x(o, v, 0); }
static void anim_ty(void *o, int32_t v) { lv_obj_set_style_translate_y(o, v, 0); }

static void animate_last_move(ui_t *u)
{
    int f = eg_sq_index(u->g.last_from), t = eg_sq_index(u->g.last_to);
    if (f < 0 || t < 0) return;
    int cf = cell_of(u, f), ct = cell_of(u, t);
    lv_obj_t *img = u->piece[ct];
    int dx = (cf % 8 - ct % 8) * SQ, dy = (cf / 8 - ct / 8) * SQ;
    lv_obj_move_foreground(u->cells[ct]);            /* draw above neighbouring squares while sliding */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, img);
    lv_anim_set_duration(&a, 380);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, anim_tx);
    lv_anim_set_values(&a, dx, 0);
    lv_anim_start(&a);
    lv_anim_set_exec_cb(&a, anim_ty);
    lv_anim_set_values(&a, dy, 0);
    lv_anim_start(&a);
}

/* ---------- panel ---------- */

static const char CAP_ORDER[5] = {'q', 'r', 'b', 'n', 'p'};
static const int CAP_START[5] = {1, 2, 2, 2, 8};
static const int CAP_VALUE[5] = {9, 5, 3, 3, 1};

/* Pieces of `color` ('w'/'b') missing from the board, i.e. captured by the other side. Returns material value. */
static int missing(const eg_game_t *g, char color, int out[5])
{
    int have[5] = {0};
    for (int i = 0; i < 64; i++) {
        char c = g->board[i];
        if (!c || (isupper((unsigned char)c) ? 'w' : 'b') != color) continue;
        const char *p = strchr("qrbnp", tolower((unsigned char)c));
        if (p) have[p - "qrbnp"]++;
    }
    int value = 0;
    for (int k = 0; k < 5; k++) {
        out[k] = CAP_START[k] - have[k];
        if (out[k] < 0) out[k] = 0;            /* promotions */
        value += out[k] * CAP_VALUE[k];
    }
    return value;
}

/* Row of small icons for the pieces this side has taken, plus +N if ahead on material. */
static void draw_caps(lv_obj_t *row, const int taken[5], char taken_color, int lead)
{
    lv_obj_clean(row);
    int x = 0, y = 0, w = RAIL_W;               /* fixed: layout may not have run yet */
    for (int k = 0; k < 5; k++) {
        for (int n = 0; n < taken[k]; n++) {
            const void *src = eg_piece_src(taken_color, CAP_ORDER[k]);
            if (!src) continue;
            if (x > w - 22) { x = 0; y += 26; }      /* wrap onto a second line in the narrow rail */
            lv_obj_t *im = lv_image_create(row);
            lv_image_set_src(im, src);
            lv_image_set_pivot(im, 0, 0);
            lv_image_set_scale(im, 256 * 24 / EG_PIECE_PX);
            lv_obj_set_pos(im, x, y);
            x += 13;
        }
        if (taken[k]) x += 5;
    }
    if (lead > 0) {
        lv_obj_t *l = mk_label(row, &eg_sora_16, EG_FG_HAZE, "");
        lv_label_set_text_fmt(l, "+%d", lead);
        if (x > w - 40) { x = 0; y += 26; }
        lv_obj_set_pos(l, x + 14, y + 4);
    }
    lv_obj_set_height(row, y + 30);
}

static void rebuild_moves(ui_t *u)
{
    if (!u->drawer_list) return;
    lv_obj_clean(u->drawer_list);
    int n = u->g.moves_n;
    if (n == 0) {
        lv_obj_set_pos(mk_label(u->drawer_list, &eg_sora_16, EG_FG_MIST, "No moves yet."), 4, 2);
        return;
    }
    int start = n > 120 ? n - 120 : 0;
    if (start % 2) start--;
    if (start < 0) start = 0;
    for (int i = start; i < n; i += 2) {
        lv_obj_t *row = lv_obj_create(u->drawer_list);
        plain(row);
        lv_obj_set_size(row, LV_PCT(100), 24);
        char num[16];
        snprintf(num, sizeof num, "%d.", (u->g.moves_first_ply + i) / 2 + 1);
        lv_obj_align(mk_label(row, &eg_sora_16, EG_FG_MIST, num), LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_align(mk_label(row, &eg_sora_16, i == n - 1 ? EG_YELLOW : EG_FG, u->g.moves[i]), LV_ALIGN_LEFT_MID, 52, 0);
        if (i + 1 < n)
            lv_obj_align(mk_label(row, &eg_sora_16, i + 1 == n - 1 ? EG_YELLOW : EG_FG, u->g.moves[i + 1]), LV_ALIGN_LEFT_MID, 150, 0);
    }
    lv_obj_scroll_to_y(u->drawer_list, LV_COORD_MAX, LV_ANIM_OFF);
}

/* ---------- clocks ---------- */

#define LOW_MS 30000         /* your clock turns magenta and pulses under 30 s */

static lv_obj_t *mk_clock(lv_obj_t *parent, int x, int y)
{
    lv_obj_t *c = lv_obj_create(parent);
    plain(c);
    lv_obj_set_size(c, 80, 40);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_style_radius(c, 10, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(c, EG_SURFACE, 0);
    lv_obj_t *l = mk_label(c, &eg_vt323_36, EG_FG_MIST, "");
    lv_obj_center(l);
    lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
    return c;
}

static void clock_text(char out[12], int32_t ms)
{
    if (ms < 0) ms = 0;
    if (ms < 10000) { snprintf(out, 12, "%d.%d", (int)(ms / 1000), (int)(ms % 1000 / 100)); return; }
    int32_t s = (ms + 999) / 1000;      /* never show 0:00 while time is left */
    snprintf(out, 12, "%d:%02d", (int)(s / 60), (int)(s % 60));
}

static void paint_clock(lv_obj_t *c, int32_t ms, bool running, bool mine, uint32_t now)
{
    char t[12];
    clock_text(t, ms);
    lv_obj_t *l = lv_obj_get_child(c, 0);
    lv_label_set_text(l, t);
    bool low = ms <= LOW_MS;
    lv_color_t fg = !running ? EG_FG_MIST : low ? EG_MAGENTA : mine ? EG_CYAN : EG_PINK;
    lv_color_t bg = EG_SURFACE;
    if (running && mine && low && (now / 500) % 2) { bg = EG_MAGENTA; fg = EG_FG; }   /* pulse: no speaker, so it has to be seen */
    lv_obj_set_style_text_color(l, fg, 0);
    lv_obj_set_style_bg_color(c, bg, 0);
    lv_obj_set_style_border_width(c, running ? 2 : 0, 0);
    lv_obj_set_style_border_color(c, fg, 0);
}

static void clock_tick(lv_timer_t *t)
{
    ui_t *u = lv_timer_get_user_data(t);
    if (!u->g.timed) return;
    uint32_t now = lv_tick_get();
    int32_t used = (int32_t)(now - u->clk_at);
    bool live = u->g.in_progress;
    int32_t you = u->g.you_ms - (live && u->g.running == 'y' ? used : 0);
    int32_t opp = u->g.opp_ms - (live && u->g.running == 'o' ? used : 0);
    paint_clock(u->clk_you, you, live && u->g.running == 'y', true, now);
    paint_clock(u->clk_opp, opp, live && u->g.running == 'o', false, now);
    if (live && u->g.running && (you <= 0 || opp <= 0) && !u->zero_sent) {
        u->zero_sent = true;                 /* the server settles it; ask now instead of at the next poll */
        if (u->cb.on_clock_zero) u->cb.on_clock_zero();
    }
}

static void clocks_from(ui_t *u, const eg_game_t *g)
{
    u->g.timed = g->timed;
    u->g.you_ms = g->you_ms;
    u->g.opp_ms = g->opp_ms;
    u->g.running = g->running;
    u->clk_at = lv_tick_get();
    u->zero_sent = false;
    if (g->timed) { lv_obj_remove_flag(u->clk_you, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(u->clk_opp, LV_OBJ_FLAG_HIDDEN); }
    else { lv_obj_add_flag(u->clk_you, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(u->clk_opp, LV_OBJ_FLAG_HIDDEN); }
}

void eg_game_clock(lv_obj_t *screen, const eg_game_t *g)
{
    ui_t *u = U(screen);
    if (strcmp(u->g.id, g->id) != 0) return;
    clocks_from(u, g);
}

static void set_status(ui_t *u)
{
    const char *txt;
    lv_color_t col = EG_FG_HAZE;
    if (!u->g.in_progress) {
        bool on_time = strcmp(u->g.end_reason, "timeout") == 0;
        if (strcmp(u->g.status, "DRAW") == 0) txt = on_time ? "Draw: time ran out" : "Draw";
        else if (strcmp(u->g.status, "ABANDONED") == 0) txt = "Game abandoned";
        else {
            bool you_won = (strcmp(u->g.status, "WHITE_WON") == 0) == u->g.you_white;
            txt = you_won ? (on_time ? "You won on time" : "You won") : (on_time ? "You lost on time" : "You lost");
            col = you_won ? EG_MINT : EG_MAGENTA;
        }
    } else if (u->g.your_turn && !u->sent) {
        txt = "Your move";
        col = EG_CYAN;
    } else if (u->g.opp_ai) {
        txt = "Thinking...";
    } else {
        txt = u->sent ? "Sending..." : "Their move";
    }
    lv_label_set_text(u->status, txt);
    lv_obj_set_style_text_color(u->status, col, 0);
}

static void redraw_panel(ui_t *u)
{
    /* Opponent: a round avatar with their initial (their picture once the API sends it). Tap it for the name. */
    const char *nm = u->g.opponent[0] ? u->g.opponent : "Opponent";
    char initial[2] = {(char)((nm[0] >= 'a' && nm[0] <= 'z') ? nm[0] - 32 : nm[0]), 0};
    lv_label_set_text(u->avatar_txt, u->g.opp_ai ? "AI" : initial);
    lv_obj_set_style_border_color(u->avatar, u->g.opp_ai ? EG_CYAN : EG_PINK, 0);
    lv_label_set_text(u->opp_name, nm);
    if (u->g.opp_ai) lv_label_set_text_fmt(u->opp_sub, "Computer, %s", eg_difficulty_label(u->g.opp_difficulty[0] ? u->g.opp_difficulty : "intermediate"));
    else lv_label_set_text(u->opp_sub, "Player");
    lv_label_set_text(u->you_sub, u->g.you_white ? "Playing white" : "Playing black");
    if (u->g.in_progress) lv_obj_remove_flag(u->btn_menu, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(u->btn_menu, LV_OBJ_FLAG_HIDDEN);

    char you = u->g.you_white ? 'w' : 'b', opp = u->g.you_white ? 'b' : 'w';
    int lost_you[5], lost_opp[5];
    int v_you_lost = missing(&u->g, you, lost_you);
    int v_opp_lost = missing(&u->g, opp, lost_opp);
    draw_caps(u->you_caps, lost_opp, 'b', v_opp_lost - v_you_lost);   /* what you took: their (dark) pieces */
    draw_caps(u->opp_caps, lost_you, 'w', v_you_lost - v_opp_lost);   /* what they took: your (light) pieces */

    set_status(u);
    if (u->g.moves_n) lv_label_set_text_fmt(u->last, "Last move %s", u->g.moves[u->g.moves_n - 1]);
    else lv_label_set_text(u->last, "");
    rebuild_moves(u);
}

/* ---------- input ---------- */

static void close_overlay(lv_obj_t **o)
{
    if (*o) { lv_obj_delete(*o); *o = NULL; }
}

/* Show our move right away; the server's answer replaces this when it arrives. */
static void apply_local(ui_t *u, const char *from, const char *to, char promo)
{
    int f = eg_sq_index(from), t = eg_sq_index(to);
    if (f < 0 || t < 0) return;
    char pc = u->g.board[f];
    char lower = (char)tolower((unsigned char)pc);
    bool white = isupper((unsigned char)pc);
    if (lower == 'p' && f % 8 != t % 8 && !u->g.board[t]) u->g.board[(f / 8) * 8 + t % 8] = 0;   /* en passant */
    if (lower == 'k' && abs(f % 8 - t % 8) == 2) {                                                    /* castling: move the rook */
        int row = f / 8;
        bool king_side = t % 8 > f % 8;
        int rf = row * 8 + (king_side ? 7 : 0), rt = row * 8 + (king_side ? 5 : 3);
        u->g.board[rt] = u->g.board[rf];
        u->g.board[rf] = 0;
    }
    u->g.board[t] = promo ? (char)(white ? toupper((unsigned char)promo) : promo) : pc;
    u->g.board[f] = 0;
    memcpy(u->g.last_from, from, 3);
    memcpy(u->g.last_to, to, 3);
    u->g.legal_n = 0;
}

static void send_move(ui_t *u, char promo)
{
    u->selected = -1;
    apply_local(u, u->pending_from, u->pending_to, promo);
    u->sent = true;
    redraw_board(u);
    redraw_panel(u);
    if (u->cb.on_move) u->cb.on_move(u->pending_from, u->pending_to, promo);
}
static void promo_cb(lv_event_t *e)
{
    lv_obj_t *screen = lv_event_get_user_data(e);
    ui_t *u = U(screen);
    char promo = (char)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    close_overlay(&u->promo);
    send_move(u, promo);
}

static void open_promotion(lv_obj_t *screen, ui_t *u)
{
    close_overlay(&u->promo);
    u->promo = lv_obj_create(screen);
    plain(u->promo);
    lv_obj_set_size(u->promo, BOARD, BOARD);
    lv_obj_set_pos(u->promo, BOARD_X, BOARD_Y);
    lv_obj_set_style_bg_color(u->promo, EG_BG_VOID, 0);
    lv_obj_set_style_bg_opa(u->promo, LV_OPA_80, 0);
    lv_obj_t *title = mk_label(u->promo, &eg_bungee_28, EG_YELLOW, "PROMOTE TO");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 80);
    static const char kinds[4] = {'q', 'r', 'b', 'n'};
    char col = 'w';                           /* your pieces use the light art */
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = lv_obj_create(u->promo);
        lv_obj_set_size(b, 96, 96);
        lv_obj_align(b, LV_ALIGN_CENTER, (i - 1) * 108 - 54, 20);
        lv_obj_set_style_bg_color(b, EG_SURFACE, 0);
        lv_obj_set_style_border_color(b, EG_CYAN, 0);
        lv_obj_set_style_border_width(b, 3, 0);
        lv_obj_set_style_radius(b, 18, 0);
        lv_obj_set_style_pad_all(b, 0, 0);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_user_data(b, (void *)(intptr_t)kinds[i]);
        const void *src = eg_piece_src(col, kinds[i]);
        if (src) {
            lv_obj_t *im = lv_image_create(b);
            lv_image_set_src(im, src);
            lv_image_set_scale(im, 256 * 80 / EG_PIECE_PX);
            lv_obj_center(im);
        }
        lv_obj_add_event_cb(b, promo_cb, LV_EVENT_CLICKED, screen);
    }
}

static void cell_cb(lv_event_t *e)
{
    lv_obj_t *screen = lv_event_get_user_data(e);
    ui_t *u = U(screen);
    int cell = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    int sq = cell_of(u, cell);
    if (!u->g.your_turn || !u->g.in_progress || u->promo || u->sent) return;

    if (u->selected >= 0) {
        char from[3], to[3];
        sq_name(u->selected, from);
        sq_name(sq, to);
        bool promo = false;
        if (sq != u->selected && eg_move_legal(&u->g, from, to, &promo)) {
            memcpy(u->pending_from, from, 3);
            memcpy(u->pending_to, to, 3);
            if (promo) open_promotion(screen, u);
            else send_move(u, 0);
            return;
        }
    }
    if (eg_is_yours(&u->g, sq) && sq != u->selected) u->selected = sq;
    else u->selected = -1;
    show_targets(u);
}

/* ---------- Menu: Home, Resign (two taps), Close ---------- */

static uint32_t resign_armed_at;

static void menu_item_cb(lv_event_t *e)
{
    lv_obj_t *screen = lv_event_get_user_data(e);
    ui_t *u = U(screen);
    lv_obj_t *item = lv_event_get_current_target(e);
    int which = (int)(intptr_t)lv_obj_get_user_data(item);
    if (which == 1) {                                   /* Resign: second tap within 3 s confirms */
        uint32_t now = lv_tick_get();
        if (!resign_armed_at || now - resign_armed_at > 3000) {
            resign_armed_at = now;
            lv_label_set_text(lv_obj_get_child(item, 0), "Tap again to resign");
            return;
        }
        resign_armed_at = 0;
        close_overlay(&u->menu);
        if (u->cb.on_resign) u->cb.on_resign();
        return;
    }
    close_overlay(&u->menu);
    if (which == 0 && u->cb.on_menu_home) u->cb.on_menu_home();
}

static void menu_cb(lv_event_t *e)
{
    lv_obj_t *screen = lv_event_get_user_data(e);
    ui_t *u = U(screen);
    if (u->menu) { close_overlay(&u->menu); return; }
    resign_armed_at = 0;
    u->menu = lv_obj_create(screen);
    plain(u->menu);
    int n = 2;
    lv_obj_set_size(u->menu, 288, 12 + n * 62);
    lv_obj_set_pos(u->menu, 800 - 12 - 288, 58);
    lv_obj_set_style_bg_color(u->menu, EG_SURFACE, 0);
    lv_obj_set_style_bg_opa(u->menu, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(u->menu, 3, 0);
    lv_obj_set_style_border_color(u->menu, EG_PINK, 0);
    lv_obj_set_style_radius(u->menu, 18, 0);
    const char *items[2] = {"Resign", "Close"};
    int ids[2] = {1, 2};
    for (int i = 0; i < n; i++) {
        lv_obj_t *b = lv_obj_create(u->menu);
        plain(b);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(b, 288, 60);
        lv_obj_set_pos(b, 0, 6 + i * 62);
        lv_obj_set_user_data(b, (void *)(intptr_t)ids[i]);
        lv_obj_center(mk_label(b, &eg_sora_20_bold, ids[i] == 1 ? EG_PINK_SOFT : EG_FG, items[i]));
        lv_obj_add_event_cb(b, menu_item_cb, LV_EVENT_CLICKED, screen);
    }
}

static void back_cb(lv_event_t *e)
{
    ui_t *u = U(lv_event_get_user_data(e));
    close_overlay(&u->menu);
    if (u->cb.on_menu_home) u->cb.on_menu_home();
}

/* ---------- build ---------- */

static lv_obj_t *mk_button(lv_obj_t *p, const char *icon, const char *txt, lv_color_t accent, int x, int y, int w)
{
    lv_obj_t *b = lv_obj_create(p);
    lv_obj_set_size(b, w, 40);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_style_bg_color(b, EG_SURFACE, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_border_color(b, accent, 0);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    int tx = 0;
    if (icon) {
        lv_obj_t *ic = mk_label(b, &lv_font_montserrat_20, accent, icon);
        lv_obj_align(ic, LV_ALIGN_LEFT_MID, 14, 0);
        tx = 40;
    }
    lv_obj_t *l = mk_label(b, &eg_sora_20_bold, EG_FG, txt);
    if (icon) lv_obj_align(l, LV_ALIGN_LEFT_MID, tx, 0);
    else lv_obj_center(l);
    return b;
}

/* A player card: round avatar, name, one line under it, and the clock on the right. */
static lv_obj_t *mk_card(lv_obj_t *p, int y, lv_obj_t **avatar, lv_obj_t **avatar_txt, lv_obj_t **name, lv_obj_t **sub)
{
    lv_obj_t *c = lv_obj_create(p);
    plain(c);
    lv_obj_set_size(c, RAIL_W, 64);
    lv_obj_set_pos(c, 0, y);
    lv_obj_set_style_bg_color(c, EG_SURFACE, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, 14, 0);
    lv_obj_t *a = lv_obj_create(c);
    plain(a);
    lv_obj_set_size(a, 44, 44);
    lv_obj_set_pos(a, 10, 10);
    lv_obj_set_style_radius(a, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(a, EG_BG_MIST, 0);
    lv_obj_set_style_bg_opa(a, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(a, 2, 0);
    lv_obj_set_style_border_color(a, EG_PINK, 0);
    *avatar_txt = mk_label(a, &eg_sora_20_bold, EG_FG, "");
    lv_obj_center(*avatar_txt);
    *name = mk_label(c, &eg_sora_20_bold, EG_FG, "");
    lv_label_set_long_mode(*name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(*name, 130);
    lv_obj_set_pos(*name, 64, 8);
    *sub = mk_label(c, &eg_sora_16, EG_FG_HAZE, "");
    lv_label_set_long_mode(*sub, LV_LABEL_LONG_DOT);
    lv_obj_set_size(*sub, 200, 20);
    lv_obj_set_pos(*sub, 64, 36);
    if (avatar) *avatar = a;
    return c;
}

lv_obj_t *eg_game_create(lv_obj_t *parent, const eg_game_cb_t *cb)
{
    ui_t *u = calloc(1, sizeof *u);
    u->cb = *cb;
    u->selected = -1;

    lv_obj_t *scr = lv_obj_create(parent);
    plain(scr);
    lv_obj_set_size(scr, 800, 480);
    lv_obj_set_style_bg_color(scr, EG_BG_STAGE, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_user_data(scr, u);

    /* board frame: pink offset shadow, cyan border (the website board) */
    lv_obj_t *shadow = lv_obj_create(scr);
    plain(shadow);
    lv_obj_set_size(shadow, BOARD + 8, BOARD + 8);
    lv_obj_set_pos(shadow, BOARD_X - 4 + 8, BOARD_Y - 4 + 8);
    lv_obj_set_style_bg_color(shadow, EG_BOARD_SHADOW, 0);
    lv_obj_set_style_bg_opa(shadow, LV_OPA_COVER, 0);
    lv_obj_t *frame = lv_obj_create(scr);
    plain(frame);
    lv_obj_set_size(frame, BOARD + 8, BOARD + 8);
    lv_obj_set_pos(frame, BOARD_X - 4, BOARD_Y - 4);
    lv_obj_set_style_bg_color(frame, EG_BOARD_EDGE, 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);

    lv_obj_t *board = lv_obj_create(scr);
    plain(board);
    lv_obj_set_size(board, BOARD, BOARD);
    lv_obj_set_pos(board, BOARD_X, BOARD_Y);
    for (int i = 0; i < 64; i++) {
        int sr = i / 8, sf = i % 8;
        lv_obj_t *c = lv_obj_create(board);
        plain(c);
        lv_obj_set_size(c, SQ, SQ);
        lv_obj_set_pos(c, sf * SQ, sr * SQ);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
        lv_obj_set_user_data(c, (void *)(intptr_t)i);
        lv_obj_add_event_cb(c, cell_cb, LV_EVENT_CLICKED, scr);
        u->cells[i] = c;

        u->tint[i] = lv_obj_create(c);
        plain(u->tint[i]);
        lv_obj_set_size(u->tint[i], SQ, SQ);
        lv_obj_clear_flag(u->tint[i], LV_OBJ_FLAG_CLICKABLE);

        u->piece[i] = lv_image_create(c);
        /* pieces are stored at EG_PIECE_PX == SQ: drawn 1:1, no scaling */
        lv_obj_center(u->piece[i]);
        lv_obj_clear_flag(u->piece[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(u->piece[i], LV_OBJ_FLAG_HIDDEN);

        u->mark[i] = lv_obj_create(c);
        plain(u->mark[i]);
        lv_obj_set_size(u->mark[i], SQ - 6, SQ - 6);
        lv_obj_center(u->mark[i]);
        lv_obj_clear_flag(u->mark[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(u->mark[i], LV_OBJ_FLAG_HIDDEN);

        /* coordinates, like the website: file bottom-right on the bottom row, rank top-left on the left column */
        char t[2] = {0, 0};
        if (sr == 7) {
            t[0] = (char)('a' + sf);
            lv_obj_t *l = mk_label(c, &eg_sora_16, EG_PINK_SOFT, t);
            lv_obj_set_style_text_opa(l, LV_OPA_60, 0);
            lv_obj_align(l, LV_ALIGN_BOTTOM_RIGHT, -3, 0);
            lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
        }
        if (sf == 0) {
            t[0] = (char)('8' - sr);
            lv_obj_t *l = mk_label(c, &eg_sora_16, EG_PINK_SOFT, t);
            lv_obj_set_style_text_opa(l, LV_OPA_60, 0);
            lv_obj_align(l, LV_ALIGN_TOP_LEFT, 3, 0);
            lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
        }
    }

    /* right panel: Back and Menu, opponent, status, moves, you */
    lv_obj_t *pn = lv_obj_create(scr);
    plain(pn);
    lv_obj_set_size(pn, RAIL_W, 480);
    lv_obj_set_pos(pn, PANEL_X, 0);

    u->btn_back = mk_button(pn, LV_SYMBOL_LEFT, "Back", EG_CYAN, 0, 12, 128);
    lv_obj_add_event_cb(u->btn_back, back_cb, LV_EVENT_CLICKED, scr);
    u->btn_menu = mk_button(pn, NULL, "Menu", EG_YELLOW, RAIL_W - 112, 12, 112);
    lv_obj_add_event_cb(u->btn_menu, menu_cb, LV_EVENT_CLICKED, scr);

    lv_obj_t *opp_sub_card = mk_card(pn, 64, &u->avatar, &u->avatar_txt, &u->opp_name, &u->opp_sub);
    u->clk_opp = mk_clock(opp_sub_card, RAIL_W - 90, 12);
    u->opp_caps = lv_obj_create(pn);
    plain(u->opp_caps);
    lv_obj_set_size(u->opp_caps, RAIL_W, 26);
    lv_obj_set_pos(u->opp_caps, 4, 132);

    u->status = mk_label(pn, &eg_sora_20_bold, EG_FG_HAZE, "");
    lv_label_set_long_mode(u->status, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(u->status, RAIL_W);
    lv_obj_set_pos(u->status, 4, 168);
    u->last = mk_label(pn, &eg_sora_16, EG_YELLOW, "");
    lv_obj_set_pos(u->last, 4, 196);

    u->drawer_list = lv_obj_create(pn);
    plain(u->drawer_list);
    lv_obj_set_size(u->drawer_list, RAIL_W, 120);
    lv_obj_set_pos(u->drawer_list, 0, 224);
    lv_obj_set_style_bg_color(u->drawer_list, EG_BG_VOID, 0);
    lv_obj_set_style_bg_opa(u->drawer_list, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(u->drawer_list, 10, 0);
    lv_obj_set_style_pad_ver(u->drawer_list, 4, 0);
    lv_obj_set_flex_flow(u->drawer_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(u->drawer_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(u->drawer_list, LV_DIR_VER);

    u->you_caps = lv_obj_create(pn);
    plain(u->you_caps);
    lv_obj_set_size(u->you_caps, RAIL_W, 26);
    lv_obj_set_pos(u->you_caps, 4, 350);
    lv_obj_t *you_av, *you_txt, *you_sub;
    u->you_card = mk_card(pn, 404, &you_av, &you_txt, &u->you_name, &you_sub);
    lv_label_set_text(you_txt, "Y");
    lv_obj_set_style_border_color(you_av, EG_YELLOW, 0);
    lv_label_set_text(u->you_name, "You");
    lv_label_set_text(you_sub, "");
    u->you_sub = you_sub;
    u->clk_you = mk_clock(u->you_card, RAIL_W - 90, 12);
    lv_timer_create(clock_tick, 100, u);

    return scr;
}

void eg_game_set(lv_obj_t *screen, const eg_game_t *g)
{
    ui_t *u = U(screen);
    bool new_game = strcmp(u->g.id, g->id) != 0;
    /* the opponent just moved if a new last move arrived and it's now our turn */
    bool opp_moved = !new_game && g->your_turn && g->last_to[0] &&
                     (strcmp(g->last_from, u->g.last_from) != 0 || strcmp(g->last_to, u->g.last_to) != 0 || g->move_count != u->g.move_count);
    u->g = *g;
    u->flipped = !g->you_white;              /* your side is always at the bottom */
    u->selected = -1;
    u->sent = false;
    close_overlay(&u->promo);
    if (new_game) close_overlay(&u->menu);
    redraw_board(u);
    redraw_panel(u);
    clocks_from(u, g);
    if (opp_moved) animate_last_move(u);
}
