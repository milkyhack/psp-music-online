"""Scan music folder and populate SQLite catalog."""

from __future__ import annotations

import json
import threading
import time
from pathlib import Path
from typing import Any, Optional

from mutagen import File as MutagenFile

from .config import settings
from .db import (
    AUDIO_EXTS,
    get_db,
    get_meta,
    get_or_create_album,
    get_or_create_artist,
    init_db,
    set_meta,
    upsert_track,
)

_scan_lock = threading.Lock()
_scan_thread: Optional[threading.Thread] = None


def _first(val: Any) -> Optional[str]:
    if val is None:
        return None
    if isinstance(val, (list, tuple)):
        return str(val[0]) if val else None
    return str(val)


def _parse_track_num(val: Any) -> Optional[int]:
    s = _first(val)
    if not s:
        return None
    s = s.split("/")[0].strip()
    try:
        return int(s)
    except ValueError:
        return None


def _parse_year(val: Any) -> Optional[int]:
    s = _first(val)
    if not s:
        return None
    import re

    m = re.match(r"(\d{4})", s.strip())
    if not m:
        return None
    year = int(m.group(1))
    return year if 1900 <= year <= 2100 else None


def _parse_genre(val: Any) -> Optional[str]:
    s = _first(val)
    if not s:
        return None
    s = s.strip()
    if not s:
        return None
    for sep in ("/", ";", "|"):
        if sep in s:
            s = s.split(sep)[0].strip()
            break
    return s or None


def read_tags(path: Path) -> dict[str, Any]:
    from urllib.parse import unquote

    from .translit import clean_album, clean_artist, clean_title

    tags: dict[str, Any] = {
        "artist": "Unknown Artist",
        "album": "Unknown Album",
        "title": path.stem,
        "track_num": None,
        "duration": None,
        "genre": None,
        "year": None,
        "codec": path.suffix.lower().lstrip(".") or None,
        "sample_rate": None,
        "bit_depth": None,
        "channels": None,
        "bytes": None,
        "lossless": 1 if path.suffix.lower() == ".flac" else 0,
        "bitrate": None,
    }

    try:
        tags["bytes"] = path.stat().st_size
    except OSError:
        pass

    # Prefer folder layout: MUSIC_DIR / Artist / Album / file
    try:
        rel = path.resolve().relative_to(settings.music_dir.resolve())
        parts = rel.parts
        if len(parts) >= 3:
            tags["artist"] = unquote(parts[-3].replace("_", "/"))
            tags["album"] = unquote(parts[-2])
        elif len(parts) == 2:
            tags["artist"] = unquote(parts[0])
    except Exception:
        pass

    try:
        audio = MutagenFile(path, easy=True)
    except Exception:
        tags["title"] = clean_title(tags["title"], path_stem=path.stem)
        tags["artist"] = clean_artist(tags["artist"])
        tags["album"] = clean_album(tags["album"], artist=tags["artist"])
        return tags

    if audio is None:
        tags["title"] = clean_title(tags["title"], path_stem=path.stem)
        tags["artist"] = clean_artist(tags["artist"])
        tags["album"] = clean_album(tags["album"], artist=tags["artist"])
        return tags

    info = getattr(audio, "info", None)
    if info is not None:
        if getattr(info, "length", None):
            tags["duration"] = float(info.length)
        if getattr(info, "sample_rate", None):
            tags["sample_rate"] = int(info.sample_rate)
        if getattr(info, "channels", None):
            tags["channels"] = int(info.channels)
        if getattr(info, "bits_per_sample", None):
            tags["bit_depth"] = int(info.bits_per_sample)
        elif path.suffix.lower() == ".mp3":
            tags["bit_depth"] = 16
        br = getattr(info, "bitrate", None)
        if br:
            tags["bitrate"] = int(br) // 1000 if br > 10000 else int(br)

    if path.suffix.lower() == ".flac":
        tags["lossless"] = 1
        tags["codec"] = "flac"
    elif path.suffix.lower() == ".mp3":
        tags["lossless"] = 0
        tags["codec"] = "mp3"
    else:
        tags["lossless"] = 0
        tags["codec"] = path.suffix.lower().lstrip(".") or "unknown"

    if audio.tags is None:
        tags["title"] = clean_title(tags["title"], path_stem=path.stem)
        tags["artist"] = clean_artist(tags["artist"])
        tags["album"] = clean_album(tags["album"], artist=tags["artist"])
        return tags

    artist = _first(audio.tags.get("artist")) or _first(audio.tags.get("albumartist"))
    album = _first(audio.tags.get("album"))
    title = _first(audio.tags.get("title"))
    track_num = _parse_track_num(audio.tags.get("tracknumber"))
    genre = _parse_genre(audio.tags.get("genre"))
    year = _parse_year(audio.tags.get("date")) or _parse_year(audio.tags.get("year"))

    if artist:
        tags["artist"] = artist.strip()
    if album:
        tags["album"] = album.strip()
    if title:
        tags["title"] = title.strip()
    tags["track_num"] = track_num
    tags["genre"] = genre
    tags["year"] = year

    # Persist ASCII-safe names so PSP never sees raw Cyrillic / URL-encoding.
    tags["title"] = clean_title(tags["title"], path_stem=path.stem)
    tags["artist"] = clean_artist(tags["artist"])
    tags["album"] = clean_album(tags["album"], artist=tags["artist"])
    if genre:
        from .translit import to_latin

        tags["genre"] = to_latin(genre, fallback=genre)
    return tags


