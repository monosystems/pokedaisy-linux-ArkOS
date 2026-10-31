// BAG: one pocket at a time in the game's own pocket order, its items in a
// list a drag scrolls, and the tapped item's description - in each game's bag
// colours (FireRedBag / EmeraldBag in the app's ItemsScreen.kt).
#include <stdio.h>
#include <string.h>

#include "pd_dex.h"
#include "pd_tables.h"
#include "pd_ui_internal.h"

struct bag_palette {
    struct pd_layer panel[4];
    int panelLayers;
    int tabLayers; // the header reuses this many panel layers, filled with the next one's colour
    uint32_t fill, divider; // divider 0 = none
    uint32_t text, textShadow;
    uint32_t tabText, tabTextShadow;
    uint32_t cursor;
    struct pd_layer desc[2];
    int descLayers;
    uint32_t descFill, descText, descTextShadow;
    uint32_t backdropA, backdropB; // 0 = the companion's own backdrop
};

static const struct bag_palette FIRERED_BAG = {
    .panel = { { 0x686868, 2 }, { 0xE8E0A8, 1 }, { 0xF0C870, 2 }, { 0xD0B050, 2 } },
    .panelLayers = 4,
    .tabLayers = 2,
    .fill = 0xF8F8C8, .divider = 0xE8E0A8,
    .text = 0x606060, .textShadow = 0xD0D0C8,
    .tabText = 0xF8F8F8, .tabTextShadow = 0x606060,
    .cursor = 0xD88848,
    .desc = { { 0x005070, 2 }, { 0x10A8D8, 1 } },
    .descLayers = 2,
    .descFill = 0x0078C0, .descText = 0xF8F8F8, .descTextShadow = 0x606060,
};

static const struct bag_palette EMERALD_BAG = {
    .panel = { { 0x335170, 2 }, { 0x2C3941, 1 }, { 0x636372, 1 }, { 0xFBE898, 2 } },
    .panelLayers = 4,
    .tabLayers = 3,
    .fill = 0xFFFFD3, .divider = 0,
    .text = 0x000000, .textShadow = 0xD6D6CF,
    .tabText = 0x000000, .tabTextShadow = 0xB89D63,
    .cursor = 0xEC5F2A,
    .desc = { { 0x7B7B7B, 1 } },
    .descLayers = 1,
    .descFill = 0xFFFFFF, .descText = 0x000000, .descTextShadow = 0xD6D6CF,
    .backdropA = 0xC871F7, .backdropB = 0x5882F7,
};

#define TAB_W 74
#define TAB_H 17
#define TAB_GAP 2
#define LIST_X (MARGIN + TAB_W + 6)
#define LIST_Y CONTENT_Y
#define DESC_Y 160
#define BAG_ROW PD_BAG_ROW_H
#define FIRST_HM 339 // ITEM_HM01..ITEM_HM08: no quantity, like the game
#define LAST_HM 346

// A row of the list: which pocket's which item - the tabs and the order pick
// the rows, so a row can be found again after either changes.
struct bag_row {
    int pocket, index;
};
#define BAG_ROWS (PD_POCKET_COUNT * PD_POCKET_SLOTS)
static struct bag_row rows[BAG_ROWS];
static int rowCount;
// What the last draw left for the tap handler, which has no game at hand: the
// tabs it drew, this ROM's id, and the chosen item's id.
static int tabIds[PD_POCKET_COUNT + 2], tabCount;
static unsigned lastRom;
static int selItemId;

// --- favorites (C-B-06) ---

// This ROM's starred items (the settings keep one list per ROM), or NULL;
// create takes a free slot the first time something is starred.
static struct pd_favs* favs(struct pd_ui* ui, unsigned rom, bool create) {
    if (!ui->settings || !rom) return NULL;
    for (int i = 0; i < PD_FAV_ROMS; i++) {
        if (ui->settings->favs[i].rom == rom) return &ui->settings->favs[i];
    }
    if (!create) return NULL;
    for (int i = 0; i < PD_FAV_ROMS; i++) {
        struct pd_favs* f = &ui->settings->favs[i];
        if (f->rom) continue;
        memset(f, 0, sizeof(*f));
        f->rom = rom;
        return f;
    }
    return NULL; // four games already keep a list
}

