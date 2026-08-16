from __future__ import annotations

import hmac
from pathlib import Path
from typing import Any, Optional
from io import BytesIO

import json

from fastapi import Depends, FastAPI, Header, HTTPException, Query, Request
from fastapi.responses import FileResponse, Response
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field
from PIL import Image

from . import __version__
from . import admin_ops
from .catalog import (
    album_row,
    artist_row,
    browse_summary,
    get_album_detail,
    get_artist_detail,
    list_genres,
    track_row,
)
from .config import settings
from .covers import backfill_missing_covers, cover_abs_path, ensure_album_cover, ensure_cover_for_track
from .db import get_db, get_meta, init_db, resolve_path, set_meta
from .metadata import aggregate_album_stats, backfill_metadata
from .scanner import scan_status, start_scan_background
from .stream import stream_file

STATIC_DIR = Path(__file__).resolve().parent / "static"
ADMIN_DIR = STATIC_DIR / "admin"

app = FastAPI(title="PSP Music Server", version=__version__)


def require_api_key(x_api_key: Optional[str] = Header(default=None)) -> None:
    if not settings.api_key:
        return
    provided = x_api_key or ""
    expected = settings.api_key
    if len(provided) != len(expected) or not hmac.compare_digest(provided, expected):
        raise HTTPException(status_code=401, detail="invalid or missing X-Api-Key")


class RatingBody(BaseModel):
    rating: int = Field(ge=0, le=5)


class PlayBody(BaseModel):
    track_id: int
    ms_listened: int = Field(default=0, ge=0)
    completed: bool = False


class PlaylistBody(BaseModel):
    name: str = Field(min_length=1, max_length=120)


class PlaylistTrackBody(BaseModel):
    track_id: int


class SettingsBody(BaseModel):
    music_dir: Optional[str] = None
    host: Optional[str] = None
    port: Optional[int] = Field(default=None, ge=1, le=65535)
    api_key: Optional[str] = None


def _track_urls(track_id: int) -> dict[str, str]:
    return {
        "stream_url": f"/api/stream/{track_id}",
        "cover_url": f"/api/covers/{track_id}",
    }


def _like_escape(q: str) -> str:
    return q.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_")


def _album_sort_sql(sort: str) -> str:
    mapping = {
        "name": "al.name COLLATE NOCASE",
        "year": "al.year IS NULL, al.year DESC",
        "genre": "al.genre COLLATE NOCASE, al.name COLLATE NOCASE",
        "rating": "al.user_rating IS NULL, al.user_rating DESC",
        "plays": "al.play_count DESC, al.name COLLATE NOCASE",
        "external": "al.external_score IS NULL, al.external_score DESC",
    }
    return mapping.get(sort, "al.name COLLATE NOCASE")


@app.on_event("startup")
def on_startup() -> None:
    settings.data_dir.mkdir(parents=True, exist_ok=True)
    settings.covers_dir.mkdir(parents=True, exist_ok=True)
    settings.cache_dir.mkdir(parents=True, exist_ok=True)
    init_db()
    admin_ops.load_persisted_music_dir()
    admin_ops.print_startup_banner()


@app.get("/api/status")
def status(_: None = Depends(require_api_key)) -> dict[str, Any]:
    with get_db() as conn:
        track_count = conn.execute("SELECT COUNT(*) AS c FROM tracks").fetchone()["c"]
        artist_count = conn.execute("SELECT COUNT(*) AS c FROM artists").fetchone()["c"]
        album_count = conn.execute("SELECT COUNT(*) AS c FROM albums").fetchone()["c"]
        scan_status = get_meta(conn, "scan_status", "idle")
    ff = admin_ops.ffmpeg_info()
    payload = {
        "version": __version__,
        "music_dir": str(settings.music_dir),
        "music_dir_exists": settings.music_dir.is_dir(),
        "tracks": track_count,
        "artists": artist_count,
        "albums": album_count,
        "scan_status": scan_status,
        "ffmpeg": ff["available"],
        "formats": admin_ops.format_breakdown(),
        "psp_setup": admin_ops.psp_setup_info(),
    }
    return payload


@app.get("/api/admin/diagnostics")
def admin_diagnostics(_: None = Depends(require_api_key)) -> dict[str, Any]:
    return {"version": __version__, **admin_ops.diagnostics()}


@app.get("/api/admin/settings")
def admin_get_settings(_: None = Depends(require_api_key)) -> dict[str, Any]:
    return admin_ops.get_settings_view()


