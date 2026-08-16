"""Extract embedded covers, sidecars, or fetch from iTunes Search API."""

from __future__ import annotations

import hashlib
import io
import logging
import re
import sqlite3
import time
from pathlib import Path
from typing import Optional
from urllib.parse import quote

import httpx
from mutagen import File as MutagenFile
from mutagen.flac import FLAC, Picture
from mutagen.id3 import ID3
from mutagen.mp4 import MP4
from PIL import Image, UnidentifiedImageError

from .config import settings
from .db import resolve_path
from .translit import folder_labels_from_relpath

log = logging.getLogger("covers")

_SIDECAR_NAMES = (
    "cover.jpg",
    "cover.jpeg",
    "cover.png",
    "folder.jpg",
    "folder.jpeg",
    "folder.png",
    "AlbumArt.jpg",
    "AlbumArtSmall.jpg",
    "front.jpg",
    "Front.jpg",
)

_UNKNOWN = {"", "unknown", "unknown artist", "unknown album", "various artists", "media.localized"}


def _is_unknown(value: str) -> bool:
    return (value or "").strip().lower() in _UNKNOWN


def _is_junk_album(artist: str, album: str) -> bool:
    a = (album or "").strip().lower()
    return _is_unknown(a) or a.endswith(".localized") or a in {"itunes music", "music"}


def _safe_name(*parts: str) -> str:
    raw = "_".join(parts)
    raw = re.sub(r"[^\w\-]+", "_", raw, flags=re.UNICODE)
    return raw[:80] or "cover"


def _cover_file(album_id: int, artist: str, album: str) -> Path:
    digest = hashlib.sha1(f"{artist}|{album}".encode("utf-8")).hexdigest()[:10]
    return settings.covers_dir / f"{album_id}_{_safe_name(artist, album)}_{digest}.jpg"


def normalize_cover_bytes(data: bytes) -> Optional[bytes]:
    """Validate image bytes and re-encode as JPEG RGB."""
    if not data or len(data) < 32:
        return None
    try:
        with Image.open(io.BytesIO(data)) as im:
            im = im.convert("RGB")
            # Cap absurd sizes to keep disk/PSP sane
            im.thumbnail((1200, 1200), Image.Resampling.LANCZOS)
            out = io.BytesIO()
            im.save(out, format="JPEG", quality=90, optimize=True)
            return out.getvalue()
    except (UnidentifiedImageError, OSError, ValueError) as exc:
        log.debug("cover normalize failed: %s", exc)
        return None


def extract_embedded_cover(audio_path: Path) -> Optional[bytes]:
    try:
        audio = MutagenFile(audio_path)
    except Exception:
        return None
    if audio is None:
        return None

    # MP3 / ID3
    try:
        id3 = ID3(audio_path)
        for key in id3.keys():
            if key.startswith("APIC"):
                raw = normalize_cover_bytes(bytes(id3[key].data))
                if raw:
                    return raw
    except Exception:
        pass

    # FLAC
    if isinstance(audio, FLAC) and audio.pictures:
        raw = normalize_cover_bytes(bytes(audio.pictures[0].data))
        if raw:
            return raw

    # MP4 / M4A
    if isinstance(audio, MP4) and audio.tags:
        covr = audio.tags.get("covr")
        if covr:
            raw = normalize_cover_bytes(bytes(covr[0]))
            if raw:
                return raw

    # OGG / Opus / generic pictures
    pictures = getattr(audio, "pictures", None)
    if pictures:
        pic = pictures[0]
        if isinstance(pic, Picture):
            raw = normalize_cover_bytes(bytes(pic.data))
            if raw:
                return raw
        data = getattr(pic, "data", None)
        if data:
            raw = normalize_cover_bytes(bytes(data))
            if raw:
                return raw

    return None


def find_sidecar_cover(audio_path: Path) -> Optional[bytes]:
    """Look for cover.jpg / folder.jpg next to the track or in parent folder."""
    dirs = [audio_path.parent]
    if audio_path.parent.parent != audio_path.parent:
        dirs.append(audio_path.parent.parent)
    for folder in dirs:
        for name in _SIDECAR_NAMES:
            candidate = folder / name
            if candidate.is_file():
                try:
                    raw = normalize_cover_bytes(candidate.read_bytes())
                except OSError:
                    continue
                if raw:
                    return raw
        # Any reasonably named image in the album folder
        try:
            for p in folder.iterdir():
                if not p.is_file():
                    continue
                low = p.name.lower()
                if low.endswith((".jpg", ".jpeg", ".png", ".webp")) and any(
                    k in low for k in ("cover", "folder", "front", "album", "art")
                ):
                    raw = normalize_cover_bytes(p.read_bytes())
                    if raw:
                        return raw
        except OSError:
            continue
    return None


