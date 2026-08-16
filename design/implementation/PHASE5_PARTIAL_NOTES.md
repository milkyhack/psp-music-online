# PHASE 5 — Partial Implementation Notes (AD-cleared themes only)

**Date:** 2026-08-03  
**Gate:** `design/art_direction/PHASE4_AD_REVIEW.md` §7 — selective clearance  
**Scope:** Now Playing + Appearance preview craft for **5 approved** skins only  

---

## Clearance boundaries (honored)

| Status | Themes |
|--------|--------|
| **Implemented (atlas elevated)** | Winamp Classic · Cassette Deck · MiniDisc · GameBoy DMG · CRT TV |
| **Blocked (placeholders left; no atlas “improvement”)** | Walkman, CD, GBC, DOS, Matrix, Cyberpunk, PS2, XMB, Dreamcast, Arcade |

Blocked themes keep prior NP shells so Skip / TCP abort-retry / EQ presets / offline START paths are unchanged.

---

## Theme ID mapping (code `skin` / `SKINS[]` index)

AD docs use 1-based theme numbers; runtime uses **0-based** skin IDs.

| Skin ID | `name` in `theme.c` | AD Theme # | Composition / Viz | Phase 5 status |
|--------:|---------------------|------------|-------------------|----------------|
| **1** | Winamp Classic | 2 | `COMP_WINAMP` / `VIZ_WINAMP_SPEC` | **Elevated** |
| **2** | Cassette Deck | 3 | `COMP_CASSETTE` / `VIZ_CASSETTE_METERS` | **Elevated** |
| **3** | MiniDisc | 4 | `COMP_MINIDISC` / `VIZ_MD_WAVE` | **Elevated** |
| **5** | GameBoy DMG | 6 | `COMP_GAMEBOY` / `VIZ_PIXEL_SPEC` | **Elevated** |
| **10** | CRT TV | 11 | `COMP_CRT` / `VIZ_CRT_SCAN` | **Elevated** |

Other IDs (0, 4, 6–9, 11–14) remain placeholders pending AD re-review / still redos.

---

## What changed

### Files touched
- `psp/src/theme.c` — Phase 2/AD palettes for the 5 approved skins; DMG display name → `GameBoy DMG`
- `psp/src/ui.c` — NP shells, meters, materials helpers, Appearance preview motifs for approved set
- `design/implementation/PHASE5_PARTIAL_NOTES.md` — this note
- Build artifact: `psp/EBOOT.PBP` → Desktop `EBOOT_PSP_Music.PBP`

### Per-theme craft (NP / preview)

1. **Winamp Classic (1)** — Desk void + **inset Main 330×140** (AD Q3); bevel metal panel; green→yellow→red peak spectrum **inside Main**; orange seek thumb; pledit strip; transport in-Main (not full-bleed stretch).
2. **Cassette Deck (2)** — Phase 3 zone map: dual **VU needles** flanking center **well + reels**; brushed Al grain faceplate; rubber transport key row; counter + title under well. Materials from Phase 2 / AD “materials only” still — layout from wireframe, not product-photo copy.
3. **MiniDisc (3)** — Body housing + smoked **disc window** (CLV spin) + teal 3-line LCD + **level blocks**; jog jewel; TOC column subordinate to disc.
4. **GameBoy DMG (5)** — Letterbox soft void; olive shell; thick grey bezel; **all UI inside STN LCD**; pixel-block meters; D-pad / A·B / SEL·STA affordances on plastic (Q4).
5. **CRT TV (10)** — Full cabinet + speaker fabric L/R; thick bezel; smoked glass; **service scope + scanline** meters; OSD title/time lower third; **no rectangle phone scrubber**; front-panel transport + power LED.

### Shared helpers added (procedural, frame-friendly)
- `np_brush_fill` / `np_metal_panel` — brushed / bevel chrome  
- `np_draw_vu_needle` / `np_line` — analog VU  
- `np_speaker_fabric` — CRT flanks  

### Explicitly unchanged (preserve behavior)
- Skip (L/R), TCP abort/retry, EQ audio presets panel, offline START save flow — no edits to `main.c` / `http.c` / `player.c` / `offline.c` for this pass.

---

## Build / deploy

| Step | Result |
|------|--------|
| `bash /home/khakberdinma/build_psp.sh` | **BUILD_OK** (`psp/EBOOT.PBP` ~461044 bytes) |
| Desktop `EBOOT_PSP_Music.PBP` | Copied |
| `d:\PSP\GAME\music\` | **Absent** — skipped |

Compiler notes: only pre-existing unused-function / strncpy warnings; no new errors.

---

## Still blocked — pending AD re-review of revised stills

Do **not** elevate until Concept Art redo / gap stills clear Phase 4 mandatory list:

| Skin ID | Theme | Block reason (AD) |
|--------:|-------|-------------------|
| 0 | Walkman Premium | Wrong form factor (Q1/Q18) |
| 4 | CD Player | Blueprint board (Q14) |
| 6 | GameBoy Color | No still — hard gate |
| 7 | DOS | Soft gate / no still |
| 8 | Matrix | Soft gate / no still |
| 9 | Cyberpunk | Soft gate / no still |
| 11 | PS2 Browser | Soft gate / no still |
| 12 | PSP XMB | Fake firmware (Q5/Q15) |
| 13 | Dreamcast | Swirl not dominant (Q12) |
| 14 | Arcade | IP + paywall flavor (Q16/Q17) |

---

## Success criteria (this pass)

- [x] Only AD-approved themes received atlas elevation  
- [x] Distinct meter language per approved theme  
- [x] Materials depth beyond flat fills (bevel / grain / recessed LCD / housing)  
- [x] Build green; Desktop EBOOT deployed  
- [x] Blocked themes not “improved”; network/playback code paths untouched  
