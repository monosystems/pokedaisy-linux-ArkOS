// The companion's frame: the tab bar, the open tab, overlays, and the stylus.
// Each tab draws itself in its own file (pd_ui_party.c, _battle, _bag,
// _settings); pd_ui_internal.h has what they share.
#include <stdlib.h>
#include <string.h>

#include "pd_dex.h"
#include "pd_guide.h"
#include "pd_ui_internal.h"

#define CHIP_H 22
// The bar's three icon chips - MORE, the fast-forward pair and the gear.
// Narrow, because a battle's six chips have to fit 320 px next to them and
// BATTLE alone is 42 of those pixels.
#define ICON_W 20
// How far apart the bar's chips sit.
#define CHIP_GAP 3
// How far the stylus moves on a list before it scrolls instead of tapping.
#define DRAG_SLOP 6
// TWEAKS > TAB ANIMATIONS: the new tab slides in from the side over this long,
// starting a sixth of the screen out - the app's slideInHorizontally, w / 6 at
// 220 ms.
#define TAB_SLIDE_MS 220
#define TAB_SLIDE_PX (PD_UI_WIDTH / 6)
// The party icon's fastest frame (a healthy Pokémon's six GBA frames): while a
// slot is held, draw at that rate so the game's own speed reads right.
#define ICON_MS 60

// The companion's tabs in the app's order (COMPANION_TABS: PARTY, DEX, MAP,
// ITEMS, GUIDE, CARD). The TRAINER CARD is off the bar until the player puts it
// there in SETTINGS > TAB BAR; it also opens from SETTINGS' title window.
const enum pd_tab PD_TAB_ORDER[PD_TAB_ORDER_COUNT] = {
    PD_TAB_PARTY, PD_TAB_DEX, PD_TAB_MAP, PD_TAB_BAG, PD_TAB_GUIDE, PD_TAB_CARD,
};

static const char* const TAB_LABELS[PD_TAB_COUNT] = {
    [PD_TAB_PARTY] = "PARTY", [PD_TAB_BAG] = "BAG", [PD_TAB_MAP] = "MAP", [PD_TAB_GUIDE] = "GUIDE",
    [PD_TAB_DEX] = "DEX", [PD_TAB_BATTLE] = "BATTLE", [PD_TAB_SETTINGS] = "", [PD_TAB_CARD] = "CARD",
    [PD_TAB_MORE] = "MORE",
};

const char* pd_tab_label(enum pd_tab tab) {
    return (unsigned) tab < PD_TAB_COUNT ? TAB_LABELS[tab] : "";
}

// The MORE chip's nine squares, 11x11 (PixelIcons.more, the gear's size):
// three rows of three 3x3 squares.
static const uint16_t MORE_BITMAP[11] = {
    0x777, 0x777, 0x777, 0, 0x777, 0x777, 0x777, 0, 0x777, 0x777, 0x777,
};

// C-04: a tab exists only when this game has its data - the POKéDEX its
// tables, the GUIDE its pages, the TRAINER CARD its art (and a game this port
// reads at all: a refused ROM shows NOT SUPPORTED on every page, so no chip
// should offer one). PARTY, BAG and MAP read the save such a game still has.
// SETTINGS > TAB BAR asks the same question, so it is not static.
bool pd_tab_available(const struct pd_game* g, enum pd_tab t) {
    switch (t) {
    case PD_TAB_DEX: return pd_dex_available(g);
    case PD_TAB_GUIDE: return g->cfg != NULL && pd_guide_for(g) != NULL;
    case PD_TAB_CARD: return g->cfg != NULL && g->cfg->cardStyle && g->rom != NULL;
    default: return true;
    }
}

static bool tab_in_more(const struct pd_ui* ui, enum pd_tab t) {
    for (int i = 0; i < ui->moreCount; i++) {
        if (ui->more[i] == t) return true;
    }
    return false;
}

static bool tab_on_bar(const struct pd_ui* ui, enum pd_tab t) {
    for (int i = 0; i < ui->barCount; i++) {
        if (ui->bar[i] == t) return true;
    }
    return false;
}