@app.put("/api/admin/settings")
def admin_put_settings(
    body: SettingsBody,
    _: None = Depends(require_api_key),
) -> dict[str, Any]:
    return admin_ops.apply_settings(
        music_dir=body.music_dir,
        host=body.host,
        port=body.port,
        api_key=body.api_key,
    )


@app.get("/api/admin/tracks")
def admin_tracks(
    limit: int = Query(40, ge=1, le=200),
    _: None = Depends(require_api_key),
) -> list[dict[str, Any]]:
    return admin_ops.recent_tracks(limit=limit)


@app.post("/api/admin/cache/warm")
def admin_warm_cache(
    limit: int = Query(200, ge=1, le=2000),
    _: None = Depends(require_api_key),
) -> dict[str, Any]:
    """Pre-transcode non-native-MP3 tracks (incl. FLAC) so PSP never waits on first play."""
    from .stream import ensure_mp3, is_mp3

    done = 0
    skipped = 0
    errors: list[str] = []
    with get_db() as conn:
        rows = conn.execute(
            "SELECT id, path FROM tracks ORDER BY id LIMIT ?",
            (limit,),
        ).fetchall()
    for row in rows:
        try:
            path = resolve_path(row["path"])
            if not path.is_file():
                skipped += 1
                continue
            if is_mp3(path):
                skipped += 1
                continue
            ensure_mp3(path)
            done += 1
        except Exception as exc:  # noqa: BLE001
            errors.append(f"{row['id']}: {exc}")
    return {"ok": True, "warmed": done, "skipped": skipped, "errors": errors[:8]}


@app.post("/api/admin/cache/clear")
def admin_clear_cache(_: None = Depends(require_api_key)) -> dict[str, Any]:
    return admin_ops.clear_transcode_cache()


@app.post("/api/scan")
def scan(
    fetch_covers: bool = Query(default=True),
    fetch_metadata: bool = Query(default=True),
    _: None = Depends(require_api_key),
) -> dict[str, Any]:
    if not settings.music_dir.is_dir():
        raise HTTPException(status_code=400, detail=f"MUSIC_DIR not found: {settings.music_dir}")
    return start_scan_background(fetch_covers=fetch_covers, fetch_metadata=fetch_metadata)


@app.get("/api/scan/status")
def scan_status_endpoint(_: None = Depends(require_api_key)) -> dict[str, Any]:
    st = scan_status()
    return st


@app.post("/api/metadata/backfill")
def metadata_backfill(
    limit: int = Query(default=0, ge=0, le=5000),
    _: None = Depends(require_api_key),
) -> dict[str, Any]:
    result = backfill_metadata(limit=limit, pause_s=0.25)
    return {"ok": True, **result}


@app.get("/api/genres")
def genres(
    limit: int = Query(100, ge=1, le=500),
    offset: int = Query(0, ge=0),
    _: None = Depends(require_api_key),
) -> list[dict[str, Any]]:
    return list_genres(limit=limit, offset=offset)


@app.get("/api/browse")
def browse(_: None = Depends(require_api_key)) -> dict[str, Any]:
    return browse_summary()


@app.get("/api/artists")
def list_artists(
    offset: int = Query(0, ge=0),
    limit: int = Query(50, ge=1, le=200),
    genre: Optional[str] = None,
    min_rating: Optional[float] = Query(default=None, ge=0, le=5),
    _: None = Depends(require_api_key),
) -> list[dict[str, Any]]:
    sql = """
        SELECT a.id, a.name, a.genre, a.bio, a.country,
               a.external_score, a.external_source,
               COUNT(t.id) AS track_count,
               COUNT(DISTINCT al.id) AS album_count
        FROM artists a
        LEFT JOIN tracks t ON t.artist_id = a.id
        LEFT JOIN albums al ON al.artist_id = a.id
        WHERE 1=1
    """
    params: list[Any] = []
    if genre:
        sql += " AND (a.genre = ? COLLATE NOCASE OR EXISTS (SELECT 1 FROM tracks tx WHERE tx.artist_id = a.id AND tx.genre = ? COLLATE NOCASE))"
        params.extend([genre, genre])
    if min_rating is not None:
        sql += " AND EXISTS (SELECT 1 FROM tracks tx WHERE tx.artist_id = a.id AND tx.rating >= ?)"
        params.append(min_rating)
    sql += " GROUP BY a.id ORDER BY a.name COLLATE NOCASE LIMIT ? OFFSET ?"
    params.extend([limit, offset])
    with get_db() as conn:
        rows = conn.execute(sql, params).fetchall()
    return [artist_row(r) for r in rows]


