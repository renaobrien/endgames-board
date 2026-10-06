/* SPDX-License-Identifier: MIT */
/* Display modes: Dark (default), Light, Mono. The colors themselves come from the generated theme.h
 * (dark tokens as EG_*, light as EG_LIGHT_*, mono as EG_MONO_*). Include this instead of theme.h:
 * every EG_<color> then reads the palette picked at boot. Changing the mode restarts the console. */
#pragma once
#include "theme.h"

typedef enum { EG_MODE_DARK, EG_MODE_LIGHT, EG_MODE_MONO } eg_mode_t;

typedef struct {
    lv_color_t bg_void;
    lv_color_t bg_night;
    lv_color_t bg_haze;
    lv_color_t bg_mist;
    lv_color_t bg_stage;
    lv_color_t surface;
    lv_color_t cyan;
    lv_color_t cyan_deep;
    lv_color_t pink;
    lv_color_t pink_soft;
    lv_color_t magenta;
    lv_color_t purple;
    lv_color_t mint;
    lv_color_t yellow;
    lv_color_t orange;
    lv_color_t peach;
    lv_color_t lavender;
    lv_color_t white;
    lv_color_t fg;
    lv_color_t fg_haze;
    lv_color_t fg_mist;
    lv_color_t fg_on_light;
    lv_color_t board_light;
    lv_color_t board_dark;
    lv_color_t board_edge;
    lv_color_t board_shadow;
} eg_palette_t;

extern eg_palette_t eg_pal;
void eg_theme_init(eg_mode_t mode);       /* before any screen is built */
bool eg_theme_has(eg_mode_t mode);        /* false until theme.h carries that mode */
eg_mode_t eg_theme_mode(void);
lv_color_t eg_on(lv_color_t bg);           /* text color that reads on bg: the dark void, or white */

#undef EG_BG_VOID
#define EG_BG_VOID (eg_pal.bg_void)
#undef EG_BG_NIGHT
#define EG_BG_NIGHT (eg_pal.bg_night)
#undef EG_BG_HAZE
#define EG_BG_HAZE (eg_pal.bg_haze)
#undef EG_BG_MIST
#define EG_BG_MIST (eg_pal.bg_mist)
#undef EG_BG_STAGE
#define EG_BG_STAGE (eg_pal.bg_stage)
#undef EG_SURFACE
#define EG_SURFACE (eg_pal.surface)
#undef EG_CYAN
#define EG_CYAN (eg_pal.cyan)
#undef EG_CYAN_DEEP
#define EG_CYAN_DEEP (eg_pal.cyan_deep)
#undef EG_PINK
#define EG_PINK (eg_pal.pink)
#undef EG_PINK_SOFT
#define EG_PINK_SOFT (eg_pal.pink_soft)
#undef EG_MAGENTA
#define EG_MAGENTA (eg_pal.magenta)
#undef EG_PURPLE
#define EG_PURPLE (eg_pal.purple)
#undef EG_MINT
#define EG_MINT (eg_pal.mint)
#undef EG_YELLOW
#define EG_YELLOW (eg_pal.yellow)
#undef EG_ORANGE
#define EG_ORANGE (eg_pal.orange)
#undef EG_PEACH
#define EG_PEACH (eg_pal.peach)
#undef EG_LAVENDER
#define EG_LAVENDER (eg_pal.lavender)
#undef EG_WHITE
#define EG_WHITE (eg_pal.white)
#undef EG_FG
#define EG_FG (eg_pal.fg)
#undef EG_FG_HAZE
#define EG_FG_HAZE (eg_pal.fg_haze)
#undef EG_FG_MIST
#define EG_FG_MIST (eg_pal.fg_mist)
#undef EG_FG_ON_LIGHT
#define EG_FG_ON_LIGHT (eg_pal.fg_on_light)
#undef EG_BOARD_LIGHT
#define EG_BOARD_LIGHT (eg_pal.board_light)
#undef EG_BOARD_DARK
#define EG_BOARD_DARK (eg_pal.board_dark)
#undef EG_BOARD_EDGE
#define EG_BOARD_EDGE (eg_pal.board_edge)
#undef EG_BOARD_SHADOW
#define EG_BOARD_SHADOW (eg_pal.board_shadow)
