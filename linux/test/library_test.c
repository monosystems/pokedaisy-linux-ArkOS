// Drive the real library loop with scripted button edges and disposable files.
// The renderer is replaced only to record the screens the loop produces.
#include "menu.h"
#include "files.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct pd_input {
    const int* buttons;
    int count, index, current;
};

void pd_input_event(struct pd_input* input, const SDL_Event* event) { (void) input; (void) event; }
void pd_input_poll(struct pd_input* input) { input->current = input->buttons[input->index++]; }
bool pd_input_pressed(const struct pd_input* input, enum pd_button button) { return input->current == (int) button; }
bool pd_input_quit(const struct pd_input* input) { return input->index >= input->count; }

static unsigned hashes[32];
static int frames;
void pd_render_frame(struct pd_render* render, const color_t* game, const uint32_t* pixels, int w, int h) {
    (void) render;
    (void) game;
    unsigned hash = 2166136261u;
    for (int i = 0; i < w * h; i++) hash = (hash ^ pixels[i]) * 16777619u;
    assert(frames < (int) (sizeof(hashes) / sizeof(hashes[0])));
    hashes[frames++] = hash;
}
bool pd_render_screenshot(struct pd_render* render, const char* path) { (void) render; (void) path; return false; }

static void write_text(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    assert(f && fwrite(text, 1, strlen(text), f) == strlen(text));
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

static void path(char* out, size_t len, const char* dir, const char* name) {
    int n = snprintf(out, len, "%s/%s", dir, name);
    assert(n > 0 && (size_t) n < len);
}

static void run(const char* dir, const char* playing, const int* buttons, int count) {
    struct pd_input input = { buttons, count, 0, -1 };
    char out[1024];
    frames = 0;
    assert(!pd_pick_rom(dir, NULL, NULL, out, sizeof(out), NULL, &input, playing));
}

int main(int argc, char** argv) {
    assert(argc == 2);
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    char rom[1024], save[1024], backup[1024], resume[1024], state[1024], latest[PD_BACKUP_PATH];
    path(rom, sizeof(rom), argv[1], "game.gba");
    path(save, sizeof(save), argv[1], "game.srm");
    path(backup, sizeof(backup), argv[1], "game.backup-20200101-000000.sav");
    assert(pd_resume_path(rom, resume, sizeof(resume)));
    assert(pd_state_path(rom, state, sizeof(state)));
    write_text(rom, "synthetic rom");
    write_text(save, "current save");
    write_text(backup, "older save");
    path(backup, sizeof(backup), argv[1], "game.backup-20200102-000000.sav");
    write_text(backup, "newer save");
    write_text(resume, "resume state");
    write_text(state, "save state");

    // Each -1 releases the previous button, as a real second press needs.
    // Cancel the confirmation and check that nothing changed on disk.
    const int cancel[] = { PD_BTN_SELECT, PD_BTN_A, PD_BTN_DOWN, PD_BTN_A, PD_BTN_B,
                           -1, PD_BTN_B, -1, PD_BTN_B, -1, PD_BTN_B };
    run(argv[1], NULL, cancel, (int) (sizeof(cancel) / sizeof(cancel[0])));
    assert(frames == 10);
    assert(hashes[0] != hashes[1] && hashes[1] != hashes[2] && hashes[2] != hashes[3]);
    assert(hashes[2] == hashes[4]);
    expect_text(save, "current save");
    expect_text(resume, "resume state");
    expect_text(state, "save state");

    // Choose the older backup again and confirm it.
    const int restore[] = { PD_BTN_SELECT, PD_BTN_A, PD_BTN_DOWN, PD_BTN_A, -1,
                            PD_BTN_A, PD_BTN_B, -1, PD_BTN_B };
    run(argv[1], NULL, restore, (int) (sizeof(restore) / sizeof(restore[0])));
    assert(frames == 8 && hashes[3] == hashes[4]);
    expect_text(save, "older save");
    expect_text(state, "save state");
    assert(access(resume, F_OK) != 0);
    assert(pd_latest_backup(save, latest, sizeof(latest)));
    expect_text(latest, "current save");

    // A paused game's open battery save cannot be restored from its INFO page.
    write_text(save, "playing save");
    write_text(resume, "playing resume");
    const int playing[] = { PD_BTN_SELECT, PD_BTN_A, -1, PD_BTN_A, -1, PD_BTN_A, PD_BTN_B, -1, PD_BTN_B };
    run(argv[1], rom, playing, (int) (sizeof(playing) / sizeof(playing[0])));
    assert(frames == 8 && hashes[0] == hashes[1] && hashes[1] == hashes[3] && hashes[3] == hashes[5]);
    expect_text(save, "playing save");
    expect_text(resume, "playing resume");
    expect_text(state, "save state");

    // Another ROM can refer to the same open save via a hard link.
    char aliasRom[1024], aliasSave[1024];
    path(aliasRom, sizeof(aliasRom), argv[1], "alias.gba");
    path(aliasSave, sizeof(aliasSave), argv[1], "alias.srm");
    path(backup, sizeof(backup), argv[1], "alias.backup-20200101-000000.sav");
    write_text(aliasRom, "synthetic rom");
    write_text(backup, "other backup");
    assert(link(save, aliasSave) == 0);
    const int alias[] = { PD_BTN_UP, PD_BTN_SELECT, PD_BTN_A, -1, PD_BTN_A, PD_BTN_B, -1, PD_BTN_B };
    run(argv[1], rom, alias, (int) (sizeof(alias) / sizeof(alias[0])));
    expect_text(save, "playing save");
    expect_text(aliasSave, "playing save");
    expect_text(resume, "playing resume");

    // A missing backup cannot alter a .sav, and an existing one can restore it.
    char second[1024], secondRom[1024], secondSave[1024];
    path(second, sizeof(second), argv[1], "second");
    assert(mkdir(second, 0700) == 0);
    path(secondRom, sizeof(secondRom), second, "test.gba");
    path(secondSave, sizeof(secondSave), second, "test.sav");
    write_text(secondRom, "synthetic rom");
    write_text(secondSave, "original sav");
    const int noBackup[] = { PD_BTN_SELECT, PD_BTN_A, PD_BTN_B, -1, PD_BTN_B };
    run(second, NULL, noBackup, (int) (sizeof(noBackup) / sizeof(noBackup[0])));
    expect_text(secondSave, "original sav");
    path(backup, sizeof(backup), second, "test.backup-20200101-000000.sav");
    write_text(backup, "restored sav");
    const int restoreSav[] = { PD_BTN_SELECT, PD_BTN_A, -1, PD_BTN_A, -1, PD_BTN_A,
                               PD_BTN_B, -1, PD_BTN_B };
    run(second, NULL, restoreSav, (int) (sizeof(restoreSav) / sizeof(restoreSav[0])));
    expect_text(secondSave, "restored sav");
    assert(pd_latest_backup(secondSave, latest, sizeof(latest)));
    expect_text(latest, "original sav");
    SDL_Quit();
    puts("library: navigation, cancel, .sav/.srm restore, missing backup and playing guard ok");
    return 0;
}
