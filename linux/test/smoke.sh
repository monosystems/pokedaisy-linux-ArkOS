#!/bin/sh
# Headless smoke test: no game and no screen needed. Writes a looping GBA ROM
# (with FireRed's game code, so the companion recognises it), runs it through
# the real emulator, and checks three screenshots:
#
#   game.png   the game's own frame, painted red through mGBA (--test-fill):
#              core, frame loop, texture, scaling and PNG writer
#   party.png  the companion over it: the portable UI, its 2x scaling, the cursor
#              - the waiting view (C-05), because this ROM never reaches a field
#   bag.png    the same after a tab switch: the D-pad navigation works
#
#   linux/test/smoke.sh [build-dir]        (default: linux/build)
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
build=${1:-$root/linux/build}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

python3 "$here/make_smoke_rom.py" "$work/smoke.gba"

# Exercise backup ordering and restoration without an emulator or real saves.
mkdir -p "$work/files/pokedaisy-states"
${CC:-cc} -std=c11 -D_POSIX_C_SOURCE=200809L -I"$root/linux" -I"$root/3ds/core" \
    "$here/files_test.c" "$root/linux/files.c" "$root/linux/log.c" \
    "$root/3ds/core/pd_settings.c" -o "$work/files-test"
"$work/files-test" "$work/files"

# Exercise the library's real navigation, including cancel and restoration.
mkdir -p "$work/library/pokedaisy-states"
${CC:-cc} -std=c11 -D_POSIX_C_SOURCE=200809L -DCOLOR_16_BIT -DCOLOR_5_6_5 \
    -I"$root/linux" -I"$root/3ds/core" -I"$root/third_party/mgba/include" -I"$build/mgba/include" \
    $(pkg-config --cflags sdl2) "$here/library_test.c" "$root/linux/menu.c" "$root/linux/files.c" \
    "$root/linux/log.c" "$root/3ds/core/pd_canvas.c" "$root/3ds/core/pd_font_gen.c" \
    "$root/3ds/core/pd_menu.c" "$root/3ds/core/pd_style.c" $(pkg-config --libs sdl2) -lm -o "$work/library-test"
SDL_VIDEODRIVER=dummy "$work/library-test" "$work/library"

