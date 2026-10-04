/* SPDX-License-Identifier: MIT */
/* Minimal LVGL 9 config for the desktop preview. The board's own config lives in sdkconfig.defaults. */
#if 1
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 32
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#define LV_MEM_SIZE (4 * 1024 * 1024)
#define LV_DEF_REFR_PERIOD 16
#define LV_DPI_DEF 130

#define LV_USE_OS LV_OS_NONE
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_COMPLEX 1
#define LV_USE_LOG 0

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_USE_QRCODE 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_LETTER 'A'
#define LV_FS_STDIO_PATH ""
#define LV_FS_STDIO_CACHE_SIZE 0

#define LV_USE_LODEPNG 1
#define LV_USE_SNAPSHOT 1
#define LV_USE_PERF_MONITOR 0

#endif
#endif
