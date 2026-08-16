# PHASE 3 — Wireframes (Themes 1–15)

**Проект:** Music Online PSP — commercial AAA Sony 2006 exhibition  
**Роль:** UI/UX Wireframe Team  
**Дата:** 2026-08-03  
**AD gate:** Binding locks Q1–Q12 — `design/art_direction/PHASE1_AD_REVIEW.md`  
**Moodboards:** `design/moodboards/PHASE2_MOODBOARDS.md`  
**Язык:** нарратив на русском; px / coords / zone names на English  
**NO CODE · NO COLOR / MATERIAL FILL**

Canvas: **480 × 272**. Origin: top-left `(0,0)`.

---

# 0. Master cross-theme wireframe principles

## 0.1 Rhythm

1. **One hero per Now Playing.** If two objects compete at equal size, composition fails AD Hero Object rule.  
2. **Meters accompany hero**, never replace title readability. Title/time remain hierarchy-1 or -2.  
3. **Transport lives where the fiction has keys** — bottom row (deck/arcade), in-chrome (Winamp), physical shell buttons (GB), soft-keys (HUD) — never a floating phone FAB.  
4. **Calm density:** leave intentional void (desk / letterbox / black field) rather than packing 15 widgets. Exhibition silhouette must read at 2–3 m.  
5. **Marquee only in dedicated lanes** — never across bezels or over meter glass.

## 0.2 Margins & gutters

| Rule | Value |
|------|-------|
| Outer safe margin | **≥ 8 px** (prefer **12 px** for primary labels) |
| Between major blocks | **≥ 8–12 px** |
| Inside glass/LCD content inset | **≥ 4–6 px** from bezel inner edge |
| Letterbox gutters (GB/GBC) | Soft void; **no second UI** in gutters (AD Q4) |
| Winamp remainder | Playlist/EQ fill remainder; keep **≥ 8 px** gap from Main chrome |

## 0.3 Alignment grid

- Prefer edges snapped to **4 px** increments.  
- Shared vertical anchors across object themes: faceplate top ≈ `y:8–16`, transport baseline ≈ `y:232–264`.  
- Shared horizontal anchors: body center ≈ `x:240`, remote-left themes use left strip `x:0–56`.  
- Do **not** force all 15 themes onto one skeleton — grid is a measuring tool, not a clone template.

## 0.4 Visual hierarchy (labeling used below)

| Rank | Role | Arm’s-length duty |
|------|------|-------------------|
| **1 Primary** | Hero object silhouette + (usually) track identity | Readable ~0.5–1.5 m |
| **2 Secondary** | Time, play state, meters in motion | Confirms “device is alive” |
| **3 Tertiary** | Silkscreen, mono/stereo, micro icons, EQ labels | Bonus at close range |

## 0.5 Truncation / marquee rules

| Lane type | Max visual width (typical) | Behavior |
|-----------|----------------------------|----------|
| Single-line title | theme-specific; see NP | Truncate with ellipsis **or** marquee if fiction is LCD/segment |
| Segment LCD (Walkman/MD) | character-cell limited | Marquee / page-flip; no soft wrap |
| Winamp title | Main text field width | Classic scroll |
| XMB list | remaining width after icon column | Ellipsis preferred; soft marquee optional |
| Marquee cabinet | full marquee rect | Attract blink OK; keep type large |

## 0.6 What Art Director rejects

- Same NP template with only renamed zones  
- Full-bleed Winamp stretch  
- GB green without shell letterbox  
- EL flood across Walkman faceplate  
- Phone bottom tabs / huge circular play (unless round physical control fiction)  
- Generic rectangle “meters”  
- Cover-art hero where AD Q9 forbids  

---

# 1. Shared screens (define once)

Ниже — **общие шаблоны**. Пер-темные отличия = секции *Deltas* у каждой темы. Не дублировать полный ASCII на каждую тему.

## 1.1 Appearance / theme picker

**Job:** list of 15 themes + **large live preview** of selected skin’s Now Playing silhouette (greyscale wireframe ok in Phase 3).

### Zone map (shared)

```
(0,0) ---- 480 ----+
| STATUS? (optional micro)           y:0–20
|------------------------------------|
| THEME LIST          | PREVIEW      |
| x:12,y:24           | x:200,y:24   |
| w:176,h:200         | w:268,h:200  |
|  ~15 rows~          |  NP thumbnail|
|                     |  silhouette  |
|------------------------------------|
| FOOTER hints: SELECT confirm · O back   y:232–264
+------------------------------------+
```

### Exact-ish rects

| Zone | x | y | w | h |
|------|---|---|---|---|
| List column | 12 | 24 | 176 | 200 |
| List row | 12 | — | 168 | 22–24 |
| Preview frame | 200 | 24 | 268 | 200 |
| Preview inner NP | 208 | 32 | 252 | 168 |
| Footer / hints | 12 | 236 | 456 | 28 |
| Focus caret | 12 | row | 6 | 22 |

### Hierarchy

1. Preview silhouette (selected theme identity)  
2. Focused list row label  
3. Footer control hints  

### Truncation

- List labels: max ~18–20 chars then ellipsis.  
- Preview: no readable full track required — silhouette + fake title lane enough.

### PSP mapping hints

| Control | Action |
|---------|--------|
| ↑↓ | Move theme focus |
| L/R | Page jump optional (5 themes) |
| ✕ or ○ (region) | Apply theme / confirm |
| Other face button | Cancel back to previous |
| SELECT | Open this screen from NP / Empty |
| START | Ignore or confirm (pick one globally later) |

### Risks

- Preview too small → themes become palette chips (FAIL). Keep preview **≥ 250×160** inner.  
- List-only without preview → exhibition fail.  
- Don’t put preview *above* list on 272 height — horizontal split is mandatory.

---

## 1.2 Browse list (artists / albums / tracks)

**Job:** hierarchical music browser. Pattern is **list-forward**; chrome follows theme fiction.

### Zone map (shared skeleton)

```
| HEADER / path crumb                     y:8–28   h:20 |
| OPTIONAL side index (A–Z) or TOC rail   x:8 w:28 |
| MAIN LIST                               x:44–12, y:32, w:424–448, h:176 |
| OPTIONAL now-playing mini strip         y:216–236 |
| FOOTER transport micro / hints          y:240–264 |
```

### Exact-ish rects (default)

| Zone | x | y | w | h |
|------|---|---|---|---|
| Header / crumb | 12 | 8 | 456 | 20 |
| Side rail (optional) | 8 | 32 | 28 | 176 |
| Main list | 44 | 32 | 424 | 176 |
| Row | — | — | list-w | **24** |
| NP mini strip | 12 | 216 | 456 | 20 |
| Footer | 12 | 240 | 456 | 24 |

### Hierarchy

1. Focused row  
2. Header path  
3. Mini NP strip  

### Truncation

- Primary field: ellipsis; secondary metadata (time/bitrate) right-aligned column `w:48–64`.

### PSP mapping hints