def fetch_deezer_cover(artist: str, album: str) -> Optional[bytes]:
    """Deezer public search — often has covers when iTunes misses Cyrillic albums."""
    queries: list[str] = []
    if artist and not _is_unknown(artist) and album and not _is_unknown(album):
        queries.append(f"{artist} {album}")
    if album and not _is_unknown(album):
        queries.append(album)
    if artist and not _is_unknown(artist):
        queries.append(artist)
    # de-dupe keep order
    seen: set[str] = set()
    uniq: list[str] = []
    for q in queries:
        q = q.strip()
        key = q.lower()
        if q and key not in seen:
            seen.add(key)
            uniq.append(q)

    album_l = (album or "").lower().strip()
    artist_l = (artist or "").lower().strip()
    try:
        with httpx.Client(timeout=20.0, follow_redirects=True) as client:
            for q in uniq:
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
                if not pick and artist_l and not album_l:
                    for item in data:
                        aname = ((item.get("artist") or {}).get("name") or "").lower()
                        if artist_l in aname:
                            pick = item
                            break
                if not pick and data and (album_l or artist_l):
                    # Prefer same artist when album name was approximate
                    for item in data:
                        aname = ((item.get("artist") or {}).get("name") or "").lower()
                        if artist_l and artist_l in aname:
                            pick = item
                            break
                    if not pick:
                        pick = data[0]
                if not pick:
                    continue
                art = pick.get("cover_xl") or pick.get("cover_big") or pick.get("cover_medium")
                if not art:
                    continue
                img = client.get(art)
                img.raise_for_status()
                raw = normalize_cover_bytes(img.content)
                if raw:
                    return raw
    except Exception as exc:
        log.info("Deezer cover fail artist=%r album=%r: %s", artist, album, exc)
    return None


def fetch_caa_cover(artist: str, album: str) -> Optional[bytes]:
    """MusicBrainz release search → Cover Art Archive front image."""
    if _is_unknown(artist) and _is_unknown(album):
        return None
    q_parts = []
    if not _is_unknown(artist):
        q_parts.append(f'artist:"{artist}"')
    if not _is_unknown(album):
        q_parts.append(f'release:"{album}"')
    if not q_parts:
        return None
    mb_url = (
        "https://musicbrainz.org/ws/2/release/"
        f"?query={quote(' AND '.join(q_parts))}&fmt=json&limit=5"
    )
    headers = {"User-Agent": "PSPMusicServer/1.1 (local; cover-fetch)", "Accept": "application/json"}
    try:
        time.sleep(1.05)  # MB rate limit
        with httpx.Client(timeout=25.0, follow_redirects=True, headers=headers) as client:
            resp = client.get(mb_url)
            if resp.status_code == 503:
                return None
            resp.raise_for_status()
            releases = resp.json().get("releases") or []
            if not releases:
                return None
            mbid = releases[0].get("id")
            if not mbid:
                return None
            caa = f"https://coverartarchive.org/release/{mbid}/front-500"
            img = client.get(caa)
            if img.status_code == 404:
                return None
            img.raise_for_status()
            return normalize_cover_bytes(img.content)
    except Exception as exc:
        log.info("CAA cover fail %s - %s: %s", artist, album, exc)
        return None


def fetch_itunes_cover(artist: str, album: str, *, retries: int = 3) -> Optional[bytes]:
    if _is_unknown(artist) and _is_unknown(album):
        return None
    terms: list[str] = []
    if not _is_unknown(artist) and not _is_unknown(album):
        terms.append(f"{artist} {album}")
    if not _is_unknown(album):
        terms.append(album)
    if not _is_unknown(artist):
        terms.append(artist)
    if not terms:
        return None

    album_l = album.lower().strip()
    artist_l = artist.lower().strip()

    for term in terms:
        url = (
            "https://itunes.apple.com/search"
            f"?term={quote(term)}&entity=album&limit=8"
        )
        for attempt in range(retries):
            try:
                with httpx.Client(timeout=20.0, follow_redirects=True) as client:
                    resp = client.get(url)
                    if resp.status_code in (403, 429, 503):
                        time.sleep(0.6 * (attempt + 1))
                        continue
                    resp.raise_for_status()
                    results = resp.json().get("results") or []
                    art_url = None
                    # Prefer exact-ish album+artist match
                    for item in results:
                        cname = (item.get("collectionName") or "").lower()
                        aname = (item.get("artistName") or "").lower()
                        if album_l and album_l in cname and (not artist_l or artist_l in aname):
                            art_url = item.get("artworkUrl100")
                            break
                    if not art_url:
                        for item in results:
                            cname = (item.get("collectionName") or "").lower()
                            if album_l and album_l in cname:
                                art_url = item.get("artworkUrl100")
                                break
                    if not art_url and results:
                        art_url = results[0].get("artworkUrl100")
                    if not art_url:
                        break
                    art_url = art_url.replace("100x100bb", "600x600bb")
                    img = client.get(art_url)
                    if img.status_code in (403, 429, 503):
                        time.sleep(0.6 * (attempt + 1))
                        continue
                    img.raise_for_status()
                    raw = normalize_cover_bytes(img.content)
                    if raw:
                        return raw
                    break
            except Exception as exc:
                log.info("iTunes cover fail term=%r attempt=%d: %s", term, attempt + 1, exc)
                time.sleep(0.4 * (attempt + 1))
        time.sleep(0.15)  # light pacing between query variants
    return None


