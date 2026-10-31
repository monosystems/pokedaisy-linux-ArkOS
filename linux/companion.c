#include "companion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL.h>

#include "files.h"
#include "log.h"
#include "pd_canvas.h"
#include "pd_dex.h"
#include "pd_map.h"
#include "pd_romart.h"
#include "pd_settings.h"
#include "pd_ui_internal.h"

// How often the game is read, in emulated frames.
#define PD_SAMPLE_EVERY 10
// One press on a list's edge scrolls this far, in canvas pixels.
#define PD_SCROLL_STEP 64

#define PD_NOTICE_BG 0x101820u
#define PD_NOTICE_TITLE 0xF8F8F8u
#define PD_NOTICE_TEXT 0xC8D0E0u
#define PD_NOTICE_DIM 0x8FA0B0u
#define PD_FOCUS_COLOR 0xFFFFFFu

// "2x" / "100%" / "INFINITE" for a speed in percent (0 = INFINITE).
static void speed_text(int pct, char* buf, size_t len) {
    if (pct <= 0) snprintf(buf, len, "INFINITE");
    else if (pct % 100 == 0) snprintf(buf, len, "%dx", pct / 100);
    else if (pct % 10 == 0) snprintf(buf, len, "%d.%dx", pct / 100, pct % 100 / 10);
    else snprintf(buf, len, "%d.%02dx", pct / 100, pct % 100);
}

struct pd_companion {
    struct pd_game game;
    struct pd_snapshot snap, shown;
    struct pd_ui ui;
    struct pd_settings settings;
    struct pd_host_info host;
    char settingsPath[1024];
    char title[64];
    char statusLabel[112]; // the game name and place last drawn in the status bar
    uint32_t px[PD_COMPANION_W * PD_COMPANION_H];
    uint32_t statusPx[PD_STATUS_W * PD_STATUS_H];
    Uint32 statusAt;
    bool statusDrawn;
    bool visible;
    bool dirty;
    bool cursorSet; // fx/fy point at something the last draw made
    int fx, fy;     // the cursor, in canvas pixels
    bool sampled;
    unsigned lastSample;
};

static void set_host(struct pd_companion* c) {
    c->host.count = 2;
    c->host.hasStatusBar = true;
    c->host.hasLibrary = true;
    c->host.labels[0] = "GAME";
    // A 40-byte SETTINGS value against the file name's 64: cut here, on purpose,
    // instead of letting snprintf do it behind the compiler's back (-Wformat-
    // truncation says the name can be longer than the row).
    snprintf(c->host.values[0], sizeof(c->host.values[0]), "%.*s", (int) sizeof(c->host.values[0]) - 1,
             c->title);
    c->host.labels[1] = "SPEED";
    speed_text(c->settings.gameSpeed, c->host.values[1], sizeof(c->host.values[1]));
}

struct pd_companion* pd_companion_open(const char* romPath, const char* settingsPath, const uint8_t* rom,
                                       size_t romSize) {
    struct pd_companion* c = calloc(1, sizeof(*c));
    if (!c) {
        return NULL;
    }
    if (settingsPath) {
        snprintf(c->settingsPath, sizeof(c->settingsPath), "%s", settingsPath);
    }
    pd_settings_load(&c->settings, c->settingsPath); // defaults when the file is missing

    char stem[1024];
    if (pd_path_stem(romPath, stem, sizeof(stem))) {
        const char* slash = strrchr(stem, '/');
        snprintf(c->title, sizeof(c->title), "%s", slash ? slash + 1 : stem);
    } else {
        snprintf(c->title, sizeof(c->title), "GAME");
    }

    pd_game_detect(&c->game, rom, romSize);
    c->game.rom = rom;
    c->game.romSize = romSize;
    if (c->game.cfg) {
        // The game's own art (region map, trainer card), once, while the game
        // loads.
        uint32_t started = SDL_GetTicks();
        pd_romart_scan(&c->game);
        pd_map_art(&c->game);
        // "RAM only" games (FireRed rev 0, LeafGreen) have no POKéDEX or GUIDE
        // tables: the tabs that need them say so instead of reading garbage.
        bool tables = c->game.cfg->dexEntries || c->game.cfg->trainers;
        pd_logf("companion: %s (%s) rev %d, %s, art in %u ms", c->game.title, c->game.code, c->game.revision,
                tables ? "ROM tables" : "RAM only", (unsigned) (SDL_GetTicks() - started));
        // The POKéDEX's EVOLVE section has no chip without the table: say which
        // it is, so a missing chip on a supported game is easy to report.
        if (tables && c->game.cfg->dexEntries) {
            pd_logf("companion: evolution table %s", pd_dex_has_evolutions(&c->game) ? "found" : "not found");
        }
    } else {
        pd_logf("companion: %s (%s) rev %d, no data: %s", c->game.title, c->game.code, c->game.revision,
                c->game.unsupported ? c->game.unsupported : "not supported");
    }

    pd_ui_init(&c->ui, &c->settings);
    set_host(c);
    c->dirty = true;
    return c;
}

