# AAA Screenshot Gate — Hard PASS/FAIL Checklist

Use this checklist for every build that touches the Now Playing or Library screens. Any single FAIL means the build does not pass visual QA.

## General
- [ ] Screenshot is exactly 480 × 272 pixels.
- [ ] Background is `BG` `#08080A` (±1 RGB channel).
- [ ] No debug HUD, no FPS, no memory counters, no version hash visible.
- [ ] No help-text footer anywhere on the screen.
- [ ] English only; no untranslated or non-English strings.
- [ ] No Winamp-style elements, pixel-font text, or retro 1-bit icons.

## Now Playing Screen
- [ ] Album art is present at (24, 24), exactly 160 × 160 px, corner radius 12 px.
- [ ] Title text starts at (204, 40) in `UI_FONT_LG`, color `TEXT` `#FFFFFF`.
- [ ] Artist text starts at (204, 70) in `UI_FONT_MD`, color `TEXT` `#FFFFFF`.
- [ ] Album text starts at (204, 92) in `UI_FONT_SM`, color `MUTED` `#A0A0A8`.
- [ ] Title/artist/album text is smooth anti-aliased, not pixelated or clipped mid-character.
- [ ] Progress bar starts at (24, 208), width 432 px, height 4 px.
- [ ] Progress track is `PANEL` `#121216`; progress fill is `ACCENT` `#1ED760`.
- [ ] Progress knob is radius 5 px, `ACCENT` color, visible when playing or paused.
- [ ] Elapsed time at (24, 190) in `UI_FONT_SM`, `MUTED`, left-aligned.
- [ ] Remaining time at (424, 190) in `UI_FONT_SM`, `MUTED`, right-aligned.
- [ ] Play/pause circle is centered at (240, 248) with radius 26 px, color `ACCENT`.
- [ ] Previous circle is centered at (168, 248) with radius 18 px, color `TEXT` at 40% opacity.
- [ ] Next circle is centered at (312, 248) with radius 18 px, color `TEXT` at 40% opacity.
- [ ] Transport icons are soft vector-like symbols, not jagged sprites.

## Library Screen
- [ ] Header bar is at (0, 0), height 40 px, background `PANEL` `#121216`.
- [ ] Header title "Library" at (24, 12), `UI_FONT_MD`, `TEXT`.
- [ ] Track list starts below header at y=40 and ends before mini-player at y=228.
- [ ] Row height is 30 px (±1 px).
- [ ] Row background is `BG`; selected row is `CARD` `#202028`.
- [ ] Mini-player is at (0, 228), height 44 px, background `PANEL`.
- [ ] Mini-player cover is 36 × 36 px at (8, 4), radius 6 px.
- [ ] Mini-player title/artist in `UI_FONT_SM`, `TEXT`/`MUTED`.

## Colors (Tolerance ±1 RGB channel)
- [ ] `BG` `#08080A`
- [ ] `ACCENT` `#1ED760`
- [ ] `TEXT` `#FFFFFF`
- [ ] `MUTED` `#A0A0A8`
- [ ] `CARD` `#202028`
- [ ] `PANEL` `#121216`

## Accent Presets (Settings Screen)
- [ ] Exactly 15 presets listed.
- [ ] Names are: Midnight, Ocean, Violet, Ember, Mint, Rose, Amber, Ice, Neon, Graphite, Crimson, Indigo, Sand, Forest, Aurora.
- [ ] Preset text is `UI_FONT_SM`, `TEXT`, English only.

## Final Gate
- [ ] Reference mockups `design/qa/mockups/np_spotify_ref.png` and `design/qa/mockups/library_spotify_ref.png` exist and are up to date.
- [ ] Screenshot visually matches the reference mockups for layout, colors, and proportions.

**PASS:** Every required checkbox is checked.  
**FAIL:** Any required checkbox is unchecked. Fix and re-capture before merging.