static bool is_favorite(struct pd_ui* ui, unsigned rom, int item) {
    const struct pd_favs* f = favs(ui, rom, false);
    if (!f) return false;
    for (int i = 0; i < f->count; i++) {
        if (f->items[i] == item) return true;
    }
    return false;
}

// Stars the item, or takes its star away; false when the list is full.
static bool toggle_favorite(struct pd_ui* ui, unsigned rom, int item) {
    struct pd_favs* f = favs(ui, rom, true);
    if (!f) return false;
    for (int i = 0; i < f->count; i++) {
        if (f->items[i] != item) continue;
        f->items[i] = f->items[--f->count];
        return true;
    }
    if (f->count >= PD_FAV_MAX) return false;
    f->items[f->count++] = (unsigned short) item;
    return true;
}

// Whether anything starred is still in the bag: the FAVORITES tab comes and
// goes with it (the app's rule).
static bool favs_in_bag(struct pd_ui* ui, unsigned rom, const struct pd_snapshot* s) {
    const struct pd_favs* f = favs(ui, rom, false);
    for (int i = 0; f && i < f->count; i++) {
        for (int p = 0; p < PD_POCKET_COUNT; p++) {
            for (int k = 0; k < s->bag[p].count; k++) {
                if (s->bag[p].items[k].id == f->items[i]) return true;
            }
        }
    }
    return false;
}

// The tabs drawn: ALL, the game's pockets that hold something (an empty pocket
// is no dead tab), then FAVORITES while the bag holds a starred item.
static int bag_tabs(struct pd_ui* ui, const struct pd_game* g, const struct pd_snapshot* s, int* out) {
    int n = 0;
    out[n++] = PD_BAG_ALL;
    for (int i = 0; i < PD_POCKET_COUNT; i++) {
        int p = g->cfg->bagOrder[i];
        if (s->bag[p].count > 0) out[n++] = p;
    }
    if (favs_in_bag(ui, pd_favorite_rom_id(g), s)) out[n++] = PD_BAG_FAVORITES;
    return n;
}

static int pocket_rank(const struct pd_game* g, int pocket) {
    for (int i = 0; i < PD_POCKET_COUNT; i++) {
        if (g->cfg->bagOrder[i] == pocket) return i;
    }
    return PD_POCKET_COUNT;
}

// The item's name, with the move a TM or HM teaches after it ("TM06 TOXIC"),
// the way the games' own TM cases print it (C-B-07).
static const char* row_name(const struct pd_game* g, const struct pd_snapshot* s, const struct bag_row* r, char* buf,
                            size_t len) {
    const struct pd_item* it = &s->bag[r->pocket].items[r->index];
    const char* name = pd_item_name(g, it->id);
    int move = pd_dex_tm_move(g, it->id);
    if (move) {
        char m[24];
        snprintf(buf, len, "%s %s", name, pd_move_name(move, m, sizeof(m)));
        return buf;
    }
    return name;
}

// Which row comes first under the order button (C-B-05), in strcmp's sign.
static int compare_rows(struct pd_ui* ui, const struct pd_game* g, const struct pd_snapshot* s,
                        const struct bag_row* a, const struct bag_row* b) {
    const struct pd_item* ia = &s->bag[a->pocket].items[a->index];
    const struct pd_item* ib = &s->bag[b->pocket].items[b->index];
    switch (ui->bagOrder) {
    case PD_BAG_NAME:
        return strcmp(pd_item_name(g, ia->id), pd_item_name(g, ib->id));
    case PD_BAG_TYPE: {
        int ra = pocket_rank(g, a->pocket), rb = pocket_rank(g, b->pocket);
        if (ra != rb) return ra - rb;
        return strcmp(pd_item_name(g, ia->id), pd_item_name(g, ib->id));
    }
    case PD_BAG_COUNT:
        if (ia->quantity != ib->quantity) return ib->quantity - ia->quantity;
        return strcmp(pd_item_name(g, ia->id), pd_item_name(g, ib->id));
    default: // DEFAULT: the starred items first, then the bag's own order
        return (is_favorite(ui, pd_favorite_rom_id(g), ia->id) ? 0 : 1) - (is_favorite(ui, pd_favorite_rom_id(g), ib->id) ? 0 : 1);
    }
}

