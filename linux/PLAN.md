# PokeDaisy for Linux handhelds — plan

An SDL2 port of PokeDaisy for one-screen Linux handhelds. The target is
**ArkOS4Clone on the R36S** (Rockchip RK3326, 640x480); the same binary runs in a
window on a desktop, which is where the development happens. The companion is
the portable C core from [`3ds/core`](../3ds/core), unchanged and shared with
the 3DS port, so both ports read and draw the game the same way.

`README.md` says what the port does today; this file says what comes next and
in which order. The row by row inventory of the app's features against both
ports, with the effort each missing one needs, is [`../PORTING.md`](../PORTING.md).

## Status

**Phase 0–3 done; phase 4 (content parity) is next.** Everything below is
in `linux/` unless it says otherwise, and every phase ends with the headless
smoke test (`linux/test/smoke.sh`) and the CI jobs
(`.github/workflows/ci.yml`: x86_64, ASan, aarch64 + PortMaster zip).

| Phase | What | State |
| - | - | - |
| 0 | libmgba in-process behind an SDL2 window, the frame on screen, no companion | done |
| 1 | The host: sound, pad and keyboard, saves and their backups, one save state, fast-forward, the `/roms/gba` launcher, resume | done |
| 2 | The companion over the game: `pd_ui` at 320x240 scaled 2x, the D-pad cursor over its hits, SETTINGS, the PortMaster package | done |
| 3 | The first run on the R36S itself | done: it boots and plays, the DEX's EVOLVE works, the three SCREEN modes are measured, and the zip installs through PortMaster |
| 4 | Content parity with the app's companion | next: the P1 items in `TODO.md` |
| 5 | More games and languages, then the app's field actions | planned |

## Phase 3 — the handheld

What cannot be checked on a desktop: frame time per SCREEN mode on the RK3326,
sound through ArkOS's SDL2 (KMSDRM), save and state writes on the exFAT card, a
panel mounted sideways, and the Ports-menu launch with `START+SELECT` as the only
way out.

- Run it on the R36S with `--stats` and read the `stats:` and `system:` lines
  (README "Test"). The Ports-menu launch script already passes `--stats` and
  writes `log.txt` into the port folder, which is the way in when there is no
  ssh: start from the menu, play, leave with `START+SELECT`, copy the log off
  the card.
- Note frames per second and the emulator/drawing split for `fit`, `integer` and
  `stretch`; if drawing costs too much, the companion can stay a 320x240 texture
  (it is one today) and only its scaling changes. On the card the mode is
  `screen=` in `pokedaisy.ini` (0 whole pixels, 1 fill, 2 stretch), and the
  startup line names it; a switch made in SETTINGS during the run logs
  `screen: <mode>`, so one session can cover more than one mode.
- Check the save backup, the `.resume` state and the one state slot on a card
  that is mounted exFAT.
- Install the zip from `linux/arkos/build.sh` through PortMaster, not only by
  hand, and leave the game with `START+SELECT`.

Where the numbers go: frames per second, then the emulator and drawing share in
milliseconds.

| Mode | Companion closed | Companion open |
| - | - | - |
| `fit` | 58.5 fps, emu 9.9, draw 7.1 ms | 58.4 fps, emu 9.9, draw 7.1 ms |
| `integer` | 58.4 fps, emu 9.9, draw 7.1 ms | 58.5 fps, emu 9.9, draw 7.1 ms |
| `stretch` | 58.4 fps, emu 9.9, draw 7.1 ms | 58.5 fps, emu 9.9, draw 7.1 ms |

Measured on 27.10.2026 with FireRed (BPRE rev 1) standing still, new binary, in
all three modes. The panel is 640x480 at 59.0 Hz, so a frame is 16.95 ms, and the
frame comes out at 17.09 ms: `draw` is the frame period minus the emulator, which
is the shape of waiting for the panel's vblank in the present, not of GPU work.
The companion changed nothing measurable, closed and open measured the same, and
the audio pump is 0.01 ms. The emulator's 9.9 ms is the real cost, about 7 ms of
each frame is idle, and a feature that needs more than that starts dropping
frames. All three modes came out within 0.1 fps of each other, from 480x320 whole
pixels to 640x480 stretched, so the fill rate does not limit this game. Because
the present waits for the panel, the game runs 2% under the GBA's 59.73 fps.

