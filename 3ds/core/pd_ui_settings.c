// SETTINGS: what the companion knows about the game, then the options - the
// two pages of their own the app opens from here (TAB BAR, C-03, and TWEAKS,
// C-07) - and the black-out overlays they open: a pick-list for a multi-choice
// setting (the app's OptionSelector: never a row cycling through more than two
// values), a YES / NO confirm (OptionConfirm) and the TAB BAR's one-button
// notice.
#include <stdio.h>
#include <string.h>

#include "pd_ui_internal.h"

#define TITLE_H 24
#define LIST_Y (CONTENT_Y + TITLE_H + 4)

static const char* const SCREEN_NAMES[PD_SCREEN_MODES] = { "PIXEL 1x", "SHARP 1.5x", "STRETCH" };

const char* pd_screen_mode_name(int mode) {
    return mode >= 0 && mode < PD_SCREEN_MODES ? SCREEN_NAMES[mode] : "";
}

static void setting(struct pd_ui* ui, struct pd_canvas* c, int x, int* y, int w, enum setting_row row,
                    const char* label, const char* value) {
    ui_row(c, x, *y, w, label, value, ui->pressedId == HIT_SETTING + (int) row);
    ui_add_hit(ui, x - 4, *y, w + 8, ROW_H, HIT_SETTING + (int) row);
    *y += ROW_H;
}

// A row that opens a page of its own: the app's arrow at its right end.
static void page_row(struct pd_ui* ui, struct pd_canvas* c, int x, int* y, int w, enum setting_row row,
                     const char* label, const char* value) {
    setting(ui, c, x, y, w, row, label, value);
    pd_triangle(c, x + w - 6, *y - ROW_H + 6, 5, PD_RIGHT, PD_MUTED);
}

// A LABEL VALUE pair inside one column of the game's own facts.
static void info_value(struct pd_canvas* c, int x, int y, int w, const char* label, const char* value) {
    int ty = pd_text_y(y, ROW_H);
    pd_text(c, x, ty, label, PD_LABEL, PD_LABEL_SHADOW);
    int vx = x + w * 35 / 100;
    pd_text_fit(c, vx, ty, x + w - vx, value, PD_VALUE, PD_VALUE_SHADOW);
}

// Two facts to a row. The app's SETTINGS list scrolls; this screen is 320x240
// and holds options from the day the TAB BAR and TWEAKS rows arrived, so the
// four single rows the game's facts used to take became two.
static void info_pair(struct pd_canvas* c, int x, int y, int w, const char* l1, const char* v1, const char* l2,
                      const char* v2) {
    int col = (w - 8) / 2;
    info_value(c, x, y, col, l1, v1);
    if (l2) info_value(c, x + col + 8, y, col, l2, v2);
}

// --- SETTINGS' own pages ---

// A page's title window: its name, a count or note at its right, and BACK to
// the option list (the app's OptionTitleWindow with its arrow).
static void companion_page(struct pd_ui* ui, struct pd_canvas* c);
static void speed_page(struct pd_ui* ui, struct pd_canvas* c);
static void speed_label(int pct, char* buf, size_t len);
static void battle_page(struct pd_ui* ui, struct pd_canvas* c);
static void saves_page(struct pd_ui* ui, struct pd_canvas* c, const struct pd_host_info* host);

static void page_title(struct pd_ui* ui, struct pd_canvas* c, const char* title, const char* trailing) {
    int x = MARGIN, w = c->w - 2 * MARGIN;
    int f = pd_title_box(c, x, CONTENT_Y, w, TITLE_H, PD_TITLE_FILL);
    int ty = pd_text_y(CONTENT_Y, TITLE_H);
    int lw = pd_text(c, x + f + 6, ty, title, PD_TITLE_TEXT, PD_TITLE_SHADOW);
    if (trailing && *trailing) pd_text(c, x + f + 6 + lw + 10, ty, trailing, PD_MUTED, PD_MUTED_SHADOW);
    const char* label = "BACK";
    int bw = pd_text_width(label) + 12, bh = TITLE_H - 6;
    int bx = x + w - f - 2 - bw, by = CONTENT_Y + 3;
    pd_title_box(c, bx, by, bw, bh, ui->pressedId == HIT_SETTINGS_BACK ? PD_TITLE_FILL : PD_LIST_FILL);
    pd_text(c, bx + 6, pd_text_y(by, bh), label, PD_TITLE_TEXT, PD_TITLE_SHADOW);
    ui_add_hit(ui, bx, by, bw, bh, HIT_SETTINGS_BACK);
}