// The rows the tab shows, in the order asked for: the game's pockets in their
// own order, or ALL / FAVORITES over them.
static void build_rows(struct pd_ui* ui, const struct pd_game* g, const struct pd_snapshot* s) {
    rowCount = 0;
    for (int i = 0; i < PD_POCKET_COUNT; i++) {
        int pocket = g->cfg->bagOrder[i];
        if (ui->bagPocket != PD_BAG_ALL && ui->bagPocket != PD_BAG_FAVORITES && ui->bagPocket != pocket) continue;
        const struct pd_pocket_items* items = &s->bag[pocket];
        for (int k = 0; k < items->count && rowCount < BAG_ROWS; k++) {
            if (ui->bagPocket == PD_BAG_FAVORITES && !is_favorite(ui, pd_favorite_rom_id(g), items->items[k].id)) continue;
            rows[rowCount++] = (struct bag_row) { pocket, k };
        }
    }
    // An insertion sort, stable for the rows one key leaves equal.
    for (int i = 1; i < rowCount; i++) {
        struct bag_row r = rows[i];
        int j = i - 1;
        while (j >= 0 && compare_rows(ui, g, s, &rows[j], &r) > 0) {
            rows[j + 1] = rows[j];
            j--;
        }
        rows[j + 1] = r;
    }
}

// The item the list's cursor is on, or NULL.
static const struct pd_item* selected_item(struct pd_ui* ui, const struct pd_snapshot* s) {
    if (ui->bagSelected < 0 || ui->bagSelected >= rowCount) return NULL;
    const struct bag_row* r = &rows[ui->bagSelected];
    return &s->bag[r->pocket].items[r->index];
}

// The tabs down the left, the PC and the order button under them (the app's
// FireRed bag): the open one white with red text and the orange underline the
// game draws under its pocket title, the rest in the window's own colour.
static void tabs(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                 const struct bag_palette* p, int open) {
    static const char* const ORDER_LABELS[PD_BAG_ORDERS] = { "DEFAULT", "NAME", "TYPE", "COUNT" };
    int list[PD_POCKET_COUNT + 2], n = bag_tabs(ui, g, s, list);
    int y = CONTENT_Y;
    for (int i = 0; i < n; i++) {
        int tab = list[i];
        bool on = tab == open;
        const char* label = tab == PD_BAG_ALL ? "ALL" : tab == PD_BAG_FAVORITES ? "FAVORITES" : pd_pocket_name(tab);
        pd_layered_box(c, MARGIN, y, TAB_W, TAB_H, 3, p->panel, p->tabLayers,
                       on || ui->pressedId == HIT_BAG_TAB + i ? PD_TITLE_FILL : p->panel[p->tabLayers].color);
        int tw = pd_text_width(label);
        pd_text_fit(c, MARGIN + (TAB_W - (tw > TAB_W - 6 ? TAB_W - 6 : tw)) / 2, pd_text_y(y, TAB_H), TAB_W - 6, label,
                    on ? PD_VALUE : p->tabText, on ? PD_VALUE_SHADOW : p->tabTextShadow);
        // The game's underline under the open pocket's name.
        if (on) pd_fill(c, MARGIN + 3, y + TAB_H - 3, TAB_W - 6, 2, p->cursor);
        ui_add_hit(ui, MARGIN, y, TAB_W, TAB_H, HIT_BAG_TAB + i);
        y += TAB_H + TAB_GAP;
    }
    // PC and the order button pinned to the bottom of the column.
    int by = c->h - MARGIN - 2 * (TAB_H + TAB_GAP);
    ui_button(ui, c, MARGIN, by, TAB_W, TAB_H, "PC", HIT_BAG_PC);
    ui_button(ui, c, MARGIN, by + TAB_H + TAB_GAP, TAB_W, TAB_H, ORDER_LABELS[ui->bagOrder % PD_BAG_ORDERS],
              HIT_BAG_ORDER);
}

// The gold star before a starred item's name (the app's, drawn - the font has
// none): a 7x6 pixel star.
static void star_mark(struct pd_canvas* c, int x, int y, uint32_t color) {
    static const uint16_t STAR[6] = { 0x08, 0x1C, 0x7F, 0x3E, 0x1C, 0x14 };
    pd_bitmap(c, x, y, STAR, 6, color);
}