| Control | Action |
|---------|--------|
| ↑↓ | Focus row |
| ←→ | Collapse / enter / switch pane (TOC dual) |
| Confirm | Open album / play track |
| Cancel | Up one level |
| □ | Toggle sort / view mode (optional) |
| △ | Jump to Appearance or NP |
| L/R | Letter jump or next category |

### Risks

- Rows &lt; 20 px → unreadable at arm’s length.  
- Dual-pane without gap → overlap. Keep pane gutter ≥ 8 px.

---

## 1.3 EQ panel

**Job:** equalizer as **panel overlay or dedicated mode**, not a second phone app.

### Zone map (shared skeleton)

```
| TITLE "EQ" / presets row                y:12–36 |
| METER / BAND GRAPH                      y:44–160 |
| BAND LABELS                             y:160–180 |
| PRESET LIST or soft keys                y:188–228 |
| FOOTER back hint                        y:236–264 |
```

### Exact-ish rects (default)

| Zone | x | y | w | h |
|------|---|---|---|---|
| Title + presets | 12 | 12 | 456 | 28 |
| Graph / bands | 24 | 44 | 432 | 116 |
| Band labels | 24 | 164 | 432 | 16 |
| Preset / soft keys | 12 | 188 | 456 | 40 |
| Footer | 12 | 236 | 456 | 28 |

### Hierarchy

1. Band graph / needles  
2. Active preset  
3. Hz labels  

### PSP mapping hints

| Control | Action |
|---------|--------|
| ←→ | Select band |
| ↑↓ | Adjust gain |
| L/R | Preset step |
| Confirm | Toggle EQ on/off |
| Cancel | Close panel → NP |

### Risks

- Too many bands for 432 px → merge visually; prefer **8–10** exhibition bands.  
- Don’t cover hero entirely on themes where EQ is a *mode* (Winamp EQ window should be inset companion — see Theme 2 delta).

---

## 1.4 Empty / No music

**Job:** honest idle state of the **same hero object**, not a separate “empty app” screen.

### Zone map (shared)

```
| Same NP housing / void as theme        |
| CENTER message lane                     |
|   "No music" / "Insert library"         |
| Soft CTA: Browse · SELECT themes        |
```

### Exact-ish rects (default message)

| Zone | x | y | w | h |
|------|---|---|---|---|
| Message lane | 80 | 110 | 320 | 36 |
| Secondary hint | 80 | 152 | 320 | 24 |
| CTA row | 80 | 200 | 320 | 36 |

*(Object themes place message **inside** LCD/glass/well — see deltas.)*

### Hierarchy

1. Hero idle silhouette  
2. Empty message  
3. CTA hints  

### PSP mapping hints

| Control | Action |
|---------|--------|
| Confirm | Open Browse |
| SELECT | Appearance |
| △ | Appearance alternate |
| L/R | No-op or theme peek (optional) |

### Risks

- Centering only text on flat void → homebrew. Always keep housing.  
- Don’t use phone illustration empty-states.

---

## 1.5 Loading / buffering (visual intent only)

**Job:** show progress **in meter language of the theme** — not a Material spinner.

| Intent pattern | Use on |
|----------------|--------|
| LED ladder fill / buffer LEDs | Walkman, Cyberpunk, CD ESP |
| Spectrum rise / scrub ghost | Winamp |
| Needle settle / reel spin-up | Cassette |
| CLV spin accelerate | MD / CD |
| Pixel blocks fill | GB / GBC / Arcade |
| Cursor blink + “LOADING…” | DOS / Matrix |
| Wave brighten / icon pulse | XMB / PS2 |
| Swirl pulse | Dreamcast |
| Scope calibrate line | CRT |

### Shared geometry note

Keep loading affordance inside **existing meter or LCD rect** from NP. Do not invent a centered modal card.

### Risks

- Full-screen busy spinner → phone.  
- Blocking &gt; calm motion budget (AD Q11).

---

# 2. Shared Now Playing zone vocabulary

Каждый NP pack заполняет эти имена (уникальными rects):

| Zone ID | Meaning |
|---------|---------|
| **HERO** | Signature object |
| **LCD** | Readout / title·time glass |
| **TRANSPORT** | Play controls affordance |
| **METERS** | Viz / VU / spectrum / rain / etc. |
| **STATUS** | Footer / header / lamps |
| **VOLUME** | Level control fiction |

---

# 3. Per-theme wireframes (1–15)

---

## Theme 1 — Sony Walkman Premium

**Hero:** Mirror cassette door / open window + **EL remote strip** (twin signatures).  
**AD:** EX industrial; EL **remote-only**; no pastel NW; no cover art.

### Now Playing — zone map

```
0        56                    240                         472 480
+--------+---------------------------------------------+----+--+
| EL     |  STATUS wordmark micro                      |VOL |
| REMOTE |  y:8 h:16                                   |DIAL|
| x:8    |---------------------------------------------|x:448|
| y:28   |         HERO cassette door/window           |y:40|
| w:48   |         x:72 y:28 w:352 h:120               |w:24|
| h:180  |---------------------------------------------|h:140|
| LCD    |  LCD strip title/time  x:72 y:156 w:352 h:28|
| on     |---------------------------------------------|
| remote |  METERS LED ladder     x:72 y:188 w:200 h:20|
| +body  |  TRANSPORT keys        x:280 y:188 w:144 h:36     |
| micro  |  STATUS footer hold/batt x:72 y:232 w:352 h:28    |
+--------+---------------------------------------------+----+--+
```

### Exact-ish rects

| Zone | x | y | w | h |
|------|---|---|---|---|
| EL remote strip | 8 | 28 | 48 | 180 |
| Remote LCD digits | 12 | 40 | 40 | 56 |
| Remote key legends | 12 | 100 | 40 | 96 |
| Hero door/window | 72 | 28 | 352 | 120 |
| Under-window LCD strip | 72 | 156 | 352 | 28 |
| LED ladder meters | 72 | 188 | 200 | 20 |
| Transport key row | 280 | 188 | 144 | 36 |
| Volume edge dial | 448 | 40 | 24 | 140 |
| Status footer | 72 | 232 | 352 | 28 |
| Quiet wordmark | 72 | 8 | 120 | 16 |

### Hierarchy

1. Cassette door/window (+ remote silhouette)  
2. Title in LCD strip + time; EL legends  
3. LED ladder, hold/batt, wordmark  

### Truncation / marquee

- Under-window title lane `w:280` of strip; time right `w:64`.  
- Remote may show truncated 6–9 segment chars — marquee allowed.

### PSP mapping hints

| Control | Hint |
|---------|------|
| START / Confirm | Play/pause |
| L/R | Prev/next |
| ←→ | Seek on thin slider fiction (inside door or under strip `y:148` optional 8 px) |
| □ | Mega Bass / EQ |
| △ | Appearance |
| SELECT | Appearance |
| ↑↓ | Volume (or dial) |

### Diff vs shared NP template

**Left remote jewelry column** + **horizontal door hero** — not centered disc, not letterbox shell, not full-bleed void. Twin-hero balance: door larger; remote must stay **≥ 48 px** wide so EL myth reads.

### Risks

- Remote &lt; 40 px → EL becomes noise.  
- Door + full orange wash → AD Q2 FAIL.  
- Title only on remote → unreadable at 1.5 m — keep under-window strip.  
- Dead zone right of door if transport too far — keep keys under right half of door.

