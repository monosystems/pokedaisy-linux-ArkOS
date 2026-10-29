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

"$build/pokedaisy" "$work/smoke.gba" --headless --frames 10 --test-fill --no-resume \
    --screenshot "$work/game.png" --log "$work/game.log"

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

pw, ph, party, prow = decode(os.path.join(work, 'party.png'))
assert (pw, ph) == (640, 480)
bright = sum(1 for y in range(0, ph, 2) for x in range(0, pw, 2) if min(pixel(party, prow, x, y)) > 200)
assert bright > 200, f'the companion drew almost no text ({bright} bright pixels)'
colours = {party[y * prow + 1 + x * 3:y * prow + 4 + x * 3] for y in range(0, ph, 4) for x in range(0, pw, 4)}
assert len(colours) >= 4, f'the companion screen looks flat ({len(colours)} colours)'
assert party != game, 'the companion did not draw over the game'
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
