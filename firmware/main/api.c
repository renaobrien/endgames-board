/* SPDX-License-Identifier: MIT */
#include "api.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "eg_api";
#define MAX_BODY (96 * 1024)

typedef struct { char *buf; size_t len; } body_t;

static esp_err_t on_event(esp_http_client_event_t *e)
{
    body_t *b = e->user_data;
    if (e->event_id == HTTP_EVENT_ON_DATA && b && b->len + e->data_len < MAX_BODY) {
        char *n = heap_caps_realloc(b->buf, b->len + e->data_len + 1, MALLOC_CAP_SPIRAM);
        if (!n) return ESP_FAIL;
        b->buf = n;
        memcpy(b->buf + b->len, e->data, e->data_len);
        b->len += e->data_len;
        b->buf[b->len] = 0;
    }
    return ESP_OK;
}

/* One persistent HTTPS connection to the API host. A fresh TLS handshake costs about a second on this
 * chip, so every call reuses it; on any transport error it is torn down and rebuilt on the next call.
 * Only the network task calls into here, so no locking. */
static esp_http_client_handle_t s_client;
static body_t *s_body;                                /* where on_event writes the current response */

static esp_err_t on_event_shared(esp_http_client_event_t *e)
{
    e->user_data = s_body;
    return on_event(e);
}

static void client_reset(void)
{
    if (s_client) { esp_http_client_cleanup(s_client); s_client = NULL; }
}

/* Returns the HTTP status, or -1 on transport failure. *body is heap memory (free with heap_caps_free). */
static int request(esp_http_client_method_t m, const char *path, const char *token, const char *json_body, char **body)
{
    char url[256];
    snprintf(url, sizeof url, "%s/%s", EG_API_BASE, path);
    body_t b = {0};
    s_body = &b;
    for (int attempt = 0; attempt < 2; attempt++) {
        if (!s_client) {
            esp_http_client_config_t cfg = {
                .url = url, .event_handler = on_event_shared,
                .crt_bundle_attach = esp_crt_bundle_attach, .timeout_ms = 15000,
                .keep_alive_enable = true,
            };
            s_client = esp_http_client_init(&cfg);
            if (!s_client) break;
        }
        esp_http_client_set_url(s_client, url);
        esp_http_client_set_method(s_client, m);
        if (token) {
            char auth[128];
            snprintf(auth, sizeof auth, "Bearer %s", token);
            esp_http_client_set_header(s_client, "Authorization", auth);
        } else {
            esp_http_client_delete_header(s_client, "Authorization");
        }
        if (json_body) {
            esp_http_client_set_header(s_client, "Content-Type", "application/json");
            esp_http_client_set_post_field(s_client, json_body, (int)strlen(json_body));
        } else {
            esp_http_client_delete_header(s_client, "Content-Type");
            esp_http_client_set_post_field(s_client, NULL, 0);
        }
        esp_err_t err = esp_http_client_perform(s_client);
        if (err == ESP_OK) {
            int status = esp_http_client_get_status_code(s_client);
            *body = b.buf;
            s_body = NULL;
            return status;
        }
        ESP_LOGW(TAG, "%s failed: %s (attempt %d)", path, esp_err_to_name(err), attempt + 1);
        heap_caps_free(b.buf);
        b = (body_t){0};
        client_reset();                                /* stale keep-alive socket: reconnect once */
    }
    s_body = NULL;
    *body = NULL;
    return -1;
}

static void copy_str(char *dst, size_t n, const cJSON *j)
{
    if (cJSON_IsString(j)) snprintf(dst, n, "%s", j->valuestring);
}

