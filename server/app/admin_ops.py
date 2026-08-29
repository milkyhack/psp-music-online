"""Admin diagnostics and settings persistence."""

from __future__ import annotations

import logging
import re
from pathlib import Path
from typing import Any, Optional

from .config import apply_runtime_settings, settings
from .db import AUDIO_EXTS, get_db, get_meta, resolve_path, set_meta
from .ffmpeg_util import ffmpeg_info

_SERVER_ROOT = Path(__file__).resolve().parent.parent
ENV_PATH = _SERVER_ROOT / ".env"

# Settings that belong in .env (host/port need process restart to bind)
ENV_KEYS = ("MUSIC_DIR", "HOST", "PORT", "API_KEY")

STREAM_BITRATE = "native (FLAC/MP3)"
STREAM_SAMPLE_RATE = "source"
STREAM_CHANNELS = 2


def _format_bytes(n: int) -> str:
    units = ["B", "KB", "MB", "GB"]
    size = float(n)
    for unit in units:
        if size < 1024 or unit == units[-1]:
            if unit == "B":
                return f"{int(size)} {unit}"
            return f"{size:.1f} {unit}"
        size /= 1024
    return f"{n} B"


def _quote_env(value: str) -> str:
    if any(ch in value for ch in ' \t#"\'\\'):
        return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'
    return value


def normalize_music_dir(raw: str) -> Path:
    path = Path(raw.strip()).expanduser()
    if not path.is_absolute():
        path = (_SERVER_ROOT / path).resolve()
    else:
        try:
            path = path.resolve()
        except OSError:
            path = path
    # macOS Music often uses Media.localized; admins type ".../Media" by mistake.
    if not path.is_dir():
        localized = path.with_name(path.name + ".localized")
        if localized.is_dir():
            return localized
        if not path.name.endswith(".localized"):
            alt = path.parent / (path.name + ".localized")
            if alt.is_dir():
                return alt
    return path


def lan_addresses() -> list[str]:
    import socket

    found: list[str] = []
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.connect(("8.8.8.8", 80))
        ip = sock.getsockname()[0]
        sock.close()
        if ip and not ip.startswith("127."):
            found.append(ip)
    except OSError:
        pass
    try:
        hostname = socket.gethostname()
        for info in socket.getaddrinfo(hostname, None, socket.AF_INET):
            ip = info[4][0]
            if ip.startswith("127.") or ip in found:
                continue
            found.append(ip)
    except OSError:
        pass
    return found


def psp_setup_info() -> dict[str, Any]:
    """IP + port to enter on PSP (Setup IP/Port) and in server.cfg."""
    ips = lan_addresses()
    port = int(settings.port)
    primary = ips[0] if ips else ""
    setup_line = f"{primary} {port}" if primary else ""
    return {
        "lan_ips": ips,
        "port": port,
        "primary_ip": primary,
        "setup_line": setup_line,
        "server_cfg_line": setup_line,
        "hint": (
            f"На PSP в Setup IP/Port: {setup_line}"
            if setup_line
            else f"LAN IP не найден — укажи IP этого ПК и порт {port}"
        ),
        "admin_local": f"http://127.0.0.1:{port}/",
        "admin_lan": f"http://{primary}:{port}/" if primary else None,
    }


def print_startup_banner() -> None:
    info = psp_setup_info()
    port = info["port"]
    log = logging.getLogger("psp.server")
    lines = [
        "=" * 60,
        "  PSP Music Server",
        f"  Админка:  http://127.0.0.1:{port}/",
    ]
    if info["setup_line"]:
        lines.append(f"  PSP Setup IP/Port:  {info['setup_line']}")
        lines.append(f"  server.cfg: {info['setup_line']}")
    else:
        lines.append(f"  PSP Setup IP/Port:  <IP_этого_ПК> {port}")
        lines.append("  LAN IP не определился — посмотри в настройках роутера.")
    if len(info["lan_ips"]) > 1:
        lines.append(f"  Другие LAN IP: {', '.join(info['lan_ips'][1:])}")
    lines.append(f"  Музыка: {settings.music_dir}")
    lines.append("=" * 60)
    for line in lines:
        log.info(line)
        print(line, flush=True)


