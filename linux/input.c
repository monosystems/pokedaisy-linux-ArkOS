#include "input.h"

#include <stdlib.h>

#include "log.h"

// The GBA's key bits, as in mgba/internal/gba/input.h and 3ds/ctr/main.c.
enum {
    GBA_A = 1 << 0,
    GBA_B = 1 << 1,
    GBA_SELECT = 1 << 2,
    GBA_START = 1 << 3,
    GBA_RIGHT = 1 << 4,
    GBA_LEFT = 1 << 5,
    GBA_UP = 1 << 6,
    GBA_DOWN = 1 << 7,
    GBA_R = 1 << 8,
    GBA_L = 1 << 9,
};

#define PD_STICK_DEADZONE 12000
#define PD_TRIGGER_DOWN 16000

struct pd_input {
    SDL_GameController* pad;
    int padIndex;
    uint32_t now;  // buttons held this poll
    uint32_t prev; // and the poll before
    bool quit;
};

static bool pad_button(const struct pd_input* in, SDL_GameControllerButton button) {
    return in->pad && SDL_GameControllerGetButton(in->pad, button);
}

static Sint16 pad_axis(const struct pd_input* in, SDL_GameControllerAxis axis) {
    return in->pad ? SDL_GameControllerGetAxis(in->pad, axis) : 0;
}

static void open_pad(struct pd_input* in, int index) {
    if (!SDL_IsGameController(index)) {
        return;
    }
    SDL_GameController* pad = SDL_GameControllerOpen(index);
    if (!pad) {
        pd_logf("input: controller %d didn't open: %s", index, SDL_GetError());
        return;
    }
    const char* name = SDL_GameControllerName(pad);
    in->pad = pad;
    in->padIndex = index;
    pd_logf("input: controller %d: %s", index, name ? name : "?");
}

struct pd_input* pd_input_open(void) {
    struct pd_input* in = calloc(1, sizeof(*in));
    if (!in) {
        return NULL;
    }
    in->padIndex = -1;
    int n = SDL_NumJoysticks();
    for (int i = 0; i < n && !in->pad; i++) {
        if (SDL_IsGameController(i)) {
            open_pad(in, i);
        }
    }
    if (!in->pad) {
        pd_logf("input: no controller yet; keyboard: X=A, Z=B, A=L, S=R, Enter=START, Backspace=SELECT, "
                "arrows=d-pad, Q=companion, Space=fast-forward");
    }
    return in;
}

void pd_input_close(struct pd_input* in) {
    if (!in) {
        return;
    }
    if (in->pad) {
        SDL_GameControllerClose(in->pad);
    }
    free(in);
}

void pd_input_event(struct pd_input* in, const SDL_Event* ev) {
    if (!in || !ev) {
        return;
    }
    switch (ev->type) {
    case SDL_QUIT:
        in->quit = true;
        break;
    case SDL_KEYDOWN:
        if (ev->key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
            in->quit = true;
        }
        break;
    case SDL_CONTROLLERDEVICEADDED:
        if (!in->pad) {
            open_pad(in, ev->cdevice.which);
        }
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        if (in->pad && ev->cdevice.which == in->padIndex) {
            pd_logf("input: controller %d went away", in->padIndex);
            SDL_GameControllerClose(in->pad);
            in->pad = NULL;
            in->padIndex = -1;
        }
        break;
    default:
        break;
    }
}

