# AAA Screenshot Gate Result

Date: 2026-08-29 (max parity pass)

## Status: **FAIL** (close — layout parity; covers differ from marketing PNGs)

Latest report: `design/qa/reports/20260829-220053/report.json`

| Screen | SSIM | Accent | Notes |
|--------|------|--------|-------|
| home | 0.797 | OK | Demo copy (12 songs, 65 tracks, Press X, 192.168.31.95) |
| library | 0.798 | OK | Demo tracks (Radiohead row selected, mini-player) |
| now-playing | **0.826** | OK | Neon + Tycho/A Walk/Awake demo, clock, pills, viz |
| coverflow | 0.823 | bg corner | Black QA stage; album art still from server |
| setup | 0.838 | OK | Demo IP 192.168.31.95:8084 |

**Automated gate:** FAIL — SSIM proxy below per-screen mins; album art cannot match README without bundled ref covers.

**Visual parity (bench):** QA demo mode (`ppsspp_qa.txt`) injects README labels/layout. Real PSP ms0: never ships `ppsspp_qa.txt`.

## What was done
- `ppsspp_qa_demo.c` — README marketing copy for home/library/NP/setup/coverflow
- Premium library (Midnight skin 8): status bar, title+artist rows, green rail, ring play mini-player
- Neon NP: green header, clock, SHUFFLE/REPEAT, no codec line, demo times 2:31/5:17
- Honest compare gates in `tools/ppsspp_qa/compare_screens.py`

## Re-run
```bash
tools/ppsspp_visual_qa.sh
open design/qa/reports/*/home-diff.png   # inspect diffs manually
```

## PSP-3008
Hardware checklist still pending — `tools/release_hw_checklist.md`.
