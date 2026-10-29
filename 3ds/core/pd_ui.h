// The companion on the 3DS's 320x240 bottom screen: PARTY (with a summary on
// tap), BAG, MAP, GUIDE, POKéDEX, BATTLE (cards + your moves' verdicts) and
// SETTINGS, in the app's
// FireRed OPTION-screen look (app/.../companion/ui/GbaMenu.kt). Immediate
// mode: every pd_ui_draw records where its tappable parts went, and
// pd_ui_touch looks a tap up there - no layout pass to keep in sync.
//
// The host owns the emulator: it shows what pd_ui_draw drew, feeds the stylus
// and buttons in, and carries out what pd_ui_take_action hands back (fast-
// forward, save states, leaving the game, saving changed settings).
#ifndef PD_UI_H
#define PD_UI_H

#include <stdbool.h>

#include "pd_canvas.h"
#include "pd_game.h"
#include "pd_snapshot.h"

#define PD_UI_WIDTH 320
#define PD_UI_HEIGHT 240

enum pd_tab {
    PD_TAB_PARTY,
    PD_TAB_BAG,
    PD_TAB_MAP,
    PD_TAB_GUIDE,
    PD_TAB_DEX,
    PD_TAB_BATTLE,
    PD_TAB_SETTINGS,
    PD_TAB_CARD, // the TRAINER CARD: off the bar until TAB BAR puts it on, else from SETTINGS
    PD_TAB_MORE, // the chip left of the gear: the game's tabs the bar has no room for
    PD_TAB_COUNT,
};

// The game's own chips on the bar - the app's MAX_BAR_TABS. A battle takes one
// of their places for its own chip, which is why BATTLE leads the bar then and
// the last tab it pushed off opens from MORE.
#define PD_BAR_TABS 5

// How the game fills the top screen.
enum pd_screen_mode {
    PD_SCREEN_PIXEL,   // 1x, pixel for pixel
    PD_SCREEN_SHARP,   // 1.5x (fills the height), sharp bilinear
    PD_SCREEN_STRETCH, // the whole 400x240, sharp bilinear
    PD_SCREEN_MODES,
};

// Fast-forward steps (H-24), in percent of normal speed; the last is 0 =
// INFINITE (as fast as the emulator goes between two presents).
#define PD_FF_STEPS 9
extern const int PD_FF_SPEEDS[PD_FF_STEPS];
// GAME SPEED (H-27): what the game stays at, in percent.
#define PD_GAME_SPEEDS 8
extern const int PD_GAME_SPEED_STEPS[PD_GAME_SPEEDS];
// Slow motion while held (H-29), in percent.
#define PD_SLOW_SPEED 50

// SETTINGS > TWEAKS, the app's CompanionTweaks: small on / off preferences for
// the companion's motion and habits, each on by default.
enum pd_tweak {
    PD_TWEAK_PARTY_ICONS, // the party icon's two-frame bounce
    PD_TWEAK_MAP_CURSOR,  // the MAP cursor swapping between its two sizes
    PD_TWEAK_TAB_ANIM,    // a tab sliding in instead of cutting
    PD_TWEAK_JUMP_BATTLE, // a battle opening BATTLE by itself
    // C-BT-08: the app's BATTLE HINTS, the move verdicts and the best pick.
    // A settings row there; the port's SETTINGS is one column, so it sits with
    // the switches for now (P1.13 groups them).
    PD_TWEAK_BATTLE_HINTS,
    PD_TWEAK_COUNT,
};

// SETTINGS > TAB BAR (the app's COMPANION_TABS): a bit per tab the player put
// on the bar. The game's other tabs fill the places left over, and what still
// has no room opens from MORE.
#define PD_TAB_BIT(tab) (1u << (unsigned) (tab))
#define PD_TWEAK_BIT(tweak) (1u << (unsigned) (tweak))
#define PD_TABS_DEFAULT \
    (PD_TAB_BIT(PD_TAB_PARTY) | PD_TAB_BIT(PD_TAB_DEX) | PD_TAB_BIT(PD_TAB_MAP) | \
     PD_TAB_BIT(PD_TAB_BAG) | PD_TAB_BIT(PD_TAB_GUIDE))
#define PD_TWEAKS_DEFAULT ((1u << PD_TWEAK_COUNT) - 1)

// C-B-06: the bag's starred items, one list per ROM (the app keeps them per
// game). A ROM's list is empty the first time it is opened.
#define PD_FAV_ROMS 4
#define PD_FAV_MAX 32
struct pd_favs {
    unsigned rom; // pd_rom_id, 0 = the slot is free
    unsigned short items[PD_FAV_MAX];
    int count;
};

// What the player set; the host loads and saves it (pd_settings.h).
struct pd_settings {
    int screenMode; // enum pd_screen_mode
    int ffSpeed;    // PD_FF_SPEEDS' percent, 0 = INFINITE (H-24)
    int gameSpeed;  // PD_GAME_SPEED_STEPS' percent the game stays at (H-27)
    bool slowMotion; // L1 held slows the game down (H-29)
    int guideNotice; // a bit per game kind whose GUIDE notice was accepted
    unsigned tabsShown; // a bit per enum pd_tab: the player's pick of the bar
    unsigned tweaks;    // a bit per enum pd_tweak: on
    struct pd_favs favs[PD_FAV_ROMS];
};

