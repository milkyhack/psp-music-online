# PHASE 1 — Device & Lineage Research Pack

**Проект / Project:** Music Online PSP — AAA exhibition bar music player UI (Sony 2006 era, commercial-grade)  
**Роль / Role:** Research Team only — **NO CODE, NO IMPLEMENTATION**  
**Дата / Date:** 2026-08-03  
**Экран / Frame:** 480×272 (PSP native), soft GPU, limited RAM  

---

## 0. Mandate & Philosophy / Мандат

Это должно ощущаться как **официальный Sony industrial object** + **культовые Winamp collectible skins**, а не как «цветные прямоугольники homebrew».

### Primary material language (обязательный словарь)

| Material | Feel | Typical use in themes |
|----------|------|------------------------|
| Brushed metal / aluminum | directional grain, specular streaks | Walkman, Cassette, MiniDisc, CD, Head-unit |
| Matte ABS / rubberized plastic | soft scatter, fingerprint-resistant | Walkman grips, GameBoy shell, CRT cabinet |
| Smoked / tinted glass | darkens blacks, slight reflection | Arcade bezel, Discman window, LCD cover |
| Acrylic / polycarbonate lens | hard highlight edge, scratch catch | LCD windows, remote EL panels |
| Carbon / textured polymer | fine weave or orange-peel | Cyberpunk / late Hi-Fi faceplates |
| Anodized aluminum | colored metal, soft specular | MiniDisc colored shells, premium Walkman |
| Soft rubber (TPE) | dark edges, mute specular | Walkman EX5 edge rubber, transport buttons |

### Forbidden as primary language

- Flat Material / iOS / Android card stacks  
- Empty minimalism (large empty fields of solid color as the “design”)  
- Palette-swap of rectangles without housing, bezel, glass, depth  
- Neon glow as the *only* material (allowed as accent, not as body)

### Pixel-theme tension (GameBoy / Arcade / DOS)

Эти темы **исторически пиксельные**. Правильный путь:  
**аутентичный device language** (корпус, безель, стекло LCD, кнопки, part-lines) +  
**elevation beyond cheap homebrew squares** (объём, пластик, стекло, не только 4-color swap).

---

## 1. Repo baseline (read-only) / Что уже есть в репо

Краткий осмотр `psp/src/theme.h`, `theme.c`, фрагментов `ui.c` (без правок):

| Есть сейчас | Чего нет (research должен заменить) |
|-------------|-------------------------------------|
| 15 именованных skins | Материал / объём / specular / glass as design system |
| Палитры RGB + `chrome_hi/lo` | Реальные finish vocab (brushed, matte, smoked) |
| Enum compositions (`COMP_*`) | Пропорции корпуса vs LCD vs transport на 480×272 |
| Viz modes (`VIZ_*`) | Signature motion specs (reels, disc CLV, XMB wave cadence) |
| Упрощённые shell fills в `ui.c` | Bezel / housing / LCD glass / rubber / part-lines |

**Вывод для Concept Team:** текущие skins = **skeleton palettes**. Research pack ниже — критерий «collectible AAA», против которого Phase 2 moodboards и будущие спеки будут оцениваться.

---

## 2. Inspirational lineages / Линейки вдохновения

### 2.1 Sony Walkman (cassette era, esp. black plastic + EL remote)

**Key models:** WM-EX808 / EX808HG (1993, Mg-Al alloy “slim strong”), WM-EX90 “Adult Walkman” (1991, large LCD + EL remote), WM-EX5 (1996 anniversary, mirror cassette door, grey rubber edges, RM-WM71EL blue EL remote), late EX921 / EX651 (jog lever, backlit remote).  

**Materials:** magnesium-mixed aluminum, chrome-plated / mirror lids, soft grey rubber edge bands, matte black or silver plastic frames, gumstick battery aesthetic.  
**Displays:** small body LCDs; **signature orange/blue EL backlight lives mostly on remotes** (not always on the deck itself) — critical myth-bust: “orange backlight Walkman” often = remote EL, not faceplate.  
**Layout DNA:** horizontal cassette window or closed mirror door; transport clustered or hidden under rear door; volume as edge dial; remote as secondary “UI skin”.  
**Sources:** https://www.sony.com/en/SonyInfo/design/gallery/WM-EX808/ · https://walkmancentral.com/products/wm-ex5 · https://walkman.land/sony/wm-ex90 · https://obsoletesony.substack.com/p/history-of-the-walkman-1979-2004

### 2.2 Sony PSP / XMB

**Key facts:** native **480×272**; horizontal category icons + vertical lists; **icons move, cursor does not**; white focus glow pulse; background **waves / wavy lines**; monthly color shifts (firmware eras); official theme asset sizes (wallpaper 480×272 BMP, icons 32–64 px). Graphics tech lineage: Q-Games involvement (visualizers / waves). Emmy 2006 for XMB presentation tech.  
**Feel:** black void + translucent wave ribbons + soft specular glass icons + orange/amber selection (common custom theme accent; stock colors vary by month/theme).  
**Sources:** https://en.wikipedia.org/wiki/XrossMediaBar · https://cdn.us.playstation.com/pscomauth/groups/public/documents/webasset/ps_custom_theme-english_pp.pdf · https://blog.playstation.com/2008/06/20/how-to-make-your-own-psp-themes/

