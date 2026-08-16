"""Fetch album/artist metadata from iTunes, MusicBrainz, and optional Last.fm."""

from __future__ import annotations

import logging
import re
import sqlite3
import time
from typing import Any, Optional
from urllib.parse import quote

import httpx

from .config import settings
from .covers import _is_junk_album, _is_unknown

log = logging.getLogger("metadata")

_USER_AGENT = "PSPMusicServer/0.1 (local library; contact=local)"
_LAST_MB_CALL = 0.0


def _parse_year(raw: Any) -> Optional[int]:
    if raw is None:
        return None
    s = str(raw).strip()
    m = re.match(r"(\d{4})", s)
    if not m:
        return None
    year = int(m.group(1))
    return year if 1900 <= year <= 2100 else None


def _clip(text: Optional[str], limit: int = 1200) -> Optional[str]:
    if not text:
        return None
    text = re.sub(r"\s+", " ", text.strip())
    if len(text) <= limit:
        return text
    return text[: limit - 1].rstrip() + "…"


def _musicbrainz_get(path: str) -> Optional[dict[str, Any]]:
    global _LAST_MB_CALL
    elapsed = time.time() - _LAST_MB_CALL
    if elapsed < 1.05:
        time.sleep(1.05 - elapsed)
    _LAST_MB_CALL = time.time()
    url = f"https://musicbrainz.org/ws/2/{path}"
    try:
        with httpx.Client(timeout=20.0, follow_redirects=True) as client:
            resp = client.get(url, headers={"User-Agent": _USER_AGENT, "Accept": "application/json"})
            if resp.status_code == 404:
                return None
            resp.raise_for_status()
            return resp.json()
    except Exception as exc:
        log.info("MusicBrainz fail %s: %s", path, exc)
        return None


def search_itunes_album(artist: str, album: str) -> Optional[dict[str, Any]]:
    if _is_unknown(artist) and _is_unknown(album):
        return None
    terms: list[str] = []
    if not _is_unknown(artist) and not _is_unknown(album):
        terms.append(f"{artist} {album}")
    if not _is_unknown(album):
        terms.append(album)
    album_l = (album or "").lower().strip()
    artist_l = (artist or "").lower().strip()

    for term in terms:
        url = f"https://itunes.apple.com/search?term={quote(term)}&entity=album&limit=8"
        try:
            with httpx.Client(timeout=20.0, follow_redirects=True) as client:
                resp = client.get(url)
                resp.raise_for_status()
                results = resp.json().get("results") or []
                item = None
                for candidate in results:
                    cname = (candidate.get("collectionName") or "").lower()
                    aname = (candidate.get("artistName") or "").lower()
                    if album_l and album_l in cname and (not artist_l or artist_l in aname):
                        item = candidate
                        break
                if not item:
                    for candidate in results:
                        cname = (candidate.get("collectionName") or "").lower()
                        if album_l and album_l in cname:
                            item = candidate
                            break
                if not item and results:
                    item = results[0]
                if not item:
                    continue
                genre = (item.get("primaryGenreName") or "").strip() or None
                year = _parse_year(item.get("releaseDate"))
                copyright_text = (item.get("copyright") or "").strip() or None
                track_count = item.get("trackCount")
                collection = (item.get("collectionName") or album).strip()
                artist_name = (item.get("artistName") or artist).strip()
                summary_parts = []
                if genre:
                    summary_parts.append(f"Жанр: {genre}.")
                if year:
                    summary_parts.append(f"Год: {year}.")
                if track_count:
                    summary_parts.append(f"Треков в релизе: {track_count}.")
                if copyright_text:
                    summary_parts.append(copyright_text)
                return {
                    "genre": genre,
                    "year": year,
                    "summary": _clip(" ".join(summary_parts)),
                    "external_score": None,
                    "external_source": "itunes",
                    "collection": collection,
                    "artist": artist_name,
                }
        except Exception as exc:
            log.info("iTunes meta fail term=%r: %s", term, exc)
        time.sleep(0.12)
    return None