/* Fills out from the "game" object. Returns false if game is null or malformed. */
static bool parse_game(const cJSON *game, eg_game_t *g)
{
    if (!cJSON_IsObject(game)) return false;
    memset(g, 0, sizeof *g);
    g->clock_you_s = g->clock_opp_s = -1;
    copy_str(g->id, sizeof g->id, cJSON_GetObjectItem(game, "id"));
    const cJSON *fen = cJSON_GetObjectItem(game, "fen");
    if (!cJSON_IsString(fen) || !eg_parse_fen(g, fen->valuestring)) return false;
    const cJSON *yc = cJSON_GetObjectItem(game, "yourColor");
    g->you_white = cJSON_IsString(yc) && strcmp(yc->valuestring, "white") == 0;
    g->your_turn = cJSON_IsTrue(cJSON_GetObjectItem(game, "yourTurn"));
    copy_str(g->status, sizeof g->status, cJSON_GetObjectItem(game, "status"));
    g->in_progress = strcmp(g->status, "IN_PROGRESS") == 0;
    const cJSON *mc = cJSON_GetObjectItem(game, "moveCount");
    g->move_count = cJSON_IsNumber(mc) ? mc->valueint : 0;
    const cJSON *lm = cJSON_GetObjectItem(game, "lastMove");
    if (cJSON_IsObject(lm)) {
        copy_str(g->last_from, sizeof g->last_from, cJSON_GetObjectItem(lm, "from"));
        copy_str(g->last_to, sizeof g->last_to, cJSON_GetObjectItem(lm, "to"));
    }
    const cJSON *opp = cJSON_GetObjectItem(game, "opponent");
    copy_str(g->opponent, sizeof g->opponent, cJSON_GetObjectItem(opp, "name"));
    g->opp_ai = cJSON_IsTrue(cJSON_GetObjectItem(opp, "isAi"));
    copy_str(g->opp_difficulty, sizeof g->opp_difficulty, cJSON_GetObjectItem(opp, "difficulty"));
    const cJSON *legal = cJSON_GetObjectItem(game, "legalMoves");
    cJSON *it;
    cJSON_ArrayForEach(it, legal) {
        if (g->legal_n < EG_MAX_LEGAL && cJSON_IsString(it)) snprintf(g->legal[g->legal_n++], 6, "%s", it->valuestring);
    }
    const cJSON *moves = cJSON_GetObjectItem(game, "moves");
    int total = cJSON_GetArraySize(moves);
    int start = total > 160 ? total - 160 : 0;
    g->moves_first_ply = start;
    for (int i = start; i < total; i++) {
        const cJSON *m = cJSON_GetArrayItem(moves, i);
        if (cJSON_IsString(m)) snprintf(g->moves[g->moves_n++], 8, "%s", m->valuestring);
    }
    return true;
}

eg_result_t eg_api_pair_start(eg_pairing_t *out)
{
    char *body = NULL;
    int st = request(HTTP_METHOD_POST, "board-pair-start", NULL, NULL, &body);
    eg_result_t r = EG_ERROR;
    if (st == 200 && body) {
        cJSON *j = cJSON_Parse(body);
        copy_str(out->code, sizeof out->code, cJSON_GetObjectItem(j, "code"));
        copy_str(out->poll_secret, sizeof out->poll_secret, cJSON_GetObjectItem(j, "pollSecret"));
        copy_str(out->claim_url, sizeof out->claim_url, cJSON_GetObjectItem(j, "claimUrl"));
        r = out->code[0] && out->poll_secret[0] ? EG_OK : EG_ERROR;
        cJSON_Delete(j);
    }
    heap_caps_free(body);
    return r;
}

eg_result_t eg_api_pair_poll(const char *poll_secret, char token_out[96])
{
    char req[160];
    snprintf(req, sizeof req, "{\"pollSecret\":\"%s\"}", poll_secret);
    char *body = NULL;
    int st = request(HTTP_METHOD_POST, "board-pair-poll", NULL, req, &body);
    eg_result_t r = EG_ERROR;
    if (st == 410) r = EG_EXPIRED;
    else if ((st == 200 || st == 202) && body) {
        cJSON *j = cJSON_Parse(body);
        const cJSON *s = cJSON_GetObjectItem(j, "status");
        if (cJSON_IsString(s) && strcmp(s->valuestring, "paired") == 0) {
            copy_str(token_out, 96, cJSON_GetObjectItem(j, "deviceToken"));
            r = token_out[0] ? EG_OK : EG_ERROR;
        } else {
            r = EG_PENDING;
        }
        cJSON_Delete(j);
    }
    heap_caps_free(body);
    return r;
}

