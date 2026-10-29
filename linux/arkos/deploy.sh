#!/bin/bash
# Copies a built PokeDaisy to the handheld and, if asked, starts a game on it.
#
# Nothing has to be deleted first: over ssh the binary is copied next to the old
# one, checked, and renamed over it (that works while PokeDaisy is running); on a
# mounted card it is written in place with dd's fsync, because macOS's exfat
# driver is unreliable for rename(). Either way the copy is checked against the
# built binary's SHA-256 before the script says it worked.
#
# Settings, once: copy deploy.conf.example to deploy.conf beside this script and
# put the handheld's address in it (deploy.conf is not tracked):
#
#   cp linux/arkos/deploy.conf.example linux/arkos/deploy.conf
#   $EDITOR linux/arkos/deploy.conf
#
#   linux/arkos/deploy.sh                      copy build-arm64/pokedaisy
#   linux/arkos/deploy.sh --build              build the aarch64 binary first
#   linux/arkos/deploy.sh --rom "Pokemon Emerald.gba" --run
#                                              copy, then play that game and
#                                              print the log when it exits
#   linux/arkos/deploy.sh --run                copy, then open the game list
#   linux/arkos/deploy.sh --full               also copy the Ports-menu files
#   linux/arkos/deploy.sh --local /Volumes/EASYROMS/ports/pokedaisy
#                                              copy to a mounted card instead
#   linux/arkos/deploy.sh --dry-run            print the ssh/scp commands only
#
# Environment (overrides deploy.conf): PD_HOST, PD_DEST, PD_ROMS, PD_LOCAL,
# PD_BINARY, PD_LOG.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)

host=
dest=
roms=
local_dest=
if [ -f "$here/deploy.conf" ]; then
  # shellcheck disable=SC1091
  . "$here/deploy.conf"
fi
host=${PD_HOST:-$host}
dest=${PD_DEST:-${dest:-/roms/ports/pokedaisy}}
roms=${PD_ROMS:-${roms:-/roms/gba}}
local_dest=${PD_LOCAL:-${local:-$local_dest}}
binary=${PD_BINARY:-$root/build-arm64/pokedaisy}
log=${PD_LOG:-/tmp/pd.log}

build=false
full=false
run=false
stop_es=false
keep_es=false
show_log=true
dry=false
rom=

usage() {
  cat <<'EOF'
Copies a built PokeDaisy to the handheld (no deleting first; the copy is
checked against the binary's SHA-256).

  deploy.sh [options]

Options:
  --host USER@HOST   the handheld's ssh address (or PD_HOST / deploy.conf)
  --local DIR        copy to a mounted card instead of over ssh: DIR is the
                     pokedaisy folder (e.g. /Volumes/EASYROMS/ports/pokedaisy);
                     the file keeps its name (pokedaisy)
  --binary FILE      what to copy (default build-arm64/pokedaisy)
  --rom FILE         after copying, play this game (a device path, or a name in
                     the ROMs folder); stops EmulationStation meanwhile
  --run              after copying, open the game list instead of a ROM
  --build            run linux/arkos/build.sh first (aarch64, Debian buster)
  --full             also copy PokeDaisy.sh, port.json, gameinfo.xml and the
                     screenshot to the ports folder (first install, or a menu
                     change)
  --stop-es          stop EmulationStation while copying (a run does this too)
  --keep-es          leave EmulationStation stopped after a run
  --log FILE         the game's log on the device (default /tmp/pd.log)
  --no-tail          don't print the log after a run
  --dry-run          print the ssh and scp commands instead of running them
  -h, --help

Settings: linux/arkos/deploy.conf (copy deploy.conf.example); the environment
variables PD_HOST, PD_DEST, PD_ROMS, PD_LOCAL, PD_BINARY and PD_LOG override it.
EOF
}

die() {
  echo "!! $*" >&2
  exit 1
}

while [ $# -gt 0 ]; do
  case $1 in
  --host)
    [ $# -ge 2 ] || die "--host needs USER@HOST"
    host=$2
    shift 2
    ;;
  --rom)
    [ $# -ge 2 ] || die "--rom needs a file name"
    rom=$2
    run=true
    shift 2
    ;;
  --local)
    [ $# -ge 2 ] || die "--local needs the pokedaisy folder on the card"
    local_dest=$2
    shift 2
    ;;
  --binary)
    [ $# -ge 2 ] || die "--binary needs a file"
    binary=$2
    shift 2
    ;;
  --run)
    run=true
    shift
    ;;
  --build)
    build=true
    shift
    ;;
  --full)
    full=true
    shift
    ;;
  --stop-es)
    stop_es=true
    shift
    ;;
  --keep-es)
    keep_es=true
    shift
    ;;
  --log)
    [ $# -ge 2 ] || die "--log needs a file"
    log=$2
    shift 2
    ;;
  --no-tail)
    show_log=false
    shift
    ;;
  --dry-run)
    dry=true
    shift
    ;;
  -h | --help)
    usage
    exit 0
    ;;
  *)
    die "unknown option: $1 (try --help)"
    ;;
  esac