**Done:** it boots on the R36S, plays with sound, writes saves and the companion
opens; the DEX's EVOLVE was checked there; the three SCREEN modes are measured
(the table above); the save backup, the `.resume` and the one state slot work on
an exFAT card; and the zip installs through PortMaster's `autoinstall` folder
(`/roms/tools/PortMaster/autoinstall` on this image, and PortMaster logs
`Unknown port: PokeDaisy.sh` afterwards, which is only its catalogue missing the
port).

With this run the gate is open: phases 4–5 can start, and about 7 ms of every
frame turned out to be idle, so drawing over the game is affordable.

## Phase 4 — content parity

The app's companion has more than the portable core reads and draws. These are
read-only features — no writes into the running game — so they are safe to do
before anything that acts on the game, and they land on the 3DS too.

1. **DEX sections** (`3ds/core/pd_dex.c`, `pd_ui_dex.c`): the app's INFO / EVOLVE /
   AREA / MOVES pages (`DexDetailPages.kt`, `DexDetails.kt`). The ROM addresses
   exist in the app (`Pokedex.kt`: `evolutions`, `tmhmLearnsets`, `tmhmMoves`);
   the wild tables are already read by the GUIDE (`pd_game.wildHeaders`).
   - **EVOLVE: done.** `pd_dex_family` walks the family both ways in chain order
     (the app's `DexDetails.family`) and `pd_evo_how` words each method
     (`Evolutions.kt`, `evolutionHow`); the entry has INFO / EVOLVE chips, the
     family list, a tap to open another member, and the section stays open while
     stepping through the dex. `pd_preview` shoots it as `-dex-evolve`.
     FireRed rev 1 and Emerald only, while LeafGreen keeps its RAM-only config.
   - Next: AREA (wild tables by species, plus the GUIDE's gifts and trades) and
     MOVES (level-up from `learnsets`, TMs / HMs from `tmhmLearnsets`).
2. **PARTY summary pages** (`pd_ui_party.c`): STATS (values, IVs, EVs, nature) and
   LEARN (what it learns next, the TMs in the bag it can use), read the way the
   app's `Gen3Mon.kt` / `MoveLearning.kt` do. The mon struct is already decoded
   for the party list; the extra fields come out of the same 100 bytes.
3. **STATES: more slots and thumbnails** (`linux/state.c`, `files.c`, the
   companion's SETTINGS): the app keeps ten slots with screenshots. This one is
   host-side (the 3DS keeps its single slot until its own screen is designed for
   a grid), so it does not touch `3ds/core`.
4. **The port's own text in other languages**: the app's strings are translated
   (`app/.../companion/i18n/`); the core's ~80 labels are English only. A
   generated table keeps the two in step. The GUIDE's page text is separate
   (`gen_guide.py` reads the English DSL), so it follows only if the translated
   guide text is generated too.

## Phase 5 — support and actions

1. **More games and languages** (`3ds/core/pd_game.c`, `pd_snapshot.c`): the app's
   `NativeConfig` covers Ruby / Sapphire and every FireRed / Emerald language;
   the port reads FireRed rev 1, Emerald (English) and LeafGreen's RAM only.
   Each game needs its addresses and a probe; each language a charset
   (`pd_gen3_text` is English only) and its own name / item tables. This is the
   biggest gap for real users of the handheld — most cards are not English
   vanilla — but it is support work, not a new screen.
2. **Field actions** (the app's HUD, Quick menu and quality of life): FLY from the
   MAP, PC anywhere, the keep-away spray, wild encounters off, auto-run. These
   write into the running game and need per-game code and a hardware check, so
   they come last. The app's touch-first HUD and ring are not a good fit for a
   640x480 screen with a D-pad cursor: expect a smaller menu of actions instead.

## Keeping in step with the app

The core's tables, guides and addresses are generated from the app's Kotlin
sources:

```sh
make -C 3ds tables
```

Run it after changing anything in `app/src/main/kotlin/.../companion/data/`, or
the ports will disagree with the app about the games.

That is also how an upstream update reaches the ports: new guide pages, renamed
items, a corrected address or a new game's tables are regenerated into C, and CI
(`Generated tables in sync with the app`) fails when someone merged an app change
without re-running the generators. The generators read the Kotlin sources, so a
refactor there breaks them loudly - e.g. upstream moved item descriptions from a
bundled table into the ROM (`RomItemText.kt`, `NativeConfig.itemDescs`), which is
why the two description tables now sit frozen in `3ds/core/pd_item_desc.c`; the
port should read `gItems` like the app and drop that file.