eg_result_t eg_api_game(const char *token, eg_game_t *out)
{
    char *body = NULL;
    int st = request(HTTP_METHOD_GET, "board-game", token, NULL, &body);
    eg_result_t r = EG_ERROR;
    if (st == 401) r = EG_UNAUTHORIZED;
    else if (st == 200 && body) {
        cJSON *j = cJSON_Parse(body);
        r = parse_game(cJSON_GetObjectItem(j, "game"), out) ? EG_OK : EG_NO_GAME;
        cJSON_Delete(j);
    }
    heap_caps_free(body);
    return r;
}

eg_result_t eg_api_move(const char *token, const eg_game_t *g, const char *from, const char *to, char promo, eg_game_t *out)
{
    char req[200];
    int n = snprintf(req, sizeof req, "{\"gameId\":\"%s\",\"from\":\"%s\",\"to\":\"%s\",\"moveCount\":%d", g->id, from, to, g->move_count);
    if (promo) n += snprintf(req + n, sizeof req - n, ",\"promotion\":\"%c\"", promo);
    snprintf(req + n, sizeof req - n, "}");
    char *body = NULL;
    int st = request(HTTP_METHOD_POST, "board-move", token, req, &body);
    eg_result_t r = EG_ERROR;
    if (st == 401) r = EG_UNAUTHORIZED;
    else if (st == 200 || st == 409 || st == 422) {
        r = st == 200 ? EG_OK : (st == 409 ? EG_CONFLICT : EG_ILLEGAL);
        if (body) {
            cJSON *j = cJSON_Parse(body);
            parse_game(cJSON_GetObjectItem(j, "game"), out);   /* every 200/409/422 carries the current game */
            cJSON_Delete(j);
        }
    }
    heap_caps_free(body);
    return r;
}

eg_result_t eg_api_pieces(const char *token, char set_id[48], char urls[12][160])
{
    char *body = NULL;
    int st = request(HTTP_METHOD_GET, "board-pieces", token, NULL, &body);
    eg_result_t r = EG_ERROR;
    if (st == 401) r = EG_UNAUTHORIZED;
    else if (st == 200 && body) {
        cJSON *j = cJSON_Parse(body);
        copy_str(set_id, 48, cJSON_GetObjectItem(cJSON_GetObjectItem(j, "set"), "id"));
        const cJSON *p = cJSON_GetObjectItem(j, "pieces");
        static const char types[6] = {'k', 'q', 'r', 'b', 'n', 'p'};
        int ok = 0;
        for (int c = 0; c < 2; c++)
            for (int t = 0; t < 6; t++) {
                char key[3] = {c ? 'b' : 'w', types[t], 0};
                const cJSON *u = cJSON_GetObjectItem(p, key);
                if (cJSON_IsString(u)) { snprintf(urls[c * 6 + t], 160, "%s", u->valuestring); ok++; }
            }
        r = ok == 12 ? EG_OK : EG_ERROR;
        cJSON_Delete(j);
    }
    heap_caps_free(body);
    return r;
}

/* ---------- home, new games, sets ---------- */

static eg_result_t game_reply(int st, char *body, eg_game_t *out)
{
    eg_result_t r = EG_ERROR;
    if (st == 401) r = EG_UNAUTHORIZED;
    else if (st == 200 && body) {
        cJSON *j = cJSON_Parse(body);
        r = parse_game(cJSON_GetObjectItem(j, "game"), out) ? EG_OK : EG_NO_GAME;
        cJSON_Delete(j);
    }
    heap_caps_free(body);
    return r;
}

