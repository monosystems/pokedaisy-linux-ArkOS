# TODO: what gets built, in which order

The working order for the port. `PORTING.md` says what exists and what is missing;
this file says what to pick up next, why in that order, and how a task counts as
done. Every item names the rows it closes, so the percentage in `README.md` can be
checked with `python3 porting_stats.py` when the item lands.

**Rules**

- One item per change (or per small series). When it lands: flip its rows in
  `PORTING.md`, run `porting_stats.py`, move the README number, all in one commit.
- Done means: a preview shot or the Linux smoke test shows it, the row's note
  is rewritten from what the code does, and no table address is guessed.
- A task that needs a new ROM table does not start before that table has a probe
  (`AGENTS.md`).
- The order below is by value per effort, not by tab. An item may be skipped, but
  then it is dropped from the list instead of moving to the back.

## P0: measure before drawing anything else

Nothing that puts more pixels over the game starts before this is recorded, because
the RK3326 shares its CPU with the emulator.

- [x] **P0.1** Run the R36S from the Ports menu (its launch script already passes
      `--stats` and writes `log.txt` into the port folder) and record frames per
      second and the emulator/drawing split for `fit`, `integer` and `stretch`,
      with the companion closed and open (`linux/PLAN.md` phase 3 has the table;
      the mode is `screen=` in `pokedaisy.ini` and the log names it). Gates P5.1,
      P5.2 and P5.6.
- [x] **P0.2** Save state and save backup on the exFAT card (`.sav`/`.srm`,
      `.backup-*.sav`, `pokedaisy-states/*.ss1` and `.resume`), then the PortMaster
      install from the zip's `autoinstall` folder rather than a copied binary.
      Closes the rest of phase 3.

## P1: free parity, no new addresses

All of this reads what `3ds/core` already decodes, so it lands in one change and
carries no per-game risk. About 29 rows, roughly +13 points.

- [x] **P1.1 Tab bar rules.** A battle puts `BATTLE` first and the bar holds one
      chip fewer; `MORE` only when something is hidden; no `MORE` chip when
      nothing is. Hidden tabs remain in `MORE` rather than filling empty places,
      a deliberate difference from the app. (C-02, C-04)
      *Done when the preview shows the same five chips as the app, and a battle
      shot shows the sixth at the front with the last one gone.*
- [x] **P1.2 Settings for the bar and the tweaks.** A `TAB BAR` page (shown/hidden
      per tab) and the four `TWEAKS` switches. (C-03, C-07) — needs P1.1's tab list.
      *Done: the six pickable tabs (`PARTY` to `CARD`) say `SHOWN`/`HIDDEN`, the
      title counts `n/5`, and a full bar gives the app's notice; the four switches
      each drive their own animation or habit. The preview has them as
      `-settings-tabs*`, `-settings-tweaks*`, `-tab-slide-*` and `-battle-nojump`,
      and `PORTING.md` takes the icon bounce as C-P-04's half. Hiding every tab
      leaves `MORE`, fast-forward and settings reachable, without filling the
      empty places back in.*
