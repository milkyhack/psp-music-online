"""Store remote PSP client debug breadcrumbs (no Memory Stick on device)."""
from __future__ import annotations

import json
import time
from pathlib import Path
from typing import Any

from .config import settings

_MAX_FILES = 120
_MAX_EVENTS = 80
_MAX_BODY = 64 * 1024


def _logs_dir() -> Path:
    d = settings.data_dir / "client_logs"
    d.mkdir(parents=True, exist_ok=True)
    return d


def _prune(dir_path: Path) -> None:
    files = sorted(dir_path.glob("*.json"), key=lambda p: p.stat().st_mtime)
    while len(files) > _MAX_FILES:
        try:
            files.pop(0).unlink(missing_ok=True)
        except OSError:
            break


def ingest_client_logs(payload: dict[str, Any]) -> dict[str, Any]:
    events = payload.get("events")
    if not isinstance(events, list) or not events:
        return {"ok": False, "error": "no events"}
    if len(events) > _MAX_EVENTS:
        events = events[-_MAX_EVENTS:]

    raw = json.dumps(payload, ensure_ascii=False)
    if len(raw.encode("utf-8")) > _MAX_BODY:
        # keep tail only
        payload = dict(payload)
        payload["events"] = events[-20:]
        payload["truncated"] = True

    now = time.time()
    stamp = time.strftime("%Y%m%d-%H%M%S", time.localtime(now))
    ms = int((now % 1) * 1000)
    name = f"{stamp}-{ms:03d}.json"
    path = _logs_dir() / name

    record = {
        "received_at": now,
        "app_version": str(payload.get("app_version") or "")[:32],
        "app_code": int(payload.get("app_code") or 0),
        "device": str(payload.get("device") or "psp")[:24],
        "session": str(payload.get("session") or "")[:48],
        "events": events,
        "truncated": bool(payload.get("truncated")),
    }
    path.write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    # Append one-line summary for quick tail in terminal
    summary = {
        "file": name,
        "t": now,
        "ver": record["app_version"],
        "n": len(events),
        "last": events[-1] if events else None,
    }
    with (_logs_dir() / "index.jsonl").open("a", encoding="utf-8") as f:
        f.write(json.dumps(summary, ensure_ascii=False) + "\n")

    _prune(_logs_dir())
    return {"ok": True, "file": name, "events": len(events)}


def list_client_logs(limit: int = 30) -> list[dict[str, Any]]:
    dir_path = _logs_dir()
    files = sorted(dir_path.glob("*.json"), key=lambda p: p.stat().st_mtime, reverse=True)
    out: list[dict[str, Any]] = []
    for path in files[: max(1, min(limit, 100))]:
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        events = data.get("events") or []
        last = events[-1] if events else {}
        out.append(
            {
                "file": path.name,
                "received_at": data.get("received_at"),
                "app_version": data.get("app_version"),
                "app_code": data.get("app_code"),
                "device": data.get("device"),
                "session": data.get("session"),
                "event_count": len(events),
                "last_message": (last.get("msg") or last.get("message") or ""),
                "last_location": (last.get("loc") or last.get("location") or ""),
            }
        )
    return out


def read_client_log(name: str) -> dict[str, Any]:
    safe = Path(name).name
    if not safe.endswith(".json") or ".." in safe:
        raise FileNotFoundError(name)
    path = _logs_dir() / safe
    if not path.is_file():
        raise FileNotFoundError(safe)
    return json.loads(path.read_text(encoding="utf-8"))
