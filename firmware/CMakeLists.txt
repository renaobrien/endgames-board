# /* SPDX-License-Identifier: MIT */
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(PROJECT_VER "0.11.9")   # bump for every release; the board updates when the server lists a newer one
project(endgames_board)
