# PokeDaisy for ArkOS4Clone (PortMaster port)

PokeDaisy, ported to Linux: the game runs on mGBA in-process and PokeDaisy's
companion is drawn over it. Party, bag, region map, guide, Pokedex, battle
matchups and the trainer card, all read from the game as you play, in the game's
own menu style.

Based on **PokeDaisy by Lidor Itzhari** - https://github.com/lidor30/pokedaisy
(GPLv3, plus the credit term in the `NOTICE`). This port lives in the same repo
under `linux/`; the companion is the portable C core the 3DS port uses
(`3ds/core`, unchanged).

## Install

1. Install the port with PortMaster: put `PokeDaisy.zip` into PortMaster's
   autoinstall folder (`/roms/tools/PortMaster/autoinstall/` on ArkOS) and start
   PortMaster, or unzip `PokeDaisy.zip` into `/roms/ports/`.
2. Put your own legally dumped `.gba` games in `/roms/gba`.
3. Start PokeDaisy from the Ports menu and pick a game.

No games, BIOS or game art are included: the art (region map, trainer card) is
rebuilt from your own ROM.

## Controls

| Button | Does |
|---|---|
| A / B, L1 / R1, START / SELECT, D-pad | the GBA's buttons (the left stick moves too) |
| R2, held | fast-forward |
| L2 | open and close the companion |
| In the companion: D-pad / A / B | move the cursor / press / back |
| In the companion: L1 / R1 | previous / next tab |
| START+SELECT, held | leave the game, back to EmulationStation |

## What the companion reads

FireRed, LeafGreen and Emerald, English, so far. Any other GBA game plays, and
the overlay says it has no companion for it instead of guessing. Saves are the
usual `.srm`/`.sav` beside the ROM (RetroArch's included), backed up to
`<game>.backup-<date>.sav` before every start; one save state per game lives in
`pokedaisy-states/` beside the ROM.

## Notes

- **No gptokeyb.** The port reads the pad through SDL itself, so a
  pad-to-keyboard mapping would make every press happen twice. If a device's pad
  ever needs gptokeyb, give it an empty mapping file.
- **Logs:** `log.txt` in the port folder (it also gets the frame count).
- **Glad it runs?** The credit line above is required; everything else is shared
  with upstream.

Thanks to Lidor Itzhari for PokeDaisy and to the mGBA, melonDS and rcheevos
projects it stands on.

## Build

`linux/arkos/build.sh` builds the aarch64 binary in a Debian buster container
(glibc 2.28, so it runs on ArkOS4Clone's 2.30 without the `glibc242` wrapper) and
writes `PokeDaisy.zip` next to this file. See `linux/README.md` for the full
build, test and development notes.