### 2.3 Sony MiniDisc / Discman / Hi-Fi

**MiniDisc:** MZ-R90/R91 (Mg press-forged), MZ-R909 (2001 Al body, 3-line LCD, horizontal grain), MZ-N1 (Mg, curved edges, rocker bar, chrome logos). Disc window + CLV spin metaphor (350–2800 rpm historically). Teal/cyan LCD segments common in marketing; R90-era blue indicators vs later black text.  
**Discman / CD Walkman:** D-E01 (1999 anniversary — chrome buttons, polarizing window coating, slide-in loading), D-EJ2000 (ultra-slim Mg full-circle body, EL slim-stick remote), D-NE series.  
**Hi-Fi / cassette decks:** Technics RS-series metal faceplates, peak/VU switchable meters, brushed aluminum + black acrylic.  
**Sources:** https://www.minidisc.org/brian_youn/MZR909/page1.html · https://www.minidisc.org/brian_youn/mzn1/page2.html · https://www.sony.com/en/SonyInfo/design/gallery/D-E01/ · http://minidisc.org/mzr90/

### 2.4 Winamp classic + famous skins / AIMP / foobar / WMP

**Winamp Classic anatomy (fixed chrome, paint-over bitmaps):** Main / Titlebar / Cbuttons / Volume / Balance / Posbar / Numbers / Text / Monoster / Playpaus / Shufrep; EQ (`Eqmain.bmp`); Playlist (`Pledit.bmp`). No freeform layout in classic skins — **collectible value is texture craft inside rigid slots**. Famous language: green LCD digits, orange seek, grey bevelled metal, spectrum analyzer bars.  
**Era cousins:** WMP9/11 glass/gloss skins (still remade for AIMP); AIMP Classic matte horizontal; foobar2000 Columns UI modular panels (more “pro tool” than object).  
**Sources:** http://wiki.winamp.com/wiki/Creating_Classic_Skins · https://winampskins.neocities.org/base · http://fileformats.archiveteam.org/wiki/Winamp_Skin · https://www.aimp.ru/?do=catalog&rec_id=242

### 2.5 Car head units (Pioneer / Clarion / Kenwood / Alpine / Panasonic / Technics car)

**Language:** single-DIN aluminum faceplates, motorized flip faces, **VFD / OEL** blue-green or pure-blue glow, spectrum analyzers, level meters, kitschy BGV animations (dolphins, races — Clarion horse race, Pioneer dolphins). Rotary commanders, soft-key rows under displays. Panasonic CQ-TX5500W “Tube Head” — analog VU + vacuum tube as hero object.  
**Sources:** https://www.bestcaraudio.com/crazy-car-radio-features-from-the-past/ · https://www.youtube.com/watch?v=VFAX13rOFc0 · Pioneer DEH OEL catalogs (dot-matrix spectrum / level indicator)

### 2.6 PS2 Browser / System Configuration

**Language:** deep Emotion Engine blue gradient field; drifting translucent “sand” / data-cubes; glass command panels with sheen + bloom; floating menu tiles; crystal-orb analog clock in System Configuration; glass cubes on insert-disc screen. Early-2000s chrome-and-glass / proto–Frutiger Aero.  
**Sources:** https://github.com/Timmy-Lane/ps2ui · https://gamia-archive.fandom.com/wiki/PlayStation_2_internal_display_clock · https://www.avid.wiki/PlayStation_2

### 2.7 Dreamcast BIOS / UI

**Language:** white / off-white plastic housing swirl brand; boot swirl **orange (JP/NA family) vs blue (EU)** — regional, not three rainbow myths; dithered framebuffer swirl; soft plastic consumer toy-meets-appliance. Orange swirl = primary motif for “Dreamcast music player” skin.  
**Sources:** https://redringrico.com/weblog/on-the-many-colours-of-the-dreamcast-swirl

### 2.8 Nintendo Game Boy / GBC

**DMG-01:** olive-grey ABS, large grey bezel (makes 160×144 feel bigger), reflective STN LCD (greenish 4 shades), polycarbonate screen lens, magenta A/B, black D-pad, rubber Start/Select, speaker grill asymmetry, part-lines as styling.  
**GBC:** translucent / opaque colored shells (Atomic Purple etc.), color LCD in dark bezel, same housing hierarchy.  
**Elevation rule:** never just green pixels on grey — always **shell + bezel + lens + labels + buttons**.  
**Sources:** https://modulelabsdesign.com/design-review/2017/10/16/nintendo-gameboy · https://en.wikipedia.org/wiki/Game_Boy · https://b13rg.icecdn.tech/Gameboy_DMG/

### 2.9 Neo Geo / Arcade cabinets

