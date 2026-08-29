"""Catalog browse helpers and JSON serializers."""

from __future__ import annotations

from typing import Any, Optional

from .db import get_db
from .translit import clean_album, clean_artist, clean_title, to_latin


def _track_urls(track_id: int) -> dict[str, str]:
    return {
        "stream_url": f"/api/stream/{track_id}",
        "cover_url": f"/api/covers/{track_id}",
    }


def track_row(r: Any) -> dict[str, Any]:
    title = clean_title(r["title"])
    artist = clean_artist(r["artist"])
    album = clean_album(r["album"], artist=r["artist"])
    item = {
        "id": r["id"],
        "title": title,
        "track_num": r["track_num"],
        "duration": r["duration"],
        "rating": r["rating"] or 0,
        "album_id": r["album_id"],
        "artist_id": r["artist_id"],
        "artist": artist,
        "album": album,
        **_track_urls(r["id"]),
    }
    genre = None
    if "genre" in r.keys() and r["genre"]:
        genre = to_latin(r["genre"], fallback="")
    if genre:
        item["genre"] = genre
    else:
        item["genre"] = "Unknown"
    if "year" in r.keys() and r["year"]:
        item["year"] = r["year"]
    codec = None
    if "codec" in r.keys() and r["codec"]:
        codec = str(r["codec"]).lower()
    elif "path" in r.keys() and r["path"]:
        p = str(r["path"]).lower()
        if "." in p:
            codec = p.rsplit(".", 1)[-1]
    if codec:
        # Online play is always 320k MP3 (sceMp3). Report what the PSP will hear.
        if codec == "mp3":
            item["format"] = "mp3"
            item["lossless"] = False
        else:
            item["format"] = "mp3"
            item["lossless"] = False
            item["source_format"] = codec
    for key in ("sample_rate", "bit_depth", "channels", "bytes", "bitrate"):
        if key in r.keys() and r[key] is not None:
            item[key] = r[key]
    if "lossless" in r.keys() and r["lossless"] is not None and "lossless" not in item:
        item["lossless"] = bool(r["lossless"])
    if "play_count" in r.keys() and r["play_count"] is not None:
        item["play_count"] = int(r["play_count"] or 0)
    if "listen_ms" in r.keys() and r["listen_ms"] is not None:
        item["listen_ms"] = int(r["listen_ms"] or 0)
    return item


def album_row(r: Any) -> dict[str, Any]:
    artist = clean_artist(r["artist"])
    item = {
        "id": r["id"],
        "name": clean_album(r["name"], artist=r["artist"]),
        "artist_id": r["artist_id"],
        "artist": artist,
        "tracks": r["track_count"],
        "user_rating": int(r["user_rating"] or 0) if "user_rating" in r.keys() else 0,
    }
    if "play_count" in r.keys() and r["play_count"] is not None:
        item["play_count"] = int(r["play_count"] or 0)
    if r["cover_path"]:
        item["cover_url"] = f"/api/covers/album/{r['id']}"
    genre = None
    if "genre" in r.keys() and r["genre"]:
        genre = to_latin(r["genre"], fallback="")
    item["genre"] = genre or "Unknown"
    if "year" in r.keys() and r["year"]:
        item["year"] = r["year"]
    summary = None
    if "summary" in r.keys() and r["summary"]:
        summary = to_latin(r["summary"], fallback="")
    if summary:
        item["summary"] = summary
    else:
        item["summary"] = f"{item['genre']} album by {artist}." if artist else "Album."
    for key in ("external_score", "external_source"):
        if key in r.keys() and r[key] is not None:
            item[key] = r[key]
    return item


def artist_row(r: Any) -> dict[str, Any]:
    name = clean_artist(r["name"])
    item = {
        "id": r["id"],
        "name": name,
        "tracks": r["track_count"],
    }
    genre = None
    if "genre" in r.keys() and r["genre"]:
        genre = to_latin(r["genre"], fallback="")
    item["genre"] = genre or "Unknown"
    bio = None
    if "bio" in r.keys() and r["bio"]:
        bio = to_latin(r["bio"], fallback="")
    item["bio"] = bio or f"Artist in your library ({item.get('tracks', 0)} tracks)."
    for key in ("country", "external_score", "external_source"):
        if key in r.keys() and r[key] is not None:
            if key == "country":
                item[key] = to_latin(str(r[key]), fallback=str(r[key]))
            else:
                item[key] = r[key]
    if "album_count" in r.keys():
        item["albums"] = r["album_count"]
    return item


def list_genres(*, limit: int = 100, offset: int = 0) -> list[dict[str, Any]]:
    with get_db() as conn:
        rows = conn.execute(
            """
            SELECT COALESCE(NULLIF(TRIM(genre), ''), 'Unknown') AS genre,
                   COUNT(*) AS track_count,
                   COUNT(DISTINCT album_id) AS album_count,
                   COUNT(DISTINCT artist_id) AS artist_count
            FROM tracks
            GROUP BY genre COLLATE NOCASE
            ORDER BY track_count DESC, genre COLLATE NOCASE
            LIMIT ? OFFSET ?
            """,
            (limit, offset),
        ).fetchall()
    return [dict(r) for r in rows]


