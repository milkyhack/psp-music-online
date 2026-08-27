# Homebrew Player UX Audit → Music Online

**Date:** 2026-08-27  
**Frame:** 480×272 · CFW client  
**Rule:** Steal *interaction*; keep AAA device fiction (Phase 1 §8). Cover Flow 3D is an approved **selectable style**.

---

## Steal / reject map

| Source | Steal | Reject |
|--------|-------|--------|
| LightMP3 | L+R help, smart Prev (>2% restart), dwell cover, economy/sleep, media library DNA | skin.cfg rectangle skins |
| DJSP | Viz-first density, power display modes, EQ window | Particle carnival BGV |
| CoverFlow / PS-CoverFlow | **3D album carousel as style**, info density cycle, cover cache | Bounce stickers / carnival |
| Media Player Engine | Clear zones, sleep end-of-track, screen-off | WMP chrome clone |
| AVi player | Dim / OSD while play | Video-first UX |
| 2TUFF.wav | — | Desktop emulator, not CFW DNA |

---

## Shipped in 1.3.15

| Pattern | Where |
|---------|--------|
| Cover quality 160px + premium frame | `ui_image.*`, `ui_gpu.*`, `np_draw_cover_hero` |
| Cover Flow style (skin + COMP) | `theme.*`, `np_comp_coverflow`, album carousel |
| Controls overlay L+R | `main.c` + `ui_draw_help_overlay` |
| Smart Previous | `skip_track` / playing LEFT |
| Library dwell preview | `ui_draw_library_ex` |
| NP density cycle | START+SELECT on Now Playing |
| Economy + sleep-at-end | Settings + playback pump |

---

## Cover Flow style brief

- Appearance skin **Cover Flow** (`COMP_COVERFLOW`).
- Albums list → horizontal 3D-ish carousel (center hero, side skew/fade).
- NP → cover-forward theater + thin progress; meters only, no particle soup.
- Materials: smoked glass well + hairline specular — exhibition object, not iPod cards.