**Language:** black laminate cabinet, colored control panel buttons, lit marquee, **smoked/tinted monitor glass**, thick bezel framing CRT, neon cabinet art. Tinted glass historically improves black levels / hides burn-in. Pixel art sits *behind* glass and bezel — never full-bleed cheap tiles.  
**Sources:** https://forums.arcade-museum.com/threads/glass-on-my-neo-geo-tinted.334873/ · Neo Geo MVS restoration notes

---

## 3. Theme research cards (15)

Каждая карточка: references → layout 480×272 → materials → type/display → motion/meters → anti-phone → PSP constraints → sources.

---

### Card 1 — Sony Walkman Premium

| Field | Notes |
|-------|--------|
| **References** | WM-EX808HG (Al-Mg, High Grade), WM-EX5 (1996 mirror door + rubber edges), WM-EX90 (LCD + EL remote), late EX921 jog-lever. Premium = **object**, not app chrome. |
| **Layout @ 480×272** | Horizontal “deck” occupying ~full width; cassette window or smoked mirror door upper-center (~40–50% width); LCD strip under window or as remote-style readout left; transport cluster bottom-center or bottom-right; volume as edge dial motif right; brand wordmark small, engraved feel top-left. Avoid phone-style top app bar. |
| **Materials** | Brushed / mirror aluminum lid; matte silver or black polymer frame; soft grey rubber edge band; acrylic LCD window with polarizing-ish dark tint; subtle hairline parting. |
| **Typography / display** | Segment or small LCD digits; optional **EL orange/amber or blue** backlight on “remote panel” zone (not whole screen). Thin Sony-like Latin + condensed Japanese optional later. |
| **Signature motion & meters** | Subtle cassette-window shimmer / reel hints if open door; LED bar or Mega Bass style indicators; seek as thin physical slider, not Material progress. |
| **NOT from phones** | No floating FABs, no card carousel, no translucent iOS blur sheets, no huge centered sans headline. |
| **PSP constraints** | Specular = prebaked gradients / few highlight strips, not realtime env maps. EL glow = 1–2 additive layers max. Rubber edge = darker flat + 1px highlight. |
| **Sources** | https://walkmancentral.com/products/wm-ex5 · https://www.sony.com/en/SonyInfo/design/gallery/WM-EX808/ · https://walkman.land/sony/wm-ex90 |

**Repo gap:** current “Walkman Premium” palette is light silver + pastel pink accent — closer to fashion NW-A1000 than EX-era industrial. Research prefers **black/silver EX industrial** or clearly split “Adult Walkman desk” vs “pocket EX” variants for Art Director.

---

### Card 2 — Winamp Classic

| Field | Notes |
|-------|--------|
| **References** | Winamp 2.x Base Skin; cult classic skins (metal bevel, green LCD); Nullsoft default DNA. AIMP “Classic” / WMP9 remakes as secondary lineage. |
| **Layout @ 480×272** | Compact **main window** upper-left or centered top (~275×116 classic aspect scaled into frame); remaining space = playlist strip right or below; optional EQ strip. Mental map: titlebar → LCD time+track → spectrum → posbar → cbuttons. Do not stretch main chrome into fullscreen phone player. |
| **Materials** | Bevelled grey metal (hi/mid/lo 3-stop), dark inset LCD well, orange plastic seek knob, green phosphor-ish LCD paint. Texture = subtle noise / brushed, not flat #808080. |
| **Typography / display** | Bitmap LCD digit font (Numbers.bmp DNA); small monospaced track text; stereo/mono lights. |
| **Signature motion & meters** | Classic **peak spectrum bars** (green→yellow→red); stereo LED; play/pause indicator sprites; optional AVS-lite but keep classic first. |
| **NOT from phones** | No album-art full bleed as hero; no swipe gestures as visual language; no large circular play button. |
| **PSP constraints** | Spectrum: fixed bar count (8–16), CPU-cheap peaks. Skin = atlas of small sprites, not per-pixel shaders. |
| **Sources** | http://wiki.winamp.com/wiki/Creating_Classic_Skins · https://winampskins.neocities.org/base · https://winampskins.neocities.org/equalizer |

---

### Card 3 — Cassette Deck

| Field | Notes |
|-------|--------|
| **References** | Technics RS-series front-loaders; Hi-Fi cassette with dual VU / peak meters; Walkman EX open-door view as portable cousin. |
| **Layout @ 480×272** | Full-width brushed faceplate; dual cassette windows or single large well center; **VU meters** left or above transport; mechanical counter digits; transport keys bottom row (■ ▶ ◀◀ ▶▶ ●). |
| **Materials** | Brushed aluminum face; black matte cassette well; smoked acrylic meter windows; red recording LED; rubber transport keys. |
| **Typography / display** | Amber / green VFD-like or mechanical counter digits; small silkscreen labels (DOLBY, CrO2, MPX). |
| **Signature motion & meters** | Dual **analog needles** or LED ladder VU; reel rotation (even stylized); soft counter tick. Prefer needles over phone waveform. |
| **NOT from phones** | No waveform scrubber as primary; no glassmorphism cards. |
| **PSP constraints** | Needle = rotate thin sprite or line; reels = 2–4 frame loop. Avoid per-pixel cassette photo. |
| **Sources** | https://www.radiomuseum.org/r/technics_stereo_cassette_deck_rs_6_14.html · Walkman open-door EX models |