@app.get("/api/artists/{artist_id}")
def get_artist(artist_id: int, _: None = Depends(require_api_key)) -> dict[str, Any]:
    detail = get_artist_detail(artist_id)
    if not detail:
        raise HTTPException(status_code=404, detail="artist not found")
    return detail


@app.get("/api/albums")
def list_albums(
    artist_id: Optional[int] = None,
    genre: Optional[str] = None,
    min_user_rating: Optional[float] = Query(default=None, ge=0, le=5),
    min_play_count: Optional[int] = Query(default=None, ge=1),
    min_external_score: Optional[float] = Query(default=None, ge=0, le=100),
    sort: str = Query(default="name", pattern="^(name|year|genre|rating|plays|external)$"),
    offset: int = Query(0, ge=0),
    limit: int = Query(50, ge=1, le=200),
    _: None = Depends(require_api_key),
) -> list[dict[str, Any]]:
    sql = """
        SELECT al.id, al.name, al.artist_id, ar.name AS artist,
               al.cover_path, al.genre, al.year, al.summary,
               al.user_rating, al.external_score, al.external_source,
               COALESCE(al.play_count, 0) AS play_count,
               COUNT(t.id) AS track_count
        FROM albums al
        JOIN artists ar ON ar.id = al.artist_id
        LEFT JOIN tracks t ON t.album_id = al.id
        WHERE 1=1
    """
    params: list[Any] = []
    if artist_id is not None:
        sql += " AND al.artist_id = ?"
        params.append(artist_id)
    if genre:
        # Genres list is built from tracks.genre; album.genre is often empty/stale.
        if genre.strip().lower() in ("unknown", "unk", "n/a", "none"):
            sql += """ AND (
                al.genre IS NULL OR TRIM(al.genre) = ''
                OR EXISTS (
                    SELECT 1 FROM tracks tx
                    WHERE tx.album_id = al.id
                      AND (tx.genre IS NULL OR TRIM(tx.genre) = '')
                )
            )"""
        else:
            sql += """ AND (
                al.genre = ? COLLATE NOCASE
                OR EXISTS (
                    SELECT 1 FROM tracks tx
                    WHERE tx.album_id = al.id AND tx.genre = ? COLLATE NOCASE
                )
            )"""
            params.extend([genre, genre])
    if min_user_rating is not None:
        sql += " AND al.user_rating >= ?"
        params.append(min_user_rating)
    if min_play_count is not None:
        sql += " AND COALESCE(al.play_count, 0) >= ?"
        params.append(min_play_count)
    if min_external_score is not None:
        sql += " AND al.external_score >= ?"
        params.append(min_external_score)
    sql += f" GROUP BY al.id ORDER BY {_album_sort_sql(sort)} LIMIT ? OFFSET ?"
    params.extend([limit, offset])
    with get_db() as conn:
        rows = conn.execute(sql, params).fetchall()
    return [album_row(r) for r in rows]


@app.get("/api/albums/{album_id}")
def get_album(album_id: int, _: None = Depends(require_api_key)) -> dict[str, Any]:
    detail = get_album_detail(album_id)
    if not detail:
        raise HTTPException(status_code=404, detail="album not found")
    return detail


