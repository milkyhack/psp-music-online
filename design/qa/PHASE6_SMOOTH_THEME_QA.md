# PHASE 6 — Smooth Theme Redesign QA Checklist

**Date:** 2026-08-08  
**Scope:** PSP client — 15 themes without pixel-art aesthetic; real queue rows; play/pause/stop/skip; download progress; working EQ picker.

## Hard requirements

| # | Check | Pass? |
|---|--------|:-----:|
| 1 | No 1-bit sprite transport / 8×8-only pixel aesthetic on NP | ☐ |
| 2 | Soft panels, gradients, rounded chrome visible on all 15 skins | ☐ |
| 3 | Library / Appearance / messages use active skin tokens | ☐ |
| 4 | Queue rows show **real** titles from current list (not `TRACK01.MP3`) | ☐ |
| 5 | Triangle → queue focus → Up/Down → X plays selected track | ☐ |
| 6 | X play/pause/resume; Square stop; L/R skip | ☐ |
| 7 | START save shows download % / pending / saved states | ☐ |
| 8 | EQ: 5 presets selectable; X applies; audio changes | ☐ |
| 9 | Theme Apply persists after reboot (`data/ui.cfg`) | ☐ |
| 10 | Skip / TCP / offline paths still work | ☐ |

## Per-skin spot (0–14)

Cycle Appearance apply → NP for each skin. Confirm title/time readable, transport state matches playback, queue/EQ overlays usable.

| ID | Skin | Spot |
|---:|------|:----:|
| 0 | Walkman Premium | ☐ |
| 1 | Winamp Classic | ☐ |
| 2 | Cassette Deck | ☐ |
| 3 | MiniDisc | ☐ |
| 4 | CD Player | ☐ |
| 5 | GameBoy DMG | ☐ |
| 6 | GameBoy Color | ☐ |
| 7 | DOS | ☐ |
| 8 | Code Rain | ☐ |
| 9 | Cyberpunk | ☐ |
| 10 | CRT TV | ☐ |
| 11 | PS2 Browser | ☐ |
| 12 | XMB Music | ☐ |
| 13 | Dreamcast | ☐ |
| 14 | Arcade | ☐ |

## Perf notes

- Dreamcast ring count reduced vs Phase 5 multi-ring field  
- Matrix rain columns capped (~14) with soft glyph spacing  
- Alpha fills used sparingly on panels/overlays  

## Files

- `psp/src/ui_gfx.*` — smooth primitives  
- `psp/src/ui_font.*` — soft scaled text + vector icons  
- `psp/src/ui.c` / `ui.h` — compositions + shared screens  
- `psp/src/theme.*` — extended tokens, same 15 IDs  
- `psp/src/jutil.*` — track artist/album/duration  
- `psp/src/main.c` — queue focus, EQ cursor, download fields  
