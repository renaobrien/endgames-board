/* SPDX-License-Identifier: MIT */
#include "theme.h"       /* the generated values; not theme_rt.h, whose macros read eg_pal */
#include <stdbool.h>

#include "lvgl.h"
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

eg_palette_t eg_pal;
static eg_mode_t s_mode;
static lv_color_t s_ink;                   /* dark-mode void: text on bright fills */

static void fill_dark(void)
{
    eg_pal.bg_void = EG_BG_VOID;
    eg_pal.bg_night = EG_BG_NIGHT;
    eg_pal.bg_haze = EG_BG_HAZE;
    eg_pal.bg_mist = EG_BG_MIST;
    eg_pal.bg_stage = EG_BG_STAGE;
    eg_pal.surface = EG_SURFACE;
    eg_pal.cyan = EG_CYAN;
    eg_pal.cyan_deep = EG_CYAN_DEEP;
    eg_pal.pink = EG_PINK;
    eg_pal.pink_soft = EG_PINK_SOFT;
    eg_pal.magenta = EG_MAGENTA;
    eg_pal.purple = EG_PURPLE;
    eg_pal.mint = EG_MINT;
    eg_pal.yellow = EG_YELLOW;
    eg_pal.orange = EG_ORANGE;
    eg_pal.peach = EG_PEACH;
    eg_pal.lavender = EG_LAVENDER;
    eg_pal.white = EG_WHITE;
    eg_pal.fg = EG_FG;
    eg_pal.fg_haze = EG_FG_HAZE;
    eg_pal.fg_mist = EG_FG_MIST;
    eg_pal.fg_on_light = EG_FG_ON_LIGHT;
    eg_pal.board_light = EG_BOARD_LIGHT;
    eg_pal.board_dark = EG_BOARD_DARK;
    eg_pal.board_edge = EG_BOARD_EDGE;
    eg_pal.board_shadow = EG_BOARD_SHADOW;
}

/* A mode is available once the export writes all of its tokens (EG_LIGHT_FG marks light, EG_MONO_FG mono).
 * Any token the export leaves out keeps its dark value. */
static bool fill_light(void)
{
#ifdef EG_LIGHT_FG
    fill_dark();
#ifdef EG_LIGHT_BG_VOID
    eg_pal.bg_void = EG_LIGHT_BG_VOID;
#endif
#ifdef EG_LIGHT_BG_NIGHT
    eg_pal.bg_night = EG_LIGHT_BG_NIGHT;
#endif
#ifdef EG_LIGHT_BG_HAZE
    eg_pal.bg_haze = EG_LIGHT_BG_HAZE;
#endif
#ifdef EG_LIGHT_BG_MIST
    eg_pal.bg_mist = EG_LIGHT_BG_MIST;
#endif
#ifdef EG_LIGHT_BG_STAGE
    eg_pal.bg_stage = EG_LIGHT_BG_STAGE;
#endif
#ifdef EG_LIGHT_SURFACE
    eg_pal.surface = EG_LIGHT_SURFACE;
#endif
#ifdef EG_LIGHT_CYAN
    eg_pal.cyan = EG_LIGHT_CYAN;
#endif
#ifdef EG_LIGHT_CYAN_DEEP
    eg_pal.cyan_deep = EG_LIGHT_CYAN_DEEP;
#endif
#ifdef EG_LIGHT_PINK
    eg_pal.pink = EG_LIGHT_PINK;
#endif
#ifdef EG_LIGHT_PINK_SOFT
    eg_pal.pink_soft = EG_LIGHT_PINK_SOFT;
#endif
#ifdef EG_LIGHT_MAGENTA
    eg_pal.magenta = EG_LIGHT_MAGENTA;
#endif
#ifdef EG_LIGHT_PURPLE
    eg_pal.purple = EG_LIGHT_PURPLE;
#endif
#ifdef EG_LIGHT_MINT
    eg_pal.mint = EG_LIGHT_MINT;
#endif
#ifdef EG_LIGHT_YELLOW
    eg_pal.yellow = EG_LIGHT_YELLOW;
#endif
#ifdef EG_LIGHT_ORANGE
    eg_pal.orange = EG_LIGHT_ORANGE;
#endif
#ifdef EG_LIGHT_PEACH
    eg_pal.peach = EG_LIGHT_PEACH;
#endif
#ifdef EG_LIGHT_LAVENDER
    eg_pal.lavender = EG_LIGHT_LAVENDER;
#endif
#ifdef EG_LIGHT_WHITE
    eg_pal.white = EG_LIGHT_WHITE;
#endif
#ifdef EG_LIGHT_FG
    eg_pal.fg = EG_LIGHT_FG;
#endif
#ifdef EG_LIGHT_FG_HAZE
    eg_pal.fg_haze = EG_LIGHT_FG_HAZE;
#endif
#ifdef EG_LIGHT_FG_MIST
    eg_pal.fg_mist = EG_LIGHT_FG_MIST;
#endif
#ifdef EG_LIGHT_FG_ON_LIGHT
    eg_pal.fg_on_light = EG_LIGHT_FG_ON_LIGHT;
#endif
#ifdef EG_LIGHT_BOARD_LIGHT
    eg_pal.board_light = EG_LIGHT_BOARD_LIGHT;
#endif
#ifdef EG_LIGHT_BOARD_DARK
    eg_pal.board_dark = EG_LIGHT_BOARD_DARK;
#endif
#ifdef EG_LIGHT_BOARD_EDGE
    eg_pal.board_edge = EG_LIGHT_BOARD_EDGE;
#endif
#ifdef EG_LIGHT_BOARD_SHADOW
    eg_pal.board_shadow = EG_LIGHT_BOARD_SHADOW;
#endif
    return true;
#else
    return false;
#endif
}

