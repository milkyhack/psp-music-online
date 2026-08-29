# AAA Visual Tokens — Premium PSP Music Player

## Canvas
- **Resolution:** 480 × 272 (native PSP)
- **Language:** English only
- **Direction:** Premium Sony/PSP device UI — XMB depth + Walkman clarity. Not Spotify clone, not Material, not pixel-art toy.

## Depth Layers
1. **Background** — vertical atmosphere gradient + faint accent bloom behind cover
2. **Midground** — glass panels, list rails, header chrome
3. **Foreground** — album art frame, primary type, transport hero

## Color Palette (Midnight default)
| Token | Hex | Usage |
|-------|-----|-------|
| `BG` | `#07070A` | Base atmosphere |
| `BG_TOP` | `#101018` | Gradient top |
| `PANEL` | `#12121A` | Header / glass face |
| `CARD` | `#1C1C26` | Selection / elevated |
| `ELEVATED` | `#242430` | Cover well / button wells |
| `HAIRLINE` | `#3A3A48` @ 55% | Thin borders |
| `ACCENT` | `#1ED760` | Play, progress, focus |
| `ACCENT_DIM` | `#1ED760` @ 28% | Soft glow |
| `TEXT` | `#F4F4F8` | Primary |
| `TEXT_SEC` | `#C8C8D0` | Artist |
| `MUTED` | `#8A8A96` | Metadata |
| `DANGER` | `#E94646` | Errors |
| `SUCCESS` | `#1ED760` | Saved / online |

15 accent presets keep structure; only hue of `ACCENT` / motif shifts.

## Typography
Atlas font (not 8×8):
- **LG** — track title / screen title
- **MD** — artist, list rows
- **SM** — album, times, pills

Controlled edge AA (no soft mush, no 1-bit jaggies). Letterforms stay sharp on 480×272.

## Now Playing (480×272)
| Element | Spec |
|---------|------|
| Atmosphere | `grad_v(BG_TOP→BG)` + soft accent bloom behind cover |
| Cover frame | outer 168×168 @ (16,16), inner art 152×152 @ (24,24), r=10 |
| Cover glow | accent alpha bloom under frame |
| Title | (196, 28) LG TEXT, clip 268 |
| Artist | (196, 56) MD TEXT_SEC, clip 268 |
| Album | (196, 80) SM MUTED, clip 268 |
| Status row | shuffle/repeat pills + volume ticks @ y≈108 |
| Times | (24, 178) / right (456) SM MUTED |
| Progress | (24, 196) 432×5 inset track, accent fill, knob r=5 + halo |
| Transport | prev (156,246)r18 · play (240,242)r28 glow · next (324,246)r18 |
| Prohibited | help footer, debug HUD, pixel chrome |

## Library
| Element | Spec |
|---------|------|
| Header | 40px glass panel + hairline bottom + title MD |
| Rows | 32px; selected = CARD + 3px accent rail + soft glow |
| Thumbs | 24×24 r=5 in elevated well |
| Mini | 48px frosted bar, cover 36×36, progress 160×3, play well |

## Motion (PSP-safe)
- Selection: solid fill + rail (no blur)
- Play press: existing lit/unlit swap
- Progress: per-frame fill only
- EQ bars: existing soft bars (sim)

## Art Director Gates
- Hierarchy: title > artist > album > chrome
- Play must dominate prev/next
- Cover must feel framed, not a flat `<img>`
- No identical rectangle stacks
- Focus always obvious (accent rail / glow)
- Readable at arm’s length on 4.3"
