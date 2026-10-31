// Exercise the Linux D-pad cursor against the real bag and its RAM fixture.
#include "companion.h"
#include "pd_ui_internal.h"
#include "pd_png.h"
#include "pd_settings.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char ewram[0x40000], iwram[0x8000];

static void load(const char* dir, const char* name, unsigned char* out, size_t size) {
    char path[1024];
    int n = snprintf(path, sizeof(path), "%s/%s", dir, name);
    assert(n > 0 && (size_t) n < sizeof(path));
    FILE* f = fopen(path, "rb");
    assert(f && fread(out, 1, size, f) == size);
    assert(fclose(f) == 0);
}

static bool read_ram(void* ctx, uint32_t addr, void* out, size_t len) {
    (void) ctx;
    if (addr >= 0x02000000 && (size_t) (addr - 0x02000000) + len <= sizeof(ewram)) {
        memcpy(out, ewram + (addr - 0x02000000), len);
        return true;
    }
    if (addr >= 0x03000000 && (size_t) (addr - 0x03000000) + len <= sizeof(iwram)) {
        memcpy(out, iwram + (addr - 0x03000000), len);
        return true;
    }
    return false;
}

static int focused(struct pd_companion* c) {
    int focus;
    pd_companion_draw(c);
    pd_companion_debug(NULL, NULL, &focus);
    return focus;
}

static void press(struct pd_companion* c, enum pd_nav nav) {
    assert(pd_companion_nav(c, nav));
    pd_companion_draw(c);
}