### Shared-screen deltas

| Screen | Delta |
|--------|-------|
| Appearance | Preview shows door+remote silhouette |
| Browse | Industrial list; optional remote micro still visible left 32 px collapsed |
| EQ | LED-ladder bands; panel over lower half, door remains top |
| Empty | Door closed/mirror idle; message in LCD strip |
| Loading | LED ladder climbs; remote EL breathe |

---

## Theme 2 — Winamp Classic

**Hero:** Inset **Main window** classic proportions.  
**AD:** Do **not** stretch Main to 480×272.

### Classic aspect target

Main content ≈ **275∶116** family. Scaled for PSP: **`w:330, h:140`** (≈2.36∶1) or tighter **`w:300, h:128`**. Wireframe locks **`330×140`**.

### Now Playing — zone map

```
+----------------------------------------------------------+
|  MAIN WINDOW inset          PLAYLIST strip               |
|  x:16 y:16 w:330 h:140      x:356 y:16 w:108 h:200       |
|  +----------------------+   +--------------------------+ |
|  | titlebar             |   | pledit rows              | |
|  | LCD time+track       |   |                          | |
|  | SPECTRUM             |   |                          | |
|  | posbar / seek        |   +--------------------------+ |
|  | cbuttons + vol       |   EQ teaser OR empty desk    |
|  +----------------------+   x:16 y:168 w:330 h:88      |
|  desk void / Win98 feel                                  |
+----------------------------------------------------------+
```

### Exact-ish rects (inside Main)

| Zone | x | y | w | h |
|------|---|---|---|---|
| Main outer | 16 | 16 | 330 | 140 |
| Titlebar | 16 | 16 | 330 | 14 |
| LCD time | 24 | 34 | 72 | 28 |
| LCD track text | 100 | 34 | 230 | 28 |
| Spectrum | 24 | 66 | 300 | 28 |
| Posbar / seek | 24 | 98 | 300 | 12 |
| Cbuttons | 24 | 114 | 160 | 32 |
| Volume/balance | 200 | 114 | 130 | 32 |
| Playlist | 356 | 16 | 108 | 200 |
| EQ companion (optional mode) | 16 | 168 | 330 | 88 |
| Desk void remainder | 356 | 224 | 108 | 40 |

### Hierarchy

1. Main window chrome object  
2. Spectrum + time  
3. Playlist rows, mono/stereo lamps  

### Truncation / marquee

- Track field inside Main scrolls classic-style within `w:230`.  
- Playlist: 10–12 px font feel; ellipsis.

### PSP mapping hints

| Control | Hint |
|---------|------|
| Confirm / START | Play/pause (cbuttons) |
| ←→ | Seek posbar |
| L/R | Prev/next |
| ↑↓ | Playlist focus when pledit active |
| □ | Toggle EQ companion |
| △ | Skin menu / Appearance |
| SELECT | Appearance |

### Diff vs shared NP template

**Collectible inset**, not faceplate deck. Remainder = playlist + desk void. Transport **inside** Main, not bottom full-width row.

### Risks

- Stretching Main to full frame → AD Q3 FAIL.  
- Playlist wider than Main → Main stops being hero. Cap pledit ~108–140 px.  
- Album art bleed → forbidden.

### Shared-screen deltas

| Screen | Delta |
|--------|-------|
| Appearance | Preview = tiny Main+pledit |
| Browse | Prefer pledit-as-browser; or fullscreen pledit chrome |
| EQ | Classic EQ window inset `330×88` under/ overlapping Main — not fullscreen graph |
| Empty | Main shows “Stopped” LCD; pledit “No files” |
| Loading | Spectrum climbs; title “Buffering…” |

---

## Theme 3 — Cassette Deck

**Hero:** Center cassette well + reels (Technics-serious faceplate).  
**≠ Walkman:** full-width hi-fi faceplate, dual VU, bottom mechanical keys — not pocket remote.

### Now Playing — zone map

```
+----------------------------------------------------------+
| FACEPLATE legends / lamps     x:12 y:8 w:456 h:18        |
| VU-L        CASSETTE WELL           VU-R                 |
| x:12        x:100 y:32 w:280 h:120  x:400                |
| y:32        (reels hero)            y:32                 |
| w:80 h:72                           w:68 h:72            |
| COUNTER x:100 y:156 w:120 h:24                           |
| TRANSPORT full key row x:12 y:196 w:456 h:44             |
| STATUS dolby/source     x:12 y:248 w:456 h:16            |
+----------------------------------------------------------+
```

### Exact-ish rects

| Zone | x | y | w | h |
|------|---|---|---|---|
| Faceplate status lamps | 12 | 8 | 456 | 18 |
| VU left | 12 | 32 | 80 | 72 |
| Cassette well hero | 100 | 32 | 280 | 120 |
| VU right | 392 | 32 | 76 | 72 |
| Mechanical counter / LCD | 100 | 156 | 140 | 28 |
| Mode legends | 260 | 156 | 200 | 28 |
| Transport keys | 12 | 196 | 456 | 44 |
| Volume (integrated end of keys or small) | 400 | 196 | 68 | 44 |
| Footer silkscreen | 12 | 248 | 456 | 16 |

### Hierarchy

1. Cassette well + reels  
2. Dual VU needles + counter  
3. Silkscreen / lamps  

### Truncation / marquee

- Track title: prefer under-well line `x:100 y:156 w:280` if counter shares row — split counter `w:100` + title `w:180`, marquee title.

### PSP mapping hints

| Control | Hint |
|---------|------|
| Confirm / START | Play |
| □ | Stop |
| ←→ | FF/REW feel (seek) |
| L/R | Prev/next program |
| ↑↓ | Input/EQ or volume |
| △ | Appearance |
| SELECT | Appearance |

### Diff vs shared NP template

**Symmetric VU flanking** + **full-width bottom transport** + center well. No left remote column (that’s Walkman). No TOC column (that’s MD).

### Risks

- Well &lt; 240 px wide → loses deck seriousness.  
- VU overlapping well → needles unreadable. Keep ≥ 8 px gap.  
- Empty corners above transport → fill with faceplate grain zones, not widgets.

### Shared-screen deltas

| Screen | Delta |
|--------|-------|
| Browse | Faceplate list; transport mini remains |
| EQ | Replace VU with multi-band needles or keep VU + EQ mode lamps |
| Empty | Open well empty shell; “NO TAPE” in counter window fiction |
| Loading | Reels spin-up; needles rise |

---

## Theme 4 — MiniDisc

**Hero:** MD cartridge in smoked window + body; **TOC column** beside unit.  
**≠ Cassette Deck:** compact portable body left/center, not full faceplate; teal LCD 3-line; jog jewel.

### Now Playing — zone map

