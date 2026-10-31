// POKéDEX: the list (national or the game's regional dex) with the save's
// seen / caught marks, and an entry page read from the ROM - the app's
// PokedexScreen, fitted to 320x212.
#include <stdio.h>
#include <string.h>

#include "pd_dex.h"
#include "pd_tables.h"
#include "pd_ui_internal.h"

#define TITLE_H 24
#define FILTER_Y (CONTENT_Y + TITLE_H + 2)
#define FILTER_H 18
#define LIST_Y (FILTER_Y + FILTER_H + 2)
#define DEX_ROW PD_DEX_ROW_H
#define TOGGLE_W 78

// Whether the National Dex shows: the player's pick, else the save's own.
static bool national_view(const struct pd_ui* ui, const struct pd_snapshot* s) {
    return ui->dexNational >= 0 ? ui->dexNational : s->dexNational;
}

// The dex's filters (the app's DexFilter): everything, caught, seen but not
// caught, never seen.
enum dex_filter { DEX_ALL, DEX_CAUGHT, DEX_SEEN, DEX_UNSEEN, DEX_FILTERS };
static const char* const FILTER_LABELS[DEX_FILTERS] = { "ALL", "CAUGHT", "SEEN ONLY", "NOT SEEN" };

// Whether [nat] belongs on the list under [filter] - the app's DexUiState.entries.
static bool filter_takes(const struct pd_snapshot* s, int nat, int filter) {
    bool seen = pd_dex_seen(s, nat), caught = pd_dex_caught(s, nat);
    switch (filter) {
    case DEX_CAUGHT: return caught;
    case DEX_SEEN: return seen && !caught;
    case DEX_UNSEEN: return !seen;
    default: return true;
    }
}

// The list: national numbers in the order shown, through the filter; returns
// how many.
static int dex_list(const struct pd_game* g, const struct pd_snapshot* s, bool national, int filter, int* out) {
    int n = 0;
    if (national) {
        for (int i = 1; i <= PD_NATIONAL_COUNT; i++) {
            if (pd_dex_species(g, i) && filter_takes(s, i, filter)) out[n++] = i;
        }
    } else {
        for (int i = 1; i <= g->cfg->regionalCount; i++) {
            int nat = pd_dex_regional(g, i);
            if (nat && filter_takes(s, nat, filter)) out[n++] = nat;
        }
    }
    return n;
}

// The number shown for an entry: its regional position in a regional dex.
static int shown_number(const struct pd_game* g, bool national, int nat) {
    if (national) return nat;
    for (int i = 1; i <= g->cfg->regionalCount; i++) {
        if (pd_dex_regional(g, i) == nat) return i;
    }
    return nat;
}

// The whole dex's entry, seen and caught counts. The filter leaves the title's
// numbers and the filters' own counts alone (V-09).
static void dex_counts(const struct pd_game* g, const struct pd_snapshot* s, bool national, int* all, int* seen,
                       int* caught) {
    int n = 0, se = 0, ca = 0;
    for (int i = 1; i <= (national ? PD_NATIONAL_COUNT : g->cfg->regionalCount); i++) {
        int nat = national ? i : pd_dex_regional(g, i);
        if (!nat || !pd_dex_species(g, nat)) continue;
        n++;
        se += pd_dex_seen(s, nat);
        ca += pd_dex_caught(s, nat);
    }
    *all = n;
    *seen = se;
    *caught = ca;
}

// A section chip (INFO / EVOLVE, and the dex's filters): white with red text
// when open, like the tab bar's, grey otherwise.
static void section_chip(struct pd_ui* ui, struct pd_canvas* c, int x, int y, int w, int h, const char* label,
                         int id, bool open) {
    pd_title_box(c, x, y, w, h, open || ui->pressedId == id ? PD_TITLE_FILL : PD_LIST_FILL);
    int tw = pd_text_width(label);
    pd_text(c, x + (w - tw) / 2, pd_text_y(y, h), label, open ? PD_VALUE : PD_TITLE_TEXT,
            open ? PD_VALUE_SHADOW : PD_TITLE_SHADOW);
    ui_add_hit(ui, x, y, w, h, id);
}