void pd_companion_close(struct pd_companion* c) {
    free(c);
}

bool pd_companion_supported(const struct pd_companion* c) {
    return c && c->game.cfg != NULL;
}

const char* pd_companion_title(const struct pd_companion* c) {
    return c ? c->title : "";
}

// ASCII uppercase for the app's gameCaps: its own "POKéMON" keeps the é,
// which is a multi-byte character here and must pass through untouched.
static void upper_copy(char* out, size_t len, const char* in) {
    size_t n = 0;
    for (; in && *in && n + 1 < len; in++) {
        unsigned char ch = (unsigned char) *in;
        out[n++] = ch >= 'a' && ch <= 'z' ? (char) (ch - 'a' + 'A') : (char) ch;
    }
    if (len) out[n] = 0;
}

// The app's BatteryIndicator: a 15x9 body with a 2x3 nub, its 11 inner columns
// filled by the charge. ink draws the outline, fillInk what is left of it.
static void battery_icon(struct pd_canvas* c, int x, int y, int percent, uint32_t ink, uint32_t fillInk) {
    pd_fill(c, x, y, 15, 1, ink);
    pd_fill(c, x, y + 8, 15, 1, ink);
    pd_fill(c, x, y, 1, 9, ink);
    pd_fill(c, x + 14, y, 1, 9, ink);
    pd_fill(c, x + 15, y + 3, 2, 3, ink);
    int filled = percent <= 0 ? 0 : (percent * 11 + 50) / 100;
    if (percent > 0 && filled < 1) filled = 1;
    if (filled > 11) filled = 11;
    if (filled) pd_fill(c, x + 2, y + 2, filled, 5, fillInk);
}