def ensure_album_cover(
    conn: sqlite3.Connection,
    *,
    album_id: int,
    artist: str,
    album: str,
    force: bool = False,
) -> Optional[str]:
    settings.covers_dir.mkdir(parents=True, exist_ok=True)
    row = conn.execute(
        "SELECT cover_path FROM albums WHERE id = ?", (album_id,)
    ).fetchone()
    if row and row["cover_path"] and not force:
        existing = cover_abs_path(row["cover_path"])
        if existing:
            return row["cover_path"]

    tracks = conn.execute(
        "SELECT path, title FROM tracks WHERE album_id = ? ORDER BY track_num, id",
        (album_id,),
    ).fetchall()

    data: Optional[bytes] = None
    # 1) Embedded art (all tracks)
    for t in tracks:
        path = resolve_path(t["path"])
        data = extract_embedded_cover(path)
        if data:
            break
    # 2) Sidecar images in album folder
    if not data:
        for t in tracks:
            path = resolve_path(t["path"])
            data = find_sidecar_cover(path)
            if data:
                break

    # Build extra query pairs: Latin catalog names + original Cyrillic folders from disk
    query_pairs: list[tuple[str, str]] = [(artist or "", album or "")]
    for t in tracks[:3]:
        raw_art, raw_alb = folder_labels_from_relpath(t["path"])
        if raw_art or raw_alb:
            query_pairs.append((raw_art or artist or "", raw_alb or album or ""))
        # Unknown Album → search artist + track title
        if raw_art and not raw_alb and t["title"]:
            query_pairs.append((raw_art, str(t["title"])))
        stem = Path(str(t["path"])).stem
        if raw_art and stem:
            query_pairs.append((raw_art, stem))

    # de-dupe
    seen_q: set[tuple[str, str]] = set()
    pairs: list[tuple[str, str]] = []
    for a, b in query_pairs:
        key = (a.strip().lower(), b.strip().lower())
        if key in seen_q:
            continue
        seen_q.add(key)
        pairs.append((a.strip(), b.strip()))

    if not data:
        for a, b in pairs:
            if _is_junk_album(a, b) and _is_unknown(a):
                continue
            data = fetch_itunes_cover(a, b)
            if data:
                break
            data = fetch_deezer_cover(a, b)
            if data:
                break
            data = fetch_caa_cover(a, b)
            if data:
                break
        # last resort: artist-only Deezer
        if not data:
            for a, _b in pairs:
                if a and not _is_unknown(a):
                    data = fetch_deezer_cover(a, "")
                    if data:
                        break

    if not data:
        log.debug("no cover for album_id=%s %s - %s", album_id, artist, album)
        return None

    out = _cover_file(album_id, artist, album)
    out.write_bytes(data)
    rel = out.name
    conn.execute("UPDATE albums SET cover_path = ? WHERE id = ?", (rel, album_id))
    conn.execute(
        "UPDATE tracks SET cover_path = ? WHERE album_id = ? AND (cover_path IS NULL OR cover_path = '')",
        (rel, album_id),
    )
    return rel


def ensure_cover_for_track(conn: sqlite3.Connection, track_id: int) -> Optional[str]:
    """Resolve cover for a track, fetching on demand if missing."""
    row = conn.execute(
        """
        SELECT t.id, t.cover_path AS track_cover, a.id AS album_id,
               a.cover_path AS album_cover, a.name AS album, ar.name AS artist
        FROM tracks t
        JOIN albums a ON a.id = t.album_id
        JOIN artists ar ON ar.id = a.artist_id
        WHERE t.id = ?
        """,
        (track_id,),
    ).fetchone()
    if not row:
        return None
    for rel in (row["track_cover"], row["album_cover"]):
        if cover_abs_path(rel):
            return rel
    return ensure_album_cover(
        conn,
        album_id=int(row["album_id"]),
        artist=row["artist"] or "",
        album=row["album"] or "",
    )


def backfill_missing_covers(*, limit: int = 0, pause_s: float = 0.2) -> dict[str, int]:
    """Fetch covers for albums that still lack one. Safe to run while server is up."""
    from .db import get_db

    attempted = 0
    filled = 0
    with get_db() as conn:
        rows = conn.execute(
            """
            SELECT a.id, a.name AS album, ar.name AS artist
            FROM albums a
            JOIN artists ar ON ar.id = a.artist_id
            WHERE a.cover_path IS NULL OR a.cover_path = ''
            ORDER BY a.id
            """
        ).fetchall()
        for row in rows:
            if limit and attempted >= limit:
                break
            attempted += 1
            rel = ensure_album_cover(
                conn,
                album_id=int(row["id"]),
                artist=row["artist"] or "",
                album=row["album"] or "",
            )
            if rel:
                filled += 1
            if pause_s > 0:
                time.sleep(pause_s)
    return {"attempted": attempted, "filled": filled, "remaining_hint": max(0, len(rows) - attempted)}


def cover_abs_path(cover_rel: Optional[str]) -> Optional[Path]:
    if not cover_rel:
        return None
    p = Path(cover_rel)
    if p.is_absolute():
        return p if p.is_file() else None
    full = settings.covers_dir / cover_rel
    return full if full.is_file() else None