// TAB BAR (C-03): every tab the player can put on the bar, SHOWN or HIDDEN.
// A tab this game doesn't have says N/A and keeps its setting for the games
// that do (the app's rule).
static void tabs_page(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g) {
    char count[16];
    snprintf(count, sizeof(count), "%d/%d", ui->tabsPicked, PD_BAR_TABS);
    page_title(ui, c, "TAB BAR", count);
    int x = MARGIN, w = c->w - 2 * MARGIN, y = LIST_Y;
    int lf = pd_list_box(c, x, y, w, c->h - y - MARGIN);
    int ix = x + lf + 4, iw = w - 2 * (lf + 4);
    y += lf;
    for (int i = 0; i < PD_TAB_ORDER_COUNT; i++) {
        enum pd_tab t = PD_TAB_ORDER[i];
        bool there = pd_tab_available(g, t);
        const char* value = !there ? "N/A" : pd_tab_shown(ui->settings, t) ? "SHOWN" : "HIDDEN";
        ui_row(c, ix, y, iw, pd_tab_label(t), value, ui->pressedId == HIT_SETTINGS_TAB + (int) t);
        if (there) ui_add_hit(ui, ix - 4, y, iw + 8, ROW_H, HIT_SETTINGS_TAB + (int) t);
        y += ROW_H;
    }
    pd_text_wrap(c, ix, y + 6, iw, 14, 2, "Hidden tabs open from MORE, left of the gear.", PD_MUTED,
                 PD_MUTED_SHADOW);
}

// TWEAKS (C-07): the app's CompanionTweaks, each on by default, with the grey
// line under it that says what it does.
static const char* const TWEAK_LABELS[PD_TWEAK_COUNT] = {
    [PD_TWEAK_PARTY_ICONS] = "PARTY ICONS MOVE",
    [PD_TWEAK_MAP_CURSOR] = "MAP CURSOR BLINK",
    [PD_TWEAK_TAB_ANIM] = "TAB ANIMATIONS",
    [PD_TWEAK_JUMP_BATTLE] = "JUMP TO BATTLE",
    [PD_TWEAK_BATTLE_HINTS] = "BATTLE HINTS",
};
static const char* const TWEAK_DESCS[PD_TWEAK_COUNT] = {
    [PD_TWEAK_PARTY_ICONS] = "Hold a party slot and its icon bounces",
    [PD_TWEAK_MAP_CURSOR] = "The MAP cursor pulses between two sizes",
    [PD_TWEAK_TAB_ANIM] = "Tabs slide in. Off: they switch at once",
    [PD_TWEAK_JUMP_BATTLE] = "A battle opens BATTLE, then goes back",
    [PD_TWEAK_BATTLE_HINTS] = "Move ratings and the best pick in battle",
};

static void tweaks_page(struct pd_ui* ui, struct pd_canvas* c) {
    page_title(ui, c, "TWEAKS", NULL);
    int x = MARGIN, w = c->w - 2 * MARGIN, y = LIST_Y;
    int lf = pd_list_box(c, x, y, w, c->h - y - MARGIN);
    int ix = x + lf + 4, iw = w - 2 * (lf + 4);
    y += lf;
    for (int i = 0; i < PD_TWEAK_COUNT; i++) {
        bool on = pd_tweak_on(ui->settings, (enum pd_tweak) i);
        ui_row(c, ix, y, iw, TWEAK_LABELS[i], on ? "ON" : "OFF", ui->pressedId == HIT_TWEAK + i);
        ui_add_hit(ui, ix - 4, y, iw + 8, ROW_H + 14, HIT_TWEAK + i);
        pd_text_fit(c, ix, y + ROW_H - 2, iw, TWEAK_DESCS[i], PD_MUTED, PD_MUTED_SHADOW);
        y += ROW_H + 14;
    }
}