done

if [ -n "$local_dest" ]; then
  [ "$run" = false ] || die "--run/--rom need --host: a mounted card can't be started from here"
  [ "$stop_es" = false ] || die "--stop-es is for --host"
else
  [ -n "$host" ] || die "no handheld: set host in linux/arkos/deploy.conf (deploy.conf.example), pass --host, or --local DIR for a mounted card"
  case $host in
  *"<"* | *">"*) die "the host still looks like the example ($host): edit linux/arkos/deploy.conf" ;;
  esac
fi

# Shell quoting for the device's busybox/ash.
q() {
  printf '%q' "$1"
}

run_remote() {
  if [ "$dry" = true ]; then
    echo "ssh $host \"$1\""
  else
    ssh -o ConnectTimeout=10 "$host" "$1"
  fi
}

put_as() { # put_as LOCAL DEVICE_PATH
  if [ "$dry" = true ]; then
    echo "scp $1 $host:$2"
  else
    scp "$1" "$host:$2"
  fi
}

# A file's SHA-256, for checking that what landed on the card is what was built.
hash_of() {
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$1" 2>/dev/null | awk '{print $1}'
  elif command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" 2>/dev/null | awk '{print $1}'
  else
    cksum "$1" 2>/dev/null | awk '{print $1 "-" $2}'
  fi
}

stopped_es=false
cleanup() {
  if [ "$dry" = true ]; then
    return 0
  fi
  if [ "$stopped_es" = true ] && [ "$keep_es" = false ]; then
    echo ">> starting EmulationStation again"
    run_remote "systemctl start emulationstation" || echo "!! could not start EmulationStation" >&2
  fi
}
trap cleanup EXIT

if [ "$build" = true ]; then
  "$here/build.sh"
fi