@app.get("/api/tracks")
def list_tracks(
    album_id: Optional[int] = None,
    artist_id: Optional[int] = None,
    genre: Optional[str] = None,
    min_rating: Optional[int] = Query(default=None, ge=0, le=5),
    min_play_count: Optional[int] = Query(default=None, ge=1),
    q: Optional[str] = None,
    sort: str = Query(default="album", pattern="^(album|rating|title|genre|plays)$"),
    offset: int = Query(0, ge=0),
    limit: int = Query(50, ge=1, le=200),
    _: None = Depends(require_api_key),
) -> list[dict[str, Any]]:
    sql = """
        SELECT t.id, t.title, t.track_num, t.duration, t.rating,
               t.album_id, t.artist_id, t.genre, t.year, t.path,
               t.codec, t.sample_rate, t.bit_depth, t.channels, t.bytes, t.lossless, t.bitrate,
               COALESCE(t.play_count, 0) AS play_count,
               COALESCE(t.listen_ms, 0) AS listen_ms,
               ar.name AS artist, al.name AS album
        FROM tracks t
        JOIN artists ar ON ar.id = t.artist_id
        JOIN albums al ON al.id = t.album_id
        WHERE 1=1
    """
    params: list[Any] = []
    if album_id is not None:
        sql += " AND t.album_id = ?"
        params.append(album_id)
    if artist_id is not None:
        sql += " AND t.artist_id = ?"
        params.append(artist_id)
    if genre:
        if genre.strip().lower() in ("unknown", "unk", "n/a", "none"):
            sql += " AND (t.genre IS NULL OR TRIM(t.genre) = '')"
        else:
            sql += " AND t.genre = ? COLLATE NOCASE"
            params.append(genre)
    if min_rating is not None:
        sql += " AND t.rating >= ?"
        params.append(min_rating)
    if min_play_count is not None:
        sql += " AND COALESCE(t.play_count, 0) >= ?"
        params.append(min_play_count)
    if q:
        like = f"%{_like_escape(q)}%"
        sql += (
            " AND (t.title LIKE ? ESCAPE '\\' OR ar.name LIKE ? ESCAPE '\\'"
            " OR al.name LIKE ? ESCAPE '\\' OR t.genre LIKE ? ESCAPE '\\')"
        )
        params.extend([like, like, like, like])
    order = {
        "album": "al.name COLLATE NOCASE, t.track_num IS NULL, t.track_num",
        "rating": "t.rating DESC, t.title COLLATE NOCASE",
        "title": "t.title COLLATE NOCASE",
        "genre": "t.genre COLLATE NOCASE, t.title COLLATE NOCASE",
        "plays": "t.play_count DESC, t.rating DESC, t.title COLLATE NOCASE",
    }[sort]
    sql += f" ORDER BY {order} LIMIT ? OFFSET ?"
    params.extend([limit, offset])
    with get_db() as conn:
        rows = conn.execute(sql, params).fetchall()
    return [track_row(r) for r in rows]


@app.get("/api/tracks/{track_id}")
def get_track(track_id: int, _: None = Depends(require_api_key)) -> dict[str, Any]:
    with get_db() as conn:
        r = conn.execute(
            """
            SELECT t.id, t.title, t.track_num, t.duration, t.rating, t.path,
                   t.album_id, t.artist_id, ar.name AS artist, al.name AS album
            FROM tracks t
            JOIN artists ar ON ar.id = t.artist_id
            JOIN albums al ON al.id = t.album_id
            WHERE t.id = ?
            """,
            (track_id,),
        ).fetchone()
    if not r:
        raise HTTPException(status_code=404, detail="track not found")
    return {
        "id": r["id"],
        "title": r["title"],
        "track_num": r["track_num"],
        "duration": r["duration"],
        "rating": r["rating"],
        "album_id": r["album_id"],
        "artist_id": r["artist_id"],
        "artist": r["artist"],
        "album": r["album"],
        **_track_urls(r["id"]),
    }


@app.put("/api/tracks/{track_id}/rating")
def set_rating(
    track_id: int,
    body: RatingBody,
    _: None = Depends(require_api_key),
) -> dict[str, Any]:
    with get_db() as conn:
        cur = conn.execute(
            "UPDATE tracks SET rating = ? WHERE id = ?",
            (body.rating, track_id),
        )
        if cur.rowcount == 0:
            raise HTTPException(status_code=404, detail="track not found")
        row = conn.execute("SELECT album_id FROM tracks WHERE id = ?", (track_id,)).fetchone()
        if row:
            aggregate_album_stats(conn, album_id=int(row["album_id"]))
    return {"id": track_id, "rating": body.rating}


@app.post("/api/plays")
def record_play(
    body: PlayBody,
    _: None = Depends(require_api_key),
) -> dict[str, Any]:
    """PSP reports listen time; counts as a play at 30s or track completed."""
    ms = int(body.ms_listened or 0)
    count_play = body.completed or ms >= 30000
    with get_db() as conn:
        row = conn.execute(
            "SELECT id, album_id, COALESCE(play_count, 0) AS play_count FROM tracks WHERE id = ?",
            (body.track_id,),
        ).fetchone()
        if not row:
            return {"ok": False, "ignored": True, "track_id": body.track_id}
        album_id = int(row["album_id"])
        if count_play:
            conn.execute(
                """
                UPDATE tracks
                SET play_count = COALESCE(play_count, 0) + 1,
                    listen_ms = COALESCE(listen_ms, 0) + ?,
                    last_played = strftime('%Y-%m-%dT%H:%M:%SZ','now')
                WHERE id = ?
                """,
                (ms, body.track_id),
            )
            conn.execute(
                """
                UPDATE albums
                SET play_count = COALESCE(play_count, 0) + 1
                WHERE id = ?
                """,
                (album_id,),
            )
        elif ms > 0:
            conn.execute(
                """
                UPDATE tracks
                SET listen_ms = COALESCE(listen_ms, 0) + ?,
                    last_played = strftime('%Y-%m-%dT%H:%M:%SZ','now')
                WHERE id = ?
                """,
                (ms, body.track_id),
            )
        out = conn.execute(
            "SELECT COALESCE(play_count, 0) AS play_count, COALESCE(listen_ms, 0) AS listen_ms FROM tracks WHERE id = ?",
            (body.track_id,),
        ).fetchone()
    return {
        "id": body.track_id,
        "counted": bool(count_play),
        "play_count": int(out["play_count"]),
        "listen_ms": int(out["listen_ms"]),
    }


