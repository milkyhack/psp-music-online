# PPSSPP Visual QA

Automated screenshot gate for **PSP Music Online** in PPSSPP. Validates that live emulator output matches [`docs/assets/`](../docs/assets/) README screenshots at native **480×272**.

## Hardware parity (PSP-3008)

PPSSPP is a **dev/QA bench only**. The release EBOOT for a real **PSP-3008** must run **without** bench flag files on `ms0:`.

| Bench file (emu only) | Effect |
|----------------------|--------|
| `data/ppsspp_auto.txt` | Auto Wi‑Fi + API connect |
| `data/ppsspp_qa.txt` | QA script + BMP capture |
| `data/cf_safe.txt` | CPU Cover Flow fallback in emu |

**Never copy these to a real Memory Stick.** See [`release_hw_checklist.md`](../release_hw_checklist.md) for hardware smoke before GitHub Release.

## Quick run

```bash
tools/ppsspp_visual_qa.sh
```

Requirements:

- PPSSPP SDL at `/Applications/PPSSPPSDL.app`
- Music server with scanned library (`http://127.0.0.1:8084/`)
- `pspdev` for EBOOT rebuild (optional if `psp/EBOOT.PBP` exists)

Output: `design/qa/reports/<timestamp>/` — BMP captures, diff PNGs, `report.json`.

## Scenarios

```bash
SCENARIO=full tools/ppsspp_visual_qa.sh          # default — cf_safe on
SCENARIO=coverflow-hw tools/ppsspp_visual_qa.sh  # skin 15, no cf_safe (closer to PSP-3008 3D)
```

## Manual bench (no QA)

```bash
tools/run_ppsspp_music.sh
```

## Gating map (code)

All PPSSPP-specific paths are runtime-gated:

- `net_want_auto_connect()` → `ppsspp_auto.txt`
- `net_want_ppsspp_qa()` → `ppsspp_qa.txt`
- `cf_safe.txt` → CPU Cover Flow in [`ui_coverflow.c`](../../psp/src/ui_coverflow.c)
- `ui_save_screenshot()` → only when `ppsspp_qa.txt` present

Without these files, behaviour matches **PSP-3008 hardware release**.
