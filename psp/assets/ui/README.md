# UI assets

Raw ABGR8888 blobs embedded into `EBOOT.PBP` via `psp-objcopy -I binary`.

| File | Size | Contents |
|------|------|----------|
| `atlas.rgba` | 256×256 | Fallback cover, card chrome, transport icons, mini-bar, accent pill |
| `font.rgba` | 256×128 | 96 ASCII glyphs in a 16×6 grid (16×16 cells) |
| `bg.rgba` | 512×272 | Optional soft background (not embedded; themes tint procedurally) |

Regenerate with Pillow from the project root if you edit the PNGs:

```bash
# atlas.png / font.png -> .rgba (R,G,B,A byte order matching PSP 8888)
python3 tools/gen_ui_assets.py
```

`ICON0.PNG`, `PIC1.PNG`, and `SND0.AT3` in the parent folder are Naruto XMB
assets provided by the user and packed by `pack-pbp`.