// The bar from what this game has and what the player picked (SETTINGS > TAB
// BAR): the picks first, in the app's order, then the game's other tabs fill
// the places left over. BATTLE leads a battle and takes one of the places, and
// what is left waits behind MORE (C-02, C-03, C-04).
static void layout(struct pd_ui* ui, const struct pd_game* g) {
    int room = PD_BAR_TABS - (ui->battleWasOn ? 1 : 0);
    bool on[PD_TAB_COUNT] = { false };
    ui->barCount = 0;
    ui->moreCount = 0;
    ui->tabsPicked = 0;
    if (ui->battleWasOn) ui->bar[ui->barCount++] = PD_TAB_BATTLE;
    int seated = 0;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < PD_TAB_ORDER_COUNT; i++) {
            enum pd_tab t = PD_TAB_ORDER[i];
            if (!pd_tab_available(g, t)) continue;
            bool picked = pd_tab_shown(ui->settings, t);
            if (pass == 0 && picked) ui->tabsPicked++;
            if (pass == 0 ? !picked : picked) continue;
            if (on[t]) continue;
            if (seated >= room) continue;
            on[t] = true;
            seated++;
        }
    }
    for (int i = 0; i < PD_TAB_ORDER_COUNT; i++) {
        enum pd_tab t = PD_TAB_ORDER[i];
        if (!pd_tab_available(g, t)) continue;
        if (on[t]) ui->bar[ui->barCount++] = t;
        else ui->more[ui->moreCount++] = t;
    }
    // A tab this game lost, or one the bar gave up and MORE doesn't hold,
    // falls back to the first chip - the app's `current`. PARTY is in every
    // game's bar, so there always is one.
    bool ok = (ui->tab == PD_TAB_MORE) ? ui->moreCount > 0
            : ui->tab == PD_TAB_SETTINGS;
    for (int i = 0; !ok && i < ui->barCount; i++) ok = ui->bar[i] == ui->tab;
    for (int i = 0; !ok && i < ui->moreCount; i++) ok = ui->more[i] == ui->tab;
    if (!ok) ui->tab = ui->bar[0];
}

// The SETTINGS gear, 11x11 (bit 0 = left):
//   ....###....   .##.###.##.   .#########.   ..###.###..
//   ####...####   ###.....###   ####...####   ...and back up.
static const uint16_t GEAR[11] = {
    0x070, 0x376, 0x3FE, 0x1DC, 0x78F, 0x707, 0x78F, 0x1DC, 0x3FE, 0x376, 0x070,
};

// The bar's slot for a tab, which is the way the slide points: a tab MORE
// holds sits where MORE does, and the gear ends the row (the app's barIndex).
static int bar_slot(const struct pd_ui* ui, enum pd_tab t) {
    for (int i = 0; i < ui->barCount; i++) {
        if (ui->bar[i] == t) return i;
    }
    if (t == PD_TAB_MORE || tab_in_more(ui, t)) return ui->barCount;
    return ui->barCount + (ui->moreCount ? 1 : 0);
}

void ui_open_tab(struct pd_ui* ui, enum pd_tab tab) {
    if (ui->tab == tab) return;
    if (pd_tweak_on(ui->settings, PD_TWEAK_TAB_ANIM)) {
        ui->tabSlideDir = bar_slot(ui, tab) >= bar_slot(ui, ui->tab) ? 1 : -1;
        ui->tabSlideMs = TAB_SLIDE_MS;
    } else {
        ui->tabSlideMs = 0;
    }
    ui->tab = tab;
    ui->summarySlot = -1;
}

// How far the tab body is still from home, in pixels: the app's slide eases
// out, so the tab covers most of the way in the first half.
static int slide_dx(const struct pd_ui* ui) {
    if (!ui->tabSlideMs) return 0;
    int left = ui->tabSlideMs;
    return ui->tabSlideDir * TAB_SLIDE_PX * (left * left / TAB_SLIDE_MS) / TAB_SLIDE_MS;
}

