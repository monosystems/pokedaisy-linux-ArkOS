# PokeDaisy for Linux handhelds (early preview)

An SDL2 port of PokeDaisy for one-screen Linux handhelds. The target is
**ArkOS4Clone on the R36S** (Rockchip RK3326, 640x480), and the same binary runs
in a window on a desktop, which is where the development happens.

The game runs on **libmgba** in-process, and the companion (party, bag, map,
guide, dex, battle, trainer card) is drawn as an overlay over it. The companion
is the portable C core from [`3ds/core`](../3ds/core) - unchanged, shared with
the 3DS port - so both ports read and draw the game the same way.

**Status: phase 3.** The host boots a GBA game with sound, controller input,
saves, save backups, one save state and fast-forward; the companion runs as an
overlay over the game, driven by a D-pad cursor instead of the 3DS's stylus; and
started without a game it lists `/roms/gba` so the PortMaster port can be
launched from the Ports menu. `arkos/` holds the port package, its build script
and `deploy.sh`. It has run on the R36S: the DEX's EVOLVE was checked there, and
all three SCREEN modes hold 58.5 fps with 9.9 ms in the emulator (`PLAN.md`
phase 3 has the table). [`PLAN.md`](PLAN.md) has the phases.

## Build (desktop)

```sh
brew install cmake                                  # macOS
# Linux: apt install cmake libsdl2-dev
git submodule update --init third_party/mgba
cmake -S linux -B linux/build -DCMAKE_BUILD_TYPE=Release
cmake --build linux/build -j
```

## Play

```sh
./linux/build/pokedaisy "/path/to/Pokemon Emerald.gba"
```

| Input | Does |
|---|---|
| `X` / `Z` (pad A / B) | GBA A / B |
| `A` / `S` (pad L1 / R1) | GBA L / R |
| `Enter` (pad START), `Backspace` (pad SELECT) | GBA START / SELECT |
| arrows (pad D-pad or left stick) | move |
| `Space` or pad `R2`, held | fast-forward (muted) |
| `Q` or pad `L2` | open and close the companion |
| `START+SELECT`, held | leave the game (the port needs this: the Ports menu has no Back) |
| `Esc` or closing the window | leave the game |

Started without a game it lists the `.gba` files of `/roms/gba` (or `--roms
DIR`): D-pad picks, A starts, START quits. That is what the PortMaster port
does.

## The companion

`L2` slides the companion over the game; the game keeps running underneath and
doesn't see the buttons while it is up. There is no touchscreen, so a **cursor**
does the tapping: the D-pad moves it onto the next real control on the screen
(the game's own OPTION-look tabs, rows and buttons), `A` presses what it is on,
`B` goes back, `L1`/`R1` change tab, `START` (or `L2`) puts it away again. On a
list, pushing past the last row scrolls it, the way a finger drag would.

The tabs are PARTY, BAG, MAP, GUIDE, DEX, BATTLE (it takes the DEX slot during a
battle and gives it back afterwards), SETTINGS (the gear) and the TRAINER CARD
(from SETTINGS). SETTINGS holds the screen mode, the fast-forward speed, SAVE
STATE and LOAD STATE (one slot per game, beside the ROM in
`pokedaisy-states/`), and LEAVE GAME. SAVE and LOAD ask first; `A` on YES does
it. A save state never carries the battery save, and the save is backed up
before every load.

What the companion can read is the portable core's list, not every game:
FireRed, LeafGreen and Emerald, English, so far - `pd_game`'s game codes. Any
other GBA game plays, and the overlay says so instead of guessing. Hacks over
16 MB are recognised as hacks and left alone for now; more games and languages
are phase 5 in the plan.

**One wart:** the SETTINGS tab's SCREEN row uses the 3DS port's labels
(`SHARP 1.5x`, `PIXEL 1x`). On this port they mean *fit the screen*, *whole
pixels only* and *stretch*, which is what the host does with them.

`--fullscreen` is what the handheld's launch script uses. `--scale fit` (the
default) fills the screen with whole pixels left as they are; `integer` keeps
2x pixels exactly (480x320 on a 640x480 screen); `stretch` fills both ways.
`--rotation 90` and friends handle a panel that is mounted sideways.

## Saves