---

### Card 4 — MiniDisc

| Field | Notes |
|-------|--------|
| **References** | MZ-R909 (2001), MZ-N1 (NetMD, Mg), MZ-R90 magnesium flagship. Desk stand aesthetic optional. |
| **Layout @ 480×272** | Portrait-ish MD body centered or left; disc window upper; 3-line LCD below window; jog dial / rocker right of LCD; slim edge chrome. Playlist as “TOC list” beside unit. |
| **Materials** | Aluminum with **horizontal grain**; bright silver trim (R909) or flush Mg (N1); charcoal / colored anodized options; smoked disc window showing MD cartridge. |
| **Typography / display** | Multi-line LCD (9-char era feel); teal/cyan or black-on-grey; optional remote EL stick as secondary strip. |
| **Signature motion & meters** | MD disc **CLV spin** in window (speed feel, not accurate RPM UI); TOC scroll; simple ATRAC-era waveform or level blocks. |
| **NOT from phones** | No Spotify-like large cover art; MD window *is* the hero object. |
| **PSP constraints** | Disc spin = rotating sprite under window mask; LCD = clipped text region with scan-ish dim. |
| **Sources** | https://www.minidisc.org/brian_youn/MZR909/page1.html · https://www.minidisc.org/brian_youn/mzn1/page2.html · https://www.obsoletesony.com/minidisc/mz-r909/ |

---

### Card 5 — CD Player

| Field | Notes |
|-------|--------|
| **References** | Sony D-E01 (polarizing windows, chrome controls, slide-in), D-EJ2000 (Mg circle body), D-NE portable CD Walkman; component Hi-Fi CD faceplates. |
| **Layout @ 480×272** | Circular / near-square disc body dominating center; laser window or lid window over spinning CD art; LCD arc or bar under disc; jog lever motif; slim remote strip optional. |
| **Materials** | Magnesium / dark metal circle; chrome eject / control jewels; polarizing smoked acrylic window; blue laser glow accent (stylized, not sci-fi neon flood). |
| **Typography / display** | Blue/cyan LCD; track/time digits; G-PROTECTION / ESP icons as tiny status. |
| **Signature motion & meters** | Disc spin + **laser ring** / pickup arm hint; ring progress; optional skip-protection buffer bar as industrial LED. |
| **NOT from phones** | Avoid vinyl-app skeuomorphism clichés (wood grain + oversized tonearm unless CD-specific). |
| **PSP constraints** | Disc art = small circular blit + rotation; glow = additive ring, not blur bloom stacks. |
| **Sources** | https://www.sony.com/en/SonyInfo/design/gallery/D-E01/ · D-EJ2000 product literature |

---

### Card 6 — GameBoy (DMG)

| Field | Notes |
|-------|--------|
| **References** | Nintendo DMG-01 (1989); Play It Loud cosmetic variants; LSDj chiptune culture as music-adjacent authenticity. |
| **Layout @ 480×272** | Centered handheld shell (~360×230 usable); **large grey bezel** around LCD (~upper 40%); Nintendo wordmark / battery LED off-center on bezel; D-pad left / A·B right lower; Start/Select rubber center; speaker grill lower-right asymmetry. Music UI lives **inside the LCD glass**, not on the plastic. |
| **Materials** | Matte olive-grey ABS; textured back ridges implied; polycarbonate screen lens with slight green tint; hard plastic D-pad / A·B; softer rubber Start/Select. |
| **Typography / display** | 4-shade green STN palette; chunky bitmap font; contrast-knob metaphor optional. |
| **Signature motion & meters** | Pixel spectrum / block levels **inside LCD**; power LED; optional scroll like Game & Watch. |
| **Elevation beyond cheap squares** | Part-lines, bezel thickness, lens edge highlight, button domes, silkscreen labels, speaker holes — these are the design, not the green fill. |
| **NOT from phones** | No flat green full-screen; no Material chips. |
| **PSP constraints** | Shell = few rounded rects + sprites; LCD content 2bpp-feel; avoid heavy alpha under whole shell. |
| **Sources** | https://modulelabsdesign.com/design-review/2017/10/16/nintendo-gameboy · https://en.wikipedia.org/wiki/Game_Boy |

---

### Card 7 — GameBoy Color

| Field | Notes |
|-------|--------|
| **References** | GBC (1998) Atomic Purple / Berry / etc.; same housing hierarchy as DMG with color LCD and colored translucent shells. |
| **Layout @ 480×272** | Same shell proportions as DMG card; darker LCD bezel; color UI inside screen; shell hue is brand (purple/grape). |
| **Materials** | Translucent or opaque colored ABS; visible inner structure optional (premium); clear lens over color LCD. |
| **Typography / display** | Limited bright GBC-like palette; still bitmap, not antialiased UI chrome. |
| **Signature motion & meters** | Color pixel bars; flashy but still “handheld LCD”, not OLED phone. |
| **Elevation** | Translucency + inner PCB hints + colored buttons beat flat purple rectangle. |
| **NOT from phones** | No gradient mesh backgrounds outside shell. |
| **PSP constraints** | Translucency ≈ tinted overlay sprite, not realtime refraction. |
| **Sources** | https://en.wikipedia.org/wiki/Game_Boy_Color (general) · DMG industrial notes apply |