def stream_health() -> dict[str, Any]:
    with get_db() as conn:
        row = conn.execute("SELECT id, path FROM tracks ORDER BY id LIMIT 1").fetchone()
        total = conn.execute("SELECT COUNT(*) AS c FROM tracks").fetchone()["c"]
    if not row:
        return {"ok": False, "reason": "empty_catalog", "tracks": 0}
    path = resolve_path(row["path"])
    missing = 0
    with get_db() as conn:
        sample = conn.execute("SELECT path FROM tracks LIMIT 40").fetchall()
    for r in sample:
        if not resolve_path(r["path"]).is_file():
            missing += 1
    return {
        "ok": path.is_file() and missing == 0,
        "tracks": total,
        "sample_id": row["id"],
        "sample_path": str(path),
        "sample_exists": path.is_file(),
        "missing_in_sample": missing,
        "sample_size": len(sample),
    }


def library_path_info() -> dict[str, Any]:
    music = settings.music_dir
    exists = music.is_dir()
    persisted = ""
    try:
        with get_db() as conn:
            persisted = get_meta(conn, "music_dir", "")
    except Exception:
        persisted = ""
    return {
        "path": str(music.expanduser()),
        "exists": exists,
        "supported_extensions": sorted(AUDIO_EXTS),
        "persisted": persisted or str(music),
        "env_file": str(ENV_PATH),
        "env_file_exists": ENV_PATH.is_file(),
        "matches_saved": (not persisted) or str(music) == persisted,
    }


def format_breakdown() -> dict[str, int]:
    counts: dict[str, int] = {}
    with get_db() as conn:
        rows = conn.execute("SELECT path FROM tracks").fetchall()
    for row in rows:
        ext = Path(row["path"]).suffix.lower() or "(none)"
        counts[ext] = counts.get(ext, 0) + 1
    return dict(sorted(counts.items(), key=lambda kv: (-kv[1], kv[0])))


def cover_stats() -> dict[str, Any]:
    with get_db() as conn:
        albums = conn.execute("SELECT COUNT(*) AS c FROM albums").fetchone()["c"]
        with_cover = conn.execute(
            "SELECT COUNT(*) AS c FROM albums WHERE cover_path IS NOT NULL AND cover_path != ''"
        ).fetchone()["c"]
        tracks_with = conn.execute(
            """
            SELECT COUNT(*) AS c FROM tracks t
            JOIN albums a ON a.id = t.album_id
            WHERE COALESCE(t.cover_path, a.cover_path) IS NOT NULL
              AND COALESCE(t.cover_path, a.cover_path) != ''
            """
        ).fetchone()["c"]
        tracks = conn.execute("SELECT COUNT(*) AS c FROM tracks").fetchone()["c"]
    cover_files = 0
    cover_bytes = 0
    if settings.covers_dir.is_dir():
        for p in settings.covers_dir.glob("*.jpg"):
            if p.is_file():
                cover_files += 1
                try:
                    cover_bytes += p.stat().st_size
                except OSError:
                    pass
    return {
        "albums_total": albums,
        "albums_with_cover": with_cover,
        "albums_missing_cover": max(0, albums - with_cover),
        "tracks_with_cover": tracks_with,
        "tracks_total": tracks,
        "cover_files_on_disk": cover_files,
        "covers_size_bytes": cover_bytes,
        "covers_size_human": _format_bytes(cover_bytes),
        "covers_dir": str(settings.covers_dir),
        "sources": ["embedded (MP3/FLAC/MP4/OGG)", "sidecar cover.jpg/folder.jpg", "iTunes Search API"],
    }


def cache_stats() -> dict[str, Any]:
    files = 0
    size = 0
    if settings.cache_dir.is_dir():
        for p in settings.cache_dir.glob("*.mp3"):
            if p.is_file():
                files += 1
                try:
                    size += p.stat().st_size
                except OSError:
                    pass
    return {
        "files": files,
        "size_bytes": size,
        "size_human": _format_bytes(size),
        "cache_dir": str(settings.cache_dir),
    }