def search_musicbrainz_artist(name: str) -> Optional[dict[str, Any]]:
    if _is_unknown(name):
        return None
    q = quote(f'artist:"{name}"')
    data = _musicbrainz_get(f"artist/?query={q}&limit=3&fmt=json")
    if not data:
        return None
    artists = data.get("artists") or []
    if not artists:
        return None
    pick = artists[0]
    name_l = name.lower().strip()
    for candidate in artists:
        if (candidate.get("name") or "").lower().strip() == name_l:
            pick = candidate
            break
    mbid = pick.get("id")
    if not mbid:
        return None
    detail = _musicbrainz_get(f"artist/{mbid}?inc=tags+aliases+annotation&fmt=json")
    if not detail:
        detail = pick
    tags = detail.get("tags") or []
    top_tags = sorted(tags, key=lambda t: int(t.get("count") or 0), reverse=True)
    genres = [t.get("name") for t in top_tags[:5] if t.get("name")]
    genre = genres[0] if genres else None
    country = None
    area = detail.get("area") or detail.get("begin-area") or {}
    if isinstance(area, dict):
        country = area.get("name")
    life = detail.get("life-span") or {}
    begin = _parse_year(life.get("begin"))
    end = _parse_year(life.get("end"))
    bio_parts = []
    if detail.get("disambiguation"):
        bio_parts.append(detail["disambiguation"])
    if country:
        bio_parts.append(f"Страна: {country}.")
    if begin:
        bio_parts.append(f"Активен с {begin}" + (f" по {end}" if end else "") + ".")
    if genres:
        bio_parts.append("Теги: " + ", ".join(genres) + ".")
    annotation = (detail.get("annotation") or "").strip()
    if annotation:
        bio_parts.append(annotation)
    score = None
    if top_tags:
        score = min(100.0, float(sum(int(t.get("count") or 0) for t in top_tags[:5])) * 4.0)
    return {
        "genre": genre,
        "bio": _clip(" ".join(bio_parts)),
        "country": country,
        "external_score": score,
        "external_source": "musicbrainz",
    }


def search_lastfm_album(artist: str, album: str, api_key: str) -> Optional[dict[str, Any]]:
    if not api_key or (_is_unknown(artist) and _is_unknown(album)):
        return None
    params = {
        "method": "album.getinfo",
        "api_key": api_key,
        "artist": artist,
        "album": album,
        "format": "json",
    }
    try:
        with httpx.Client(timeout=20.0, follow_redirects=True) as client:
            resp = client.get("https://ws.audioscrobbler.com/2.0/", params=params)
            resp.raise_for_status()
            data = resp.json()
            alb = data.get("album") or {}
            if not alb:
                return None
            listeners = int(alb.get("listeners") or 0)
            playcount = int(alb.get("playcount") or 0)
            tags = [t.get("name") for t in (alb.get("tags") or {}).get("tag") or [] if t.get("name")]
            wiki = alb.get("wiki") or {}
            summary = _clip((wiki.get("summary") or wiki.get("content") or "").strip())
            genre = tags[0] if tags else None
            score = None
            if listeners > 0:
                import math

                score = min(100.0, round(math.log10(max(listeners, 1)) * 22 + math.log10(max(playcount, 1)) * 8, 1))
            return {
                "genre": genre,
                "summary": summary,
                "external_score": score,
                "external_source": "lastfm",
                "listeners": listeners,
                "playcount": playcount,
            }
    except Exception as exc:
        log.info("Last.fm album fail %s - %s: %s", artist, album, exc)
        return None


def aggregate_album_stats(conn: sqlite3.Connection, album_id: Optional[int] = None) -> None:
    """Fill album genre/year/user_rating from track tags and ratings."""
    album_filter = "AND albums.id = ?" if album_id is not None else ""
    album_params: tuple[Any, ...] = (album_id,) if album_id is not None else ()
    conn.execute(
        f"""
        UPDATE albums SET genre = (
            SELECT t.genre FROM tracks t
            WHERE t.album_id = albums.id AND t.genre IS NOT NULL AND t.genre != ''
            GROUP BY t.genre ORDER BY COUNT(*) DESC LIMIT 1
        )
        WHERE (genre IS NULL OR genre = '') {album_filter}
        """,
        album_params,
    )
    conn.execute(
        f"""
        UPDATE albums SET year = (
            SELECT MIN(t.year) FROM tracks t
            WHERE t.album_id = albums.id AND t.year IS NOT NULL
        )
        WHERE year IS NULL {album_filter}
        """,
        album_params,
    )
    conn.execute(
        f"""
        UPDATE albums SET user_rating = (
            SELECT ROUND(AVG(CASE WHEN t.rating > 0 THEN t.rating END), 2)
            FROM tracks t WHERE t.album_id = albums.id
        )
        WHERE 1=1 {album_filter}
        """,
        album_params,
    )
    artist_sql = """
        UPDATE artists SET genre = (
            SELECT a.genre FROM albums a
            WHERE a.artist_id = artists.id AND a.genre IS NOT NULL AND a.genre != ''
            GROUP BY a.genre ORDER BY COUNT(*) DESC LIMIT 1
        )
        WHERE genre IS NULL OR genre = ''
    """
    if album_id is not None:
        conn.execute(
            artist_sql + " AND id = (SELECT artist_id FROM albums WHERE id = ?)",
            (album_id,),
        )
    else:
        conn.execute(artist_sql)


