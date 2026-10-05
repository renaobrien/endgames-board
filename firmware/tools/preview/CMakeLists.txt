# SPDX-License-Identifier: MIT
# Desktop preview: renders the real game screen with LVGL into a PNG-able bitmap, no hardware needed.
# Build and run: ./render.sh
cmake_minimum_required(VERSION 3.16)
project(eg_preview C)

include(FetchContent)
FetchContent_Declare(lvgl GIT_REPOSITORY https://github.com/lvgl/lvgl.git GIT_TAG v9.2.2)
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(lvgl)

set(MAIN ${CMAKE_CURRENT_SOURCE_DIR}/../../main)
file(GLOB FONTS ${MAIN}/fonts/*.c)
add_executable(preview
  preview.c pieces_sim.c
  ${MAIN}/ui_game.c ${MAIN}/ui_screens.c ${MAIN}/chess_pos.c ${FONTS})
target_include_directories(preview PRIVATE ${MAIN})
target_compile_definitions(preview PRIVATE LV_CONF_INCLUDE_SIMPLE EG_SET_EDIT=1)
target_link_libraries(preview PRIVATE lvgl)