static void title(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                  bool national) {
    int x = MARGIN, w = c->w - 2 * MARGIN - TOGGLE_W - 4;
    int f = pd_title_box(c, x, CONTENT_Y, w, TITLE_H, PD_TITLE_FILL);
    int ty = pd_text_y(CONTENT_Y, TITLE_H);
    pd_text(c, x + f + 6, ty, "POKéDEX", PD_TITLE_TEXT, PD_TITLE_SHADOW);
    int all, seen, caught;
    dex_counts(g, s, national, &all, &seen, &caught);
    char counts[40];
    snprintf(counts, sizeof(counts), "SEEN %d  OWN %d", seen, caught);
    pd_text_right(c, x + w - f - 6, ty, counts, PD_VALUE, PD_VALUE_SHADOW);
    // Which dex: tap for the other one.
    char other[24];
    snprintf(other, sizeof(other), "%s", national ? g->cfg->regionName : "NATIONAL");
    ui_button(ui, c, x + w + 4, CONTENT_Y, TOGGLE_W, TITLE_H, other, HIT_DEX_TOGGLE);
}

// The four filters with how many entries each one leaves (the app's counts):
// ALL, CAUGHT, SEEN ONLY, NOT SEEN. The counts are the whole dex's, so they
// stay put while the filter changes (V-09).
static void filter_row(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                       bool national) {
    int all, seen, caught;
    dex_counts(g, s, national, &all, &seen, &caught);
    int have[DEX_FILTERS] = { all, caught, seen - caught, all - seen };
    int x = MARGIN, right = c->w - MARGIN;
    for (int i = 0; i < DEX_FILTERS; i++) {
        char label[32];
        snprintf(label, sizeof(label), "%s %d", FILTER_LABELS[i], have[i]);
        int w = pd_text_width(label) + 6;
        // The row is even: the chips run from the left edge and the last one
        // ends at the right one.
        int cx = i == DEX_FILTERS - 1 ? right - w : x;
        section_chip(ui, c, cx, FILTER_Y, w, FILTER_H, label, HIT_DEX_FILTER + i, ui->dexFilter == i);
        x += w + 4;
    }
}


// The list shown (rebuilt each draw), kept for the tap handler, which has no
// game at hand: the toggle flips what shows, up / down step through it.
static int lastList[PD_NATIONAL_COUNT], lastCount;
static bool lastNational;

int pd_ui_dex_row_count(void) {
    return lastCount;
}

int pd_ui_dex_row_national(int index) {
    return index >= 0 && index < lastCount ? lastList[index] : 0;
}

int pd_ui_dex_row_index(int national) {
    for (int i = 0; i < lastCount; i++) {
        if (lastList[i] == national) return i;
    }
    return -1;
}

