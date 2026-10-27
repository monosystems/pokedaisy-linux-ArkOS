# AGENTS.md

Short, shared rules for the ports in this repo (the Linux handheld port and the
3DS port). The long versions are `CLAUDE.md` (upstream project notes and
conventions), `docs/DEVELOPMENT.md` (how the app reads each game) and
`linux/PLAN.md` (the Linux port's phases). This file is what an agent must not
get wrong.

## What belongs to whom

- `app/`, `scripts/`, `docs/`, `website/`, `ui-preview/` and `native-capture/`
  are upstream. Do not edit them: the ports are a fork of a project that keeps
  moving, and an untouched app tree is what keeps an upstream merge clean.
- The ports live in `3ds/core` (portable C, no platform headers), `3ds/ctr`
  (devkitARM host), `3ds/desktop` (preview tool), `linux/` (SDL2 host) and
  `linux/arkos/` (PortMaster package, `deploy.sh`).
- `3ds/core` is shared by both hosts and must build as plain C11, without SDL,
  mGBA or 3DS includes.

## Build and verify before committing port changes

```sh
python3 3ds/tools/gen_tables.py       # names, moves, types, item names, gender
python3 3ds/tools/gen_map_tables.py   # map sections, art fingerprints, cursors
python3 3ds/tools/gen_guide.py        # guides, bosses, area data
# `make -C 3ds tables` runs those three and the font and icon (needs Pillow)
make -C 3ds preview           # every tab, from the RAM fixtures -> build/shots/*.png
./linux/test/smoke.sh         # the Linux host, headless, no game needed
cmake -S linux -B linux/build -DCMAKE_BUILD_TYPE=Release && cmake --build linux/build
make -C 3ds 3ds               # the real .3dsx (devkitARM in Docker)
```

For a UI or data change, look at the matching preview shot, not only at exit 0.

## Data, addresses and saves

- Tables are generated from the app (`3ds/tools/`): names, moves, types, item
  names, guides, map art, font. Never hand-edit `3ds/core/*_gen.c`; run
  `make -C 3ds tables`. CI's "Generated tables in sync with the app" job fails
  when re-running the generators would change them.
- `3ds/core/pd_item_desc.c` is frozen on purpose: upstream stopped bundling item
  descriptions and reads the ROM's `gItems` (`RomItemText.kt`). Replace it by
  reading `gItems` in `pd_item_description`; don't extend the frozen tables.
- Addresses in `pd_game.c` come from `NativeReader.kt` / `Pokedex.kt`. Every
  ROM table needs a probe (the app's check, e.g. BULBASAUR = SEED / LV 16), so a
  moved table is rejected instead of misread. Never invent or guess an address.
- Saves are sacred: a savestate never carries the battery save
  (`SAVESTATE_RTC`), the save is backed up before every load, and a savestate
  must never be written into a save file.
- Never hand mGBA's ROM block to `3ds/core`: mGBA remaps it after reset, so both
  hosts keep their own copy of the ROM and point `pd_game.rom` at it.
- Nothing in the repo that is not ours to publish: no ROMs, saves, keys or
  personal paths. `linux/arkos/deploy.conf` is ignored for that reason.

## Deploying to a handheld

`linux/arkos/deploy.sh` writes a build to the card (`local=` in the ignored
`deploy.conf`, or `--local DIR`) or copies it over ssh (`host=`), then checks the
SHA-256. On macOS's exFAT driver, `cp` and `mv` are unreliable - that is why the
script writes with `dd conv=fsync`; don't replace the binary with Finder or a
plain `cp` when deploy.sh is there.

## Writing

- Code comments explain why, in English, in the voice of the file around them.
- Docs are `PORTING.md` (the feature inventory against the app), `TODO.md` (the
  order the work goes in), `linux/PLAN.md`, `linux/README.md` and `3ds/README.md`;
  update them in the same change as the behaviour they describe, including the phase
  table and a feature's row in `PORTING.md`. A TODO item that lands gets ticked, and
  its rows move with it.
- `README.md` opens with this fork's own section: what the ports are and how much
  of the app is ported. The upstream text below the divider stays unchanged, so an
  upstream README change still merges cleanly. The percentage is counted from
  `PORTING.md` (a partial feature counts half, not-applicable rows are left out) by
  `python3 porting_stats.py`, and is updated in the same commit as the row that moved
  it.
- Commits carry no Claude/Anthropic attribution (no `Co-Authored-By`, no
  "Generated with"); upstream keeps a commit-msg hook that rejects it, and this
  fork follows the same rule.
- User-facing text is plain and second person, with no em or en dashes (the
  manual's rules in `CLAUDE.md`).
