# PokeDaisy for Linux handhelds - see linux/PLAN.md.
#
#   cmake -S linux -B linux/build -DCMAKE_BUILD_TYPE=Release && cmake --build linux/build
#
# Same shape as linux/arkos/build.sh: libmgba (the submodule) built as a bare
# core, no frontends, no dependencies, plus our SDL2 host and the portable
# companion core from 3ds/core.
cmake_minimum_required(VERSION 3.13)
project(pokedaisy C)

# mGBA 0.10.5 still declares cmake_minimum_required(VERSION 3.1), which CMake 4
# refuses (the ArkOS container's CMake 3.13 doesn't need this).
if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Release" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

get_filename_component(MGBA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/mgba" ABSOLUTE)
if(NOT EXISTS "${MGBA_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "mGBA submodule missing at ${MGBA_DIR} - run: git submodule update --init third_party/mgba")
endif()

# libmgba only, GBA only for now (the Game Boy core comes with the Game Boy
# games, phase 5 of the plan) - the same switches as the Android and 3DS builds.
set(BUILD_STATIC        ON  CACHE BOOL "" FORCE)
set(BUILD_SHARED        OFF CACHE BOOL "" FORCE)
set(DISABLE_FRONTENDS   ON  CACHE BOOL "" FORCE)
set(DISABLE_DEPS        ON  CACHE BOOL "" FORCE)
set(SKIP_LIBRARY        OFF CACHE BOOL "" FORCE)
set(BUILD_GL            OFF CACHE BOOL "" FORCE)
set(BUILD_GLES2         OFF CACHE BOOL "" FORCE)
set(BUILD_GLES3         OFF CACHE BOOL "" FORCE)
set(BUILD_PERF          OFF CACHE BOOL "" FORCE)
set(BUILD_TEST          OFF CACHE BOOL "" FORCE)
set(BUILD_SUITE         OFF CACHE BOOL "" FORCE)
set(BUILD_LIBRETRO      OFF CACHE BOOL "" FORCE)
set(BUILD_PYTHON        OFF CACHE BOOL "" FORCE)
set(USE_DEBUGGERS       OFF CACHE BOOL "" FORCE)
set(USE_GDB_STUB        OFF CACHE BOOL "" FORCE)
set(USE_EDITLINE        OFF CACHE BOOL "" FORCE)
set(USE_FFMPEG          OFF CACHE BOOL "" FORCE)
set(USE_DISCORD_RPC     OFF CACHE BOOL "" FORCE)
set(USE_LZMA            OFF CACHE BOOL "" FORCE)
set(USE_LUA             OFF CACHE BOOL "" FORCE)
set(USE_ELF             OFF CACHE BOOL "" FORCE)
set(USE_SQLITE3         OFF CACHE BOOL "" FORCE)
set(USE_LIBZIP          OFF CACHE BOOL "" FORCE)
set(USE_MINIZIP         OFF CACHE BOOL "" FORCE)
set(USE_PNG             OFF CACHE BOOL "" FORCE)
set(USE_ZLIB            OFF CACHE BOOL "" FORCE)
set(M_CORE_GBA          ON  CACHE BOOL "" FORCE)
set(M_CORE_GB           OFF CACHE BOOL "" FORCE)
set(BUILD_LTO           OFF CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL    ON  CACHE BOOL "" FORCE)

# RGB565 (what the 3DS build uses): half the bytes to copy per frame. mGBA and
# this host must agree on color_t, so this goes in before the subdirectory.
add_definitions(-DCOLOR_16_BIT -DCOLOR_5_6_5)

add_subdirectory("${MGBA_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/mgba" EXCLUDE_FROM_ALL)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

# The portable companion from 3ds/core (unchanged; the 3DS host shares it).
set(CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_canvas.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_card.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_dex.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_font_gen.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_game.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_guide.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_guide_gen.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_icon.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_item_desc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_map.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_map_gen.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_menu.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_romart.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_settings.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_snapshot.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_style.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_tables.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_tables_gen.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_ui.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_ui_bag.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_ui_battle.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_ui_card.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_ui_dex.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_ui_guide.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_ui_map.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_ui_more.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_ui_party.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_ui_settings.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core/pd_ui_widgets.c")

# pd_png.c is the 3DS preview's own PNG writer, reused for --screenshot.
add_executable(pokedaisy
    main.c render.c audio.c input.c files.c log.c state.c companion.c menu.c
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/desktop/pd_png.c"
    ${CORE_SOURCES})

target_include_directories(pokedaisy PRIVATE
    "${MGBA_DIR}/include"
    "${MGBA_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/mgba/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3ds/desktop")

# mGBA's own sources get USE_PTHREADS from its build (UNIX), and some structs
# change shape with it; ours must see the same headers. glibc before 2.34 keeps
# pthread symbols in libpthread, hence Threads::Threads.
target_compile_definitions(pokedaisy PRIVATE USE_PTHREADS M_CORE_GBA)
# -g so a crash line resolves to a file and line with addr2line; the release
# keeps its symbols for the same reason (no SSH on the handheld).
target_compile_options(pokedaisy PRIVATE -Wall -Wextra -g -fno-omit-frame-pointer)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(pokedaisy PRIVATE mgba PkgConfig::SDL2 Threads::Threads m)
if(UNIX AND NOT APPLE)
    target_link_libraries(pokedaisy PRIVATE dl)
endif()
