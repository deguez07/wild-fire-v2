#!/usr/bin/env bash
# Builds the project and renames all generated output files to WildFireV2.*
set -euo pipefail

cd "$(dirname "$0")"

NEW_NAME="WildFireV2"
BASE_NAME="pokefirered"   # Makefile output name: poke$(BUILD_NAME)

make -j"$(sysctl -n hw.ncpu)" "$@"

for ext in gba elf map sym; do
    src="${BASE_NAME}.${ext}"
    if [[ -f "$src" ]]; then
        mv -f "$src" "${NEW_NAME}.${ext}"
        echo "Renamed $src -> ${NEW_NAME}.${ext}"
    fi
done
