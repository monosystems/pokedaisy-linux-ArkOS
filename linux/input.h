// The R36S' own buttons through SDL2: the pad (hot plug included) and, for the
// desktop dev loop, the keyboard. Edges matter: the overlay moves its cursor on
// a press, not while a button is held.
#ifndef PD_INPUT_H
#define PD_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#include <SDL.h>

enum pd_button {
    PD_BTN_UP,
    PD_BTN_DOWN,
    PD_BTN_LEFT,
    PD_BTN_RIGHT,
    PD_BTN_A,
    PD_BTN_B,
    PD_BTN_START,
    PD_BTN_SELECT,
    PD_BTN_L1,
    PD_BTN_R1,
    PD_BTN_L2,
    PD_BTN_R2,
    PD_BTN_COUNT,
};

struct pd_input;

struct pd_input* pd_input_open(void);
void pd_input_close(struct pd_input* in);
void pd_input_event(struct pd_input* in, const SDL_Event* ev);

// Latches this frame's buttons: call once per frame, after the events.
void pd_input_poll(struct pd_input* in);
bool pd_input_down(const struct pd_input* in, enum pd_button button);
// Held now and not on the last poll.
bool pd_input_pressed(const struct pd_input* in, enum pd_button button);

// The GBA buttons held now, as mGBA's key bits.
uint32_t pd_input_keys(const struct pd_input* in);

// The window was closed or Esc was pressed.
bool pd_input_quit(const struct pd_input* in);

#endif