// A tap on one of the two pages: a tab flipped, or a switch. Whether another
// chip still fits is the count the last draw worked out (ui->tabsPicked), the
// app's barCount over the tabs this game has.
bool ui_settings_act(struct pd_ui* ui, int id) {
    if (id == HIT_SETTINGS_BACK) {
        // TAB BAR and TWEAKS live behind COMPANION (C-SET-01).
        bool inner = ui->settingsPage == PD_SETTINGS_TABS || ui->settingsPage == PD_SETTINGS_TWEAKS;
        ui->settingsPage = inner ? PD_SETTINGS_COMPANION : PD_SETTINGS_HOME;
        return true;
    }
    if (id == HIT_SETTING + SETTING_SLOW) {
        ui->settings->slowMotion = !ui->settings->slowMotion;
        ui->action = PD_ACTION_SETTINGS_CHANGED;
        return true;
    }
    if (id == HIT_SETTING + SETTING_HINTS) {
        ui->settings->tweaks ^= PD_TWEAK_BIT(PD_TWEAK_BATTLE_HINTS);
        ui->action = PD_ACTION_SETTINGS_CHANGED;
        return true;
    }
    if (id >= HIT_SETTINGS_TAB && id < HIT_SETTINGS_TAB + PD_TAB_COUNT) {
        enum pd_tab t = (enum pd_tab) (id - HIT_SETTINGS_TAB);
        if (pd_tab_shown(ui->settings, t)) {
            ui->settings->tabsShown &= ~PD_TAB_BIT(t);
            ui->action = PD_ACTION_SETTINGS_CHANGED;
        } else if (ui->tabsPicked >= PD_BAR_TABS) {
            ui->overlay = PD_OVERLAY_TABS_FULL;
        } else {
            ui->settings->tabsShown |= PD_TAB_BIT(t);
            ui->action = PD_ACTION_SETTINGS_CHANGED;
        }
        return true;
    }
    if (id >= HIT_TWEAK && id < HIT_TWEAK + PD_TWEAK_COUNT) {
        ui->settings->tweaks ^= PD_TWEAK_BIT(id - HIT_TWEAK);
        ui->action = PD_ACTION_SETTINGS_CHANGED;
        return true;
    }
    return false;
}

void ui_settings_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                     const struct pd_host_info* host) {
    if (ui->settingsPage == PD_SETTINGS_TABS) {
        tabs_page(ui, c, g);
        return;
    }
    if (ui->settingsPage == PD_SETTINGS_TWEAKS) {
        tweaks_page(ui, c);
        return;
    }
    if (ui->settingsPage == PD_SETTINGS_SPEED) {
        speed_page(ui, c);
        return;
    }
    if (ui->settingsPage == PD_SETTINGS_COMPANION) {
        companion_page(ui, c);
        return;
    }
    if (ui->settingsPage == PD_SETTINGS_BATTLE) {
        battle_page(ui, c);
        return;
    }
    if (ui->settingsPage == PD_SETTINGS_SAVES) {
        saves_page(ui, c, host);
        return;
    }
    int x = MARGIN, w = c->w - 2 * MARGIN;
    int f = pd_title_box(c, x, CONTENT_Y, w, TITLE_H, PD_TITLE_FILL);
    int ty = pd_text_y(CONTENT_Y, TITLE_H);
    pd_text(c, x + f + 6, ty, "SETTINGS", PD_TITLE_TEXT, PD_TITLE_SHADOW);
    int right = x + w - f - 6;
    // The TRAINER CARD opens from here, and from its own chip once the player
    // puts it on the bar: a grey chip at the title window's right, white while
    // pressed, like the tab bar's.
    if (pd_tab_available(g, PD_TAB_CARD)) {
        const char* label = "TRAINER CARD";
        int bw = pd_text_width(label) + 12, bh = TITLE_H - 6;
        int bx = x + w - f - 2 - bw, by = CONTENT_Y + 3;
        pd_title_box(c, bx, by, bw, bh, ui->pressedId == HIT_OPEN_CARD ? PD_TITLE_FILL : PD_LIST_FILL);
        pd_text(c, bx + 6, pd_text_y(by, bh), label, PD_TITLE_TEXT, PD_TITLE_SHADOW);
        ui_add_hit(ui, bx, by, bw, bh, HIT_OPEN_CARD);
        right = bx - 8;
    }
    if (g->title) {
        int tx = x + f + 6 + pd_text_width("SETTINGS") + 12;
        int tw = pd_text_width(g->title);
        if (tw > right - tx) tw = right - tx;
        pd_text_fit(c, right - tw, ty, tw, g->title, PD_TITLE_TEXT, PD_TITLE_SHADOW);
    }

    int lf = pd_list_box(c, x, LIST_Y, w, c->h - LIST_Y - MARGIN);
    int ix = x + lf + 4, iw = w - 2 * (lf + 4);
    int y = LIST_Y + lf;
    char buf[40];
    if (s->valid) {
        const char* place = pd_mapsec_name(g, s->mapsec);
        char money[40];
        ui_format_money(s->money, money, sizeof(money));
        info_pair(c, ix, y, iw, "PLACE", *place ? place : "-", "MONEY", money);
        y += ROW_H;
    }
    for (int i = 0; host && i < host->count && i < PD_UI_HOST_LINES; i += 2) {
        bool two = i + 1 < host->count && i + 1 < PD_UI_HOST_LINES;
        info_pair(c, ix, y, iw, host->labels[i], host->values[i], two ? host->labels[i + 1] : NULL,
                  two ? host->values[i + 1] : NULL);
        y += ROW_H;
    }
    pd_fill(c, ix, y + 1, iw, 1, PD_DIVIDER);
    y += 4;

    setting(ui, c, ix, &y, iw, SETTING_SCREEN, "SCREEN", pd_screen_mode_name(ui->settings->screenMode));
    speed_label(ui->settings->gameSpeed, buf, sizeof(buf));
    page_row(ui, c, ix, &y, iw, SETTING_SPEED, "SPEED", buf);
    // The app's groups, each a page behind its row (C-SET-01). The two settings
    // a player changes most stay on this page.
    snprintf(buf, sizeof(buf), "%d/%d", ui->tabsPicked, PD_BAR_TABS);
    page_row(ui, c, ix, &y, iw, SETTING_COMPANION, "COMPANION", buf);
    page_row(ui, c, ix, &y, iw, SETTING_BATTLE, "BATTLE", pd_tweak_on(ui->settings, PD_TWEAK_BATTLE_HINTS) ? "HINTS ON" : "HINTS OFF");
    page_row(ui, c, ix, &y, iw, SETTING_SAVES, "SAVES", host && host->hasState ? host->stateWhen : NULL);
}