def browse_summary() -> dict[str, Any]:
    with get_db() as conn:
        genres = conn.execute(
            "SELECT COUNT(DISTINCT genre) AS c FROM tracks WHERE genre IS NOT NULL AND genre != ''"
        ).fetchone()["c"]
        rated_tracks = conn.execute(
            "SELECT COUNT(*) AS c FROM tracks WHERE rating > 0"
        ).fetchone()["c"]
        rated_albums = conn.execute(
            "SELECT COUNT(*) AS c FROM albums WHERE user_rating IS NOT NULL AND user_rating > 0"
        ).fetchone()["c"]
        with_meta = conn.execute(
            "SELECT COUNT(*) AS c FROM albums WHERE summary IS NOT NULL AND summary != ''"
        ).fetchone()["c"]
        with_bio = conn.execute(
            "SELECT COUNT(*) AS c FROM artists WHERE bio IS NOT NULL AND bio != ''"
        ).fetchone()["c"]
        top_genres = conn.execute(
            """
            SELECT genre, COUNT(*) AS tracks FROM tracks
            WHERE genre IS NOT NULL AND genre != ''
            GROUP BY genre COLLATE NOCASE
            ORDER BY tracks DESC LIMIT 8
            """
        ).fetchall()
    return {
        "genres": genres,
        "rated_tracks": rated_tracks,
        "rated_albums": rated_albums,
        "albums_with_network_info": with_meta,
        "artists_with_bio": with_bio,
        "top_genres": [{"genre": r["genre"], "tracks": r["tracks"]} for r in top_genres],
    }


def get_artist_detail(artist_id: int) -> Optional[dict[str, Any]]:
    with get_db() as conn:
        r = conn.execute(
            """
            SELECT a.id, a.name, a.genre, a.bio, a.country,
                   a.external_score, a.external_source,
                   COUNT(DISTINCT t.id) AS track_count,
                   COUNT(DISTINCT al.id) AS album_count,
                   ROUND(AVG(CASE WHEN t.rating > 0 THEN t.rating END), 2) AS user_rating
            FROM artists a
            LEFT JOIN tracks t ON t.artist_id = a.id
            LEFT JOIN albums al ON al.artist_id = a.id
            WHERE a.id = ?
            GROUP BY a.id
            """,
            (artist_id,),
        ).fetchone()
        if not r:
            return None
        albums = conn.execute(
            """
            SELECT al.id, al.name, al.cover_path, al.genre, al.year,
                   al.user_rating, al.external_score, al.external_source,
                   ar.name AS artist, al.artist_id,
                   COUNT(t.id) AS track_count
            FROM albums al
            JOIN artists ar ON ar.id = al.artist_id
            LEFT JOIN tracks t ON t.album_id = al.id
            WHERE al.artist_id = ?
            GROUP BY al.id
            ORDER BY al.year IS NULL, al.year DESC, al.name COLLATE NOCASE
            """,
            (artist_id,),
        ).fetchall()
    out = artist_row(r)
    out["user_rating"] = r["user_rating"]
    out["albums_list"] = [album_row(a) for a in albums]
    return out


def get_album_detail(album_id: int) -> Optional[dict[str, Any]]:
    with get_db() as conn:
        r = conn.execute(
            """
            SELECT al.id, al.name, al.cover_path, al.genre, al.year, al.summary,
                   al.user_rating, al.external_score, al.external_source,
                   al.artist_id, ar.name AS artist,
                   COUNT(t.id) AS track_count
            FROM albums al
            JOIN artists ar ON ar.id = al.artist_id
            LEFT JOIN tracks t ON t.album_id = al.id
            WHERE al.id = ?
            GROUP BY al.id
            """,
            (album_id,),
        ).fetchone()
        if not r:
            return None
        tracks = conn.execute(
            """
            SELECT t.id, t.title, t.track_num, t.duration, t.rating,
                   t.album_id, t.artist_id, t.genre, t.year,
                   ar.name AS artist, al.name AS album
            FROM tracks t
            JOIN artists ar ON ar.id = t.artist_id
            JOIN albums al ON al.id = t.album_id
            WHERE t.album_id = ?
            ORDER BY t.track_num IS NULL, t.track_num, t.title COLLATE NOCASE
            """,
            (album_id,),
        ).fetchall()
        artist = conn.execute(
            "SELECT genre, bio, country, external_score FROM artists WHERE id = ?",
            (r["artist_id"],),
        ).fetchone()
    out = album_row(r)
    out["tracks_list"] = [track_row(t) for t in tracks]
    if artist:
        out["artist_info"] = {
            "genre": artist["genre"],
            "bio": artist["bio"],
            "country": artist["country"],
            "external_score": artist["external_score"],
        }
    return out