def search_deezer_album(artist: str, album: str) -> Optional[dict[str, Any]]:
    if _is_unknown(artist) and _is_unknown(album):
        return None
    queries: list[str] = []
    if not _is_unknown(artist) and not _is_unknown(album):
        queries.append(f"{artist} {album}")
    if not _is_unknown(album):
        queries.append(album)
    if not _is_unknown(artist):
        queries.append(artist)
    album_l = (album or "").lower().strip()
    artist_l = (artist or "").lower().strip()
    try:
        with httpx.Client(timeout=20.0, follow_redirects=True) as client:
            for q in queries:
                url = f"https://api.deezer.com/search/album?q={quote(q)}&limit=8"
                resp = client.get(url)
                resp.raise_for_status()
                data = resp.json().get("data") or []
                pick = None
                for item in data:
                    title = (item.get("title") or "").lower()
                    aname = ((item.get("artist") or {}).get("name") or "").lower()
                    if album_l and album_l in title and (not artist_l or artist_l in aname or _is_unknown(artist)):
                        pick = item
                        break
                if not pick and artist_l and data:
                    for item in data:
                        aname = ((item.get("artist") or {}).get("name") or "").lower()
                        if artist_l in aname:
                            pick = item
                            break
                if not pick and data and album_l:
                    pick = data[0]
                if not pick:
                    continue
                genre = None
                # Deezer album detail for genres
                alb_id = pick.get("id")
                if alb_id:
                    detail = client.get(f"https://api.deezer.com/album/{alb_id}")
                    if detail.status_code == 200:
                        body = detail.json()
                        genres = (body.get("genres") or {}).get("data") or []
                        if genres:
                            genre = (genres[0].get("name") or "").strip() or None
                        year = _parse_year(body.get("release_date"))
                        fans = int(body.get("fans") or 0)
                        title = (body.get("title") or pick.get("title") or album or "").strip()
                        artist_name = ((body.get("artist") or {}).get("name") or artist or "").strip()
                        parts = []
                        if genre:
                            parts.append(f"Жанр: {genre}.")
                        if year:
                            parts.append(f"Год: {year}.")
                        if body.get("nb_tracks"):
                            parts.append(f"Треков в релизе: {body.get('nb_tracks')}.")
                        score = None
                        if fans > 0:
                            import math

                            score = min(100.0, round(math.log10(max(fans, 1)) * 25, 1))
                        return {
                            "genre": genre,
                            "year": year,
                            "summary": _clip(" ".join(parts)) if parts else None,
                            "external_score": score,
                            "external_source": "deezer",
                            "collection": title,
                            "artist": artist_name,
                        }
                year = None
                title = (pick.get("title") or album or "").strip()
                artist_name = ((pick.get("artist") or {}).get("name") or artist or "").strip()
                return {
                    "genre": None,
                    "year": year,
                    "summary": _clip(f"{artist_name} — {title}"),
                    "external_score": None,
                    "external_source": "deezer",
                    "collection": title,
                    "artist": artist_name,
                }
    except Exception as exc:
        log.info("Deezer album fail %s - %s: %s", artist, album, exc)
    return None