// Moves what the tab body drew by dx pixels, uncovering the backdrop behind it
// - the other half of the slide (the app slides the old tab out under it; here
// the frame is redrawn whole, so the backdrop is what it uncovers).
static void slide_content(struct pd_canvas* c, int dx) {
    int strip = dx < 0 ? -dx : dx;
    for (int y = CONTENT_Y; y < c->h; y++) {
        uint32_t* row = c->px + y * c->w;
        if (dx > 0) memmove(row + dx, row, (size_t) (c->w - dx) * sizeof(uint32_t));
        else memmove(row, row - dx, (size_t) (c->w + dx) * sizeof(uint32_t));
    }
    pd_clip(c, dx > 0 ? 0 : c->w + dx, CONTENT_Y, strip, c->h - CONTENT_Y);
    pd_backdrop(c);
    pd_unclip(c);
}

void pd_ui_init(struct pd_ui* ui, struct pd_settings* settings) {
    memset(ui, 0, sizeof(*ui));
    ui->tab = PD_TAB_PARTY;
    ui->summarySlot = -1;
    ui->bagPocket = PD_BAG_ALL;
    ui->bagSelected = -1;
    ui->pressedId = -1;
    ui->dexNational = -1;
    ui->mapPage = -1;
    ui->mapSel = -1;
    ui->mapDungeon = -1;
    ui->guideOpen = -1;
    ui->foeSelected = ui->foeShown = -1;
    ui->lastFoeActive = ui->lastFoeNext = -1;
    ui->settingsPage = PD_SETTINGS_HOME;
    ui->waitBlink = true;
    ui->settings = settings;
}

void pd_ui_update(struct pd_ui* ui, const struct pd_snapshot* s) {
    // C-05: before the field (the game's title screen, its intro, CONTINUE
    // loading the save) the save's tabs must not show what the RAM still holds.
    // The app ends the wait at the field's own callback, a battle or five map
    // positions; the port reads no callback, so it takes a battle, a party (one
    // only a loaded save has) or the fifth position.
    if (!ui->started && s->valid) {
        if (s->inBattle || s->partyCount > 0) {
            ui->started = true;
        } else if (s->posOk) {
            bool known = false;
            for (int i = 0; i < ui->posCount && !known; i++) {
                known = ui->posSeen[i][0] == s->mapGroup && ui->posSeen[i][1] == s->mapNum &&
                        ui->posSeen[i][2] == s->x && ui->posSeen[i][3] == s->y;
            }
            if (!known && ui->posCount < 5) {
                ui->posSeen[ui->posCount][0] = s->mapGroup;
                ui->posSeen[ui->posCount][1] = s->mapNum;
                ui->posSeen[ui->posCount][2] = s->x;
                ui->posSeen[ui->posCount][3] = s->y;
                ui->posCount++;
            }
            if (ui->posCount >= 5) ui->started = true;
        }
    }
    bool on = s->valid && s->inBattle && s->battlers[PD_POS_PLAYER_LEFT].species;
    if (on && !ui->battleWasOn) {
        // A battle opens BATTLE, at the front of the bar (the app's rule);
        // its end brings back what was open. TWEAKS > JUMP TO BATTLE off leaves
        // the companion where it is: the chip still shows up to be tapped.
        ui->tabBeforeBattle = ui->tab;
        if (pd_tweak_on(ui->settings, PD_TWEAK_JUMP_BATTLE)) {
            ui->tabSlideMs = 0;
            ui->tab = PD_TAB_BATTLE;
            ui->summarySlot = -1;
        }
        ui->overlay = PD_OVERLAY_NONE;
        ui->battlePane = 0;
        ui->foeSeen = 0;
        ui->foeSelected = -1;
        ui->lastFoeActive = ui->lastFoeNext = -1;
    } else if (!on && ui->battleWasOn) {
        if (ui->tab == PD_TAB_BATTLE) ui->tab = ui->tabBeforeBattle;
    }
    ui->battleWasOn = on;
    // The FOE TEAM: what's been sent out or announced counts as seen; a new
    // pick by the trainer is shown, whatever was tapped (CompanionScreen).
    if (on && (s->foeActive != ui->lastFoeActive || s->foeNext != ui->lastFoeNext)) {
        if (s->foeActive >= 0) ui->foeSeen |= 1u << s->foeActive;
        if (s->foeNext >= 0) {
            ui->foeSeen |= 1u << s->foeNext;
            ui->foeSelected = -1;
        }
        ui->lastFoeActive = s->foeActive;
        ui->lastFoeNext = s->foeNext;
    }
    if (ui->foeSelected >= s->foeCount) ui->foeSelected = -1;
    if (ui->summarySlot >= s->partyCount) ui->summarySlot = -1;
}

