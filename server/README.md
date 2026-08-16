# PSP Music Server

Full setup guide: **[../README.md](../README.md)**.

```bash
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env   # set MUSIC_DIR
python -m app          # http://127.0.0.1:8084/
```
