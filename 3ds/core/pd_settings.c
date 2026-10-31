#include "pd_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void pd_settings_defaults(struct pd_settings* s) {
    s->screenMode = PD_SCREEN_SHARP;
    s->ffSpeed = 200; // 2x
    s->gameSpeed = 100;
    s->slowMotion = true;
    s->statusBar = false;
    s->guideNotice = 0;
    s->tabsShown = PD_TABS_DEFAULT;
    s->tweaks = PD_TWEAKS_DEFAULT;
    memset(s->favs, 0, sizeof(s->favs));
    memset(s->legacyFavs, 0, sizeof(s->legacyFavs));
}

static bool int_in(const char* v, int lo, int hi, int* out) {
    char* end;
    long n = strtol(v, &end, 10);
    if (end == v || n < lo || n > hi) return false;
    *out = (int) n;
    return true;
}

// A favorites line (C-B-06): "<rom id>:<item>,<item>,...". Anything that
// doesn't parse is dropped, so a hand-edited file can't break the bag.
static void read_favs(struct pd_favs* f, const char* value) {
    memset(f, 0, sizeof(*f));
    char* end;
    unsigned long rom = strtoul(value, &end, 10);
    if (end == value || *end != ':') return;
    f->rom = (unsigned) rom;
    const char* p = end + 1;
    while (*p && f->count < PD_FAV_MAX) {
        long id = strtol(p, &end, 10);
        if (end == p) break;
        if (id > 0 && id < 0xFFFF) f->items[f->count++] = (unsigned short) id;
        p = *end == ',' ? end + 1 : end;
    }
}

bool pd_settings_load(struct pd_settings* s, const char* path) {
    pd_settings_defaults(s);
    FILE* f = fopen(path, "r");
    if (!f) return false;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char* key = line;
        const char* value = eq + 1;
        int v;
        if (!strcmp(key, "screen")) int_in(value, 0, PD_SCREEN_MODES - 1, &s->screenMode);
        else if (!strcmp(key, "ff") && int_in(value, 0, 1000, &v)) s->ffSpeed = v == 0 || v >= 150 ? v : 200;
        else if (!strcmp(key, "speed") && int_in(value, PD_GAME_SPEED_STEPS[0], PD_GAME_SPEED_STEPS[PD_GAME_SPEEDS - 1], &s->gameSpeed)) {}
        else if (!strcmp(key, "slow") && int_in(value, 0, 1, &v)) s->slowMotion = v != 0;
        else if (!strcmp(key, "status_bar") && int_in(value, 0, 1, &v)) s->statusBar = v != 0;
        else if (!strcmp(key, "guide")) int_in(value, 0, 0xFFFF, &s->guideNotice);
        // The TAB BAR page's pick and the TWEAKS switches: a bit each, so an
        // unknown bit (a newer build's tab) is dropped instead of read.
        else if (!strcmp(key, "tabs") && int_in(value, 0, 0xFF, &v)) s->tabsShown = (unsigned) v;
        else if (!strcmp(key, "tweaks") && int_in(value, 0, 0xFF, &v)) s->tweaks = (unsigned) v;
        // Legacy IDs were process-local; retain them without guessing a ROM.
        else if (key[0] == 'f' && key[1] == 'a' && key[2] == 'v' && key[3] >= '1' && key[3] <= '4' && !key[4]) {
            read_favs(&s->legacyFavs[key[3] - '1'], value);
        } else if (!strncmp(key, "fav2_", 5) && key[5] >= '1' && key[5] <= '4' && !key[6]) {
            read_favs(&s->favs[key[5] - '1'], value);
        }
    }
    fclose(f);
    return true;
}

bool pd_settings_save(const struct pd_settings* s, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return false;
    fprintf(f, "screen=%d\nff=%d\nguide=%d\ntabs=%u\ntweaks=%u\n", s->screenMode, s->ffSpeed, s->guideNotice,
            s->tabsShown, s->tweaks);
    fprintf(f, "speed=%d\nslow=%d\n", s->gameSpeed, s->slowMotion ? 1 : 0);
    fprintf(f, "status_bar=%d\n", s->statusBar ? 1 : 0);
    for (int i = 0; i < PD_FAV_ROMS; i++) {
        for (int legacy = 0; legacy < 2; legacy++) {
            const struct pd_favs* v = legacy ? &s->legacyFavs[i] : &s->favs[i];
            if (!v->rom || v->count <= 0) continue;
            fprintf(f, legacy ? "fav%d=%u:" : "fav2_%d=%u:", i + 1, v->rom);
            for (int k = 0; k < v->count; k++) fprintf(f, "%s%u", k ? "," : "", v->items[k]);
            fputc('\n', f);
        }
    }
    return fclose(f) == 0;
}

// The app's fast-forward steps (1.5x to 10x, then INFINITE) and the GAME SPEED
// steps it offers, both in percent of normal speed.
const int PD_FF_SPEEDS[PD_FF_STEPS] = { 150, 200, 300, 400, 500, 600, 800, 1000, 0 };
const int PD_GAME_SPEED_STEPS[PD_GAME_SPEEDS] = { 25, 50, 75, 100, 150, 200, 300, 400 };
