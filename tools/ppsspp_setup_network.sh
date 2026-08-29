#!/bin/bash
# Write PPSSPP network settings into isolated memstick (both Mac config paths).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FAKE="${HOME}/.local/ppsspp-music-home"
NET_INI="${ROOT}/tools/ppsspp_network.ini"

if [[ ! -f "$NET_INI" ]]; then
  echo "Missing ${NET_INI}" >&2
  exit 1
fi

patch_one() {
  local ini="$1"
  mkdir -p "$(dirname "$ini")"
  if [[ ! -f "$ini" ]]; then
    cp -f "$NET_INI" "$ini"
    return
  fi
  if grep -q '^\[Network\]' "$ini"; then
    if grep -q '^EnableWlan' "$ini"; then
      sed -i '' 's/^EnableWlan = .*/EnableWlan = True/' "$ini"
    else
      sed -i '' '/^\[Network\]/a\
EnableWlan = True
' "$ini"
    fi
  else
    printf '\n' >> "$ini"
    awk '/^\[/ {found=1} found {print}' "$NET_INI" >> "$ini"
  fi
  if grep -q '^\[General\]' "$ini"; then
    if grep -q '^EnablePlugins' "$ini"; then
      sed -i '' 's/^EnablePlugins = .*/EnablePlugins = True/' "$ini"
    else
      sed -i '' '/^\[General\]/a\
EnablePlugins = True
' "$ini"
    fi
  fi
}

patch_one "${FAKE}/.config/ppsspp/PSP/SYSTEM/ppsspp.ini"
patch_one "${FAKE}/Library/Application Support/ppsspp/PSP/SYSTEM/ppsspp.ini"

echo "PPSSPP network: EnableWlan=True, EnablePlugins=True"
echo "Runtime overlay: --appendconfig=${NET_INI}"
