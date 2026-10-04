/* SPDX-License-Identifier: MIT */
#include "ui_screens.h"
#include "fonts.h"
#include "pieces.h"
#include "theme.h"
#include <string.h>
#include <stdio.h>

static lv_obj_t *base(lv_obj_t *parent)
{
    lv_obj_t *s = lv_obj_create(parent);
    lv_obj_set_size(s, 800, 480);
    lv_obj_set_style_bg_color(s, EG_BG_STAGE, 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_style_radius(s, 0, 0);
    lv_obj_set_style_pad_all(s, 0, 0);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    return s;
}

static lv_obj_t *label(lv_obj_t *p, const lv_font_t *f, lv_color_t c, const char *t)
{
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_label_set_text(l, t);
    return l;
}

/* ---------- keyboard ----------
 * Phone-style: shift capitalizes one letter then drops back to lowercase, double-tap shift locks caps,
 * "123" and "#+=" pages cover every printable ASCII character a Wi-Fi password can use.
 * Replaces LVGL's default handler (which has a hide-keyboard key, cursor arrows and a "1#" key). */

#define K_SHIFT LV_SYMBOL_UP
#define K_BKSP  LV_SYMBOL_BACKSPACE
#define K_ENTER LV_SYMBOL_NEW_LINE

static const char *map_lower[] = {"q","w","e","r","t","y","u","i","o","p","\n",
    "a","s","d","f","g","h","j","k","l","\n",
    K_SHIFT,"z","x","c","v","b","n","m",K_BKSP,"\n",
    "123"," ",K_ENTER,""};
static const char *map_upper[] = {"Q","W","E","R","T","Y","U","I","O","P","\n",
    "A","S","D","F","G","H","J","K","L","\n",
    K_SHIFT,"Z","X","C","V","B","N","M",K_BKSP,"\n",
    "123"," ",K_ENTER,""};
static const char *map_num[] = {"1","2","3","4","5","6","7","8","9","0","\n",
    "-","/",":",";","(",")","$","&","@","\"","\n",
    "#+=",".",",","?","!","'",K_BKSP,"\n",
    "ABC"," ",K_ENTER,""};
static const char *map_sym[] = {"[","]","{","}","#","%","^","*","+","=","\n",
    "_","\\","|","~","<",">","`",".",",","\n",
    "123","?","!","'","\"","@",K_BKSP,"\n",
    "ABC"," ",K_ENTER,""};

#define NOREP LV_BUTTONMATRIX_CTRL_NO_REPEAT
/* widths are relative within a row and must stay 1..15 (higher bits are flags) */
static const lv_buttonmatrix_ctrl_t ctrl_alpha[] = {
    2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,
    3|NOREP,2,2,2,2,2,2,2,3,
    3|NOREP,10,3|NOREP};
static const lv_buttonmatrix_ctrl_t ctrl_num[] = {
    2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,
    3|NOREP,2,2,2,2,2,3,
    3|NOREP,10,3|NOREP};
static const lv_buttonmatrix_ctrl_t ctrl_sym[] = {
    2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,
    3|NOREP,2,2,2,2,2,3,
    3|NOREP,10,3|NOREP};

typedef struct {
    lv_obj_t *kb;
    bool caps_lock;
    uint32_t last_shift_ms;
    void (*on_enter)(void *ctx, lv_obj_t *ta);
    void *ctx;
} kbd_t;

static void kbd_mode(kbd_t *k, lv_keyboard_mode_t m)
{
    lv_keyboard_set_mode(k->kb, m);
    if (m == LV_KEYBOARD_MODE_TEXT_UPPER) {
        /* shift key (index 19) shows checked; locked caps also stays checked */
        lv_buttonmatrix_set_button_ctrl(k->kb, 19, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
}

static void kbd_event(lv_event_t *e)
{
    kbd_t *k = lv_event_get_user_data(e);
    lv_obj_t *kb = k->kb;
    uint32_t id = lv_buttonmatrix_get_selected_button(kb);
    if (id == LV_BUTTONMATRIX_BUTTON_NONE) return;
    const char *t = lv_buttonmatrix_get_button_text(kb, id);
    if (!t) return;
    lv_obj_t *ta = lv_keyboard_get_textarea(kb);
    lv_keyboard_mode_t mode = lv_keyboard_get_mode(kb);

    if (strcmp(t, K_SHIFT) == 0) {
        uint32_t now = lv_tick_get();
        if (mode == LV_KEYBOARD_MODE_TEXT_LOWER) {
            bool dbl = now - k->last_shift_ms < 400;
            k->caps_lock = dbl;
            kbd_mode(k, LV_KEYBOARD_MODE_TEXT_UPPER);
        } else if (!k->caps_lock && now - k->last_shift_ms < 400) {
            k->caps_lock = true;                       /* second tap of a double tap */
            kbd_mode(k, LV_KEYBOARD_MODE_TEXT_UPPER);
        } else {
            k->caps_lock = false;
            kbd_mode(k, LV_KEYBOARD_MODE_TEXT_LOWER);
        }
        k->last_shift_ms = now;
        return;
    }
    if (strcmp(t, "123") == 0) { kbd_mode(k, LV_KEYBOARD_MODE_SPECIAL); return; }
    if (strcmp(t, "#+=") == 0) { kbd_mode(k, LV_KEYBOARD_MODE_USER_1); return; }
    if (strcmp(t, "ABC") == 0) { k->caps_lock = false; kbd_mode(k, LV_KEYBOARD_MODE_TEXT_LOWER); return; }
    if (!ta) return;
    if (strcmp(t, K_BKSP) == 0) { lv_textarea_delete_char(ta); return; }
    if (strcmp(t, K_ENTER) == 0) { if (k->on_enter) k->on_enter(k->ctx, ta); return; }

    lv_textarea_add_text(ta, t);
    if (mode == LV_KEYBOARD_MODE_TEXT_UPPER && !k->caps_lock) kbd_mode(k, LV_KEYBOARD_MODE_TEXT_LOWER);
}

static kbd_t *kbd_create(lv_obj_t *parent, void (*on_enter)(void *, lv_obj_t *), void *ctx)
{
    kbd_t *k = lv_malloc(sizeof *k);
    memset(k, 0, sizeof *k);
    k->on_enter = on_enter;
    k->ctx = ctx;
    k->kb = lv_keyboard_create(parent);
    lv_obj_remove_event_cb(k->kb, lv_keyboard_def_event_cb);
    lv_keyboard_set_popovers(k->kb, false);
    lv_keyboard_set_map(k->kb, LV_KEYBOARD_MODE_TEXT_LOWER, map_lower, ctrl_alpha);
    lv_keyboard_set_map(k->kb, LV_KEYBOARD_MODE_TEXT_UPPER, map_upper, ctrl_alpha);
    lv_keyboard_set_map(k->kb, LV_KEYBOARD_MODE_SPECIAL, map_num, ctrl_num);
    lv_keyboard_set_map(k->kb, LV_KEYBOARD_MODE_USER_1, map_sym, ctrl_sym);
    lv_keyboard_set_mode(k->kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_event_cb(k->kb, kbd_event, LV_EVENT_VALUE_CHANGED, k);

    lv_obj_set_size(k->kb, 800, 216);
    lv_obj_align(k->kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_all(k->kb, 6, 0);
    lv_obj_set_style_pad_gap(k->kb, 6, 0);
    lv_obj_set_style_bg_color(k->kb, EG_BG_VOID, 0);
    lv_obj_set_style_bg_color(k->kb, EG_SURFACE, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(k->kb, EG_CYAN, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(k->kb, EG_PINK, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(k->kb, EG_FG, LV_PART_ITEMS);
    lv_obj_set_style_text_color(k->kb, EG_BG_VOID, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_font(k->kb, &lv_font_montserrat_20, LV_PART_ITEMS);
    lv_obj_set_style_border_width(k->kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(k->kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_radius(k->kb, 8, LV_PART_ITEMS);
    return k;
}

/* ---------- Wi-Fi ---------- */

typedef struct {
    lv_obj_t *ssid, *pass, *err, *list, *show_lbl;
    kbd_t *k;
    eg_wifi_cb_t cb;
    void (*rescan)(void);
} wifi_t;

static void focus_cb(lv_event_t *e)
{
    wifi_t *w = lv_event_get_user_data(e);
    lv_keyboard_set_textarea(w->k->kb, lv_event_get_target(e));
}

static void do_connect(wifi_t *w)
{
    if (!lv_textarea_get_text(w->ssid)[0]) { lv_label_set_text(w->err, "Pick a network first."); return; }
    lv_label_set_text(w->err, "Connecting...");
    if (w->cb) w->cb(lv_textarea_get_text(w->ssid), lv_textarea_get_text(w->pass));
}

static void enter_cb(void *ctx, lv_obj_t *ta)
{
    wifi_t *w = ctx;
    if (ta == w->ssid) lv_keyboard_set_textarea(w->k->kb, w->pass);
    else do_connect(w);
}

static void show_cb(lv_event_t *e)
{
    wifi_t *w = lv_event_get_user_data(e);
    bool hidden = lv_textarea_get_password_mode(w->pass);
    lv_textarea_set_password_mode(w->pass, !hidden);
    lv_label_set_text(w->show_lbl, hidden ? "Hide" : "Show");
}

static void connect_cb(lv_event_t *e)
{
    do_connect(lv_event_get_user_data(e));
}

static void rescan_cb(lv_event_t *e)
{
    wifi_t *w = lv_event_get_user_data(e);
    if (w->rescan) w->rescan();
}

static void pick_cb(lv_event_t *e)
{
    wifi_t *w = lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    lv_textarea_set_text(w->ssid, lv_label_get_text(lbl));
    lv_textarea_set_text(w->pass, "");
    lv_label_set_text(w->err, "");
    uint32_t n = lv_obj_get_child_count(w->list);
    for (uint32_t i = 0; i < n; i++) lv_obj_remove_state(lv_obj_get_child(w->list, i), LV_STATE_CHECKED);
    lv_obj_add_state(btn, LV_STATE_CHECKED);
    lv_keyboard_set_textarea(w->k->kb, w->pass);              /* keyboard jumps to the password */
}

static lv_obj_t *field(lv_obj_t *p, const char *placeholder, int y, bool pw, wifi_t *w)
{
    lv_obj_t *t = lv_textarea_create(p);
    lv_textarea_set_one_line(t, true);
    lv_textarea_set_placeholder_text(t, placeholder);
    lv_textarea_set_password_mode(t, pw);
    lv_obj_set_size(t, 360, 52);
    lv_obj_set_pos(t, 40, y);
    lv_obj_set_style_bg_color(t, EG_SURFACE, 0);
    lv_obj_set_style_text_color(t, EG_FG, 0);
    lv_obj_set_style_text_font(t, &eg_sora_20, 0);
    lv_obj_set_style_border_color(t, EG_CYAN, 0);
    lv_obj_set_style_border_width(t, 2, 0);
    lv_obj_add_event_cb(t, focus_cb, LV_EVENT_FOCUSED, w);
    lv_obj_add_event_cb(t, focus_cb, LV_EVENT_CLICKED, w);
    return t;
}

static lv_obj_t *pill(lv_obj_t *p, const char *text, lv_color_t bg, int x, int y, int wdt, int h)
{
    lv_obj_t *b = lv_button_create(p);
    lv_obj_set_size(b, wdt, h);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_style_bg_color(b, bg, 0);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_center(label(b, &eg_sora_20_bold, lv_color_white(), text));
    return b;
}

static void list_message(wifi_t *w, const char *msg)
{
    lv_obj_clean(w->list);
    lv_obj_t *l = label(w->list, &eg_sora_16, EG_FG_HAZE, msg);
    lv_obj_set_style_pad_all(l, 12, 0);
}

lv_obj_t *eg_wifi_create(lv_obj_t *parent, eg_wifi_cb_t on_connect, void (*on_rescan)(void))
{
    lv_obj_t *s = base(parent);
    wifi_t *w = lv_malloc(sizeof *w);
    memset(w, 0, sizeof *w);
    w->cb = on_connect;
    w->rescan = on_rescan;
    lv_obj_set_user_data(s, w);

    lv_obj_set_style_text_font(s, &eg_sora_20, 0);
    lv_obj_t *t = label(s, &eg_bungee_28, EG_CYAN, "CONNECT TO WI-FI");
    lv_obj_set_pos(t, 40, 20);

    /* left: name, password, connect */
    w->ssid = field(s, "Network name", 72, false, w);
    w->pass = field(s, "Password", 134, true, w);
    lv_obj_add_event_cb(pill(s, "Connect", EG_PINK, 40, 198, 180, 48), connect_cb, LV_EVENT_CLICKED, w);
    w->err = label(s, &eg_sora_16, EG_MAGENTA, "");
    lv_label_set_long_mode(w->err, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(w->err, 190);
    lv_obj_set_pos(w->err, 232, 200);

    /* right: nearby networks */
    w->list = lv_obj_create(s);
    lv_obj_set_size(w->list, 320, 140);
    lv_obj_set_pos(w->list, 440, 72);
    lv_obj_set_flex_flow(w->list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(w->list, EG_SURFACE, 0);
    lv_obj_set_style_border_color(w->list, EG_CYAN, 0);
    lv_obj_set_style_border_width(w->list, 2, 0);
    lv_obj_set_style_pad_all(w->list, 4, 0);
    lv_obj_set_style_pad_row(w->list, 4, 0);
    lv_obj_set_scroll_dir(w->list, LV_DIR_VER);
    list_message(w, "Searching...");
    lv_obj_add_event_cb(pill(s, "Scan again", EG_SURFACE, 440, 218, 180, 40), rescan_cb, LV_EVENT_CLICKED, w);

    /* Show / Hide inside the password field */
    lv_obj_t *sb = lv_button_create(s);
    lv_obj_set_size(sb, 76, 40);
    lv_obj_set_pos(sb, 40 + 360 - 76 - 6, 134 + 6);
    lv_obj_set_style_bg_color(sb, EG_BG_STAGE, 0);
    lv_obj_set_style_shadow_width(sb, 0, 0);
    lv_obj_set_style_radius(sb, 6, 0);
    w->show_lbl = label(sb, &eg_sora_16, EG_CYAN, "Show");
    lv_obj_center(w->show_lbl);
    lv_obj_add_event_cb(sb, show_cb, LV_EVENT_CLICKED, w);
    lv_obj_set_style_pad_right(w->pass, 86, 0);       /* typed text never runs under the button */

    w->k = kbd_create(s, enter_cb, w);
    lv_keyboard_set_textarea(w->k->kb, w->pass);
    return s;
}

void eg_wifi_set_error(lv_obj_t *screen, const char *msg)
{
    wifi_t *w = lv_obj_get_user_data(screen);
    lv_label_set_text(w->err, msg ? msg : "");
}

void eg_wifi_set_scanning(lv_obj_t *screen)
{
    list_message(lv_obj_get_user_data(screen), "Searching...");
}

void eg_wifi_set_networks(lv_obj_t *screen, const eg_ap_t *aps, int n)
{
    wifi_t *w = lv_obj_get_user_data(screen);
    if (n == 0) { list_message(w, "No networks found."); return; }
    lv_obj_clean(w->list);
    const char *current = lv_textarea_get_text(w->ssid);
    for (int i = 0; i < n; i++) {
        lv_obj_t *b = lv_button_create(w->list);
        lv_obj_set_width(b, LV_PCT(100));
        lv_obj_set_height(b, 40);
        lv_obj_set_style_radius(b, 6, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_set_style_bg_color(b, EG_BG_STAGE, 0);
        lv_obj_set_style_bg_color(b, EG_CYAN, LV_STATE_CHECKED);
        lv_obj_set_style_text_color(b, EG_FG, 0);
        lv_obj_set_style_text_color(b, EG_BG_VOID, LV_STATE_CHECKED);
        lv_obj_t *l = label(b, &eg_sora_20, EG_FG, aps[i].ssid);
        lv_obj_set_style_text_color(l, EG_FG, 0);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, 220);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 4, 0);
        /* signal: 4 bars, filled by strength */
        int bars = aps[i].rssi > -55 ? 4 : aps[i].rssi > -65 ? 3 : aps[i].rssi > -75 ? 2 : 1;
        for (int k = 0; k < 4; k++) {
            lv_obj_t *bar = lv_obj_create(b);
            lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_size(bar, 5, 6 + k * 4);
            lv_obj_set_style_border_width(bar, 0, 0);
            lv_obj_set_style_radius(bar, 1, 0);
            lv_obj_set_style_bg_color(bar, k < bars ? EG_CYAN : EG_FG_MIST, 0);
            lv_obj_set_style_bg_opa(bar, k < bars ? LV_OPA_COVER : LV_OPA_30, 0);
            lv_obj_align(bar, LV_ALIGN_BOTTOM_RIGHT, -4 - (3 - k) * 8, -8);
        }
        if (current[0] && strcmp(current, aps[i].ssid) == 0) lv_obj_add_state(b, LV_STATE_CHECKED);
        lv_obj_add_event_cb(b, pick_cb, LV_EVENT_CLICKED, w);
    }
}

/* ---------- pairing ---------- */

typedef struct { lv_obj_t *code, *qr, *status; } pair_t;

#define EG_PAIR_URL "https://endgam.es/board/pair"

lv_obj_t *eg_pair_create(lv_obj_t *parent)
{
    lv_obj_t *s = base(parent);
    pair_t *p = lv_malloc(sizeof *p);
    memset(p, 0, sizeof *p);
    lv_obj_set_user_data(s, p);

    /* left: QR on a white tile (phones need the light quiet zone) */
    lv_obj_t *tile = lv_obj_create(s);
    lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(tile, 300, 300);
    lv_obj_set_pos(tile, 60, 90);
    lv_obj_set_style_bg_color(tile, lv_color_white(), 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_radius(tile, 16, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    p->qr = lv_qrcode_create(tile);
    lv_qrcode_set_size(p->qr, 260);
    lv_qrcode_set_dark_color(p->qr, EG_BG_VOID);
    lv_qrcode_set_light_color(p->qr, lv_color_white());
    lv_qrcode_update(p->qr, EG_PAIR_URL, strlen(EG_PAIR_URL));
    lv_obj_center(p->qr);

    /* right: instructions and the typed fallback */
    lv_obj_t *t = label(s, &eg_bungee_28, EG_YELLOW, "PAIR THIS BOARD");
    lv_obj_set_pos(t, 410, 90);
    lv_obj_t *a1 = label(s, &eg_sora_20, EG_FG, "Scan with your phone.");
    lv_obj_set_pos(a1, 410, 150);
    lv_obj_t *a2 = label(s, &eg_sora_16, EG_FG_HAZE, "No camera? Go to");
    lv_obj_set_pos(a2, 410, 206);
    lv_obj_t *u = label(s, &eg_sora_20_bold, EG_CYAN, "endgam.es/board/pair");
    lv_obj_set_pos(u, 410, 230);
    lv_obj_t *a3 = label(s, &eg_sora_16, EG_FG_HAZE, "and enter");
    lv_obj_set_pos(a3, 410, 262);
    p->code = label(s, &eg_bungee_44, EG_PINK, "------");
    lv_obj_set_style_text_letter_space(p->code, 8, 0);
    lv_obj_set_pos(p->code, 410, 288);

    p->status = label(s, &eg_sora_16, EG_FG_MIST, "");
    lv_obj_set_pos(p->status, 410, 360);
    return s;
}

void eg_pair_set_code(lv_obj_t *screen, const char *code, const char *claim_url)
{
    (void)claim_url;                     /* the QR always carries the code, whatever the server sends */
    pair_t *p = lv_obj_get_user_data(screen);
    lv_label_set_text(p->code, code);
    char url[96];
    snprintf(url, sizeof url, EG_PAIR_URL "?code=%s", code);
    lv_qrcode_update(p->qr, url, strlen(url));
}

void eg_pair_set_status(lv_obj_t *screen, const char *msg)
{
    pair_t *p = lv_obj_get_user_data(screen);
    lv_label_set_text(p->status, msg);
}

/* ---------- idle ---------- */

lv_obj_t *eg_idle_create(lv_obj_t *parent)
{
    lv_obj_t *s = base(parent);
    lv_obj_t *t = label(s, &eg_bungee_28, EG_CYAN, "NO GAME IN PROGRESS");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_t *sub = label(s, &eg_sora_20, EG_FG_HAZE, "Start one on endgam.es and it shows up here.");
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 92);
    eg_idle_refresh(s);
    return s;
}

void eg_idle_refresh(lv_obj_t *s)
{
    static const char order[6] = {'k', 'q', 'r', 'b', 'n', 'p'};
    lv_obj_t *old = lv_obj_get_child(s, -1);
    if (old && lv_obj_get_user_data(old) == (void *)1) lv_obj_delete(old);
    lv_obj_t *row = lv_obj_create(s);
    lv_obj_set_user_data(row, (void *)1);
    lv_obj_set_size(row, 6 * 100, 100);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, 70);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < 6; i++) {
        const void *src = eg_piece_src('w', order[i]);
        if (!src) continue;
        lv_obj_t *im = lv_image_create(row);
        lv_image_set_src(im, src);
        lv_image_set_scale(im, 256 * 90 / 60);
        lv_obj_set_pos(im, i * 100 + 5, 5);
    }
}
