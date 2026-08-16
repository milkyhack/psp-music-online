# PSP client (`PSPMUSIC` / `PSPMUSICUPD`)

Full setup guide: **[../README.md](../README.md)**.

## Build

```bash
# Requires pspdev on PATH
make            # EBOOT.PBP — player
make companion  # dist/PSPMUSICUPD/EBOOT.PBP — Music Updater
```

Release builds have debug logging compiled out. QA: `make DEBUG_HUD=1`.

## Install paths

```text
ms0:/PSP/GAME/PSPMUSIC/EBOOT.PBP
ms0:/PSP/GAME/PSPMUSIC/server.cfg          # "IP PORT"
ms0:/PSP/GAME/PSPMUSICUPD/EBOOT.PBP        # OTA companion
```

Online audio is **MP3 320** from the server (including FLAC sources). Soft-FLAC is for local/offline files only.
