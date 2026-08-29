# PSP-3008 Release Hardware Checklist

Use this **after** PPSSPP visual QA passes and **before** tagging a GitHub Release.

Target: Sony PSP-3008 (or equivalent) with **ARK-5 CFW**, Memory Stick for EBOOT only.

## Deploy (clean — no bench flags)

Copy **only**:

```
ms0:/PSP/GAME/PSPMUSIC/EBOOT.PBP
ms0:/PSP/GAME/PSPMUSIC/data/server.cfg   ← LAN IP from admin, e.g. 192.168.x.x 8084
```

**Do not copy:**

- `ppsspp_auto.txt`
- `ppsspp_qa.txt`
- `cf_safe.txt`

Run `tools/verify_release_eboot.sh` on the PC build tree to confirm bench files are not bundled.

## Smoke tests on hardware

- [ ] App boots to Home (Neon Terminal default)
- [ ] Settings → Connect Wi‑Fi → system dialog → connected
- [ ] Online Library → Songs / Albums load from server
- [ ] Play track — audio via `sceMp3`, progress bar moves
- [ ] Now Playing — album cover visible (not black box)
- [ ] Appearance → Cover Flow (skin 15) → Albums 3D carousel
- [ ] Setup — save LAN IP, survives reboot
- [ ] Offline save (START) writes one file under `ms0:/MUSIC/...`
- [ ] Select album/track — no crash or full screen reset
- [ ] 5+ minutes playback — no progressive slowdown or OOM

## Pass criteria

All boxes checked on **real hardware**. PPSSPP PASS alone is necessary but not sufficient for Cover Flow 3D and streaming (HLE differs from hardware).
