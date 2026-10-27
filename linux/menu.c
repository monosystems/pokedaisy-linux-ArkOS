#include "menu.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <SDL.h>

#include "companion.h"
#include "log.h"
#include "pd_canvas.h"
#include "pd_menu.h"

#define PD_MENU_MAX 256
#define PD_LIST_W PD_COMPANION_W
#define PD_LIST_H PD_COMPANION_H

static bool ends_with_gba(const char* name) {
    size_t n = strlen(name);
    return n > 4 && strcasecmp(name + n - 4, ".gba") == 0;
}

static int cmp_names(const void* a, const void* b) {
    return strcasecmp(*(const char* const*) a, *(const char* const*) b);
}

// Case-insensitive "does haystack contain needle" (no strcasestr everywhere).
static bool contains_fold(const char* haystack, const char* needle) {
    size_t n = strlen(needle);
    if (n == 0) {
        return true;
    }
    for (const char* at = haystack; *at; at++) {
        size_t i = 0;
        while (i < n && at[i] && tolower((unsigned char) at[i]) == tolower((unsigned char) needle[i])) {
            i++;
        }
        if (i == n) {
            return true;
        }
    }
    return false;
}

// The .gba files in dir, sorted; each entry is malloc'd ("name.gba").
static int list_roms(const char* dir, char** out, int max) {
    int n = 0;
    DIR* d = opendir(dir);
    if (!d) {
        return 0;
    }
    struct dirent* e;
    while ((e = readdir(d)) && n < max) {
        if (e->d_name[0] == '.' || !ends_with_gba(e->d_name)) {
            continue;
        }
        out[n] = strdup(e->d_name);
        if (!out[n]) {
            break;
        }
        n++;
    }
    closedir(d);
    qsort(out, (size_t) n, sizeof(out[0]), cmp_names);
    return n;
}

bool pd_pick_rom(const char* dir, const char* pick, const char* shot, char* out, size_t outLen,
                 struct pd_render* render, struct pd_input* input) {
    char* names[PD_MENU_MAX];
    int n = list_roms(dir, names, PD_MENU_MAX);
    pd_logf("menu: %d game(s) in %s", n, dir);

    // The list shows the games without ".gba".
    static char labels[PD_MENU_MAX][128];
    const char* items[PD_MENU_MAX];
    for (int i = 0; i < n; i++) {
        snprintf(labels[i], sizeof(labels[i]), "%s", names[i]);
        size_t len = strlen(labels[i]);
        if (len > 4) {
            labels[i][len - 4] = '\0';
        }
        items[i] = labels[i];
    }
    struct pd_menu menu;
    pd_menu_init(&menu, "CHOOSE A GAME", items, n);

    static uint32_t px[PD_LIST_W * PD_LIST_H];
    struct pd_canvas canvas;
    pd_canvas_init(&canvas, PD_LIST_W, PD_LIST_H, px);

    bool dirty = true;
    bool picked = false;
    while (!pd_input_quit(input)) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            pd_input_event(input, &ev);
        }
        if (pd_input_quit(input)) {
            break;
        }
        pd_input_poll(input);

        if (pd_input_pressed(input, PD_BTN_UP)) {
            pd_menu_move(&menu, -1);
            dirty = true;
        }
        if (pd_input_pressed(input, PD_BTN_DOWN)) {
            pd_menu_move(&menu, 1);
            dirty = true;
        }
        if (n > 0 && pd_input_pressed(input, PD_BTN_A)) {
            picked = true;
            break;
        }
        if (pd_input_pressed(input, PD_BTN_START) || pd_input_pressed(input, PD_BTN_B)) {
            break;
        }

        if (dirty) {
            if (n == 0) {
                // pd_menu's own empty state names the 3DS's folder; say ours.
                pd_splash_draw(&canvas, "NO GAMES FOUND", "Put .gba files in:", dir);
            } else {
                pd_menu_draw(&menu, &canvas);
            }
            dirty = false;
        }
        pd_render_frame(render, NULL, px, PD_LIST_W, PD_LIST_H);
        if (pick) {
            // Tests: choose without a pad, after the list has been drawn once.
            if (shot && pd_render_screenshot(render, shot)) {
                pd_logf("menu: wrote %s", shot);
            }
            for (int i = 0; i < n; i++) {
                if (contains_fold(names[i], pick)) {
                    menu.selected = i;
                    picked = true;
                    break;
                }
            }
            if (!picked) {
                pd_logf("menu: no game matching %s", pick);
            }
            break;
        }
        SDL_Delay(16);
    }

    bool ok = picked && n > 0;
    if (ok) {
        snprintf(out, outLen, "%s/%s", dir, names[menu.selected]);
    }
    for (int i = 0; i < n; i++) {
        free(names[i]);
    }
    return ok;
}