@app.get("/api/top")
def top_rated(
    limit: int = Query(50, ge=1, le=200),
    _: None = Depends(require_api_key),
) -> list[dict[str, Any]]:
    with get_db() as conn:
        rows = conn.execute(
            """
            SELECT t.id, t.title, t.track_num, t.duration, t.rating,
                   t.album_id, t.artist_id,
                   COALESCE(t.play_count, 0) AS play_count,
                   COALESCE(t.listen_ms, 0) AS listen_ms,
                   ar.name AS artist, al.name AS album
            FROM tracks t
            JOIN artists ar ON ar.id = t.artist_id
            JOIN albums al ON al.id = t.album_id
            WHERE COALESCE(t.play_count, 0) > 0 OR t.rating > 0
            ORDER BY t.play_count DESC, t.rating DESC, t.title COLLATE NOCASE
            LIMIT ?
            """,
            (limit,),
        ).fetchall()
    return [track_row(r) for r in rows]


@app.post("/api/covers/backfill")
def covers_backfill(
    limit: int = Query(default=0, ge=0, le=5000),
    _: None = Depends(require_api_key),
) -> dict[str, Any]:
    """Fetch covers for albums still missing art (iTunes / embedded / sidecars)."""
    result = backfill_missing_covers(limit=limit, pause_s=0.2)
    return {"ok": True, **result}


def _resolve_track_cover_path(track_id: int):
    with get_db() as conn:
        # Ensure track exists; ensure_cover_for_track returns None if missing track
        exists = conn.execute("SELECT id FROM tracks WHERE id = ?", (track_id,)).fetchone()
        if not exists:
            raise HTTPException(status_code=404, detail="track not found")
        rel = ensure_cover_for_track(conn, track_id)
    path = cover_abs_path(rel)
    if not path:
        raise HTTPException(status_code=404, detail="cover not found")
    return path


def _resolve_album_cover_path(album_id: int) -> Path:
    from .covers import ensure_album_cover

    with get_db() as conn:
        r = conn.execute(
            """
            SELECT a.cover_path, a.name AS album, ar.name AS artist
            FROM albums a
            JOIN artists ar ON ar.id = a.artist_id
            WHERE a.id = ?
            """,
            (album_id,),
        ).fetchone()
        if not r:
            raise HTTPException(status_code=404, detail="album not found")
        path = cover_abs_path(r["cover_path"])
        if not path:
            rel = ensure_album_cover(
                conn,
                album_id=album_id,
                artist=r["artist"] or "",
                album=r["album"] or "",
            )
            path = cover_abs_path(rel)
    if not path:
        raise HTTPException(status_code=404, detail="cover not found")
    return path


@app.get("/api/covers/album/{album_id}")
def cover_for_album(album_id: int, _: None = Depends(require_api_key)):
    path = _resolve_album_cover_path(album_id)
    return FileResponse(path, media_type="image/jpeg")


def _thumbnail_response(path: Path, size: int, fmt: str) -> Response:
    try:
        with Image.open(path) as source:
            cover = source.convert("RGB")
            cover.thumbnail((size, size), Image.Resampling.LANCZOS)
            canvas = Image.new("RGB", (size, size), (20, 20, 24))
            x = (size - cover.width) // 2
            y = (size - cover.height) // 2
            canvas.paste(cover, (x, y))
            output = BytesIO()
            if fmt == "bmp":
                canvas.save(output, format="BMP")
                media_type = "image/bmp"
            else:
                canvas.save(output, format="PNG", optimize=True)
                media_type = "image/png"
    except OSError as exc:
        raise HTTPException(status_code=500, detail="cover thumbnail failed") from exc
    return Response(
        content=output.getvalue(),
        media_type=media_type,
        headers={"Cache-Control": "public, max-age=86400"},
    )


@app.get("/api/covers/album/{album_id}/thumbnail")
def cover_album_thumbnail(
    album_id: int,
    size: int = Query(default=96, ge=32, le=192),
    format: str = Query(default="png", pattern="^(png|bmp)$"),
    _: None = Depends(require_api_key),
) -> Response:
    path = _resolve_album_cover_path(album_id)
    return _thumbnail_response(path, size, format)


