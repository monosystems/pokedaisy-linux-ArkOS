// The game list a launcher shows when it is started without a ROM: the .gba
// files of a folder, in the portable core's OPTION-look list (pd_menu), picked
// with the D-pad. Without it a PortMaster port has nothing to start.
#ifndef PD_MENU_INCLUDED
#define PD_MENU_INCLUDED

#include <stdbool.h>
#include <stddef.h>

#include "input.h"
#include "render.h"

// Puts the folder's games on screen and waits for one. False when the player
// backs out (START, B or Esc). The chosen game's full path goes into out.
// pick: tests only - a name to choose without a pad (NULL = wait for one);
// shot: with pick, write the list to this PNG first (NULL = none).
bool pd_pick_rom(const char* dir, const char* pick, const char* shot, char* out, size_t outLen,
                 struct pd_render* render, struct pd_input* input, const char* playing);

#endif
