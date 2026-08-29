#!/bin/bash
# PPSSPP memstick: network + keyboard + auto online bench flags.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if command -v psp-config >/dev/null 2>&1 && [[ -f "${ROOT}/psp/Makefile" ]]; then
  make -C "${ROOT}/psp" -s
fi

"${ROOT}/tools/ppsspp_start_server.sh"
"${ROOT}/tools/ppsspp_sync_bench.sh"

echo ""
echo "Launch: ${ROOT}/tools/run_ppsspp_music.sh"