int main(int argc, char** argv) {
    assert(argc == 4 || argc == 5);
    load(argv[1], "ewram.bin", ewram, sizeof(ewram));
    load(argv[1], "iwram.bin", iwram, sizeof(iwram));
    unsigned char smallRom[256] = { 0 };
    assert(strlen(argv[3]) == 4);
    memcpy(smallRom + 0xAC, argv[3], 4);
    const unsigned char* romData = smallRom;
    size_t romSize = sizeof(smallRom);
    struct pd_game g;
    pd_game_detect(&g, smallRom, sizeof(smallRom));
    struct pd_snapshot snap;
    pd_snapshot_read(&snap, &g, read_ram, NULL);
    int count = 0, firstItem = 0, lastItem = 0;
    for (int i = 0; i < PD_POCKET_COUNT; i++) {
        const struct pd_pocket_items* pocket = &snap.bag[g.cfg->bagOrder[i]];
        if (pocket->count && !firstItem) firstItem = pocket->items[0].id;
        if (pocket->count) lastItem = pocket->items[pocket->count - 1].id;
        count += pocket->count;
    }
    assert(snap.bagOk && count > 10);

    struct pd_companion* c = pd_companion_open("fixture.gba", argv[2], romData, romSize);
    assert(c);
    pd_companion_show(c, true);
    pd_companion_resumed(c);
    pd_companion_tick(c, read_ram, NULL, 0, 0, false);
    pd_companion_draw(c);
    press(c, PD_NAV_TAB_NEXT); // MAP
    press(c, PD_NAV_TAB_NEXT); // BAG
    pd_companion_tick(c, read_ram, NULL, 1, 300, false); // finish the tab slide
    pd_companion_draw(c);
    press(c, PD_NAV_RIGHT);
    press(c, PD_NAV_RIGHT);
    press(c, PD_NAV_DOWN);
    if (focused(c) == HIT_BAG_TAB) press(c, PD_NAV_RIGHT);
    if (focused(c) != HIT_BAG_ROW) fprintf(stderr, "first bag focus: %d\n", focused(c));
    assert(focused(c) == HIT_BAG_ROW);

    // Select an item, move directly to its star, and toggle it on and off.
    press(c, PD_NAV_OK);
    press(c, PD_NAV_RIGHT);
    assert(focused(c) == HIT_BAG_STAR);
    const char* starShot = getenv("PD_BAG_TEST_STAR_SHOT");
    if (starShot) {
        char path[1024];
        int n = snprintf(path, sizeof(path), "%s-%s.png", starShot, argv[3]);
        assert(n > 0 && (size_t) n < sizeof(path));
        assert(pd_png_write(path, PD_COMPANION_W, PD_COMPANION_H, pd_companion_draw(c)));
    }
    press(c, PD_NAV_OK);
    assert(pd_companion_save_settings(c));
    struct pd_settings saved;
    assert(pd_settings_load(&saved, argv[2]) && saved.favs[0].count == 1 && saved.favs[0].items[0] == firstItem);
    press(c, PD_NAV_OK);
    assert(pd_companion_save_settings(c));
    assert(pd_settings_load(&saved, argv[2]) && saved.favs[0].count == 0);
    press(c, PD_NAV_LEFT);
    assert(focused(c) == HIT_BAG_ROW);

    for (int i = 1; i < count; i++) {
        press(c, PD_NAV_DOWN);
        assert(focused(c) == HIT_BAG_ROW + i);
        const char* shot = getenv("PD_BAG_TEST_SHOT");
        if (shot && i == 8) {
            char path[1024];
            int n = snprintf(path, sizeof(path), "%s-%s.png", shot, argv[3]);
            assert(n > 0 && (size_t) n < sizeof(path));
            assert(pd_png_write(path, PD_COMPANION_W, PD_COMPANION_H, pd_companion_draw(c)));
        }
    }
    assert(!pd_companion_nav(c, PD_NAV_DOWN));
    assert(focused(c) == HIT_BAG_ROW + count - 1);
    press(c, PD_NAV_OK); // the last item is selectable, not PC
    assert(focused(c) == HIT_BAG_ROW + count - 1);
    press(c, PD_NAV_RIGHT);
    assert(focused(c) == HIT_BAG_STAR);
    press(c, PD_NAV_OK);
    assert(pd_companion_save_settings(c));
    assert(pd_settings_load(&saved, argv[2]) && saved.favs[0].count == 1 && saved.favs[0].items[0] == lastItem);
    press(c, PD_NAV_OK);
    press(c, PD_NAV_LEFT);
    assert(focused(c) == HIT_BAG_ROW + count - 1);
    for (int i = count - 2; i >= 0; i--) {
        press(c, PD_NAV_UP);
        assert(focused(c) == HIT_BAG_ROW + i);
    }
    press(c, PD_NAV_LEFT); // leave the list toward its pocket buttons
    assert(focused(c) < HIT_BAG_ROW || focused(c) >= HIT_BAG_ROW + count);
    for (int i = 0; i < PD_POCKET_COUNT + 3 && focused(c) != HIT_BAG_PC; i++) press(c, PD_NAV_DOWN);
    assert(focused(c) == HIT_BAG_PC);
    press(c, PD_NAV_OK);
    pd_companion_close(c);
    printf("companion: %s bag D-pad selects and scrolls %d items in both directions\n", argv[3], count);

    if (argc == 5) {
        // A real ROM gives the DEX its tables: walk the whole list down and
        // back up, checking that the cursor lands on a different entry each
        // press and returns exactly to where it started.
        FILE* f = fopen(argv[4], "rb");
        assert(f);
        assert(fseek(f, 0, SEEK_END) == 0);
        long size = ftell(f);
        assert(size > 0 && fseek(f, 0, SEEK_SET) == 0);
        unsigned char* real = malloc((size_t) size);
        assert(real && fread(real, 1, (size_t) size, f) == (size_t) size);
        assert(fclose(f) == 0);
        romData = real;
        romSize = (size_t) size;
        c = pd_companion_open("fixture.gba", argv[2], romData, romSize);
        assert(c);
        pd_companion_show(c, true);
        pd_companion_resumed(c);
        pd_companion_tick(c, read_ram, NULL, 0, 0, false);
        pd_companion_draw(c);
        press(c, PD_NAV_TAB_NEXT); // DEX
        pd_companion_tick(c, read_ram, NULL, 1, 300, false);
        pd_companion_draw(c);
        for (int i = 0; i < 8; i++) {
            int f = focused(c);
            if (f >= HIT_DEX_ROW && f < HIT_DEX_ROW + PD_NATIONAL_COUNT) break;
            press(c, PD_NAV_DOWN);
        }
        int first = focused(c);
        assert(first >= HIT_DEX_ROW && first < HIT_DEX_ROW + PD_NATIONAL_COUNT);
        int steps = 0, last = first;
        for (int i = 0; i < 60; i++) {
            if (!pd_companion_nav(c, PD_NAV_DOWN)) break;
            pd_companion_draw(c);
            int now = focused(c);
            assert(now >= HIT_DEX_ROW && now < HIT_DEX_ROW + PD_NATIONAL_COUNT);
            assert(now != last);
            last = now;
            steps++;
            if (steps == 20 && getenv("PD_DEX_TEST_SHOT")) {
                char path[1024];
                int n = snprintf(path, sizeof(path), "%s-%s.png", getenv("PD_DEX_TEST_SHOT"), argv[3]);
                assert(n > 0 && (size_t) n < sizeof(path));
                assert(pd_png_write(path, PD_COMPANION_W, PD_COMPANION_H, pd_companion_draw(c)));
            }
        }
        assert(steps > 10);
        for (int i = 0; i < steps; i++) {
            assert(pd_companion_nav(c, PD_NAV_UP));
            pd_companion_draw(c);
            assert(focused(c) >= HIT_DEX_ROW && focused(c) < HIT_DEX_ROW + PD_NATIONAL_COUNT);
        }
        assert(focused(c) == first);
        pd_companion_close(c);
        free(real);
        printf("companion: %s DEX cursor visits %d entries down and back to the first\n", argv[3], steps + 1);
    }
    return 0;
}