static uint32_t read_buttons(const struct pd_input* in) {
    const Uint8* kb = SDL_GetKeyboardState(NULL);
    uint32_t m = 0;

    if (pad_button(in, SDL_CONTROLLER_BUTTON_DPAD_UP) || kb[SDL_SCANCODE_UP]) {
        m |= 1u << PD_BTN_UP;
    }
    if (pad_button(in, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || kb[SDL_SCANCODE_DOWN]) {
        m |= 1u << PD_BTN_DOWN;
    }
    if (pad_button(in, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || kb[SDL_SCANCODE_LEFT]) {
        m |= 1u << PD_BTN_LEFT;
    }
    if (pad_button(in, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || kb[SDL_SCANCODE_RIGHT]) {
        m |= 1u << PD_BTN_RIGHT;
    }
    if (pad_button(in, SDL_CONTROLLER_BUTTON_A) || kb[SDL_SCANCODE_X]) {
        m |= 1u << PD_BTN_A;
    }
    if (pad_button(in, SDL_CONTROLLER_BUTTON_B) || kb[SDL_SCANCODE_Z]) {
        m |= 1u << PD_BTN_B;
    }
    // X and Y are a second START and SELECT, as in the Android app.
    if (pad_button(in, SDL_CONTROLLER_BUTTON_START) || pad_button(in, SDL_CONTROLLER_BUTTON_X) ||
        kb[SDL_SCANCODE_RETURN]) {
        m |= 1u << PD_BTN_START;
    }
    if (pad_button(in, SDL_CONTROLLER_BUTTON_BACK) || pad_button(in, SDL_CONTROLLER_BUTTON_Y) ||
        kb[SDL_SCANCODE_BACKSPACE]) {
        m |= 1u << PD_BTN_SELECT;
    }
    if (pad_button(in, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) || kb[SDL_SCANCODE_A]) {
        m |= 1u << PD_BTN_L1;
    }
    if (pad_button(in, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) || kb[SDL_SCANCODE_S]) {
        m |= 1u << PD_BTN_R1;
    }
    if (pad_axis(in, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > PD_TRIGGER_DOWN || kb[SDL_SCANCODE_Q]) {
        m |= 1u << PD_BTN_L2;
    }
    if (pad_axis(in, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > PD_TRIGGER_DOWN || kb[SDL_SCANCODE_SPACE]) {
        m |= 1u << PD_BTN_R2;
    }

    // The left stick moves as the D-pad, the way the 3DS host treats its circle pad.
    Sint16 x = pad_axis(in, SDL_CONTROLLER_AXIS_LEFTX);
    Sint16 y = pad_axis(in, SDL_CONTROLLER_AXIS_LEFTY);
    if (x > PD_STICK_DEADZONE) {
        m |= 1u << PD_BTN_RIGHT;
    } else if (x < -PD_STICK_DEADZONE) {
        m |= 1u << PD_BTN_LEFT;
    }
    if (y > PD_STICK_DEADZONE) {
        m |= 1u << PD_BTN_DOWN;
    } else if (y < -PD_STICK_DEADZONE) {
        m |= 1u << PD_BTN_UP;
    }
    return m;
}

void pd_input_poll(struct pd_input* in) {
    if (!in) {
        return;
    }
    in->prev = in->now;
    in->now = read_buttons(in);
}

bool pd_input_down(const struct pd_input* in, enum pd_button button) {
    return in && ((in->now >> button) & 1u);
}

bool pd_input_pressed(const struct pd_input* in, enum pd_button button) {
    return in && (((in->now & ~in->prev) >> button) & 1u);
}

uint32_t pd_input_keys(const struct pd_input* in) {
    if (!in) {
        return 0;
    }
    uint32_t k = 0;
    if (in->now & (1u << PD_BTN_A)) {
        k |= GBA_A;
    }
    if (in->now & (1u << PD_BTN_B)) {
        k |= GBA_B;
    }
    if (in->now & (1u << PD_BTN_START)) {
        k |= GBA_START;
    }
    if (in->now & (1u << PD_BTN_SELECT)) {
        k |= GBA_SELECT;
    }
    if (in->now & (1u << PD_BTN_UP)) {
        k |= GBA_UP;
    }
    if (in->now & (1u << PD_BTN_DOWN)) {
        k |= GBA_DOWN;
    }
    if (in->now & (1u << PD_BTN_LEFT)) {
        k |= GBA_LEFT;
    }
    if (in->now & (1u << PD_BTN_RIGHT)) {
        k |= GBA_RIGHT;
    }
    if (in->now & (1u << PD_BTN_L1)) {
        k |= GBA_L;
    }
    if (in->now & (1u << PD_BTN_R1)) {
        k |= GBA_R;
    }
    return k;
}

bool pd_input_quit(const struct pd_input* in) {
    return in && in->quit;
}
