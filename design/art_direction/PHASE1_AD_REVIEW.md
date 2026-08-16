# PHASE 1 — Art Director Gate Review

**Проект:** Music Online PSP — AAA exhibition bar music player UI (Sony 2006 era)  
**Роль:** Art Director  
**Дата:** 2026-08-03  
**Вход:** `design/research/PHASE1_DEVICE_RESEARCH.md`  
**Экран:** 480×272, arm’s-length / bar demo (~0.5–1.5 m primary; 2–3 m glance)

---

## Verdict / Вердикт

| Gate | Score |
|------|-------|
| **Phase 1 Research** | **PASS WITH NOTES** |
| **Concept cleared for Phase 2 Moodboards** | **YES** (выполняется в этом же deliverable) |
| **Concept cleared for Phase 3 Wireframes** | **YES** — после moodboards; wireframes **не стартуют** в этой задаче |
| **Code / EBOOT** | **FORBIDDEN** на этой фазе |

### Criteria check

| Criterion | Result |
|-----------|--------|
| Research supports PlayStation Experience–demoable UI | **Yes** — device lineages, layout DNA, PSP constraints, anti-homebrew list |
| Materials language clear | **Yes** — brushed metal, ABS, smoked glass, acrylic, rubber, carbon, anodized |
| Themes distinguishable | **Yes** — 15 cards with unique hero objects / meters / type; risk of palette clones mitigated by AD locks below |

### Notes (не блокируют Phase 2)

1. Repo palette «Walkman Premium» (silver + pastel pink) **kill** — см. решение Q1; Concept не опирается на текущий `theme.c` цвет как north star.  
2. Album-art policy (Q9) зафиксирована ниже — moodboards обязаны помечать cover-allowed vs fiction-break.  
3. Shared chrome kit (Q8): **shared finish vocabulary**, не shared layout — иначе 15 тем станут clones.  
4. Exhibition readability (Q10) **перевешивает** microscopic authenticity на LCD contrast / label size.  
5. Matrix / Cyberpunk: language homage only — без trademarked logos / exact HUD clones.

---

## Binding decisions on open questions

Каждое решение **обязательно** для moodboards, wireframes и будущей реализации.

### Q1 — Walkman Premium north star

**Решение: EX-era black / silver industrial (Premium Object).**

- **Pick:** WM-EX808HG / WM-EX5 / WM-EX90 lineage — Mg-Al / mirror door / grey rubber edges / quiet embossed branding.  
- **Kill:** NW-A1000 soft white / pastel pink as primary Walkman Premium language (fashion NW ≠ exhibition industrial object).  
- **Why:** PlayStation Experience bar 2006 требует *серьёзный портативный Hi-Fi object*, не candy NW fashion. Pastel NW можно как future variant skin, не как Premium flagship.  
- **Repo note:** текущая light silver + pastel pink accent → **replace** в будущих спеках.

### Q2 — Orange EL myth

**Решение: Remote-stick EL orange/amber as signature; body LCD stays subdued segment/LCD.**

- **Pick:** EL glow lives in a dedicated **remote panel zone** (left strip or bottom remote bar) — orange/amber primary, blue EL as secondary accent option within same theme.  
- **Not:** full-faceplate orange flood / whole-screen EL wash.  
- **Why:** historically accurate (EX remotes carry the myth) + creates a *hero glow object* without turning the deck into neon homebrew. Body keeps brushed/mirror seriousness; glow = jewelry, not body paint.

### Q3 — Winamp fidelity

**Решение: Strict classic proportions inset (collectible Main window), not fullscreen stretch.**

