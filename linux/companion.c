#include "companion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "files.h"
#include "log.h"
#include "pd_canvas.h"
#include "pd_dex.h"
#include "pd_map.h"
#include "pd_romart.h"
#include "pd_settings.h"

// How often the game is read, in emulated frames.
#define PD_SAMPLE_EVERY 10
// One press on a list's edge scrolls this far, in canvas pixels.
#define PD_SCROLL_STEP 64

#define PD_NOTICE_BG 0x101820u
#define PD_NOTICE_TITLE 0xF8F8F8u
#define PD_NOTICE_TEXT 0xC8D0E0u
#define PD_NOTICE_DIM 0x8FA0B0u
#define PD_FOCUS_COLOR 0xFFFFFFu

struct pd_companion {
    struct pd_game game;
    struct pd_snapshot snap, shown;
    struct pd_ui ui;
    struct pd_settings settings;
    struct pd_host_info host;
    char settingsPath[1024];
    char title[64];
    uint32_t px[PD_COMPANION_W * PD_COMPANION_H];
    bool visible;
    bool dirty;
    bool cursorSet; // fx/fy point at something the last draw made
    int fx, fy;     // the cursor, in canvas pixels
    bool sampled;
    unsigned lastSample;
};

static void set_host(struct pd_companion* c) {
    c->host.count = 2;
    c->host.labels[0] = "GAME";
    snprintf(c->host.values[0], sizeof(c->host.values[0]), "%s", c->title);
    c->host.labels[1] = "SPEED";
    snprintf(c->host.values[1], sizeof(c->host.values[1]), "x%d", c->settings.ffSpeed);
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
    snprintf(c->host.values[1], sizeof(c->host.values[1]), "%sx%d", ffOn ? "FF " : "", c->settings.ffSpeed);

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
    int best = -1;
    long bestDist = 0;
    for (int i = start; i < c->ui.hitCount; i++) {
        const struct pd_hit* h = &c->ui.hits[i];
        if (c->fx >= h->x && c->fx < h->x + h->w && c->fy >= h->y && c->fy < h->y + h->h) {
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
    return under >= 0 ? under : best;
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
    pd_fill(cv, h->x, h->y, h->w, 2, PD_FOCUS_COLOR);
    pd_fill(cv, h->x, h->y + h->h - 2, h->w, 2, PD_FOCUS_COLOR);
    pd_fill(cv, h->x, h->y, 2, h->h, PD_FOCUS_COLOR);
    pd_fill(cv, h->x + h->w - 2, h->y, 2, h->h, PD_FOCUS_COLOR);
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

// The nearest hit in that direction. A list scrolls when there is none.
static bool step(struct pd_companion* c, int dx, int dy) {
    int cur = focus_index(c);
    if (cur < 0) {
        return false;
    }
    const struct pd_hit* h = &c->ui.hits[cur];
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
    pd_logf("companion: draw tab %d overlay %d (party %d, bag %s)", c->ui.tab, c->ui.overlay, c->shown.partyCount,
            c->shown.bagOk ? "yes" : "no");
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
    return c ? c->settings.ffSpeed : PD_FF_MIN;
}

bool pd_companion_save_settings(struct pd_companion* c) {
    if (!c) {
        return false;
    }
    set_host(c);
    return pd_settings_save(&c->settings, c->settingsPath);
}