bool pd_ui_waiting(const struct pd_ui* ui) {
    return ui && !ui->started;
}

void pd_ui_resumed(struct pd_ui* ui) {
    if (ui) ui->started = true;
}

static const struct pd_hit* hit_at(const struct pd_ui* ui, int x, int y) {
    for (int i = ui->hitCount - 1; i >= 0; i--) {
        const struct pd_hit* h = &ui->hits[i];
        if (x >= h->x && x < h->x + h->w && y >= h->y && y < h->y + h->h) return h;
    }
    return NULL;
}

static void chip(struct pd_ui* ui, struct pd_canvas* c, int x, int w, bool open, int id) {
    pd_title_box(c, x, 2, w, CHIP_H, open || ui->pressedId == id ? PD_TITLE_FILL : PD_LIST_FILL);
    ui_add_hit(ui, x, 2, w, CHIP_H, id);
}

static void tab_bar(struct pd_ui* ui, struct pd_canvas* c, const struct pd_host_info* host) {
    int more = ui->moreCount ? 1 : 0;
    // Three pixels between the chips rather than four: a battle's six chips
    // and the three icon chips after them have to fit 320 px, and BATTLE's
    // word alone is 42 of them. What the division leaves over goes to the
    // first chips, so the row ends at the content's right edge.
    int gaps = CHIP_GAP * (ui->barCount + more + 1);
    int room = c->w - 2 * MARGIN - gaps - ICON_W * (2 + more);
    int w = room / ui->barCount, rem = room - w * ui->barCount;
    int x = MARGIN;
    for (int i = 0; i < ui->barCount; i++) {
        enum pd_tab t = ui->bar[i];
        int ww = w + (i < rem ? 1 : 0);
        bool open = ui->tab == t;
        chip(ui, c, x, ww, open, HIT_TAB + (int) t);
        int tw = pd_text_width(TAB_LABELS[t]);
        pd_text(c, x + (ww - tw) / 2, pd_text_y(2, CHIP_H), TAB_LABELS[t], open ? PD_VALUE : PD_TITLE_TEXT,
                open ? PD_VALUE_SHADOW : PD_TITLE_SHADOW);
        x += ww + CHIP_GAP;
    }
    // MORE, left of the gear and only while something is hidden: lit while it
    // is open and while one of its tabs is, since BACK goes there.
    if (more) {
        bool open = ui->tab == PD_TAB_MORE || tab_in_more(ui, ui->tab);
        chip(ui, c, x, ICON_W, open, HIT_MORE);
        pd_bitmap(c, x + (ICON_W - 11) / 2, 2 + (CHIP_H - 11) / 2, MORE_BITMAP, 11, open ? PD_VALUE : PD_TITLE_TEXT);
        x += ICON_W + CHIP_GAP;
    }
    // Fast-forward: lit while on.
    bool ff = host && host->ffOn;
    chip(ui, c, x, ICON_W, ff, HIT_FF);
    uint32_t col = ff ? PD_VALUE : PD_TITLE_TEXT;
    pd_triangle(c, x + 4, 2 + CHIP_H / 2 - 4, 5, PD_RIGHT, col);
    pd_triangle(c, x + 10, 2 + CHIP_H / 2 - 4, 5, PD_RIGHT, col);
    x += ICON_W + CHIP_GAP;
    // The TRAINER CARD opens from SETTINGS: the gear stays lit on its page
    // until the card has a chip or a MORE tile of its own.
    bool open = ui->tab == PD_TAB_SETTINGS ||
                (ui->tab == PD_TAB_CARD && !tab_on_bar(ui, PD_TAB_CARD) && !tab_in_more(ui, PD_TAB_CARD));
    chip(ui, c, x, ICON_W, open, HIT_TAB + PD_TAB_SETTINGS);
    pd_bitmap(c, x + (ICON_W - 11) / 2, 2 + (CHIP_H - 11) / 2, GEAR, 11, open ? PD_VALUE : PD_TITLE_TEXT);
}