def capabilities() -> dict[str, Any]:
    lib = library_path_info()
    ff = ffmpeg_info()
    formats = format_breakdown()
    flac_count = formats.get(".flac", 0)
    needs_transcode = sum(
        c for ext, c in formats.items() if ext != ".mp3"
    )
    return {
        "library_path_known": True,
        "library_dir_exists": lib["exists"],
        "flac_supported": ".flac" in AUDIO_EXTS,
        "flac_tracks_in_db": flac_count,
        "transcode_needed_tracks": needs_transcode,
        "covers_embedded": True,
        "covers_itunes": True,
        "stream_mp3_passthrough": True,
        "stream_transcode_ffmpeg": ff["available"],
        "ffmpeg": ff,
    }


def diagnostics() -> dict[str, Any]:
    with get_db() as conn:
        track_count = conn.execute("SELECT COUNT(*) AS c FROM tracks").fetchone()["c"]
        artist_count = conn.execute("SELECT COUNT(*) AS c FROM artists").fetchone()["c"]
        album_count = conn.execute("SELECT COUNT(*) AS c FROM albums").fetchone()["c"]
        from .db import get_meta

        scan_status = get_meta(conn, "scan_status", "idle")
        scan_finished = get_meta(conn, "scan_finished", "")
    return {
        "library": library_path_info(),
        "counts": {
            "tracks": track_count,
            "artists": artist_count,
            "albums": album_count,
        },
        "formats": format_breakdown(),
        "covers": cover_stats(),
        "streaming": {
            "bitrate": STREAM_BITRATE,
            "sample_rate": STREAM_SAMPLE_RATE,
            "channels": STREAM_CHANNELS,
            "target_format": "audio/flac or audio/mpeg (no FLAC to MP3)",
            "passthrough": [".flac", ".mp3"],
            "transcode_via": "disabled",
            "ffmpeg": ffmpeg_info(),
            "cache": cache_stats(),
        },
        "capabilities": capabilities(),
        "scan_status": scan_status,
        "scan_finished": scan_finished,
        "data_dir": str(settings.data_dir),
        "db_path": str(settings.db_path),
        "stream_health": stream_health(),
        "psp_setup": psp_setup_info(),
        "listen": {"host": settings.host, "port": settings.port},
    }


def get_settings_view() -> dict[str, Any]:
    persisted = ""
    try:
        with get_db() as conn:
            persisted = get_meta(conn, "music_dir", "")
    except Exception:
        persisted = ""
    return {
        "music_dir": str(settings.music_dir),
        "music_dir_exists": settings.music_dir.is_dir(),
        "music_dir_persisted": persisted or str(settings.music_dir),
        "env_path": str(ENV_PATH),
        "host": settings.host,
        "port": settings.port,
        "api_key_set": bool(settings.api_key),
        "api_key": settings.api_key,
        "data_dir": str(settings.data_dir),
        "psp_setup": psp_setup_info(),
        "stream_bitrate": STREAM_BITRATE,
        "restart_required_for": ["host", "port"],
        "notes": {
            "music_dir": "Сохраняется в .env и в SQLite. После рестарта поднимается из базы.",
            "host": "Bind address. Restart server after change.",
            "port": "HTTP port. Restart server after change.",
            "api_key": "If set, clients must send X-Api-Key. Leave empty for LAN.",
            "stream_bitrate": "FLAC and MP3 served as-is. No lossy transcode of lossless.",
        },
    }


def _read_env_lines() -> list[str]:
    if not ENV_PATH.is_file():
        return []
    return ENV_PATH.read_text(encoding="utf-8").splitlines()


def _write_env(values: dict[str, str]) -> None:
    lines = _read_env_lines()
    seen: set[str] = set()
    out: list[str] = []
    for line in lines:
        m = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)=(.*)$", line)
        if not m:
            out.append(line)
            continue
        key = m.group(1)
        if key in values:
            out.append(f"{key}={_quote_env(values[key])}")
            seen.add(key)
        else:
            out.append(line)
    for key in ENV_KEYS:
        if key in values and key not in seen:
            out.append(f"{key}={_quote_env(values[key])}")
    text = "\n".join(out).rstrip() + "\n"
    ENV_PATH.write_text(text, encoding="utf-8")


