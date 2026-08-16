========================================
PSP MUSIC PLAYER PERFORMANCE REPORT
========================================
Build: EBOOT.PBP (lossless architecture, 2026-08-15)
Size: ~2.4 MB packed PBP (ELF ~2.1 MB stripped)

FORMAT
FLAC LOSSLESS (primary, native libFLAC) / MP3 LOSSY (sceMp3 passthrough)
Other formats: rejected (HTTP 415 / client message) — no FLAC↔MP3

SAMPLE RATE
Source STREAMINFO shown in UI (typically 44.1 / 48 / 96 kHz)
DAC path: 44.1 kHz 16-bit (48→44.1 resample; 24→16); 96 kHz try-or-Unsupported

BIT DEPTH
Source 16 or 24 (UI); DAC 16-bit

RAM
Ring buffer: adaptive 512 KB – 2 MB (SPSC, sceKernelCreateSema)
UI framebuffer + cover LRU (6×128×128)
libFLAC stream decoder: callback (ring or file), not whole-file load
Embedded FLAC PICTURE decoded in RAM (JPEG via libjpeg / PNG via libpng)

RING BUFFER
Network WRITE / Decoder READ
Online streaming path: ZERO Memory Stick chunk writes (no cache.mp3)

AVERAGE BUFFER / MIN / MAX
Runtime on Diagnostics (BUFFERED sec / USED%)
Target prebuffer: ~8s FLAC, ~5s MP3 (clamp 5–15s)

FLAC DECODE RATE / AUDIO CONSUMPTION / NETWORK RATE
Shown on Diagnostics (DECODER / NETWORK Mbps)
PCM consumption ≈ 1.41 Mbps @ 44.1/16/stereo
Wire FLAC typically ~0.7–1.0 Mbps — need net_throughput > consumption × headroom

REBUFFER COUNT
metrics.rebufferCount / bufferUnderruns (Diagnostics REBUFFER)

MEMORY STICK WRITES / BYTES WRITTEN
Target after 1h online play (no Download / Update / first-hit cover): ≈ 0
Counters: storage_stats()->memoryStickWrites / BytesWritten / BytesDeleted
Allowed MS writes: explicit Download (song.tmp→.flac), Update (EBOOT.tmp),
write-once covers, rare position.txt, settings on apply/exit

TEMP FILES CREATED / DELETED
Download: song.tmp → verify → song.flac (or discard)
Update: EBOOT.PBP.tmp → verify → replace (+ .bak)

MEMORY LEAKS / CPU
Ring/decoder/cover freed on track change; MaxFreeMem on long session
96 kHz may stop with Unsupported if underrun streak

QA CHECKLIST (run on PPSSPP + hardware)
[ ] TEST 1 Online FLAC — buffer/CPU/RAM/rebuffer stable
[ ] TEST 2 1 hour MS writes ≈ 0
[ ] TEST 3 Disconnect mid-play → RAM continues → Range resume
[ ] TEST 4 Download tmp → verify → flac under ms0:/MUSIC/
[ ] TEST 5 Interrupted download resume (START)
[ ] TEST 6 MS full → Not enough storage, no broken .flac
[ ] TEST 7 Corrupted FLAC → FLAC ERROR, no hang
[ ] TEST 8 Client update EBOOT.tmp → sha256/PBP → replace; exit XMB

ARCHITECTURE NOTES
- Server stream.py: FileResponse FLAC/MP3 only
- Client Update: GET /api/client/update + /api/client/EBOOT.PBP (not Sony XMB)
- Offline: legacy data/offline/{id}.mp3 + ms0:/MUSIC/**/*.flac
- Download UI: % / MB/MB / speed; START pause/resume; Square cancel while DL

========================================
