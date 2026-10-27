#!/bin/bash
# Builds the aarch64 binary and packs the PortMaster release.
#
# The build runs in Debian buster (glibc 2.28): ArkOS4Clone has 2.30, so the
# binary runs there without the image's glibc242 wrapper, and the newest dArkOS
# images are fine too. Docker (or OrbStack) must be running; on Apple Silicon the
# container is native arm64, no emulation.
#
#   linux/arkos/build.sh          -> linux/arkos/PokeDaisy.zip
#   linux/arkos/build.sh --pack-only   repack what build-arm64/pokedaisy holds
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
stage="$here/stage"
zip="$here/PokeDaisy.zip"
pack_only=false
if [ "${1:-}" = "--pack-only" ]; then
  pack_only=true
fi

if [ "$pack_only" = false ]; then
  echo ">> building aarch64 in debian:buster"
  docker run --rm --platform linux/arm64 -v "$root:/src" -w /src debian:buster bash -lc '
    set -e
    # buster is EOL: its packages live in the archive now.
    rm -f /etc/apt/sources.list.d/*
    printf "deb http://archive.debian.org/debian buster main\ndeb http://archive.debian.org/debian-security buster/updates main\n" \
      > /etc/apt/sources.list
    apt-get -o Acquire::Check-Valid-Until=false update -qq
    apt-get install -y -qq build-essential cmake libsdl2-dev >/dev/null
    cmake -S linux -B build-arm64 -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build build-arm64 --parallel "$(nproc)" >/dev/null
    # Symbols stay in (no strip): the crash handler reports an offset that
    # resolves with addr2line against this exact binary.
    ls -l build-arm64/pokedaisy
  '
fi

if [ ! -f "$root/build-arm64/pokedaisy" ]; then
  echo "!! no build-arm64/pokedaisy: the container build failed" >&2
  exit 1
fi
file "$root/build-arm64/pokedaisy" || true

echo ">> packing"
rm -rf "$stage" "$zip"
mkdir -p "$stage/pokedaisy/licenses"
cp "$root/build-arm64/pokedaisy" "$stage/pokedaisy/"
cp "$here/PokeDaisy.sh" "$here/port.json" "$here/gameinfo.xml" "$stage/"
[ -f "$here/screenshot.png" ] && cp "$here/screenshot.png" "$stage/"
cp "$here/README.md" "$stage/"
cp "$root/LICENSE" "$root/NOTICE" "$stage/pokedaisy/licenses/"
{
  echo "PokeDaisy port package"
  echo "built: $(date '+%Y-%m-%d %H:%M')"
  echo "binary: aarch64, glibc 2.28, symbols kept (crash lines resolve with addr2line)"
  echo "install: PokeDaisy.sh and the pokedaisy folder both go into /roms/ports/"
} > "$stage/pokedaisy/VERSION.txt"
chmod +x "$stage/PokeDaisy.sh" "$stage/pokedaisy/pokedaisy"

(cd "$stage" && zip -qr "$zip" .)
rm -rf "$stage"
echo ">> $zip"