- **Pick:** Main chrome ~classic aspect (≈275×116 scaled) inset upper-left or top-center; playlist strip + optional EQ mode fill remainder.  
- **Not:** stretching Main window to 480×272 (destroys cult proportions — Anti-pattern #12).  
- **Why:** Winamp’s value is *craft inside rigid slots*. Fullscreen metal = generic head-unit clone, loses collectible identity.

### Q4 — GB / GBC portrait-in-landscape

**Решение: Letterbox centered handheld shell (portrait object in landscape frame).**

- **Pick:** Centered DMG/GBC shell (~360×230 usable), large grey/dark bezel, music UI **only inside LCD glass**. Side gutters = soft desk/void, not second UI.  
- **Not:** rotate metaphor / sideways handheld / full-bleed green without shell.  
- **Why:** Shell + bezel + lens *are* the design elevation. Letterbox sells “handheld object on table”; rotation confuses transport mapping on PSP controls.

### Q5 — XMB authenticity vs Sony brand-risk

**Решение: “Inspired by XMB language” — not a fake firmware / official theme claim.**

- **Pick:** Cross-bar navigation DNA (icons travel, focus pulse, waves, Music category feel), **custom** icon set + wave art, no Sony wordmarks as product claim, no “Official PSP Theme” framing. Exhibition copy: *inspired by XMB visual language*.  
- **Not:** pixel-perfect system XMB clone, stock Sony theme assets dump, or UI that could be mistaken for firmware overlay.  
- **Why:** PlayStation Experience can celebrate Sony UI grammar; claiming official firmware skin is brand/legal risk and cheapens the custom player fiction.

### Q6 — Matrix / Cyberpunk IP

**Решение: Generic film / cyber cinema language; no named trademark dumps.**

- **Matrix theme:** monochrome green rain + terminal decode fiction; **no** film logos, “The Matrix” wordmark, or exact Warner IP assets. Title internally may stay “Matrix” as project skin name; on-exhibition copy prefers “Code Rain / Digital Cascade” if legal review tightens.  
- **Cyberpunk:** Blade Runner / GitS *atmosphere* + head-unit OEL hardware cousin; **no** 1:1 game HUD clones (Cyberpunk 2077 etc.).  
- **Why:** Homage is exhibition-safe; trademarked marks are not.

### Q7 — Head-unit kitsch budget

**Решение: Meters-only seriousness as default; dolphin-tier BGV forbidden as primary.**

- **Pick:** VFD/OEL spectrum, VU, soft BGV at most as *barely visible* ambience in Cyberpunk/head-unit-adjacent skins.  
- **Not:** Clarion horse race / Pioneer dolphins competing with track title.  
- **Why:** Bar demo = track/artist readable; kitsch BGV eats attention and looks homebrew carnival.

### Q8 — Shared chrome kit vs unique per theme

**Решение: Shared finish vocabulary + unique composition per theme.**

- **Shared:** material dictionary (brushed, matte ABS, smoked glass, acrylic edge, rubber, carbon, anodized), specular recipe (prebaked 2–3 stop gradients), glow budget (1–2 additive layers).  
- **Unique:** housing proportions, hero object, meter type, type/display tech, motion signature.  
- **Why:** RAM/GPU need a kit; AAA distinction needs unique *objects*, not 15 palette swaps of one layout.

### Q9 — Album art policy

**Решение: Cover art only where object fiction supports a window / disc face.**

| Theme | Cover art |
|-------|-----------|
| CD Player | **Yes** — in disc window / circular face |
| MiniDisc | **Optional tiny** cartridge label feel; not Spotify hero |
| Cassette / Walkman | **No** full cover — cassette window / door is hero; optional tiny J-card hint max |
| Winamp | **No** full-bleed art; classic has no album hero |
| Arcade / GB / GBC / DOS / Matrix | **No** |
| CRT / PS2 / XMB / Dreamcast / Cyberpunk | **Optional small** inset only if it reads as “media object,” not phone hero |

### Q10 — Exhibition viewing distance

**Решение: Prioritize arm’s-length readability on 480×272; authenticity yields to contrast.**

- **Primary read @ ~0.5–1.5 m:** track title, time, play state, transport affordance.  
- **Secondary @ 2–3 m:** theme silhouette / hero object / meter motion must still “read as a device.”  
- **Rules:** LCD digits high-contrast; avoid authentic-but-muddy green-on-green; labels ≥ readable bitmap sizes; no microscopic silkscreen as sole status.  
- **Why:** Expo bar ≠ museum loupe. Cheap homebrew fails here by tiny low-contrast text.

### Q11 — Motion quietness (bar environment)

**Решение: Calm presence — “alive but not ADHD.”**

| Intensity | Allowed |
|-----------|---------|
| Always-on slow | Waves, swirl drift, scanline crawl, CRT phosphor, rain columns (capped), disc/reel loops |
| Medium | Spectrum/VU ballistics, focus pulse, marquee blink, EL remote breathe |
| Rare / event | Degauss flash, track-change glitch (1–2 frames), theme-enter sting |

- **Max:** no competing full-screen particle storms; BGV never louder than meters + title.  
- **Tempo:** exhibition = lounge Hi-Fi, not rave HUD.

### Q12 — Dreamcast swirl color

**Решение: JP/NA orange swirl as default for all locales.**

- **Pick:** Orange energy as brand motif (white ABS + orange swirl).  
- **Not:** per-locale blue EU toggle in v1 (adds confusion in a 15-skin showcase).  
- **Why:** One strong silhouette for the Dreamcast skin in a demo reel; EU blue can be a later easter-egg variant.

---

## Additional AD locks (exhibition / AAA)

1. **Tone:** commercial AAA Sony 2006 exhibition — industrial object + cult Winamp collectible, never “colored rectangles homebrew.”  
2. **Forbidden primary language:** Material/iOS cards, empty flat fields, palette-only clones, neon-as-body, phone FABs / bottom tabs / huge circular play (except where device fiction has a physical round control).  
3. **Pixel themes (GB/GBC/Arcade/DOS):** device housing first; pixels live behind glass/bezel/CRT.  
4. **XMB copy line:** always “inspired by XMB language.”  
5. **Hero object rule:** every Now Playing screen has one signature physical/metaphor object (cassette, MD, CD, Walkman body, XMB wave, marquee, swirl, etc.).  
6. **Viz rule:** no generic UI rectangles as meters — LED ladders, VFD, needles, scope, phosphor blocks, laser ring, neon tubes, ASCII blocks, etc.

---

## Score rationale (PASS WITH NOTES)

Research pack is **demo-ready as a brief**: lineages sourced, 15 cards with layout/materials/type/motion/PSP constraints, cross-cutting Sony principles, meter table, Winamp anatomy, XMB/PS2 motion DNA, anti-patterns, bibliography.

Notes are **direction locks**, not missing research. No FAIL blockers: materials language and theme differentiation are sufficient for moodboards.

### What would have been FAIL (not present)

- Missing materials dictionary  
- Themes only as color lists  
- No 480×272 layout intent  
- No anti-homebrew / phone transplant guidance  
- Unresolved Walkman industrial vs pastel without flagging the conflict  

---

## Phase clearance

| Phase | Cleared? |
|-------|----------|
| Phase 2 — Moodboards | **YES** — proceed |
| Phase 3 — Wireframes | **YES in principle** after moodboards land; **do not start** in this task |
| Implementation / `psp/src/**` | **NO** |

**Art Director sign-off:** Phase 1 accepted with notes. Binding answers Q1–Q12 above. Concept / Moodboard Team authorized for Phase 2 only.