---

### Card 8 — DOS

| Field | Notes |
|-------|--------|
| **References** | MS-DOS / Norton Commander / early MOD players / text-mode trackers (Impulse Tracker vibe without copying IP); CGA/EGA/VGA text modes; amber or green phosphor terminals. |
| **Layout @ 480×272** | Full-bleed text matrix; header status line; dual-pane playlist optional; ASCII spectrum bottom; no window chrome — **phosphor CRT as material**. |
| **Materials** | Not plastic body — **CRT phosphor + scan** as surface; optional beige PC case *frame* around screen (ties to CRT TV theme). |
| **Typography / display** | 8×8 / 8×16 bitmap; green `#00FF00` or amber; blink cursor block. |
| **Signature motion & meters** | ASCII / block spectrum; cursor blink 2 Hz; optional snow/noise. |
| **NOT from phones** | No rounded cards; no emoji; no system sans. |
| **PSP constraints** | Perfect fit: glyph blit is cheap; avoid fancy TT fonts. |
| **Sources** | Historical DOS UI conventions; tracker culture (general knowledge, verify any specific UI clone carefully for IP) |

---

### Card 9 — Matrix

| Field | Notes |
|-------|--------|
| **References** | *The Matrix* (1999) digital rain (cinematic code cascade); green-on-black terminal fetish; late-90s cyber cinema. Treat as **film UI homage**, not trademarked logo dump. |
| **Layout @ 480×272** | Full-screen rain field; now-playing as “decoded” center column or glass terminal inset; controls as faint terminal commands. |
| **Materials** | Black void + phosphor green; optional smoked glass terminal frame to avoid flatness. |
| **Typography / display** | Katakana/Latin mix rain glyphs; bright head / dim trail columns. |
| **Signature motion & meters** | Column rain; pulse on beat; LED-style progress. |
| **NOT from phones** | No neon pink cyber overlays; keep monochrome green discipline. |
| **PSP constraints** | Rain = column offsets + glyph table, not per-pixel particles. Cap active columns. |
| **Sources** | Film visual language (fair-use research); terminal aesthetic references |

---

### Card 10 — Cyberpunk

| Field | Notes |
|-------|--------|
| **References** | Blade Runner / Ghost in the Shell / mid-2000s “neon wet street” UI; also Pioneer OEL head-unit kitsch as *hardware* cousin; carbon faceplates + magenta/cyan accents. |
| **Layout @ 480×272** | Dark HUD frame; horizon grid or city silhouette lower third; meters as neon ladders; title as stencil / condensed techno type. |
| **Materials** | Near-black carbon / anodized; acrylic neon edge light; wet specular streaks (prebaked). |
| **Typography / display** | Condensed techno, HUD labels, cyan body / magenta alerts. |
| **Signature motion & meters** | Perspective grid pulse; neon LED ladders; optional glitch on track change (1–2 frames). |
| **NOT from phones** | No Instagram story gradients; no glass cards with drop shadows; no purple Material theme default. |
| **PSP constraints** | Grid = few lines; neon = colored 1px + dim outer; avoid blur bloom. |
| **Sources** | Head-unit OEL era + cyber cinema language; avoid copying specific game HUDs 1:1 |

---

### Card 11 — CRT TV

| Field | Notes |
|-------|--------|
| **References** | 90s–early-2000s consumer CRT: beige/grey ABS cabinets; Sony Trinitron / WEGA (e.g. KV-FV310 blue-grey bezel era); darker phosphor faceplates mid-80s onward; aperture grille / scanline feel. |
| **Layout @ 480×272** | Beige/grey plastic TV shell filling frame; thick bezel; **inset screen** with slight barrel/overscan; channel/OSD lower; speakers left/right grille texture; power LED. Music UI = “broadcast OSD” on CRT face. |
| **Materials** | Injection ABS cabinet; textured speaker fabric; smoked CRT glass; soft specular on bezel corners. |
| **Typography / display** | Blue/cyan OSD; scanline overlay; slight RGB misregister optional (very subtle). |
| **Signature motion & meters** | Scanline crawl; degauss flash on theme enter; scope/waveform as “TV service” style. |
| **NOT from phones** | No thin modern TV bezels; no OLED pure black full-bleed without cabinet. |
| **PSP constraints** | Scanlines = every-other-line darken; curvature = optional mask sprite, not mesh warp. |
| **Sources** | https://crtdatabase.com/crts/sony/sony-kv-32fv310 · https://en.wikipedia.org/wiki/Trinitron |

---