```
+----------------------------------------------------------+
|  MD BODY unit                    TOC LIST                 |
|  x:16 y:20 w:280 h:220           x:312 y:20 w:152 h:220   |
|  +----------------------------+  +---------------------+ |
|  | disc window x:40 y:28      |  | TOC header          | |
|  |          w:200 h:72        |  | rows                | |
|  | 3-line LCD x:40 y:108      |  |                     | |
|  |          w:200 h:52        |  |                     | |
|  | jog/rocker x:250 y:108     |  +---------------------+ |
|  | edge keys bottom of body   |                          |
|  +----------------------------+                          |
+----------------------------------------------------------+
```

### Exact-ish rects

| Zone | x | y | w | h |
|------|---|---|---|---|
| MD body housing | 16 | 20 | 280 | 220 |
| Disc window hero | 40 | 28 | 200 | 72 |
| LCD 3-line | 40 | 108 | 200 | 52 |
| Level blocks (in LCD) | 40 | 148 | 120 | 12 |
| Jog / rocker | 248 | 108 | 40 | 72 |
| Edge transport keys | 40 | 172 | 200 | 36 |
| Volume rocker | 248 | 188 | 40 | 40 |
| TOC column | 312 | 20 | 152 | 220 |
| TOC row | 316 | — | 144 | 20 |
| Status micro on body | 40 | 220 | 200 | 16 |

### Hierarchy

1. Disc window (CLV)  
2. LCD title lines + TOC focus  
3. Jog, level blocks  

### Truncation / marquee

- LCD line1: title marquee (~9–16 cells feel, visually `w:200`)  
- LCD line2: artist/time  
- TOC: ellipsis  

### PSP mapping hints

| Control | Hint |
|---------|------|
| ↑↓ | TOC step |
| Confirm | Play focused TOC |
| ←→ / jog | Seek / menu |
| L/R | Prev/next track |
| □ | Edit/mode fiction optional |
| △ / SELECT | Appearance |
| START | Play/pause |

### Diff vs shared NP template

**Portable body + side TOC** dual composition. Hero is cartridge window, not reels-in-deck. Remote EL optional thin strip only if needed — not Walkman’s primary twin hero.

### Risks

- TOC full-bleed → loses MD object. Keep body ≥ 280 px wide.  
- Cover-art large in window → AD Q9 FAIL (tiny label max).  
- LCD &lt; 48 px tall → 3-line unreadable.

### Shared-screen deltas

| Screen | Delta |
|--------|-------|
| Browse | TOC becomes primary; body shrinks to header strip `h:64` with disc window |
| EQ | Level blocks expand to band row under LCD |
| Empty | Window shows blank cartridge ghost; LCD “NO DISC” |
| Loading | CLV spin + TOC “READING” |

---

## Theme 5 — CD Player

**Hero:** Circular Mg disc body + polarizing window (+ laser ring).  
**≠ MD:** round-centered object; cover art **allowed** in disc face; no TOC column required.

### Now Playing — zone map

```
+----------------------------------------------------------+
| optional remote strip L x:8 y:40 w:40 h:160              |
|                                                          |
|          CIRCULAR BODY (hero)                            |
|          center ~(240,120)  outer box x:120 y:16         |
|          w:240 h:200  (circle inscribed)                 |
|          disc window inset ~ x:150 y:40 w:180 h:140      |
|                                                          |
| LCD arc/bar under body x:120 y:208 w:240 h:28            |
| TRANSPORT chrome cluster x:160 y:236 w:160 h:28          |
| VOLUME edge x:448 y:60 w:24 h:120                        |
| ESP icons x:20 y:220 w:80 h:40                           |
+----------------------------------------------------------+
```

### Exact-ish rects

| Zone | x | y | w | h |
|------|---|---|---|---|
| Optional slim remote | 8 | 40 | 40 | 160 |
| Circular body bbox | 120 | 16 | 240 | 200 |
| Disc window (cover OK) | 150 | 40 | 180 | 140 |
| Laser ring (meter) | 150 | 40 | 180 | 140 | *(overlay)* |
| LCD bar | 120 | 208 | 240 | 28 |
| Transport chrome | 160 | 236 | 160 | 28 |
| Volume dial | 448 | 60 | 24 | 120 |
| ESP / G-PROT icons | 16 | 220 | 88 | 40 |
| Buffer LED meter | 120 | 196 | 240 | 10 |

### Hierarchy

1. Spinning disc / circular housing  
2. LCD time+track; laser ring  
3. ESP icons, remote  

### Truncation / marquee

- LCD bar: title marquee `w:160` + time `w:64`.

### PSP mapping hints

| Control | Hint |
|---------|------|
| Confirm / START | Play/pause |
| ←→ | Ring seek |
| L/R | Prev/next |
| □ | ESP / buffer fiction toggle |
| △ / SELECT | Appearance |
| ↑↓ | Volume |

### Diff vs shared NP template

**Radial composition** — meters are ring + buffer, not side VU or left remote-primary. Cover art inside window differentiates from Walkman/Cassette.

### Risks

- Square “CD sticker” on phone layout → FAIL. Keep circular bbox.  
- Laser flood entire screen → AD glow budget break. Ring = thin overlay.  
- LCD only on far edge → title fails arm’s-length; keep bar under body ≥ 28 px.

### Shared-screen deltas

| Screen | Delta |
|--------|-------|
| Browse | List right; mini disc `w:120` left |
| EQ | Buffer/LED bands under LCD |
| Empty | Empty tray / no disc art; “NO DISC” |
| Loading | Buffer LEDs + spin-up |

---

## Theme 6 — GameBoy (DMG)

**Hero:** Centered olive handheld shell letterbox.  
**AD Q4:** gutters = soft void; music UI **only inside LCD glass**.

### Now Playing — zone map

```
(0,0)                                              480
+------ soft void ------+=================+------+
|                       | SHELL ~360×230  |      |
| gutter                | x:60 y:21       |gutter|
|                       | +-------------+ |      |
|                       | | LCD glass   | |      |
|                       | | x:92 y:36   | |      |
|                       | | w:200 h:90  | |      |
|                       | | title/time  | |      |
|                       | | pixel meters| |      |
|                       | +-------------+ |      |
|                       | D-pad  A/B      |      |
|                       | Start Select    |      |
+-----------------------+=================+------+
```

### Exact-ish rects

| Zone | x | y | w | h |
|------|---|---|---|---|
| Soft void L | 0 | 0 | 60 | 272 |
| Soft void R | 420 | 0 | 60 | 272 |
| Shell outer | 60 | 21 | 360 | 230 |
| Bezel ring (frame) | 84 | 32 | 216 | 110 |
| **LCD glass (all UI)** | 92 | 40 | 200 | 90 |
| Title lane in LCD | 96 | 44 | 192 | 20 |
| Time / status in LCD | 96 | 66 | 192 | 16 |
| Pixel meters in LCD | 96 | 86 | 192 | 36 |
| D-pad affordance | 100 | 160 | 64 | 64 |
| A/B affordance | 280 | 168 | 72 | 56 |
| Start/Select | 160 | 220 | 120 | 20 |
| Power LED | 300 | 36 | 8 | 8 |

### Hierarchy

1. Shell silhouette in letterbox  
2. LCD contents (title, meters)  
3. Physical buttons as mapped affordances  

### Truncation / marquee

- Strictly inside LCD; chunky bitmap; marquee on title lane `h:20`.

