# AAA Screenshot Gate Result

Date: 2026-08-08 (iteration)

## Build
- EBOOT: `psp/EBOOT.PBP` (~1.8MB)
- Deployed: `~/.config/ppsspp/PSP/GAME/music/EBOOT.PBP`
- Theme: Midnight (`skin 0`)

## Changes this pass
- Regenerated AA font atlas (Arial Bold, proportional advances, bilinear blit)
- Larger on-screen glyph sizes (MD≈22, LG≈28)
- Soft AA circles for transport
- Baked chrome labels (Music header, etc.)
- Library/home: panel header, card selection, accent dot
- Host reference frames: `gate_np_live.png`, `gate_home_target.png`

## Captures
| File | Notes |
|------|--------|
| `screenshots/ppsspp_home.png` | Live PPSSPP home |
| `screenshots/gate_np_live.png` | Target NP composition (tokens) |
| `screenshots/gate_home_target.png` | Target home composition |
| `mockups/np_spotify_ref.png` | Design reference |

## Gate status
- Layout tokens for NP: implemented in `np_comp_modern`
- Live home typography: improved vs FONT8; 480p still limits perceived smoothness
- Full NP live capture: blocked without offline/online track playback in this session
- Continue: play a track → capture NP/Library → diff vs `gate_np_live.png`

## How to re-capture
1. Launch PPSSPP with memstick `~/.config/ppsspp`
2. In-game L+R saves BMP via client screenshot path
3. Or window capture → crop to 480×272