static void list_view(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    const int* list = lastList;
    int n = lastCount;
    bool national = lastNational;
    title(ui, c, g, s, national);
    filter_row(ui, c, g, s, national);

    int x = MARGIN, w = c->w - 2 * MARGIN;
    int f = pd_list_box(c, x, LIST_Y, w, c->h - LIST_Y - MARGIN);
    int ix = x + f, iy = LIST_Y + f + 1, iw = w - 2 * f, ih = c->h - LIST_Y - MARGIN - 2 * f - 2;
    ui_add_scroll_hit(ui, ix, iy, iw, ih, HIT_DEX_LIST);
    ui->dexScroll = ui_clamp_scroll(ui->dexScroll, n * DEX_ROW, ih);
    pd_clip(c, ix, iy, iw, ih);
    char buf[40], num[16];
    for (int i = ui->dexScroll / DEX_ROW; i < n; i++) {
        int ry = iy + i * DEX_ROW - ui->dexScroll;
        if (ry >= iy + ih) break;
        int nat = list[i];
        bool seen = pd_dex_seen(s, nat), caught = pd_dex_caught(s, nat);
        if (ui->pressedId == HIT_DEX_ROW + nat) pd_fill(c, ix, ry, iw, DEX_ROW, PD_ROW_SELECTED);
        if (seen) ui_ball(c, ix + 6, ry + 4, caught);
        snprintf(num, sizeof(num), "No.%03d", shown_number(g, national, nat));
        int ty = pd_text_y(ry, DEX_ROW);
        pd_text(c, ix + 22, ty, num, PD_MUTED, PD_MUTED_SHADOW);
        const char* name = pd_species_name(pd_dex_species(g, nat), buf, sizeof(buf));
        pd_text(c, ix + 74, ty, name, seen ? PD_LABEL : PD_MUTED, seen ? PD_LABEL_SHADOW : PD_MUTED_SHADOW);
        int top = ry < iy ? iy : ry;
        int bottom = ry + DEX_ROW > iy + ih ? iy + ih : ry + DEX_ROW;
        ui_add_scroll_hit(ui, ix, top, iw, bottom - top, HIT_DEX_ROW + nat);
    }
    pd_unclip(c);
    ui_scroll_bar(c, ix + iw - 4, iy, ih, ui->dexScroll, n * DEX_ROW, PD_MUTED);
}

static void draw_sprite(struct pd_canvas* c, int x, int y, const uint32_t* px) {
    for (int py = 0; py < PD_SPRITE; py++) {
        for (int pxx = 0; pxx < PD_SPRITE; pxx++) {
            uint32_t v = px[py * PD_SPRITE + pxx];
            if (v != 0xFF000000u) pd_fill(c, x + pxx, y + py, 1, 1, v);
        }
    }
}

// The entry's bottom row: its sections on the left (the app's DexSectionChips),
// stepping through the list and BACK on the right. Between them the entry's
// place in that list ("12 / 386", the app's counter beside the arrows).
static void entry_buttons(struct pd_ui* ui, struct pd_canvas* c, int x, int y, int w, int h, int f, bool evos) {
    int byBtn = y + h - f - 26;
    int sx = x + f + 2;
    section_chip(ui, c, sx, byBtn, 40, 22, "INFO", HIT_DEX_INFO, ui->dexSection == 0);
    if (evos) section_chip(ui, c, sx + 43, byBtn, 58, 22, "EVOLVE", HIT_DEX_EVOLVE, ui->dexSection == 1);
    int arrows = x + w - f - 70 - 2 * 40;
    for (int i = 0; i < lastCount; i++) {
        if (lastList[i] != ui->dexOpen) continue;
        char at[24];
        snprintf(at, sizeof(at), "%d / %d", i + 1, lastCount);
        pd_text_right(c, arrows - 8, byBtn + 3, at, PD_MUTED, PD_MUTED_SHADOW);
        break;
    }
    ui_button(ui, c, arrows, byBtn, 36, 22, "", HIT_DEX_PREV);
    pd_triangle(c, arrows + 13, byBtn + 7, 6, PD_UP, PD_TITLE_TEXT);
    ui_button(ui, c, x + w - f - 70 - 40, byBtn, 36, 22, "", HIT_DEX_NEXT);
    pd_triangle(c, x + w - f - 70 - 40 + 13, byBtn + 8, 6, PD_DOWN, PD_TITLE_TEXT);
    ui_button(ui, c, x + w - f - 66, byBtn, 62, 22, "BACK", HIT_BACK);
}