### PSP mapping hints

| Control | Hint |
|---------|------|
| D-pad | UI nav / seek fiction on LCD menus |
| ✕/○ mapped to A/B | Confirm / back |
| START/SELECT | Play/pause · menu (match physical labels) |
| L/R | Prev/next |
| △ | Appearance (even if not on shell — exhibition shortcut) |

### Diff vs shared NP template

**Portrait object in landscape** with mandatory gutters. No full-width faceplate. Transport = shell buttons, not bottom deck row.

### Risks

- UI leaking into gutters → AD Q4 FAIL.  
- LCD &lt; 180×80 → unreadability.  
- Ignoring bezel thickness (LCD flush to shell edge) → cheap homebrew.

### Shared-screen deltas

| Screen | Delta |
|--------|-------|
| Appearance | Preview = shell letterbox |
| Browse | List **inside LCD** only; shell remains |
| EQ | Pixel bars inside LCD |
| Empty | LCD “NO CART” / “NO MUS” |
| Loading | Pixel blocks fill |

---

## Theme 7 — GameBoy Color

**Hero:** Translucent GBC shell + **dark bezel** + color LCD stage.  
**≠ DMG compositionally:** darker/larger bezel ratio; color meter blocks; same letterbox rule but shell proportions shift slightly taller LCD.

### Now Playing — zone map

Same letterbox contract as Theme 6, with deltas:

| Zone | x | y | w | h | Notes |
|------|---|---|---|---|-------|
| Shell outer | 60 | 18 | 360 | 236 | Slightly taller shell |
| Dark bezel | 78 | 28 | 228 | 118 | **Thicker / darker** vs DMG |
| Color LCD | 94 | 40 | 196 | 94 | UI only here |
| Title | 98 | 44 | 188 | 22 | |
| Time | 98 | 68 | 188 | 16 | |
| Color pixel meters | 98 | 88 | 188 | 40 | |
| Colored A/B | 276 | 164 | 80 | 60 | Larger jewel read |
| D-pad | 96 | 160 | 64 | 64 | |
| Inner structure hint | shell only | — | — | — | Non-interactive silhouette |

### Hierarchy

1. Translucent shell + dark bezel silhouette  
2. Color LCD content  
3. Colored buttons  

### Truncation / marquee

Same as DMG — inside LCD only.

### PSP mapping hints

Same family as Theme 6.

### Diff vs Theme 6 / shared template

Must **not** be hue-swap only: bezel darker/thicker, LCD taller, color meters, translucency edge as composition (inner hint), A/B larger. Gutters still void.

### Risks

- Identical rects to DMG + purple note → AD moodboard anti-copy FAIL.  
- UI chrome outside shell.

### Shared-screen deltas

Same pattern as DMG; empty copy “NO MUS”; meters = color bars.

---

## Theme 8 — DOS

**Hero:** Phosphor text matrix (optional beige PC frame).  
**≠ Matrix:** full text utility layout with header/path + dual pane + ASCII spectrum — not cinematic rain field.

### Now Playing — zone map

```
+----------------------------------------------------------+
| optional beige frame inset content x:16 y:12 w:448 h:248 |
| STATUS line path/time/mode         y:16 h:16             |
| DUAL PANE:                         y:36 h:140            |
|   left playlist w:220   | right now/file info w:220      |
| ASCII SPECTRUM                     y:184 h:40            |
| FUNCTION key legends               y:228 h:24            |
+----------------------------------------------------------+
```

### Exact-ish rects

| Zone | x | y | w | h |
|------|---|---|---|---|
| Optional case frame | 8 | 8 | 464 | 256 |
| Phosphor content | 16 | 12 | 448 | 248 |
| Status line | 20 | 16 | 440 | 16 |
| Left pane (list) | 20 | 36 | 220 | 140 |
| Right pane (NP info) | 248 | 36 | 208 | 140 |
| Title block (right) | 252 | 40 | 200 | 48 |
| Time / mode | 252 | 92 | 200 | 32 |
| ASCII spectrum | 20 | 184 | 440 | 40 |
| Transport as F-key row | 20 | 228 | 440 | 24 |
| Volume as text “VOL ##” | 360 | 92 | 88 | 16 |

### Hierarchy

1. Phosphor field / dual pane  
2. ASCII spectrum + title  
3. F-key legends  

### Truncation / marquee

- Fixed columns; truncate with `…` or scroll line; high contrast mandatory (AD Q10).

### PSP mapping hints

| Control | Hint |
|---------|------|
| ↑↓←→ | Pane navigation |
| Confirm | Play |
| L/R | Prev/next |
| □ | Toggle pane focus |
| △ / SELECT | Appearance |
| START | Play/pause |

### Diff vs shared NP template

**Text matrix is the object** — no plastic transport row, no circular hero. Meters = characters.

### Risks

- Low-contrast green-on-green → AD Q10 FAIL.  
- Rounded cards → anti-copy FAIL.  
- No spectrum → loses living meter.

### Shared-screen deltas

| Screen | Delta |
|--------|-------|
| Browse | Full Norton-like dual pane |
| EQ | ASCII bar chart “EQ” mode |
| Empty | Dir listing empty + message |
| Loading | `LOADING...` + cursor blink |

---

## Theme 9 — Matrix

**Hero:** Full-field code rain + **center decode column / glass terminal**.  
**≠ DOS:** rain atmosphere primary; sparse terminal inset — not dual-pane file manager.  
**AD Q6:** homage language; no film wordmarks.

### Now Playing — zone map

```
+----------------------------------------------------------+
| RAIN FIELD full-bleed (capped columns)                   |
|                                                          |
|      DECODE COLUMN / terminal inset                      |
|      x:140 y:48 w:200 h:140                              |
|      +----------------------------------------------+   |
|      | track decode                                  |   |
|      | time                                          |   |
|      | LED progress ticks                            |   |
|      +----------------------------------------------+   |
| sparse command legends y:210                             |
| STATUS micro headers corners                             |
+----------------------------------------------------------+
```

### Exact-ish rects

| Zone | x | y | w | h |
|------|---|---|---|---|
| Rain field (hero atmosphere) | 0 | 0 | 480 | 272 |
| Decode terminal inset | 140 | 48 | 200 | 140 |
| Title decode lane | 148 | 60 | 184 | 36 |
| Time lane | 148 | 104 | 184 | 28 |
| LED progress / meter | 148 | 144 | 184 | 24 |
| Transport CLI legends | 120 | 210 | 240 | 28 |
| Volume as tick row | 148 | 172 | 184 | 12 |
| Corner status | 12 | 8 | 120 | 16 |

### Hierarchy

1. Decode column readable against rain  
2. Rain pulse as viz  
3. CLI legends  

### Truncation / marquee

- Decode title: marquee inside terminal `w:184`.  
- Rain never carries primary title.

### PSP mapping hints

| Control | Hint |
|---------|------|
| Confirm / START | Play/pause |
| L/R | Prev/next |
| ←→ | Seek ticks |
| △ / SELECT | Appearance |
| □ | Toggle rain density (calm cap) |

### Diff vs shared NP template