// The COMPANION page: the bar's own pick, then its small switches.
static void companion_page(struct pd_ui* ui, struct pd_canvas* c) {
    page_title(ui, c, "COMPANION", NULL);
    int x = MARGIN, w = c->w - 2 * MARGIN, y = LIST_Y;
    int lf = pd_list_box(c, x, y, w, c->h - y - MARGIN);
    int ix = x + lf + 4, iw = w - 2 * (lf + 4);
    y += lf;
    char buf[40];
    snprintf(buf, sizeof(buf), "%d/%d", ui->tabsPicked, PD_BAR_TABS);
    page_row(ui, c, ix, &y, iw, SETTING_TAB_BAR, "TAB BAR", buf);
    page_row(ui, c, ix, &y, iw, SETTING_TWEAKS, "TWEAKS", NULL);
}

// The BATTLE page: the hints switch (C-BT-08), where the app keeps it.
static void battle_page(struct pd_ui* ui, struct pd_canvas* c) {
    page_title(ui, c, "BATTLE", NULL);
    int x = MARGIN, w = c->w - 2 * MARGIN, y = LIST_Y;
    int lf = pd_list_box(c, x, y, w, c->h - y - MARGIN);
    int ix = x + lf + 4, iw = w - 2 * (lf + 4);
    y += lf;
    setting(ui, c, ix, &y, iw, SETTING_HINTS, "BATTLE HINTS",
            pd_tweak_on(ui->settings, PD_TWEAK_BATTLE_HINTS) ? "ON" : "OFF");
    ui_row(c, ix, y, iw, "Move ratings and the best pick", NULL, false);
    ui_add_hit(ui, ix - 4, y, iw + 8, ROW_H, SETTING_HINTS);
}

// The step's place in the two tables (the pick lists' cursor).
static int ff_index(int pct) {
    for (int i = 0; i < PD_FF_STEPS; i++) {
        if (PD_FF_SPEEDS[i] == pct) return i;
    }
    return 1;
}

static int speed_index(int pct) {
    for (int i = 0; i < PD_GAME_SPEEDS; i++) {
        if (PD_GAME_SPEED_STEPS[i] == pct) return i;
    }
    return 3;
}

// "2x" / "100%" / "INFINITE" for a speed in percent (0 = INFINITE).
static void speed_label(int pct, char* buf, size_t len) {
    if (pct == 0) snprintf(buf, len, "INFINITE");
    else if (pct % 100 == 0) snprintf(buf, len, "%dx", pct / 100);
    else if (pct % 10 == 0) snprintf(buf, len, "%d.%dx", pct / 100, pct % 100 / 10);
    else snprintf(buf, len, "%d.%02dx", pct / 100, pct % 100);
}

