# Contributing

## Client (PSP)

```bash
cd psp
make companion          # release
make DEBUG_HUD=1        # QA overlay + debug.log
```

Keep online streaming on the **MP3 320** path. Soft-FLAC is for local/offline only.

## Server

```bash
cd server
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
python -m app
```

Do not commit `server/data/` (library DB, caches, client logs).

## Releases

1. Bump `APP_VER` / `APP_VER_CODE` in `psp/Makefile` and `psp/src/updater.h`
2. `make companion`
3. Publish `EBOOT.PBP` + `update.json` under `server/data/client/` (local) or GitHub Releases