**Cinema void + floating terminal**, not housing faceplate. Transport is faint CLI, not rubber keys.

### Risks

- Uncapped rain → ADHD / GPU (AD Q11). Wireframe implies **column cap**.  
- Terminal too small (&lt; 160×100) → title fails.  
- Trademark logos → AD Q6 FAIL.  
- Becoming DOS dual-pane → loses Matrix identity.

### Shared-screen deltas

| Screen | Delta |
|--------|-------|
| Browse | Terminal list overlay; rain dimmed |
| EQ | LED ticks as bands |
| Empty | Rain only + “NO SIGNAL” decode |
| Loading | Decode flicker 1–2 frames + ticks |

---

## Theme 10 — Cyberpunk

**Hero:** Carbon HUD frame + neon edge + **horizon grid lower third**.  
**≠ Matrix:** hardware faceplate + cyan/magenta dual accent + flanking LED ladders.  
**AD Q7:** meters-first; no dolphin BGV.

### Now Playing — zone map

```
+----------------------------------------------------------+
| HUD corners status         x:12/360 y:8 w:100 h:20       |
| LED ladder L               OEL TITLE WELL        LED R   |
| x:16 y:40 w:36 h:120       x:68 y:36 w:344 h:88          |
|                            TIME + state                   |
| TRANSPORT soft-keys        x:68 y:132 w:344 h:36         |
| HORIZON GRID (hero lower)  x:0 y:180 w:480 h:92          |
| VOLUME as neon stub        x:420 y:40 w:40 h:100         |
+----------------------------------------------------------+
```

### Exact-ish rects

| Zone | x | y | w | h |
|------|---|---|---|---|
| Carbon HUD frame (inset) | 8 | 8 | 464 | 256 |
| Status corners L | 12 | 8 | 100 | 20 |
| Status corners R | 368 | 8 | 100 | 20 |
| Neon LED ladder L | 16 | 40 | 36 | 120 |
| Neon LED ladder R | 428 | 40 | 36 | 120 |
| OEL title/time well | 68 | 36 | 344 | 88 |
| Title lane | 76 | 44 | 328 | 32 |
| Time / mode | 76 | 80 | 200 | 28 |
| Soft-key transport | 68 | 132 | 344 | 36 |
| Horizon grid hero | 0 | 180 | 480 | 92 |
| Volume stub | 420 | 40 | 40 | 100 |
| Optional tiny media inset | 300 | 80 | 96 | 40 |

### Hierarchy

1. HUD frame + OEL well (title)  
2. LED ladders + grid motion  
3. Corner stencil status  

### Truncation / marquee

- Title in OEL `w:328` marquee; keep high contrast.

### PSP mapping hints

| Control | Hint |
|---------|------|
| Confirm / START | Play/pause |
| ←→ | Soft-key focus / seek |
| L/R | Prev/next |
| ↑↓ | Volume / menu |
| □ | EQ |
| △ / SELECT | Appearance |

### Diff vs shared NP template

**Lower-third perspective grid** as spatial hero partner; meters = flanking neon ladders. Not rain-void, not beige CRT.

### Risks

- Grid louder than title → AD Q7/Q11 FAIL.  
- Glass cards → anti-copy FAIL.  
- Magenta flood → keep alert-only.

### Shared-screen deltas

| Screen | Delta |
|--------|-------|
| Browse | HUD list; grid dimmed |
| EQ | Ladder bands fullscreen width under OEL |
| Empty | “NO DATA” OEL; grid idle |
| Loading | Ladder fill + 1–2 frame glitch max |

---

## Theme 11 — CRT TV

**Hero:** Full-frame TV cabinet + inset CRT face.  
**≠ Arcade:** living-room ABS cabinet + speaker fabric + OSD — not marquee fighter cabinet.

### Now Playing — zone map

```
+----------------------------------------------------------+
| CABINET outer = full frame                               |
| speaker L        CRT INSET             speaker R         |
| x:8 w:56         x:72 y:24 w:336 h:176 x:416 w:56        |
|                  OSD lower third on CRT                  |
|                  scope meters on CRT                     |
| FRONT PANEL buttons x:72 y:212 w:336 h:36                |
| POWER LED x:420 y:220                                    |
+----------------------------------------------------------+
```

### Exact-ish rects

| Zone | x | y | w | h |
|------|---|---|---|---|
| Cabinet outer | 0 | 0 | 480 | 272 |
| Speaker L | 8 | 40 | 56 | 160 |
| Speaker R | 416 | 40 | 56 | 160 |
| CRT bezel | 68 | 20 | 344 | 188 |
| CRT glass | 80 | 32 | 320 | 164 |
| Scope / waveform (meters) | 96 | 48 | 288 | 80 |
| OSD title/time | 96 | 140 | 288 | 44 |
| Front panel transport | 72 | 212 | 336 | 36 |
| Volume (panel wheel) | 360 | 212 | 48 | 36 |
| Power LED | 424 | 220 | 12 | 12 |
| Channel/status OSD micro | 96 | 36 | 120 | 16 |

### Hierarchy

1. Cabinet + CRT inset silhouette  
2. OSD title + scope  
3. Panel buttons, power LED  

### Truncation / marquee

- OSD title marquee inside `w:288`; service fonts oversized vs real TV for expo.

### PSP mapping hints

| Control | Hint |
|---------|------|
| Confirm / START | Play/pause |
| ←→ | Seek / channel fiction |
| L/R | Prev/next |
| ↑↓ | Volume |
| □ | Scope mode |
| △ / SELECT | Appearance |

### Diff vs shared NP template

**Appliance cabinet with L/R speakers**; meters = scope on glass; transport = front panel, not shell D-pad / not marquee.

### Risks

- Thin modern bezel → anti-copy FAIL. Keep CRT inset margins ≥ 12 px bezel.  
- Full-bleed black without cabinet → FAIL.  
- Tiny channel fonts → AD Q10.

### Shared-screen deltas

| Screen | Delta |
|--------|-------|
| Browse | OSD list on CRT |
| EQ | Service bars on CRT |
| Empty | Blue-ish void / “NO INPUT” OSD |
| Loading | Scope calibrate + degauss rare on theme enter only |

---

## Theme 12 — PS2 Browser

**Hero:** Deep navy void + **floating glass command tiles** + sparse cubes.  
**≠ XMB:** tiles/panels + ribbon scope, not cross-bar icon highway.  
**≠ iOS:** EE blue depth fog language.

### Now Playing — zone map

```
+----------------------------------------------------------+
| CLOCK ORB motif optional x:200 y:8 w:80 h:36             |
|                                                          |
| GLASS COMMAND BOX (title/time) x:90 y:56 w:300 h:88      |
|                                                          |
| TRANSPORT tiles row x:60 y:160 w:360 h:44                |
|   [ |<< ] [ |> ] [ >>| ] [ EQ ]                          |
| RIBBON SCOPE meters x:90 y:212 w:300 h:24                |
| LIST peek lower-right x:320 y:240 w:140 h:24             |
| ambient cube sprites (non-hit) sparse                    |
+----------------------------------------------------------+
```

### Exact-ish rects