static bool starts_with(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

// A short name for the bar. The detected game wins ("FireRed"), because the file
// name is a download's business: "Pokemon - Fire Red Version (U) (V1.1).gba"
// would fill the whole row. Others get the stem without its tags.
static void game_label(const struct pd_companion* c, char* out, size_t len) {
    if (c->game.cfg || c->game.code[0]) {
        if (!strcmp(c->game.code, "BPRE")) { snprintf(out, len, "FireRed"); return; }
        if (!strcmp(c->game.code, "BPGE")) { snprintf(out, len, "LeafGreen"); return; }
        if (!strcmp(c->game.code, "BPEE")) { snprintf(out, len, "Emerald"); return; }
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", c->title);
    // Cut the tags: " (U)", " [b]", and the word Version.
    char* cut = strpbrk(buf, "([");
    if (cut) {
        while (cut > buf && (cut[-1] == ' ' || cut[-1] == '-')) cut--;
        *cut = 0;
    }
    const char* s = buf;
    if (starts_with(s, "Pokemon - ")) s += 10;
    else if (starts_with(s, "Pokemon ")) s += 8;
    size_t n = strlen(s);
    if (n > 8 && !strcmp(s + n - 8, " Version")) ((char*) s)[n - 8] = 0;
    snprintf(out, len, "%s", s);
    n = strlen(out);
    while (n && out[n - 1] == ' ') out[--n] = 0;
}

const uint32_t* pd_companion_status(struct pd_companion* c) {
    if (!c || !c->settings.statusBar) return NULL;
    Uint32 now = SDL_GetTicks();
    if (c->statusDrawn && now - c->statusAt < 1000) return c->statusPx;
    c->statusDrawn = true;
    c->statusAt = now;
    struct pd_canvas cv;
    pd_canvas_init(&cv, PD_STATUS_W, PD_STATUS_H, c->statusPx);
    // The app's GameStatusBar: one row in the OPTION screen's title window.
    // The frame's own colour behind the rounded corners, so the bar reads as one
    // rectangle over the game instead of carrying black corner pixels.
    pd_fill(&cv, 0, 0, PD_STATUS_W, PD_STATUS_H, 0x63737B);
    int f = pd_title_box(&cv, 0, 0, PD_STATUS_W, PD_STATUS_H, PD_TITLE_FILL);
    int ty = pd_text_y(0, PD_STATUS_H);
    int left = f + 5, right = PD_STATUS_W - f - 5;

    // Right side first, so the game name can give way to it.
    char clock[24], battery[32], moneyText[24];
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    if (!tm || !strftime(clock, sizeof(clock), "%H:%M", tm)) snprintf(clock, sizeof(clock), "--:--");
    int seconds = -1, percent = -1;
    SDL_PowerState power = SDL_GetPowerInfo(&seconds, &percent);
    bool haveMoney = c->shown.valid && c->shown.money >= 0;
    if (haveMoney) snprintf(moneyText, sizeof(moneyText), "%ld", c->shown.money);
    int clockW = pd_text_width(clock);
    int moneyW = 7 + 1 + (haveMoney ? pd_text_width(moneyText) : 0);
    int batteryW = 0;
    if (percent >= 0) {
        snprintf(battery, sizeof(battery), "%d%%", percent);
        batteryW = 18 + 3 + pd_text_width(battery);
    }
    int rx = right;
    if (batteryW) {
        rx -= batteryW;
        int bx = rx;
        uint32_t ink = PD_TITLE_TEXT, shade = PD_TITLE_SHADOW;
        if (power == SDL_POWERSTATE_CHARGING) ink = 0x2C9A3C;
        else if (percent <= 15) ink = PD_VALUE;
        // Shadow one pixel right, down and diagonal, like the text's.
        battery_icon(&cv, bx + 1, ty + 1, percent, shade, shade);
        battery_icon(&cv, bx, ty, percent, ink, ink);
        pd_text(&cv, bx + 18 + 3, ty, battery, PD_TITLE_TEXT, PD_TITLE_SHADOW);
        rx -= 8;
    }
    rx -= clockW;
    pd_text(&cv, rx, ty, clock, PD_TITLE_TEXT, PD_TITLE_SHADOW);
    rx -= 8;
    if (haveMoney) {
        rx -= moneyW;
        // The Pokédollar sign: a P whose stem two bars cross.
        static const uint16_t DOLLAR[9] = { 0x1E, 0x21, 0x21, 0x1E, 0x10, 0x0F, 0x10, 0x0F, 0x10 };
        pd_bitmap(&cv, rx + 1, ty + 1, DOLLAR, 9, PD_TITLE_SHADOW);
        pd_bitmap(&cv, rx, ty, DOLLAR, 9, PD_TITLE_TEXT);
        pd_text(&cv, rx + 8, ty, moneyText, PD_TITLE_TEXT, PD_TITLE_SHADOW);
        rx -= 8;
    }

    // Left: the place first (it is what changes), the game's short name while
    // there is room for it. A long name never squeezes the place out.
    char label[48], name[48];
    game_label(c, label, sizeof(label));
    upper_copy(name, sizeof(name), label);
    const char* place = c->shown.valid ? pd_mapsec_name(&c->game, c->shown.mapsec) : "";
    char placeText[64] = { 0 };
    if (place && *place) upper_copy(placeText, sizeof(placeText), place);
    int available = rx - 8 - left;
    int placeW = *placeText ? 9 + 2 + pd_text_width(placeText) : 0;
    int nameBudget = available - (placeW ? placeW + 10 : 0);
    int nw = 0;
    if (nameBudget >= 34) nw = pd_text_fit(&cv, left, ty, nameBudget, name, PD_TITLE_TEXT, PD_TITLE_SHADOW);
    if (*placeText) {
        static const uint16_t PIN[9] = { 0x3E, 0x7F, 0x77, 0x63, 0x77, 0x3E, 0x1C, 0x1C, 0x08 };
        int px = left + (nw ? nw + 10 : 0);
        pd_bitmap(&cv, px + 1, ty + 1, PIN, 9, PD_VALUE_SHADOW);
        pd_bitmap(&cv, px, ty, PIN, 9, PD_VALUE);
        pd_text_fit(&cv, px + 11, ty, rx - 8 - (px + 11), placeText, PD_LABEL, PD_LABEL_SHADOW);
    }
    char key[112];
    snprintf(key, sizeof(key), "%s|%s", name, *placeText ? placeText : "");
    if (strcmp(c->statusLabel, key)) {
        snprintf(c->statusLabel, sizeof(c->statusLabel), "%s", key);
        pd_logf("status: game %s, place %s", name, *placeText ? placeText : "-");
    }
    return c->statusPx;
}

void pd_companion_show(struct pd_companion* c, bool visible) {
    if (!c || c->visible == visible) {
        return;
    }
    c->visible = visible;
    c->cursorSet = false;
    c->dirty = true;
}

void pd_companion_toggle(struct pd_companion* c) {
    if (c) {
        pd_companion_show(c, !c->visible);
    }
}

bool pd_companion_visible(const struct pd_companion* c) {
    return c && c->visible;
}

enum pd_action pd_companion_tick(struct pd_companion* c, pd_read_fn read, void* ctx, unsigned frame,
                                 unsigned elapsedMs, bool ffOn) {
    if (!c) {
        return PD_ACTION_NONE;
    }
    c->host.ffOn = ffOn;
    char sp[20];
    speed_text(ffOn ? c->settings.ffSpeed : c->settings.gameSpeed, sp, sizeof(sp));
    snprintf(c->host.values[1], sizeof(c->host.values[1]), "%s%s", ffOn ? "FF " : "", sp);

    if (c->game.cfg && read && (!c->sampled || frame / PD_SAMPLE_EVERY != c->lastSample / PD_SAMPLE_EVERY)) {
        c->sampled = true;
        c->lastSample = frame;
        struct pd_snapshot s;
        pd_snapshot_read(&s, &c->game, read, ctx);
        if (getenv("PD_DEBUG_RAM")) {
            // What the game's memory actually holds (host debugging only).
            uint8_t count = 0xFF;
            uint8_t mon[4] = { 0 };
            read(ctx, c->game.cfg->playerPartyCount, &count, 1);
            read(ctx, c->game.cfg->playerParty, mon, 4);
            pd_logf("debug: partyCount byte %u, first mon %02x %02x %02x %02x", count, mon[0], mon[1], mon[2],
                    mon[3]);
        }
        s.frame = 0; // the counter changes every frame and nothing shows it
        pd_ui_update(&c->ui, &s);
        if (memcmp(&s, &c->shown, sizeof(s)) != 0) {
            c->shown = s;
            c->dirty = true;
        }
    }
    // The blink and flip animations only matter on screen.
    if (c->visible && pd_ui_tick(&c->ui, elapsedMs)) {
        c->dirty = true;
    }
    return pd_ui_take_action(&c->ui);
}

// A dialog adds a full-screen scrim before its own buttons, so the hits after
// it are the ones the player can reach: the screen behind can't be tapped
// through it, and the cursor must not wander back there either.
static int hits_start(const struct pd_companion* c) {
    if (c->ui.overlay == PD_OVERLAY_NONE) {
        return 0;
    }
    for (int i = 0; i < c->ui.hitCount; i++) {
        const struct pd_hit* h = &c->ui.hits[i];
        if (h->x <= 0 && h->y <= 0 && h->w >= PD_COMPANION_W && h->h >= PD_COMPANION_H) {
            return i + 1;
        }
    }
    return 0;
}

// The hit the cursor is on: the smallest one under (fx, fy) - a list row beats
// the list box around it, so the ring and the next press work from the row -
// else the nearest, else the first the screen drew.
static int focus_index(struct pd_companion* c) {
    int start = hits_start(c);
    if (start >= c->ui.hitCount) {
        return -1;
    }
    if (!c->cursorSet) {
        return start;
    }
    int under = -1;
    long underArea = 0;
    int tiny = -1; // a clipped row under the cursor is a last resort: no ring fits
    int best = -1;
    long bestDist = 0;
    for (int i = start; i < c->ui.hitCount; i++) {
        const struct pd_hit* h = &c->ui.hits[i];
        if (c->fx >= h->x && c->fx < h->x + h->w && c->fy >= h->y && c->fy < h->y + h->h) {
            if (h->h < 4) {
                if (tiny < 0) tiny = i;
                continue;
            }
            long area = (long) h->w * h->h;
            if (under < 0 || area < underArea) {
                under = i;
                underArea = area;
            }
            continue;
        }
        long dx = (h->x + h->w / 2) - c->fx;
        long dy = (h->y + h->h / 2) - c->fy;
        long d = dx * dx + dy * dy;
        if (best < 0 || d < bestDist) {
            best = i;
            bestDist = d;
        }
    }
    return under >= 0 ? under : tiny >= 0 ? tiny : best;
}

static void focus_center(struct pd_companion* c, int i) {
    const struct pd_hit* h = &c->ui.hits[i];
    c->fx = h->x + h->w / 2;
    c->fy = h->y + h->h / 2;
    c->cursorSet = true;
}

static void draw_ring(struct pd_canvas* cv, const struct pd_hit* h) {
    if (h->w < 4 || h->h < 4) {
        return;
    }
    // A white-only cursor disappears on Emerald's white description box.
    pd_fill(cv, h->x, h->y, h->w, 2, 0x182030);
    pd_fill(cv, h->x, h->y + h->h - 2, h->w, 2, 0x182030);
    pd_fill(cv, h->x, h->y, 2, h->h, 0x182030);
    pd_fill(cv, h->x + h->w - 2, h->y, 2, h->h, 0x182030);
    pd_fill(cv, h->x + 1, h->y + 1, h->w - 2, 1, PD_FOCUS_COLOR);
    pd_fill(cv, h->x + 1, h->y + h->h - 2, h->w - 2, 1, PD_FOCUS_COLOR);
    pd_fill(cv, h->x + 1, h->y + 1, 1, h->h - 2, PD_FOCUS_COLOR);
    pd_fill(cv, h->x + h->w - 2, h->y + 1, 1, h->h - 2, PD_FOCUS_COLOR);
}

static void draw_notice(struct pd_canvas* cv, const struct pd_companion* c) {
    pd_fill(cv, 0, 0, PD_COMPANION_W, PD_COMPANION_H, PD_NOTICE_BG);
    pd_text(cv, 12, 14, "NO COMPANION FOR THIS GAME", PD_NOTICE_TITLE, PD_NO_SHADOW);
    pd_text_wrap(cv, 12, 42, PD_COMPANION_W - 24, 14, 2, c->title, PD_NOTICE_TEXT, PD_NO_SHADOW);
    pd_text_wrap(cv, 12, 88, PD_COMPANION_W - 24, 14, 4,
                 c->game.unsupported ? c->game.unsupported : "The companion doesn't read this game yet.",
                 PD_NOTICE_DIM, PD_NO_SHADOW);
    pd_text(cv, 12, PD_COMPANION_H - 24, "The game plays normally.", PD_NOTICE_DIM, PD_NO_SHADOW);
    pd_text_right(cv, PD_COMPANION_W - 12, PD_COMPANION_H - 24, "L2 BACK TO GAME", PD_NOTICE_TEXT, PD_NO_SHADOW);
}

// A stylus tap, the way pd_ui expects one: down, then up.
static bool tap(struct pd_companion* c) {
    int i = focus_index(c);
    if (i < 0) {
        return false;
    }
    focus_center(c, i);
    pd_logf("companion: tap hit %d at (%d, %d)", c->ui.hits[i].id, c->fx, c->fy);
    pd_ui_touch(&c->ui, c->fx, c->fy, true);
    pd_ui_touch(&c->ui, c->fx, c->fy, false);
    if (c->ui.overlay != PD_OVERLAY_NONE) {
        // A dialog opened (a confirm, a pick-list): start on its first button,
        // not on the scrim behind it.
        c->cursorSet = false;
    }
    c->dirty = true;
    return true;
}

// A drag on a list: down, move, up. Scrolling down drags up, as a finger does.
static void drag(struct pd_companion* c, int x, int y, int toY) {
    pd_ui_touch(&c->ui, x, y, true);
    pd_ui_touch(&c->ui, x, toY, true);
    pd_ui_touch(&c->ui, x, toY, false);
    c->dirty = true;
}

// Do two spans share a band, with a little slack? Used to keep a left/right
// press in its own row (the GUIDE's page chips sit right under the tab bar, and
// a pure distance score sent the cursor up to DEX instead of on to the next
// chip) and an up/down press in its own column.
static bool in_band(int a0, int a1, int b0, int b1) {
    return b0 < a1 + 4 && b1 > a0 - 4;
}

// A bag row is a logical item, including when its next neighbour is below the
// clipped list window. Keep up/down in that list instead of jumping to PC.
static bool focus_bag_row(struct pd_companion* c, int next) {
    if (next < 0 || next >= c->ui.bagRowCount) return false;
    const struct pd_hit* list = NULL;
    for (int i = 0; i < c->ui.hitCount; i++) {
        if (c->ui.hits[i].id == HIT_BAG_LIST) { list = &c->ui.hits[i]; break; }
    }
    if (!list) return false;
    int top = list->y + next * PD_BAG_ROW_H - c->ui.bagScroll;
    int scroll = c->ui.bagScroll;
    if (top < list->y) scroll -= list->y - top;
    else if (top + PD_BAG_ROW_H > list->y + list->h) scroll += top + PD_BAG_ROW_H - (list->y + list->h);
    if (scroll < 0) scroll = 0;
    if (scroll > c->ui.bagScrollMax) scroll = c->ui.bagScrollMax;
    if (scroll != c->ui.bagScroll) {
        c->ui.bagScroll = scroll;
        c->dirty = true;
        pd_companion_draw(c); // rebuild the clipped hits before focusing the new row
    }
    for (int i = 0; i < c->ui.hitCount; i++) {
        if (c->ui.hits[i].id == HIT_BAG_ROW + next) {
            focus_center(c, i);
            c->dirty = true;
            return true;
        }
    }
    return false;
}

// A DEX row is a logical entry too. The list's scroll settles on a row
// boundary, so the cursor never sits on a clipped sliver that draws no ring.
static bool focus_dex_row(struct pd_companion* c, int next) {
    int count = pd_ui_dex_row_count();
    if (next < 0 || next >= count) return false;
    const struct pd_hit* list = NULL;
    for (int i = 0; i < c->ui.hitCount; i++) {
        if (c->ui.hits[i].id == HIT_DEX_LIST) { list = &c->ui.hits[i]; break; }
    }
    if (!list) return false;
    int top = list->y + next * PD_DEX_ROW_H - c->ui.dexScroll;
    int scroll = c->ui.dexScroll;
    if (top < list->y) scroll -= list->y - top;
    else if (top + PD_DEX_ROW_H > list->y + list->h) scroll += top + PD_DEX_ROW_H - (list->y + list->h);
    int maxScroll = count * PD_DEX_ROW_H - list->h;
    if (maxScroll < 0) maxScroll = 0;
    if (scroll < 0) scroll = 0;
    if (scroll > maxScroll) scroll = maxScroll;
    if (scroll != c->ui.dexScroll) {
        c->ui.dexScroll = scroll;
        c->dirty = true;
        pd_companion_draw(c); // rebuild the clipped hits before focusing the new row
    }
    int national = pd_ui_dex_row_national(next);
    for (int i = 0; i < c->ui.hitCount; i++) {
        if (c->ui.hits[i].id == HIT_DEX_ROW + national) {
            focus_center(c, i);
            c->dirty = true;
            return true;
        }
    }
    return false;
}

// The nearest hit in that direction. A list scrolls when there is none.
static bool step(struct pd_companion* c, int dx, int dy) {
    int cur = focus_index(c);
    if (cur < 0) {
        return false;
    }
    const struct pd_hit* h = &c->ui.hits[cur];
    if (dy && c->ui.tab == PD_TAB_DEX && c->ui.overlay == PD_OVERLAY_NONE && !c->ui.dexOpen &&
        h->id >= HIT_DEX_ROW && h->id < HIT_DEX_ROW + PD_NATIONAL_COUNT) {
        int index = pd_ui_dex_row_index(h->id - HIT_DEX_ROW);
        if (index >= 0) {
            if (focus_dex_row(c, index + dy)) return true;
            // At the last entry, DOWN must not leave the list sideways.
            if (dy > 0) return false;
        }
    }
    if (c->ui.tab == PD_TAB_BAG && c->ui.overlay == PD_OVERLAY_NONE) {
        if (dx > 0 && h->id == HIT_BAG_ROW + c->ui.bagSelected && c->ui.bagSelected >= 0) {
            for (int i = 0; i < c->ui.hitCount; i++) {
                if (c->ui.hits[i].id == HIT_BAG_STAR) {
                    focus_center(c, i);
                    c->dirty = true;
                    return true;
                }
            }
        }
        if (dx < 0 && h->id == HIT_BAG_STAR && focus_bag_row(c, c->ui.bagSelected)) return true;
    }
    if (dy && c->ui.tab == PD_TAB_BAG && c->ui.overlay == PD_OVERLAY_NONE &&
        h->id >= HIT_BAG_ROW && h->id < HIT_BAG_ROW + c->ui.bagRowCount) {
        if (focus_bag_row(c, h->id - HIT_BAG_ROW + dy)) return true;
        // At the bottom, DOWN must not send the cursor sideways to PC.
        if (dy > 0) return false;
    }
    int cx = h->x + h->w / 2;
    int cy = h->y + h->h / 2;
    int best = -1;
    long bestScore = 0;
    int start = hits_start(c);
    bool trace = getenv("PD_DEBUG_HITS") != NULL;
    if (trace) {
        pd_logf("hits: from %d at (%d, %d), dir (%d, %d)", h->id, cx, cy, dx, dy);
    }
    // Same row first, then anything: the row pass only fails when the press
    // leaves the block the cursor is in (a chip row's last chip, say).
    for (int pass = 0; pass < 2 && best < 0; pass++) {
        for (int i = start; i < c->ui.hitCount; i++) {
            if (i == cur) {
                continue;
            }
            const struct pd_hit* o = &c->ui.hits[i];
            if (pass == 0) {
                bool band = dx ? in_band(h->y, h->y + h->h, o->y, o->y + o->h)
                               : in_band(h->x, h->x + h->w, o->x, o->x + o->w);
                if (!band) {
                    continue;
                }
            }
            int ox = o->x + o->w / 2;
            int oy = o->y + o->h / 2;
            int along = dx ? (ox - cx) * dx : (oy - cy) * dy;
            if (along <= 2) {
                continue;
            }
            int across = dx ? abs(oy - cy) : abs(ox - cx);
            long score = along + (long) across * 2;
            if (trace) {
                pd_logf("hits:   [%d] %d at (%d, %d) along %d across %d score %ld", pass, o->id, ox, oy, along, across,
                        score);
            }
            if (best < 0 || score < bestScore) {
                best = i;
                bestScore = score;
            }
        }
    }
    if (trace) {
        pd_logf("hits: picked %d", best >= 0 ? c->ui.hits[best].id : -1);
    }
    if (best >= 0) {
        focus_center(c, best);
        c->dirty = true;
        return true;
    }
    if (dy != 0 && h->scroll) {
        drag(c, cx, cy, cy + (dy > 0 ? -PD_SCROLL_STEP : PD_SCROLL_STEP));
        return true;
    }
    return false;
}

// One press, before the trace line below.
static bool nav_key(struct pd_companion* c, enum pd_nav key) {
    switch (key) {
    case PD_NAV_OK:
        return tap(c);
    case PD_NAV_BACK:
        c->cursorSet = false;
        if (pd_ui_back(&c->ui)) {
            c->dirty = true;
            return true;
        }
        return false;
    case PD_NAV_TAB_NEXT:
    case PD_NAV_TAB_PREV:
        pd_ui_next_tab(&c->ui, key == PD_NAV_TAB_NEXT ? 1 : -1);
        c->cursorSet = false;
        c->dirty = true;
        return true;
    case PD_NAV_UP:
        return step(c, 0, -1);
    case PD_NAV_DOWN:
        return step(c, 0, 1);
    case PD_NAV_LEFT:
        return step(c, -1, 0);
    case PD_NAV_RIGHT:
        return step(c, 1, 0);
    default:
        return false;
    }
}

bool pd_companion_nav(struct pd_companion* c, enum pd_nav key) {
    if (!c || !c->visible || !c->game.cfg) {
        return false;
    }
    bool dirty = nav_key(c, key);
    // A line per press: with the "draw tab" line, a crash in the portable UI
    // says exactly where it happened.
    int hits = 0;
    int focus = -1;
    pd_companion_debug(NULL, &hits, &focus);
    pd_logf("companion: nav %d on tab %d overlay %d, %d hits, focus %d -> %s", (int) key, c->ui.tab,
            c->ui.overlay, hits, focus, dirty ? "redraw" : "nothing");
    return dirty;
}

// What the crash handler reads: updated around every draw, so a crash in the
// portable UI says which tab and which control it was on.
static volatile int dbgTab;
static volatile int dbgHits;
static volatile int dbgFocus = -1;
static volatile int dbgOverlay;

void pd_companion_debug(int* tab, int* hits, int* focus) {
    if (tab) {
        *tab = dbgTab;
    }
    if (hits) {
        *hits = dbgHits;
    }
    if (focus) {
        *focus = dbgFocus;
    }
}

const uint32_t* pd_companion_draw(struct pd_companion* c) {
    if (!c) {
        return NULL;
    }
    if (!c->dirty) {
        return c->px;
    }
    struct pd_canvas canvas;
    pd_canvas_init(&canvas, PD_COMPANION_W, PD_COMPANION_H, c->px);
    dbgTab = c->ui.tab;
    dbgOverlay = c->ui.overlay;
    pd_logf("companion: draw tab %d overlay %d (party %d, bag %s, waiting %d)", c->ui.tab, c->ui.overlay,
            c->shown.partyCount, c->shown.bagOk ? "yes" : "no", pd_ui_waiting(&c->ui));
    if (c->game.cfg) {
        dbgHits = c->ui.hitCount;
        int i = focus_index(c);
        dbgFocus = i >= 0 ? c->ui.hits[i].id : -1;
        pd_ui_draw(&c->ui, &canvas, &c->game, &c->shown, &c->host);
        dbgHits = c->ui.hitCount;
        i = focus_index(c);
        if (i >= 0) {
            dbgFocus = c->ui.hits[i].id;
            focus_center(c, i);
            draw_ring(&canvas, &c->ui.hits[i]);
        }
    } else {
        draw_notice(&canvas, c);
        dbgFocus = -1;
    }
    c->dirty = false;
    return c->px;
}

void pd_companion_set_state(struct pd_companion* c, bool hasState, const char* when) {
    if (!c) {
        return;
    }
    bool changed = c->host.hasState != hasState || (hasState && when && strcmp(c->host.stateWhen, when) != 0);
    c->host.hasState = hasState;
    snprintf(c->host.stateWhen, sizeof(c->host.stateWhen), "%s", when ? when : "");
    if (changed) {
        c->dirty = true;
    }
}

void pd_companion_resumed(struct pd_companion* c) {
    if (!c) {
        return;
    }
    pd_ui_resumed(&c->ui);
    c->dirty = true;
}

void pd_companion_restarted(struct pd_companion* c) {
    if (!c) return;
    pd_ui_restarted(&c->ui);
    c->dirty = true;
}

int pd_companion_screen_mode(const struct pd_companion* c) {
    return c ? c->settings.screenMode : PD_SCREEN_SHARP;
}

void pd_companion_set_screen_mode(struct pd_companion* c, int mode) {
    if (!c || mode == c->settings.screenMode) {
        return;
    }
    c->settings.screenMode = mode;
    c->dirty = true;
}

int pd_companion_ff_speed(const struct pd_companion* c) {
    return c ? c->settings.ffSpeed : 200;
}

void pd_companion_set_ff_speed(struct pd_companion* c, int pct) {
    if (!c) return;
    c->settings.ffSpeed = pct;
    c->dirty = true;
}

int pd_companion_game_speed(const struct pd_companion* c) {
    return c ? c->settings.gameSpeed : 100;
}

bool pd_companion_slow_motion(const struct pd_companion* c) {
    return c ? c->settings.slowMotion : false;
}

bool pd_companion_save_settings(struct pd_companion* c) {
    if (!c) {
        return false;
    }
    set_host(c);
    return pd_settings_save(&c->settings, c->settingsPath);
}
