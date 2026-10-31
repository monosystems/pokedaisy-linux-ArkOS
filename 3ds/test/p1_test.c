// Deterministic P1 checks against the portable UI, without a game or a pad.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "pd_canvas.h"
#include "pd_dex.h"
#include "pd_settings.h"
#include "pd_snapshot.h"
#include "pd_tables.h"
#include "pd_ui_internal.h"

static int failures;
#define CHECK(name, condition) do { \
    bool ok = (condition); \
    printf("%s: %s\n", ok ? "PASS" : "FAIL", name); \
    if (!ok) failures++; \
} while (0)

static uint32_t pixels[PD_UI_WIDTH * PD_UI_HEIGHT];
static struct pd_canvas canvas;
static struct pd_ui ui;
static struct pd_settings settings;
static struct pd_game game;
static struct pd_snapshot snap;
static struct pd_host_info host = { .hasStatusBar = true, .hasLibrary = true, .hasState = true };
static uint8_t rom[256];

static void draw(void) { pd_ui_draw(&ui, &canvas, &game, &snap, &host); }

static unsigned image_hash(void) {
    draw();
    unsigned h = 2166136261u;
    for (int i = 0; i < PD_UI_WIDTH * PD_UI_HEIGHT; i++) h = (h ^ pixels[i]) * 16777619u;
    return h;
}

static bool has_hit(int id) {
    draw();
    for (int i = 0; i < ui.hitCount; i++) if (ui.hits[i].id == id) return true;
    return false;
}

static bool tap(int id) {
    draw();
    for (int i = ui.hitCount - 1; i >= 0; i--) {
        struct pd_hit h = ui.hits[i];
        if (h.id != id || h.w <= 0 || h.h <= 0) continue;
        int x = h.x + h.w / 2, y = h.y + h.h / 2;
        if (x < 0 || x >= PD_UI_WIDTH || y < 0 || y >= PD_UI_HEIGHT) return false;
        pd_ui_touch(&ui, x, y, true);
        pd_ui_touch(&ui, x, y, false);
        return true;
    }
    return false;
}