- [x] **P1.3 Title screen.** `PARTY`, `BAG`, `MAP` and `CARD` show the game's empty
      slots with a blinking `PRESS START` instead of zeros, until the field is
      readable. (C-05)
      *Done: the wait ends at a battle, five map positions or a resume the host
      loaded (less the app's field-callback check). A party on its own no longer
      ends the wait because its data can already be present on the title screen. The
      preview has it as `-waiting-*`; the smoke test checks both ends: a cold
      boot waits, a resumed one shows.*
- [x] **P1.4 Held item tag.** A thin bar with the item's icon and name on a party
      slot. (C-P-03)
      *Done: `held_item_tag` in `pd_ui_party.c` draws the game's own marker
      (`sHeldItemGfx`, found in both games' ROMs) and the item's name on a white
      bar under the HP numbers; a statused slot drops the marker so the name
      still fits. The preview's party shots show it (`firered-party.png`: KADABRA
      holds QUICK CLAW, JOLTEON EXP. SHARE; `emerald-party.png`: CLEANSE TAG).*
- [x] **P1.5 Summary: EXP and matchups.** The `EXP` bar with `NEXT` / `MAX`, and the
      `WEAK TO` / `RESISTS` / `IMMUNE TO` blocks with a multiplier per type. (C-P-07,
      C-P-08)
      *Done: the summary's info block carries the `EXP` bar (`ui_exp_bar`, the
      species' growth curve from `ExpTables.kt` through `gen_tables.py`), and the
      matchups are the summary's third page (`summary_matchups`): type chips with
      `2x` / `1/2x` / `0x`, the strongest weakness first. `firered-summary` and
      `firered-summary-matchups` show it (KADABRA weak to Bug, Ghost and Dark;
      CHARIZARD's Rock 4x).*
- [x] **P1.6 Summary: STATS.** `STAT` / `IV` / `EV` columns with `TOTAL`, the nature's
      red up and blue down arrow, and `HIDDEN POWER` with its type and power. (C-P-09)
      *Done: the summary's second page (`summary_stats`): `STAT` / IV bar and number
      / `EV` in two columns of three, `TOTAL`, `NATURE` with the arrows on the stats
      it moves, `HIDDEN POWER` with its type chip and `PWR`. The preview shoots it as
      `-summary-stats`; the IVs and EVs were checked by recomputing KADABRA's,
      CHARIZARD's and MANECTRIC's stats from their base stats.*
- [x] **P1.7 Bag: `ALL`, order, TM names.** The `ALL` pocket, the order button
      (`DEFAULT`, `NAME`, `TYPE`, `COUNT`) and the move after a TM's number.
      (C-B-04, C-B-05, C-B-07)
      *Done: `pd_ui_bag.c` builds the list from the tab and the order
      (`build_rows`, `compare_rows`), `ALL` mixes every pocket in the game's
      order, and `pd_dex_tm_move` (a probed `gTMHMMoves` read) puts the move
      after a machine's number (`TM05 ROAR`). The preview shoots it as
      `-bag-order` and `-bag-pocket`.*
- [x] **P1.8 Bag: favorites.** The star in the description bar, the gold star in the
      list and the `FAVORITES` pocket that comes and goes with it, kept per ROM.
      (C-B-06)
      *Done: the description box's star toggles the item, the list marks it gold
      and a `FAVORITES` tab comes and goes with it; the sets live in the settings
      file as one `fav2_1`..`fav2_4` line per ROM (a stable ROM-content
      fingerprint and items), 32 items each. Old `fav1`..`fav4` lines are kept
      but cannot be assigned to a game safely and are not shown automatically.
      On Linux, right from the selected item reaches its star and left returns;
      selecting a different item updates the description and star. The preview
      shoots it as `-bag-star`.*
- [x] **P1.9 Bag: the game's own look.** Yellow pockets with `PC` and the order
      button under them, instead of one pocket in a white list. (C-B-01) — the visual
      half of the tab, worth doing before more bag features land on it.
      *Done: the pockets run down the left in the game's own order with the open
      one white/red and the game's underline, `PC` and the order button pinned
      under them, the list beside and the description box under it; Emerald keeps
      its navy/gold colours and striped backdrop. The Linux cursor follows item
      rows on up/down and scrolls the list; left returns to the pockets. Narrower than the app: no item
      icons and no ROM bag tiles, and `PC` says it isn't here yet.*
- [x] **P1.10 Dex: filters and arrows.** The filter list with a count per filter and
      the previous and next arrows with the counter. (C-D-03, C-D-11)
      *Done: a chip row under the POKéDEX title (`filter_row` in `pd_ui_dex.c`)
      with `ALL`, `CAUGHT`, `SEEN ONLY` and `NOT SEEN` and each one's count, the
      list through the filter, and the entry's place beside the stepper
      (`6 / 386`). The preview shoots the filtered list as `-dex-filtered` and the
      counter in `-dex-entry` / `-dex-next`. On Linux the cursor follows the list
      row by row across its scroll, so every entry stays reachable up and down.*
- [x] **P1.11 Battle: the foe's side.** `WEAK TO` / `RESISTS`, the per-side threat
      tag, and `BATTLE HINTS` as a switch. (C-BT-03, C-BT-08)
      *Done: the INFO pane lists the foe's matchups as chips under its card and
      both cards carry the other side's threat pill (`ui_matchup_block`,
      `ui_matchups` in `pd_ui_widgets.c`); the pane scrolls when they push it past
      its window. `BATTLE HINTS` (TWEAKS, until SETTINGS gets the app's groups)
      drops the verdicts, the best pick's verdict and the matchups. The preview
      shoots `-battle-scrolled` and `-battle-nohints`.*
- [x] **P1.12 Guide: more of the app.** The `EVOLUTIONS` page, and the bosses after
      the next one as headers. (C-G-10, C-G-07)
      *Done: the guide's last page is `EVOLUTIONS` (`build_evolutions`), the party
      first and then every kind with a step that isn't a plain level-up, read
      through `pd_dex_evolutions`. The `BOSS` page already opened on the next
      unbeaten one and listed the following ones as sections - PORTING's row now
      says so. The preview shoots the page as `-guide-p5`; the 6th chip needed the
      chip row to size itself by its labels, and the preview's TAB BAR step now
      puts the bar back (it had been hiding GUIDE, which had left the guide's
      shots stale since P1.2).*
- [x] **P1.13 Settings as the app has it.** Groups and pages instead of one column,
      plus `RESTART GAME` and the resume wording on `LEAVE GAME`. (C-SET-01,
      C-SET-04)
      *Done: the home page keeps `SCREEN` and `FAST-FORWARD` and lists the groups
      `COMPANION` (TAB BAR, TWEAKS), `BATTLE` (BATTLE HINTS, moved there from
      TWEAKS) and `SAVES` (the states, `RESTART GAME`, `LEAVE GAME`), each a page
      with its summary on the row. `RESTART GAME` asks and then reboots the game
      (`PD_ACTION_RESTART_GAME`, the Linux host resets the core and drops the
      stale resume); `LEAVE GAME` says what the port keeps. The preview shoots
      `-settings-companion`, `-settings-battle`, `-settings-saves` and
      `-confirm-restart`.*
- [x] **P1.14 Speed rows.** `FF` as a toggle on Linux too, speeds from 1.5x to 10x
      plus `INFINITE`, `GAME SPEED`, and slow motion while held. (H-23, H-24, H-27,
      H-29)
      *Done: SETTINGS > SPEED holds `FAST-FORWARD` (1.5x to 10x and `INFINITE`,
      `PD_FF_SPEEDS`), `GAME SPEED` (25% to 400%) and `SLOW MOTION (L1)`. The
      Linux host runs all three from one percent accumulator, so 1.5x is really
      1.5x; it paces one frame time per present, so a fast-forward step raises
      the emulated frames and not the delay. `INFINITE` runs in 30 ms slices and
      every step is capped at about two frame times, so a slow device keeps
      presenting the picture instead of freezing it; `--ff` takes the percent and
      the 3DS host adapts the same table. The settings file keeps GAME SPEED and
      SLOW MOTION too; the preview shoots the speed page and both pick lists.*
- [x] **P1.15 Status bar.** Game, place, money, clock and battery over the game.
      (H-38)
      *Done: SETTINGS > COMPANION > STATUS BAR switches on the app's one-row bar
      over the game, in the OPTION title window: a short game label (FireRed,
      LeafGreen, Emerald, not the file name), map pin and place, Pokédollar and
      money, local time, and the battery with its charge. The place gets the room
      first and the name is dropped when the row is narrow. It refreshes once a
      second, stays off the companion, and leaves the battery out when the host
      has none to report.*
- [x] **P1.16 Library: a game's page.** `INFO` with the ROM, save and states, plus
      `RESTORE BACKUP` and a `PLAYING` marker. (L-10, L-13, H-16)
      *Done: SELECT opens file information; A lists backups and asks before
      restoring one after protecting the current save. SETTINGS > SAVES > LIBRARY
      opens the paused library, marking the running game and saves with text. The
      smoke test draws the INFO, backup, confirm and playing views. Achievements
      on INFO and graphical save icons remain partial in PORTING.*

## P2: host quality of life, no game data

Host side, testable on a desktop. About 14 rows, roughly +6 points.

- [ ] **P2.1 Ten state slots.** Thumbnail, date and `SAVE` / `LOAD` per slot, plus
      `UNDO SAVE` and `UNDO LOAD`; host side, the core draws the grid.
      (H-18, H-19, C-ST-01, C-ST-02, C-ST-03)
- [ ] **P2.2 Slot hotkeys and the current slot.** (H-20, C-ST-04) — after P2.1.
- [ ] **P2.3 Button and hotkey remapping.** A `GAME BUTTONS` page, a `HOTKEYS` page
      with one off switch, and `TURBO A` / `TURBO B`. (H-43, H-44, H-45)
- [ ] **P2.4 Small host fixes.** A backup only when the save
      changed, a screenshot hotkey. (H-12, H-53)

## P3: the game you actually play

The reference phone plays a 32 MB FireRed hack that the app reads partially, and the
ports refuse it outright. That makes this the most valuable support work, ahead of
German retail games.

- [ ] **P3.1 Partial support for FireRed-layout hacks.** Stop refusing at 16 MB, load
      the rev 0 / rev 1 configs and let each table's probe decide tab by tab, then
      say `PARTIALLY SUPPORTED` in the app's words. Needs a "did this probe pass"
      flag per tab in the core. (S-17)
      *Done when a desktop run on the reference hack shows party, bag and map, hides
      the tabs whose probes failed, and reads no table it did not check.*
- [ ] **P3.2 Item text from the ROM.** Read `gItems` in `pd_item_description` and drop
      the frozen `pd_item_desc.c`. (S-12) — fixes hacks and every language.
- [ ] **P3.3 Best effort matching.** Try every known game, keep the fit, remember it
      per ROM, with `PICK A GAME` / `FORGET MATCH`. (S-08, S-09) — only after P3.1
      proves the probes reject a moved table.
- [ ] **P3.4 German Emerald and FireRed.** The first non-English pair: charset, name,
      item and move tables, addresses per language, probes. Generator work first
      (D-07). (S-05)
- [ ] **P3.5 Ruby and Sapphire.** The third engine family, English first. (S-06)
- [ ] **P3.6 Hacks one by one**, each with its own addresses and probe, in the order
      the reference phone uses them. (S-07)
- [ ] **P3.7 The guide's pages in other languages.** Follows P3.4. (S-14)

## P4: actions that write into the game

Each one is a per-game patch or script call with its own check and a hardware test.
About 14 rows, roughly +6 points.

- [ ] **P4.1 `FLY` from the map.** (C-M-08)
- [ ] **P4.2 `PC` from the bag.** (C-B-13)
- [ ] **P4.3 `USE` and `GIVE`**, then `TAKE` in the summary. (C-B-11, C-B-12, C-P-12)
- [ ] **P4.4 The quick menu**, starting with the four tiles that already exist as
      buttons (`SAVE STATE`, `LOAD STATE`, `FF`, `COMPANION`), then `PC`, `FLY`, the
      keep-away item, `BIKE`, `ROD` and `WILD`. (P-QM-01 to P-QM-09)

## P5: optional, expensive, or both

Only what P0 says fits, or what is explicitly asked for.

- [ ] **P5.1 HUD**, one layout (`CORNERS`) and all parts, without the narrower
      fallbacks. (P-HUD-01 to P-HUD-09)
- [ ] **P5.2 Battle buttons**, or the pad-button version of them on the R36S.
      (C-BT-11, C-BT-12, C-BT-13, P-BB-01 to P-BB-04)
- [ ] **P5.3 Cheats**, through mGBA's own cheat device. (X-01 to X-08)
- [ ] **P5.4 RetroAchievements**, a new dependency and a network client. (C-A-01 and
      below)
- [ ] **P5.5 Makeover**: day and night first, then follower and player look.
      (C-MK-05, C-MK-04, C-MK-01 to C-MK-03)
- [ ] **P5.6 Screen extras**: `EXPAND`, shaders, overlays, rewind, `FF MUSIC`
      STEADY. (H-32, H-33, H-34, H-35, H-36, H-37, H-21, H-26)
- [ ] **P5.7 Library extras**: covers and a grid, zipped ROMs. (L-02, L-08, H-07)

## Not on the list

- The rows `PORTING.md` marks `n/a`: two-screen layouts, phone portrait docking,
  casting, touch controls, cover art services, updates. Part 7 there has the reasons.
- Game Boy, Game Boy Color and DS support (H-02, H-03, S-10, S-11): they change what
  the companion is, so they wait until the GBA side is complete.
- The save editor (SE-01 to SE-11): upstream code that 1.3.2 does not ship.

## Where the numbers come from

The "+13 points" and the like are the rows an item closes, divided by the 221
applicable features. They are a guide, not a promise: `porting_stats.py` is the
authority, and it is what the README number is copied from.
