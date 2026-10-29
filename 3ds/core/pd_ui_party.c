// PARTY: the six slots in FireRed's party colours, each with the game's own
// icon (from the ROM, pd_icon), and a summary on tap.
#include <stdio.h>
#include <string.h>

#include "pd_icon.h"
#include "pd_tables.h"
#include "pd_ui_internal.h"

// FireRedPartyPalette (PartyScreen.kt).
struct slot_colors {
    uint32_t outline, light, fill, shade;
};
static const struct slot_colors SLOT_NORMAL = { 0x4A4A63, 0x84C6DE, 0x3994DE, 0x297BB5 };
static const struct slot_colors SLOT_SELECTED = { 0xFF7331, 0xADEFFF, 0x7BD6EF, 0x4AADCE };
static const struct slot_colors SLOT_FAINTED = { 0x4A4A63, 0xD6A521, 0xC66B10, 0xA54A00 };
#define SLOT_TEXT 0xFFFFFF
#define SLOT_TEXT_SHADOW 0x737373
#define MALE 0x42CEFF
#define MALE_SHADOW 0x006394
#define FEMALE 0xFF9C94
#define FEMALE_SHADOW 0x9C4239
// The held item tag's bar (C-P-03): its height, the padding round the marker
// and the name, the marker's size and the gap after it.
#define TAG_H 12
#define TAG_INSET 4
#define TAG_MARKER 8
#define TAG_GAP 3

static const char* mon_display_name(const struct pd_mon* m, char* buf, size_t len) {
    if (m->species == PD_SPECIES_EGG) return "EGG";
    if (m->nickname[0]) return m->nickname;
    return pd_species_name(m->species, buf, len);
}

// The party menu's ♂ / ♀ after the name, in their own colours.
static void gender_mark(struct pd_canvas* c, int x, int y, enum pd_gender g) {
    if (g == PD_GENDER_MALE) pd_text(c, x, y, "♂", MALE, MALE_SHADOW);
    if (g == PD_GENDER_FEMALE) pd_text(c, x, y, "♀", FEMALE, FEMALE_SHADOW);
}

// The icon's two frames, at the game's HP-based speed (PartySlotLayout: a
// healthy Pokémon's six GBA frames up to a nearly fainted one's 22). The app
// animates the slot the game's cursor is on; this port's selected slot is the
// one under the press, which is also the one it draws selected.
static int icon_shift(const struct pd_ui* ui, const struct pd_mon* m, int slot) {
    if (!pd_tweak_on(ui->settings, PD_TWEAK_PARTY_ICONS) || ui->pressedId != HIT_SLOT + slot) return 0;
    static const unsigned TICKS[4] = { 6, 8, 14, 22 };
    int level;
    if (m->species == PD_SPECIES_EGG) level = 0;
    else if (m->hp <= 0) return 0; // a fainted slot stands still
    else if (m->hp >= m->maxHp) level = 0;
    else {
        int pct = m->hp * 100 / m->maxHp;
        level = pct > 50 ? 1 : pct > 20 ? 2 : 3;
    }
    unsigned interval = TICKS[level] * 1000 / 60;
    return ((ui->animMs / interval) & 1) ? -3 : 1;
}

// C-P-03: what the slot's mon holds, on a thin bar under the HP numbers and
// above the HP bar - the ROM's own held-item marker and the item's name, in
// the app's HeldItemTag colours (V-20). The bar is right-aligned and keeps
// clear of the status badge on the left (emptyLeft is the row's free left
// edge); when a status icon leaves no room for both, the name wins and the
// marker goes. A ROM without the marker's art shows the name alone.
static void held_item_tag(struct pd_canvas* c, const struct pd_game* g, const struct pd_mon* m, int x, int y, int w,
                          int emptyLeft) {
    if (!m->heldItem) return;
    const char* name = pd_item_name(g, m->heldItem);
    if (!name[0]) return;
    int right = x + w - 8;
    int room = right - emptyLeft - 2 * TAG_INSET;
    int nameW = pd_text_width(name);
    bool icon = pd_held_marker_ready(g) && nameW <= room - TAG_MARKER - TAG_GAP;
    int maxName = room - (icon ? TAG_MARKER + TAG_GAP : 0);
    if (maxName < pd_text_width("W...")) return; // no room for even a word
    if (nameW > maxName) nameW = maxName;
    int iconW = icon ? TAG_MARKER + TAG_GAP : 0;
    int bw = 2 * TAG_INSET + iconW + nameW;
    int bx = right - bw;
    // The app's title window, flat: a grey line round a white bar.
    pd_round_rect(c, bx, y, bw, TAG_H, 2, 0x63737B);
    pd_round_rect(c, bx + 1, y + 1, bw - 2, TAG_H - 2, 1, PD_TITLE_FILL);
    if (icon) pd_draw_held_marker(c, g, bx + TAG_INSET, y + 2, m->heldItem >= PD_MAIL_FIRST && m->heldItem <= PD_MAIL_LAST);
    // Caps sit in the bar's middle rows (the 16px cell starts two rows up).
    pd_text_fit(c, bx + TAG_INSET + iconW, y - 2, nameW + 1, name, PD_LABEL, PD_LABEL_SHADOW);
}