# A relative --binary is taken from the repo root when it isn't here.
case $binary in
/*) ;;
*) [ -f "$binary" ] || binary="$root/$binary" ;;
esac

[ -f "$binary" ] || die "no $binary - build it first (deploy.sh --build)"
if command -v file >/dev/null 2>&1; then
  if ! file "$binary" | grep -q 'ELF 64-bit.*aarch64'; then
    if [ -n "$local_dest" ]; then
      echo "!! $binary is not an aarch64 ELF: copying it anyway" >&2
    else
      die "$binary is not an aarch64 ELF (the desktop build?) - use --build"
    fi
  fi
fi
# Only linux/arkos/build.sh produces the binary --build installs, so the
# "sources are newer" hint is about that one.
if [ "$binary" = "$root/build-arm64/pokedaisy" ]; then
  newer=$(find "$root/3ds/core" "$root/linux" \( -name '*.c' -o -name '*.h' \) -newer "$binary" -print -quit 2>/dev/null || true)
  if [ -n "$newer" ]; then
    echo "!! the sources are newer than $binary (${newer#"$root"/}): consider --build" >&2
  fi
fi

if [ -n "$local_dest" ]; then
  name=$(basename "$binary")
  portdir=$(dirname "$local_dest")
  target="$local_dest/$name"
  want=$(hash_of "$binary")
  echo ">> $target (binary $(du -h "$binary" | cut -f1 | tr -d ' '))"
  if [ "$dry" = true ]; then
    echo "mkdir -p $local_dest"
    [ "$full" = false ] || echo "cp $here/{PokeDaisy.sh,port.json,gameinfo.xml} $portdir/"
    echo "dd if=$binary of=$target bs=1m conv=fsync"
    echo ">> dry run: nothing was copied"
    exit 0
  fi
  mkdir -p "$local_dest"
  if [ "$full" = true ]; then
    echo ">> Ports-menu files to $portdir"
    COPYFILE_DISABLE=1 cp "$here/PokeDaisy.sh" "$here/port.json" "$here/gameinfo.xml" "$portdir/"
    if [ -f "$here/screenshot.png" ]; then
      COPYFILE_DISABLE=1 cp "$here/screenshot.png" "$portdir/"
    fi
    chmod +x "$portdir/PokeDaisy.sh"
  fi
  if [ -f "$target" ] && [ "$(hash_of "$target")" = "$want" ]; then
    echo ">> already up to date ($(printf '%s' "$want" | cut -c1-12)); nothing copied"
  else
    # macOS's exfat driver (fskit) is unreliable for rename() - it fails with
    # ENOENT now and then even to a free name - and reports a freshly copied
    # file late. So write in place through dd's fsync instead, then read it
    # back, and only call it done when the hash matches.
    copied=false
    attempt=1
    while [ "$attempt" -le 3 ]; do
      echo ">> writing $name (attempt $attempt, fsync)"
      if dd if="$binary" of="$target" bs=1m conv=fsync 2>/dev/null; then
        chmod +x "$target" 2>/dev/null || true
        if [ "$(hash_of "$target")" = "$want" ]; then
          copied=true
          break
        fi
      fi
      echo "!! the card didn't take the file (attempt $attempt); trying again" >&2
      attempt=$((attempt + 1))
      sleep 2
    done
    if [ "$copied" != true ]; then
      die "the card doesn't hold the new file after 3 attempts - unplug it and plug it in again, then run this again"
    fi
    echo ">> verified: $(printf '%s' "$want" | cut -c1-12)"
  fi
  ls -l "$target"
  echo ">> done: $target"
  exit 0
fi

portdir=$(dirname "$dest")
echo ">> $host:$dest (binary $(du -h "$binary" | cut -f1 | tr -d ' '))"
run_remote "mkdir -p $(q "$dest")"

if [ "$full" = true ]; then
  echo ">> Ports-menu files to $portdir"
  put_as "$here/PokeDaisy.sh" "$portdir/PokeDaisy.sh"
  put_as "$here/port.json" "$portdir/port.json"
  put_as "$here/gameinfo.xml" "$portdir/gameinfo.xml"
  if [ -f "$here/screenshot.png" ]; then
    put_as "$here/screenshot.png" "$portdir/screenshot.png"
  fi
  run_remote "chmod +x $(q "$portdir/PokeDaisy.sh")"
fi

echo ">> copying the binary (as pokedaisy.new, then renamed over pokedaisy)"
put_as "$binary" "$dest/pokedaisy.new"
run_remote "chmod +x $(q "$dest/pokedaisy.new") && mv -f $(q "$dest/pokedaisy.new") $(q "$dest/pokedaisy") && sync"
run_remote "ls -l $(q "$dest/pokedaisy")"

if [ "$run" = true ] || [ "$stop_es" = true ]; then
  echo ">> stopping EmulationStation"
  run_remote "systemctl stop emulationstation 2>/dev/null || true"
  stopped_es=true
fi

if [ "$run" = true ]; then
  # A name without a slash is taken from the ROMs folder (the one the launcher
  # uses); a full path is used as it is.
  case $rom in
  "") target="--roms $(q "$roms")" ;;
  /*) target=$(q "$rom") ;;
  *) target=$(q "$roms/$rom") ;;
  esac
  echo ">> starting it, log on the device: $log"
  if ! run_remote "cd $(q "$dest") && ./pokedaisy $target --fullscreen --log $(q "$log") < /dev/null"; then
    echo "!! the game exited with an error (or the connection dropped)" >&2
  fi
  if [ "$show_log" = true ]; then
    echo ">> last lines of $log"
    run_remote "tail -n 60 $(q "$log") 2>/dev/null || echo '(no log)'"
  fi
fi

if [ "$dry" = true ]; then
  echo ">> dry run: nothing was copied"
fi
echo ">> done: $host:$dest/pokedaisy"
