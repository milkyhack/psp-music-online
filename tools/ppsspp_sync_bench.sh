#!/bin/bash
# Sync PPSSPP bench memstick copy — isolated from real PSP paths.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FAKE="${HOME}/.local/ppsspp-music-home"
GAME="PSPMUSIC"
MS="${FAKE}/.config/ppsspp/PSP/GAME/${GAME}"
BENCH="${ROOT}/tools/ppsspp_bench"
EBOOT_SRC="${EBOOT_SRC:-${ROOT}/psp/EBOOT.PBP}"
CTRL_INI="${ROOT}/tools/ppsspp_controls.ini"
PORT="${PORT:-8084}"
SCENARIO="${SCENARIO:-default}"
QA_MODE="${QA_MODE:-0}"
QA_DEMO="${QA_DEMO:-0}"

lan_ip() {
  ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1 2>/dev/null || echo "127.0.0.1"
}

HOST_IP="$(lan_ip)"
PPSSPP_HOST="${PPSSPP_HOST:-127.0.0.1}"

"${ROOT}/tools/ppsspp_setup_network.sh"

mkdir -p "${MS}/data" "${MS}/data/qa"
rm -f "${MS}/data/qa/"*.bmp "${MS}/data/qa/status.log" 2>/dev/null || true
cp -f "$EBOOT_SRC" "${MS}/EBOOT.PBP"
printf '%s %s\n' "$PPSSPP_HOST" "$PORT" > "${MS}/data/server.cfg"
printf '%s %s\n' "$PPSSPP_HOST" "$PORT" > "${MS}/server.cfg"

if [[ -d "${BENCH}/data" ]]; then
  cp -f "${BENCH}/data/"* "${MS}/data/" 2>/dev/null || true
fi

case "$SCENARIO" in
  coverflow-hw)
    rm -f "${MS}/data/cf_safe.txt"
    if [[ -f "${BENCH}/scenarios/coverflow-hw/ui.cfg" ]]; then
      cp -f "${BENCH}/scenarios/coverflow-hw/ui.cfg" "${MS}/data/ui.cfg"
    fi
    ;;
  five-track-hw)
    rm -f "${MS}/data/cf_safe.txt"
    if [[ -f "${BENCH}/scenarios/five-track-hw/ui.cfg" ]]; then
      cp -f "${BENCH}/scenarios/five-track-hw/ui.cfg" "${MS}/data/ui.cfg"
    fi
    ;;
  coverflow-quick-hw)
    rm -f "${MS}/data/cf_safe.txt"
    if [[ -f "${BENCH}/scenarios/coverflow-quick-hw/ui.cfg" ]]; then
      cp -f "${BENCH}/scenarios/coverflow-quick-hw/ui.cfg" "${MS}/data/ui.cfg"
    fi
    ;;
  controls-smoke)
    if [[ -f "${BENCH}/scenarios/controls-smoke/ui.cfg" ]]; then
      cp -f "${BENCH}/scenarios/controls-smoke/ui.cfg" "${MS}/data/ui.cfg"
    fi
    ;;
  default|full|*)
    ;;
esac

if [[ "$QA_MODE" == "1" ]]; then
  if [[ -f "${BENCH}/data/ppsspp_qa.txt" ]]; then
    cp -f "${BENCH}/data/ppsspp_qa.txt" "${MS}/data/ppsspp_qa.txt"
  fi
  if [[ -f "${BENCH}/scenarios/${SCENARIO}/ppsspp_qa.txt" ]]; then
    cp -f "${BENCH}/scenarios/${SCENARIO}/ppsspp_qa.txt" "${MS}/data/ppsspp_qa.txt"
  fi
  if [[ "$QA_DEMO" == "1" && -f "${BENCH}/data/ppsspp_qa_demo.txt" ]]; then
    cp -f "${BENCH}/data/ppsspp_qa_demo.txt" "${MS}/data/ppsspp_qa_demo.txt"
  else
    rm -f "${MS}/data/ppsspp_qa_demo.txt"
  fi
else
  rm -f "${MS}/data/ppsspp_qa.txt"
  rm -f "${MS}/data/ppsspp_qa_demo.txt"
fi

for base in "${FAKE}/.config/ppsspp" "${FAKE}/Library/Application Support/ppsspp"; do
  mkdir -p "${base}/PSP/SYSTEM"
  cp -f "$CTRL_INI" "${base}/PSP/SYSTEM/controls.ini"
done

echo "PPSSPP bench memstick:"
echo "  EBOOT:      ${MS}/EBOOT.PBP"
echo "  server.cfg: ${PPSSPP_HOST} ${PORT} (bench loopback; LAN=${HOST_IP})"
echo "  scenario:   ${SCENARIO}"
echo "  qa mode:    ${QA_MODE}"
echo "  auto flags: ${MS}/data/ppsspp_auto.txt (emulator only — never on PSP-3008)"
if [[ -f "${MS}/data/cf_safe.txt" ]]; then
  echo "  cf_safe:    ${MS}/data/cf_safe.txt (CPU Cover Flow in emu)"
else
  echo "  cf_safe:    (off — hardware Cover Flow path)"
fi
if [[ -f "${MS}/data/ppsspp_qa.txt" ]]; then
  echo "  qa script:  ${MS}/data/ppsspp_qa.txt"
fi
if [[ -f "${MS}/data/ppsspp_qa_demo.txt" ]]; then
  echo "  qa demo:    ${MS}/data/ppsspp_qa_demo.txt"
fi
