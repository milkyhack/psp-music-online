# Phase 3 — Wireframe Pack Index

**Проект:** Music Online PSP — AAA exhibition bar music player UI  
**Фаза:** 3 — UI/UX Wireframes (NO CODE · NO COLOR FILL)  
**Вход:** `design/art_direction/PHASE1_AD_REVIEW.md` + `design/moodboards/PHASE2_MOODBOARDS.md`  
**Экран:** 480×272 (PSP native)  
**Статус:** Wireframe Team deliverable — composition / zones / hierarchy only

---

## Documents

| File | Purpose |
|------|---------|
| [PHASE3_WIREFRAMES.md](./PHASE3_WIREFRAMES.md) | Shared screens + 15 theme Now Playing packs + master principles |
| [../moodboards/PHASE2_MOODBOARDS.md](../moodboards/PHASE2_MOODBOARDS.md) | Hero / meter / composition intent (bind visual language) |
| [../art_direction/PHASE1_AD_REVIEW.md](../art_direction/PHASE1_AD_REVIEW.md) | Binding AD locks Q1–Q12 |
| [../research/PHASE1_DEVICE_RESEARCH.md](../research/PHASE1_DEVICE_RESEARCH.md) | Device DNA & anti-patterns |

---

## How to read these packs

1. **Read AD locks first** (Q1–Q12) — wireframes may not reopen them.  
2. **Read the moodboard card** for the theme you are drawing — hero object + meter language are contracts.  
3. **Shared screens** (Appearance, Browse, EQ, Empty, Loading) are defined once; each theme lists **deltas only**.  
4. **Now Playing** is fully specified per theme — Art Director rejects “same boxes, different labels.”  
5. Boxes are **zones**, not materials. No hex fills, no brushed metal recipes here (those live in Phase 2 / Phase 4).  
6. Coordinates are **px on 480×272**, origin top-left `(0,0)`.  
7. ASCII diagrams are schematic — Concept Art (Phase 4) must preserve zone proportions, not pixel-copy ASCII.

### Legend (all diagrams)

| Symbol | Meaning |
|--------|---------|
| `[====]` | Solid zone / housing block |
| `(....)` | Soft void / letterbox / desk |
| `~text~` | Marquee / truncate text lane |
| `▼ ▲` | Hierarchy markers (1 = primary) |
| `+--+` | Bezel / glass edge (outline only) |

---

## Shared PSP canvas rules

### Frame

| Property | Value |
|----------|-------|
| Canvas | **480 × 272** |
| Origin | top-left `(0, 0)` |
| Safe margin (outer) | **≥ 8 px** from screen edge for interactive hit targets; **≥ 12 px** preferred for primary labels |
| Content gutter | Prefer **12 px** rhythm between major blocks |
| Dead-edge risk | Avoid critical type in outer **4 px** (overscan / bezel crop on some capture setups) |

### Suggested alignment grid

- **Columns:** 12-col mental grid → column width ≈ 36 px + 4 px gutter → usable content ≈ `x: 12 … 468`  
- **Rows:** 8-row mental grid → row height ≈ 30 px + 4 px gutter → usable content ≈ `y: 8 … 264`  
- **Baseline type:** titles ≥ ~14–16 px visual height at arm’s length; time digits ≥ ~18–22 px where fiction allows  
- **Primary action cluster:** keep within lower **80 px** or within the theme’s physical key row fiction  

### Clock / status strip (when fiction allows)

| Mode | When | Rect (default) |
|------|------|----------------|
| **Full status** | XMB-inspired, CRT OSD, DOS header, PS2 | `x:8–12, y:4–8, w:456–464, h:16–20` |
| **Micro status** | Walkman remote, MD body, Discman | embedded in remote/LCD well — not a phone app bar |
| **None** | Winamp Main (status inside chrome), Arcade marquee-as-title | do not invent a second status bar |

**Rule:** Never add a floating “iOS status bar” on industrial object themes. Clock/battery only where the fiction already has a header (XMB, DOS, CRT OSD, PS2).

### Volume placement (shared intent)

| Pattern | Themes | Zone |
|---------|--------|------|
| Edge dial / slim vertical | Walkman, MD, CD | Right edge strip `x≈448–472` or remote rocker |
| In-chrome slider | Winamp | Inside Main (volume sprite lane) |
| Soft-key / HUD | Cyberpunk, Cassette, CRT | Near transport row |
| Implicit (L/R or analog) | XMB, DOS, Matrix | Legend only — no phone volume FAB |

---

## PSP control mapping notes (exhibition — no new gameplay)

Wireframes hint **affordances**, not invent mechanics. Map to existing player verbs: play/pause, seek, prev/next, browse, EQ, theme picker, back.

| Control | Typical verb (exhibition default) | Reachability note |
|---------|-----------------------------------|-------------------|
| **D-pad ↑↓** | List focus / TOC step / EQ band | Lists need ≥ 22–24 px row height |
| **D-pad ←→** | Seek coarse / category cross-bar / scrub | Keep scrub lane ≥ 12 px tall |
| **✕ (X)** | Confirm / open / (region: JP often cancel) | Primary confirm cluster should read near lower-right fiction |
| **○ (O)** | Back / cancel / (region: JP often confirm) | Always have an obvious “back out of panel” path |
| **□ ([])** | Secondary: shuffle / EQ toggle / mode | Do not hide sole transport behind □ |
| **△ (△)** | Appearance / info / overlay | Theme picker reachable without deep menu tree |
| **L / R** | Prev / next track (or category step on XMB-like) | Must work on Now Playing without leaving hero |
| **SELECT** | Appearance / theme picker shortcut | Documented on Empty + Appearance screens |
| **START** | Play/pause **or** pause overlay (pick one per theme; stay consistent) | Prefer START = play/pause on object decks; XMB may use START = overlay |

**Region note:** Do not hardcode JP vs US confirm in wireframe geometry — keep both buttons in the physical cluster fiction; labels in Phase 4/localization.

**Anti-invention:** No new minigames, no paywall “insert coin” gates, no fake firmware settings trees. Attract blink on Arcade is flavor only.

---

## Screen set (every theme must cover)

| Screen | Shared template? | Per-theme duty |
|--------|------------------|----------------|
| Now Playing | Shared NP zone *names* only | Full unique composition + rects |
| Appearance / theme picker | Shared once | Delta (preview chrome only) |
| Browse (artists / albums / tracks) | Shared once | Delta (list chrome / TOC / pledit) |
| EQ panel | Shared once | Delta (needles vs bars vs OEL) |
| Empty / No music | Shared once | Delta (hero idle state) |
| Loading / buffering | Shared once | Delta (visual intent only) |

---

## AD locks (wireframe team must obey)

- Walkman = **EX industrial** composition (door + remote strip)  
- EL orange = **remote zone geometry**, not full-face flood  
- Winamp = **inset classic aspect** Main (~scaled 275∶116 family)  
- GB / GBC = **letterbox centered shell** (~360×230), gutters = void  
- XMB = **inspired-by** cross-bar; not fake system firmware chrome claim  
- Meters first over kitsch BGV; calm motion budget  
- Dreamcast = **orange swirl as dominant field**, not tiny corner mark  
- Cover art windows only where AD Q9 allows  

---

## Out of scope

- Phase 4 concept art / pixel paintings (beyond optional tiny ASCII)  
- `psp/src/**`, EBOOT, builds  
- Color / material fill recipes (Phase 2 owns palette; Phase 4 paints)  

---

## Clearance request

Wireframe Team recommends: **YES — clear for Phase 4 Concept Art**, provided Concept Art obeys zone maps + AD locks and does not collapse themes into palette swaps.
