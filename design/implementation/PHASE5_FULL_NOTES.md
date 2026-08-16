# PHASE 5 — Full Atlas Implementation Notes

**Date:** 2026-08-03  
**Gate:** `design/art_direction/PHASE4_AD_REVIEW.md` RE-REVIEW — **PASS WITH NOTES** · Blocked: **none**  
**Scope:** Now Playing + Appearance preview craft for **all 15** skins  

> **QA + AD stamp (2026-08-03):** `design/qa/PHASE5_QA_AD_REVIEW.md` — Implementation **PASS WITH NOTES** · Exhibition clearance **WITH NOTES**. Hard AD locks clear in code. Surgical fix: GBC/DMG LCD UI stack inside glass (Q4) in `ui.c` — **rebuild EBOOT** before device demo. Open: device regression §4. Desktop EBOOT synced; `d:\PSP\GAME\music\` absent.

Supersedes selective clearance in `PHASE5_PARTIAL_NOTES.md` (shipped five remain; ten elevated; CRT polished).

---

## Theme ID mapping (code `skin` / `SKINS[]` index)

AD docs use **1-based** theme numbers; runtime uses **0-based** skin IDs.

| Skin ID | `name` in `theme.c` | AD Theme # | Composition / Viz | Phase 5 status |
|--------:|---------------------|------------|-------------------|----------------|
| **0** | Walkman Premium | 1 | `COMP_WALKMAN` / `VIZ_LED_BAR` | **Elevated** |
| **1** | Winamp Classic | 2 | `COMP_WINAMP` / `VIZ_WINAMP_SPEC` | **Remain** (prior ship) |
| **2** | Cassette Deck | 3 | `COMP_CASSETTE` / `VIZ_CASSETTE_METERS` | **Remain** |
| **3** | MiniDisc | 4 | `COMP_MINIDISC` / `VIZ_MD_WAVE` | **Remain** |
| **4** | CD Player | 5 | `COMP_CD` / `VIZ_CD_RING` | **Elevated** |
| **5** | GameBoy DMG | 6 | `COMP_GAMEBOY` / `VIZ_PIXEL_SPEC` | **Remain** |
| **6** | GameBoy Color | 7 | `COMP_GBC` / `VIZ_PIXEL_SPEC` | **Elevated** |
| **7** | DOS | 8 | `COMP_DOS` / `VIZ_DOS_ASCII` | **Elevated** |
| **8** | Code Rain | 9 | `COMP_MATRIX` / `VIZ_MATRIX_RAIN` | **Elevated** (exhibition name) |
| **9** | Cyberpunk | 10 | `COMP_CYBER` / `VIZ_CYBER_GRID` | **Elevated** |
| **10** | CRT TV | 11 | `COMP_CRT` / `VIZ_CRT_SCAN` | **Polished** (scope progress) |
| **11** | PS2 Browser | 12 | `COMP_PS2` / `VIZ_SCOPE` | **Elevated** |
| **12** | XMB Music | 13 | `COMP_XMB` / `VIZ_XMB_WAVE` | **Elevated** |
| **13** | Dreamcast | 14 | `COMP_DREAMCAST` / `VIZ_DC_ORANGE` | **Elevated** |
| **14** | Arcade | 15 | `COMP_ARCADE` / `VIZ_ARCADE_NEON` | **Elevated** |

---

## AD NOTES obeyed in code

| Note | Implementation |
|------|----------------|
| **XMB** ruthless inspired-by Music | Cross-bar + custom N/D/W/H/M icons; “Now Playing” / “Music” chrome; no Settings/Photo/Video/Game firmware list; no clock/battery strip |
| **Matrix** rain capped | `viz_matrix_rain` ≤ **18 columns**, shorter trails; decode terminal is hero |
| **PS2** quiet system glyphs | Glass command box + floating tiles; legends PREV/PLAY/NEXT/EQ + “OK / BACK” — no X/O / WWW / HDD rail |
| **Walkman** slim EX + EL remote-only | Left EL column amber-only; industrial Mg-Al body; door/window hero; ≠ Cassette dual-VU deck |
| **Dreamcast** swirl dominant | Full-field procedural orange rings; thin white ABS header; dark well floats on swirl; no SEGA™ dump |
| **Arcade** IP-clean / attract ≠ paywall | Marquee = title/artist; `* ATTRACT *` micro; no INSERT COIN / ¥ |
| **GBC ≠ DMG recolor** | Thicker charcoal bezel, translucent purple shell + inner rail, taller color LCD, larger magenta/cyan A·B |
| **CRT** scope progress | Vertical scope cursor from elapsed — **no** hollow rectangle scrubber |

---

## Files touched

| File | Change |
|------|--------|
| `psp/src/theme.c` | Palettes + exhibition names for 0/4/6–9/11–14; CRT `PROG_NEEDLE`; Cyberpunk anti-purple carbon/cyan |
| `psp/src/ui.c` | NP shells for all newly cleared themes; Matrix rain cap; GBC shell deltas; CRT scope cursor; Appearance previews for all 15 |
| `design/implementation/PHASE5_FULL_NOTES.md` | This note |
| Build artifact | `psp/EBOOT.PBP` → Desktop `EBOOT_PSP_Music.PBP` |

### Explicitly unchanged (preserve behavior)

- Skip (L/R), TCP abort/retry (`http.c`), EQ audio presets, offline START — **no edits** to `main.c` / `http.c` / `player.c` / `offline.c`.

---

## Per-theme craft (new / polished)

1. **Walkman (0)** — EX industrial brush; EL remote ≥48 px; door+reels; LED ladder; edge dial.  
2. **CD (4)** — Centered Mg circle + cyan laser ring + buffer LEDs + under-body LCD; slim remote (not Walkman twin).  
3. **GBC (6)** — Letterbox; Atomic Purple translucency; charcoal bezel; color pixel meters.  
4. **DOS (7)** — Beige frame + phosphor dual-pane + ASCII spectrum + F-key legends.  
5. **Code Rain (8)** — Capped rain field + center DECODE terminal + LED ticks + CLI legends.  
6. **Cyberpunk (9)** — Carbon HUD frame; cyan edge; L/R neon ladders; OEL well; sparse lower grid.  
7. **CRT (10)** — Prior cabinet craft + **scope progress cursor** on service scope.  
8. **PS2 (11)** — Navy void; glass NP box; 4 floating tiles; EE SCOPE ribbon.  
9. **XMB Music (12)** — Calm waves; horizontal icon cross-bar; Music detail column; soft waveform + compact transport.  
10. **Dreamcast (13)** — Swirl ≥ majority of frame; energy bar; BIOS soft rows PLAY/PAUSE/SKIP/EQ.  
11. **Arcade (14)** — Marquee/stack; smoked playfield; flank neon; attract micro; candy panel.

**Prior five unchanged in layout intent:** Winamp inset Main · Cassette VU/well · MiniDisc TOC · DMG LCD-only · CRT cabinet (plus scope polish).

---

## Build / deploy

| Step | Result |
|------|--------|
| `bash /home/khakberdinma/build_psp.sh` | **BUILD_OK** (`psp/EBOOT.PBP` ~465572 bytes) |
| Desktop `EBOOT_PSP_Music.PBP` | Copied |
| `d:\PSP\GAME\music\` | **Absent** — skipped |

Compiler notes: unused-function warnings for `np_draw_viz` / `np_draw_transport_icons` (legacy helpers retained); pre-existing `http.c` / `strncpy` warnings; **no errors**.

---

## Harsh self-review (Implementation → recommend QA+AD device pass)

**Ship-ready for atlas, but spot-check these on hardware:**

1. **XMB / PS2 / Code Rain** — AD mini-spot list: confirm XMB never reads as System Settings; PS2 tiles stay glass-not-frost; rain stays behind terminal at 1.5 m.  
2. **Dreamcast swirl** — procedural rings are exhibition-cheap; at 2–3 m should still read orange-majority; if thin, thicken ring stroke before next paint pass.  
3. **Walkman EL** — remote keys are solid amber blocks; fine for PSP budget; watch faceplate for accidental orange wash.  
4. **GBC vs DMG** — side-by-side Appearance preview + NP; if still “purple DMG,” thicken bezel further.  
5. **Perf** — Dreamcast multi-ring + Matrix glyphs are the heaviest; frame hitch → drop Dreamcast ring count first, then Matrix columns (already ≤18).  
6. **Naming** — Skin 8 label is `Code Rain` (exhibition); skin 12 is `XMB Music` — Appearance list should match legal copy if AD renames again.

**Recommend:** QA walkthrough all 15 on device (NP + Appearance apply) + AD visual pass focused on XMB / Matrix / PS2 adjacency notes.

---

## Success criteria

- [x] Full 15-theme atlas elevated under RE-REVIEW notes  
- [x] Distinct meter / shell language per theme  
- [x] Matrix rain capped; GBC ≠ DMG recolor; Arcade attract-only; XMB Music framing  
- [x] Network / Skip / EQ / offline paths untouched  
- [x] Build green + Desktop EBOOT (see deploy table)  
- [x] Code QA + AD review → `design/qa/PHASE5_QA_AD_REVIEW.md` (**PASS WITH NOTES**)  
- [ ] Device QA + AD visual pass *(checklist in QA review §4)*  
