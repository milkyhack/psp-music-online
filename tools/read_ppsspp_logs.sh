#!/bin/bash
# Read Cover Flow / PPSSPP debug output (local memstick + server + PPSSPP log).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FAKE="${HOME}/.local/ppsspp-music-home"
GAME="PSPMUSIC"
MS="${FAKE}/.config/ppsspp/PSP/GAME/${GAME}"
PORT="${PORT:-8084}"
HOST="${HOST:-127.0.0.1}"

echo "=== server.cfg (PPSSPP memstick) ==="
for f in "${MS}/data/server.cfg" "${MS}/server.cfg"; do
  if [[ -f "$f" ]]; then
    echo "# $f"
    cat "$f"
  fi
done
echo

echo "=== cf_trace.log (PPSSPP memstick) ==="
CF="${MS}/data/cf_trace.log"
if [[ -f "$CF" ]]; then
  tail -n 40 "$CF"
else
  echo "(нет файла — запусти tools/run_ppsspp_music.sh и зайди в Cover Flow)"
  # legacy path from older script
  LEG="${FAKE}/.config/ppsspp/PSP/GAME/music/data/cf_trace.log"
  if [[ -f "$LEG" ]]; then
    echo "--- legacy music/data/cf_trace.log ---"
    tail -n 40 "$LEG"
  fi
fi
echo

echo "=== PPSSPP log (tail) ==="
if [[ -f /tmp/ppsspp-music.log ]]; then
  tail -n 30 /tmp/ppsspp-music.log
else
  echo "(нет /tmp/ppsspp-music.log)"
fi
echo

echo "=== server client_logs (network) ==="
if curl -sf "http://${HOST}:${PORT}/api/status" >/dev/null 2>&1; then
  curl -sf "http://${HOST}:${PORT}/api/client/logs?limit=5" | python3 -m json.tool 2>/dev/null || \
    curl -sf "http://${HOST}:${PORT}/api/client/logs?limit=5"
  echo
  IDX="${ROOT}/server/data/client_logs/index.jsonl"
  if [[ -f "$IDX" ]]; then
    echo "--- last index entries ---"
    tail -n 5 "$IDX"
  fi
else
  echo "(сервер не отвечает на http://${HOST}:${PORT} — запусти: cd server && .venv/bin/python -m app)"
fi