def enrich_album(
    conn: sqlite3.Connection,
    *,
    album_id: int,
    artist: str,
    album: str,
    force: bool = False,
) -> bool:
    row = conn.execute(
        "SELECT genre, year, summary, external_score, meta_fetched_at FROM albums WHERE id = ?",
        (album_id,),
    ).fetchone()
    if not row:
        return False
    if row["meta_fetched_at"] and not force and row["summary"]:
        return False
    if _is_junk_album(artist, album):
        return False

    from .translit import folder_labels_from_relpath

    pairs: list[tuple[str, str]] = [(artist or "", album or "")]
    for t in conn.execute(
        "SELECT path FROM tracks WHERE album_id = ? ORDER BY track_num, id LIMIT 3",
        (album_id,),
    ).fetchall():
        raw_a, raw_b = folder_labels_from_relpath(t["path"])
        if raw_a or raw_b:
            pairs.append((raw_a or artist or "", raw_b or album or ""))

    merged: dict[str, Any] = {}
    for a, b in pairs:
        itunes = search_itunes_album(a, b)
        if itunes:
            merged.update({k: v for k, v in itunes.items() if v is not None})
            break
    if not merged.get("summary"):
        for a, b in pairs:
            deezer = search_deezer_album(a, b)
            if deezer:
                for k, v in deezer.items():
                    if v is not None and not merged.get(k):
                        merged[k] = v
                if merged.get("summary"):
                    break

    lastfm_key = getattr(settings, "lastfm_api_key", "") or ""
    if lastfm_key:
        for a, b in pairs:
            lastfm = search_lastfm_album(a, b, lastfm_key)
            if lastfm:
                if lastfm.get("external_score") is not None:
                    merged["external_score"] = lastfm["external_score"]
                    merged["external_source"] = lastfm["external_source"]
                if not merged.get("genre") and lastfm.get("genre"):
                    merged["genre"] = lastfm["genre"]
                if not merged.get("summary") and lastfm.get("summary"):
                    merged["summary"] = lastfm["summary"]
                if merged.get("summary"):
                    break

    if not merged:
        conn.execute(
            "UPDATE albums SET meta_fetched_at = ? WHERE id = ?",
            (str(time.time()), album_id),
        )
        return False

    conn.execute(
        """
        UPDATE albums SET
            genre = COALESCE(?, genre),
            year = COALESCE(?, year),
            summary = COALESCE(?, summary),
            external_score = COALESCE(?, external_score),
            external_source = COALESCE(?, external_source),
            meta_fetched_at = ?
        WHERE id = ?
        """,
        (
            merged.get("genre"),
            merged.get("year"),
            merged.get("summary"),
            merged.get("external_score"),
            merged.get("external_source"),
            str(time.time()),
            album_id,
        ),
    )
    return True


def enrich_artist(conn: sqlite3.Connection, *, artist_id: int, name: str, force: bool = False) -> bool:
    row = conn.execute(
        "SELECT bio, meta_fetched_at FROM artists WHERE id = ?", (artist_id,)
    ).fetchone()
    if not row:
        return False
    if row["meta_fetched_at"] and not force and row["bio"]:
        return False
    if _is_unknown(name):
        return False

    merged: dict[str, Any] = {}
    mb = search_musicbrainz_artist(name)
    if mb:
        merged.update({k: v for k, v in mb.items() if v is not None})

    if not merged:
        conn.execute(
            "UPDATE artists SET meta_fetched_at = ? WHERE id = ?",
            (str(time.time()), artist_id),
        )
        return False

    conn.execute(
        """
        UPDATE artists SET
            genre = COALESCE(?, genre),
            bio = COALESCE(?, bio),
            country = COALESCE(?, country),
            external_score = COALESCE(?, external_score),
            external_source = COALESCE(?, external_source),
            meta_fetched_at = ?
        WHERE id = ?
        """,
        (
            merged.get("genre"),
            merged.get("bio"),
            merged.get("country"),
            merged.get("external_score"),
            merged.get("external_source"),
            str(time.time()),
            artist_id,
        ),
    )
    return True


def backfill_metadata(*, limit: int = 0, pause_s: float = 0.25) -> dict[str, int]:
    from .db import get_db

    albums_done = 0
    artists_done = 0
    albums_filled = 0
    artists_filled = 0
    with get_db() as conn:
        aggregate_album_stats(conn)
        album_rows = conn.execute(
            """
            SELECT a.id, a.name AS album, ar.id AS artist_id, ar.name AS artist
            FROM albums a
            JOIN artists ar ON ar.id = a.artist_id
            WHERE a.meta_fetched_at IS NULL OR a.summary IS NULL OR a.summary = ''
            ORDER BY a.id
            """
        ).fetchall()
        for row in album_rows:
            if limit and albums_done >= limit:
                break
            albums_done += 1
            if enrich_album(
                conn,
                album_id=int(row["id"]),
                artist=row["artist"] or "",
                album=row["album"] or "",
            ):
                albums_filled += 1
            if pause_s > 0:
                time.sleep(pause_s)

        artist_rows = conn.execute(
            """
            SELECT id, name FROM artists
            WHERE meta_fetched_at IS NULL OR bio IS NULL OR bio = ''
            ORDER BY id
            """
        ).fetchall()
        for row in artist_rows:
            if limit and artists_done >= limit:
                break
            artists_done += 1
            if enrich_artist(conn, artist_id=int(row["id"]), name=row["name"] or ""):
                artists_filled += 1
            if pause_s > 0:
                time.sleep(pause_s)
    return {
        "albums_attempted": albums_done,
        "albums_filled": albums_filled,
        "artists_attempted": artists_done,
        "artists_filled": artists_filled,
    }