### Card 12 — PS2 Browser

| Field | Notes |
|-------|--------|
| **References** | PS2 Browser / System Configuration; insert-disc glass cubes; floating clock orbs; memory card browser glass lists. |
| **Layout @ 480×272** | Deep navy full-bleed; floating **glass tiles** for transport / library; clock motif upper; track list as glass command box lower-right; ambient particles sparse. |
| **Materials** | Translucent blue glass panels; soft bloom on focus; volumetric light hints (prebaked). |
| **Typography / display** | Clean PS2 BIOS sans; soft white / ice blue; focus bloom not thick outline. |
| **Signature motion & meters** | Slow drift cubes/sand; tile glide on select; scope viz as soft luminous ribbon. |
| **NOT from phones** | This is already “glass UI” — differentiate from iOS by **depth fog, EE blue, cube language**, not white frosted cards. |
| **PSP constraints** | Cap simultaneous glass panels; fake blur with dithered translucent sprites; particles ≤ dozens. |
| **Sources** | https://github.com/Timmy-Lane/ps2ui · https://gamia-archive.fandom.com/wiki/PlayStation_2_internal_display_clock |

---

### Card 13 — PSP XMB

| Field | Notes |
|-------|--------|
| **References** | Official PSP XMB; Sony theme guidelines; wave backgrounds; category × list cross. |
| **Layout @ 480×272** | Exact native frame. Horizontal category icons mid-height; vertical list through focus; status clock/battery top; wave background full-bleed; now-playing can live as Music category detail or dedicated overlay respecting XMB spacing. |
| **Materials** | Black void; translucent wave ribbons; glossy icon plastics with white focus corona. |
| **Typography / display** | Official-like UI font; white labels; dim neighbors. |
| **Signature motion & meters** | Icon **translate** (not cursor move); focus pulse; wave phase animation; XMB music visualizer language (soft waveform). |
| **NOT from phones** | Do not replace cross-bar with hamburger menus or bottom tabs. |
| **PSP constraints** | This theme can be the most authentic — but waves must be cheap (few sine ribbons / textured scroll). Icons small atlases. |
| **Sources** | https://en.wikipedia.org/wiki/XrossMediaBar · https://cdn.us.playstation.com/pscomauth/groups/public/documents/webasset/ps_custom_theme-english_pp.pdf · https://blog.playstation.com/2008/06/20/how-to-make-your-own-psp-themes/ |

---

### Card 14 — Dreamcast

| Field | Notes |
|-------|--------|
| **References** | Sega Dreamcast boot swirl; white consumer plastic console; orange JP/NA swirl vs blue EU. |
| **Layout @ 480×272** | White / off-white shell panels; orange swirl motif as background hero (not tiny logo); dark inset LCD/info well; soft rounded plastic buttons. Music controls as DC BIOS-like menu rows. |
| **Materials** | Smooth white ABS; soft shadows in panel seams; orange translucent swirl energy (additive, soft). |
| **Typography / display** | Friendly rounded sans; white on dark panels; orange accents. |
| **Signature motion & meters** | Swirl rotate/pulse; orange energy meter; soft menu highlight underlines. |
| **NOT from phones** | No flat white Material cards; swirl must feel **volumetric / dithered**, not SVG flat. |
| **PSP constraints** | Swirl = rotating textured sprite + dither; avoid fullscreen distortion shaders. |
| **Sources** | https://redringrico.com/weblog/on-the-many-colours-of-the-dreamcast-swirl |

---

### Card 15 — Arcade

| Field | Notes |
|-------|--------|
| **References** | Neo Geo MVS cabinets; generic 90s fighters cabinets; marquee + bezel + smoked glass + control panel. |
| **Layout @ 480×272** | Outer black cabinet frame; lit marquee strip top (title/artist as marquee type); thick bezel; **smoked glass** over playfield UI; virtual control panel bottom with colored buttons. Pixel viz behind glass. |
| **Materials** | Black laminate / metal corners; neon cabinet art accents; tinted glass; chrome coin door hints optional. |
| **Typography / display** | Marquee display font; in-bezel pixel UI; high-contrast yellow/magenta/cyan sparingly. |
| **Signature motion & meters** | Attract-mode blink; neon level meters; insert-coin style prompts as flavor (not paywall). |
| **Elevation beyond cheap squares** | Marquee lighting, bezel thickness, glass tint, button plastics — same rule as GameBoy. |
| **NOT from phones** | No neon-border CSS cards; no full-bleed pixel without cabinet. |
| **PSP constraints** | Cabinet chrome = 9-slice; glass tint = multiply overlay; blink = timer, not particles. |
| **Sources** | https://forums.arcade-museum.com/threads/glass-on-my-neo-geo-tinted.334873/ · arcade restoration communities |

---

## 4. Cross-cutting: Sony industrial design principles (2000s)