# The actual Linux cursor must reach every item, including clipped rows.
${CC:-cc} -std=c11 -D_POSIX_C_SOURCE=200809L -I"$root/linux" -I"$root/3ds/core" -I"$root/3ds/desktop" \
    $(pkg-config --cflags sdl2) "$here/companion_bag_test.c" "$root/linux/companion.c" \
    "$root/linux/files.c" "$root/linux/log.c" "$root/3ds/desktop/pd_png.c" "$root"/3ds/core/*.c \
    $(pkg-config --libs sdl2) -lm -o "$work/companion-bag-test"
"$work/companion-bag-test" "$root/app/src/test/resources/fixtures/firered_vanilla" "$work/bag-fr.ini" BPRE
"$work/companion-bag-test" "$root/app/src/test/resources/fixtures/emerald_vanilla" "$work/bag-em.ini" BPEE
fr_rom="$root/3ds/build/decomps/pokefirered/pokefirered_rev1.gba"
[ -f "$fr_rom" ] && "$work/companion-bag-test" "$root/app/src/test/resources/fixtures/firered_vanilla" \
    "$work/dex-fr.ini" BPRE "$fr_rom"

"$build/pokedaisy" "$work/smoke.gba" --headless --frames 10 --test-fill --no-resume \
    --screenshot "$work/game.png" --log "$work/game.log"

printf 'status_bar=1\n' > "$work/status.ini"
# The bar's name comes from the detected game, not the download's file name.
cp "$work/smoke.gba" "$work/Pokemon - Fire Red Version (U) (V1.1).gba"
"$build/pokedaisy" "$work/Pokemon - Fire Red Version (U) (V1.1).gba" --headless --frames 10 --test-fill --no-resume \
    --settings "$work/status.ini" --screenshot "$work/status.png" --log "$work/status.log"
"$build/pokedaisy" "$work/smoke.gba" --headless --frames 10 --companion --no-resume \
    --settings "$work/status.ini" --screenshot "$work/status-companion.png" --log "$work/status-companion.log"

"$build/pokedaisy" "$work/smoke.gba" --headless --frames 10 --companion --no-resume \
    --screenshot "$work/party.png" --log "$work/party.log"

# The bar is the tabs this ROM has, in the app's order (C-04): PARTY, MAP,
# BAG, GUIDE - the test ROM has FireRed's game code but no tables, so no DEX
# chip. Two steps reach BAG, three reach GUIDE; the delay is one frame,
# because a step needs the bar the first draw built.
"$build/pokedaisy" "$work/smoke.gba" --headless --frames 12 --companion --test-nav tabnext,tabnext \
    --test-nav-delay 1 --no-resume --screenshot "$work/bag.png" --log "$work/bag.log"

# The GUIDE tab reads the game's tables through our own copy of the ROM, and it
# must draw even with nothing to list: a stale ROM pointer crashed exactly here.
"$build/pokedaisy" "$work/smoke.gba" --headless --frames 14 --companion --test-nav tabnext,tabnext,tabnext \
    --test-nav-delay 1 --no-resume --screenshot "$work/guide.png" --log "$work/guide.log"

# 3. The launcher: the port starts without a game, so the list and its empty
# state are screens too (picked here without a pad, to keep the run headless).
mkdir -p "$work/roms"
cp "$work/smoke.gba" "$work/roms/Pokemon Test.gba"
printf 'test save' > "$work/roms/Pokemon Test.sav"
printf 'older save' > "$work/roms/Pokemon Test.backup-20200101-000000.sav"
PD_TEST_MENU_VIEW=info "$build/pokedaisy" --headless --roms "$work/roms" --pick test --menu-shot "$work/info.png" \
    --frames 1 --no-resume --log "$work/info.log"
PD_TEST_MENU_VIEW=backups "$build/pokedaisy" --headless --roms "$work/roms" --pick test --menu-shot "$work/backups.png" \
    --frames 1 --no-resume --log "$work/backups.log"
PD_TEST_MENU_VIEW=confirm "$build/pokedaisy" --headless --roms "$work/roms" --pick test --menu-shot "$work/confirm.png" \
    --frames 1 --no-resume --log "$work/confirm.log"
PD_TEST_MENU_VIEW=playing "$build/pokedaisy" --headless --roms "$work/roms" --pick test --menu-shot "$work/playing.png" \
    --frames 1 --no-resume --log "$work/playing.log"
"$build/pokedaisy" --headless --roms "$work/roms" --pick test --menu-shot "$work/menu.png" \
    --frames 4 --screenshot "$work/menu-game.png" --log "$work/menu.log"

mkdir -p "$work/empty"
"$build/pokedaisy" --headless --roms "$work/empty" --pick x --menu-shot "$work/menu-empty.png" \
    --log "$work/empty.log"

# 4. Resume: leaving a game writes a state; starting it again carries on from
# it, so the screen the first run painted is back without painting it again.
"$build/pokedaisy" "$work/smoke.gba" --headless --frames 6 --test-fill --log "$work/resume-a.log"
"$build/pokedaisy" "$work/smoke.gba" --headless --frames 3 --screenshot "$work/resume.png" \
    --log "$work/resume-b.log"
# ... and what it does to the companion (C-05): a boot that carried a resume on
# is in the game already, so its tabs show instead of the cold boot's waiting
# view (party.png).
"$build/pokedaisy" "$work/smoke.gba" --headless --frames 4 --companion \
    --screenshot "$work/resume-companion.png" --log "$work/resume-companion.log"

python3 - "$work" <<'EOF'
import os
import struct
import sys


def decode(path):
    """pd_png_write's output: RGB8, filter 0, stored deflate blocks."""
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', f'{path}: not a PNG'
    w, h = struct.unpack('>II', data[16:24])
    pos, idat = 8, b''
    while pos < len(data):
        n = struct.unpack('>I', data[pos:pos + 4])[0]
        kind = data[pos + 4:pos + 8]
        if kind == b'IDAT':
            idat += data[pos + 8:pos + 8 + n]
        pos += 12 + n
    assert idat[:2] == b'\x78\x01', f'{path}: unexpected zlib header'
    raw, p = b'', 2
    while p < len(idat):
        final = idat[p] & 1
        assert ((idat[p] >> 1) & 3) == 0, f'{path}: compressed deflate block'
        n = struct.unpack('<H', idat[p + 1:p + 3])[0]
        raw += idat[p + 5:p + 5 + n]
        p += 5 + n
        if final:
            break
    row = w * 3 + 1
    assert len(raw) >= row * h, f'{path}: short pixel data'
    return w, h, raw, row


def pixel(raw, row, x, y):
    at = y * row + 1 + x * 3
    return raw[at:at + 3]


work = sys.argv[1]

w, h, game, grow = decode(os.path.join(work, 'game.png'))
assert (w, h) == (640, 480), f'{w}x{h}, wanted 640x480'
assert pixel(game, grow, w // 2, h // 2) == b'\xff\x00\x00', 'the game frame is not red'

_, _, status, srow = decode(os.path.join(work, 'status.png'))
# The OPTION title window's white fill inside its frame, in the strip's row.
assert pixel(status, srow, w // 2, 10) == b'\xff\xff\xff', 'the status bar is not shown'
assert pixel(status, srow, w // 2, 30) != b'\xff\x00\x00', 'the status bar is shorter than its row'
assert pixel(status, srow, w // 2, h // 2) == b'\xff\x00\x00', 'the status bar covered the game'
status_log = open(os.path.join(work, 'status.log')).read()
assert 'status: game FIRERED' in status_log, 'the bar used the file name instead of the game name'
assert 'POKEMON - FIRE' not in status_log, 'the file name leaked into the bar'

pw, ph, party, prow = decode(os.path.join(work, 'party.png'))
assert (pw, ph) == (640, 480)
bright = sum(1 for y in range(0, ph, 2) for x in range(0, pw, 2) if min(pixel(party, prow, x, y)) > 200)
assert bright > 200, f'the companion drew almost no text ({bright} bright pixels)'
colours = {party[y * prow + 1 + x * 3:y * prow + 4 + x * 3] for y in range(0, ph, 4) for x in range(0, pw, 4)}
assert len(colours) >= 4, f'the companion screen looks flat ({len(colours)} colours)'
assert party != game, 'the companion did not draw over the game'
_, _, status_party, _ = decode(os.path.join(work, 'status-companion.png'))
assert status_party == party, 'the status bar covered the companion'
# C-05: this ROM never reaches a field, so the save's tabs wait.
assert 'waiting 1' in open(os.path.join(work, 'party.log')).read(), 'a cold boot did not wait'

_, _, bag, _ = decode(os.path.join(work, 'bag.png'))
assert bag != party, 'tabnext did not change the screen'

_, _, guide, _ = decode(os.path.join(work, 'guide.png'))
assert guide != bag, 'the GUIDE tab did not draw'
guide_log = open(os.path.join(work, 'guide.log')).read()
assert 'draw tab 3' in guide_log, 'the GUIDE tab was never drawn'
assert 'crash:' not in guide_log, 'the GUIDE tab crashed'

for name in ('game.log', 'party.log', 'bag.log'):
    text = open(os.path.join(work, name)).read()
    assert 'frames' in text, f'{name}: no frame count'
assert 'companion: Pokémon FireRed' in open(os.path.join(work, 'party.log')).read(), 'the companion did not start'

# The launcher's list, and what it says with no games in the folder.
for shot in ('menu.png', 'menu-empty.png'):
    mw, mh, menu, mrow = decode(os.path.join(work, shot))
    assert (mw, mh) == (640, 480), f'{shot}: {mw}x{mh}'
    bright = sum(1 for y in range(0, mh, 2) for x in range(0, mw, 2) if min(pixel(menu, mrow, x, y)) > 200)
    assert bright > 200, f'{shot}: the list drew almost no text ({bright} bright pixels)'

_, _, info, _ = decode(os.path.join(work, 'info.png'))
_, _, backups, _ = decode(os.path.join(work, 'backups.png'))
_, _, confirm, _ = decode(os.path.join(work, 'confirm.png'))
assert info != backups and info != confirm and backups != confirm, 'library pages did not change'
_, _, playing, _ = decode(os.path.join(work, 'playing.png'))
_, _, menu, _ = decode(os.path.join(work, 'menu.png'))
assert playing != menu, 'the playing marker did not change the library list'

menu_log = open(os.path.join(work, 'menu.log')).read()
assert 'menu: 1 game(s)' in menu_log, 'the list did not find the one game'
assert 'Pokemon Test.gba' in menu_log, 'the picked game was not started'
assert 'companion: Pokémon FireRed' in menu_log, 'the picked game did not reach the companion'
assert 'no game picked' in open(os.path.join(work, 'empty.log')).read(), 'an empty folder did not stop cleanly'

# Resume: the second run starts from the first one's moment, painted screen and all.
_, _, resumed, rrow = decode(os.path.join(work, 'resume.png'))
assert pixel(resumed, rrow, w // 2, h // 2) == b'\xff\x00\x00', 'resume did not bring the painted screen back'
resume_b = open(os.path.join(work, 'resume-b.log')).read()
assert 'resume: carried on' in resume_b, 'the second run did not load the resume state'
assert 'resume: saved' in open(os.path.join(work, 'resume-a.log')).read(), 'leaving did not write a resume state'
# C-05: the resumed boot is in the game, so the companion's tabs draw instead of
# the waiting view the cold boot (party.png) showed.
_, _, resumed_party, _ = decode(os.path.join(work, 'resume-companion.png'))
assert resumed_party != party, 'a resumed boot still drew the waiting view'
resume_companion = open(os.path.join(work, 'resume-companion.log')).read()
assert 'waiting 0' in resume_companion, 'the resumed boot kept waiting'

print(f'smoke: ok (game, companion, tab switch, game list, resume; {bright} bright pixels)')
EOF
