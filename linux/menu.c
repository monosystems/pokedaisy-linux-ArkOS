#include "menu.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include <SDL.h>

#include "companion.h"
#include "files.h"
#include "log.h"
#include "pd_canvas.h"
#include "pd_menu.h"

#define PD_MENU_MAX 256
#define PD_LIST_W PD_COMPANION_W
#define PD_LIST_H PD_COMPANION_H

static void draw_info(struct pd_canvas* c, const char* dir, const char* name, const char* selectedBackup, const char* message, bool playing) {
    char rom[1024], save[1024], state[1024], resume[1024], backup[2200], line[160], when[40];
    struct stat st;
    int len = snprintf(rom, sizeof(rom), "%s/%s", dir, name);
    pd_fill(c, 0, 0, c->w, c->h, 0x203048);
    pd_text(c, 12, 10, "GAME INFO", 0xffffff, PD_NO_SHADOW);
    pd_text_fit(c, 12, 32, c->w - 24, name, 0xffffff, PD_NO_SHADOW);
    if (len < 0 || (size_t) len >= sizeof(rom)) {
        pd_text(c, 12, 60, "PATH TOO LONG", 0xffffff, PD_NO_SHADOW);
        return;
    }
    if (stat(rom, &st) == 0) snprintf(line, sizeof(line), "ROM: %lld BYTES", (long long) st.st_size);
    else snprintf(line, sizeof(line), "ROM: NOT FOUND");
    pd_text(c, 12, 60, line, 0xffffff, PD_NO_SHADOW);
    bool hasSave = pd_save_path(rom, save, sizeof(save)) && stat(save, &st) == 0 && st.st_size > 0;
    if (hasSave) snprintf(line, sizeof(line), "SAVE: %lld BYTES", (long long) st.st_size);
    else snprintf(line, sizeof(line), "SAVE: NONE");
    pd_text(c, 12, 82, line, 0xffffff, PD_NO_SHADOW);
    bool hasState = pd_state_path(rom, state, sizeof(state)) && pd_state_info(state, when, sizeof(when));
    snprintf(line, sizeof(line), "STATE: %s", hasState ? when : "NONE");
    pd_text(c, 12, 104, line, 0xffffff, PD_NO_SHADOW);
    bool hasResume = pd_resume_path(rom, resume, sizeof(resume)) && pd_state_info(resume, when, sizeof(when));
    snprintf(line, sizeof(line), "RESUME: %s", hasResume ? when : "NONE");
    pd_text(c, 12, 126, line, 0xffffff, PD_NO_SHADOW);
    bool hasBackup = pd_save_path(rom, save, sizeof(save)) && pd_latest_backup(save, backup, sizeof(backup));
    pd_text(c, 12, 150, hasBackup ? "BACKUP: AVAILABLE" : "BACKUP: NONE", 0xffffff, PD_NO_SHADOW);
    if (selectedBackup) {
        const char* stamp = strstr(selectedBackup, ".backup-");
        pd_text_fit(c, 12, 168, c->w - 24, stamp ? stamp + 8 : selectedBackup, 0xffffff, PD_NO_SHADOW);
        pd_text(c, 12, 186, "RESTORE THIS BACKUP?", 0xffffff, PD_NO_SHADOW);
        pd_text(c, 12, 210, "A: RESTORE   B: CANCEL", 0xffffff, PD_NO_SHADOW);
    } else {
        if (playing) pd_text(c, 12, 174, "PLAYING: RESTORE DISABLED", 0xffffff, PD_NO_SHADOW);
        else if (message) pd_text(c, 12, 174, message, 0xffffff, PD_NO_SHADOW);
        pd_text(c, 12, 210, hasBackup && !playing ? "A: RESTORE   B: BACK" : "B: BACK", 0xffffff, PD_NO_SHADOW);
    }
}