1. **Material honesty** — aluminum looks like aluminum (grain), plastic looks matte or soft-touch, not “grey fill”.  
2. **Thin strength** — Mg/Al alloys enabled slim bodies without looking fragile (EX808, MZ-R90, D-EJ2000).  
3. **Seamless enclosure** — minimize visible screws/joints; side bands / clam shells (NW-A1000 DNA even when referencing earlier EX).  
4. **Control as jewelry** — chrome eject, jog levers, rocker bars; one hero control.  
5. **Window luxury** — polarizing / smoked acrylic over LCD or media (D-E01).  
6. **Remote as second skin** — EL backlit stick remotes carry orange/blue glow mythology.  
7. **Quiet branding** — small embossed logos; product form > loud wordmarks.  
8. **Portable Hi-Fi seriousness** — black/silver industrial before candy colors (colors = special editions).  

**Sources:** Sony Design Gallery entries (WM-EX808, D-E01, NW-A1000, NW-MS70D) · minidisc.org R90 press materials.

---

## 5. Cross-cutting: Head-unit / Hi-Fi meter language

| Meter type | Look | When to use |
|------------|------|-------------|
| Analog VU needle | Amber window, slow ballistics | Cassette, Hi-Fi, Tube Head homage |
| Peak LED ladder | Green→yellow→red segments | Walkman Mega Bass, live peak |
| Spectrum analyzer | Vertical bars, peak hold dots | Winamp, Pioneer/Kenwood OEL, AIMP |
| VFD / OEL blue | Self-emissive blue-green glyphs | Head-unit themes, Cyberpunk hardware |
| Segment LCD | Hard digits, icons | MiniDisc, Discman, remotes |
| Ring / laser | Circular progress | CD Player |
| Scope line | Soft luminous waveform | PS2, XMB music |

**Ballistics note for Art Director:** VU is slow/average; peak is fast; spectrum bins are FFT-feel but on PSP must be coarse. Prefer **readable industrial meters** over decorative noise.

**Sources:** https://www.bestcaraudio.com/crazy-car-radio-features-from-the-past/ · Technics RS peak/VU · Pioneer DEH OEL spectrum features

---

## 6. Cross-cutting: Winamp skin anatomy

Classic skin is a **collectible painting inside fixed slots**:

```
[ Titlebar ][ Clutter / shade ]
[ LCD time ][ Track text     ]
[ Spectrum analyzer          ]
[ ==== seek / posbar ======= ]
[ Vol ][ Bal ][ ◀◀ ▶ ■ ▶▶ ]
[ Shuffle ][ Repeat ][ Mono/Stereo ]
```

- **Main window** = identity  
- **EQ window** = vertical sliders + spline + “ON/AUTO”  
- **Playlist** = taller list chrome, pledit fonts/colors  
- Cult value = craft of bevels, LCD phosphors, metal textures — **not layout invention**

For PSP: one screen must **compress** Main + glance of playlist; EQ can be a mode, not third OS window.

**Sources:** Winamp Developer Wiki / neocities skin tutorial (linked in Card 2).

---

## 7. Cross-cutting: XMB / PS2 Browser motion & glass

| System | Motion DNA | Glass DNA |
|--------|------------|-----------|
| **XMB** | Icons travel; focus pulses white; waves phase slowly; neighbor icons dim | Icons are glossy objects on black; waves = translucent ribbons |
| **PS2 Browser** | Slow drift; cubes tumble; menu tiles float in; clock orbs orbit | Frosted blue command boxes; sheen streaks; bloom on focus |

**Shared rule:** selection is **luminous**, not outlined Material blue. Background is **alive but calm** (exhibition bar — motion presence, not ADHD).

---

## 8. Anti-patterns: what makes PSP homebrew look cheap

1. **Palette-only themes** — swap RGB, keep identical rectangle layout (current risk in repo).  
2. **Full-bleed flat color** with tiny text — no housing, no bezel, no glass.  
3. **Phone UI transplants** — bottom nav, cards, FABs, huge circular play.  
4. **Default AI aesthetic traps** — purple gradients, cream+serif terracotta, glow soup, pill clusters.  
5. **Pixel themes without device** — GameBoy green screen without ABS shell.  
6. **Arcade neon without cabinet** — cyber borders around empty list.  
7. **Over-blur / fake acrylic** that eats fillrate and still looks muddy.  
8. **Illegible LCD** — low contrast “authentic” that fails at exhibition distance.  
9. **Busy BGV** (dolphin races) competing with track title — head-unit kitsch is seasoning, not meal.  
10. **Inconsistent specular** — one button chrome, adjacent button flat vector.  
11. **System font only** — no LCD / bitmap / marquee personality.  
12. **Stretching Winamp main window to 480×272** — destroys collectible proportions.

---

## 9. Open questions for Art Director