static void list(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                 const struct bag_palette* p) {
    int x = LIST_X, w = c->w - MARGIN - x;
    int h = DESC_Y - 4 - LIST_Y;
    int f = pd_layered_box(c, x, LIST_Y, w, h, 3, p->panel, p->panelLayers, p->fill);
    int ix = x + f, iy = LIST_Y + f + 1, iw = w - 2 * f, ih = h - 2 * f - 2;
    ui_add_scroll_hit(ui, ix, iy, iw, ih, HIT_BAG_LIST);
    ui->bagRowCount = rowCount;
    ui->bagScrollMax = 0;
    if (rowCount == 0) {
        pd_text(c, ix + 14, pd_text_y(iy, BAG_ROW), "No items.", p->text, p->textShadow);
        return;
    }
    int maxScroll = rowCount * BAG_ROW - ih;
    if (maxScroll < 0) maxScroll = 0;
    ui->bagScrollMax = maxScroll;
    if (ui->bagScroll > maxScroll) ui->bagScroll = maxScroll;
    if (ui->bagScroll < 0) ui->bagScroll = 0;

    pd_clip(c, ix, iy, iw, ih);
    int first = ui->bagScroll / BAG_ROW;
    for (int i = first; i < rowCount; i++) {
        int ry = iy + i * BAG_ROW - ui->bagScroll;
        if (ry >= iy + ih) break;
        const struct bag_row* r = &rows[i];
        const struct pd_item* it = &s->bag[r->pocket].items[r->index];
        bool cursor = i == ui->bagSelected || ui->pressedId == HIT_BAG_ROW + i;
        if (cursor) pd_triangle(c, ix + 4, ry + 3, 6, PD_RIGHT, p->cursor);
        int tx = ix + 14;
        if (is_favorite(ui, pd_favorite_rom_id(g), it->id)) {
            star_mark(c, tx, ry + 5, 0xE8B830);
            tx += 11;
        }
        char buf[40];
        const char* name = row_name(g, s, r, buf, sizeof(buf));
        int ty = pd_text_y(ry, BAG_ROW);
        pd_text_fit(c, tx, ty, ix + iw - 10 - 54 - tx, name, p->text, p->textShadow);
        if (r->pocket != PD_POCKET_KEY && !(it->id >= FIRST_HM && it->id <= LAST_HM)) {
            char qty[12];
            snprintf(qty, sizeof(qty), "×%d", it->quantity);
            pd_text_right(c, ix + iw - 10, ty, qty, p->text, p->textShadow);
        }
        if (p->divider) pd_fill(c, ix + 4, ry + BAG_ROW - 1, iw - 8, 1, p->divider);
        // Rows cut off at the window's edge still take a tap on what shows.
        int top = ry < iy ? iy : ry;
        int bottom = ry + BAG_ROW > iy + ih ? iy + ih : ry + BAG_ROW;
        ui_add_scroll_hit(ui, ix, top, iw, bottom - top, HIT_BAG_ROW + i);
    }
    pd_unclip(c);
    if (maxScroll > 0) {
        int th = ih * ih / (rowCount * BAG_ROW);
        if (th < 8) th = 8;
        int ty = iy + (ih - th) * ui->bagScroll / maxScroll;
        pd_fill(c, ix + iw - 4, ty, 2, th, p->cursor);
    }
}

// The description box: the item's name and its text, the star on the right
// (C-B-06), or what the PC button has to say (C-B-01).
static void description(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                        const struct bag_palette* p) {
    // Under the list, not the tabs: the PC and the order button keep the foot
    // of their own column.
    int x = LIST_X, w = c->w - MARGIN - x, h = c->h - DESC_Y - MARGIN;
    int f = pd_layered_box(c, x, DESC_Y, w, h, 3, p->desc, p->descLayers, p->descFill);
    int tx = x + f + 6, tw = w - 2 * (f + 6);
    int ty = DESC_Y + f + 2;
    const struct pd_item* it = selected_item(ui, s);
    const char* text = "Tap an item to read about it.";
    if (ui->bagPcNotice) {
        pd_text(c, tx, ty, "PC", p->descText, p->descTextShadow);
        text = "The game's own PC isn't in the companion yet.";
    } else if (it) {
        char buf[40];
        const char* name = row_name(g, s, &rows[ui->bagSelected], buf, sizeof(buf));
        pd_text_fit(c, tx, ty, tw - 26, name, p->descText, p->descTextShadow);
        const char* d = pd_item_description(g, it->id);
        if (*d) text = d;
        // The star that keeps this item in FAVORITES: gold when it has one.
        bool on = is_favorite(ui, pd_favorite_rom_id(g), it->id);
        int sx = tx + tw - 16, sy = ty + 4;
        pd_round_rect(c, sx - 3, sy - 3, 15, 16, 3, on ? 0xE8B830 : 0x9C9C8C);
        pd_fill(c, sx - 2, sy - 2, 13, 14, p->descFill);
        star_mark(c, sx, sy, on ? 0xE8B830 : 0x9C9C8C);
        ui_add_hit(ui, sx - 3, sy - 3, 15, 16, HIT_BAG_STAR);
        ty += 18;
    }
    pd_text_wrap(c, tx, ty, tw, 16, (DESC_Y + h - f - ty) / 16, text, p->descText, p->descTextShadow);
}