static void info_view(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                      const struct pd_dex_entry* e, const uint32_t* sprite, bool spriteOk) {
    bool national = lastNational;
    char buf[64], buf2[40];
    int x = MARGIN, y = CONTENT_Y, w = c->w - 2 * MARGIN, h = c->h - CONTENT_Y - MARGIN;
    int f = pd_title_box(c, x, y, w, h, PD_TITLE_FILL);
    int ix = x + f + 4, iw = w - 2 * (f + 4);
    int top = y + f + 2;

    // The sprite on the list window's grey, framed.
    int bx = ix, by = top;
    pd_list_box(c, bx, by, PD_SPRITE + 16, PD_SPRITE + 16);
    if (spriteOk) draw_sprite(c, bx + 8, by + 8, sprite);

    int tx = bx + PD_SPRITE + 24, tw = ix + iw - tx;
    snprintf(buf, sizeof(buf), "No.%03d", shown_number(g, national, e->national));
    pd_text(c, tx, top, buf, PD_MUTED, PD_MUTED_SHADOW);
    int nw = pd_text(c, tx + 50, top, pd_species_name(e->species, buf2, sizeof(buf2)), PD_LABEL, PD_LABEL_SHADOW);
    if (pd_dex_seen(s, e->national)) ui_ball(c, tx + 50 + nw + 6, top + 4, pd_dex_caught(s, e->national));
    snprintf(buf, sizeof(buf), "%s POKéMON", e->category);
    pd_text_fit(c, tx, top + 18, tw, buf, PD_MUTED, PD_MUTED_SHADOW);
    if (e->type2 != e->type1 && e->type2 != PD_TYPE_NONE) {
        snprintf(buf, sizeof(buf), "%s / %s", pd_type_name(e->type1), pd_type_name(e->type2));
    } else {
        snprintf(buf, sizeof(buf), "%s", pd_type_name(e->type1));
    }
    pd_text(c, tx, top + 36, buf, PD_VALUE, PD_VALUE_SHADOW);
    char ht[16], wt[20];
    pd_dex_height(e->heightDm, ht, sizeof(ht));
    pd_dex_weight(e->weightHg, wt, sizeof(wt));
    snprintf(buf, sizeof(buf), "HT %s   WT %s", ht, wt);
    pd_text_fit(c, tx, top + 54, tw, buf, PD_LABEL, PD_LABEL_SHADOW);

    int dy = by + PD_SPRITE + 20;
    pd_fill(c, ix, dy - 2, iw, 1, PD_DIVIDER);
    pd_text_wrap(c, ix, dy, iw, 16, 3, e->description, PD_LABEL, PD_LABEL_SHADOW);
    dy += 3 * 16 + 2;
    static const char* const STAT[6] = { "HP", "ATK", "DEF", "SPA", "SPD", "SPE" };
    int sx = ix;
    for (int i = 0; i < 6; i++) {
        sx += pd_text(c, sx, dy, STAT[i], PD_MUTED, PD_MUTED_SHADOW) + 3;
        snprintf(buf, sizeof(buf), "%d", e->stats[i]);
        sx += pd_text(c, sx, dy, buf, PD_LABEL, PD_LABEL_SHADOW) + 9;
    }
    dy += 18;
    if (e->ability2[0]) {
        snprintf(buf, sizeof(buf), "%s / %s", e->ability1, e->ability2);
    } else {
        snprintf(buf, sizeof(buf), "%s", e->ability1);
    }
    int aw = pd_text(c, ix, dy, "ABILITY", PD_MUTED, PD_MUTED_SHADOW);
    pd_text_fit(c, ix + aw + 6, dy, iw - aw - 150, buf, PD_LABEL, PD_LABEL_SHADOW);

    // Step through the list (wrapping) or go back to it; the sections on the left.
    entry_buttons(ui, c, x, y, w, h, f, pd_dex_has_evolutions(g));
}

// The EVOLVE section (the app's DexEvolveSection): the whole family, both ways,
// each link over how it happens. Cached per species, like the entry's sprite.
static struct pd_evo_link evoLinks[PD_EVO_MAX];
static int evoNational[PD_EVO_MAX];
static int evoCount, evoSpecies = -1;
static unsigned evoRomId;