def scan_library(*, fetch_covers: bool = True, fetch_metadata: bool = False) -> dict[str, int]:
    """Full rescan. Keeps ratings for paths that still exist."""
    from .covers import ensure_album_cover
    from .metadata import aggregate_album_stats, enrich_album, enrich_artist

    init_db()
    music_dir = settings.music_dir.resolve()
    if not music_dir.is_dir():
        raise FileNotFoundError(f"MUSIC_DIR not found: {music_dir}")

    seen_paths: set[str] = set()
    added = 0
    updated = 0
    processed = 0

    try:
        with get_db() as conn:
            set_meta(conn, "scan_status", "running")
            set_meta(conn, "scan_started", str(time.time()))
            set_meta(conn, "scan_error", "")
            set_meta(conn, "scan_progress", "0")

            for path in music_dir.rglob("*"):
                if not path.is_file() or path.suffix.lower() not in AUDIO_EXTS:
                    continue
                rel = path.relative_to(music_dir).as_posix()
                seen_paths.add(rel)
                tags = read_tags(path)
                artist_id = get_or_create_artist(conn, tags["artist"])
                album_id = get_or_create_album(conn, artist_id, tags["album"])
                existing = conn.execute(
                    "SELECT id FROM tracks WHERE path = ?", (rel,)
                ).fetchone()
                upsert_track(
                    conn,
                    path=rel,
                    artist_id=artist_id,
                    album_id=album_id,
                    title=tags["title"],
                    track_num=tags["track_num"],
                    duration=tags["duration"],
                    genre=tags.get("genre"),
                    year=tags.get("year"),
                    codec=tags.get("codec"),
                    sample_rate=tags.get("sample_rate"),
                    bit_depth=tags.get("bit_depth"),
                    channels=tags.get("channels"),
                    bytes_=tags.get("bytes"),
                    lossless=tags.get("lossless"),
                    bitrate=tags.get("bitrate"),
                )
                if existing:
                    updated += 1
                else:
                    added += 1
                processed += 1
                if processed % 5 == 0:
                    set_meta(conn, "scan_progress", str(processed))
                    conn.commit()

            rows = conn.execute("SELECT id, path FROM tracks").fetchall()
            removed = 0
            for row in rows:
                if row["path"] not in seen_paths:
                    conn.execute("DELETE FROM tracks WHERE id = ?", (row["id"],))
                    removed += 1

            conn.execute(
                "DELETE FROM albums WHERE id NOT IN (SELECT DISTINCT album_id FROM tracks)"
            )
            conn.execute(
                "DELETE FROM artists WHERE id NOT IN (SELECT DISTINCT artist_id FROM tracks)"
            )

            aggregate_album_stats(conn)

            # File pass done — expose real total immediately (UI used to stick on 50).
            set_meta(conn, "scan_progress", str(processed))
            set_meta(conn, "scan_phase", "files_done")
            set_meta(
                conn,
                "scan_result",
                json.dumps(
                    {
                        "added": added,
                        "updated": updated,
                        "removed": removed,
                        "total": processed,
                    }
                ),
            )
            conn.commit()

            if fetch_covers:
                set_meta(conn, "scan_phase", "covers")
                conn.commit()
                albums = conn.execute(
                    "SELECT a.id, a.name, ar.name AS artist_name FROM albums a "
                    "JOIN artists ar ON ar.id = a.artist_id"
                ).fetchall()
                for i, album in enumerate(albums, start=1):
                    try:
                        ensure_album_cover(
                            conn,
                            album_id=int(album["id"]),
                            artist=album["artist_name"],
                            album=album["name"],
                        )
                    except Exception:
                        pass
                    if i % 3 == 0 or i == len(albums):
                        set_meta(conn, "scan_progress", str(processed))
                        set_meta(conn, "scan_phase", f"covers {i}/{len(albums)}")
                        conn.commit()
                    time.sleep(0.05)

            if fetch_metadata:
                set_meta(conn, "scan_phase", "metadata")
                conn.commit()
                albums = conn.execute(
                    "SELECT a.id, a.name, ar.id AS artist_id, ar.name AS artist_name FROM albums a "
                    "JOIN artists ar ON ar.id = a.artist_id"
                ).fetchall()
                seen_artists: set[int] = set()
                for i, album in enumerate(albums, start=1):
                    try:
                        enrich_album(
                            conn,
                            album_id=int(album["id"]),
                            artist=album["artist_name"],
                            album=album["name"],
                        )
                        aid = int(album["artist_id"])
                        if aid not in seen_artists:
                            seen_artists.add(aid)
                            enrich_artist(conn, artist_id=aid, name=album["artist_name"])
                    except Exception:
                        pass
                    if i % 2 == 0 or i == len(albums):
                        set_meta(conn, "scan_progress", str(processed))
                        set_meta(conn, "scan_phase", f"metadata {i}/{len(albums)}")
                        conn.commit()
                    time.sleep(0.15)
                aggregate_album_stats(conn)

            total = conn.execute("SELECT COUNT(*) AS c FROM tracks").fetchone()["c"]
            result = {
                "added": added,
                "updated": updated,
                "removed": removed,
                "total": int(total),
            }
            set_meta(conn, "scan_progress", str(total))
            set_meta(conn, "scan_phase", "done")
            set_meta(conn, "scan_result", json.dumps(result))
            set_meta(conn, "scan_status", "idle")
            set_meta(conn, "scan_finished", str(time.time()))
            set_meta(conn, "track_count", str(total))
        return result
    except Exception:
        with get_db() as conn:
            set_meta(conn, "scan_status", "idle")
            set_meta(conn, "scan_error", "scan failed")
            set_meta(conn, "scan_phase", "error")
            set_meta(conn, "scan_finished", str(time.time()))
        raise