void ui_bag_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    if (!s->bagOk) {
        ui->bagRowCount = 0;
        ui->bagScrollMax = 0;
        ui_notice(c, "NO BAG YET", "The bag shows once the game", "has loaded a save.");
        return;
    }
    const struct bag_palette* p = g->kind == PD_GAME_EMERALD ? &EMERALD_BAG : &FIRERED_BAG;
    if (p->backdropA) {
        for (int y = CONTENT_Y - 2; y < c->h; y += 8) {
            pd_fill(c, 0, y, c->w, 8, ((y - CONTENT_Y + 2) / 8) % 2 ? p->backdropB : p->backdropA);
        }
    }
    if (ui->bagOrder < 0 || ui->bagOrder >= PD_BAG_ORDERS) ui->bagOrder = PD_BAG_DEFAULT;
    // Back to ALL when the tab the player picked isn't there any more (a pocket
    // the bag emptied, FAVORITES after its last star went).
    tabCount = bag_tabs(ui, g, s, tabIds);
    bool have = false;
    for (int i = 0; i < tabCount; i++) have |= tabIds[i] == ui->bagPocket;
    if (!have) ui->bagPocket = PD_BAG_ALL;
    build_rows(ui, g, s);
    // Keep the cursor on the item it was on: a star or the order can move its
    // row (the app keeps the item's key the same way).
    if (ui->bagSelected >= 0 && selItemId) {
        const struct pd_item* at = selected_item(ui, s);
        if (!at || at->id != selItemId) {
            for (int i = 0; i < rowCount; i++) {
                if (s->bag[rows[i].pocket].items[rows[i].index].id == selItemId) {
                    ui->bagSelected = i;
                    break;
                }
            }
        }
    }
    if (ui->bagSelected >= rowCount) ui->bagSelected = -1;
    lastRom = pd_favorite_rom_id(g);
    const struct pd_item* sel = selected_item(ui, s);
    selItemId = sel ? sel->id : 0;
    tabs(ui, c, g, s, p, ui->bagPocket);
    list(ui, c, g, s, p);
    description(ui, c, g, s, p);
}

// The bag's own taps: the tabs, the order button, the PC button, an item's row
// and the description box's star. What they need comes from the last draw.
bool ui_bag_act(struct pd_ui* ui, int id) {
    if (id >= HIT_BAG_TAB && id < HIT_BAG_TAB + PD_POCKET_COUNT + 2) {
        int i = id - HIT_BAG_TAB;
        if (i >= tabCount) return false;
        ui->bagPocket = tabIds[i];
        ui->bagSelected = -1;
        ui->bagScroll = 0;
        ui->bagPcNotice = false;
        return true;
    }
    if (id == HIT_BAG_ORDER) {
        ui->bagOrder = (ui->bagOrder + 1) % PD_BAG_ORDERS;
        ui->bagScroll = 0;
        return true;
    }
    if (id == HIT_BAG_PC) {
        // C-B-01's button; its action is the game's own PC (C-B-13, P4.2).
        ui->bagPcNotice = true;
        ui->bagSelected = -1;
        return true;
    }
    if (id >= HIT_BAG_ROW && id < HIT_BAG_ROW + rowCount) {
        ui->bagSelected = id - HIT_BAG_ROW;
        // This is an explicit new selection, not a reorder of the old item.
        selItemId = 0;
        ui->bagPcNotice = false;
        return true;
    }
    if (id == HIT_BAG_STAR) {
        if (!selItemId) return false;
        if (toggle_favorite(ui, lastRom, selItemId)) ui->action = PD_ACTION_SETTINGS_CHANGED;
        return true;
    }
    return false;
}