// Whether the player left [t] on (SETTINGS > TWEAKS).
static inline bool pd_tweak_on(const struct pd_settings* s, enum pd_tweak t) {
    return s && (s->tweaks & PD_TWEAK_BIT(t)) != 0;
}

// Whether the player put [t] on the bar (SETTINGS > TAB BAR).
static inline bool pd_tab_shown(const struct pd_settings* s, enum pd_tab t) {
    return s && (s->tabsShown & PD_TAB_BIT(t)) != 0;
}

// SETTINGS' own pages: the option list, and the two the app opens from it.
enum pd_settings_page {
    PD_SETTINGS_HOME,
    PD_SETTINGS_TABS,      // C-03: which tabs the bar holds
    PD_SETTINGS_TWEAKS,    // C-07: the companion's small switches
    // C-SET-01: the app's groups, each a page behind a row.
    PD_SETTINGS_COMPANION, // the bar and the switches
    PD_SETTINGS_BATTLE,    // BATTLE HINTS
    PD_SETTINGS_SPEED,     // GAME SPEED, FAST-FORWARD, SLOW MOTION
    PD_SETTINGS_SAVES,     // the states, RESTART GAME, LEAVE GAME
};

// A party slot's summary, in pages: its moves, the numbers behind them (IVs,
// EVs, nature - C-P-09) and what it takes damage from (C-P-08). The app keeps
// the last two beside the moves; a 320x240 screen hasn't the room, so they are
// pages of their own. The page stays put while stepping through the party.
enum pd_summary_page {
    PD_SUMMARY_MOVES,
    PD_SUMMARY_STATS,
    PD_SUMMARY_MATCHUPS,
    PD_SUMMARY_PAGES,
};

// The bag's own tabs beyond the game's pockets (C-B-04, C-B-06): ALL mixes
// every pocket in the game's order, FAVORITES holds the starred items.
#define PD_BAG_ALL (-1)
#define PD_BAG_FAVORITES (-2)

// The bag's order button (C-B-05): the game's own order with the starred items
// first, by name, by pocket, or by how many are held.
enum bag_order {
    PD_BAG_DEFAULT,
    PD_BAG_NAME,
    PD_BAG_TYPE,
    PD_BAG_COUNT,
    PD_BAG_ORDERS,
};

enum pd_action {
    PD_ACTION_NONE,
    PD_ACTION_TOGGLE_FF,
    PD_ACTION_SAVE_STATE,
    PD_ACTION_LOAD_STATE,
    PD_ACTION_LEAVE_GAME,
    PD_ACTION_RESTART_GAME, // C-SET-04: reboot the game, its save as it was
    PD_ACTION_SETTINGS_CHANGED,
};

// What the host tells the companion about itself.
#define PD_UI_HOST_LINES 2
struct pd_host_info {
    const char* labels[PD_UI_HOST_LINES]; // extra SETTINGS info rows (FILE, SPEED ...)
    char values[PD_UI_HOST_LINES][40];
    int count;
    bool ffOn;
    bool hasState;      // a save state exists for LOAD STATE
    char stateWhen[24]; // when it was made, shown beside LOAD STATE
};

#define PD_UI_MAX_HITS 64
struct pd_hit {
    int x, y, w, h;
    int id;
    bool scroll; // part of a list that a drag scrolls
};

enum pd_overlay {
    PD_OVERLAY_NONE,
    PD_OVERLAY_PICK_SCREEN,
    PD_OVERLAY_PICK_FF,
    PD_OVERLAY_PICK_SPEED, // GAME SPEED (H-27)
    PD_OVERLAY_CONFIRM_SAVE,
    PD_OVERLAY_CONFIRM_LOAD,
    PD_OVERLAY_CONFIRM_LEAVE,
    PD_OVERLAY_CONFIRM_RESTART,
    PD_OVERLAY_TABS_FULL,    // TAB BAR: no place left for another chip
    PD_OVERLAY_PLACES,       // the MAP's list of places
    PD_OVERLAY_GUIDE_NOTICE, // the GUIDE's "AI-written, may be wrong" notice
};