static void load_family(const struct pd_game* g, const struct pd_dex_entry* e) {
    if (evoSpecies == e->species && evoRomId == pd_rom_id(g)) return;
    evoSpecies = e->species;
    evoRomId = pd_rom_id(g);
    evoCount = pd_dex_family(g, e->species, evoLinks, PD_EVO_MAX);
    if (evoCount < 0) evoCount = 0;
    for (int i = 0; i < evoCount; i++) {
        evoNational[i] = pd_dex_national(g, evoLinks[i].to);
    }
}

#define EVO_LINK_ROW 30
static void evolve_view(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_dex_entry* e) {
    load_family(g, e);
    char buf[64];
    int x = MARGIN, y = CONTENT_Y, w = c->w - 2 * MARGIN, h = c->h - CONTENT_Y - MARGIN;
    int f = pd_title_box(c, x, y, w, h, PD_TITLE_FILL);
    int ix = x + f + 4, iy = y + f + 2, iw = w - 2 * (f + 4);
    int ih = y + h - f - 26 - iy - 2; // down to the section chips' row
    pd_text(c, ix, iy, pd_species_name(e->species, buf, sizeof(buf)), PD_LABEL, PD_LABEL_SHADOW);
    pd_text_right(c, ix + iw, iy, "FAMILY", PD_MUTED, PD_MUTED_SHADOW);
    int ly = iy + 18;
    ih -= 18;
    if (evoCount == 0) {
        pd_text(c, ix, ly + 8, "Doesn't evolve.", PD_MUTED, PD_MUTED_SHADOW);
    } else {
        ui_add_scroll_hit(ui, ix, ly, iw, ih, HIT_DEX_EVO_LIST);
        ui->dexEvoScroll = ui_clamp_scroll(ui->dexEvoScroll, evoCount * EVO_LINK_ROW, ih);
        pd_clip(c, ix, ly, iw, ih);
        for (int i = ui->dexEvoScroll / EVO_LINK_ROW; i < evoCount; i++) {
            int ry = ly + i * EVO_LINK_ROW - ui->dexEvoScroll;
            if (ry >= ly + ih) break;
            const struct pd_evo_link* l = &evoLinks[i];
            char from[24], to[24], how[64];
            int top = ry < ly ? ly : ry;
            int bottom = ry + EVO_LINK_ROW > ly + ih ? ly + ih : ry + EVO_LINK_ROW;
            ui_add_scroll_hit(ui, ix, top, iw, bottom - top, HIT_DEX_EVO_ROW + i);
            // The species this page is open on in the value red, the rest plain
            // (the app's FamilyMon), with an ASCII arrow - the font has no "→".
            const char* fn = pd_species_name(l->from, from, sizeof(from));
            const char* tn = pd_species_name(l->to, to, sizeof(to));
            bool fromHere = l->from == e->species, toHere = l->to == e->species;
            int fw = pd_text(c, ix, ry + 1, fn, fromHere ? PD_VALUE : PD_LABEL,
                             fromHere ? PD_VALUE_SHADOW : PD_LABEL_SHADOW);
            int aw = pd_text(c, ix + fw + 3, ry + 1, "->", PD_MUTED, PD_MUTED_SHADOW);
            pd_text_fit(c, ix + fw + aw + 6, ry + 1, iw - (fw + aw + 12), tn, toHere ? PD_VALUE : PD_LABEL,
                        toHere ? PD_VALUE_SHADOW : PD_LABEL_SHADOW);
            pd_evo_how(g, l->method, l->param, how, sizeof(how));
            pd_text_fit(c, ix + 8, ry + 17, iw - 14, how, PD_MUTED, PD_MUTED_SHADOW);
            pd_fill(c, ix, ry + EVO_LINK_ROW - 2, iw, 1, PD_DIVIDER);
        }
        pd_unclip(c);
        ui_scroll_bar(c, ix + iw - 4, ly, ih, ui->dexEvoScroll, evoCount * EVO_LINK_ROW, PD_MUTED);
    }
    entry_buttons(ui, c, x, y, w, h, f, true);
}

