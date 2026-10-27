#!/bin/bash
# PokeDaisy for ArkOS4Clone / ArkOS - launch script (PortMaster layout, but it
# also runs when the folder was unzipped onto the card by hand and PortMaster
# isn't installed).
#
# Started from the Ports menu it lists the .gba games in /roms/gba; given a ROM
# (a launcher that passes %ROM%) it plays that one. No gptokeyb: the port reads
# the pad through SDL itself, so a pad-to-keyboard mapping would double every
# press. START+SELECT held for a second and a half leaves the game.

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
controlfolder=""
for candidate in "/opt/system/Tools/PortMaster" "/opt/tools/PortMaster" "$XDG_DATA_HOME/PortMaster" \
                 "/roms/tools/PortMaster" "/roms2/tools/PortMaster" "/roms/ports/PortMaster" "/roms2/ports/PortMaster"; do
  if [ -f "$candidate/control.txt" ]; then
    controlfolder="$candidate"
    break
  fi
done

if [ -n "$controlfolder" ]; then
  source "$controlfolder/control.txt"
  if [ -f "${controlfolder}/mod_${CFW_NAME}.txt" ]; then
    source "${controlfolder}/mod_${CFW_NAME}.txt"
  fi
  get_controls
  GAMEDIR="/$directory/ports/pokedaisy"
else
  # No PortMaster: this script sits in the ports folder, next to the port's own
  # directory (that is what unpacking the zip there gives). Older hand-made
  # installs put the script inside the port's folder instead.
  script_dir=$(cd "$(dirname "$0")" && pwd)
  if [ -f "$script_dir/pokedaisy/pokedaisy" ]; then
    GAMEDIR="$script_dir/pokedaisy"
  else
    GAMEDIR="$script_dir"
  fi
fi

if [ ! -d "$GAMEDIR" ]; then
  echo "PokeDaisy: the port folder ($GAMEDIR) is missing" >&2
  exit 1
fi
cd "$GAMEDIR" || exit 1

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1
echo "PokeDaisy: in $GAMEDIR"

export SDL_GAMECONTROLLERCONFIG="${sdl_controllerconfig:-}"

# Software renderer: some units' GPU driver (Mali over SDL/GLES) dies after a
# while. Create an empty file named "software" in this folder to switch to the
# software renderer for a session; delete it to go back:
#   touch "/roms/ports/pokedaisy/software"
if [ -f "$GAMEDIR/software" ]; then
  export POKEDAISY_RENDERER=software
  echo "PokeDaisy: software renderer (found $GAMEDIR/software)"
fi

if command -v pm_platform_helper >/dev/null 2>&1; then
  pm_platform_helper "$GAMEDIR/pokedaisy"
fi

if [ -n "$1" ] && [ -f "$1" ]; then
  "$GAMEDIR/pokedaisy" "$1" --fullscreen --stats --settings "$GAMEDIR/pokedaisy.ini"
else
  "$GAMEDIR/pokedaisy" --roms /roms/gba --fullscreen --stats --settings "$GAMEDIR/pokedaisy.ini"
fi

if command -v pm_finish >/dev/null 2>&1; then
  pm_finish
fi