// The tabs that show what the save holds, so they wait for the game (C-05).
// The POKéDEX and the GUIDE read the ROM's tables and the save's flags, and
// SETTINGS and MORE never wait.
static bool tab_waits(enum pd_tab t) {
    return t == PD_TAB_PARTY || t == PD_TAB_BAG || t == PD_TAB_MAP || t == PD_TAB_CARD;
}

// C-05: a boot still on the game's title screen. The app dims the game's own
// empty party slots to 45% and lays its waiting window over them; the port
// draws its empty slots (the FireRed-like ones it has), dims them the same way
// and puts the same window and PRESS START over them.
static void waiting_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g) {
    struct pd_snapshot empty;
    memset(&empty, 0, sizeof(empty));
    empty.money = -1;
    int hits = ui->hitCount;
    ui_party_tab(ui, c, g, &empty);
    // The slots are the picture here, not a control: the app's WaitingView has
    // none either.
    ui->hitCount = hits;
    pd_dim_rect(c, 0, CONTENT_Y, c->w, c->h - CONTENT_Y, 0x8C);

    const char* line = "Your party shows up here once you're in the game.";
    int w = c->w - 2 * MARGIN;
    int inner = w - 2 * (7 + 6); // pd_list_box's frame, then its inset
    int lines = pd_text_wrap(NULL, 0, 0, inner, 16, 3, line, 0, 0);
    int h = 14 + 8 + lines * 16 + 10 + 16;
    int x = MARGIN, y = CONTENT_Y + (c->h - CONTENT_Y - h) / 2;
    int f = pd_list_box(c, x, y, w, h);
    pd_text_wrap(c, x + f + 6, y + f + 8, w - 2 * (f + 6), 16, 3, line, PD_LABEL, PD_LABEL_SHADOW);
    if (ui->waitBlink) {
        const char* label = "PRESS START";
        pd_text(c, x + (w - pd_text_width(label)) / 2, y + h - f - 6 - 16, label, PD_VALUE, PD_VALUE_SHADOW);
    }
}

void pd_ui_draw(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g,
                const struct pd_snapshot* s, const struct pd_host_info* host) {
    layout(ui, g);
    ui->hitCount = 0;
    pd_backdrop(c);
    tab_bar(ui, c, host);
    // The tab body slides in (TWEAKS): its hits move with it, so a press lands
    // on what the player sees.
    int dx = slide_dx(ui);
    ui->hitDx = dx;
    if (ui->tab == PD_TAB_SETTINGS) {
        ui_settings_tab(ui, c, g, s, host);
    } else if (!g->cfg) {
        ui_notice(c, "NOT SUPPORTED", g->unsupported ? g->unsupported : "", "SETTINGS has what was detected.");
    } else if (!ui->started && tab_waits(ui->tab)) {
        waiting_tab(ui, c, g);
    } else if (ui->tab == PD_TAB_PARTY) {
        ui_party_tab(ui, c, g, s);
    } else if (ui->tab == PD_TAB_BAG) {
        ui_bag_tab(ui, c, g, s);
    } else if (ui->tab == PD_TAB_MAP) {
        ui_map_tab(ui, c, g, s);
    } else if (ui->tab == PD_TAB_GUIDE) {
        ui_guide_tab(ui, c, g, s);
    } else if (ui->tab == PD_TAB_DEX) {
        ui_dex_tab(ui, c, g, s);
    } else if (ui->tab == PD_TAB_CARD) {
        ui_card_tab(ui, c, g, s);
    } else if (ui->tab == PD_TAB_MORE) {
        ui_more_tab(ui, c);
    } else {
        ui_battle_tab(ui, c, g, s);
    }
    ui->hitDx = 0;
    if (dx) slide_content(c, dx);
    if (ui->overlay == PD_OVERLAY_PLACES) {
        ui_places_overlay(ui, c, g);
    } else if (ui->overlay == PD_OVERLAY_GUIDE_NOTICE) {
        ui_guide_notice(ui, c, g);
    } else {
        ui_overlay(ui, c);
    }
}