@app.get("/api/covers/{track_id}")
def cover_for_track(track_id: int, _: None = Depends(require_api_key)):
    path = _resolve_track_cover_path(track_id)
    return FileResponse(path, media_type="image/jpeg")


@app.get("/api/covers/{track_id}/thumbnail")
def cover_thumbnail(
    track_id: int,
    size: int = Query(default=96, ge=32, le=192),
    format: str = Query(default="png", pattern="^(png|bmp)$"),
    _: None = Depends(require_api_key),
) -> Response:
    """Compact square thumbnail; lazily fetches missing album art."""
    path = _resolve_track_cover_path(track_id)
    return _thumbnail_response(path, size, format)


@app.get("/api/stream/{track_id}")
def stream_track(
    track_id: int,
    format: Optional[str] = None,
    start_ms: int = Query(default=0, ge=0),
    _: None = Depends(require_api_key),
):
    """
    default / format=mp3 → 320k MP3 (PSP hardware decode; FLAC sources included).
    format=flac → native FLAC only when source is .flac (optional; soft-decode is fragile).
    start_ms → begin near that position (MP3 frame-aligned for PSP seek/resume).
    """
    with get_db() as conn:
        r = conn.execute(
            "SELECT path, duration FROM tracks WHERE id = ?",
            (track_id,),
        ).fetchone()
    if not r:
        raise HTTPException(status_code=404, detail="track not found")
    try:
        path = resolve_path(r["path"])
    except ValueError as exc:
        raise HTTPException(status_code=404, detail="audio file missing") from exc
    if not path.is_file():
        raise HTTPException(status_code=404, detail="audio file missing")
    fmt = (format or "").lower()
    force_mp3 = fmt in ("mp3", "mpeg", "lossy") or fmt == ""
    prefer_flac = fmt in ("flac", "native", "lossless")
    # Empty format → MP3 (force_mp3). Explicit flac opts into native.
    if prefer_flac:
        force_mp3 = False
    dur = float(r["duration"] or 0) if r["duration"] is not None else 0.0
    return stream_file(
        path,
        force_mp3=force_mp3,
        prefer_flac=prefer_flac,
        start_ms=start_ms,
        duration_sec=dur,
    )


class PlaybackStateBody(BaseModel):
    track_id: int = Field(ge=1)
    position_ms: int = Field(default=0, ge=0)
    paused: bool = True


@app.get("/api/playback/state")
def get_playback_state(_: None = Depends(require_api_key)) -> dict[str, Any]:
    with get_db() as conn:
        tid = get_meta(conn, "playback_track_id", "")
        pos = get_meta(conn, "playback_position_ms", "0")
        paused = get_meta(conn, "playback_paused", "1")
        title = ""
        artist = ""
        album = ""
        duration = 0.0
        rating = 0
        track_id = 0
        if tid.isdigit():
            row = conn.execute(
                """
                SELECT t.title, t.duration, t.rating, ar.name AS artist, al.name AS album
                FROM tracks t
                JOIN artists ar ON ar.id = t.artist_id
                JOIN albums al ON al.id = t.album_id
                WHERE t.id = ?
                """,
                (int(tid),),
            ).fetchone()
            if row:
                track_id = int(tid)
                title = row["title"] or ""
                artist = row["artist"] or ""
                album = row["album"] or ""
                duration = float(row["duration"] or 0)
                rating = int(row["rating"] or 0)
            else:
                # Stale id after library rescan — clear so PSP does not resume 404 streams.
                set_meta(conn, "playback_track_id", "")
                set_meta(conn, "playback_position_ms", "0")
                pos = "0"
    return {
        "track_id": track_id,
        "position_ms": int(pos) if str(pos).isdigit() else 0,
        "paused": paused not in ("0", "false", "False"),
        "title": title,
        "artist": artist,
        "album": album,
        "duration": duration,
        "rating": rating,
    }


@app.put("/api/playback/state")
def put_playback_state(
    body: PlaybackStateBody,
    _: None = Depends(require_api_key),
) -> dict[str, Any]:
    with get_db() as conn:
        exists = conn.execute("SELECT id FROM tracks WHERE id = ?", (body.track_id,)).fetchone()
        if not exists:
            # Soft-ok: PSP used to hang retrying PUT 404 for deleted/stale ids.
            set_meta(conn, "playback_track_id", "")
            set_meta(conn, "playback_position_ms", "0")
            return {
                "track_id": 0,
                "position_ms": 0,
                "paused": True,
                "cleared": True,
            }
        set_meta(conn, "playback_track_id", str(body.track_id))
        set_meta(conn, "playback_position_ms", str(int(body.position_ms)))
        set_meta(conn, "playback_paused", "1" if body.paused else "0")
    return {
        "track_id": body.track_id,
        "position_ms": body.position_ms,
        "paused": body.paused,
    }