static void party_slot(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, int x, int y, int w,
                       int h, const struct pd_mon* m, int slot) {
    char buf[40];
    // Every slot is a hit, filled or not: a party of one still lets the cursor
    // walk the whole grid (OK on an empty slot does nothing, see pd_ui_tap).
    ui_add_hit(ui, x, y, w, h, HIT_SLOT + slot);
    bool pressed = ui->pressedId == HIT_SLOT + slot;
    if (!m) {
        // An empty slot: just its outline over the backdrop.
        pd_round_rect(c, x, y, w, h, 5, pressed ? SLOT_SELECTED.outline : SLOT_NORMAL.outline);
        pd_round_rect(c, x + 1, y + 1, w - 2, h - 2, 4, PD_BACKDROP_B);
        return;
    }
    bool egg = m->species == PD_SPECIES_EGG;
    bool fainted = !egg && m->hp == 0;
    const struct slot_colors* col = pressed ? &SLOT_SELECTED : fainted ? &SLOT_FAINTED : &SLOT_NORMAL;
    pd_round_rect(c, x, y, w, h, 5, col->outline);
    pd_round_rect(c, x + 1, y + 1, w - 2, h - 2, 4, col->light);
    pd_round_rect(c, x + 2, y + 2, w - 4, h - 4, 3, col->fill);
    pd_fill(c, x + 3, y + h - 6, w - 6, 3, col->shade);

    // The icon at the top left, like the game's party menu; the text beside it.
    int tx = x + 8;
    if (pd_draw_mon_icon(c, g, m->species, x + 2, y + 1 + icon_shift(ui, m, slot), false)) tx = x + 2 + PD_ICON + 3;
    const char* name = mon_display_name(m, buf, sizeof(buf));
    if (!egg) {
        char lv[12];
        snprintf(lv, sizeof(lv), "Lv%d", m->level);
        int lvW = pd_text_width(lv);
        pd_text_right(c, x + w - 8, y + 3, lv, SLOT_TEXT, SLOT_TEXT_SHADOW);
        int nameW = pd_text_fit(c, tx, y + 3, x + w - 8 - lvW - 6 - 10 - tx, name, SLOT_TEXT, SLOT_TEXT_SHADOW);
        gender_mark(c, tx + nameW + 3, y + 3, m->gender);
        const char* st = pd_status_label(m->status);
        int badgeW = *st ? pd_text_width(st) + 6 : 0;
        ui_status_badge(c, tx, y + 24, m->status);
        char hp[16];
        snprintf(hp, sizeof(hp), "%d/%d", m->hp, m->maxHp);
        pd_text_right(c, x + w - 8, y + 21, hp, SLOT_TEXT, SLOT_TEXT_SHADOW);
        ui_hp_bar(c, x + 8, y + h - 18, w - 34, m->hp, m->maxHp);
        // The tag's band: under the HP numbers, over the HP bar (C-P-03).
        held_item_tag(c, g, m, x, y + 35, w, tx + (badgeW ? badgeW + 4 : 0));
    } else {
        pd_text_fit(c, tx, y + 3, x + w - 8 - tx, name, SLOT_TEXT, SLOT_TEXT_SHADOW);
    }
}

// --- the summary: its info block and its three pages (C-P-06 to C-P-09) ---

// The body's rows: six 16px lines under the info block. The page chips and
// BACK share the window's last row.
#define SUM_ROW 16

// A page chip (MOVES / STATS / MATCHUPS): white with red text when open, like
// the POKéDEX's section chips.
static void page_chip(struct pd_ui* ui, struct pd_canvas* c, int x, int y, int w, const char* label, int page) {
    bool open = ui->summaryPage == page;
    pd_title_box(c, x, y, w, 22, open || ui->pressedId == HIT_SUMMARY_PAGE + page ? PD_TITLE_FILL : PD_LIST_FILL);
    int tw = pd_text_width(label);
    pd_text(c, x + (w - tw) / 2, pd_text_y(y, 22), label, open ? PD_VALUE : PD_TITLE_TEXT,
            open ? PD_VALUE_SHADOW : PD_TITLE_SHADOW);
    ui_add_hit(ui, x, y, w, 22, HIT_SUMMARY_PAGE + page);
}

