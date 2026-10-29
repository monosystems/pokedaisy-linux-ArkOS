// The companion on the Linux screen: the portable 3ds/core UI (pd_ui) over the
// game, driven by a D-pad cursor instead of a finger. Every press moves
// the cursor onto the nearest real hit pd_ui recorded for the last draw, and A
// "taps" it through pd_ui_touch - so the whole UI works without a touchscreen
// and 3ds/core stays untouched.
#ifndef PD_COMPANION_INCLUDED
#define PD_COMPANION_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pd_game.h"
#include "pd_snapshot.h"
#include "pd_ui.h"

#define PD_COMPANION_W PD_UI_WIDTH
#define PD_COMPANION_H PD_UI_HEIGHT

enum pd_nav {
    PD_NAV_UP,
    PD_NAV_DOWN,
    PD_NAV_LEFT,
    PD_NAV_RIGHT,
    PD_NAV_OK,
    PD_NAV_BACK,
    PD_NAV_TAB_NEXT,
    PD_NAV_TAB_PREV,
};

struct pd_companion;

// rom: the ROM as mGBA loaded it (the game's tables and art come from it).
// settingsPath: where screen mode and fast-forward speed are kept.
struct pd_companion* pd_companion_open(const char* romPath, const char* settingsPath, const uint8_t* rom,
                                       size_t romSize);
void pd_companion_close(struct pd_companion* c);

// False for a game the portable core doesn't read: the game still plays, the
// overlay just says so.
bool pd_companion_supported(const struct pd_companion* c);
const char* pd_companion_title(const struct pd_companion* c);

void pd_companion_show(struct pd_companion* c, bool visible);
void pd_companion_toggle(struct pd_companion* c);
bool pd_companion_visible(const struct pd_companion* c);

// One emulated frame: reads the game every 10 frames, runs the animations, and
// hands back what the player asked the host for (an enum pd_action).
enum pd_action pd_companion_tick(struct pd_companion* c, pd_read_fn read, void* ctx, unsigned frame,
                                 unsigned elapsedMs, bool ffOn);

// A button in the overlay. True when the canvas needs a redraw.
bool pd_companion_nav(struct pd_companion* c, enum pd_nav key);

// Redraws when something changed; the pixels (PD_COMPANION_W x PD_COMPANION_H,
// 0xRRGGBB) are valid afterwards.
const uint32_t* pd_companion_draw(struct pd_companion* c);

// What the SETTINGS tab's LOAD STATE row says about the slot on disk.
void pd_companion_set_state(struct pd_companion* c, bool hasState, const char* when);

int pd_companion_screen_mode(const struct pd_companion* c); // PD_SCREEN_*
void pd_companion_set_screen_mode(struct pd_companion* c, int mode);
int pd_companion_ff_speed(const struct pd_companion* c);
bool pd_companion_save_settings(struct pd_companion* c);

// What the crash handler prints: the tab being drawn, how many hits the last
// draw made, and the id of the hit the cursor was on (-1 = none).
void pd_companion_debug(int* tab, int* hits, int* focus);

#endif