static int type_id(const char* name) {
    for (int i = 0; i < pd_type_names_count; i++) if (!strcmp(pd_type_name(i), name)) return i;
    return -1;
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) return 2;
    memcpy(rom + 0xAC, "BPRE", 4);
    pd_game_detect(&game, rom, sizeof(rom));
    game.rom = rom;
    game.romSize = sizeof(rom);
    pd_settings_defaults(&settings);
    pd_ui_init(&ui, &settings);
    pd_canvas_init(&canvas, PD_UI_WIDTH, PD_UI_HEIGHT, pixels);
    snap.valid = true;
    snap.foeActive = snap.foeNext = -1;

    draw();
    CHECK("P1.1 only picked available tabs on bar", ui.barCount == 4 && ui.moreCount == 0);
    snap.inBattle = true;
    snap.battlers[PD_POS_PLAYER_LEFT].species = 25;
    pd_ui_update(&ui, &snap);
    draw();
    CHECK("P1.1 battle first and automatic jump", ui.barCount == 5 && ui.bar[0] == PD_TAB_BATTLE && ui.tab == PD_TAB_BATTLE);
    // A scroll left over from an earlier battle must not hide the foe's card.
    ui.battleScroll = 120;
    snap.inBattle = false;
    pd_ui_update(&ui, &snap);
    CHECK("P1.1 return after battle", ui.tab == PD_TAB_PARTY);
    snap.inBattle = true;
    pd_ui_update(&ui, &snap);
    CHECK("P1.11 a new battle starts at the foe's card", ui.battleScroll == 0);
    snap.inBattle = false;
    pd_ui_update(&ui, &snap);

    // Hide the tabs one by one through the same settings hits as the player.
    ui.tab = PD_TAB_SETTINGS;
    ui.settingsPage = PD_SETTINGS_TABS;
    bool hidden = true;
    for (int i = 0; i < PD_TAB_ORDER_COUNT; i++) {
        enum pd_tab tab = PD_TAB_ORDER[i];
        if (!pd_tab_available(&game, tab) || !pd_tab_shown(&settings, tab)) continue;
        if (!tap(HIT_SETTINGS_TAB + tab)) { hidden = false; continue; }
        draw();
        if (pd_tab_shown(&settings, tab) || has_hit(HIT_TAB + tab)) hidden = false;
    }
    CHECK("P1.2 hidden tabs leave the bar one by one", hidden);
    draw();
    CHECK("P1.2 all hidden still leaves MORE and SETTINGS", ui.barCount == 0 && ui.moreCount == 4 &&
          has_hit(HIT_MORE) && has_hit(HIT_TAB + PD_TAB_SETTINGS));
    CHECK("P1.2 MORE opens with no selected tabs", tap(HIT_MORE) && ui.tab == PD_TAB_MORE);
    ui.tabSlideMs = 0;
    pd_ui_next_tab(&ui, 1);
    CHECK("P1.2 shoulder navigation works with an empty bar", ui.tab == PD_TAB_SETTINGS);
    snap.inBattle = true;
    pd_ui_update(&ui, &snap);
    draw();
    CHECK("P1.2 battle remains reachable with all tabs hidden", ui.barCount == 1 && ui.bar[0] == PD_TAB_BATTLE && ui.moreCount == 4);
    snap.inBattle = false;
    pd_ui_update(&ui, &snap);
    settings.tabsShown = PD_TABS_DEFAULT;

    pd_ui_init(&ui, &settings);
    CHECK("P1.3 waits at cold boot", pd_ui_waiting(&ui));
    snap.partyCount = 1;
    pd_ui_update(&ui, &snap);
    CHECK("P1.3 party alone does not end wait", pd_ui_waiting(&ui));
    snap.posOk = true;
    for (int i = 0; i < 4; i++) {
        snap.x = i;
        pd_ui_update(&ui, &snap);
    }
    CHECK("P1.3 fewer than five positions still wait", pd_ui_waiting(&ui));
    snap.x = 4;
    pd_ui_update(&ui, &snap);
    CHECK("P1.3 five positions end wait", !pd_ui_waiting(&ui));
    snap.posOk = false;
    pd_ui_restarted(&ui);
    CHECK("P1.3 restart waits again", pd_ui_waiting(&ui));
    pd_ui_resumed(&ui);
    CHECK("P1.3 resume ends wait", !pd_ui_waiting(&ui));

    settings.tweaks &= ~PD_TWEAK_BIT(PD_TWEAK_JUMP_BATTLE);
    snap.inBattle = true;
    pd_ui_update(&ui, &snap);
    CHECK("P1.2 no jump when disabled", ui.tab == PD_TAB_PARTY);
    snap.inBattle = false;
    pd_ui_update(&ui, &snap);
    ui.tab = PD_TAB_MAP;
    settings.tweaks |= PD_TWEAK_BIT(PD_TWEAK_MAP_CURSOR);
    ui.mapBlink = false;
    ui.blinkMs = 0;
    pd_ui_tick(&ui, 333);
    CHECK("P1.2 map blink on", ui.mapBlink);
    settings.tweaks &= ~PD_TWEAK_BIT(PD_TWEAK_MAP_CURSOR);
    pd_ui_tick(&ui, 333);
    CHECK("P1.2 map blink off", !ui.mapBlink);

    int rock = type_id("Rock"), fire = type_id("Fire"), flying = type_id("Flying");
    CHECK("P1.5 Rock vs Fire/Flying is 4x", rock >= 0 && fire >= 0 && flying >= 0 && pd_type_multiplier_pct(rock, fire, flying) == 400);
    CHECK("P1.5 Bulbasaur medium-slow EXP at level 16", pd_exp_for_level(1, 16) == 2535);
    CHECK("P1.6 Adamant nature", !strcmp(pd_nature_name(3), "ADAMANT") && pd_nature_raises(3) == PD_STAT_ATK && pd_nature_lowers(3) == PD_STAT_SPATK);
    struct pd_mon mon = { 0 };
    CHECK("P1.6 zero IV Hidden Power", pd_hidden_power_type(&mon) == 1 && pd_hidden_power_power(&mon) == 30);
    memset(mon.ivs, 31, sizeof(mon.ivs));
    CHECK("P1.6 maximum IV Hidden Power", pd_hidden_power_type(&mon) == 17 && pd_hidden_power_power(&mon) == 70);

    // Synthetic party and bag data exercise the real draw and touch paths.
    settings.tweaks &= ~PD_TWEAK_BIT(PD_TWEAK_TAB_ANIM);
    ui.tab = PD_TAB_PARTY;
    ui.tabSlideMs = 0;
    snap.party[0].species = 25;
    snap.party[0].level = 20;
    snap.party[0].hp = snap.party[0].maxHp = 50;
    strcpy(snap.party[0].nickname, "PIKACHU");
    snap.party[0].heldItem = 0;
    unsigned plainParty = image_hash();
    snap.party[0].heldItem = 13;
    CHECK("P1.4 held item changes party rendering", image_hash() != plainParty);
    CHECK("P1.5 summary opens", tap(HIT_SLOT) && ui.summarySlot == 0);
    CHECK("P1.6 stats page opens", tap(HIT_SUMMARY_PAGE + PD_SUMMARY_STATS) && ui.summaryPage == PD_SUMMARY_STATS);
    CHECK("P1.5 matchups page opens", tap(HIT_SUMMARY_PAGE + PD_SUMMARY_MATCHUPS) && ui.summaryPage == PD_SUMMARY_MATCHUPS);

    ui.tab = PD_TAB_BAG;
    ui.summarySlot = -1;
    snap.bagOk = true;
    snap.bag[PD_POCKET_ITEMS].count = 2;
    snap.bag[PD_POCKET_ITEMS].items[0] = (struct pd_item) { 13, 5 };
    snap.bag[PD_POCKET_ITEMS].items[1] = (struct pd_item) { 14, 2 };
    snap.bag[PD_POCKET_BALLS].count = 1;
    snap.bag[PD_POCKET_BALLS].items[0] = (struct pd_item) { 4, 3 };
    ui.bagPocket = PD_BAG_ALL;
    ui.bagSelected = -1;
    CHECK("P1.7 ALL shows items from both pockets", has_hit(HIT_BAG_ROW + 2));
    bool orders = true;
    for (int i = 1; i <= PD_BAG_ORDERS; i++) orders &= tap(HIT_BAG_ORDER) && ui.bagOrder == i % PD_BAG_ORDERS;
    CHECK("P1.7 all four order controls", orders);
    CHECK("P1.8 select item and star", tap(HIT_BAG_ROW) && tap(HIT_BAG_STAR) && settings.favs[0].count == 1);
    CHECK("P1.8 favorites tab appears", has_hit(HIT_BAG_TAB + 3));
    CHECK("P1.8 remove favorite", tap(HIT_BAG_STAR) && settings.favs[0].count == 0);
    CHECK("P1.9 PC notice", tap(HIT_BAG_PC) && ui.bagPcNotice);

    ui.tab = PD_TAB_SETTINGS;
    ui.settingsPage = PD_SETTINGS_SPEED;
    ui.tabSlideMs = 0;
    fflush(stdout);
    pid_t child = fork();
    if (child == 0) {
        bool allSpeeds = true;
        for (int i = 0; i < PD_FF_STEPS; i++) {
            ui.overlay = PD_OVERLAY_NONE;
            if (!tap(HIT_SETTING + SETTING_FF) || !tap(HIT_OPTION + i) || settings.ffSpeed != PD_FF_SPEEDS[i]) {
                fprintf(stderr, "fast-forward step %d (%d%%) could not be selected\n", i, PD_FF_SPEEDS[i]);
                allSpeeds = false;
            }
        }
        _exit(allSpeeds ? 0 : 1);
    }
    int status = 0;
    bool completed = child > 0 && waitpid(child, &status, 0) == child;
    CHECK("P1.14 fast-forward selector and all steps", completed && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    if (completed && WIFSIGNALED(status)) printf("  selector terminated by signal %d\n", WTERMSIG(status));
    fflush(stdout);
    child = fork();
    if (child == 0) {
        bool ok = true;
        for (int i = 0; i < PD_GAME_SPEEDS; i++) {
            ui.overlay = PD_OVERLAY_NONE;
            if (!tap(HIT_SETTING + SETTING_GAME_SPEED) || !tap(HIT_OPTION + i) ||
                settings.gameSpeed != PD_GAME_SPEED_STEPS[i]) ok = false;
        }
        _exit(ok ? 0 : 1);
    }
    status = 0;
    completed = child > 0 && waitpid(child, &status, 0) == child;
    CHECK("P1.14 game speed selector", completed && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    if (completed && WIFSIGNALED(status)) printf("  selector terminated by signal %d\n", WTERMSIG(status));
    // Exercise the ninth option independently of drawing the crashing selector.
    ui.overlay = PD_OVERLAY_PICK_FF;
    settings.ffSpeed = 200;
    ui.hitCount = 1;
    ui.hits[0] = (struct pd_hit) { .x = 10, .y = 10, .w = 20, .h = 20, .id = HIT_OPTION + 8 };
    pd_ui_touch(&ui, 15, 15, true);
    pd_ui_touch(&ui, 15, 15, false);
    CHECK("P1.14 INFINITE option accepted", settings.ffSpeed == 0 && ui.overlay == PD_OVERLAY_NONE);
    settings.gameSpeed = 200;
    settings.slowMotion = false;
    settings.statusBar = true;
    settings.legacyFavs[0].rom = 1;
    settings.legacyFavs[0].items[0] = 42;
    settings.legacyFavs[0].count = 1;
    settings.favs[0].rom = pd_favorite_rom_id(&game);
    settings.favs[0].items[0] = 13;
    settings.favs[0].count = 1;
    CHECK("settings write", pd_settings_save(&settings, argv[1]));
    struct pd_settings loaded;
    CHECK("settings read", pd_settings_load(&loaded, argv[1]));
    CHECK("P1.14 game speed persists", loaded.gameSpeed == 200);
    CHECK("P1.14 slow motion persists", !loaded.slowMotion);
    CHECK("P1.15 status bar persists", loaded.statusBar);
    CHECK("P1.8 stable favorites persist", loaded.favs[0].rom == settings.favs[0].rom && loaded.favs[0].items[0] == 13);
    CHECK("P1.8 legacy favorites preserved separately", loaded.legacyFavs[0].rom == 1 && loaded.legacyFavs[0].items[0] == 42);

    ui.overlay = PD_OVERLAY_NONE;
    ui.settingsPage = PD_SETTINGS_SAVES;
    pd_ui_take_action(&ui);
    CHECK("P1.13 restart opens confirm", tap(HIT_SETTING + SETTING_RESTART) && ui.overlay == PD_OVERLAY_CONFIRM_RESTART);
    CHECK("P1.13 cancel restart", tap(HIT_NO) && ui.overlay == PD_OVERLAY_NONE && pd_ui_take_action(&ui) == PD_ACTION_NONE);
    CHECK("P1.13 confirm restart", tap(HIT_SETTING + SETTING_RESTART) && tap(HIT_YES) && pd_ui_take_action(&ui) == PD_ACTION_RESTART_GAME);
    CHECK("P1.13 leave cancel", tap(HIT_SETTING + SETTING_LEAVE) && tap(HIT_NO) && pd_ui_take_action(&ui) == PD_ACTION_NONE);
    CHECK("P1.13 confirm leave", tap(HIT_SETTING + SETTING_LEAVE) && tap(HIT_YES) && pd_ui_take_action(&ui) == PD_ACTION_LEAVE_GAME);
    ui.settingsPage = PD_SETTINGS_BATTLE;
    bool hints = pd_tweak_on(&settings, PD_TWEAK_BATTLE_HINTS);
    CHECK("P1.11 battle hints toggle", tap(HIT_SETTING + SETTING_HINTS) && pd_tweak_on(&settings, PD_TWEAK_BATTLE_HINTS) != hints);

    // The widest heading must not be covered by the first chip after it.
    static uint32_t mbuf[PD_UI_WIDTH * PD_UI_HEIGHT];
    struct pd_canvas mc;
    pd_canvas_init(&mc, PD_UI_WIDTH, PD_UI_HEIGHT, mbuf);
    pd_fill(&mc, 0, 0, PD_UI_WIDTH, PD_UI_HEIGHT, 0x000000);
    struct pd_matchups immuneOnly = { 0 };
    immuneOnly.immune[0] = (struct pd_matchup) { 8, 0 }; // GHOST, 0x
    immuneOnly.immuneCount = 1;
    int blockH = ui_matchup_block(&mc, 10, 20, 300, &immuneOnly);
    uint32_t chipFill = ui_type_color(8);
    int covered = 0;
    int headingW = pd_text_width("IMMUNE TO");
    for (int yy = 18; yy < 20 + 8; yy++)
        for (int xx = 10; xx < 10 + headingW; xx++)
            if (mc.px[yy * PD_UI_WIDTH + xx] == chipFill) covered++;
    CHECK("P1.11 IMMUNE TO is not covered by its chip", blockH > 0 && covered == 0);
    ui.settingsPage = PD_SETTINGS_COMPANION;
    bool wasStatus = settings.statusBar;
    CHECK("P1.15 status toggle", tap(HIT_SETTING + SETTING_STATUS_BAR) && settings.statusBar != wasStatus);

    unsigned originalId = pd_favorite_rom_id(&game);
    struct pd_game again;
    pd_game_detect(&again, rom, sizeof(rom));
    again.rom = rom;
    again.romSize = sizeof(rom);
    CHECK("P1.8 same ROM keeps its favorites identity on reload", originalId == pd_favorite_rom_id(&again));
    CHECK("P1.8 cache identity still changes per load", pd_rom_id(&game) != pd_rom_id(&again));
    uint8_t otherRom[sizeof(rom)];
    memcpy(otherRom, rom, sizeof(rom));
    otherRom[0] = 1;
    pd_game_detect(&again, otherRom, sizeof(otherRom));
    again.rom = otherRom;
    again.romSize = sizeof(otherRom);
    CHECK("P1.8 different ROM content has separate favorites", originalId != pd_favorite_rom_id(&again));

    // Optional local decomp ROM: never copied into the repository or test data.
    if (argc == 3) {
        FILE* f = fopen(argv[2], "rb");
        long size = 0;
        uint8_t* bytes = NULL;
        if (f && fseek(f, 0, SEEK_END) == 0 && (size = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0) {
            bytes = malloc((size_t) size);
            if (bytes && fread(bytes, 1, (size_t) size, f) != (size_t) size) { free(bytes); bytes = NULL; }
        }
        if (f) fclose(f);
        CHECK("ROM fixture loads", bytes != NULL && size >= 0xC0);
        if (bytes && size >= 0xC0) {
            pd_game_detect(&game, bytes, (size_t) size);
            game.rom = bytes;
            game.romSize = (size_t) size;
            CHECK("P1.7 probed TM05 is ROAR", pd_dex_tm_move(&game, 293) == 46);
            CHECK("P1.10 dex ROM probe", pd_dex_available(&game));
            pd_ui_init(&ui, &settings);
            ui.started = true;
            ui.tab = PD_TAB_DEX;
            snap.dexOk = true;
            snap.dexNational = true;
            snap.dexSeen[0] = 3;
            snap.dexCaught[0] = 1;
            CHECK("P1.10 caught filter", tap(HIT_DEX_FILTER + 1) && ui.dexFilter == 1);
            CHECK("P1.10 caught entry opens", tap(HIT_DEX_ROW + 1) && ui.dexOpen == 1);
            CHECK("P1.10 filtered next wraps", tap(HIT_DEX_NEXT) && ui.dexOpen == 1);
            ui.tab = PD_TAB_GUIDE;
            ui.overlay = PD_OVERLAY_NONE;
            settings.guideNotice = 0xFFFF;
            snap.flagsOk = true;
            CHECK("P1.12 evolution page", tap(HIT_GUIDE_PAGE + 5) && ui.guidePage == 5);
            CHECK("P1.12 boss page", tap(HIT_GUIDE_PAGE + 1) && ui.guidePage == 1);
        }
        free(bytes);
    } else {
        puts("SKIP: P1.7 ROM TM probe, P1.10 dex and P1.12 guide (no local FireRed ROM fixture)");
    }

    printf("P1 portable checks: %d failure(s)\n", failures);
    return failures ? 1 : 0;
}