// An IV bar (the app's IvBar): a dark frame, the party bar's grey, the fill in
// the HP colours and gold at a perfect 31.
static void iv_bar(struct pd_canvas* c, int x, int y, int w, int iv) {
    int pct = iv * 100 / PD_MAX_IV;
    uint32_t fill = iv >= PD_MAX_IV ? 0xE8B830 : pct > 50 ? 0x60D048 : pct > 20 ? 0xF8C838 : 0xF04838;
    pd_fill(c, x, y, w, 6, 0x293131);
    pd_fill(c, x + 1, y + 1, w - 2, 4, 0x506858);
    int f = (w - 2) * iv / PD_MAX_IV;
    if (f > 0) pd_fill(c, x + 1, y + 1, f, 4, fill);
}

// The summary's top block: icon, name, gender (and the kind, for a nickname),
// level, its types, its status, the HP bar and the EXP bar - the app's
// InfoSection, a row tighter.
static void summary_info(struct pd_canvas* c, const struct pd_game* g, const struct pd_mon* m, int ix, int iw, int ty) {
    char buf[40], buf2[40];
    bool egg = m->species == PD_SPECIES_EGG;
    const char* name = mon_display_name(m, buf, sizeof(buf));
    int tx = ix;
    if (pd_draw_mon_icon(c, g, m->species, ix - 2, ty + 2, !egg && m->hp == 0)) {
        tx = ix - 2 + PD_ICON + 4;
        iw -= tx - ix;
    }
    int nameW = pd_text(c, tx, ty, name, PD_LABEL, PD_LABEL_SHADOW);
    if (egg) {
        pd_text(c, tx, ty + 18, "It's an egg. It will hatch in time.", PD_MUTED, PD_MUTED_SHADOW);
        return;
    }
    gender_mark(c, tx + nameW + 3, ty, m->gender);
    char lv[12];
    snprintf(lv, sizeof(lv), "Lv%d", m->level);
    int lvW = pd_text_width(lv);
    pd_text_right(c, tx + iw, ty, lv, PD_VALUE, PD_VALUE_SHADOW);
    // A nickname of the player's own: the kind after it, like the game's summary.
    const char* species = pd_species_name(m->species, buf2, sizeof(buf2));
    int spx = tx + nameW + 14;
    if (strcmp(species, name)) pd_text_fit(c, spx, ty, tx + iw - lvW - 8 - spx, species, PD_MUTED, PD_MUTED_SHADOW);
    // The kind's types, with the status on the same row's right.
    int t1, t2;
    pd_species_type_pair(m->species, &t1, &t2);
    int cx = tx;
    if (t1 != PD_TYPE_NONE) cx += ui_type_chip(c, cx, ty + 18, t1, "") + 4;
    if (t2 != PD_TYPE_NONE && t2 != t1) ui_type_chip(c, cx, ty + 18, t2, "");
    const char* st = pd_status_label(m->status);
    if (*st) ui_status_badge(c, tx + iw - pd_text_width(st) - 6, ty + 18, m->status);
    // HP, then the EXP gained within this level (C-P-07): the app's NEXT, or
    // MAX at level 100, from the species' own growth curve.
    ui_hp_bar(c, tx, ty + 40, 96, m->hp, m->maxHp);
    char hp[16];
    snprintf(hp, sizeof(hp), "%d/%d", m->hp, m->maxHp);
    pd_text_right(c, tx + iw, ty + 36, hp, PD_LABEL, PD_LABEL_SHADOW);
    bool maxed = m->level >= 100;
    long start = pd_exp_for_level(m->species, m->level);
    long next = maxed ? -1 : pd_exp_for_level(m->species, m->level + 1);
    if (start < 0 || (!maxed && next <= 0)) {
        pd_text(c, tx, ty + 54, "EXP unknown", PD_MUTED, PD_MUTED_SHADOW);
        return;
    }
    ui_exp_bar(c, tx, ty + 58, 96, m->experience, (uint32_t) start, (uint32_t) (maxed ? start + 1 : next));
    char txt[24];
    if (maxed) {
        snprintf(txt, sizeof(txt), "MAX");
    } else {
        long left = next - (long) m->experience;
        snprintf(txt, sizeof(txt), "NEXT %ld", left > 0 ? left : 0);
    }
    pd_text_right(c, tx + iw, ty + 54, txt, PD_LABEL, PD_LABEL_SHADOW);
}

