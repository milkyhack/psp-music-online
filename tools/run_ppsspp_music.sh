#!/bin/bash
# PPSSPP test bench — isolated memstick copy, auto server + auto online at boot.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FAKE="${HOME}/.local/ppsspp-music-home"
GAME="PSPMUSIC"
MS="${FAKE}/.config/ppsspp/PSP/GAME/${GAME}"
BIN="/Applications/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL"
NET_INI="${ROOT}/tools/ppsspp_network.ini"
PORT="${PORT:-8084}"

if command -v psp-config >/dev/null 2>&1 && [[ -f "${ROOT}/psp/Makefile" ]]; then
  make -C "${ROOT}/psp" -s
fi

"${ROOT}/tools/ppsspp_start_server.sh"
"${ROOT}/tools/ppsspp_sync_bench.sh"

# kill only our isolated PPSSPP instance
for pid in $(pgrep -f 'ppsspp-music-home' || true); do kill "$pid" 2>/dev/null || true; done
sleep 0.3

HOST_IP="$(ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1 2>/dev/null || echo 127.0.0.1)"

echo "============================================"
echo " PPSSPP Music test (bench copy only)"
echo " EBOOT:    ${MS}/EBOOT.PBP"
echo " server:   ${HOST_IP} ${PORT} (auto-started if needed)"
echo " boot:     auto Wi-Fi + /api/albums (ppsspp_auto.txt)"
echo " cf log:   ${MS}/data/cf_trace.log"
echo " ppsspp:   /tmp/ppsspp-music.log"
echo ""
echo " Controls: Enter=Cross  Esc=Circle  WASD/arrows"
echo " Logs:     ${ROOT}/tools/read_ppsspp_logs.sh"
echo "============================================"

if [[ ! -x "$BIN" ]]; then
  echo "PPSSPP not found at $BIN" >&2
  echo "Install PPSSPP or set BIN=..." >&2
  exit 1
fi

exec env HOME="$FAKE" "$BIN" --windowed --log=/tmp/ppsspp-music.log \
  --appendconfig="$NET_INI" \
  --appendconfig="${ROOT}/tools/ppsspp_controls.ini" \
  "${MS}/EBOOT.PBP"
