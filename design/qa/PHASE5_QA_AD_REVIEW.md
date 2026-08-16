# PHASE 5 — QA + Art Director Review (Full Atlas)

**Role:** QA + Art Director (Sony exhibition bar)  
**Date:** 2026-08-03  
**Inputs:** `PHASE5_FULL_NOTES.md` · `PHASE4_AD_REVIEW.md` (RE-REVIEW) · `psp/src/ui.c` · `psp/src/theme.c` · Appearance path in `main.c`  
**Method:** Code-mapped skin id → draw path; AD lock check (Q1–Q18); no full redesign  
**Build artifact reviewed:** `psp/EBOOT.PBP` 465572 B (Desktop copy current). `d:\PSP\GAME\music\` **absent**.

---

## 1. Overall — Implementation

| Gate | Result |
|------|--------|
| **Implementation (full 15 atlas)** | **PASS WITH NOTES** |
| **AD lock compliance (code)** | **Met** on all hard locks; residual soft notes |
| **Rubber-stamp?** | **No.** Ruthless path map + lock re-test. |

### Why not clean PASS

- Device visual pass not run (code-only gate).
- Dreamcast multi-ring swirl is exhibition-cheap and **perf-heavy** (spot on hardware).
- PS2 soft clock orb + Walkman/CD shared cassette helper remain soft atlas notes.
- GBC/DMG LCD stack overflowed glass (Q4) — **surgically fixed in `ui.c` source**; EBOOT not rebuilt in this QA pass.

### Why not FAIL

- All 15 skins have distinct `COMP_*` shells + palettes; Appearance previews cover 0–14.
- Hard AD NOTES from RE-REVIEW are obeyed in code: Walkman EL-only, XMB not firmware, Matrix ≤18 cols, PS2 quiet glyphs, GBC≠DMG structure, Dreamcast swirl-dominant, Arcade attract/IP-clean, CRT scope progress.
- Skip / TCP / EQ / offline paths untouched per Implementation notes (`main.c` Appearance wiring only).

---

## 2. Skin map (0–14) — draw path + scores

Scoring: readability · uniqueness · materials vs flat rects · AD lock · PSP perf risk.  
Pass ≥7 and no hard-lock fail.

| ID | Skin | Draw path | Read | Unique | Materials | AD lock | Perf | Verdict |
|---:|------|-----------|-----:|-------:|----------:|--------:|-----:|---------|
| **0** | Walkman Premium | `COMP_WALKMAN` → `np_comp_walkman` · `VIZ_LED_BAR` · `PROG_LED` | 8 | 8 | 7 | **PASS** Q1/Q2/Q18 — EL col amber-only; slim EX body; ≠ Cassette dual-VU | Low | **PASS** — residual: solid amber remote keys; shared `np_draw_cassette` OK if framing holds |
| **1** | Winamp Classic | `COMP_WINAMP` → `np_comp_winamp` · inset Main 330×140 · `VIZ_WINAMP_SPEC` | 9 | 9 | 8 | **PASS** Q3 | Low | **PASS** — prior ship; desk void OK |
| **2** | Cassette Deck | `COMP_CASSETTE` · dual VU + well · rubber keys | 8 | 8 | 8 | **PASS** (materials north star) | Low–Med (needles) | **PASS WITH NOTES** — layout ≠ product still (intentional) |
| **3** | MiniDisc | `COMP_MINIDISC` · disc window + TOC · `VIZ_MD_WAVE` | 8 | 8 | 8 | **PASS** | Low | **PASS** |
| **4** | CD Player | `COMP_CD` · radial Mg + cyan laser · slim RM (not EL twin) | 8 | 8 | 7 | **PASS** Q9/Q14 — no blueprint | Med (filled circles) | **PASS** |
| **5** | GameBoy DMG | `COMP_GAMEBOY` · letterbox · UI-in-LCD · `VIZ_PIXEL_SPEC` | 8 | 8 | 7 | **PASS** Q4 *(LCD stack tightened in QA)* | Low | **PASS** |
| **6** | GameBoy Color | `COMP_GBC` · purple shell + charcoal bezel · color meters | 8 | 8 | 8 | **PASS** Q4 — ≠ DMG recolor *(LCD overflow fixed)* | Low | **PASS** — side-by-side device check still recommended |
| **7** | DOS | `COMP_DOS` · beige frame · dual-pane · `VIZ_DOS_ASCII` · F-keys | 8 | 9 | 7 | **PASS** anti-card | Low | **PASS** |
| **8** | Code Rain | `COMP_MATRIX` · full-field rain + DECODE terminal · rain ≤**18** | 7 | 8 | 6 | **PASS WITH NOTES** Q6/Q11 | **Med–High** (glyphs) | **PASS WITH NOTES** — rain subordinate; exhibition name OK |
| **9** | Cyberpunk | `COMP_CYBER` · carbon HUD · cyan edge · L/R ladders · sparse grid | 8 | 8 | 7 | **PASS** Q7 anti-purple Material | Low–Med | **PASS** — motif pink edge is neon, not Material purple |
| **10** | CRT TV | `COMP_CRT` · cabinet + speakers · `PROG_NEEDLE` + **scope cursor** | 8 | 9 | 8 | **PASS** — hollow scrubber killed | Low–Med | **PASS** |
| **11** | PS2 Browser | `COMP_PS2` · glass NP box · PREV/PLAY/NEXT/EQ · OK/BACK · EE SCOPE | 7 | 8 | 7 | **PASS WITH NOTES** — quiet glyphs; soft clock orb remains | Low | **PASS WITH NOTES** — drop/soften clock orb if firmware-adjacent on device |
| **12** | XMB Music | `COMP_XMB` · N/D/W/H/M icons · “Now Playing”/“Music” · **no** clock/battery | 8 | 8 | 6 | **PASS** Q5/Q15 | Med (wave px) | **PASS** — letter icons cheap vs PSN orbs; keep ruthless |
| **13** | Dreamcast | `COMP_DREAMCAST` · `np_dc_swirl_field` (14+10 rings) · thin ABS · BIOS rows | 8 | 9 | 7 | **PASS** Q12/Q16 — swirl majority; no ™ dump | **High** | **PASS WITH NOTES** — thicken stroke or cut ring count if hitch |
| **14** | Arcade | `COMP_ARCADE` · marquee title · `* ATTRACT *` · no INSERT/¥ · flank neon | 8 | 8 | 7 | **PASS** Q16/Q17 | Low–Med | **PASS** |

### AD NOTES checklist (binding)

| Note | Code evidence | Status |
|------|---------------|--------|
| Walkman EL remote-only | `np_comp_walkman`: EL column `th->accent`; body Mg-Al chrome | **OK** |
| XMB not firmware | Custom N/D/W/H/M; no Settings/Photo/Video/Game; no clock/battery strip | **OK** |
| Matrix rain cap | `viz_matrix_rain` `max_cols ≤ 18`, trail ≤4 | **OK** |
| PS2 quiet glyphs | Labs PREV/PLAY/NEXT/EQ + “OK / BACK”; no X/O / WWW / HDD | **OK** (orb soft) |
| GBC ≠ DMG | Separate shell path: purple translucency, charcoal bezel, larger A·B, color meters | **OK** |
| Dreamcast swirl dominant | Full-field orange rings before thin white header/footer + dark well | **OK** |
| Arcade attract / IP-clean | `* ATTRACT *` only; no SNK/Neo-Geo / coin price | **OK** |
| CRT scope progress | `PROG_NEEDLE` + vertical scope cursor; no hollow rect scrubber | **OK** |

---

## 3. Shared screens (code)

### Appearance (`ui_draw_appearance` / `ui_draw_np_preview_box`)

- List: 8 visible rows, scroll clamps to `SKIN_COUNT` (15) — **OK**.
- Preview motifs for **all** compositions 0–14 — **OK**.
- Suspected soft clips: Cassette preview VU at `x + w - 44` inside 228-wide box — tight but clipped by `ui_fill` bounds; DOS ASCII + dual pane crowded in 100px height — readable as thumb only.
- Long names (`Walkman Premium`, `GameBoy Color`) use `ui_text_clip` — **OK**.

### Lists (`ui_draw_list`)

- Row 20px, visible derived from 272 − header − footer — **OK**.
- No theme-specific chrome (shared library UI) — expected; not an atlas fail.

### EQ (`ui_draw_eq_panel`)

- Full-screen overlay when `np->show_eq`; returns before skin draw — **OK**.
- 10 band sliders `200 + i*26` → last x≈434 + 8w — within 480 — **OK**.
- Theme-tinted chrome from `theme_active()` — good; no layout collision with NP shells.

### Other code notes (non-blocking)

- Walkman volume dial: `160 - (np->volume * 100) / 100` is identity math — works but sloppy; polish later.
- Legacy helpers `np_draw_viz` / `np_draw_transport_icons` unused — compiler noise only.
- CD/Walkman transport near y=236–264 — within 272; no footer on several skins — intentional per-composition.

---

## 4. Device regression checklist

Run on PSP after flashing current EBOOT (+ rebuild if taking GBC LCD fix):

| # | Check | Pass? |
|---|--------|:-----:|
| 1 | **Skip** L/R track change from NP (all skins) | ☐ |
| 2 | **TCP** abort mid-fetch + retry recovers | ☐ |
| 3 | **EQ** SELECT open → L/R presets change *audio* → close restores NP | ☐ |
| 4 | **Offline** START save / play from Offline list | ☐ |
| 5 | **Theme switch** Appearance L/R preview → X Apply → NP matches skin | ☐ |
| 6 | Cycle skins **0→14** on NP; title/time readable at arm’s length | ☐ |
| 7 | Spot **XMB**: never reads as System Settings | ☐ |
| 8 | Spot **Code Rain**: DECODE terminal reads over rain at ~1.5 m | ☐ |
| 9 | Spot **PS2**: glass tiles, no white frost; OK/BACK only | ☐ |
| 10 | Spot **GBC vs DMG**: purple shell ≠ olive recolor | ☐ |
| 11 | Spot **Dreamcast**: orange majority; note frame hitch | ☐ |
| 12 | Spot **Arcade**: ATTRACT only; no coin/¥ | ☐ |
| 13 | Spot **CRT**: progress = scope cursor, not phone bar | ☐ |
| 14 | Spot **Walkman**: amber only on EL remote | ☐ |

---

## 5. Must-fix vs nice-to-have

### Must-fix (before clean exhibition claim)

| Priority | Item | Status |
|----------|------|--------|
| **M1** | GBC/DMG: keep all NP UI inside LCD glass (Q4) | **FIXED in source** (`np_comp_gameboy` stack tightened). **Rebuild EBOOT** before device demo of GBC/DMG. |
| **M2** | Device pass checklist §4 (at least Skip/EQ/offline + XMB/Matrix/PS2/Dreamcast spots) | **Open** — blocks clean PASS, not WITH NOTES clearance |

### Nice-to-have

| Item |
|------|
| Dreamcast: reduce ring count / thicken stroke if hitch or swirl reads thin at 2–3 m |
| PS2: remove or further mute soft clock orb |
| Matrix: if ADHD on device, drop columns toward 12 |
| Walkman: quieter OEM “EX” / less blocky EL keys |
| Appearance Cassette/DOS previews: less cramped thumbs |
| Rebuild after M1 so Desktop / card EBOOT match source |

**No Concept re-paint required.** No massive atlas rewrite.

---

## 6. Exhibition demo build clearance

| Question | Answer |
|----------|--------|
| **Clearance for exhibition demo build?** | **WITH NOTES** |
| **YES if…** | Device §4 checklist green + EBOOT rebuilt with GBC LCD fix |
| **NO if…** | Firmware-confusion on XMB, rain drowning terminal, Dreamcast unplayable hitch, or Arcade coin-gate creep — none observed in code |

**Notes for demo:** Prefer spotlight skins that already ship strong: Winamp, Cassette, MiniDisc, DMG, CRT, Dreamcast, Arcade, Walkman. Keep XMB / Code Rain / PS2 as “inspired-by” talking points. Watch Dreamcast + Code Rain FPS.

---

## Deploy stamp (this QA pass)

| Artifact | Result |
|----------|--------|
| Desktop `EBOOT_PSP_Music.PBP` | Synced to `psp/EBOOT.PBP` (465572 B) |
| `d:\PSP\GAME\music\` | Absent — skipped |
| Source delta vs EBOOT | GBC/DMG LCD clamp in `ui.c` **not yet in EBOOT** — rebuild before card |

---

## Sign-off

**Implementation: PASS WITH NOTES.**  
**Exhibition clearance: WITH NOTES.**  
Hard AD locks clear in code. Top open items: **rebuild for M1**, then **device §4**.  

**— QA + Art Director · Aug 3, 2026**

- EBOOT rebuilt after Q4 GBC/DMG LCD-in-glass clamp (Aug 3, 2026 evening).