The save is `<rom>.srm` when one already exists (RetroArch's name), otherwise
`<rom>.sav` (mGBA's), next to the ROM. Before every start the save is copied to
`<rom>.backup-YYYYmmdd-HHMMSS.sav` next to it, newest 10 kept - the repo's "save
files are sacred" rule, and the same thing the 3DS port does.

**Carry on where you left off.** Leaving a game writes
`pokedaisy-states/<rom>.resume`, and starting the game again loads it, so
closing the port (or the device going to sleep) doesn't cost the walk back from
the last save. A save written after that moment wins: if you played the game in
RetroArch or copied a newer save in, the old resume is ignored instead of
rolling your progress back. `--no-resume` always starts at the title screen.
The one-slot save state in the companion's SETTINGS is separate from this.

## If a game won't open

`can't open <path>: Operation not permitted`, with the right file size in the
message, means the operating system refused the read - not that the path is
wrong. On macOS the terminal is kept out of Desktop, Documents and Downloads
until you allow it: System Settings > Privacy & Security > Files and Folders
(or Full Disk Access), then start the terminal again. Keeping the games in an
ordinary folder such as `~/roms` avoids the question. On the handheld the same
message means the file isn't readable there (permissions on the exFAT
partition), not that the game is unsupported.

## Test

```sh
./linux/test/smoke.sh
```

No game needed: the script writes a looping GBA ROM (with FireRed's game code),
runs it headless - the game's own frame painted red through the emulator
(`--test-fill`), the companion over it (`--companion`), the companion after a tab
switch (`--test-nav tabnext`), and the launcher's game list and empty state
(`--roms`, `--pick`, `--menu-shot`) - and checks every screenshot and log. That
covers the core, the frame loop, the texture upload, the scaling, the portable
UI, the cursor navigation, the launcher and the PNG writer in one run. It is
what CI runs.

`--companion` and `--test-nav LIST` are test hooks: the first opens the overlay
at start, the second feeds it presses one per frame (`down,tabnext,ok`), so a
screenshot can show any tab or dialog without a pad. `--pick NAME` and
`--menu-shot FILE` do the same for the launcher's list. Together with
`--frames N --screenshot FILE` they are how the UI is looked at without the
device.

On a new device, `--stats` is the first thing to turn on: it logs frames per
second, the frame time split into emulator, audio and drawing, and how much audio
is queued, once a second. The launch script already passes it.

```sh
./linux/build/pokedaisy "/path/to/game.gba" --stats --log /tmp/pd.log
grep -E 'system:|stats:' /tmp/pd.log
```

Without ssh, the same measurement comes off the card. Start the port from the
Ports menu, pick a game, play, leave with `START+SELECT`, then read
`/roms/ports/pokedaisy/log.txt`: the launch script passes `--stats` and sends the
log there, and it truncates that file at every start, so copy it off the card
before starting the next session. The script passes no `--scale`, so the mode is
the one in `pokedaisy.ini` (`screen=0` whole pixels, `1` fills the screen,
`2` stretch), and a change of SCREEN made in the companion logs
`screen: <mode>` as it happens. The companion itself logs `companion: shown` and
`companion: hidden`, so a stats block can be read as "the game" or "the game with
the overlay": one session can cover several modes, each with the companion up and
put away.

The startup `system:` lines say which video driver, renderer and display SDL
picked, which audio driver opened at which rate, and the scale and rotation in
use - the first things to look at when the picture or the sound misbehaves. A
panel that is mounted sideways can be turned without editing the script:

```sh
POKEDAISY_ROTATION=90 POKEDAISY_SCALE=integer ./linux/build/pokedaisy game.gba
```

(the same values the `--rotation` and `--scale` options take; the environment is
only used when the option isn't given).

When a D-pad press goes somewhere unexpected, `PD_DEBUG_HITS=1` logs the cursor's
move: where it was, every candidate with its distance score, and the hit it
picked. A left/right press stays in its own row and an up/down press in its own
column (the GUIDE's page chips sit right under the tab bar, and a plain distance
score used to send the cursor up to DEX instead of on to the next chip); the
fallback only runs when the row has nothing further on.

```sh
PD_DEBUG_HITS=1 ./linux/build/pokedaisy game.gba --log /tmp/pd.log
grep 'hits:' /tmp/pd.log
```

## The handheld (ArkOS4Clone on an R36S)

ArkOS4Clone is built on ArkOS-R3XS and has **glibc 2.30**; anything newer needs
its `glibc242` wrapper, which not every image has. Building in `debian:buster`
(glibc 2.28) avoids the wrapper entirely, and Docker (or OrbStack) on Apple
Silicon runs that container natively:

```sh
linux/arkos/build.sh          # -> linux/arkos/PokeDaisy.zip
```

The result needs only `GLIBC_2.27` and libSDL2/libm/libpthread (ArkOS4Clone has
2.30 and builds its own SDL2 with KMSDRM and OpenGL ES), so nothing is bundled
and no `glibc242` wrapper is needed.

The zip is a PortMaster port: unzip it into `/roms/ports/` (or install it with
PortMaster) and it appears in the Ports menu as **PokeDaisy**. From there it
lists `/roms/gba`; `START+SELECT` held leaves a game again.

To test a build without the menu, `linux/arkos/deploy.sh` copies a new binary
straight to the handheld: it copies it as `pokedaisy.new` and renames it over the
old file, so nothing has to be deleted first (that also works while PokeDaisy is
running), and `--run` starts a game and prints its log when it exits.

One-time setup, beside the script (`deploy.conf` is not tracked):

```sh
cp linux/arkos/deploy.conf.example linux/arkos/deploy.conf
$EDITOR linux/arkos/deploy.conf        # host=root@192.168.1.42
```

Then:

```sh
linux/arkos/deploy.sh --build                    # build the aarch64 binary, then copy it
linux/arkos/deploy.sh --full                     # first install: the Ports-menu files too
linux/arkos/deploy.sh --rom "Pokemon Emerald.gba" --run
                                                 # copy, play it, print /tmp/pd.log
linux/arkos/deploy.sh --run                      # copy, open the game list
linux/arkos/deploy.sh --dry-run                  # show the ssh/scp commands
```

`--rom` takes a name in `/roms/gba` or a full path on the device.
EmulationStation is stopped while the game runs and started again afterwards
(`--keep-es` leaves it stopped, `--stop-es` stops it for a copy alone).
`PD_HOST`, `PD_DEST`, `PD_ROMS`, `PD_BINARY` and `PD_LOG` override `deploy.conf`.

`--log` is the only way to see what happened on the device: pass it and read the
file (the deploy script passes `/tmp/pd.log` and tails it). SDL comes from the
image (ArkOS builds its own SDL2 with KMSDRM and OpenGL ES), so nothing is
bundled - if a device ever lacks it, put `libSDL2-2.0.so.0` in
`pokedaisy/lib/` and export `LD_LIBRARY_PATH` in the launch script.

## What is where

| Path | What |
|---|---|
| `main.c` | start-up, the session loop, hotkey dispatch, frame pacing, the companion's actions |
| `companion.c` | the overlay: snapshot reading, the portable UI, the D-pad cursor over its hits, settings |
| `menu.c` | the launcher's game list (`/roms/gba`) and its empty state |
| `render.c` | the game and companion textures, scaling, rotation, screenshots |
| `audio.c` | mGBA's blip output into an SDL audio queue, muted while fast-forwarding |
| `input.c` | pad (hot plug included) and keyboard, the GBA key bits, press edges |
| `files.c` | which save a game uses, the backups, the save state's path |
| `state.c` | save states through mGBA (the RTC only, never the save data) |
| `log.c` | stderr plus `--log FILE` |
| `test/` | the smoke ROM writer and the headless smoke test |
| `arkos/` | the PortMaster package: `PokeDaisy.sh`, `port.json`, `gameinfo.xml`, `README.md`, `screenshot.png`, `build.sh`, and `deploy.sh` (copies a build to the handheld) |

## Reading the game's data

The companion reads the game's tables through `pd_game.rom` and its RAM through
mGBA's EWRAM/IWRAM blocks. The RAM blocks stay put for the core's lifetime;
**mGBA's ROM block does not**: the core replaces that mapping shortly after
`core->reset()` (its ROM patch/shim rebuilds it), so a pointer taken from
`getMemoryBlock(0x08)` at startup is unmapped by the time the GUIDE tab reads a
table through it. That is what crashed the port on the handheld - `signal 11`
with the fault address exactly `wildHeaders - 0x08000000` past the old mapping.

So `main.c` reads the ROM file once into a buffer of its own (`read_rom_copy`)
and the game struct points there. Never hand mGBA's ROM block to the companion.

## Keeping in step with the app

The companion's tables, guides and addresses are generated from the Android
app's Kotlin sources:

```sh
make -C 3ds tables
```

Run it after changing anything in `app/src/main/kotlin/.../companion/data/`, or
the two ports will disagree with the app about the games.

## Credits and license

Based on PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy
GPLv3, with the extra term in [NOTICE](../NOTICE): keep the credit line.
