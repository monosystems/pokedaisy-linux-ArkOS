// Shared by the companion's tab files (pd_ui_*.c): layout, hit ids and the
// small widgets every tab draws with.
#ifndef PD_UI_INTERNAL_H
#define PD_UI_INTERNAL_H

#include <stddef.h>

#include "pd_style.h"
#include "pd_ui.h"

#define CONTENT_Y 28
#define MARGIN 4
#define ROW_H 18

// Hit ids.
enum {
    HIT_TAB = 100,         // + tab
    HIT_FF = 110,
    HIT_MORE = 111,
    HIT_SLOT = 200,        // + party slot
    HIT_BACK = 300,
    HIT_POCKET_PREV = 400,
    HIT_POCKET_NEXT = 401,
    HIT_BAG_LIST = 410,
    HIT_BAG_ROW = 500,     // + item index
    HIT_SETTING = 600,     // + enum setting_row
    HIT_SETTINGS_TAB = 610, // + enum pd_tab: a TAB BAR page row
    HIT_TWEAK = 630,       // + enum pd_tweak: a TWEAKS page row
    HIT_SETTINGS_BACK = 640,
    HIT_MORE_TAB_BAR = 641, // MORE's button under the grid: SETTINGS > TAB BAR
    HIT_OPTION = 700,      // + pick-list option
    HIT_YES = 800,
    HIT_NO = 801,
    HIT_SCRIM = 802,
    HIT_MAP = 900,         // the map itself (where: the touch's start)
    HIT_MAP_REGION = 901,
    HIT_MAP_ME = 902,
    HIT_MAP_PLACES = 903,
    HIT_PLACE = 1000,      // + PLACES row
    HIT_DEX_LIST = 1400,
    HIT_DEX_ROW = 1500,    // + national number
    HIT_DEX_TOGGLE = 1900,
    HIT_DEX_PREV = 1901,
    HIT_DEX_NEXT = 1902,
    HIT_DEX_INFO = 1903,    // the entry's INFO / EVOLVE section chips
    HIT_DEX_EVOLVE = 1904,
    HIT_DEX_EVO_LIST = 1905,
    HIT_DEX_FILTER = 1906, // + enum dex_filter: the list's ALL / CAUGHT / SEEN ONLY / NOT SEEN chips
    HIT_DEX_EVO_ROW = 1910, // + family link
    HIT_GUIDE_PAGE = 2000, // + page
    HIT_GUIDE_ROW = 2100,  // + entry
    HIT_GUIDE_LIST = 2900,
    HIT_NOTICE_OK = 2901,
    HIT_NOTICE_BACK = 2902,
    HIT_BATTLE_SUGGEST = 3000,
    HIT_BATTLE_INFO = 3001,
    HIT_FOE_SLOT = 3010,   // + foe party index
    HIT_OPEN_CARD = 3100,
    HIT_CARD = 3101,
    HIT_SUMMARY_PAGE = 3200, // + enum pd_summary_page: the summary's MOVES / STATS / MATCHUPS chips
};

enum setting_row {
    SETTING_SCREEN,
    SETTING_FF,
    SETTING_TAB_BAR, // opens TAB BAR, the pick of the bar's chips (C-03)
    SETTING_TWEAKS,  // opens TWEAKS, the companion's small switches (C-07)
    SETTING_SAVE_STATE,
    SETTING_LOAD_STATE,
    SETTING_LEAVE,
    SETTING_COUNT,
};

// The companion's tabs in the app's order (COMPANION_TABS: PARTY, DEX, MAP,
// ITEMS, GUIDE, CARD): the bar fills in this order, and SETTINGS > TAB BAR
// offers every entry. The port has no STATES or CHEEVOS tab yet.
extern const enum pd_tab PD_TAB_ORDER[];
#define PD_TAB_ORDER_COUNT 6

