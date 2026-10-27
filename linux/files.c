#include "files.h"

#include "log.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define PD_PATH_MAX 1024
#define PD_NAME_MAX 1100

static const char* base_name(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static bool file_exists_nonempty(const char* path, size_t* sizeOut) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0) {
        return false;
    }
    if (sizeOut) {
        *sizeOut = (size_t) st.st_size;
    }
    return true;
}

static bool copy_file(const char* from, const char* to) {
    FILE* in = fopen(from, "rb");
    if (!in) {
        return false;
    }
    FILE* out = fopen(to, "wb");
    if (!out) {
        fclose(in);
        return false;
    }
    static char buf[64 * 1024];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            ok = false;
            break;
        }
    }
    if (ferror(in)) {
        ok = false;
    }
    fclose(in);
    if (fclose(out) != 0) {
        ok = false;
    }
    if (!ok) {
        unlink(to);
    }
    return ok;
}

// The folder part of path ("." for a bare file name, "/" for "/x.sav").
static void dir_of(const char* path, char* out, size_t outLen) {
    const char* slash = strrchr(path, '/');
    if (!slash) {
        snprintf(out, outLen, ".");
        return;
    }
    size_t n = (size_t) (slash - path);
    if (n == 0) {
        n = 1;
    }
    if (n >= outLen) {
        n = outLen - 1;
    }
    memcpy(out, path, n);
    out[n] = '\0';
}

bool pd_path_stem(const char* path, char* out, size_t outLen) {
    if (!path || !*path) {
        return false;
    }
    const char* name = base_name(path);
    const char* dot = strrchr(name, '.');
    size_t prefix = (size_t) (name - path);
    size_t nameLen = (dot && dot != name) ? (size_t) (dot - name) : strlen(name);
    if (nameLen == 0 || prefix + nameLen >= outLen) {
        return false;
    }
    memcpy(out, path, prefix + nameLen);
    out[prefix + nameLen] = '\0';
    return true;
}

bool pd_save_path(const char* romPath, char* out, size_t outLen) {
    char stem[PD_PATH_MAX];
    if (!pd_path_stem(romPath, stem, sizeof(stem))) {
        return false;
    }
    struct stat st;
    int n = snprintf(out, outLen, "%s.srm", stem);
    if (n < 0 || (size_t) n >= outLen) {
        return false;
    }
    if (stat(out, &st) == 0) {
        return true;
    }
    n = snprintf(out, outLen, "%s.sav", stem);
    return n > 0 && (size_t) n < outLen;
}

static int cmp_names(const void* a, const void* b) {
    return strcmp(*(const char* const*) a, *(const char* const*) b);
}

// Keeps the newest PD_BACKUPS of "<name>.backup-*.sav" in dir; the names sort
// by their timestamp, so the oldest come first.
static void prune_backups(const char* dir, const char* name) {
    DIR* d = opendir(dir);
    if (!d) {
        return;
    }
    char prefix[PD_NAME_MAX];
    snprintf(prefix, sizeof(prefix), "%s.backup-", name);
    size_t prefixLen = strlen(prefix);

    char* names[256];
    int n = 0;
    struct dirent* e;
    while ((e = readdir(d)) && n < (int) (sizeof(names) / sizeof(*names))) {
        const char* entry = e->d_name;
        size_t len = strlen(entry);
        if (len < prefixLen + 4 || strncmp(entry, prefix, prefixLen) != 0 ||
            strcmp(entry + len - 4, ".sav") != 0) {
            continue;
        }
        char* copy = strdup(entry);
        if (!copy) {
            break;
        }
        names[n++] = copy;
    }
    closedir(d);
    if (n <= PD_BACKUPS) {
        for (int i = 0; i < n; i++) {
            free(names[i]);
        }
        return;
    }
    qsort(names, (size_t) n, sizeof(names[0]), cmp_names);
    for (int i = 0; i < n - PD_BACKUPS; i++) {
        char path[PD_NAME_MAX + PD_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        unlink(path);
    }
    for (int i = 0; i < n; i++) {
        free(names[i]);
    }
}

bool pd_backup_save(const char* savePath) {
    if (!file_exists_nonempty(savePath, NULL)) {
        return true; // a new game: nothing to protect yet
    }
    char stem[PD_PATH_MAX];
    if (!pd_path_stem(savePath, stem, sizeof(stem))) {
        return false;
    }
    char dir[PD_PATH_MAX];
    dir_of(stem, dir, sizeof(dir));
    const char* name = base_name(stem);

    char stamp[32];
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    if (tm) {
        strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", tm);
    } else {
        snprintf(stamp, sizeof(stamp), "00000000-000000");
    }

    char to[PD_NAME_MAX + PD_PATH_MAX];
    snprintf(to, sizeof(to), "%s/%s.backup-%s.sav", dir, name, stamp);
    if (!copy_file(savePath, to)) {
        pd_logf("files: couldn't back up %s", savePath);
        return false;
    }
    pd_logf("files: backed the save up to %s", to);
    prune_backups(dir, name);
    return true;
}

bool pd_state_path(const char* romPath, char* out, size_t outLen) {
    char stem[PD_PATH_MAX];
    if (!pd_path_stem(romPath, stem, sizeof(stem))) {
        return false;
    }
    char dir[PD_PATH_MAX];
    dir_of(stem, dir, sizeof(dir));
    int n = snprintf(out, outLen, "%s/pokedaisy-states/%s.ss1", dir, base_name(stem));
    return n > 0 && (size_t) n < outLen;
}

bool pd_state_info(const char* path, char* when, size_t whenLen) {
    struct stat st;
    if (!path || stat(path, &st) != 0 || st.st_size <= 0) {
        return false;
    }
    time_t written = st.st_mtime;
    struct tm* tm = localtime(&written);
    if (!tm) {
        snprintf(when, whenLen, "SAVED");
        return true;
    }
    strftime(when, whenLen, "%m-%d %H:%M", tm);
    return true;
}

bool pd_resume_path(const char* romPath, char* out, size_t outLen) {
    char stem[PD_PATH_MAX];
    if (!pd_path_stem(romPath, stem, sizeof(stem))) {
        return false;
    }
    char dir[PD_PATH_MAX];
    dir_of(stem, dir, sizeof(dir));
    int n = snprintf(out, outLen, "%s/pokedaisy-states/%s.resume", dir, base_name(stem));
    return n > 0 && (size_t) n < outLen;
}

bool pd_resume_is_fresh(const char* resumePath, const char* savePath) {
    struct stat resume;
    if (!resumePath || stat(resumePath, &resume) != 0 || resume.st_size <= 0) {
        return false;
    }
    struct stat save;
    if (!savePath || stat(savePath, &save) != 0 || save.st_size <= 0) {
        return true; // nothing saved in the game yet: no newer save to respect
    }
    // A save written after the state (in this port or in RetroArch) wins: the
    // player has played on, so an old moment must not replace it.
    return save.st_mtime <= resume.st_mtime;
}