static void draw_backups(struct pd_canvas* c, char paths[][PD_BACKUP_PATH], int count, int selected) {
    pd_fill(c, 0, 0, c->w, c->h, 0x203048);
    pd_text(c, 12, 10, "RESTORE BACKUP", 0xffffff, PD_NO_SHADOW);
    for (int i = 0; i < count; i++) {
        const char* name = strrchr(paths[i], '/');
        name = name ? name + 1 : paths[i];
        const char* stamp = strstr(name, ".backup-");
        if (stamp) name = stamp + 8;
        int y = 32 + i * 16;
        if (i == selected) pd_fill(c, 6, y - 1, c->w - 12, 15, 0x506888);
        pd_text_fit(c, 12, y, c->w - 24, name, 0xffffff, PD_NO_SHADOW);
    }
    pd_text(c, 12, 210, "A: SELECT   B: BACK", 0xffffff, PD_NO_SHADOW);
}

static bool ends_with_gba(const char* name) {
    size_t n = strlen(name);
    return n > 4 && strcasecmp(name + n - 4, ".gba") == 0;
}

// ROMs with the same stem can share a save, including on case-insensitive
// cards. Do not replace a save while the emulator has it open.
static bool same_save(const char* dir, const char* name, const char* playingName) {
    char a[1024], b[1024];
    if (!playingName || !pd_path_stem(name, a, sizeof(a)) || !pd_path_stem(playingName, b, sizeof(b))) return false;
    if (strcasecmp(a, b) == 0) return true;
    char romA[1024], romB[1024], saveA[1024], saveB[1024];
    int na = snprintf(romA, sizeof(romA), "%s/%s", dir, name);
    int nb = snprintf(romB, sizeof(romB), "%s/%s", dir, playingName);
    struct stat sa, sb;
    return na > 0 && (size_t) na < sizeof(romA) && nb > 0 && (size_t) nb < sizeof(romB) &&
           pd_save_path(romA, saveA, sizeof(saveA)) && pd_save_path(romB, saveB, sizeof(saveB)) &&
           stat(saveA, &sa) == 0 && stat(saveB, &sb) == 0 && sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
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
                 struct pd_render* render, struct pd_input* input, const char* playing) {
    char* names[PD_MENU_MAX];
    int n = list_roms(dir, names, PD_MENU_MAX);
    pd_logf("menu: %d game(s) in %s", n, dir);

    // The list shows the games without ".gba".
    static char labels[PD_MENU_MAX][128];
    const char* items[PD_MENU_MAX];
    const char* playingName = playing ? strrchr(playing, '/') : NULL;
    if (playing) playingName = playingName ? playingName + 1 : playing;
    const char* preview = pick && shot ? getenv("PD_TEST_MENU_VIEW") : NULL;
    if (!playingName && preview && !strcmp(preview, "playing")) {
        for (int i = 0; i < n; i++) if (contains_fold(names[i], pick)) { playingName = names[i]; break; }
    }
    for (int i = 0; i < n; i++) {
        char rom[1024], save[1024];
        struct stat st;
        int pathLen = snprintf(rom, sizeof(rom), "%s/%s", dir, names[i]);
        bool saved = pathLen > 0 && (size_t) pathLen < sizeof(rom) && pd_save_path(rom, save, sizeof(save)) &&
                     stat(save, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
        char title[100];
        snprintf(title, sizeof(title), "%s", names[i]);
        size_t len = strlen(title);
        if (len > 4 && ends_with_gba(title)) title[len - 4] = '\0';
        snprintf(labels[i], sizeof(labels[i]), "%s%s%s", playingName && !strcmp(names[i], playingName) ? "[PLAYING] " : "",
                 saved ? "[SAVE] " : "", title);
        items[i] = labels[i];
    }
    struct pd_menu menu;
    pd_menu_init(&menu, "CHOOSE A GAME", items, n);
    if (playingName) for (int i = 0; i < n; i++) if (!strcmp(names[i], playingName)) menu.selected = i;
    if (playing) menu.title = "LIBRARY: B TO RESUME";

    static uint32_t px[PD_LIST_W * PD_LIST_H];
    struct pd_canvas canvas;
    pd_canvas_init(&canvas, PD_LIST_W, PD_LIST_H, px);

    bool dirty = true;
    bool picked = false;
    bool info = false;
    bool confirm = false;
    bool backupsOpen = false;
    char backups[PD_BACKUPS][PD_BACKUP_PATH];
    int backupCount = 0, backupSelected = 0;
    const char* message = NULL;
    // Headless screenshots of the library's inner screens, only alongside the
    // existing --pick/--menu-shot test path. Normal input never uses this.
    const char* testView = pick && shot ? getenv("PD_TEST_MENU_VIEW") : NULL;
    if (testView) {
        for (int i = 0; i < n; i++) if (contains_fold(names[i], pick)) { menu.selected = i; break; }
        if (n > 0 && (!strcmp(testView, "info") || !strcmp(testView, "backups") || !strcmp(testView, "confirm"))) {
            info = true;
            if (strcmp(testView, "info")) {
                char rom[1024], save[1024];
                int len = snprintf(rom, sizeof(rom), "%s/%s", dir, names[menu.selected]);
                if (len > 0 && (size_t) len < sizeof(rom) && pd_save_path(rom, save, sizeof(save)))
                    backupCount = pd_list_backups(save, backups, PD_BACKUPS);
                backupsOpen = backupCount > 0;
                confirm = backupsOpen && !strcmp(testView, "confirm");
            }
        }
    }
    while (!pd_input_quit(input)) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            pd_input_event(input, &ev);
        }
        if (pd_input_quit(input)) {
            break;
        }
        pd_input_poll(input);

        bool wasInfo = info;
        if (info && (pd_input_pressed(input, PD_BTN_B) || pd_input_pressed(input, PD_BTN_SELECT))) {
            if (confirm) confirm = false;
            else if (backupsOpen) backupsOpen = false;
            else info = false;
            message = NULL;
            dirty = true;
        } else if (!info && n > 0 && pd_input_pressed(input, PD_BTN_SELECT)) {
            info = true;
            message = NULL;
            dirty = true;
        } else if (info && backupsOpen && !confirm && pd_input_pressed(input, PD_BTN_UP)) {
            if (backupSelected > 0) backupSelected--;
            dirty = true;
        } else if (info && backupsOpen && !confirm && pd_input_pressed(input, PD_BTN_DOWN)) {
            if (backupSelected + 1 < backupCount) backupSelected++;
            dirty = true;
        } else if (info && pd_input_pressed(input, PD_BTN_A) && !same_save(dir, names[menu.selected], playingName)) {
            char rom[1024], save[1024], backup[2200];
            int len = snprintf(rom, sizeof(rom), "%s/%s", dir, names[menu.selected]);
            if (len > 0 && (size_t) len < sizeof(rom) && pd_save_path(rom, save, sizeof(save)) &&
                pd_latest_backup(save, backup, sizeof(backup))) {
                if (confirm) {
                    message = pd_restore_backup_at(rom, backups[backupSelected]) ? "BACKUP RESTORED" : "RESTORE FAILED";
                    confirm = false;
                    backupsOpen = false;
                } else if (backupsOpen) {
                    confirm = true;
                } else {
                    backupCount = pd_list_backups(save, backups, PD_BACKUPS);
                    backupSelected = 0;
                    backupsOpen = backupCount > 0;
                }
                dirty = true;
            }
        }
        if (!info && pd_input_pressed(input, PD_BTN_UP)) {
            pd_menu_move(&menu, -1);
            dirty = true;
        }
        if (!info && pd_input_pressed(input, PD_BTN_DOWN)) {
            pd_menu_move(&menu, 1);
            dirty = true;
        }
        if (!info && !playing && n > 0 && pd_input_pressed(input, PD_BTN_A)) {
            picked = true;
            break;
        }
        if (!info && !wasInfo && (pd_input_pressed(input, PD_BTN_START) || pd_input_pressed(input, PD_BTN_B))) {
            break;
        }

        if (dirty) {
            if (info && backupsOpen && !confirm) {
                draw_backups(&canvas, backups, backupCount, backupSelected);
            } else if (info) {
                draw_info(&canvas, dir, names[menu.selected], confirm ? backups[backupSelected] : NULL, message,
                          same_save(dir, names[menu.selected], playingName));
            } else if (n == 0) {
                // pd_menu's own empty state names another folder; say ours.
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
