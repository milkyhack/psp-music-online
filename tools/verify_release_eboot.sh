#!/bin/bash
# Verify release tree does not ship PPSSPP bench flags.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FAIL=0

check_absent() {
  local label="$1"
  shift
  local f
  for f in "$@"; do
    if [[ -f "$f" ]]; then
      echo "FAIL: $label should not exist: $f"
      FAIL=1
    fi
  done
}

check_absent "bench flag in repo tools" \
  "${ROOT}/psp/data/ppsspp_auto.txt" \
  "${ROOT}/psp/data/ppsspp_qa.txt" \
  "${ROOT}/psp/data/cf_safe.txt"

if [[ -f "${ROOT}/psp/EBOOT.PBP" ]]; then
  # Bench flags must live only under tools/ppsspp_bench — not inside EBOOT payload.
  if strings "${ROOT}/psp/EBOOT.PBP" 2>/dev/null | grep -q 'ppsspp_qa.txt'; then
    echo "WARN: EBOOT strings mention ppsspp_qa.txt (expected for net_want_ppsspp_qa path string)"
  fi
fi

echo "Bench flags source of truth: tools/ppsspp_bench/data/ (deploy via ppsspp_sync_bench.sh only)"
echo "Release deploy: copy EBOOT.PBP + data/server.cfg to ms0: — no ppsspp_*.txt"

if [[ "$FAIL" -eq 0 ]]; then
  echo "verify_release_eboot: PASS"
else
  echo "verify_release_eboot: FAIL"
  exit 1
fi