// The four moves: name, its type, the PP left, as the app's MoveRow words it.
static void summary_moves(struct pd_canvas* c, const struct pd_mon* m, int ix, int iw, int y) {
    char buf[40];
    for (int i = 0; i < PD_NUM_MOVES; i++) {
        if (!m->moves[i]) continue;
        pd_text_fit(c, ix, y, iw - 120, pd_move_name(m->moves[i], buf, sizeof(buf)), PD_LABEL, PD_LABEL_SHADOW);
        pd_text(c, ix + iw - 116, y, pd_type_name(pd_move_type(m->moves[i])), PD_MUTED, PD_MUTED_SHADOW);
        char pp[12];
        snprintf(pp, sizeof(pp), "PP %d", m->pp[i]);
        pd_text_right(c, ix + iw, y, pp, PD_VALUE, PD_VALUE_SHADOW);
        y += 20; // the rows the summary had before its pages: four of them breathe
    }
}

// C-P-09: the numbers behind the moves - each stat's value, its IV on a bar and
// its EV, the nature's red up / blue down arrow on the stat it raises / lowers,
// then the totals, the nature's name and the Hidden Power this mon's IVs give.
// Two columns of three: one column of six would not fit.
static void summary_stats(struct pd_canvas* c, const struct pd_mon* m, int ix, int iw, int y) {
    static const char* const LABELS[PD_STAT_COUNT] = { "HP", "ATTACK", "DEFENSE", "SPEED", "SP. ATK", "SP. DEF" };
    static const int COLUMNS[2][3] = {
        { PD_STAT_HP, PD_STAT_ATK, PD_STAT_DEF },
        { PD_STAT_SPATK, PD_STAT_SPDEF, PD_STAT_SPEED },
    };
    int up = pd_nature_raises(m->nature), down = pd_nature_lowers(m->nature);
    int colW = (iw - 4) / 2;
    int ivTotal = 0, evTotal = 0;
    for (int s = 0; s < PD_STAT_COUNT; s++) {
        ivTotal += m->ivs[s];
        evTotal += m->evs[s];
    }
    char num[12];
    for (int col = 0; col < 2; col++) {
        int cx = ix + col * (colW + 4), right = cx + colW;
        int barX = cx + 84, barW = 20, numX = cx + 120;
        pd_text(c, cx, y, "STAT", PD_MUTED, PD_MUTED_SHADOW);
        pd_text_right(c, numX, y, "IV", PD_MUTED, PD_MUTED_SHADOW);
        pd_text_right(c, right, y, "EV", PD_MUTED, PD_MUTED_SHADOW);
        for (int r = 0; r < 3; r++) {
            int stat = COLUMNS[col][r], ry = y + SUM_ROW * (r + 1);
            int labelW = pd_text(c, cx, ry, LABELS[stat], PD_LABEL, PD_LABEL_SHADOW);
            // The nature's mark: a red up arrow or a blue down one (later
            // games' colours), beside the stat it moves.
            if (stat == up) pd_triangle(c, cx + labelW + 3, ry + 4, 5, PD_UP, 0xE04040);
            if (stat == down) pd_triangle(c, cx + labelW + 3, ry + 6, 5, PD_DOWN, 0x3984F7);
            snprintf(num, sizeof(num), "%d", m->stats[stat]);
            pd_text_right(c, cx + 80, ry, num, PD_LABEL, PD_LABEL_SHADOW);
            iv_bar(c, barX, ry + 5, barW, m->ivs[stat]);
            snprintf(num, sizeof(num), "%d", m->ivs[stat]);
            pd_text_right(c, numX, ry, num, PD_VALUE, PD_VALUE_SHADOW);
            snprintf(num, sizeof(num), "%d", m->evs[stat]);
            pd_text_right(c, right, ry, num, PD_VALUE, PD_VALUE_SHADOW);
        }
    }
    // The totals under their columns, the nature beside them.
    int ty = y + SUM_ROW * 4;
    pd_text(c, ix, ty, "TOTAL", PD_LABEL, PD_LABEL_SHADOW);
    snprintf(num, sizeof(num), "%d", ivTotal);
    pd_text_right(c, ix + 120, ty, num, PD_VALUE, PD_VALUE_SHADOW);
    snprintf(num, sizeof(num), "%d", evTotal);
    pd_text_right(c, ix + colW, ty, num, PD_VALUE, PD_VALUE_SHADOW);
    int nx = ix + colW + 4;
    pd_text(c, nx, ty, "NATURE", PD_MUTED, PD_MUTED_SHADOW);
    pd_text(c, nx + 46, ty, pd_nature_name(m->nature), PD_LABEL, PD_LABEL_SHADOW);
    // HIDDEN POWER: the type and the power these IVs give it.
    ty += SUM_ROW;
    pd_fill(c, ix, ty - 4, iw, 1, PD_DIVIDER);
    pd_text(c, ix, ty + 2, "HIDDEN POWER", PD_LABEL, PD_LABEL_SHADOW);
    int chipW = ui_type_chip(c, ix + 90, ty + 3, pd_hidden_power_type(m), "");
    snprintf(num, sizeof(num), "PWR %d", pd_hidden_power_power(m));
    pd_text(c, ix + 90 + chipW + 6, ty + 2, num, PD_VALUE, PD_VALUE_SHADOW);
}

