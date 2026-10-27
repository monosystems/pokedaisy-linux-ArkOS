// Files on the ROM partition: where a game's battery save lives, and the
// backup PokeDaisy takes before every start (CLAUDE.md: save files are sacred).
#ifndef PD_FILES_H
#define PD_FILES_H

#include <stdbool.h>
#include <stddef.h>

// How many backups are kept per game.
#define PD_BACKUPS 10

// "<dir>/<name>" without the extension, into out. False for a path with no name.
bool pd_path_stem(const char* path, char* out, size_t outLen);

// Where the game's save is: "<stem>.srm" when that exists (RetroArch's name),
// otherwise "<stem>.sav" (mGBA's). Existing saves are never moved.
bool pd_save_path(const char* romPath, char* out, size_t outLen);

// Copies the save to "<stem>.backup-YYYYmmdd-HHMMSS.sav" beside it and keeps
// the newest PD_BACKUPS. A missing or empty save is fine (nothing to do).
bool pd_backup_save(const char* savePath);

// Where the save state goes: "<rom dir>/pokedaisy-states/<stem>.ss1", beside
// the game where a player can find it.
bool pd_state_path(const char* romPath, char* out, size_t outLen);

// The "carry on where you left off" state, in the same folder: written when a
// game is left, loaded when it starts again.
bool pd_resume_path(const char* romPath, char* out, size_t outLen);

// True when the resume state is newer than the battery save (or there is no
// save yet): the player hasn't played the game somewhere else since.
bool pd_resume_is_fresh(const char* resumePath, const char* savePath);

// When the state was written ("10-26 08:44"); false when there is none.
bool pd_state_info(const char* path, char* when, size_t whenLen);

#endif
