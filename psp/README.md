# PSP client (`PSPMUSIC` / `PSPMUSICUPD`)

Full guide: **[../README.md](../README.md)**.

```bash
make            # player EBOOT.PBP
make companion  # Music Updater
```

Release builds have no debug overlay. QA HUD (RAM only): `make DEBUG_HUD=1`.

```text
ms0:/PSP/GAME/PSPMUSIC/EBOOT.PBP
ms0:/PSP/GAME/PSPMUSIC/server.cfg
ms0:/PSP/GAME/PSPMUSICUPD/EBOOT.PBP
```

Online audio is **MP3 320** in RAM. Soft-FLAC is for local/offline files only.