static void entry_view(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    static struct pd_dex_entry e;
    static uint32_t sprite[PD_SPRITE * PD_SPRITE];
    static bool spriteOk;
    static int loaded;
    static unsigned loadedRom;
    if (loaded != ui->dexOpen || loadedRom != pd_rom_id(g)) {
        pd_dex_entry(g, ui->dexOpen, &e);
        spriteOk = pd_dex_sprite(g, e.species, sprite);
        loaded = ui->dexOpen;
        loadedRom = pd_rom_id(g);
    }
    // A game without the evolution table has no EVOLVE section (LeafGreen's
    // RAM-only config, say).
    if (!pd_dex_has_evolutions(g)) ui->dexSection = 0;
    if (ui->dexSection == 1) {
        evolve_view(ui, c, g, &e);
    } else {
        info_view(ui, c, g, s, &e, sprite, spriteOk);
    }
}

void ui_dex_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    if (!pd_dex_available(g)) {
        ui_notice(c, "NO POKéDEX YET", "This version's POKéDEX tables", "aren't in the companion yet.");
        return;
    }
    if (!s->dexOk) {
        ui_notice(c, "NO POKéDEX YET", "The POKéDEX shows once the game", "has loaded a save.");
        return;
    }
    lastNational = national_view(ui, s);
    if (ui->dexFilter < 0 || ui->dexFilter >= DEX_FILTERS) ui->dexFilter = DEX_ALL;
    lastCount = dex_list(g, s, lastNational, ui->dexFilter, lastList);
    if (ui->dexOpen) {
        entry_view(ui, c, g, s);
    } else {
        list_view(ui, c, g, s);
    }
}

bool ui_dex_act(struct pd_ui* ui, int id) {
    if (ui->tab != PD_TAB_DEX || ui->overlay != PD_OVERLAY_NONE) return false;
    if (id == HIT_DEX_TOGGLE) {
        // Flip from what shows now (the save's default until something was picked).
        ui->dexNational = !lastNational;
        ui->dexScroll = 0;
        return true;
    }
    if (id >= HIT_DEX_ROW && id <= HIT_DEX_ROW + PD_NATIONAL_COUNT) {
        ui->dexOpen = id - HIT_DEX_ROW;
        ui->dexSection = 0;
        ui->dexEvoScroll = 0;
        return true;
    }
    if (id >= HIT_DEX_FILTER && id < HIT_DEX_FILTER + DEX_FILTERS) {
        // A filter chip: the list starts again from its top.
        ui->dexFilter = id - HIT_DEX_FILTER;
        ui->dexScroll = 0;
        return true;
    }
    if (id == HIT_DEX_INFO || id == HIT_DEX_EVOLVE) {
        ui->dexSection = id == HIT_DEX_EVOLVE;
        ui->dexEvoScroll = 0;
        return true;
    }
    if (id >= HIT_DEX_EVO_ROW && id < HIT_DEX_EVO_ROW + PD_EVO_MAX) {
        // The other end of a family link opens its own page (the app's tap).
        int i = id - HIT_DEX_EVO_ROW;
        if (i < evoCount && evoNational[i] > 0) {
            ui->dexOpen = evoNational[i];
            ui->dexEvoScroll = 0;
            return true;
        }
        return false;
    }
    if ((id == HIT_DEX_PREV || id == HIT_DEX_NEXT) && lastCount > 0) {
        int at = 0;
        for (int i = 0; i < lastCount; i++) {
            if (lastList[i] == ui->dexOpen) at = i;
        }
        at = (at + lastCount + (id == HIT_DEX_NEXT ? 1 : -1)) % lastCount;
        ui->dexOpen = lastList[at];
        ui->dexEvoScroll = 0;
        return true;
    }
    return false;
}
