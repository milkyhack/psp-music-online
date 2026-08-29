#!/bin/bash
# Launch Cover Flow with 3D GU path (no cf_safe.txt) — closer to PSP-3008.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export SCENARIO=coverflow-hw
export QA_MODE=0
export TIMEOUT=120

"${ROOT}/tools/ppsspp_start_server.sh"
QA_MODE=0 SCENARIO=coverflow-hw "${ROOT}/tools/ppsspp_sync_bench.sh"

FAKE="${HOME}/.local/ppsspp-music-home"
MS="${FAKE}/.config/ppsspp/PSP/GAME/PSPMUSIC"
BIN="/Applications/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL"
NET_INI="${ROOT}/tools/ppsspp_network.ini"
CTRL_INI="${ROOT}/tools/ppsspp_controls.ini"

for pid in $(pgrep -f 'ppsspp-music-home' || true); do kill "$pid" 2>/dev/null || true; done
sleep 0.5

echo "Cover Flow 3D bench — skin 15, cf_safe OFF"
echo "  Open Online Library → Albums, scroll with L/R"
echo "  CF trace: ${MS}/data/cf_trace.jsonl (if present)"

env HOME="$FAKE" "$BIN" --windowed --xres=480 --yres=272 --scale=1 \
  --appendconfig="$NET_INI" \
  --appendconfig="$CTRL_INI" \
  "${MS}/EBOOT.PBP"
