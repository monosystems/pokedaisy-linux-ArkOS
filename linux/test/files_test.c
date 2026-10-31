#include "files.h"
#include "pd_settings.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void write_text(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    assert(f);
    assert(fwrite(text, 1, strlen(text), f) == strlen(text));
    assert(fclose(f) == 0);
}

static void expect_text(const char* path, const char* text) {
    char buf[64] = { 0 };
    FILE* f = fopen(path, "rb");
    assert(f);
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    assert(!ferror(f));
    fclose(f);
    assert(n == strlen(text) && strcmp(buf, text) == 0);
}

int main(int argc, char** argv) {
    assert(argc == 2);
    char settingsPath[1024];
    snprintf(settingsPath, sizeof(settingsPath), "%s/settings.ini", argv[1]);
    struct pd_settings settings, loaded;
    pd_settings_defaults(&settings);
    assert(!settings.statusBar);
    settings.statusBar = true;
    assert(pd_settings_save(&settings, settingsPath));
    assert(pd_settings_load(&loaded, settingsPath) && loaded.statusBar);
    char rom[1024], save[1024], resume[1024], latest[2200], backup[1024];
    snprintf(rom, sizeof(rom), "%s/game.gba", argv[1]);
    snprintf(save, sizeof(save), "%s/game.srm", argv[1]);
    snprintf(backup, sizeof(backup), "%s/game.backup-20200101-000000.sav", argv[1]);
    assert(pd_resume_path(rom, resume, sizeof(resume)));
    write_text(save, "current");
    assert(!pd_restore_backup(rom));
    expect_text(save, "current");
    write_text(backup, "old");
    snprintf(backup, sizeof(backup), "%s/game.backup-20200101-000000-001.sav", argv[1]);
    write_text(backup, "newest");
    assert(pd_latest_backup(save, latest, sizeof(latest)) && strcmp(latest, backup) == 0);
    char list[PD_BACKUPS][PD_BACKUP_PATH];
    assert(pd_list_backups(save, list, PD_BACKUPS) == 2);
    assert(strcmp(list[0], backup) == 0);
    assert(!pd_restore_backup_at(rom, save));
    expect_text(save, "current");
    write_text(resume, "stale state");
    assert(pd_restore_backup_at(rom, list[1]));
    expect_text(save, "old");
    assert(access(resume, F_OK) != 0);
    assert(pd_latest_backup(save, latest, sizeof(latest)));
    expect_text(latest, "current");
    assert(pd_backup_save(save));
    assert(pd_backup_save(save));
    assert(pd_latest_backup(save, latest, sizeof(latest)));
    expect_text(latest, "old");
    return 0;
}