// C-04: whether this game has the data a tab needs (the POKéDEX its tables,
// the GUIDE its pages, the TRAINER CARD its art).
bool pd_tab_available(const struct pd_game* g, enum pd_tab t);

// Switches the companion to [tab], with the slide TWEAKS > TAB ANIMATIONS asks
// for, and closes an open summary (what every tab change does).
void ui_open_tab(struct pd_ui* ui, enum pd_tab tab);

// SETTINGS' own pages (the TAB BAR pick and the TWEAKS switches): a tap on
// one of their rows; true if it was theirs.
bool ui_settings_act(struct pd_ui* ui, int id);

void ui_add_hit(struct pd_ui* ui, int x, int y, int w, int h, int id);
void ui_add_scroll_hit(struct pd_ui* ui, int x, int y, int w, int h, int id);

// A white framed button; pressed = grey, like the tab chips.
void ui_button(struct pd_ui* ui, struct pd_canvas* c, int x, int y, int w, int h, const char* label, int id);
// The party menu's HP bar: a dark frame, the "HP" tag, a two-tone fill.
void ui_hp_bar(struct pd_canvas* c, int x, int y, int barW, int hp, int maxHp);
// The summary's EXP bar: the same build with an "EXP" tag, filled from
// [start] (the level's first EXP) to [next] (the next level's).
void ui_exp_bar(struct pd_canvas* c, int x, int y, int barW, uint32_t value, uint32_t start, uint32_t next);
void ui_status_badge(struct pd_canvas* c, int x, int y, uint32_t status);
// A type's colour (AppColors.typeColors), and its chip: the type's name in
// white on that colour, with the multiplier after it when label isn't empty
// ("ROCK 4x") - the app's TypeBadge / TypeBadgeWithLabel. Both return pixels.
uint32_t ui_type_color(int type);
#define TYPE_CHIP_H 14
int ui_type_chip_width(int type, const char* label);
int ui_type_chip(struct pd_canvas* c, int x, int y, int type, const char* label);
// A message in a list window across the content area.
void ui_notice(struct pd_canvas* c, const char* title, const char* line1, const char* line2);
// A LABEL  VALUE row, the value from ~42% across (OptionLine).
void ui_row(struct pd_canvas* c, int x, int y, int w, const char* label, const char* value, bool selected);
void ui_format_money(long v, char* buf, size_t len);

void ui_party_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s);
// The chip's word for a tab ("" for the gear, which draws the gear instead).
const char* pd_tab_label(enum pd_tab tab);
// MORE: the game's tabs the bar had no room for (their chips open them).
void ui_more_tab(struct pd_ui* ui, struct pd_canvas* c);
void ui_battle_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s);
void ui_bag_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s);
void ui_settings_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s,
                     const struct pd_host_info* host);
void ui_map_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s);
void ui_guide_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s);
void ui_dex_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s);
void ui_overlay(struct pd_ui* ui, struct pd_canvas* c);
// Overlays drawn by their tab's file (PLACES, the GUIDE notice).
void ui_places_overlay(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g);
void ui_guide_notice(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g);

// A tap each tab's file handles; true if it was theirs.
bool ui_map_act(struct pd_ui* ui, int id);
bool ui_dex_act(struct pd_ui* ui, int id);
bool ui_guide_act(struct pd_ui* ui, int id);
bool ui_battle_act(struct pd_ui* ui, int id);
bool ui_card_act(struct pd_ui* ui, int id);
void ui_card_tab(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g, const struct pd_snapshot* s);
bool ui_card_tick(struct pd_ui* ui, unsigned ms);

// The lists' caught / seen mark: a Poké Ball, or a grey one.
void ui_ball(struct pd_canvas* c, int x, int y, bool caught);

// A scrolled list's offset clamped to its content: max(0, total - view).
int ui_clamp_scroll(int scroll, int total, int view);
// A thin scroll bar along a list's right edge.
void ui_scroll_bar(struct pd_canvas* c, int x, int y, int h, int scroll, int total, uint32_t color);

#endif