def scan_status() -> dict[str, Any]:
    with get_db() as conn:
        result_raw = get_meta(conn, "scan_result", "")
        result: dict[str, Any] = {}
        if result_raw:
            try:
                parsed = json.loads(result_raw)
                if isinstance(parsed, dict):
                    result = parsed
            except json.JSONDecodeError:
                result = {}
        progress_raw = get_meta(conn, "scan_progress", "0") or "0"
        try:
            progress_val: Any = int(str(progress_raw).split("+", 1)[0].strip() or "0")
        except ValueError:
            progress_val = progress_raw
        return {
            "status": get_meta(conn, "scan_status", "idle"),
            "phase": get_meta(conn, "scan_phase", ""),
            "started": get_meta(conn, "scan_started", ""),
            "finished": get_meta(conn, "scan_finished", ""),
            "track_count": get_meta(conn, "track_count", "0"),
            "progress": progress_val,
            "error": get_meta(conn, "scan_error", ""),
            **result,
        }


def start_scan_background(*, fetch_covers: bool = True, fetch_metadata: bool = False) -> dict[str, Any]:
    global _scan_thread
    with _scan_lock:
        current = scan_status()
        if current.get("status") == "running":
            return {"ok": True, "status": "running", "already": True, "phase": current.get("phase")}
        with get_db() as conn:
            set_meta(conn, "scan_status", "running")
            set_meta(conn, "scan_phase", "files")
            set_meta(conn, "scan_error", "")
            set_meta(conn, "scan_progress", "0")

        def _run() -> None:
            try:
                scan_library(fetch_covers=fetch_covers, fetch_metadata=fetch_metadata)
            except FileNotFoundError as exc:
                with get_db() as conn:
                    set_meta(conn, "scan_status", "idle")
                    set_meta(conn, "scan_phase", "error")
                    set_meta(conn, "scan_error", str(exc))
            except Exception as exc:
                with get_db() as conn:
                    set_meta(conn, "scan_status", "idle")
                    set_meta(conn, "scan_phase", "error")
                    set_meta(conn, "scan_error", str(exc)[:200])

        _scan_thread = threading.Thread(target=_run, daemon=True, name="library-scan")
        _scan_thread.start()
        return {"ok": True, "status": "running"}
