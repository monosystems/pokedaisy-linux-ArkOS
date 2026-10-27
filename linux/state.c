#include "state.h"

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include <mgba/core/serialize.h>
#include <mgba-util/vfs.h>

#include "log.h"

// The RTC, never the save data: loading a state must not write an old copy of
// the battery save over the player's file.
#define PD_STATE_FLAGS SAVESTATE_RTC
#define PD_PATH_MAX 1024

// mkdir -p for the folder the state goes into.
static void make_dirs(const char* path) {
    char tmp[PD_PATH_MAX];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) {
        return;
    }
    memcpy(tmp, path, len + 1);
    for (char* p = tmp + 1; *p; p++) {
        if (*p != '/') {
            continue;
        }
        *p = '\0';
        mkdir(tmp, 0777);
        *p = '/';
    }
}

bool pd_state_save(struct mCore* core, const char* path) {
    if (!core || !path) {
        return false;
    }
    make_dirs(path);
    struct VFile* vf = VFileOpen(path, O_CREAT | O_TRUNC | O_RDWR);
    if (!vf) {
        pd_logf("state: can't write %s", path);
        return false;
    }
    bool ok = mCoreSaveStateNamed(core, vf, PD_STATE_FLAGS);
    vf->close(vf);
    return ok;
}

bool pd_state_load(struct mCore* core, const char* path) {
    if (!core || !path) {
        return false;
    }
    struct VFile* vf = VFileOpen(path, O_RDONLY);
    if (!vf) {
        return false;
    }
    bool ok = mCoreLoadStateNamed(core, vf, PD_STATE_FLAGS);
    vf->close(vf);
    return ok;
}