def _client_dir() -> Path:
    d = settings.data_dir / "client"
    d.mkdir(parents=True, exist_ok=True)
    return d


def _client_eboot_path() -> Path:
    return _client_dir() / "EBOOT.PBP"


def _sha256_file(path: Path) -> str:
    import hashlib

    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


@app.post("/api/client/logs")
async def client_logs_ingest(
    request: Request,
    _: None = Depends(require_api_key),
) -> dict[str, Any]:
    """Accept PSP breadcrumbs. Always 2xx so broken clients stop WiFi-retry spam."""
    from .client_logs import ingest_client_logs

    raw = await request.body()
    if not raw:
        return {"ok": False, "error": "empty"}
    if len(raw) > 64 * 1024:
        return {"ok": False, "error": "too large"}
    try:
        payload = json.loads(raw.decode("utf-8", errors="replace"))
    except (UnicodeError, json.JSONDecodeError):
        # 1.2.22 sometimes POSTed truncated/invalid JSON every frame → FLAC stutter.
        return {"ok": False, "error": "bad json"}
    if not isinstance(payload, dict):
        return {"ok": False, "error": "not object"}
    device = str(payload.get("device") or "psp")[:24]
    app_version = str(payload.get("app_version") or "")[:32]
    try:
        app_code = int(payload.get("app_code") or 0)
    except (TypeError, ValueError):
        app_code = 0
    session = str(payload.get("session") or "")[:48]
    events = payload.get("events")
    if not isinstance(events, list):
        events = []
    clean_events: list[dict[str, Any]] = []
    for ev in events[:80]:
        if isinstance(ev, dict):
            clean_events.append(ev)
    return ingest_client_logs(
        {
            "device": device,
            "app_version": app_version,
            "app_code": max(0, min(app_code, 999999)),
            "session": session,
            "events": clean_events,
        }
    )


@app.get("/api/client/logs")
def client_logs_list(
    limit: int = Query(30, ge=1, le=100),
    _: None = Depends(require_api_key),
) -> list[dict[str, Any]]:
    from .client_logs import list_client_logs

    return list_client_logs(limit=limit)


@app.get("/api/client/logs/{name}")
def client_logs_get(name: str, _: None = Depends(require_api_key)) -> dict[str, Any]:
    from .client_logs import read_client_log

    try:
        return read_client_log(name)
    except FileNotFoundError as exc:
        raise HTTPException(status_code=404, detail="log not found") from exc


@app.get("/api/client/update")
def client_update_manifest(_: None = Depends(require_api_key)) -> dict[str, Any]:
    eboot = _client_eboot_path()
    meta = _client_dir() / "update.json"
    version = "1.0.0"
    version_code = 100
    notes = ""
    if meta.is_file():
        import json

        try:
            data = json.loads(meta.read_text(encoding="utf-8"))
            version = str(data.get("version", version))
            version_code = int(data.get("version_code", version_code))
            notes = str(data.get("notes", ""))
        except Exception:
            pass
    if not eboot.is_file():
        return {
            "version": version,
            "version_code": version_code,
            "size": 0,
            "sha256": "",
            "notes": notes or "No EBOOT uploaded yet",
            "available": False,
        }
    return {
        "version": version,
        "version_code": version_code,
        "size": eboot.stat().st_size,
        "sha256": _sha256_file(eboot),
        "notes": notes,
        "available": True,
    }


@app.get("/api/client/EBOOT.PBP")
def client_eboot_download(_: None = Depends(require_api_key)):
    eboot = _client_eboot_path()
    if not eboot.is_file():
        raise HTTPException(status_code=404, detail="EBOOT not published")
    return FileResponse(
        eboot,
        media_type="application/octet-stream",
        filename="EBOOT.PBP",
    )


@app.get("/api/playlists")
def list_playlists(_: None = Depends(require_api_key)) -> list[dict[str, Any]]:
    with get_db() as conn:
        rows = conn.execute(
            """
            SELECT p.id, p.name, p.created_at, COUNT(pt.track_id) AS tracks
            FROM playlists p
            LEFT JOIN playlist_tracks pt ON pt.playlist_id = p.id
            GROUP BY p.id
            ORDER BY p.name COLLATE NOCASE
            """
        ).fetchall()
    return [dict(r) for r in rows]