// The list a drag scrolls right now.
static int* scroll_target(struct pd_ui* ui) {
    if (ui->overlay == PD_OVERLAY_PLACES) return &ui->placesScroll;
    switch (ui->tab) {
    case PD_TAB_BAG: return &ui->bagScroll;
    case PD_TAB_DEX: return &ui->dexScroll;
    case PD_TAB_GUIDE: return &ui->guideScroll;
    case PD_TAB_BATTLE: return &ui->battleScroll;
    default: return NULL;
    }
}

bool pd_ui_tick(struct pd_ui* ui, unsigned ms) {
    bool redraw = ui_card_tick(ui, ms);
    // Time passing, for the party icon's two frames (below).
    ui->animMs += ms;
    // The tab just opened slides in (TWEAKS): draw until it has arrived.
    if (ui->tabSlideMs) {
        ui->tabSlideMs -= (int) ms;
        if (ui->tabSlideMs < 0) ui->tabSlideMs = 0;
        redraw = true;
    }
    // The MAP cursor swaps sizes every 20 GBA frames, like the game's; TWEAKS
    // can keep it at one size.
    if (pd_tweak_on(ui->settings, PD_TWEAK_MAP_CURSOR)) {
        ui->blinkMs += ms;
        if (ui->blinkMs >= 333) {
            ui->blinkMs %= 333;
            ui->mapBlink = !ui->mapBlink;
            redraw = redraw || (ui->tab == PD_TAB_MAP && ui->overlay == PD_OVERLAY_NONE);
        }
    } else {
        ui->mapBlink = false;
    }
    // The party icon under the press bounces (TWEAKS), at the game's HP-based
    // speed: the fastest frame is 100 ms, so draw every 60.
    if (pd_tweak_on(ui->settings, PD_TWEAK_PARTY_ICONS) && ui->tab == PD_TAB_PARTY &&
        ui->summarySlot < 0 && ui->pressedId >= HIT_SLOT && ui->pressedId < HIT_SLOT + PD_PARTY_SIZE) {
        ui->iconMs += ms;
        if (ui->iconMs >= ICON_MS) {
            ui->iconMs = 0;
            redraw = true;
        }
    } else {
        ui->iconMs = 0;
    }
    // C-05: PRESS START blinks on the title screen's rhythm - the app fades it
    // over 1200 ms and shows it while the fade is under 0.6.
    if (!ui->started && tab_waits(ui->tab)) {
        bool on = (ui->animMs % 1200) < 720;
        if (on != ui->waitBlink) {
            ui->waitBlink = on;
            redraw = true;
        }
    }
    return redraw;
}

static void close_overlay(struct pd_ui* ui, enum pd_action action) {
    ui->overlay = PD_OVERLAY_NONE;
    if (action != PD_ACTION_NONE) ui->action = action;
}