eg_result_t eg_api_home(const char *token, eg_home_t *out)
{
    memset(out, 0, sizeof *out);
    out->elo = -1;
    char *body = NULL;
    int st = request(HTTP_METHOD_GET, "board-home", token, NULL, &body);
    eg_result_t r = EG_ERROR;
    if (st == 401) r = EG_UNAUTHORIZED;
    else if (st == 200 && body) {
        cJSON *j = cJSON_Parse(body);
        const cJSON *prof = cJSON_GetObjectItem(j, "profile");
        copy_str(out->name, sizeof out->name, cJSON_GetObjectItem(prof, "name"));
        const cJSON *elo = cJSON_GetObjectItem(prof, "elo");
        if (cJSON_IsNumber(elo) && cJSON_IsTrue(cJSON_GetObjectItem(prof, "ranked"))) out->elo = elo->valueint;
        const cJSON *g;
        cJSON_ArrayForEach(g, cJSON_GetObjectItem(j, "games")) {
            if (out->n_games >= EG_HOME_MAX_GAMES) break;
            eg_home_game_t *h = &out->games[out->n_games];
            copy_str(h->id, sizeof h->id, cJSON_GetObjectItem(g, "id"));
            const cJSON *opp = cJSON_GetObjectItem(g, "opponent");
            copy_str(h->opponent, sizeof h->opponent, cJSON_GetObjectItem(opp, "name"));
            h->opponent_ai = cJSON_IsTrue(cJSON_GetObjectItem(opp, "isAi"));
            copy_str(h->difficulty, sizeof h->difficulty, cJSON_GetObjectItem(opp, "difficulty"));
            const cJSON *col = cJSON_GetObjectItem(g, "yourColor");
            h->your_color = cJSON_IsString(col) && col->valuestring[0] == 'b' ? 'b' : 'w';
            h->your_turn = cJSON_IsTrue(cJSON_GetObjectItem(g, "yourTurn"));
            const cJSON *mc = cJSON_GetObjectItem(g, "moveCount");
            h->move_count = cJSON_IsNumber(mc) ? mc->valueint : 0;
            if (h->id[0]) out->n_games++;
        }
        copy_str(out->active_set, sizeof out->active_set, cJSON_GetObjectItem(j, "activeSetId"));
        const cJSON *st2;
        cJSON_ArrayForEach(st2, cJSON_GetObjectItem(j, "sets")) {
            if (out->n_sets >= EG_HOME_MAX_SETS) break;
            eg_set_t *e = &out->sets[out->n_sets];
            copy_str(e->id, sizeof e->id, cJSON_GetObjectItem(st2, "id"));
            copy_str(e->name, sizeof e->name, cJSON_GetObjectItem(st2, "name"));
            const cJSON *pv = cJSON_GetObjectItem(st2, "preview");
            copy_str(e->preview_k, sizeof e->preview_k, cJSON_GetObjectItem(pv, "wk"));
            copy_str(e->preview_n, sizeof e->preview_n, cJSON_GetObjectItem(pv, "wn"));
            if (e->id[0]) out->n_sets++;
        }
        r = EG_OK;
        cJSON_Delete(j);
    }
    heap_caps_free(body);
    return r;
}

eg_result_t eg_api_game_id(const char *token, const char *game_id, eg_game_t *out)
{
    char path[96];
    snprintf(path, sizeof path, "board-game?gameId=%s", game_id);
    char *body = NULL;
    int st = request(HTTP_METHOD_GET, path, token, NULL, &body);
    return game_reply(st, body, out);
}

eg_result_t eg_api_new_ai(const char *token, const char *difficulty, const char *color, eg_game_t *out)
{
    char req[128];
    snprintf(req, sizeof req, "{\"mode\":\"ai\",\"difficulty\":\"%s\",\"color\":\"%s\"}", difficulty, color);
    char *body = NULL;
    int st = request(HTTP_METHOD_POST, "board-new-game", token, req, &body);
    return game_reply(st, body, out);
}