// The SPEED page (H-24, H-27, H-29): the fast-forward step, what the game stays
// at, and slow motion on a held L1.
static void speed_page(struct pd_ui* ui, struct pd_canvas* c) {
    page_title(ui, c, "SPEED", NULL);
    int x = MARGIN, w = c->w - 2 * MARGIN, y = LIST_Y;
    int lf = pd_list_box(c, x, y, w, c->h - y - MARGIN);
    int ix = x + lf + 4, iw = w - 2 * (lf + 4);
    y += lf;
    char buf[24];
    speed_label(ui->settings->ffSpeed, buf, sizeof(buf));
    setting(ui, c, ix, &y, iw, SETTING_FF, "FAST-FORWARD", buf);
    speed_label(ui->settings->gameSpeed, buf, sizeof(buf));
    setting(ui, c, ix, &y, iw, SETTING_GAME_SPEED, "GAME SPEED", buf);
    setting(ui, c, ix, &y, iw, SETTING_SLOW, "SLOW MOTION (L1)",
            ui->settings->slowMotion ? "ON" : "OFF");
}

// The SAVES page: the one state slot, RESTART GAME and LEAVE GAME (C-SET-04).
static void saves_page(struct pd_ui* ui, struct pd_canvas* c, const struct pd_host_info* host) {
    page_title(ui, c, "SAVES", NULL);
    int x = MARGIN, w = c->w - 2 * MARGIN, y = LIST_Y;
    int lf = pd_list_box(c, x, y, w, c->h - y - MARGIN);
    int ix = x + lf + 4, iw = w - 2 * (lf + 4);
    y += lf;
    setting(ui, c, ix, &y, iw, SETTING_SAVE_STATE, "SAVE STATE", NULL);
    if (host && host->hasState) {
        setting(ui, c, ix, &y, iw, SETTING_LOAD_STATE, "LOAD STATE", host->stateWhen);
    } else {
        // Nothing to load: shown, not tappable.
        pd_text(c, ix, pd_text_y(y, ROW_H), "LOAD STATE", PD_MUTED, PD_MUTED_SHADOW);
        pd_text(c, ix + iw * 42 / 100, pd_text_y(y, ROW_H), "none yet", PD_MUTED, PD_MUTED_SHADOW);
        y += ROW_H;
    }
    setting(ui, c, ix, &y, iw, SETTING_RESTART, "RESTART GAME", NULL);
    setting(ui, c, ix, &y, iw, SETTING_LEAVE, "LEAVE GAME", NULL);
}

// --- overlays ---

static void title_window(struct pd_canvas* c, int y, const char* title) {
    int x = MARGIN, w = c->w - 2 * MARGIN;
    int f = pd_title_box(c, x, y, w, TITLE_H, PD_TITLE_FILL);
    pd_text(c, x + f + 6, pd_text_y(y, TITLE_H), title, PD_TITLE_TEXT, PD_TITLE_SHADOW);
}

static void pick_list(struct pd_ui* ui, struct pd_canvas* c, const char* title, const char* const* options, int n,
                      int current) {
    int x = MARGIN, w = c->w - 2 * MARGIN;
    // The nine fast-forward choices need a little more vertical room so the
    // last row and the bottom of its frame stay on the 240-pixel screen.
    int y = CONTENT_Y + (n >= PD_FF_STEPS ? 2 : 8);
    title_window(c, y, title);
    y += TITLE_H + 4;
    int lf = pd_list_box(c, x, y, w, n * ROW_H + 2 * 7 + 4);
    int ix = x + lf + 4, iw = w - 2 * (lf + 4);
    int ry = y + lf + 2;
    for (int i = 0; i < n; i++) {
        bool on = i == current || ui->pressedId == HIT_OPTION + i;
        if (on) pd_fill(c, ix - 4, ry, iw + 8, ROW_H, PD_ROW_SELECTED);
        pd_text(c, ix + 10, pd_text_y(ry, ROW_H), options[i], on ? PD_VALUE : PD_LABEL,
                on ? PD_VALUE_SHADOW : PD_LABEL_SHADOW);
        if (i == current) pd_triangle(c, ix, ry + 4, 5, PD_RIGHT, PD_VALUE);
        ui_add_hit(ui, ix - 4, ry, iw + 8, ROW_H, HIT_OPTION + i);
        ry += ROW_H;
    }
}

