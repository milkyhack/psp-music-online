# PSP graphics notes (agent + contributor digest)

Learning notes from community tutorials and Vita Adrenaline display settings.
Used when changing UI for **PSP Music Online** (native **480×272**, `sceGu` / CPU framebuffer — not GLUT).

## Sources

1. [PSPinfo — Урок 4 (OpenGL)](https://www.pspinfo.ru/homebrew/18320-uroki-programmirovanija.-urok-chetvjortyjj.html) (.:Witcher:., ~2009) and lessons 1–5 where available
2. [GBATemp — Best Adrenaline/PSP picture settings](https://gbatemp.net/threads/the-best-adrenaline-psp-picture-settings-ever.604395/)

---

## PSPinfo lesson series (Witcher)

Incomplete and partly broken by the site HTML parser. Public content:

| Lesson | Topic |
|--------|--------|
| 1 | SDK / toolchain, Hello-style start |
| 2 | C++ theory on PC (algorithms, functions, types, `if` / `while`) |
| 3 | Missing as a full article |
| 4 | OpenGL/GLUT on PSP: lines → rotating 3D, GLUT key map |
| 5 | Forum preview only: textured rotating cube; later lessons planned, never finished |

Also useful (same era, not Witcher): [LIon__ Hello World + DevKitPSP](https://www.pspinfo.ru/forum/post/87611.html) — `PSP_MODULE_INFO`, exit callbacks, `pspDebugScreen*`, `pack-pbp`, `SceCtrlData`.

Community advice then: beginners → Lua/PGE; serious apps → **C/C++ + PSPSDK**.

### Takeaways for this repo

**Already aligned**

- `pspdev`, `PSP_FW_VERSION=371`, ELF → strip → `EBOOT.PBP`
- Exit callback pattern
- Input via `pspctrl` (more reliable than GLUT key remapping)

**Do not port blindly**

- Lesson 4’s **GLUT/OpenGL** stack is a different render path. This app UI lives in `psp/src/ui_gfx.c`, `ui_gpu.c`, `ui.c`.
- Useful ideas only: clear primitives first, then effects; square textured covers; keep draw vs input loops separate.

---

## Adrenaline picture settings (Vita)

Adrenaline **upscales** our 480×272 frame. Soft bloom / glass / non-integer scale often looks like “dirty stripes” or mud on OLED.

### Recommended preset (thread OP)

| Where | Setting | Value |
|-------|---------|--------|
| Official Settings | Bilinear Filtering | **OFF** |
| Adrenaline → Graphics Filtering | Sharp bilinear | or Sharp bilinear without scanlines |
| Smooth Graphics | | **No** for classic Sharp bilinear; **Yes** for without-scanlines |
| Scale X / Y (PSP) | | **2.00** (integer 2×) |

### Filter notes

- **Original** — slightly less lag, often softer
- **Sharp bilinear** — crisp, mild scanlines
- **LCD3x** — PSP-LCD-like grid
- **Advanced AA** — smooths pixels (usually worse for UI apps)
- Non-integer scales (1.555 / 1.720 / 1.995) reintroduce blur

### Rules for our UI code

1. Pixel-crisp: integer coords, bitmap font, solid 1px lines over soft alpha fog
2. Avoid large soft bloom / glass bands behind covers (ugly at 2× sharp)
3. Covers: square + thin frame (Neon Terminal path in `np_draw_cover_hero`)
4. Prefer screenshots on real PSP or Adrenaline with Sharp bilinear + 2.00

### User checklist (Vita)

1. Adrenaline menu → Official Settings → disable **Bilinear Filtering**
2. Settings tab (L/R) → Graphics Filtering: **Sharp bilinear**
3. Smooth Graphics per table above
4. Scale X/Y = **2.00**
5. Client **1.3.10+** (Neon Terminal without cover halo / Matrix strip)

---

## Summary

| Source | What we keep |
|--------|----------------|
| Witcher lessons | Toolchain + ctrl + PBP lifecycle; not GLUT UI |
| Adrenaline thread | Integer 2× + Sony bilinear off + sharp filter for Vita viewing |
| Product UI | Sense Me / XMB restraint: clean edges, square art, minimal chrome noise |
