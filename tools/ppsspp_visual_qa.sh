#!/bin/bash
# Automated PPSSPP visual QA — captures 480x272 BMPs and compares to docs/assets/.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FAKE="${HOME}/.local/ppsspp-music-home"
GAME="PSPMUSIC"
MS="${FAKE}/.config/ppsspp/PSP/GAME/${GAME}"
QA_SRC="${MS}/data/qa"
BIN="/Applications/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL"
NET_INI="${ROOT}/tools/ppsspp_network.ini"
CTRL_INI="${ROOT}/tools/ppsspp_controls.ini"
PORT="${PORT:-8084}"
SCENARIO="${SCENARIO:-full}"
TIMEOUT="${TIMEOUT:-180}"
QA_DEMO="${QA_DEMO:-0}"
STAMP="$(date +%Y%m%d-%H%M%S)"
REPORT_DIR="${ROOT}/design/qa/reports/${STAMP}"

die() { echo "ERROR: $*" >&2; exit 1; }

need_server() {
  curl -sf "http://127.0.0.1:${PORT}/api/status" >/dev/null || die "Server not up on :${PORT}"
  local albums tracks
  albums="$(curl -sf "http://127.0.0.1:${PORT}/api/status" | python3 -c "import sys,json; d=json.load(sys.stdin); print(int(d.get('albums', 0)))" 2>/dev/null || echo 0)"
  tracks="$(curl -sf "http://127.0.0.1:${PORT}/api/status" | python3 -c "import sys,json; d=json.load(sys.stdin); print(int(d.get('tracks', 0)))" 2>/dev/null || echo 0)"
  if [[ "$albums" -lt 1 || "$tracks" -lt 1 ]]; then
    die "Server library empty — scan music in admin http://127.0.0.1:${PORT}/"
  fi
  echo "Server OK: albums=${albums} tracks=${tracks}"
}

if command -v psp-config >/dev/null 2>&1 && [[ -f "${ROOT}/psp/Makefile" ]]; then
  make -C "${ROOT}/psp" -s
fi

"${ROOT}/tools/ppsspp_start_server.sh"
need_server

QA_MODE=1 QA_DEMO="$QA_DEMO" SCENARIO="$SCENARIO" "${ROOT}/tools/ppsspp_sync_bench.sh"

for pid in $(pgrep -f 'ppsspp-music-home' || true); do kill "$pid" 2>/dev/null || true; done
sleep 0.5

if [[ ! -x "$BIN" ]]; then
  die "PPSSPP not found at $BIN"
fi

echo "Launching PPSSPP QA (timeout ${TIMEOUT}s)..."
rm -f "${QA_SRC}/status.log" "${QA_SRC}/"*.bmp 2>/dev/null || true

env HOME="$FAKE" "$BIN" --windowed --xres=480 --yres=272 --scale=1 \
  --log=/tmp/ppsspp-qa.log \
  --appendconfig="$NET_INI" \
  --appendconfig="$CTRL_INI" \
  "${MS}/EBOOT.PBP" &
EMU_PID=$!

deadline=$((SECONDS + TIMEOUT))
while kill -0 "$EMU_PID" 2>/dev/null; do
  if [[ -f "${QA_SRC}/status.log" ]] && grep -q '^done$' "${QA_SRC}/status.log" 2>/dev/null; then
    sleep 1
    kill "$EMU_PID" 2>/dev/null || true
    wait "$EMU_PID" 2>/dev/null || true
    break
  fi
  if (( SECONDS >= deadline )); then
    echo "TIMEOUT — killing PPSSPP" >&2
    kill "$EMU_PID" 2>/dev/null || true
    wait "$EMU_PID" 2>/dev/null || true
    die "QA script did not finish within ${TIMEOUT}s — see ${QA_SRC}/status.log"
  fi
  sleep 0.5
done

mkdir -p "$REPORT_DIR"
if [[ -d "$QA_SRC" ]]; then
  cp -f "${QA_SRC}/"*.bmp "$REPORT_DIR/" 2>/dev/null || true
  cp -f "${QA_SRC}/status.log" "$REPORT_DIR/" 2>/dev/null || true
fi

echo "Captures in ${REPORT_DIR}:"
ls -la "$REPORT_DIR" || true

VENV="${ROOT}/tools/ppsspp_qa/.venv"
if [[ ! -x "${VENV}/bin/python" ]]; then
  python3 -m venv "$VENV"
  "${VENV}/bin/pip" install -q -r "${ROOT}/tools/ppsspp_qa/requirements.txt"
fi

set +e
"${VENV}/bin/python" "${ROOT}/tools/ppsspp_qa/compare_screens.py" "$REPORT_DIR" "$REPORT_DIR"
RC=$?
set -e

echo "Report: ${REPORT_DIR}/report.json"
exit "$RC"