static void act(struct pd_ui* ui, int id) {
    if (ui_map_act(ui, id) || ui_dex_act(ui, id) || ui_guide_act(ui, id) || ui_battle_act(ui, id) ||
        ui_card_act(ui, id) || ui_bag_act(ui, id) || ui_settings_act(ui, id)) {
        return;
    }
    if (ui->overlay != PD_OVERLAY_NONE) {
        if (id >= HIT_OPTION && id < HIT_OPTION + 8) {
            int i = id - HIT_OPTION;
            if (ui->overlay == PD_OVERLAY_PICK_SCREEN) ui->settings->screenMode = i;
            if (ui->overlay == PD_OVERLAY_PICK_FF) ui->settings->ffSpeed = PD_FF_MIN + i;
            close_overlay(ui, PD_ACTION_SETTINGS_CHANGED);
        } else if (id == HIT_YES) {
            enum pd_action a = ui->overlay == PD_OVERLAY_CONFIRM_SAVE ? PD_ACTION_SAVE_STATE
                : ui->overlay == PD_OVERLAY_CONFIRM_LOAD ? PD_ACTION_LOAD_STATE
                : ui->overlay == PD_OVERLAY_CONFIRM_LEAVE ? PD_ACTION_LEAVE_GAME : PD_ACTION_NONE;
            close_overlay(ui, a);
        } else if (id == HIT_NO || id == HIT_SCRIM) {
            close_overlay(ui, PD_ACTION_NONE);
        }
        return;
    }
    if (id >= HIT_TAB && id < HIT_TAB + PD_TAB_COUNT) {
        ui_open_tab(ui, (enum pd_tab) (id - HIT_TAB));
    } else if (id == HIT_MORE) {
        ui_open_tab(ui, PD_TAB_MORE);
    } else if (id == HIT_MORE_TAB_BAR) {
        // MORE's own button: the app's way into the pick of the bar's chips.
        ui->settingsPage = PD_SETTINGS_TABS;
        ui_open_tab(ui, PD_TAB_SETTINGS);
    } else if (id == HIT_FF) {
        ui->action = PD_ACTION_TOGGLE_FF;
    } else if (id >= HIT_SLOT && id < HIT_SLOT + PD_PARTY_SIZE) {
        ui->summarySlot = id - HIT_SLOT;
    } else if (id >= HIT_SUMMARY_PAGE && id < HIT_SUMMARY_PAGE + PD_SUMMARY_PAGES) {
        // The summary's page chips (moves / stats / matchups).
        ui->summaryPage = id - HIT_SUMMARY_PAGE;
    } else if (id == HIT_BACK) {
        pd_ui_back(ui);
    } else if (id >= HIT_SETTING && id < HIT_SETTING + SETTING_COUNT) {
        // The two pages of their own, then the rows that open an overlay.
        static const enum pd_overlay OPENS[SETTING_COUNT] = {
            [SETTING_SCREEN] = PD_OVERLAY_PICK_SCREEN,
            [SETTING_FF] = PD_OVERLAY_PICK_FF,
            [SETTING_SAVE_STATE] = PD_OVERLAY_CONFIRM_SAVE,
            [SETTING_LOAD_STATE] = PD_OVERLAY_CONFIRM_LOAD,
            [SETTING_LEAVE] = PD_OVERLAY_CONFIRM_LEAVE,
        };
        switch (id - HIT_SETTING) {
        case SETTING_TAB_BAR: ui->settingsPage = PD_SETTINGS_TABS; break;
        case SETTING_TWEAKS: ui->settingsPage = PD_SETTINGS_TWEAKS; break;
        default: ui->overlay = OPENS[id - HIT_SETTING]; break;
        }
    }
}

bool pd_ui_touch(struct pd_ui* ui, int x, int y, bool down) {
    if (down) {
        const struct pd_hit* h = hit_at(ui, x, y);
        int id = h ? h->id : -1;
        if (!ui->touching) {
            // A press shows on the element it starts on ...
            ui->touching = true;
            ui->dragging = false;
            ui->touchScroll = h && h->scroll;
            ui->touchX0 = x;
            ui->touchY0 = y;
            ui->touchLastY = y;
            ui->pressedId = id;
            return id != -1;
        }
        // ... a list scrolls under a drag ...
        if (ui->touchScroll && !ui->dragging && abs(y - ui->touchY0) > DRAG_SLOP) {
            ui->dragging = true;
            ui->pressedId = -1;
        }
        if (ui->dragging) {
            int* scroll = scroll_target(ui);
            if (scroll) *scroll -= y - ui->touchLastY;
            ui->touchLastY = y;
            return true;
        }
        ui->touchLastY = y;
        // ... and sliding off an element cancels it; another is never picked up.
        if (ui->pressedId != -1 && id != ui->pressedId) {
            ui->pressedId = -1;
            return true;
        }
        return false;
    }
    if (!ui->touching) return false;
    ui->touching = false;
    if (ui->dragging) {
        ui->dragging = false;
        return true;
    }
    int id = ui->pressedId;
    ui->pressedId = -1;
    if (id == -1) return false;
    act(ui, id);
    return true;
}