struct pd_ui {
    enum pd_tab tab;
    enum pd_tab tabBeforeBattle; // the tab a battle took over from
    bool battleWasOn;
    // The bar, filled in on every draw from what this game has (C-04): the
    // chips' tabs in the order drawn - BATTLE leads a battle - and the game's
    // tabs that didn't fit, which MORE's page opens.
    enum pd_tab bar[PD_BAR_TABS + 1];
    int barCount;
    enum pd_tab more[PD_TAB_COUNT];
    int moreCount;
    // How many of the player's picks this game has (the TAB BAR page's "n/5",
    // and what decides whether another chip still fits).
    int tabsPicked;
    // The tab just opened slides in for a moment (TWEAKS): ms left, and the
    // side it comes from (+1 the bar's right, -1 its left). hitDx is the same
    // shift for the hits the tab body records, so a press lands on what the
    // player sees.
    int tabSlideMs, tabSlideDir, hitDx;
    // Time passing, for the party icon's two frames: since the start, and how
    // much of the fastest frame's 60 ms went by.
    unsigned animMs, iconMs;
    // SETTINGS: the page shown (the option list, or one of its own pages).
    enum pd_settings_page settingsPage;
    // C-05: whether the player is in the game yet. A boot at the title screen
    // draws empty party slots and a blinking PRESS START on the save's tabs
    // until a battle, a party or five different map positions say otherwise
    // (the app's rule, without its field-callback check), or the host resumed
    // into a state. posSeen are the positions this boot has shown.
    bool started;
    int posSeen[5][4];
    int posCount;
    bool waitBlink; // PRESS START's phase (pd_ui_tick runs it)
    // BATTLE: INFO (0) or SUGGESTIONS (1); the FOE TEAM's slots seen so far
    // this battle (a bit each), the one tapped (-1 = follow the battle) and
    // the one shown (set by the draw); what the last read said was out / next.
    int battlePane;
    unsigned foeSeen;
    int foeSelected, foeShown;
    int lastFoeActive, lastFoeNext;
    int summarySlot; // party slot whose summary is open, -1 = none
    int summaryPage; // enum pd_summary_page: the summary's page
    int battleScroll; // the INFO pane's scroll: the foe's matchups push it past the window
    // BAG: the tab shown (PD_BAG_ALL, PD_BAG_FAVORITES or a PD_POCKET_*), the
    // row chosen in the list it builds, that list's scroll, the order button's
    // pick (enum bag_order), and the PC button's "not yet" line in the
    // description box.
    int bagPocket, bagSelected, bagScroll, bagOrder;
    bool bagPcNotice;
    // POKéDEX: the list's scroll, the open entry (national number, 0 = the
    // list), which dex shows (-1 = the save's own: National once it has
    // it, else the regional one) and the filter (enum dex_filter). An entry's
    // section (0 INFO, 1 EVOLVE) and the EVOLVE family's scroll.
    int dexScroll, dexOpen, dexNational, dexFilter;
    int dexSection, dexEvoScroll;
    // MAP: the page shown (-1 = the player's), the tapped place, the cursor's blink.
    int mapPage, mapSel, mapDungeon, mapSelTx, mapSelTy;
    bool mapBlink;
    unsigned blinkMs;
    int placesScroll;
    // GUIDE: the page, its scroll, the open entry and how far it's revealed.
    int guidePage, guideScroll, guideOpen, guideLevel;
    bool guideNoticeShown;
    // TRAINER CARD: the side shown, a flip running (ms into it, 0 = none) and
    // the play time's blinking colon.
    bool cardBack, cardColon;
    int cardFlipMs;
    unsigned cardColonMs;

    enum pd_overlay overlay;
    struct pd_settings* settings;
    enum pd_action action;

    bool touching, dragging, touchScroll;
    int touchX0, touchY0, touchLastY;
    int pressedId; // what the stylus went down on (-1 = nothing)
    struct pd_hit hits[PD_UI_MAX_HITS];
    int hitCount;
};

void pd_ui_init(struct pd_ui* ui, struct pd_settings* settings);
// Follows the game: a battle opens BATTLE, its end goes back.
void pd_ui_update(struct pd_ui* ui, const struct pd_snapshot* s);
// C-05: the game has not reached its field yet (the title screen, the intro,
// the CONTINUE menu still loading the save), so PARTY, BAG, MAP and CARD wait.
bool pd_ui_waiting(const struct pd_ui* ui);
// The host loaded a savestate to start from: that moment was in the game, so
// the waiting tabs show at once (the app's resume).
void pd_ui_resumed(struct pd_ui* ui);
// The host rebooted the game (SETTING_RESTART): back to C-05's waiting view.
void pd_ui_restarted(struct pd_ui* ui);
void pd_ui_draw(struct pd_ui* ui, struct pd_canvas* c, const struct pd_game* g,
                const struct pd_snapshot* s, const struct pd_host_info* host);
// Stylus: down / held / up at (x, y). Returns true if the screen needs a redraw.
bool pd_ui_touch(struct pd_ui* ui, int x, int y, bool down);
// Buttons the GBA doesn't have (the 3DS's X / Y): next / previous tab, BACK.
void pd_ui_next_tab(struct pd_ui* ui, int dir);
bool pd_ui_back(struct pd_ui* ui);
// The last thing the player asked the host for (once), or PD_ACTION_NONE.
enum pd_action pd_ui_take_action(struct pd_ui* ui);
// Time passing (milliseconds since the last call): true when something on
// screen animates and needs a redraw (the MAP cursor's blink, the TRAINER
// CARD's colon and flip).
bool pd_ui_tick(struct pd_ui* ui, unsigned ms);

const char* pd_screen_mode_name(int mode);

#endif