// C-P-08: WEAK TO / RESISTS / IMMUNE TO - the app's MatchupGroup in its inline
// form, a chip per type with its multiplier (the work is the widgets').
static void summary_matchups(struct pd_canvas* c, const struct pd_mon* m, int ix, int iw, int y, int h) {
    int t1, t2;
    pd_species_type_pair(m->species, &t1, &t2);
    if (t1 == PD_TYPE_NONE) {
        pd_text(c, ix, y, "This kind's types aren't known.", PD_MUTED, PD_MUTED_SHADOW);
        return;
    }
    struct pd_matchups mm;
    ui_matchups(t1, t2, &mm);
    pd_clip(c, ix, y, iw, h);
    ui_matchup_block(c, ix, y, iw, &mm);
    pd_unclip(c);
}

static void summary(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_mon* m) {
    int x = MARGIN, y = CONTENT_Y, w = c->w - 2 * MARGIN, h = c->h - CONTENT_Y - MARGIN;
    int f = pd_title_box(c, x, y, w, h, PD_TITLE_FILL);
    int ix = x + f + 6, iw = w - 2 * (f + 6);
    int ty = y + f + 2;
    summary_info(c, g, m, ix, iw, ty);
    int bodyY = ty + 74;
    int bodyH = y + h - f - 30 - bodyY;
    if (m->species != PD_SPECIES_EGG) {
        pd_fill(c, ix, bodyY - 4, iw, 1, PD_DIVIDER);
        if (ui->summaryPage == PD_SUMMARY_STATS) summary_stats(c, m, ix, iw, bodyY);
        else if (ui->summaryPage == PD_SUMMARY_MATCHUPS) summary_matchups(c, m, ix, iw, bodyY, bodyH);
        else summary_moves(c, m, ix, iw, bodyY);
    }
    // The page chips along the bottom left, BACK at the right. An egg has no
    // moves, stats or matchups to show, so it gets none of the chips.
    static const char* const PAGES[PD_SUMMARY_PAGES] = { "MOVES", "STATS", "MATCHUPS" };
    int cx = ix;
    if (m->species != PD_SPECIES_EGG) {
        for (int i = 0; i < PD_SUMMARY_PAGES; i++) {
            int pw = pd_text_width(PAGES[i]) + 14;
            page_chip(ui, c, cx, y + h - f - 26, pw, PAGES[i], i);
            cx += pw + 3;
        }
    }
    ui_button(ui, c, x + w - f - 70, y + h - f - 26, 66, 22, "BACK", HIT_BACK);
}

void ui_party_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s) {
    // An empty slot is tappable too (the cursor walks all six), so a slot that
    // has no Pokémon must not open its summary.
    if (ui->summarySlot >= s->partyCount) {
        ui->summarySlot = -1;
    }
    if (ui->summarySlot >= 0) {
        summary(ui, c, g, &s->party[ui->summarySlot]);
        return;
    }
    int gap = 4;
    int w = (c->w - 2 * MARGIN - gap) / 2;
    int h = (c->h - CONTENT_Y - MARGIN - 2 * gap) / 3;
    for (int i = 0; i < PD_PARTY_SIZE; i++) {
        int x = MARGIN + (i % 2) * (w + gap);
        int y = CONTENT_Y + (i / 2) * (h + gap);
        party_slot(ui, c, g, x, y, w, h, i < s->partyCount ? &s->party[i] : NULL, i);
    }
}
