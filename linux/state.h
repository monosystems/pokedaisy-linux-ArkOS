// Save states: mGBA's own format, one slot per game, in a folder beside the
// ROM. A state never carries the battery save (SAVESTATE_RTC only), so loading
// one can't put an old save over the player's file (CLAUDE.md: save files are
// sacred) - and the host backs the save up before every load anyway.
#ifndef PD_STATE_H
#define PD_STATE_H

#include <stdbool.h>

#include <mgba/core/core.h>

bool pd_state_save(struct mCore* core, const char* path);
bool pd_state_load(struct mCore* core, const char* path);

#endif