def load_persisted_music_dir() -> None:
    """Restore MUSIC_DIR from sqlite (admin save) then .env."""
    stored = ""
    try:
        with get_db() as conn:
            stored = get_meta(conn, "music_dir", "")
    except Exception:
        stored = ""
    if stored:
        path = normalize_music_dir(stored)
        apply_runtime_settings(music_dir=path)
        if str(path) != stored:
            try:
                with get_db() as conn:
                    set_meta(conn, "music_dir", str(path))
                _write_env(
                    {
                        "MUSIC_DIR": str(path),
                        "HOST": settings.host,
                        "PORT": str(settings.port),
                        "API_KEY": settings.api_key,
                    }
                )
            except Exception:
                pass
        return
    env_dir = settings.music_dir
    if env_dir:
        path = normalize_music_dir(str(env_dir))
        apply_runtime_settings(music_dir=path)
        with get_db() as conn:
            set_meta(conn, "music_dir", str(path))


def apply_settings(
    *,
    music_dir: Optional[str] = None,
    host: Optional[str] = None,
    port: Optional[int] = None,
    api_key: Optional[str] = None,
) -> dict[str, Any]:
    current = {
        "MUSIC_DIR": str(settings.music_dir),
        "HOST": settings.host,
        "PORT": str(settings.port),
        "API_KEY": settings.api_key,
    }
    restart_needed = False

    if music_dir is not None:
        path = normalize_music_dir(music_dir)
        current["MUSIC_DIR"] = str(path)
        apply_runtime_settings(music_dir=path)
        with get_db() as conn:
            set_meta(conn, "music_dir", str(path))

    if host is not None:
        host = host.strip() or "0.0.0.0"
        if host != settings.host:
            restart_needed = True
        current["HOST"] = host
        apply_runtime_settings(host=host)

    if port is not None:
        if int(port) != settings.port:
            restart_needed = True
        current["PORT"] = str(int(port))
        apply_runtime_settings(port=int(port))

    if api_key is not None:
        current["API_KEY"] = api_key
        apply_runtime_settings(api_key=api_key)

    _write_env(current)

    return {
        "ok": True,
        "restart_needed": restart_needed,
        "saved_music_dir": str(settings.music_dir),
        "music_dir_exists": settings.music_dir.is_dir(),
        "settings": get_settings_view(),
    }


def clear_transcode_cache() -> dict[str, Any]:
    removed = 0
    if settings.cache_dir.is_dir():
        for p in settings.cache_dir.glob("*.mp3"):
            try:
                p.unlink()
                removed += 1
            except OSError:
                pass
        for p in settings.cache_dir.glob("*.tmp.mp3"):
            try:
                p.unlink()
                removed += 1
            except OSError:
                pass
    return {"ok": True, "removed": removed, "cache": cache_stats()}


def recent_tracks(limit: int = 30) -> list[dict[str, Any]]:
    with get_db() as conn:
        rows = conn.execute(
            """
            SELECT t.id, t.title, t.path, t.duration,
                   ar.name AS artist, al.name AS album,
                   COALESCE(t.cover_path, al.cover_path) AS cover_path
            FROM tracks t
            JOIN artists ar ON ar.id = t.artist_id
            JOIN albums al ON al.id = t.album_id
            ORDER BY t.id DESC
            LIMIT ?
            """,
            (limit,),
        ).fetchall()
    out = []
    for r in rows:
        ext = Path(r["path"]).suffix.lower()
        out.append(
            {
                "id": r["id"],
                "title": r["title"],
                "artist": r["artist"],
                "album": r["album"],
                "path": r["path"],
                "format": ext.lstrip(".") or "unknown",
                "ext": ext,
                "duration": r["duration"],
                "needs_transcode": ext != ".mp3",
                "has_cover": bool(r["cover_path"]),
                "stream_url": f"/api/stream/{r['id']}",
                "cover_url": f"/api/covers/{r['id']}" if r["cover_path"] else None,
            }
        )
    return out