static void confirm(struct pd_ui* ui, struct pd_canvas* c, const char* title, const char* text, const char* yes) {
    int x = MARGIN, w = c->w - 2 * MARGIN;
    int y = CONTENT_Y + 8;
    title_window(c, y, title);
    y += TITLE_H + 4;
    int h = 3 * 16 + 2 * 7 + 6;
    int lf = pd_list_box(c, x, y, w, h);
    pd_text_wrap(c, x + lf + 6, y + lf + 2, w - 2 * (lf + 6), 16, 3, text, PD_LABEL, PD_LABEL_SHADOW);
    y += h + 6;
    int bw = 96;
    ui_button(ui, c, c->w / 2 - bw - 6, y, bw, 24, yes, HIT_YES);
    ui_button(ui, c, c->w / 2 + 6, y, bw, 24, "CANCEL", HIT_NO);
}

// A black-out window that only says something: the app's OK-only dialog
// (TAB BAR IS FULL).
static void notice(struct pd_ui* ui, struct pd_canvas* c, const char* title, const char* text) {
    int x = MARGIN, w = c->w - 2 * MARGIN;
    int y = CONTENT_Y + 8;
    title_window(c, y, title);
    y += TITLE_H + 4;
    int h = 3 * 16 + 2 * 7 + 6;
    int lf = pd_list_box(c, x, y, w, h);
    pd_text_wrap(c, x + lf + 6, y + lf + 2, w - 2 * (lf + 6), 16, 3, text, PD_LABEL, PD_LABEL_SHADOW);
    y += h + 6;
    int bw = 96;
    ui_button(ui, c, c->w / 2 - bw / 2, y, bw, 24, "OK", HIT_YES);
}

void ui_overlay(struct pd_ui* ui, struct pd_canvas* c) {
    if (ui->overlay == PD_OVERLAY_NONE) return;
    pd_dim(c, 0xE6);
    // Swallows taps beside the windows (they close the overlay).
    ui_add_hit(ui, 0, 0, c->w, c->h, HIT_SCRIM);
    static char ffLabels[PD_FF_STEPS][24];
    static char gameLabels[PD_GAME_SPEEDS][24];
    const char* ffOptions[PD_FF_STEPS];
    const char* gameOptions[PD_GAME_SPEEDS];
    for (int i = 0; i < PD_FF_STEPS; i++) {
        speed_label(PD_FF_SPEEDS[i], ffLabels[i], sizeof(ffLabels[i]));
        ffOptions[i] = ffLabels[i];
    }
    for (int i = 0; i < PD_GAME_SPEEDS; i++) {
        speed_label(PD_GAME_SPEED_STEPS[i], gameLabels[i], sizeof(gameLabels[i]));
        gameOptions[i] = gameLabels[i];
    }
    switch (ui->overlay) {
    case PD_OVERLAY_PICK_SCREEN:
        pick_list(ui, c, "SCREEN", SCREEN_NAMES, PD_SCREEN_MODES, ui->settings->screenMode);
        break;
    case PD_OVERLAY_PICK_FF:
        pick_list(ui, c, "FAST-FORWARD", ffOptions, PD_FF_STEPS, ff_index(ui->settings->ffSpeed));
        break;
    case PD_OVERLAY_PICK_SPEED:
        pick_list(ui, c, "GAME SPEED", gameOptions, PD_GAME_SPEEDS,
                  speed_index(ui->settings->gameSpeed));
        break;
    case PD_OVERLAY_TABS_FULL: {
        char text[96];
        snprintf(text, sizeof(text), "Up to %d tabs fit next to the gear. Hide one first; hidden tabs open from MORE.",
                 PD_BAR_TABS);
        notice(ui, c, "TAB BAR IS FULL", text);
        break;
    }
    case PD_OVERLAY_CONFIRM_SAVE:
        confirm(ui, c, "SAVE STATE",
                "Save a state of the game as it is now? It replaces the last one. The save file isn't touched.",
                "SAVE");
        break;
    case PD_OVERLAY_CONFIRM_LOAD:
        confirm(ui, c, "LOAD STATE",
                "Go back to the saved state? Play since then is lost; the save file isn't touched.", "LOAD");
        break;
    case PD_OVERLAY_CONFIRM_RESTART:
        // C-SET-04: the app's wording, in the port's terms.
        confirm(ui, c, "RESTART GAME",
                "Reboot the game and load its save from the card again? Anything since that save is lost, and so is this moment.",
                "RESTART");
        break;
    case PD_OVERLAY_CONFIRM_LEAVE:
        // The resume wording: the port picks this moment up next time.
        confirm(ui, c, "LEAVE GAME",
                "Back to the game list? The game's save is untouched and this moment is kept: the port carries on from it next time.",
                "LEAVE");
        break;
    default:
        break;
    }
}