static bool fill_mono(void)
{
#ifdef EG_MONO_FG
    fill_dark();
#ifdef EG_MONO_BG_VOID
    eg_pal.bg_void = EG_MONO_BG_VOID;
#endif
#ifdef EG_MONO_BG_NIGHT
    eg_pal.bg_night = EG_MONO_BG_NIGHT;
#endif
#ifdef EG_MONO_BG_HAZE
    eg_pal.bg_haze = EG_MONO_BG_HAZE;
#endif
#ifdef EG_MONO_BG_MIST
    eg_pal.bg_mist = EG_MONO_BG_MIST;
#endif
#ifdef EG_MONO_BG_STAGE
    eg_pal.bg_stage = EG_MONO_BG_STAGE;
#endif
#ifdef EG_MONO_SURFACE
    eg_pal.surface = EG_MONO_SURFACE;
#endif
#ifdef EG_MONO_CYAN
    eg_pal.cyan = EG_MONO_CYAN;
#endif
#ifdef EG_MONO_CYAN_DEEP
    eg_pal.cyan_deep = EG_MONO_CYAN_DEEP;
#endif
#ifdef EG_MONO_PINK
    eg_pal.pink = EG_MONO_PINK;
#endif
#ifdef EG_MONO_PINK_SOFT
    eg_pal.pink_soft = EG_MONO_PINK_SOFT;
#endif
#ifdef EG_MONO_MAGENTA
    eg_pal.magenta = EG_MONO_MAGENTA;
#endif
#ifdef EG_MONO_PURPLE
    eg_pal.purple = EG_MONO_PURPLE;
#endif
#ifdef EG_MONO_MINT
    eg_pal.mint = EG_MONO_MINT;
#endif
#ifdef EG_MONO_YELLOW
    eg_pal.yellow = EG_MONO_YELLOW;
#endif
#ifdef EG_MONO_ORANGE
    eg_pal.orange = EG_MONO_ORANGE;
#endif
#ifdef EG_MONO_PEACH
    eg_pal.peach = EG_MONO_PEACH;
#endif
#ifdef EG_MONO_LAVENDER
    eg_pal.lavender = EG_MONO_LAVENDER;
#endif
#ifdef EG_MONO_WHITE
    eg_pal.white = EG_MONO_WHITE;
#endif
#ifdef EG_MONO_FG
    eg_pal.fg = EG_MONO_FG;
#endif
#ifdef EG_MONO_FG_HAZE
    eg_pal.fg_haze = EG_MONO_FG_HAZE;
#endif
#ifdef EG_MONO_FG_MIST
    eg_pal.fg_mist = EG_MONO_FG_MIST;
#endif
#ifdef EG_MONO_FG_ON_LIGHT
    eg_pal.fg_on_light = EG_MONO_FG_ON_LIGHT;
#endif
#ifdef EG_MONO_BOARD_LIGHT
    eg_pal.board_light = EG_MONO_BOARD_LIGHT;
#endif
#ifdef EG_MONO_BOARD_DARK
    eg_pal.board_dark = EG_MONO_BOARD_DARK;
#endif
#ifdef EG_MONO_BOARD_EDGE
    eg_pal.board_edge = EG_MONO_BOARD_EDGE;
#endif
#ifdef EG_MONO_BOARD_SHADOW
    eg_pal.board_shadow = EG_MONO_BOARD_SHADOW;
#endif
    return true;
#else
    return false;
#endif
}

bool eg_theme_has(eg_mode_t mode)
{
#ifdef EG_LIGHT_FG
    if (mode == EG_MODE_LIGHT) return true;
#endif
#ifdef EG_MONO_FG
    if (mode == EG_MODE_MONO) return true;
#endif
    return mode == EG_MODE_DARK;
}

void eg_theme_init(eg_mode_t mode)
{
    fill_dark();
    s_ink = eg_pal.bg_void;
    s_mode = EG_MODE_DARK;
    if (mode == EG_MODE_LIGHT && fill_light()) s_mode = mode;
    else if (mode == EG_MODE_MONO && fill_mono()) s_mode = mode;
}

eg_mode_t eg_theme_mode(void) { return s_mode; }

lv_color_t eg_on(lv_color_t bg)
{
    uint32_t y = (uint32_t)bg.red * 299 + (uint32_t)bg.green * 587 + (uint32_t)bg.blue * 114;   /* 0..255000 */
    return y > 140000 ? s_ink : lv_color_white();
}