@app.post("/api/playlists")
def create_playlist(body: PlaylistBody, _: None = Depends(require_api_key)) -> dict[str, Any]:
    with get_db() as conn:
        cur = conn.execute("INSERT INTO playlists (name) VALUES (?)", (body.name.strip(),))
        pid = int(cur.lastrowid)
    return {"id": pid, "name": body.name.strip(), "tracks": 0}


@app.get("/api/playlists/{playlist_id}")
def get_playlist(playlist_id: int, _: None = Depends(require_api_key)) -> dict[str, Any]:
    with get_db() as conn:
        pl = conn.execute("SELECT id, name, created_at FROM playlists WHERE id = ?", (playlist_id,)).fetchone()
        if not pl:
            raise HTTPException(status_code=404, detail="playlist not found")
        rows = conn.execute(
            """
            SELECT t.id, t.title, t.track_num, t.duration, t.rating,
                   t.album_id, t.artist_id, t.genre, t.year,
                   ar.name AS artist, al.name AS album, pt.pos
            FROM playlist_tracks pt
            JOIN tracks t ON t.id = pt.track_id
            JOIN artists ar ON ar.id = t.artist_id
            JOIN albums al ON al.id = t.album_id
            WHERE pt.playlist_id = ?
            ORDER BY pt.pos, t.title COLLATE NOCASE
            """,
            (playlist_id,),
        ).fetchall()
    return {
        "id": pl["id"],
        "name": pl["name"],
        "created_at": pl["created_at"],
        "tracks": [track_row(r) for r in rows],
    }


@app.put("/api/playlists/{playlist_id}")
def rename_playlist(
    playlist_id: int, body: PlaylistBody, _: None = Depends(require_api_key)
) -> dict[str, Any]:
    with get_db() as conn:
        cur = conn.execute(
            "UPDATE playlists SET name = ? WHERE id = ?",
            (body.name.strip(), playlist_id),
        )
        if cur.rowcount == 0:
            raise HTTPException(status_code=404, detail="playlist not found")
    return {"id": playlist_id, "name": body.name.strip()}


@app.delete("/api/playlists/{playlist_id}")
def delete_playlist(playlist_id: int, _: None = Depends(require_api_key)) -> dict[str, Any]:
    with get_db() as conn:
        cur = conn.execute("DELETE FROM playlists WHERE id = ?", (playlist_id,))
        if cur.rowcount == 0:
            raise HTTPException(status_code=404, detail="playlist not found")
    return {"ok": True, "id": playlist_id}


@app.post("/api/playlists/{playlist_id}/tracks")
def add_playlist_track(
    playlist_id: int, body: PlaylistTrackBody, _: None = Depends(require_api_key)
) -> dict[str, Any]:
    with get_db() as conn:
        pl = conn.execute("SELECT id FROM playlists WHERE id = ?", (playlist_id,)).fetchone()
        if not pl:
            raise HTTPException(status_code=404, detail="playlist not found")
        tr = conn.execute("SELECT id FROM tracks WHERE id = ?", (body.track_id,)).fetchone()
        if not tr:
            raise HTTPException(status_code=404, detail="track not found")
        pos = conn.execute(
            "SELECT COALESCE(MAX(pos), 0) + 1 AS p FROM playlist_tracks WHERE playlist_id = ?",
            (playlist_id,),
        ).fetchone()["p"]
        conn.execute(
            """
            INSERT INTO playlist_tracks (playlist_id, track_id, pos)
            VALUES (?, ?, ?)
            ON CONFLICT(playlist_id, track_id) DO NOTHING
            """,
            (playlist_id, body.track_id, pos),
        )
    return {"ok": True, "playlist_id": playlist_id, "track_id": body.track_id}


@app.delete("/api/playlists/{playlist_id}/tracks/{track_id}")
def remove_playlist_track(
    playlist_id: int, track_id: int, _: None = Depends(require_api_key)
) -> dict[str, Any]:
    with get_db() as conn:
        cur = conn.execute(
            "DELETE FROM playlist_tracks WHERE playlist_id = ? AND track_id = ?",
            (playlist_id, track_id),
        )
        if cur.rowcount == 0:
            raise HTTPException(status_code=404, detail="track not in playlist")
    return {"ok": True}


@app.get("/")
@app.get("/admin")
@app.get("/admin/")
def admin_ui() -> FileResponse:
    return FileResponse(ADMIN_DIR / "index.html")


app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")
