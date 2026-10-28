# Porting inventory: the app's features against the ports

What the upstream app can do, what `linux/` and `3ds/` can do today, and what a
port would have to build for each missing piece. The upstream trees are read
only (see `AGENTS.md`); this file is the ports' own working list, so it lives
here and not in `docs/`.

**Status of this list:** taken from the ports at commit `36b27d3` by reading
`linux/README.md`, `3ds/README.md`, `linux/PLAN.md` and the code in `3ds/core`,
`3ds/ctr` and `linux/`. It is a code inspection, not a run: every row marked
done should still be seen once on the hardware.

## How to use it

- Each row has an ID (`H-07`, `C-B-08`, `S-05`). The verification list in
  [Part 8](#part-8-verifying-on-the-phone) refers to the same IDs, so a note
  from the phone can be filed against a row without describing the feature again.
- **When a feature lands, its row changes in the same commit** (the rule
  `docs/ROM_FEATURES.md` uses upstream). A row that is wrong is worse than a
  missing one, because the plan is built on it.
- Effort is for the port that does not have it yet, on both hosts:
  `S` a few hours, `M` a day or two, `L` a week, `XL` more than that or a
  rewrite of how a host draws.

## Status marks

| Mark | Meaning |
| --- | --- |
| ✅ | In the port and working |
| ◐ | In the port, but narrower than the app: fewer games, fewer pages, fixed instead of configurable |
| — | Not in the port |
| n/a | Does not fit the hardware, see [Part 7](#part-7-what-does-not-fit-the-hardware) |

## Where the truth lives upstream

| Question | Upstream file |
| --- | --- |
| What the app promises a player | `README.md` (Features), `docs/manual/*.md` |
| Which game has which feature | `docs/ROM_FEATURES.md`, `docs/manual/supported-games.md` |
| How each game is read | `docs/DEVELOPMENT.md`, `app/src/main/kotlin/com/pokedaisy/app/companion/data/NativeReader.kt` |
| What a tab shows | `app/.../companion/ui/*.kt` (`PartyScreen.kt`, `MonDetailScreen.kt`, `ItemsScreen.kt`, `MapScreen.kt`, `PokedexScreen.kt`, `DexDetailPages.kt`, `GuideScreen.kt`, `BattleInfoScreen.kt`, `SuggestionsScreen.kt`, `TrainerCardScreen.kt`, `StatesScreen.kt`, `AchievementsScreen.kt`, `FieldHud.kt`, `QuickMenu.kt`, `BattleButtons.kt`) |
| The port's own phases | `linux/PLAN.md` |

## Parts

1. [Emulation and host](#part-1-emulation-and-host)
2. [The companion frame and its tabs](#part-2-the-companion-frame-and-its-tabs)
3. [Features over the game: HUD, quick menu, battle buttons, makeover](#part-3-features-over-the-game-hud-quick-menu-battle-buttons-makeover)
4. [Library, save editor, cheats, other surfaces](#part-4-library-save-editor-cheats-other-surfaces)
5. [Game and language support](#part-5-game-and-language-support)
6. [The data pipeline](#part-6-the-data-pipeline)
7. [What does not fit the hardware](#part-7-what-does-not-fit-the-hardware)
8. [Verifying on the phone](#part-8-verifying-on-the-phone)
9. [Suggested order](#part-9-suggested-order)

## Part 1: Emulation and host

Everything around the game: what plays, how saves and states behave, speed,
screen and input. The app's own split (`docs/manual/companion.md`,
`docs/manual/screen.md`, `docs/manual/saves.md`, `docs/manual/controls.md`) is
kept, so a row can be read next to its manual page.

### Playing and loading

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| H-01 | GBA in-process on mGBA 0.10.5 | `third_party/mgba` | ✅ | ✅ | Both hosts link the same submodule. |
| H-02 | Game Boy / Game Boy Color games play | `README.md`, `GbField.kt` | — | — | mGBA can run them, but the hosts only offer `.gba`, and the companion has no GB reader. Needs a platform switch, menu extensions and the GB side of the companion. `L` |
| H-03 | Nintendo DS games play | `third_party/melonds` | — | — | Not realistic on the R36S. On the 3DS, handing the `.nds` to TWiLight Menu or nds-bootstrap is the sane route, not melonDS in-process. `M` for the hand-off, `XL` for melonDS |
| H-04 | Sound, muted while fast-forwarding | `audio.c`, ndsp | ✅ | ✅ | No volume setting, and no separate mute key. `S` |
| H-05 | Start a chosen ROM from the command line or a launcher | `LaunchActivity` | ✅ | ✅ | `pokedaisy <game.gba>`, `3dsx` with a list. |
| H-06 | Game list when started without a ROM | `LibraryScreen.kt` | ✅ | ✅ | `/roms/gba` on Linux, `sdmc:/pokedaisy/roms` on 3DS. |
| H-07 | Zipped ROMs (`.zip`, `.7z`) | `RomIntake`, Apache Commons Compress | — | — | Needs a small inflate for zip; 7z would mean a new dependency. `M` / `L` |
| H-08 | Library tags (NOT SUPPORTED, PARTIALLY SUPPORTED, HACKS) | `GameTitles.kt` | — | — | Depends on S-08. `S` once matching exists |

### Saves and states

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| H-09 | Battery save next to the ROM (`.sav`) | `saves.md` | ✅ | ✅ | Same rule as mGBA's own builds. |
| H-10 | Existing `.srm` picked up | `saves.md` | ✅ | — | `files.c` tries `.srm` before `.sav`; the 3DS host only knows `.sav`. `S` |
| H-11 | Backup before every start, newest 10 per game | `saves.md` | ✅ | ✅ | `PD_BACKUPS` = 10 on Linux, same count in `ctr/main.c`. Seen on the R36S: a backup on start, and another before LOAD STATE. |
| H-12 | A backup only when the save changed | `saves.md` | ◐ | ◐ | Not checked on either host; the file name carries the timestamp, so it can produce duplicates. On the R36S two backups appeared in two minutes without the save changing. `S` |
| H-13 | Resume at the exact moment on the next start | `RESUME GAMES` | ✅ | — | Linux writes `<name>.resume` and prefers it while the save is not newer. The 3DS lists it under next steps. Seen on the R36S, written again on leaving. `M` |
| H-14 | Resume never overwrites a newer save | `saves.md` | ✅ | n/a | `pd_resume_is_fresh` compares the save's time. |
| H-15 | Load another save into a game, keeping the old one | `library.md` | — | — | Library UI work, needs H-08. `M` |
| H-16 | Restore a backup from the library | `library.md` | — | — | The files exist, the UI does not. `S` |
| H-17 | One save state per game, save and load behind a confirmation | `saves.md` | ✅ | ✅ | `SAVESTATE_RTC` only, so a state never writes an old save back. Seen on the R36S on the exFAT card, `state: saved` and `state: loaded`. |
| H-18 | Ten state slots with a screenshot each | `saves.md`, `StatesScreen.kt` | — | — | Host side on Linux, so the core stays as it is. Needs a thumbnail path in both hosts. `M` |
| H-19 | UNDO SAVE / UNDO LOAD | `saves.md` | — | — | Two more files per slot, and the last one kept aside. `S` |
| H-20 | State hotkeys (SELECT + R1 / L1, slot next and prev) | `controls.md` | — | — | Fixed buttons today; see H-46. `S` |
| H-21 | Rewind, last ~20 seconds | `saves.md` | — | — | mGBA has the frame buffer behind a flag; the cost on RK3326 and on a New 3DS has to be measured first. `M` |

### Speed

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| H-22 | Fast-forward while a button is held | `fast-forward.md` | ✅ | ✅ | Space or pad R2 on Linux, ZR on a New 3DS. |
| H-23 | Fast-forward as a toggle that survives the session | `fast-forward.md` | ◐ | ✅ | The 3DS has the chip and its `SETTING_FF` row; on Linux it is hold only. `S` |
| H-24 | FF speeds 1.5x to 10x plus `INFINITE` | `fast-forward.md` | ◐ | ◐ | Both ports stop at 2x to 4x (`PD_FF_MIN` / `PD_FF_MAX`). The host paces the frames; wider steps are cheap. `S` |
| H-25 | `FF MODE` SMART (normal speed in menus and on the map) | `fast-forward.md` | — | — | Needs the core to expose "a menu is open" per game, which is per-game work again. `M` |
| H-26 | `FF MUSIC` STEADY / SPED-UP / OFF | `fast-forward.md` | — | — | Today the sound is muted while fast-forwarding. `STEADY` also needs a recorded song cache, which is a project of its own. `S` for OFF / SPED-UP, `XL` for STEADY |
| H-27 | `GAME SPEED` a game stays at (0.25x to 4x) | `fast-forward.md` | — | — | A setting plus a frame divisor. `S` |
| H-28 | Scene speeds: battle, text, menu, bike, surf | `fast-forward.md` | — | — | Mostly the same per-game detection as H-25. `M` |
| H-29 | Slow motion while held | `fast-forward.md` | — | — | The mirror of the hold fast-forward. `S` |
| H-30 | `SOFT RESET` hotkey | `controls.md` | — | — | mGBA has reset; it needs a key chord and the ROM copy to survive it. `S` |

### Screen

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| H-31 | Aspect: the GBA's own shape, pixel for pixel, stretched | `screen.md` | ✅ | ✅ | Linux `fit` / `integer` / `stretch`, 3DS `PIXEL 1x` / `SHARP 1.5x` / `STRETCH`. |
| H-32 | `EXPAND`: the map drawn past the GBA's edges | `screen.md`, `WideScreen.*` | — | — | The app's biggest feature and the most expensive one: per-frame map, tiles and sprite rebuilds for each supported game. `XL`, and unclear on RK3326 |
| H-33 | `ZOOM` and looking around with the right stick (EXPAND only) | `screen.md` | — | — | Follows H-32. `L` |
| H-34 | Shaders: LCD grid, scanlines, CRT, the GBA's own colours | `screen.md` | — | — | Per-frame post-processing. On the RK3326 it competes with the emulator; measure before promising it. `M` on a desktop, `L` on the handheld |
| H-35 | Edge smoothing (xBRZ, xBR, ScaleFX) | `screen.md` | — | — | The app uses libretro's code. `L` |
| H-36 | `UNLIT SCREEN` | `screen.md` | — | — | A shader variant. `S` once H-34 exists |
| H-37 | Overlays and frames, own and imported RetroArch ones | `screen.md` | — | — | Drawing is easy; reading `.cfg` and `.zip` and the file picker is the work. `M` |
| H-38 | Status bar (game, place, money, clock, battery) | `screen.md` | — | — | The core already reads place and money, so this is mostly drawing plus a battery call per host. `M` |
| H-39 | `DARK MODE` for the app's windows | `screen.md`, `DarkMode.kt` | — | — | `pd_style.c` holds one palette; a second one is mechanical. Screens are 640x480 and 320x240, so this is a nice-to-have. `M` |
| H-40 | `FPS / CPU` readout | `screen.md` | ◐ | — | Linux prints `stats:` to the log and has `--stats`; nothing on screen. `S` |
| H-41 | `LIGHT COMPANION` redraw throttling | `screen.md` | — | — | Worth doing on the RK3326 if the companion turns out to cost frames (phase 3 measurement). `S` |

### Input

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| H-42 | The GBA buttons on the pad and the keyboard | `controls.md` | ✅ | ✅ | Fixed mapping, documented in each README. |
| H-43 | `GAME BUTTONS` remap page | `controls.md` | — | — | Needs a key capture page in the core UI plus persistence. `M` |
| H-44 | `HOTKEYS` page, one switch to turn them all off | `controls.md` | — | — | Same machinery as H-43, and it also gives H-20 for free. `M` |
| H-45 | `TURBO A` / `TURBO B` on any button | `controls.md` | — | — | Presses A or B on a timer in game time. `S` |
| H-46 | `TOUCH CONTROLS` with a layout editor | `controls.md` | n/a | n/a | The R36S has no touchscreen and on the 3DS the touchscreen is the companion. |
| H-47 | Controller hot plug while a game runs | `controls.md` | ✅ | n/a | `input.c` reopens the pad. The 3DS pad is fixed. |
| H-48 | Analog L2 / R2 treated as presses | `controls.md` | ◐ | n/a | Depends on what the pad reports on ArkOS; check on the R36S. `S` |
| H-49 | BACK tap (companion back) and hold (leave game) | `controls.md` | ◐ | ◐ | Linux opens the companion with L2 and leaves with `START+SELECT`; the 3DS uses B and its own exit. The split into tap and hold is not there. `S` |

### App plumbing

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| H-50 | Settings kept between starts | `pd_settings.c` | ✅ | ✅ | `settings.ini` next to the data, unknown keys fall back. |
| H-51 | The app's text in 7 languages | `companion/i18n/` | — | — | The core's ~80 labels are English. A generated table can keep them in step; the GUIDE's page text is the harder half. `M` for labels, `L` including the guide |
| H-52 | `HELP`: the manual inside the app | `DocsScreen.kt` | — | — | The manual is Markdown and rendered by `website/src/lib/docs.js` and `DocsScreen.kt`; a C reader plus the files on the card. `M` on Linux, `L` on 3DS |
| H-53 | Screenshots from the player's side | `render.c`, app | ◐ | — | Linux has `--screenshot FILE` for tests, no hotkey and no gallery. `S` |
| H-54 | Launch from a frontend (ES-DE, iiSU, Cocoon) | `library.md` | ◐ | n/a | The PortMaster package and `deploy.sh` stop and restart EmulationStation; a plain cartridge-style entry would be cleaner. `S` |
| H-55 | Update check and install | `AppUpdateFlow.kt` | n/a | n/a | PortMaster and the 3DS homebrew channels own this. |

## Part 2: The companion frame and its tabs

The core draws the app's FireRed OPTION look at 320x240 (`pd_ui.c`), and each
host scales it (Linux to 640x480 over the game, 3DS 1:1 on the bottom screen).
The rows below compare what is inside that frame.

### The frame

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| C-01 | A fixed row of five chips plus the gear | `companion.md` | ✅ | ✅ | `BAR` in `pd_ui.c`; CARD opens from SETTINGS instead of a chip. |
| C-02 | `MORE`: the hidden tabs as a grid | `companion.md` | — | — | Only needed once there are more tabs than the bar holds (C-03, C-ST-01, C-A-01). `S` |
| C-03 | Choosing the five tabs (TAB BAR page) | `companion.md` | — | — | A settings page and a per-tab shown/hidden bit. `S` |
| C-04 | A tab the game lacks gives its place to another | `companion.md` | ◐ | ◐ | DEX and GUIDE disappear when the game has no data for them, but nothing fills the gap. In a battle the app puts `BATTLE` first and the last tab gives up its place (V-03); the ports put `BATTLE` in the `DEX` slot instead. `S` |
| C-05 | `PRESS START` empty state on the title screen | `companion.md` | ◐ | ◐ | Before the game is readable the tabs show zeros rather than the game's empty slots. `M` |
| C-06 | `CLICK SOUND` on every tap | `companion.md` | — | — | Needs the game's own click sample; the core has no audio path at all. `M` |
| C-07 | `TWEAKS`: moving icons, blinking cursor, tab animation, jump to battle | `companion.md` | ◐ | ◐ | The cursor blink and the jump to battle exist and are not switchable; the rest do not exist. `S` |
| C-08 | `PLAYS AS` and best effort matching | `supported-games.md` | — | — | See S-08. `L` |
| C-09 | The `NOT SUPPORTED` page with the reason | `supported-games.md` | ✅ | ✅ | The core sets `unsupported` in `pd_game_detect` and the UI draws a notice. |
| C-10 | `ASK FOR SUPPORT` (file name and SHA-1, never the ROM) | `supported-games.md` | — | — | Needs a URL opener or a QR code per host. `S` |
| C-11 | Companion as a window or a side panel | `devices.md` | ✅ | n/a | Linux draws it over the game, which is the window; the 3DS has its own screen. |
| C-12 | `CLOSE COMPANION` and `OPEN COMPANION` | `companion.md` | ✅ | n/a | Closing the overlay is the same as hiding it on Linux. |

### PARTY

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| C-P-01 | Six slots with the game's icon, name, gender, level, HP bar and status | `party.md` | ✅ | ✅ | `pd_ui_party.c`, icons from the ROM (`pd_icon.c`). |
| C-P-02 | Fainted slots faded, status word on the slot | `party.md` | ✅ | ✅ | |
| C-P-03 | Held item tag in the slot's corner | `party.md` | — | — | A thin bar over the top of the slot with the item's icon and name (V-20). The mon struct is already decoded; the bag's item name table is there too. `S` |
| C-P-04 | Icons that bounce like the game's | `party.md` | — | — | The 3DS README lists the two-frame animation under next steps. `S` |
| C-P-05 | Nickname as the game shows it | `party.md` | ✅ | ✅ | The name comes out of the mon struct. |
| C-P-06 | A summary with the mon's moves, type and PP | `party.md` | ✅ | ✅ | Tap a slot. |
| C-P-07 | Summary's EXP bar (`NEXT` / `MAX`) | `party.md` | — | — | EXP is in the struct; the curve per species is per-game data. `S` |
| C-P-08 | `WEAK TO`, `RESISTS`, `IMMUNE TO` with multipliers | `party.md` | — | — | The type chart is generated (`pd_tables_gen.c`). `S` |
| C-P-09 | `STATS`: IVs and EVs as bars, nature, Hidden Power | `party.md`, `MonStatsSection.kt` | — | — | Same 100 bytes, more decoding. Nature is a field in the struct. `M` |
| C-P-10 | `LEARN`: next level-up moves and usable TMs, with verdicts and reasons | `party.md`, `MoveLearning.kt` | — | — | The biggest single party feature: learnsets, the advice rules and their short reason strings (`WEAK EFFECT`, `ITS MOVES ARE BETTER`, ...), which are copy of their own. `L` |
| C-P-11 | Party strip at the bottom of the summary to switch quickly | `party.md` | — | — | The section stays open while stepping. `S` |
| C-P-12 | `TAKE` a held item back into the bag | `party.md` | — | — | A write into the game, part of S-16. |
| C-P-13 | The game's own slot art and per-hack palettes | `PartyPalette`, `PartySlotStylesGen.kt` | ◐ | ◐ | One FireRed palette; the app has a palette per supported hack. `M` |

### BAG

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| C-B-01 | Pockets down the side, in the game's order, with the game's colours | `bag.md` | ◐ | ◐ | The app draws the game's bag with `PC` and the order button under the pockets (V-13). The ports step to one pocket in a white list instead. |
| C-B-02 | Item names and counts as the game prints them | `bag.md` | ✅ | ✅ | Names are generated, the counts come from the save. |
| C-B-03 | The tab's name follows the game (`PACK`, `CUBE`, `INVENTORY`, `POCKETS`) | `bag.md` | — | — | Only matters once hacks are read (S-06). |
| C-B-04 | `ALL` pocket showing everything at once | `bag.md` | — | — | Always drawn, even with an empty bag; the other pockets only appear when they hold something (V-13). `S` |
| C-B-05 | Sorting: game order, name, type, count | `bag.md` | — | — | All four confirmed: `DEFAULT`, `NAME`, `TYPE`, `COUNT` (V-13). `S` |
| C-B-06 | Favorites (starred items, own `FAVORITES` pocket, per ROM) | `bag.md` | — | — | The star sits in the description bar and adds both the gold star in the list and a `FAVORITES` pocket (V-13). Needs a small per-ROM file next to the settings. `M` |
| C-B-07 | TMs show the move they teach | `bag.md` | — | — | `S` |
| C-B-08 | The tapped item's description | `bag.md` | ◐ | ◐ | `pd_item_desc.c` is frozen on purpose (see `AGENTS.md`). Upstream reads `gItems` (`RomItemText.kt`), so a hack's or another language's text is wrong here. `M` |
| C-B-09 | `ABLE` / `NOT ABLE` / `LEARNED` / `BEST` for a TM | `bag.md`, `ItemPartyPicker.kt` | — | — | The app replaces the description's text with six party icons and a tag under each (V-14). `BEST` and the forget line did not show with three able, so they are conditional on the advice; open. Shares the learnset reader with C-P-10. `M` |
| C-B-10 | The sort order keeps a scroll a drag can move | `bag.md` | ✅ | ✅ | A list a drag scrolls, per the ports' READMEs. |
| C-B-11 | `USE` an item (and pick the Pokémon or move) | `bag.md` | — | — | The usable rows carry a ball mark and the list says `USE or GIVE it from here`; an HM shows neither (V-13). Writes into the game; per-game, S-16. `L` |
| C-B-12 | `GIVE` an item to a Pokémon | `bag.md` | — | — | Same as C-B-11. |
| C-B-13 | `PC` from anywhere | `bag.md`, `Pc.kt` | — | — | Confirmed: the game's own PC opens over the field and the companion stays up (V-18). The app calls the game's PC special. Per-game addresses and a field check. `L` |
| C-B-14 | `REUSABLE TMS` | `bag.md`, `ReusableTms.kt` | — | — | A ROM patch per game; the app has a table of sites. `L` |

### MAP

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| C-M-01 | The game's own region map, rebuilt from the ROM | `map.md`, `RomArt.kt` | ✅ | ✅ | `pd_map.c` plus `pd_romart.c`; nothing bundled. |
| C-M-02 | The player's head on the right tile, place name in the game's label | `map.md` | ✅ | ✅ | The label style follows the game (FireRed strip, Emerald window). |
| C-M-03 | The game's blinking cursor | `map.md` | ✅ | ✅ | Always on, see C-07. |
| C-M-04 | Tap a spot to name it, `ME` to come back | `map.md` | ✅ | ✅ | The D-pad cursor does the tap; `ME` appears once panned. |
| C-M-05 | `PLACES`: towns, routes, other places, your location | `map.md` | ✅ | ✅ | Grouped and sorted as the app's `RegionMapModel`. The app adds the map page on the right of each row (V-19), which the ports do not. |
| C-M-06 | Several map pages (Kanto / Sevii, numbered maps) | `map.md` | ✅ | ✅ | A region button next to PLACES. |
| C-M-07 | Text instead of a picture when the game has no map art | `map.md` | ◐ | ◐ | Worth a check on a hack once hacks are read; untested today. `S` |
| C-M-08 | `FLY` to a town | `map.md`, `Fly.*` | — | — | Writes into the game; the party's move has to be read and the game's own animation started. `L` |

### DEX

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| C-D-01 | The list with seen and owned counts and a caught mark per row | `dex.md` | ✅ | ✅ | `pd_ui_dex.c`; all entries open, seen or not. |
| C-D-02 | Regional / National flip, starting on the save's own | `dex.md` | ✅ | ✅ | A tap on the region name. |
| C-D-03 | Filters: `ALL`, `CAUGHT`, `SEEN ONLY`, `NOT SEEN` | `dex.md` | — | — | The seen and owned flags are already read. `S` |
| C-D-04 | Entry: number, name, picture, footprint, category, types, height and weight, dex text | `dex.md` | ◐ | ◐ | Text as the ROM has it, images LZ77; the ports draw no footprint. |
| C-D-05 | Base stats with `TOTAL` and the `ABILITY` | `dex.md` | ✅ | ✅ | `gBaseStats` and the ability name table. |
| C-D-06 | `HIDDEN` ability | `dex.md` | — | — | Gen 3 retail has none, so this follows S-07 (hacks) and not the retail readers. |
| C-D-07 | Footprint, `EGG GROUP`, gender ratio, `CATCH RATE` | `dex.md` | — | — | All in `gBaseStats`; the egg group table is not generated yet. The gender row shows both symbols with a percentage each (V-10). `M` |
| C-D-08 | `EVOLVE`: the family both ways with how each step happens | `dex.md`, `DexDetails.kt` | ✅ | ✅ | FireRed rev 1 and Emerald; LeafGreen has no table in the port's config. The app writes the condition as `LU n`, the port as `LV n`, so align the label. |
| C-D-09 | `AREA`: wild places by method with odds, plus gifts and trades | `dex.md` | — | — | The wild tables are read for HERE already, so this is a second view. `M` |
| C-D-10 | `MOVES`: level-up list plus TMs and HMs | `dex.md` | — | — | `LEVEL UP` with level, name and type chip, then `TM / TUTOR` or `NONE` (V-11). Learnsets are in the config (`gLevelUpLearnsets`); the TM half needs `tmhmLearnsets`. `M` |
| C-D-11 | Previous and next arrows with a counter | `dex.md` | — | — | `S` |
| C-D-12 | The open section stays open while stepping through entries | `dex.md` | ✅ | ✅ | The EVOLVE section survives a step. |

### GUIDE

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| C-G-01 | Hint first, then the answer, one entry open at a time | `guide.md` | ✅ | ✅ | `pd_ui_guide.c`, the `HINT` / `ANSWER` tag at the row's end. |
| C-G-02 | The page and the scroll position are kept when switching tabs | `guide.md` | ◐ | ◐ | The scroll is kept per tab; whether the page is too, after a battle jump, is unchecked. `S` |
| C-G-03 | `HERE`: hand-written to-dos for the area | `guide.md` | ✅ | ✅ | Generated pages from the app's guide DSL. |
| C-G-04 | `HERE`: wild Pokémon per method, levels and odds, `???` until tapped | `guide.md` | ✅ | ✅ | Read from `gWildMonHeaders`. |
| C-G-05 | `HERE`: `PEOPLE` (gifts, eggs, trades) | `guide.md` | ✅ | ✅ | |
| C-G-06 | `HERE`: `ITEMS`, hidden ones behind a hint, a ball when done | `guide.md` | ✅ | ✅ | The ball marks come from the save's flags. |
| C-G-07 | `BOSS`: the next unbeaten gym, Elite Four or champion with the team | `guide.md` | ◐ | ◐ | The app opens on the next boss and lists the following ones below as headers (V-16); the ports show the next one only. `pd_guide.c` checks it against the save's badges. |
| C-G-08 | `ALL DONE` when every boss is beaten | `guide.md` | ✅ | ✅ | Drawn as `ALL BEATEN`. |
| C-G-09 | Several teams per boss (difficulty, rematch) | `guide.md` | — | — | Only the retail games' single teams are generated. `M` |
| C-G-10 | The `EVOLUTIONS` page | `guide.md` | — | — | Shares C-D-08; the party comes first. `S` |
| C-G-11 | Day and night variants (Lazarus, Polished Crystal) | `guide.md` | — | — | Both are Game Boy or hack only, so it follows S-02 and S-06. |
| C-G-12 | A `WHERE IS` built from the maps for hacks | `guide.md` | — | — | Follows S-06. |
| C-G-13 | The first-open `BEFORE YOU READ` notice, once per game | `guide.md` | ✅ | ✅ | `guideNotice` in the settings, a bit per game kind. |
| C-G-14 | The guide's text in the game's language | `guide.md`, `companion/i18n/guide` | ◐ | ◐ | The English DSL only (`gen_guide.py`). `L` per language |

### BATTLE

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| C-BT-01 | Opens by itself in a battle and gives the tab back afterwards | `battle.md` | ✅ | ✅ | `pd_ui_update` follows the snapshot. |
| C-BT-02 | `INFO`: both sides with the game's icons, your four moves with verdicts | `battle.md` | ✅ | ✅ | `SUPER 2x`, `NEUTRAL 1x`, `RESISTED 1/2x`, `IMMUNE 0x`, `STATUS`, `NO PP`. |
| C-BT-03 | `INFO`: the foe's weaknesses, and a tag on each side for the other side's threat | `battle.md` | — | — | The foe's `WEAK TO` / `RESISTS` plus the per-side tag (checked in V-16). `S` |
| C-BT-04 | `INFO`: the foe's ability, held item, stats with boosts and moves | `battle.md` | — | — | The battle struct has the fields; decoding them is the work. `M` |
| C-BT-05 | `SUGGESTIONS`: the three best picks with their best move | `battle.md` | ✅ | ✅ | The app's ranking. |
| C-BT-06 | `FOE TEAM`: the trainer's party in order, balls, fainted faded | `battle.md` | ✅ | ✅ | `gEnemyParty` plus `gBattlerPartyIndexes`. |
| C-BT-07 | Tapping a foe re-ranks INFO and SUGGESTIONS against it | `battle.md` | ✅ | ✅ | `NEXT` / `FAINTED` / `RESERVE` / `UNSEEN`. |
| C-BT-08 | `BATTLE HINTS` off (facts stay, verdicts go) | `battle.md` | — | — | One setting that suppresses the verdict text. `S` |
| C-BT-09 | `ONE-TAP MOVES` | `battle.md` | — | — | Needs C-BT-11 first. `S` afterwards |
| C-BT-10 | `FOE IVS`: a STATS page for the foe | `battle.md` | — | — | Shares C-P-09's decoding. `S` |
| C-BT-11 | `FIGHT` / `BAG` / `RUN` / `POKéMON` buttons on your turn | `battle.md`, `BattleInputController` | — | — | Presses the game's own buttons at the right moment; needs the battle menu's state per game. `L` |
| C-BT-12 | The `POKéMON` switch pane with `BEST` and `OUT` | `battle.md` | — | — | Works the game's party menu for you. `L` |
| C-BT-13 | One-press picks (tapping a move uses it) | `battle.md`, `ROM_FEATURES.md` | — | — | Per game cursor addresses (`gActionSelectionCursor`); the app lists which games have it. `L` |

### TRAINER CARD, STATES, ACHIEVEMENTS, SETTINGS

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| C-C-01 | The card's front: name, ID, money, dex count, play time, badges | `trainer-card.md` | ✅ | ✅ | `pd_card.c` and `pd_ui_card.c`; pixel checks pinned by `TrainerCardTest`. |
| C-C-02 | Tap to flip to the back and back again | `trainer-card.md` | ✅ | ✅ | The app's fold animation. |
| C-C-03 | Cards for the app's ten games (Unbound, Glazed, SoulGold, Lazarus, ...) | `trainer-card.md` | ◐ | ◐ | FireRed and Emerald only; the card style is `PD_CARD_*`. `M` per hack |
| C-C-04 | Card colour by stars, the clock's colon blinking | `trainer-card.md` | ✅ | ✅ | From `gameStats`. |
| C-C-05 | The largest whole size that fits | `trainer-card.md` | ✅ | ✅ | 2x on the 640x480 overlay, 1:1 on the 3DS. |
| C-ST-01 | `STATES`: ten slots, `SLOT 0` to `SLOT 9` | `saves.md`, `StatesScreen.kt` | — | — | Host side, see H-18. |
| C-ST-02 | A screenshot and a date under each slot | `saves.md` | — | — | Same as H-18. |
| C-ST-03 | `UNDO SAVE` and `UNDO LOAD` | `saves.md` | — | — | Same as H-19. |
| C-ST-04 | The current slot in the title, the slot hotkeys act on | `saves.md` | — | — | Same as H-20. |
| C-A-01 | RetroAchievements: sign in and play softcore | `achievements.md`, rcheevos | — | — | A new dependency (`rcheevos`), an HTTPS client and storage per host. `XL` |
| C-A-02 | The `CHEEVOS` tab with badges, descriptions and progress bars | `achievements.md` | — | — | Follows C-A-01. `M` |
| C-A-03 | Filter `SHOW ALL` / `LOCKED` / `UNLOCKED` | `achievements.md` | — | — | Follows C-A-02. `S` |
| C-A-04 | Unlock and progress popups over any tab, tap to open the entry | `achievements.md` | — | — | Follows C-A-02. `M` |
| C-A-05 | Leaderboards and challenge badges | `achievements.md` | — | — | Follows C-A-02. `M` |
| C-A-06 | Cheats pause achievements | `achievements.md` | — | — | Follows C-A-01 and Part 4's cheats. |
| C-A-07 | Achievement progress travels in the save state | `achievements.md` | — | — | A section in the state file; follows C-A-01. `M` |
| C-SET-01 | SETTINGS split into groups, with pages behind rows | `settings-reference.md` | — | — | Today one column of four rows. `M` |
| C-SET-02 | A search box on the settings (top screen only) | `settings-reference.md` | — | — | Only worth it after C-SET-01. `S` |
| C-SET-03 | Every row in the reference | `settings-reference.md` | ◐ | ◐ | Screen, fast-forward, states and leave game exist; Part 1 and this part list the rest. |
| C-SET-04 | `CLOSE GAME`, `RESTART GAME` and the resume warning | `companion.md` | ◐ | ◐ | `LEAVE GAME` exists; restart and the resume wording do not. `S` |

## Part 3: Features over the game: HUD, quick menu, battle buttons, makeover

The first three are not tabs: they live over the game and are the app's answer to
a single screen (`hud.md`, `controls.md`, `battle.md`). The makeover group is over
the game too, but it changes what you see rather than what you can press
(`companion.md`). All of them build on the snapshot and the ROM the core already
reads, so the hard part is drawing per frame, not knowing the game.

### HUD

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| P-HUD-01 | Party strip (icons, level, HP, status) on the game screen, tap for the summary | `hud.md` | — | — | The snapshot has everything; it is a second, smaller drawing path over the game. `M` |
| P-HUD-02 | Minimap with the head and the place name, tap for the whole map | `hud.md` | — | — | `pd_map` can draw a smaller copy. `M` |
| P-HUD-03 | Poké Balls with counts, tap for that pocket | `hud.md` | — | — | `S` |
| P-HUD-04 | Steps left while a repel works, with a draining bar | `hud.md` | — | — | Needs the game's step counter per game. `M` |
| P-HUD-05 | `PC` button | `hud.md` | — | — | Follows C-B-13. |
| P-HUD-06 | `BAG` button and a compact bag, with `USE` | `hud.md` | — | — | Follows C-B-11. |
| P-HUD-07 | `WILD`: this area's encounters with caught marks and a count | `hud.md` | — | — | HERE's wild tables, drawn small. `M` |
| P-HUD-08 | Steps aside for battles, menus, the town map | `hud.md` | — | — | The snapshot already knows a battle; menus are the hard part. `M` |
| P-HUD-09 | `LAYOUT` (CORNERS, CORNERS 2, RAILS, DOCK), `LOOK`, `SIZE`, per-part switches | `hud.md`, `FieldHudSettings.kt` | — | — | Four layouts times seven parts is a lot of drawing for a small feature; the app also has a compact fallback for narrow screens. Start with `CORNERS` and all parts. `L` |

### Quick menu

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| P-QM-01 | One hotkey opens it, ring or grid, labels on or off | `controls.md` | — | — | Seen as a labelled grid of tiles with an orange frame on the selected one (V-21). A second overlay with its own cursor. `M` |
| P-QM-02 | `PC` | `controls.md` | — | — | Follows C-B-13. |
| P-QM-03 | `FLY` | `controls.md` | — | — | Follows C-M-08. |
| P-QM-04 | The keep-away spray, with the steps each kind lasts | `controls.md` | — | — | A bag `USE` on the repel items, so it follows C-B-11. |
| P-QM-05 | `BIKE` / `GET OFF` | `controls.md` | — | — | Uses the registered item, per game. `M` |
| P-QM-06 | `ROD` | `controls.md` | — | — | Same as BIKE. `S` |
| P-QM-07 | `WILD`: encounters off and on | `controls.md` | — | — | A ROM patch, per game (S-16). |
| P-QM-08 | `SAVE STATE`, `LOAD STATE`, `FF`, `COMPANION` in the menu | `controls.md` | — | — | These four already exist as buttons elsewhere; putting them in the menu is the cheap half. `S` |
| P-QM-09 | An action that cannot run is greyed out with the reason | `controls.md` | — | — | Worth copying, it is what makes the menu feel safe. `S` |

### Battle buttons

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| P-BB-01 | `FIGHT`, `POKéMON`, `RUN` beside the battle, `RUN` greyed in a trainer battle | `battle.md` | — | — | On the R36S these would be pad buttons, not drawn ones, since the screen has no touch. `L` |
| P-BB-02 | `FIGHT` opens the move list with `PP`, type, power and the verdict | `battle.md` | — | — | Shares C-BT-11. |
| P-BB-03 | `POKéMON` opens the party list with `BEST` and `OUT` | `battle.md` | — | — | Shares C-BT-12. |
| P-BB-04 | They slide in and out and keep out of the game's own bag and party screens | `battle.md` | — | — | Polish on top of P-BB-01. `M` |

### Makeover (drawn over the game, the save untouched)

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| C-MK-01 | `PLAYER LOOK`: wear another hero, or a party Pokémon that follows you | `companion.md`, `PlayerLook.kt`, `PlayerLookSigsGen.kt` | — | — | Sprites come from the player's own other ROM, found by fingerprint; the map head, battle back, card and map all follow. `XL` |
| C-MK-02 | `RIVAL LOOK`: the rival wears the other hero | `companion.md` | — | — | Ruby, Sapphire and Emerald only, and it depends on C-MK-01. `M` |
| C-MK-03 | `TEXT LOOK`: the other game's text boxes and letters | `companion.md` | — | — | Font and box art per game and language. `L` |
| C-MK-04 | `FOLLOWER`: the first party Pokémon walks behind you | `companion.md` | — | — | Drawn by the app over the map, with turn, run, bike and surf poses. `L` |
| C-MK-05 | `DAY AND NIGHT`: dawn, dusk and night tints on the outdoors | `companion.md` | — | — | A colour transform per frame while walking; the cheapest of the five to try on the RK3326. `M` |

## Part 4: Library, save editor, cheats, other surfaces

### Library

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| L-01 | A list of games, D-pad or keyboard, A starts one | `library.md` | ✅ | ✅ | `menu.c` and the 3DS's own list, names only. |
| L-02 | A grid with covers | `library.md` | — | — | Covers need a source and a cache. `M` |
| L-03 | Quick views (`ALL`, `FAVORITES`, `RECENT`, `GB`, `GBC`, `GBA`, `DS`, `HACKS`) | `library.md` | — | — | `S` once there is more than one console to show |
| L-04 | Sorting (name, recent, console, ...) and favorites | `library.md` | ◐ | ◐ | Alphabetical only, read from the folder each start. `S` |
| L-05 | Rescan the ROMs folder and say what changed | `library.md` | ✅ | ✅ | Reading the folder at start is the rescan. |
| L-06 | Import a file with `+` | `library.md` | n/a | n/a | Both hosts read a real folder, so there is nothing to import. |
| L-07 | Rename and hide a game | `library.md` | — | — | Would need a per-ROM side file; hiding a file in a shared folder is rarely wanted. `S` |
| L-08 | Cover art from SteamGridDB, RetroAchievements or a picked image | `library.md` | — | — | A network client and a key on the handheld; low value for a 3-entry list. `L` |
| L-09 | Dual-screen launcher (cover, last played, legend, X swaps) | `library.md` | n/a | n/a | The 3DS boots into the game list by design. |
| L-10 | The game's `INFO` page (ROM, save, states, achievements) | `library.md` | — | — | Most of the values are already on hand; the page is drawing. `M` |
| L-11 | `PICK A GAME` and `FORGET MATCH` | `library.md` | — | — | Follows S-08. |
| L-12 | A save folder per game | `saves.md` | — | — | The app's way to share a save with another emulator; on the R36S the save already sits next to the ROM. `S` |
| L-13 | `PLAYING` marker and a save icon per game | `library.md` | — | — | `S` |
| L-14 | `PC BOXES` in a game's menu, opening the offline save editor | `LibraryActivity.kt` (`FeatureFlags.saveEditor`) | — | — | Not in 1.3.2 (V-36): the entry point is behind a flag that is off. `S` |

### The save editor (the library, with the game closed)

`SaveEditorActivity.kt` with `PcBoxesScreen.kt` and `TradeScreen.kt`: the app opens
a game's save file while the emulator is not running, edits it, and writes it back
in the game's own format. It is **behind a feature flag** upstream
(`FeatureFlags.saveEditor`, "until it's been tried on real saves"), and the flag is
**off in 1.3.2**: there is no `PC BOXES` in the game menu (V-36), and it has no
manual page. Treat it as upstream code that has not shipped.

For a handheld it is unusually valuable: it needs no emulator running, it is the
one thing a plain emulator cannot do, and it can be tested on a desktop against
copies of saves. It also edits a battery save, so the "saves are sacred" rules
apply in full: a backup before writing, never a write into a savestate, and a
refusal when the file changed under us.

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| SE-01 | Read a save's party and its 14 PC boxes | `Gen3SaveData.kt`, `Gen3SaveFile.kt` | — | — | The Gen 3 save format: two blocks, checksums, encrypted party records. `L` |
| SE-02 | Move a boxed mon to another slot, swapping with what is there | `Gen3SaveEditor` | — | — | Party records are shown, not moved (they carry stats the PC's do not). `M` |
| SE-03 | Change a mon's held item | `Gen3SaveEditor` | — | — | Mail is excluded, as upstream does. `S` |
| SE-04 | Change a mon's moves, and add one | `Gen3SaveEditor` | — | — | `S` once SE-01 exists |
| SE-05 | Change money and coins | `Gen3SaveEditor` | — | — | `S` |
| SE-06 | Change the bag | `PcBoxesScreen` (BAG page) | — | — | Shares the bag reader from C-B-01. `M` |
| SE-07 | Only what the game allows (`Gen3Legal`) | `Gen3Legal.kt` | — | — | Eggs and Bad Eggs are not editable. The legality rules are the interesting half. `M` |
| SE-08 | Write the save the way the game does, with a backup first | `Gen3SaveEditor.build` | — | — | Checksums and both blocks; a backup before the write, per `AGENTS.md`. `M` |
| SE-09 | `TRADE` between two games' saves, both directions checked | `Gen3Trade.kt` | — | — | A mon for a mon, or into an empty slot, with the games' own rules; nothing changes until every check passes. `L` |
| SE-10 | A trade that evolves asks about the evolution first | `TradeEvolution` | — | — | `S` |
| SE-11 | Refuse on an egg, a Bad Egg, or a save that changed while open | `Gen3Trade`, `TradeRefused` | — | — | Same safety shape as `pd_resume_is_fresh`. `S` |

### Cheats

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| X-01 | One master switch, off by default | `cheats.md` | — | — | mGBA has the cheat engine in the core; the port never enables it. `M` |
| X-02 | Add a code by hand (name, code, `AUTO` type) | `cheats.md` | — | — | A text page in the core UI plus storage per ROM. `M` |
| X-03 | Import a RetroArch `.cht` or an mGBA `.cheats` file | `cheats.md` | — | — | Both formats are text; the picker is per host. `M` |
| X-04 | Every bad line reported, the good ones kept | `cheats.md` | — | — | The app's own parser behaviour; worth copying. `S` |
| X-05 | Turning cheats on and off while the game runs | `cheats.md` | — | — | `mCheatDevice` can be told at any time. `S` |
| X-06 | Removing a cheat | `cheats.md` | — | — | `S` |
| X-07 | Cheats belong to one ROM and never travel with a save | `cheats.md` | — | — | One file per ROM id, outside the battery save. `S` |
| X-08 | Switching a cheat off takes its patches back out | `cheats.md` | — | — | mGBA's own semantics, but it needs a check per code type. `M` |

### Other surfaces

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| O-01 | App language picker and translated UI | `companion.md` | — | — | See H-51. |
| O-02 | `HELP` (the manual in the app) | `welcome.md` | — | — | See H-52. |
| O-03 | `LICENSES` page | `README.md` | — | — | GPLv3 and the extra credit term must stay visible somewhere; a plain text page is enough. `S` |
| O-04 | The setup wizard (ROMs folder, saves folder, covers, game buttons) | `install.md` | n/a | n/a | Both hosts have no setup to walk through. |
| O-05 | An app icon, splash and a PortMaster entry | app manifest | ✅ | ✅ | `linux/arkos/` has icon, screenshot and `gameinfo.xml`. |

## Part 5: Game and language support

The companion only reads a game it can recognise, and each game needs its own
RAM addresses, ROM tables and probes (`AGENTS.md`). This is the widest gap
between the app and the ports, and the one most players notice: most cards in a
handheld are not English vanilla.

| ID | Feature | Upstream | Linux | 3DS | Notes / effort |
| --- | --- | --- | --- | --- | --- |
| S-01 | FireRed rev 1, English: every tab, ROM tables and the FOE TEAM | `NativeConfig` | ✅ | ✅ | The reference game for the ports. |
| S-02 | FireRed rev 0, English | `NATIVE_FIRERED_REV0` | ◐ | ◐ | RAM tabs, MAP art and party icons. No DEX, no GUIDE's live pages, no FOE TEAM, no CARD (`FIRERED_REV0_RAM_ONLY`). `M` |
| S-03 | LeafGreen rev 0 and rev 1 | `NATIVE_LEAFGREEN*` | ◐ | ◐ | Same as S-02 plus the CARD, and the FOE TEAM on rev 1. No DEX tables, so no EVOLVE, and no GUIDE tables. `M` |
| S-04 | Emerald, English | `NATIVE_EMERALD_RETAIL` | ✅ | ✅ | Second reference game. |
| S-05 | German, French, Italian, Spanish, Japanese retail GBA games | `NativeReader.kt` (`BPED`, `BPGD`, `AXVJ`, ...), `GameText*Gen.kt` | — | — | Each language needs its addresses (a pret build per language), a charset for `pd_gen3_text` and its name / item / move tables. The largest real gap for the handheld. `L` per game, `XL` for all five |
| S-06 | Ruby and Sapphire (rev 0 to 2, every language) | `NATIVE_RUBY*`, `NATIVE_SAPPHIRE*` | — | — | The third engine family: party, bag, map, dex and guide tables plus the probes. `L` |
| S-07 | The app's ROM hacks (Unbound, Radical Red, Gaia, Odyssey, Glazed, Seaglass, Legacy, Kaizo, Imperium, R.O.W.E., SoulGold, Lazarus, Recharged Yellow, ...) | `NativeReader.kt`, `docs/ROM_FEATURES.md` | — | — | Each hack is its own address set, probe and often its own palettes. The ports detect a >16 MB FireRed or Emerald as a hack and leave it alone. `M` per hack |
| S-17 | Reading a FireRed-layout hack partially (>16 MB, its own data) | `Poller.detect`, `GameTitles.kt`, `BestEffortStore` | — | — | The cheap half of S-08 and the case the app already handles for the reference ROM (V-01): stop refusing at 16 MB, load the rev 0 / rev 1 configs, let every probe decide per tab, and say `PARTIALLY SUPPORTED` in the app's words. Needs a per-tab "did the probe pass" flag in the core; no new addresses. `M` |
| S-08 | Best effort matching (read the ROM as each known game and keep the fit) | `Poller.detect`, `BestEffortStore` | — | — | `XL`: it needs every reader to be safe against wrong addresses first, which is the `AGENTS.md` probe rule. |
| S-09 | The match is remembered, `PICK A GAME`, `FORGET MATCH` | `supported-games.md` | — | — | Follows S-08. |
| S-10 | Game Boy / Game Boy Color companion (party, bag, map, dex) | `GbField.kt`, `GbMon.kt` | — | — | Follows H-02. `XL` |
| S-11 | Nintendo DS companion | `melonDS` readers | — | — | Follows H-03. `XL` |
| S-12 | Item descriptions read from the ROM (`gItems`) | `RomItemText.kt` | — | — | Removes the frozen `pd_item_desc.c`, fixes hacks and every other language. `M` |
| S-13 | The game's own party slot art and colours | `PartySlotStylesGen.kt`, `BagPalette` | ◐ | ◐ | FireRed's look is drawn, not extracted; the app extracts per game. `M` per game |
| S-14 | The guide's written pages in the game's language | `companion/i18n/guide` | — | — | Follows S-05; `gen_guide.py` reads the English DSL only. `L` per language |
| S-15 | The real-time clock from the device, dead-battery workaround | `fast-forward.md` | ◐ | ◐ | mGBA keeps an RTC; whether it is seeded from the device clock and survives a dead cartridge clock is unchecked on both hosts. `S` |
| S-16 | Field actions that write into the game (PC, FLY, item USE / GIVE / TAKE, WILD off, auto-run, run indoors, bike, rod) | `Pc.kt`, `Fly.*`, `FieldItemFns.kt`, `WildEncounters.kt`, `AutoRun.kt` | — | — | Every one is a per-game patch or script call with its own check, and the app's table lists which ROM has which. `XL` for all, `L` per action |

## Part 6: The data pipeline

The ports never hold game data by hand; `3ds/tools/` generates it from the app's
Kotlin sources, and CI fails when re-running the generators would change the
result (`AGENTS.md`). A ported feature that needs new data therefore starts in the
generator, not in `pd_tables_gen.c`.

| ID | Generated today | Source | Covers |
| --- | --- | --- | --- |
| D-01 | Names, moves, types, item names, gender ratios | `gen_tables.py` | C-P-01, C-B-02, C-BT-02 |
| D-02 | Map sections, art fingerprints, cursor grids | `gen_map_tables.py` | C-M-01 to C-M-06, C-C-01 |
| D-03 | Guides, bosses, area data | `gen_guide.py` | C-G-03 to C-G-08 |
| D-04 | The 1-bit font, ♂ and ♀, the icons | `gen_font.py` | everything drawn |
| D-05 | Item descriptions, frozen | `pd_item_desc.c` (hand kept) | C-B-08. To be replaced by S-12; do not extend it |
| D-06 | Egg groups and `tmhmLearnsets` / `tmhmMoves` | not generated yet | C-D-07, C-D-10, C-P-10, C-B-09 |
| D-07 | Text tables per language, and a charset per language | not generated yet | S-05, S-14, H-51 |
| D-08 | Party slot palettes and bag palettes per hack | not generated yet | C-P-13, S-13 |
| D-09 | The guide's translated pages | not generated yet | S-14 |

Two rules that come out of `AGENTS.md` and are easy to break while porting:

1. **Every ROM table gets a probe.** A moved table must be rejected, not
   misread: the app's own check (BULBASAUR = SEED / LV 16, ROXANNE) is the
   pattern. New tables (learnsets' TM half, egg groups, `gItems`) need theirs.
2. **A savestate never carries the battery save.** Anything new that stores
   state (ten slots, achievement progress, undo) keeps `SAVESTATE_RTC` and
   nothing from the save block.

## Part 7: What does not fit the hardware

Recorded so the list is not read as an omission. Each one is a deliberate `n/a`.

| Upstream feature | Why not | Where it would land instead |
| --- | --- | --- |
| Dual-screen launcher, `SWAP SCREENS`, `SINGLE SCREEN` | The app's problem is "one screen, companion must share it". The 3DS has two screens with the game on top and the companion on the bottom; the R36S has one and no touch. | Nothing to port. |
| Phone held upright, companion docked under the game, grip sizing | Android/orientation specific. | If the Linux port ever runs on a 16:9 screen, `render.c`'s scaling is the place, not the core. |
| `TOUCH CONTROLS` and the layout editor | The R36S has no touchscreen; on the 3DS the touchscreen is the companion. | Pad mapping (H-43). |
| Casting to a TV | Android presentation API. | `--fullscreen` and the display's own mode. |
| Setup wizard, folder pickers via Android SAF | Both hosts read a real filesystem path given by a script or a constant. | A line in the README, as today. |
| Cover art from SteamGridDB / RetroAchievements | A network client and an API key on a handheld whose list is three entries long. | `L-02` only if the library ever grows. |
| Update check and install | PortMaster and 3DS homebrew channels own updates. | Their release flow. |
| Sleep and resume as Android lifecycle | The R36S suspends the whole process; the 3DS has HOME. | `H-13` on the 3DS would cover the visible half. |
| `HOME` / guide button as BACK | Android button. | Nothing. |
| DS games | No DS core in either port, and the RK3326 has no headroom for one. | On the 3DS, hand `.nds` files to TWiLight Menu (`H-03`). |

One thing that is *not* on this list although it looks like it belongs:
**`EXPAND` (H-32)**. It fits the hardware in principle, and it is the app's
signature feature, but it rebuilds map strips every frame per game. Do not start
it before phase 3's frame times are known; if it ever runs, it runs on a desktop
and on a New 3DS first.

## Part 8: Verifying on the phone

You have the app on your phone, which makes it the reference the ports are
measured against. **Everything in this part is done in the Android app, not on
the R36S or the 3DS.** There are three different questions in this file and it is
easy to mix them up:

| Question | Where the answer comes from | Where it is written down |
| --- | --- | --- |
| What does the app do, exactly? | The Android app on your phone, by using it | This part, answers filed against the IDs |
| What do the ports do today? | Reading `3ds/core`, `3ds/ctr` and `linux/` | Parts 1 to 6, the Linux and 3DS columns |
| Does it hold up on the hardware? | The R36S and the 3DS themselves | Not here: `linux/PLAN.md` phase 3 (`--stats`, frame times, exFAT writes, PortMaster install) and the 3DS README's hardware section |

So this part never asks you to start ArkOS. The only port item that goes on a
device rather than on the phone is phase 3's list, and that has its own
checklist in `linux/PLAN.md`.

Two things before starting: note **which game, version and language** you play
for each check (the ports support FireRed rev 1, LeafGreen's RAM and English
Emerald only, so a German Emerald would answer both S-05 and most content rows),
and keep real Pokémon, move and item names out of anything that gets copied into
this file (the repo's publication rule).

### The route through the app

The tables below are ordered by how much an answer changes the plan. Walking the
app is easier in another order, so use this when testing and file each answer
under its ID:

| Block | Where you are | Checks |
| --- | --- | --- |
| 0 | Settings > `VERSION` | Note the app version, so the answers can be tied to an upstream state |
| 1 | Library, long press a game, its menu and `INFO` | V-01, V-02, V-36 |
| 2 | The companion on the title screen, then walking | V-03 |
| 3 | PARTY, a summary, `STATS` and `LEARN` | V-04 to V-08 |
| 4 | BAG, an item, a TM, the `PC` button | V-13 to V-15, V-18 |
| 5 | DEX, an entry, `AREA`, `MOVES`, the filter list | V-09 to V-11 |
| 6 | GUIDE, `EVOLUTIONS` | V-12 |
| 7 | MAP, `PLACES`, `FLY` | V-19 |
| 8 | A battle, `INFO` scrolled down, then `BATTLE HINTS` off | V-16, V-17 |
| 9 | The field: quick menu, HUD, `ASPECT`, `SHADERS`, `DARK MODE`, `STATUS BAR` | V-21 to V-30 |
| 10 | STATES, CHEEVOS, CHEATS | V-25, V-34, V-35 |
| 11 | A second game, ideally another language or a hack | V-31 to V-33 |

### Which game to test with

Two games, two jobs. Mixing them up is the main way this list produces wrong
answers.

| Game | Use it for | Why |
| --- | --- | --- |
| A retail build the ports already read: English FireRed rev 1, FireRed rev 0, LeafGreen, or Emerald (`BPRE`, `BPGE`, `BPEE`) | Blocks 3 to 10, the content checks | The app then has every tab (DEX, GUIDE, STATS, LEARN), so each check is possible at all. And the same ROM can later be run through `linux/build/pokedaisy` and the 3DS preview, so app and port are compared on the same save, screen by screen |
| The hack you actually play (`V-01`: 32 MB, `BPRE`) | Block 2 and Block 11, the support checks | It is the best-effort case S-17 is about: what a partial match still reads, and what it switches off |

Never check content on the partially supported hack: a missing DEX or GUIDE there
would read as "the app has no filter list and no EVOLVE page", which is false for
a retail game and would send the port in the wrong direction.

If the only retail game you own is German or French, use it anyway: the content
behaviour is the same, but the port cannot read that ROM yet (S-05), so the
side by side comparison waits.

#### Reference games

| Role | Game |
| --- | --- |
| Content blocks 3 to 10 | FireRed rev 1, English (`BPRE`) |
| Support blocks 2 and 11 | `Pokemon - Saiph` (32 MB, `BPRE`, partial match) |

A note on items: at the start of a save the bag is empty, so Block 4 has nothing to
show. The quickest way to fill it is the PC in the player's bedroom (the first
healing item), then the first shop once the parcel is delivered (healing items and
the first ball). The first machine that teaches a move comes with the first badge.
The only shop that sells TMs in FireRed and LeafGreen is the third floor of the
Celadon department store: TM05 for 1000, TM16 and TM20 for 3000, TM15 for 7500. A
TM is one use in the retail game, so buy one to look at, not to spend. Do Block 5
and 7 first, they need no items.

### A. What the app thinks you are running

| ID | On the phone | What I need |
| --- | --- | --- |
| V-01 | Library, long press your game, `INFO`, `GAME` section | The name the app recognised, and, for a hack, whether it says FULLY or PARTIALLY supported (S-02, S-03, S-07, S-08) |
| V-02 | Same page, `ROM` section | Game code and revision, so I can map it to a `NativeConfig` entry |
| V-03 | Play it and open the companion | Which tabs exist (a missing DEX or GUIDE means the game has no tables for them, which decides C-04) |

### B. Content the ports could copy next (read only, safe)

| ID | On the phone | What I need |
| --- | --- | --- |
| V-04 | PARTY, tap a mon, then `STATS` | Which numbers the page shows, whether the IV bars turn gold, whether `HIDDEN POWER` has a power or only a type (C-P-09) |
| V-05 | PARTY, summary, `LEARN` | One verdict with its full reason line, in your own words, and whether the advice feels right to you (C-P-10) |
| V-06 | PARTY, summary, the `WEAK TO` / `RESISTS` / `IMMUNE TO` blocks | Whether the multipliers are on every line and whether the blocks stay readable at a glance (C-P-08) |
| V-07 | PARTY, a mon that holds something | Where the held item tag sits and what it shows (C-P-03) |
| V-08 | PARTY, summary | Whether the strip of party icons at the bottom shows the mon you are on, and what a tap does (C-P-11) |
| V-09 | DEX | Whether the filter list keeps its counts when you change filters, and whether the counter by the arrows follows the filter (C-D-03, C-D-11) |
| V-10 | DEX, an entry, `AREA` | How a line reads for a route, with its method word and percent, and whether gifts and trades are mixed in (C-D-09) |
| V-11 | DEX, an entry, `MOVES` | How `LEVEL UP` and `TM / HM` are split, and whether the TM list is the machines you carry or the whole set (C-D-10) |
| V-12 | GUIDE, `EVOLUTIONS` | The order of the page: party first, then special evolutions, and what the hint hides (C-G-10) |
| V-13 | BAG | Whether `ALL` is a pocket of its own, and what the order button's four states look like (C-B-04, C-B-05) |
| V-14 | BAG, pick a TM | The `ABLE` / `NOT ABLE` / `LEARNED` / `BEST` tags and the line under them, especially the "should forget" suggestion (C-B-09) |
| V-15 | BAG, an item a hack renamed | Whether the description is the hack's own text or the original game's (S-12, C-B-08) |
| V-16 | BATTLE, `INFO`, scroll down | Which foe facts are there: ability, held item, stats with boosts, moves, weaknesses (C-BT-03, C-BT-04) |
| V-17 | Settings, `BATTLE HINTS` off, then a battle | What disappears and what stays, in your words (C-BT-08) |

### C. Actions that write into the game

| ID | On the phone | What I need |
| --- | --- | --- |
| V-18 | BAG, `PC`, standing on a route | Whether the game's own PC opens over the field and what happens when you press BACK out of it (C-B-13) |
| V-19 | MAP, `FLY` | The list it offers, and the exact message when you are indoors or lack the badge (C-M-08) |
| V-20 | BAG, `USE` on a healing item | How the target is asked for, and how an item it cannot help is shown as dimmed (C-B-11) |
| V-21 | Quick menu over the field | Which actions you actually use, in order, and which ones are greyed out with a reason (P-QM-01 to P-QM-09) |
| V-22 | Quick menu, `WILD` off | Whether the tag on the tile is enough for you to notice it is still off (P-QM-07) |
| V-23 | Quick menu, `BIKE` and `ROD` | Whether it needs you to face the water first, and what the game says when it fails (P-QM-05, P-QM-06) |

### D. Layout, which decides what is worth drawing at 640x480 and 320x240

| ID | On the phone | What I need |
| --- | --- | --- |
| V-24 | HUD on, walking, then in a battle | A screenshot, plus which of the six parts you would miss if they were gone (P-HUD-01 to P-HUD-07) |
| V-25 | STATES tab, save two slots | Whether ten slots read as a grid at a glance, and whether a screenshot of the moment is still useful (H-18) |
| V-26 | A battle, one screen sideways | Whether `FIGHT` / `POKéMON` / `RUN` beside the battle are reachable with one thumb, which tells me whether pad buttons are enough on the R36S (P-BB-01) |
| V-27 | `ASPECT` > `EXPAND` | How much map you gain and whether you would use it on a 4 inch screen; this decides whether H-32 is ever started (H-32) |
| V-28 | `SHADERS` on the game | Which filter you actually keep on, if any (H-34) |
| V-29 | `DARK MODE` on | Whether the companion's dark look matters before the pixels get sharper (H-39) |
| V-30 | `STATUS BAR` on | Which of the five values you read while playing (H-38) |

### E. Support paths

| ID | On the phone | What I need |
| --- | --- | --- |
| V-31 | A German or French game, if you have one | Which tabs work and whether names come out in that language (S-05) |
| V-32 | A hack, played once | Whether `PLAYS AS` appears and what it picks (S-08, C-08) |
| V-33 | The `WHICH GAME IS THIS?` page, if it ever shows | What the `LIKELY` tags look like (S-08) |
| V-34 | A game with an achievement set, and the `CHEEVOS` tab | Whether you use RetroAchievements at all; if not, C-A-01 stays at the bottom (C-A-01) |
| V-35 | Cheats, one code added and switched off | Whether turning it off really reverses it, as the manual claims (X-08) |
| V-36 | The library, long press a game, the menu | Whether `PC BOXES` is there, which tells me if the save editor's flag is on in your build (SE-01, L-14) |

### Report format

Anything is useful, this shape is easiest to file:

```
V-04 / C-P-09: Emerald (German, BPED), STATS shows ATK 21 with its IV bar, EV 0,
               NATURE bold, HIDDEN POWER as a type only, no power number.
V-24 / P-HUD:  screenshot 2026-10-27; party strip and balls matter, minimap
               not readable at this size.
```

### Answers from the phone

Filled in as the blocks are walked. Each answer carries its ID, so it can be read
next to the row it changes. Screenshots are the fastest way to answer: send them
in the chat and they are written in here as text, never committed. The app's own
screens carry real Pokémon, move and item names, and `CLAUDE.md` keeps those out
of the repository; the manual's pictures use invented creatures for that reason.

#### 2026-10-27, Block 0 and 1 (app `1.3.2`)

| ID | Answer |
| --- | --- |
| Block 0 | App version `1.3.2`, UI language German |
| V-01 | `Pokemon - Saiph`; `SPIEL` says `Pokémon FireRed`; `2. BILDSCHIRM` says `Teilweise (Versuch, als Pokémon ...)` |
| V-02 | `BPRE`, header `POKEMON FIRE`, `32,0 MB`, MD5 `ffb3d74e4b34db98e0f472e81af8966`, file changed 2020-03-27 |
| V-34 (partly) | RetroAchievements knows the file: `SET Spiel Nr. 16059`; not signed in, so no unlock count |
| V-36 | No `PC BOXES` in the game menu, so `FeatureFlags.saveEditor` is off in 1.3.2 |
| L-07 | Confirmed: `UMBENENNEN` and `AUSBLENDEN` (hide rather than delete, as a linked folder should) |
| L-11 | Confirmed: `SPIEL WÄHLEN` and `VERGESSEN` are on the INFO page |
| L-12 | Confirmed: `SPEICHERORDNER` (a save folder per game) |
| H-13 | Confirmed: `FORTSETZEN` is described as "macht weiter, wo du aufgehört hast" |
| H-15 | Confirmed: `SPEICHERSTAND LADEN` (load a save into the game) |
| H-16 | Confirmed: `BACKUP LADEN` (restore a backup) |
| H-18 | Confirmed: `SPIELSTÄNDE` shows `0 von 10 belegt` |
| X-01 | Confirmed: `CHEATS` is in the game menu in 1.3.2 |

What follows from it:

- **The game being played is a FireRed hack, 32 MB, and the app reads it only
  partially.** `2. BILDSCHIRM` says `Teilweise`, and the hint calls it an attempt
  to read it as Pokémon FireRed. That is S-08 (best effort) working as designed,
  and it is the case the ports refuse outright: `pd_game_detect` turns every
  FireRed over 16 MB into "ROM hacks aren't supported on 3DS yet".
- **This reorders the plan.** German retail games (S-05) are not the gap for this
  player; a FireRed-layout hack is. The cheap version of S-07 is to stop refusing
  over 16 MB and let the existing FireRed rev 0 / rev 1 configs try, with their
  probes rejecting a table that does not match, and to draw the same
  `PARTIALLY SUPPORTED` wording the app uses. That is exactly what the probe rule
  in `AGENTS.md` exists for.
- The save editor is code that has not shipped (SE-01 to SE-11, L-14). It stays in
  the inventory because the upstream code is there, but it is not something the
  app in your hand can do, so it is not a parity target any more.
- The app's own text is German while the ROM is English (`BPRE`), which is the
  normal case for the app: `LANGUAGE` follows the device, not the game (H-51).

#### 2026-10-27, Block 2 and 3 (FireRed rev 1, English)

| ID | Answer |
| --- | --- |
| V-03 | Tab bar on the title screen and in the game: five chips plus the grid and the gear. In a battle a sixth chip appears at the front and the last one gives up its place |
| C-05 | The title screen shows a window in `PARTY`: "Your party shows up here once you're in the game." with a blinking `PRESS START`, not zeros |
| C-04 | In a battle `BATTLE` sits at the front and `GUIDE` is gone from the bar. The ports instead put `BATTLE` in the `DEX` slot |
| C-P-01 | Slot: the ROM's icon, name, gender mark, `Lv 6`, HP bar with `21/21`; empty slots are drawn as empty game slots |
| C-P-06 | Summary: name, gender, icon, level, type chip, HP bar, EXP bar; the four moves on the right with type, `PWR` and `PP`; empty move slots drawn as `-` |
| C-P-07 | EXP bar with `NEXT 31` under it |
| C-P-08 | `WEAK TO` and `RESISTS` with a multiplier on every entry (three at 2x, five at 1/2x here). No `IMMUNE TO` block, because that mon has no immunity |
| C-P-09 | `STAT` / `IV` / `EV` columns and a `TOTAL` row; the nature shows a red up arrow on the raised stat and a blue down arrow on the lowered one; `HIDDEN POWER` gives the type and `PWR 36` |
| C-P-10 | `LEARN` lists `NEXT LEVELS` (LV, type chip, verdict `LEARN`, `PWR` / `ACC` / `PP`) and, with a machine in the bag, `TMs IN THE BAG` with the same shape and a verdict of its own (`SKIP`). Tapping an entry opens a short reason in capitals (`WEAK EFFECT`, `ITS MOVES ARE BETTER`); no red `FORGET` tag when the verdict is `SKIP` |
| C-P-11 | The summary's bottom bar holds `STATS` / `MOVES`, `LEARN`, the party icon strip and a blue back button |
| C-BT-02 | `INFO` shows both sides with the ROM's icons, your four moves with a verdict each (`NEUTRAL 1x`, `STATUS`, `NO PP` on an empty slot) |
| C-BT-03 | The foe side lists `WEAK TO` and `RESISTS` with multipliers, and each side carries a tag for the other side's threat (here a 2x tag next to `YOU`) |
| C-BT-04 | The foe side also has `ABILITY`, `HELD ITEM` (`NONE` here), the five stats and a `MOVES` list for the foe with `PWR`, `PP` and a verdict per move |

What follows from it:

- The bar during a battle is six chips: the battle tab pushes the last tab off. `C-04` now says what the app does, so the port's "BATTLE takes the DEX slot" is a difference, not a simplification, and it should follow the app instead.
- `STATS` and `LEARN` are buttons inside the summary, not tabs. That is what `C-P-09` and `C-P-10` have to be in the core, and it keeps the tab bar as it is.
- `LEARN`'s TM half could not be checked yet: it needs a TM in the bag. `V-05` stays open for that one detail.
- `C-P-08` needs the third block (`IMMUNE TO`) from a mon that has an immunity, and `C-BT-03`'s per-side threat tag is a detail the ports do not draw at all today.

#### 2026-10-27, Block 4 (BAG, FireRed rev 1, a save with items)

| ID | Answer |
| --- | --- |
| V-13 | Pockets run down the left in the game's yellow tabs and `ALL` is a pocket of its own, selected first. `FAVORITES` comes and goes with the star. Below the pockets sit two blue buttons: `PC` and the order |
| C-B-01 | `PC` and the order button sit under the pockets, not beside the list. With an empty bag only `ALL` is drawn and the list says `No items`; a pocket appears once it holds something |
| C-B-02 | A row is the item's icon, name and `× n`; the rows the companion can use carry a small ball mark, and the line under the list says `USE or GIVE it from here` |
| C-B-04 | `ALL` exists and is the default, and it mixes pockets (key items appear between the others) |
| C-B-05 | The order button reads `DEFAULT` / `NAME` / `TYPE` / `COUNT` with an up-down arrow, so all four states are confirmed |
| C-B-06 | The description bar has a star on its right. Starring puts a gold star before the name in the list and adds a `FAVORITES` pocket; unstarring the last one removes the pocket again |
| C-B-08 | The description is a blue box with the item's icon and the game's own text, with `USE` and `GIVE` on its right |
| C-B-07 | A TM's row reads `TM05 ROAR`, so the app puts the move it teaches after the number, the same as the HM rows |
| C-B-09 | A machine shows the party as six icons with `ABLE` / `NOT ABLE` under each, in place of the description's text: three of six able for the TM bought at the department store. No `BEST` tag and no line under the icons in that case |
| C-B-11 | `USE` and `GIVE` show for a usable item; the same bar for an HM has neither. A tap on `USE` replaces the bag list with the party as a 2x3 grid and a bar under it: the item's icon, `Use <item> on which POKéMON?` and `CANCEL` |
| V-20 | The target picker, but nothing was dimmed: that party was at full HP with no status, so a cure would not have helped anyone |
| C-P-03 | The held item tag is a thin bar over the top of the slot, with the item's icon and name, seen in the `USE` party grid |
| C-B-13 | `PC` opens the game's own PC over the field with its own menu (withdraw, deposit, move, move items) and the companion stays where it is |
| P-QM-01 | The quick menu opens as a labelled grid over the field: `PC`, `FF`, `FLY`, `LOAD STATE`, `SAVE STATE`, `REPEL`, `BIKE`, `WILD`, `ROD`; the selected tile gets an orange frame |
| P-QM-05 to P-QM-07 | `BIKE`, `ROD`, `WILD` and the keep-away item are tiles of their own, not a submenu. While encounters are off the `WILD` tile carries an `OFF` tag |

What follows from it:

- The bag is drawn as the game's own bag (yellow pockets, cream list on the game's
  paper colour), with the companion's own controls in blue under the pockets. The
  port draws one pocket at a time and a white list, so `C-B-01`'s look is a real
  difference, not only the extra `ALL` and the order button.
- The star is the cheapest of the bag features to port (one flag per ROM, one
  extra pocket) and the one a player notices fastest, so it belongs in step 1
  rather than with the TMs.
- One screenshot showed only `ALL` with `No items` under it, while the same save
  showed a full bag in the others. Which moment that was is still open (V-13), and
  it decides whether `C-B-01` needs an "empty" state at all.

#### Still open

| ID | What is missing | When |
| --- | --- | --- |
| C-B-11 (dimming) | The target picker is seen, but nothing was dimmed because that party had nothing to cure; whether it dims unhelpful targets is unseen | A party with a status or a fainted mon, any time |
| V-24 (HUD) | Closed from the source instead: the reference phone does not draw the HUD upright, and the chat stopped accepting images | Nothing; the parts and layouts are recorded above |
| C-B-09 (`BEST`) | Whether `BEST` needs the advice to say "learn it" rather than plain learnability | A machine the advice rates `LEARN`; low value |
| V-15 | An item a hack renamed, to see whether its text comes from the ROM | With Saiph or another hack |
| Blocks 10 and 11 | `CHEATS` as a shipped screen, and a second game that is not a FireRed hack | Only if a port decision needs them; everything else in Blocks 5 to 11 was closed from the source or from the screenshots above |

#### 2026-10-27, Block 5 (DEX, FireRed rev 1, English)

| ID | Answer |
| --- | --- |
| V-09 | Left: the `POKéDEX` title, `SEEN 184` and `OWNED 86`, a filter box (`ALL` 386, `CAUGHT` 86, `SEEN ONLY` 98, `NOT SEEN` 202) and a `NATIONAL DEX` button. Right: rows of a ball mark, the sprite, `No.001` and the name; the ball is red when caught and grey when only seen |
| V-10 | `AREA` groups by place: the place name in bold, a method line under it (`GRASS / CAVE`, `PATTERN BUSH`), and `LV 5 5%` aligned right |
| V-11 | `MOVES` has `LEVEL UP` (the level in red, the move's name, a type chip at the right) and `TM / TUTOR`, which reads `NONE` when empty |
| C-D-01 | Confirmed, and the numbers are the national ones: the counter between the arrows reads `11 / 386` |
| C-D-02 | Confirmed: the `NATIONAL DEX` button sits under the filter box |
| C-D-03 | Confirmed, every filter carries its count |
| C-D-04 | Entry: the picture on a grey square, a footprint icon before the category, the category, one type chip, `HT` and `WT` in the game's own units, then the dex text |
| C-D-05 | The base stats are five rows with a bar each, then `TOTAL` and `ABILITY` under a divider (seen on a second entry, scrolled) |
| C-D-07 | The footprint is drawn before the category, which the ports do not. Under the ability come `EGG GROUP`, `GENDER` (both symbols with a percentage each) and `CATCH RATE`, which the ports also lack. The `INFO` page scrolls |
| C-D-08 | `EVOLVE` draws one group per species that evolves: the species, an arrow to what it becomes, and the condition under the arrow in red (`LU 7`). The page's own species is red wherever it appears, and the family is shown both ways |
| C-D-09 | Confirmed (see V-10) |
| C-D-10 | Confirmed (see V-11) |
| C-D-11 | Two green arrows with the `11 / 386` counter between them, above the tab bar |

What follows from it:

- `AREA`'s two columns are the method under the place and the `LV n x%` on the right,
  with one block per place. That is one drawing pass over the same wild tables HERE
  already reads, so `C-D-09` is cheaper than it looked.
- `MOVES` is the plain list of level-up entries plus one line of text for the TM
  half, and that label follows the game (`TM / TUTOR` here, not `TM / HM`).
- The footprint in `C-D-07` is the kind of detail that can stay out of a first
  port; the egg group, gender and catch rate lines are the ones worth having.
- Still unseen: the `INFO` page below the stats (`TOTAL`, ability, egg group,
  gender, catch rate) and the `EVOLVE` chip, which the ports already draw.

#### 2026-10-27, Block 6 and 7 (GUIDE and MAP, FireRed rev 1)

Two of the guide screenshots did not arrive (they were too large), so the `TIPS`,
`WHERE IS` and `STUCK?` pages are still unseen; the page list shows all six.

| ID | Answer |
| --- | --- |
| C-G-01 | `NEXT BOSS` lists every entry as `POKéMON n`, its level and a `HINT` / `ANSWER` tag; one tap opens the hint line (`HINT: WATER / ICE TYPE`), the next the answer in red: the species and its four moves, with the sprite |
| C-G-07 | The page opens on the next boss (`NEXT: <name>`, with `POKéMON LEAGUE · 5 POKéMON · UP TO LU 66 · YOUR BEST LU 72` under it) and continues with the following bosses below, each collapsed to its header |
| C-G-03 | `HERE` starts with the place in red and a line about it (`No wild POKéMON on this map. Its buildings included. A ball = done.`), then `TO DO` rows with a `HINT` tag each |
| C-M-01 | The region map with the name in a strip at the top left, city markers, and the player's head on the tile |
| C-M-04 | `ME` is a white button under `FLY`; `PLACES` sits above it |
| C-M-05 | `PLACES` is a full-screen list with a back arrow: sections (`TOWNS & CITIES`, `ROUTES`, `OTHER PLACES`) and, on the right of every row, the map page it belongs to (`SEVII 1-3`). Routes include sub-places (`ROUTE 4 POKECENTER`) |
| C-M-06 | The region button (`SEVII 1-3`) sits left of `PLACES`, so the map page is switched there |
| C-M-08 | `FLY` is a blue button under the region button, and it is there with a party that knows the move |

What follows from it:

- `NEXT BOSS` is not one boss: the next one is expanded and the rest follow as
  headers, so the port's single boss is a narrowed version, not a different design.
- The hint-then-answer pattern is used per party member of the boss, not only per
  hand-written entry, which is why the port can reuse the same row drawing.
- `PLACES` carries the map page per row. The ports already group and sort the same
  way (`pd_map`), so this is one extra right-aligned label.

#### HUD, read from the source instead of the phone

The HUD could not be photographed: on a phone held upright with the companion
under the game it is not drawn at all (`hud.md`), and the images stopped arriving
in the chat. So it was read out of `FieldHud.kt` and `FieldHudSettings.kt`, which
is enough to size the port:

| Part | What the source says |
| --- | --- |
| Layouts | `CORNERS` (minimap top right, party bottom left, balls and buttons bottom right), `CORNERS 2` (right side swapped), `RAILS` (two columns beside the GBA's 3:2 picture), `DOCK` (one bar along the bottom, minimap top right) |
| Looks | `GLASS`, `WHITE GLASS`, `WINDOW` |
| Sizes | `SMALL`, `NORMAL`, `LARGE`, one GBA pixel per step |
| Parts, each switchable | `PARTY`, `MINIMAP`, `POKé BALLS`, `REPEL STEPS`, `PC`, `BAG`, `WILD POKéMON` |
| Narrow screens | a compact layout drops the party and minimap and puts the balls and a `MAP` button in the bottom left |

What follows from it:

- Seven parts times four layouts is the reason `P-HUD-09` is the most expensive row
  in the inventory. A port would start with one layout (`CORNERS`) and all parts.
- The narrow-screen fallback already exists in the app, which is the closest thing
  to what a 640x480 handheld would need.

#### Closed from the source, no screenshot needed

Reading the app first is the rule this list should have followed from the start. The
source answers the structural questions; the phone only answers what the source
cannot: which features a shipped build exposes, what the app recognises a ROM as,
and how a page breaks when there is no room.

| ID | Answer from the source |
| --- | --- |
| C-01, C-02 | The bar is the chosen tabs the game has, then its other tabs while there is room, then `MORE` if anything is left over (and no `MORE` chip when nothing is), then the gear. `MAX_BAR_TABS` chosen tabs, one fewer while a battle is on |
| C-03 | A per-tab shown/hidden pick (`SETTINGS > TAB BAR`), shared by every game |
| C-04 | A tab is available only when its data matched: `DEX` needs the dex tables, `GUIDE` a guide or evolutions, `CARD` the card and its art, `ACHIEVEMENTS` a set. The rest fill the bar, so a partial match loses `DEX` and `GUIDE` and gains the others |
| C-ST-01 to C-ST-04 | `SAVE STATES` with `SLOT n` in the title, `UNDO SAVE` and `UNDO LOAD` above the cards, a 3:2 thumbnail and a date per card, `SAVE` / `LOAD` on each |
| C-A-02, C-A-03 | The list with a filter pick-list (`ALL`, `LOCKED`, `UNLOCKED`), progress bars, and a leaderboards page |
| V-03 for Saiph | Answered by `C-04`: a partial match has no `DEX` and no `GUIDE`, and the bar fills with the tabs that did match. No screenshot needed |

What follows from it:

- `C-04` is now a rule rather than an observation, which is what a port needs: the
  core has to expose "did this tab's data match" and the UI fills the bar from that.
- `MAX_BAR_TABS` and the battle taking a slot is the exact behaviour the ports differ
  on today, so it is a small, well-defined change.

## Part 9: Suggested order

The order follows three rules: a feature that reads data the core already has
goes first, a feature that needs new per-game addresses waits for its probes, and
a feature that writes into the running game waits for hardware. It refines
`linux/PLAN.md`'s phase 4 and 5 instead of replacing them.

| Step | Rows | Why here |
| --- | --- | --- |
| 0. Measure | `--stats` per SCREEN mode on the R36S | Everything drawn over the game competes with the emulator. It gates H-32, H-34, H-41 and P-HUD-09, and it is the one open item of phase 3 |
| 1. Free parity | C-02, C-03, C-04, C-07, C-P-03, C-P-07, C-P-08, C-D-03, C-D-11, C-B-04, C-B-05, C-B-06, C-B-07, C-BT-03, C-BT-08, C-G-07, C-G-10, C-SET-01, C-SET-04, H-24, H-27, H-29, H-38 | No new addresses and no new probes: the snapshot and the generated tables already hold the data. One change lands on both ports. This is PLAN phase 4's small half |
| 2. DEX detail and LEARN | C-D-07, C-D-09, C-D-10, C-P-09, C-P-10, C-B-09 (needs D-06) | PLAN phase 4 items 1 and 2. Real new tables, so each one needs a probe and a dump from a retail ROM |
| 3. Host quality of life | H-13 on the 3DS, H-18, H-19, H-20, H-43, H-44, H-45, H-22, H-53, H-16, L-10, L-13 | No game data at all, testable on the desktop, and it removes the "one slot, fixed buttons" wart the READMEs admit to |
| 3b. Offline save editor | SE-01 to SE-11, with L-14's `PC BOXES` entry point | No emulator is running, so it is fully testable on a desktop with copied saves, and it is the one thing a plain emulator cannot do. It writes a battery save, so it needs SE-08's backup first. `Gen3SaveData` is plain byte work, not per-game addresses. **Demoted:** the flag is off in 1.3.2 (V-36), so this is a bet on upstream rather than on parity with today's app |
| 4. The game you actually play | S-17, then S-08 and S-09, then S-07 hack by hack, then S-05 | V-01 and V-02: the reference game is a 32 MB FireRed hack that the app reads partially. Parity for this player means the ports stop refusing over 16 MB and let the probes decide (S-17), which is much cheaper than full best effort. German retail games stay worth doing, but after this |
| 5. Actions that write | C-M-08, C-B-13, C-B-11, C-B-12, then a subset of P-QM | PLAN phase 5 item 2. Each is a per-game patch and a check on hardware; the snapshot alone will not tell you it worked |
| 6. Optional and expensive | P-HUD, C-MK-01 to C-MK-05, C-BT-11 to C-BT-13 with P-BB, X-01 to X-08, C-A-01 and below, H-32 | Touch-first designs, per-frame drawing, a new network dependency (achievements), or a per-frame rewrite (EXPAND). Only what the phone checks (V-24 to V-27) say is worth it |

### Dependencies worth writing down

- `C-BT-09` (`ONE-TAP MOVES`) and `P-BB-02`/`P-BB-03` all sit on `C-BT-11`: the
  game's own battle menu has to be driven first.
- `C-A-02` onwards sit on `C-A-01`; do not build the tab before the client.
- `S-08` (best effort) is the last support step, never the first: it can only be
  safe once every reader rejects a table it does not recognise.
- `C-G-10`, `C-D-09`, `C-D-10` and `C-P-10` share one learnset reader, so they
  should be one change, not four.
- `H-18` and `C-ST-01` are the same work seen from the host and from the
  companion; the host owns the files, the core draws the grid.

### What I would not start now

- `H-32` (`EXPAND`) and `H-26` (`FF MUSIC` STEADY) are projects, not features.
- `C-A-01` (RetroAchievements) doubles the port's dependencies for a feature
  nobody has asked for on a handheld yet; check V-34 first.
- `L-08` (cover art) and the library's grid: the list is three entries long.
- `H-02` / `H-03` (Game Boy and DS): they change what the companion is, not what
  it shows. They belong after the GBA side is complete.

### Keeping the list honest

When a row changes, change its mark and delete the effort note. When a feature
turns out to be out of scope, move it to Part 7 with the reason instead of
leaving it as a missing row. A list that only grows stops being read.