1. **Walkman Premium north star:** EX-era black/silver industrial, or NW-A1000 soft white/pastel (current palette leans latter)?  
2. **Orange EL myth:** commit to remote-stick EL orange as signature, or body LCD EL?  
3. **Winamp fidelity:** strict classic proportions inset, or “Winamp-inspired” fullscreen metal?  
4. **GB/GBC:** portrait handheld in landscape 480×272 — letterbox shell vs rotate metaphor?  
5. **XMB theme vs system XMB:** how close before it feels like a fake firmware skin (legal/branding risk)?  
6. **Matrix / Cyberpunk IP:** keep generic film language vs named homage; any exhibition brand constraints?  
7. **Head-unit kitsch budget:** allow dolphin-tier BGV in one theme, or meters-only seriousness?  
8. **Shared chrome kit:** one material shader language across 15, or fully unique per theme (RAM tradeoff)?  
9. **Album art policy:** which themes may show cover art without breaking object fiction (CD window yes; DOS no)?  
10. **Exhibition viewing distance:** prioritize 2–3m readability over microscopic authenticity?  
11. **Motion quietness:** bar environment — max animation intensity?  
12. **Dreamcast swirl color:** JP orange default for all locales, or region toggle?

---

## 10. Inputs for Phase 2 (moodboards only — do not start Phase 2 here)

Для Concept / Art при переходе к moodboards собрать **по каждой теме** (или кластерами):

1. 3–5 reference photos of **real devices** (not UI screenshots alone)  
2. 1 **material macro** board (metal grain, ABS texture, smoked glass, rubber)  
3. 1 **LCD/VFD/phosphor** type board  
4. 1 **meter / motion** board (needles, spectrum, reels, waves)  
5. 1 **anti-board** (phone UI / cheap homebrew) marked ❌  
6. Proportion thumbnail on **480×272 frame** (paper/sketch OK)  
7. Note which repo palette to **keep / kill / split**

**Cluster suggestion for efficient boards:**  
- Sony Object Cluster: Walkman, MiniDisc, CD, XMB  
- Deck/Hi-Fi Cluster: Cassette, Winamp, CRT, DOS  
- Handheld/Cabinet Cluster: GB, GBC, Arcade, Dreamcast  
- Cinema HUD Cluster: Matrix, Cyberpunk, PS2  

---

## 11. Source bibliography (URLs used)

### Sony / Walkman / MD / CD
- https://www.sony.com/en/SonyInfo/design/gallery/WM-EX808/  
- https://www.sony.com/en/SonyInfo/design/gallery/D-E01/  
- https://www.sony.com/en/SonyInfo/design/gallery/NW-A1000/  
- https://www.sony.com/en/SonyInfo/design/gallery/NW-MS70D/  
- https://walkmancentral.com/products/wm-ex5  
- https://walkman.land/sony/wm-ex90  
- https://obsoletesony.substack.com/p/history-of-the-walkman-1979-2004  
- https://www.obsoletesony.com/walkman/wm-ex5/  
- https://www.minidisc.org/brian_youn/MZR909/page1.html  
- https://www.minidisc.org/brian_youn/mzn1/page2.html  
- https://www.obsoletesony.com/minidisc/mz-r909/  
- http://minidisc.org/mzr90/  

### PSP / XMB / PS2 / Dreamcast
- https://en.wikipedia.org/wiki/XrossMediaBar  
- https://cdn.us.playstation.com/pscomauth/groups/public/documents/webasset/ps_custom_theme-english_pp.pdf  
- https://blog.playstation.com/2008/06/20/how-to-make-your-own-psp-themes/  
- https://github.com/Timmy-Lane/ps2ui  
- https://gamia-archive.fandom.com/wiki/PlayStation_2_internal_display_clock  
- https://www.avid.wiki/PlayStation_2  
- https://redringrico.com/weblog/on-the-many-colours-of-the-dreamcast-swirl  

### Winamp / PC players
- http://wiki.winamp.com/wiki/Creating_Classic_Skins  
- https://winampskins.neocities.org/base  
- https://winampskins.neocities.org/equalizer  
- http://fileformats.archiveteam.org/wiki/Winamp_Skin  
- https://www.aimp.ru/?do=catalog&rec_id=242  

### Head units / Hi-Fi / Arcade / CRT / GB
- https://www.bestcaraudio.com/crazy-car-radio-features-from-the-past/  
- https://www.youtube.com/watch?v=VFAX13rOFc0  
- https://www.radiomuseum.org/r/technics_stereo_cassette_deck_rs_6_14.html  
- https://forums.arcade-museum.com/threads/glass-on-my-neo-geo-tinted.334873/  
- https://crtdatabase.com/crts/sony/sony-kv-32fv310  
- https://en.wikipedia.org/wiki/Trinitron  
- https://modulelabsdesign.com/design-review/2017/10/16/nintendo-gameboy  
- https://en.wikipedia.org/wiki/Game_Boy  
- https://b13rg.icecdn.tech/Gameboy_DMG/  

### Repo (read-only baseline)
- `psp/src/theme.h`, `psp/src/theme.c`, `psp/src/ui.c` (local workspace)

---

## 12. Research Team sign-off

**Deliverable complete:** Phase 1 research pack for Concept Team.  
**Explicitly not done:** moodboards, wireframes-as-code, theme patches, builds.  
**Success criterion for this phase:** Art Director can brief Phase 2 boards per theme using materials/layout/motion vocabulary above without inventing device facts from scratch.
