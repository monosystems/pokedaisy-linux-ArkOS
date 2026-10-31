// The screen: the game's frames and the companion canvas, on SDL2. Works
// windowed on a desktop (the dev loop) and full screen on KMSDRM (the
// handheld), and renders into an offscreen surface for headless tests.
#ifndef PD_RENDER_H
#define PD_RENDER_H

#include <stdbool.h>
#include <stdint.h>

#include <mgba/core/interface.h> // color_t

enum pd_scale {
    PD_SCALE_INTEGER, // whole pixels only (2x on the R36S' 640x480)
    PD_SCALE_FIT,     // fills the screen, nearest (the default)
    PD_SCALE_STRETCH, // fills it both ways, aspect be damned
};

struct pd_render;

// headless: no window, a software renderer over a 640x480 surface, for tests.
struct pd_render* pd_render_open(const char* title, int gameW, int gameH, int scale, int rotation,
                                 bool fullscreen, bool headless, bool softwareOnly);
void pd_render_close(struct pd_render* r);

// Shows a game frame, with the companion canvas (0xRRGGBB, ow x oh) drawn over
// the whole screen when overlay isn't NULL.
void pd_render_frame(struct pd_render* r, const color_t* game, const uint32_t* overlay, int ow, int oh);

// The companion's SETTINGS tab picks how the game fills the screen.
void pd_render_set_scale(struct pd_render* r, int scale);

// A short opaque strip drawn over the game, but never over the companion.
void pd_render_status(struct pd_render* r, const uint32_t* pixels, int w, int h);

// Writes what the last frame looks like into a PNG (for tests and for the
// manual). Redraws first: the window's back buffer isn't readable after a present.
bool pd_render_screenshot(struct pd_render* r, const char* path);

int pd_render_width(const struct pd_render* r);
int pd_render_height(const struct pd_render* r);

// For the log: SDL's renderer name, and the display's mode ("640x480 @59.9 Hz").
const char* pd_render_name(const struct pd_render* r);
void pd_render_display(char* out, size_t outLen);

#endif