eg_result_t eg_api_new_challenge(const char *token, const char *color, char url[160])
{
    char req[96];
    snprintf(req, sizeof req, "{\"mode\":\"challenge\",\"color\":\"%s\"}", color);
    char *body = NULL;
    int st = request(HTTP_METHOD_POST, "board-new-game", token, req, &body);
    eg_result_t r = EG_ERROR;
    url[0] = 0;
    if (st == 401) r = EG_UNAUTHORIZED;
    else if (st == 200 && body) {
        cJSON *j = cJSON_Parse(body);
        copy_str(url, 160, cJSON_GetObjectItem(cJSON_GetObjectItem(j, "challenge"), "url"));
        r = strncmp(url, "https://", 8) == 0 ? EG_OK : EG_ERROR;
        cJSON_Delete(j);
    }
    heap_caps_free(body);
    return r;
}

eg_result_t eg_api_resign(const char *token, const char *game_id, eg_game_t *out)
{
    char req[96];
    snprintf(req, sizeof req, "{\"gameId\":\"%s\"}", game_id);
    char *body = NULL;
    int st = request(HTTP_METHOD_POST, "board-resign", token, req, &body);
    return game_reply(st, body, out);
}

eg_result_t eg_api_set(const char *token, const char *set_id)
{
    char req[96];
    snprintf(req, sizeof req, "{\"setId\":\"%s\"}", set_id);
    char *body = NULL;
    int st = request(HTTP_METHOD_POST, "board-set", token, req, &body);
    heap_caps_free(body);
    return st == 200 ? EG_OK : st == 401 ? EG_UNAUTHORIZED : EG_ERROR;
}

eg_result_t eg_api_leaderboard(const char *token, eg_rank_t *out)
{
    memset(out, 0, sizeof *out);
    out->you_rank = out->you_elo = -1;
    char *body = NULL;
    int st = request(HTTP_METHOD_GET, "board-leaderboard", token, NULL, &body);
    eg_result_t r = EG_ERROR;
    if (st == 401) r = EG_UNAUTHORIZED;
    else if (st == 200 && body) {
        cJSON *j = cJSON_Parse(body);
        const cJSON *e;
        cJSON_ArrayForEach(e, cJSON_GetObjectItem(j, "entries")) {
            if (out->n >= EG_RANK_MAX) break;
            const cJSON *rk = cJSON_GetObjectItem(e, "rank"), *el = cJSON_GetObjectItem(e, "elo");
            out->rows[out->n].rank = cJSON_IsNumber(rk) ? rk->valueint : out->n + 1;
            out->rows[out->n].elo = cJSON_IsNumber(el) ? el->valueint : 0;
            copy_str(out->rows[out->n].name, sizeof out->rows[0].name, cJSON_GetObjectItem(e, "name"));
            out->rows[out->n].you = cJSON_IsTrue(cJSON_GetObjectItem(e, "isYou"));
            out->n++;
        }
        const cJSON *you = cJSON_GetObjectItem(j, "you");
        const cJSON *yr = cJSON_GetObjectItem(you, "rank"), *ye = cJSON_GetObjectItem(you, "elo");
        if (cJSON_IsNumber(yr)) out->you_rank = yr->valueint;
        if (cJSON_IsNumber(ye)) out->you_elo = ye->valueint;
        r = EG_OK;
        cJSON_Delete(j);
    }
    heap_caps_free(body);
    return r;
}

void *eg_http_download(const char *url, size_t *len)
{
    body_t b = {0};
    esp_http_client_config_t cfg = {
        .url = url, .event_handler = on_event, .user_data = &b,
        .crt_bundle_attach = esp_crt_bundle_attach, .timeout_ms = 15000,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(c);
    int st = err == ESP_OK ? esp_http_client_get_status_code(c) : -1;
    esp_http_client_cleanup(c);
    if (st != 200) { heap_caps_free(b.buf); return NULL; }
    *len = b.len;
    return b.buf;
}
