# Phase 4 — Concept Art Pack Index

**Проект:** Music Online PSP — AAA exhibition bar music player UI  
**Фаза:** 4 — Concept Art (Sony Design Center / industrial + UI)  
**Вход:** AD locks · moodboards · wireframes · research  
**Экран:** 480×272 feel (hero stills prefer ~16:9)  
**Статус:** Concept Art Team deliverable — **NO CODE · NO EBOOT · NO Phase 5**

---

## Documents

| File | Purpose |
|------|---------|
| [PHASE4_CONCEPT_ART.md](./PHASE4_CONCEPT_ART.md) | 15 theme briefs: hero still, callouts, motion beats, empty/loading, Appearance thumb, booth self-score |
| [PHASE4_AD_HANDOFF.md](./PHASE4_AD_HANDOFF.md) | Art Director gate note — gaps, readiness, clearance ask |
| [images/](./images/) | Generated hero Still concepts (priority + optional) |
| [../art_direction/PHASE1_AD_REVIEW.md](../art_direction/PHASE1_AD_REVIEW.md) | **BINDING** locks Q1–Q12 |
| [../moodboards/PHASE2_MOODBOARDS.md](../moodboards/PHASE2_MOODBOARDS.md) | Materials / palette / meter language |
| [../wireframes/PHASE3_WIREFRAMES.md](../wireframes/PHASE3_WIREFRAMES.md) | Zone rects (±4–8 px for paint) |

---

## Generated images (this pack)

### Priority heroes (required)

| File | Theme |
|------|--------|
| `images/hero_01_walkman_premium.png` | 1 — Walkman Premium |
| `images/hero_02_winamp_classic.png` | 2 — Winamp Classic |
| `images/hero_03_cassette_deck.png` | 3 — Cassette Deck |
| `images/hero_13_xmb_inspired.png` | 13 — PSP XMB-inspired |
| `images/hero_14_dreamcast.png` | 14 — Dreamcast |

### Optional heroes (nice-to-have — shipped)

| File | Theme |
|------|--------|
| `images/hero_04_minidisc.png` | 4 — MiniDisc |
| `images/hero_05_cd_player.png` | 5 — CD Player |
| `images/hero_06_gameboy_dmg.png` | 6 — GameBoy DMG |
| `images/hero_11_crt_tv.png` | 11 — CRT TV |
| `images/hero_15_arcade.png` | 15 — Arcade |

Themes **without** a painted still (7 GBC, 8 DOS, 9 Matrix, 10 Cyberpunk, 12 PS2): full written briefs in `PHASE4_CONCEPT_ART.md` only — Implementation must not invent phone chrome to fill gaps; paint or atlas from brief + Phase 2/3.

---

## How Implementation must use these packs

1. **AD first.** Never reopen Q1–Q12. If a still conflicts with a lock, **lock wins** (Concept notes flag known still risks in AD handoff).  
2. **Wireframe geometry is law.** Zone rects from Phase 3 ±4–8 px. Concept paintings are lighting/material targets, not freeform re-layouts.  
3. **Moodboard hex + materials** = atlas recipe. Specular = prebaked 2–3 stop; glow budget = 1–2 additive layers.  
4. **Hero stills** = north-star look for Now Playing of that theme. Match: hero object, meter tech, transport fiction, finish language.  
5. **Do not** treat stills as pixel-perfect bitmaps to blit 1:1 — distill into sprites/gradients/glass overlays that fit PSP RAM/GPU.  
6. **Appearance preview** must read as the same silhouette as the NP hero (door+remote, Main+pledit, swirl field, etc.), not a palette chip.  
7. **Empty / Loading** keep the same housing; progress uses theme meter language — never Material spinner / phone empty-state.  
8. **Forbidden product language:** flat Material/iOS, empty minimalism, colored-rectangle UI, neon-as-body, phone FABs/bottom tabs.  
9. **Pixel themes (GB/GBC/Arcade/DOS):** elevate via bezel / plastic / glass / cabinet — pixels live *inside* housing.  
10. **XMB copy:** always “inspired by XMB language” — no official firmware claim.  
11. **Do not start Phase 5** until Art Director clears Phase 4.

---

## AD checklist (Concept → AD → Implementation)

| # | Check | Owner |
|---|--------|--------|
| 1 | Walkman = EX black/silver; EL remote-only; no pastel NW | AD + Impl |
| 2 | Winamp Main inset classic aspect — not fullscreen stretch | AD + Impl |
| 3 | GB/GBC letterbox shell; gutters void | AD + Impl |
| 4 | Dreamcast orange swirl dominant field | AD + Impl |
| 5 | Cover art only where Q9 allows | AD + Impl |
| 6 | Unique hero + unique meter per theme (uniqueness matrix) | AD |
| 7 | Viz ≠ generic UI rectangles | Concept + Impl |
| 8 | Shared finish vocabulary, unique composition (Q8) | Concept + Impl |
| 9 | Arm’s-length title/time contrast (Q10) | AD + Impl |
| 10 | Motion calm — alive but not ADHD (Q11) | Impl |
| 11 | Hero stills reviewed vs wireframe zones | AD |
| 12 | **Gate Phase 4 before any `psp/src/**` theme paint** | AD |

---

## Out of scope

- Phase 5 implementation / `psp/src/**` / EBOOT  
- Reopening AD locks  
- Trademarked film logos / official Sony firmware assets dump  

---

## Clearance request

**Concept Art Team recommends: YES — Art Director must gate Phase 4 before Implementation.**  
See `PHASE4_AD_HANDOFF.md`.