| Zone | x | y | w | h |
|------|---|---|---|---|
| Navy void field | 0 | 0 | 480 | 272 |
| Clock orb | 200 | 6 | 80 | 32 |
| Glass command box | 90 | 56 | 300 | 88 |
| Title | 100 | 64 | 280 | 36 |
| Time / state | 100 | 108 | 180 | 24 |
| Transport tiles | 60 | 160 | 360 | 44 |
| Single tile | — | 160 | 80 | 44 |
| Ribbon scope | 90 | 212 | 300 | 24 |
| Volume glass stub | 300 | 108 | 80 | 24 |
| List peek | 320 | 240 | 140 | 24 |
| Cube ambience | scattered | | ≤24 | ≤24 |

### Hierarchy

1. Glass command box + tile row  
2. Ribbon scope  
3. Clock / list peek / cubes  

### Truncation / marquee

- Title in glass `w:280`; ellipsis preferred (BIOS calm).

### PSP mapping hints

| Control | Hint |
|---------|------|
| ←→ | Tile focus |
| Confirm | Activate tile |
| L/R | Prev/next track |
| ↑↓ | Volume / list peek |
| △ / SELECT | Appearance |
| START | Play/pause |

### Diff vs shared NP template

**Floating tiles on deep void** — no metal faceplate, no cabinet. Meters = soft ribbon, not LED ladders (Cyberpunk) or waves (XMB).

### Risks

- Too many panels → mud. Cap primary surfaces to command box + 4 tiles + ribbon.  
- White frosted cards → anti-copy FAIL.  
- Cubes competing with title → keep sparse.

### Shared-screen deltas

| Screen | Delta |
|--------|-------|
| Browse | Memory-card-like glass list |
| EQ | Ribbon becomes multi-node spline |
| Empty | “Insert music” glass dialog |
| Loading | Tile glide + cube drift |

---

## Theme 13 — PSP XMB

**Hero:** Wave ribbons + **cross-bar** (icons travel, cursor doesn’t).  
**AD Q5:** “inspired by XMB language” — not fake firmware claim.  
**≠ PS2:** cross-bar + vertical list DNA, waves full-bleed.

### Now Playing — zone map

```
+----------------------------------------------------------+
| STATUS clock/battery strip x:12 y:6 w:456 h:16           |
|                                                          |
| CATEGORY ICONS cross-bar (horizontal)                    |
|   y:48 h:56   icons center on focus x~240                |
|                                                          |
| VERTICAL ITEMS (Music detail)                            |
|   x:160 y:110 w:280 h:100                                |
|   focus row center; neighbors dim                        |
|                                                          |
| SOFT WAVEFORM meters x:40 y:220 w:400 h:28               |
| waves BGV full-bleed behind                              |
+----------------------------------------------------------+
```

### Exact-ish rects

| Zone | x | y | w | h |
|------|---|---|---|---|
| Wave BGV (hero atmosphere) | 0 | 0 | 480 | 272 |
| Status strip | 12 | 6 | 456 | 16 |
| Category cross-bar lane | 0 | 48 | 480 | 56 |
| Focus icon cell | 208 | 48 | 64 | 56 |
| Neighbor icons | ±72 / ±144 from focus | 48 | 56 | 56 |
| Vertical list column | 160 | 110 | 280 | 100 |
| Focus list row | 160 | 142 | 280 | 28 |
| Soft waveform | 40 | 220 | 400 | 28 |
| Optional small cover | 40 | 120 | 96 | 72 |
| Volume | via system-like — no FAB; L+↑ fiction optional | | | |

### Hierarchy

1. Focus icon + focus list row + waves silhouette  
2. Waveform + status clock  
3. Dim neighbors  

### Truncation / marquee

- Focus row: ellipsis; arm’s-length size priority over authentic micro type.

### PSP mapping hints

| Control | Hint |
|---------|------|
| ←→ | Category icons travel |
| ↑↓ | Vertical list |
| Confirm | Play / open |
| Cancel | Back category |
| L/R | Category jump or track skip when in NP overlay |
| START | NP overlay / play-pause (pick one; document in impl) |
| SELECT / △ | Appearance (custom, not “official settings”) |

### Diff vs shared NP template

**Cross-bar navigation is the chrome**; no deck transport row. Now Playing may be Music detail overlay respecting XMB spacing — not bottom tabs.

### Risks

- Bottom tab bar → FAIL.  
- Official Sony wordmark / “Official Theme” → AD Q5 FAIL.  
- ADHD wave distortion → AD Q11.  
- Identical to PS2 glass tiles → FAIL (must keep cross-bar).

### Shared-screen deltas

| Screen | Delta |
|--------|-------|
| Appearance | Must read as custom gallery, not System Settings clone |
| Browse | Vertical Music list + category bar |
| EQ | Overlay panel; waves stay |
| Empty | Empty Music category message at focus |
| Loading | Icon pulse + waveform warm-up |

---

## Theme 14 — Dreamcast

**Hero:** **Orange swirl field** dominant + white ABS panels + dark info well.  
**AD Q12:** orange default all locales — not tiny logo.

### Now Playing — zone map

```
+----------------------------------------------------------+
| SWIRL FIELD dominant background                          |
|                                                          |
| WHITE PANEL header strip x:24 y:16 w:432 h:36            |
|                                                          |
| DARK INFO WELL (LCD) x:80 y:70 w:320 h:96                |
|   title / time                                           |
|                                                          |
| ORANGE ENERGY METER x:80 y:178 w:320 h:28                |
|                                                          |
| BIOS-like soft buttons / rows x:80 y:216 w:320 h:40      |
+----------------------------------------------------------+
```

### Exact-ish rects

| Zone | x | y | w | h |
|------|---|---|---|---|
| Swirl field (hero) | 0 | 0 | 480 | 272 |
| White ABS panel top | 24 | 16 | 432 | 36 |
| Dark info well | 80 | 70 | 320 | 96 |
| Title lane | 92 | 82 | 296 | 36 |
| Time / state | 92 | 124 | 180 | 28 |
| Energy meter | 80 | 178 | 320 | 28 |
| Transport soft rows | 80 | 216 | 320 | 40 |
| Volume as energy side tick | 404 | 70 | 40 | 96 |
| Seam legends | 24 | 248 | 200 | 16 |

### Hierarchy

1. Swirl field + dark well title  
2. Energy meter  
3. Soft BIOS buttons / seams  

### Truncation / marquee

- Title in dark well `w:296`; friendly size; ellipsis OK.

### PSP mapping hints

| Control | Hint |
|---------|------|
| ↑↓ | Soft menu rows |
| Confirm / START | Play/pause |
| L/R | Prev/next |
| ←→ | Seek / row |
| △ / SELECT | Appearance |
| □ | EQ |

### Diff vs shared NP template

**Background-as-hero (swirl)** with inset well — opposite of Winamp (chrome inset on desk) and opposite of Walkman (physical door). White panels frame but must not become Material cards.

### Risks

- Swirl as 32×32 corner mark → AD Q12 FAIL. Field must dominate.  
- EU blue default → FAIL.  
- Flat white cards covering swirl → FAIL.