void pd_ui_next_tab(struct pd_ui* ui, int dir) {
    if (ui->overlay != PD_OVERLAY_NONE) return;
    // Nothing to step along before the first draw built the bar.
    if (!ui->barCount) return;
    // The bar's chips, then MORE, then the gear; a tab MORE holds steps as
    // MORE does, since that is where its chip is (the app's barIndex).
    enum pd_tab order[PD_BAR_TABS + 3];
    int n = 0, at = 0;
    for (int i = 0; i < ui->barCount; i++) {
        order[n] = ui->bar[i];
        if (order[n] == ui->tab) at = n;
        n++;
    }
    if (ui->moreCount) {
        order[n] = PD_TAB_MORE;
        if (ui->tab == PD_TAB_MORE || tab_in_more(ui, ui->tab)) at = n;
        n++;
    }
    order[n++] = PD_TAB_SETTINGS;
    if (ui->tab == PD_TAB_SETTINGS) at = n - 1;
    ui_open_tab(ui, order[(at + n + dir) % n]);
}

bool pd_ui_back(struct pd_ui* ui) {
    // The GUIDE's notice: BACK is its GO BACK.
    if (ui->overlay == PD_OVERLAY_GUIDE_NOTICE) return ui_guide_act(ui, HIT_NOTICE_BACK);
    if (ui->overlay != PD_OVERLAY_NONE) {
        ui->overlay = PD_OVERLAY_NONE;
        return true;
    }
    // A page of SETTINGS' own goes back to its option list.
    if (ui->tab == PD_TAB_SETTINGS && ui->settingsPage != PD_SETTINGS_HOME) {
        ui->settingsPage = PD_SETTINGS_HOME;
        return true;
    }
    if (ui->summarySlot >= 0) {
        ui->summarySlot = -1;
        return true;
    }
    if (ui->tab == PD_TAB_BAG && ui->bagSelected >= 0) {
        ui->bagSelected = -1;
        return true;
    }
    if (ui->tab == PD_TAB_DEX && ui->dexOpen) {
        // From EVOLVE back to INFO first, then out to the list.
        if (ui->dexSection) {
            ui->dexSection = 0;
            ui->dexEvoScroll = 0;
            return true;
        }
        ui->dexOpen = 0;
        return true;
    }
    if (ui->tab == PD_TAB_MAP && (ui->mapSel >= 0 || ui->mapDungeon >= 0 || ui->mapPage >= 0)) {
        ui->mapSel = ui->mapDungeon = ui->mapPage = -1;
        return true;
    }
    if (ui->tab == PD_TAB_BATTLE && ui->battlePane) {
        ui->battlePane = 0;
        return true;
    }
    if (ui->tab == PD_TAB_BATTLE && ui->foeSelected >= 0) {
        ui->foeSelected = -1;
        return true;
    }
    if (ui->tab == PD_TAB_GUIDE && ui->guideOpen >= 0) {
        ui->guideOpen = -1;
        ui->guideLevel = 0;
        return true;
    }
    // BACK from a tab MORE opened returns there (the app), after the tab's own
    // pages had their turn above. The TRAINER CARD without a chip of its own
    // keeps SETTINGS as its way out: that is where its chip sits.
    if (tab_in_more(ui, ui->tab)) {
        ui_open_tab(ui, PD_TAB_MORE);
        return true;
    }
    if (ui->tab == PD_TAB_CARD && !pd_tab_shown(ui->settings, PD_TAB_CARD)) {
        ui_open_tab(ui, PD_TAB_SETTINGS);
        return true;
    }
    return false;
}

enum pd_action pd_ui_take_action(struct pd_ui* ui) {
    enum pd_action a = ui->action;
    ui->action = PD_ACTION_NONE;
    return a;
}
