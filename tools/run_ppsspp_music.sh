#!/bin/bash
# Isolated PPSSPP for music — does NOT touch the Bleach/Remake instance.
set -euo pipefail
FAKE="${HOME}/.local/ppsspp-music-home"
BIN="/Applications/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EBOOT_SRC="${ROOT}/psp/EBOOT.PBP"
mkdir -p "$FAKE/.config/ppsspp/PSP/GAME/music" "$FAKE/.config/ppsspp/PSP/SYSTEM"
cp -f "$EBOOT_SRC" "$FAKE/.config/ppsspp/PSP/GAME/music/EBOOT.PBP"
# kill only our instance
for pid in $(pgrep -f 'ppsspp-music-home' || true); do kill "$pid" 2>/dev/null || true; done
sleep 0.3
exec env HOME="$FAKE" "$BIN" --windowed --log=/tmp/ppsspp-music.log \
  "$FAKE/.config/ppsspp/PSP/GAME/music/EBOOT.PBP"