### Shared-screen deltas

| Screen | Delta |
|--------|-------|
| Browse | BIOS list rows on swirl |
| EQ | Energy meter multi-segment |
| Empty | Well “No Disc” fiction |
| Loading | Swirl pulse + meter fill |

---

## Theme 15 — Arcade

**Hero:** Lit marquee + smoked-glass bezel cabinet + control panel.  
**≠ CRT TV:** marquee type as title; control panel colored buttons; playfield behind glass — not living-room speakers OSD.

### Now Playing — zone map

```
+----------------------------------------------------------+
| MARQUEE (title/artist) x:40 y:4 w:400 h:40               |
| CABINET black frame                                      |
| BEZEL + smoked glass playfield                           |
|   x:60 y:48 w:360 h:140                                  |
|   pixel viz + time inside glass                          |
| NEON LEVEL METERS sides of bezel                         |
| CONTROL PANEL x:40 y:200 w:400 h:56                      |
|   colored buttons = transport                            |
| insert-coin flavor micro (not gate) x:160 y:252          |
+----------------------------------------------------------+
```

### Exact-ish rects

| Zone | x | y | w | h |
|------|---|---|---|---|
| Cabinet outer | 0 | 0 | 480 | 272 |
| Marquee | 40 | 4 | 400 | 40 |
| Bezel outer | 52 | 44 | 376 | 152 |
| Smoked playfield glass | 68 | 56 | 344 | 128 |
| Title duplicate? | Prefer marquee only; time in glass `x:80 y:64 w:120 h:20` |
| Pixel viz / meters in glass | 80 | 90 | 320 | 72 |
| Neon level L | 40 | 56 | 20 | 128 |
| Neon level R | 420 | 56 | 20 | 128 |
| Control panel | 40 | 200 | 400 | 56 |
| Transport buttons cluster | 120 | 208 | 240 | 40 |
| Volume (side panel stick) | 40 | 208 | 64 | 40 |
| Attract micro | 160 | 252 | 160 | 14 |

### Hierarchy

1. Marquee + cabinet bezel silhouette  
2. Playfield viz + panel buttons  
3. Attract micro / coin door hint  

### Truncation / marquee

- Marquee is primary title lane — large type; scroll if needed.  
- Avoid tiny playfield title competing with marquee.

### PSP mapping hints

| Control | Hint |
|---------|------|
| Confirm / START | Play (panel button) |
| □ / face buttons | Match colored panel |
| L/R | Prev/next |
| ←→ | Seek |
| △ / SELECT | Appearance |
| Attract blink | Visual only |

### Diff vs shared NP template

**Vertical cabinet stack:** marquee → glass → panel. Pixels always behind glass. Not beige TV, not XMB void.

### Risks

- Full-bleed pixels without bezel → FAIL.  
- Neon CSS border around empty list → FAIL.  
- Insert-coin as paywall → forbidden.  
- Marquee &lt; 36 px tall → weak hero type.

### Shared-screen deltas

| Screen | Delta |
|--------|-------|
| Browse | Playfield list behind glass; marquee = section name |
| EQ | Neon levels become bands |
| Empty | Attract “NO TUNES”; empty playfield |
| Loading | Neon meters fill; attract blink |

---

# 4. Cross-theme NP uniqueness matrix (QA)

| # | Theme | Composition family | Hero geometry | Meter geometry | Transport geometry |
|---|--------|-------------------|---------------|----------------|--------------------|
| 1 | Walkman | Pocket EX deck | Door + left remote | LED ladder under | Compact key cluster |
| 2 | Winamp | Desktop collectible | Inset Main 330×140 | Spectrum in Main | Cbuttons in Main |
| 3 | Cassette | Hi-Fi faceplate | Center well | Dual VU flanks | Full-width keys |
| 4 | MiniDisc | Portable + TOC | Window left body | LCD blocks + spin | Jog + edge keys |
| 5 | CD | Radial Discman | Center circle | Laser ring + buffer | Chrome cluster |
| 6 | DMG | Letterbox handheld | Shell 360×230 | Pixel in LCD | D-pad / A·B |
| 7 | GBC | Letterbox handheld+ | Dark bezel shell | Color pixels | Colored A·B |
| 8 | DOS | CRT text utility | Phosphor matrix | ASCII spectrum | F-key legends |
| 9 | Matrix | Cinema rain | Decode column | Rain + LED ticks | CLI legends |
| 10 | Cyberpunk | HUD head-unit | OEL + grid | Neon ladders | Soft-keys |
| 11 | CRT TV | Living-room set | Cabinet+CRT | Scope on glass | Front panel |
| 12 | PS2 | EE glass void | Command box+tiles | Ribbon scope | Floating tiles |
| 13 | XMB | Cross-bar OS lang | Waves+icons | Soft waveform | Icons+list |
| 14 | Dreamcast | Swirl appliance | Swirl+info well | Energy meter | BIOS soft rows |
| 15 | Arcade | Cabinet stack | Marquee+bezel | Neon sides+pixels | Control panel |

**Self-QA rule:** If any two rows share the same geometry family *and* hero geometry without a listed delta, revise before Phase 4.

---

# 5. AD lock conflict resolutions (this phase)

| Topic | Resolution in wireframes |
|-------|--------------------------|
| Walkman pastel repo heritage | Ignored; EX door+remote layout only (Q1) |
| EL glow desire vs myth | Remote strip geometry locked; no faceplate flood (Q2) |
| Winamp fullscreen temptation | Main fixed inset ≤330×140; pledit remainder (Q3) |
| GB rotate idea | Rejected; letterbox shell + void gutters (Q4) |
| XMB “make it official” | Status/cross-bar inspired-by; Appearance ≠ System Settings clone (Q5) |
| Matrix IP | Zones named decode/rain; no logo zones (Q6) |
| Head-unit BGV kitsch | Cyberpunk grid is sparse lower-third, subordinate to OEL (Q7) |
| Shared layout kit risk | Uniqueness matrix + per-theme rects; shared is screens 1.1–1.5 only (Q8) |
| Cover art | Only CD (disc window) + optional small PS2/XMB/CRT/Cyber/DC; denied elsewhere (Q9) |
| Tiny authentic type | Row heights / LCD lanes oversized vs museum-real (Q10) |
| Motion | Loading intents map to existing meters; no modal spinners (Q11) |
| Dreamcast EU blue | Swirl field dominant orange; no locale toggle in wireframe (Q12) |

**GBC vs DMG clone risk:** Explicit rect deltas (bezel, LCD height, A/B size) — Concept Art must paint translucency/bezel difference, not recolor only.

---

# 6. Phase clearance recommendation

| Gate | Recommendation |
|------|----------------|
| **Phase 3 Wireframes complete?** | **YES** — this pack |
| **Cleared for Phase 4 Concept Art?** | **YES** — recommend AD greenlight |
| **Implementation / `psp/src/**`?** | **NO** — still forbidden |

**Concept Art must:** obey zone rects ±4–8 px; preserve uniqueness matrix; paint materials from Phase 2; keep AD locks.

**Wireframe Team sign-off:** Composition packs ready for Art Director review → Phase 4.
