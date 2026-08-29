import sqlite3
from contextlib import contextmanager
from pathlib import Path
from typing import Any, Iterator, Optional

from .config import settings

AUDIO_EXTS = {
    ".mp3",
    ".flac",
    ".m4a",
    ".m4b",
    ".aac",
    ".ogg",
    ".oga",
    ".opus",
    ".wav",
    ".wave",
    ".wma",
    ".aiff",
    ".aif",
    ".aifc",
    ".ape",
    ".wv",
    ".mpc",
    ".ac3",
    ".mp2",
    ".mp4",
    ".caf",
    ".tak",
    ".dsf",
    ".dff",
    ".alac",
}


def _connect() -> sqlite3.Connection:
    settings.data_dir.mkdir(parents=True, exist_ok=True)
    settings.covers_dir.mkdir(parents=True, exist_ok=True)
    settings.cache_dir.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(settings.db_path, check_same_thread=False, timeout=60.0)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys = ON")
    conn.execute("PRAGMA journal_mode = WAL")
    conn.execute("PRAGMA busy_timeout = 60000")
    return conn


@contextmanager
def get_db() -> Iterator[sqlite3.Connection]:
    conn = _connect()
    try:
        yield conn
        conn.commit()
    except Exception:
        conn.rollback()
        raise
    finally:
        conn.close()


def _column_exists(conn: sqlite3.Connection, table: str, column: str) -> bool:
    rows = conn.execute(f"PRAGMA table_info({table})").fetchall()
    return any(str(r["name"]) == column for r in rows)


def _ensure_column(conn: sqlite3.Connection, table: str, column: str, typedef: str) -> None:
    if not _column_exists(conn, table, column):
        conn.execute(f"ALTER TABLE {table} ADD COLUMN {column} {typedef}")


def migrate_schema(conn: sqlite3.Connection) -> None:
    for col, typedef in (
        ("genre", "TEXT"),
        ("bio", "TEXT"),
        ("country", "TEXT"),
        ("external_score", "REAL"),
        ("external_source", "TEXT"),
        ("meta_fetched_at", "TEXT"),
    ):
        _ensure_column(conn, "artists", col, typedef)
    for col, typedef in (
        ("genre", "TEXT"),
        ("year", "INTEGER"),
        ("summary", "TEXT"),
        ("user_rating", "REAL"),
        ("external_score", "REAL"),
        ("external_source", "TEXT"),
        ("meta_fetched_at", "TEXT"),
        ("play_count", "INTEGER NOT NULL DEFAULT 0"),
    ):
        _ensure_column(conn, "albums", col, typedef)
    for col, typedef in (
        ("genre", "TEXT"),
        ("year", "INTEGER"),
        ("codec", "TEXT"),
        ("sample_rate", "INTEGER"),
        ("bit_depth", "INTEGER"),
        ("channels", "INTEGER"),
        ("bytes", "INTEGER"),
        ("lossless", "INTEGER"),
        ("bitrate", "INTEGER"),
        ("play_count", "INTEGER NOT NULL DEFAULT 0"),
        ("listen_ms", "INTEGER NOT NULL DEFAULT 0"),
        ("last_played", "TEXT"),
    ):
        _ensure_column(conn, "tracks", col, typedef)
    conn.execute(
        "CREATE INDEX IF NOT EXISTS idx_tracks_genre ON tracks(genre COLLATE NOCASE)"
    )
    conn.execute(
        "CREATE INDEX IF NOT EXISTS idx_albums_genre ON albums(genre COLLATE NOCASE)"
    )
    conn.execute(
        "CREATE INDEX IF NOT EXISTS idx_albums_year ON albums(year)"
    )
    conn.execute(
        "CREATE INDEX IF NOT EXISTS idx_tracks_plays ON tracks(play_count DESC)"
    )
    conn.execute(
        "CREATE INDEX IF NOT EXISTS idx_albums_plays ON albums(play_count DESC)"
    )
    conn.executescript(
        """
        CREATE TABLE IF NOT EXISTS playlists (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL COLLATE NOCASE,
            created_at TEXT NOT NULL DEFAULT (strftime('%s','now'))
        );
        CREATE TABLE IF NOT EXISTS playlist_tracks (
            playlist_id INTEGER NOT NULL,
            track_id INTEGER NOT NULL,
            pos INTEGER NOT NULL DEFAULT 0,
            PRIMARY KEY (playlist_id, track_id),
            FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,
            FOREIGN KEY (track_id) REFERENCES tracks(id) ON DELETE CASCADE
        );
        CREATE INDEX IF NOT EXISTS idx_playlist_tracks_pos ON playlist_tracks(playlist_id, pos);
        """
    )


def init_db() -> None:
    with get_db() as conn:
        conn.executescript(
            """
            CREATE TABLE IF NOT EXISTS artists (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL UNIQUE COLLATE NOCASE
            );

            CREATE TABLE IF NOT EXISTS albums (
                id INTEGER PRIMARY KEY,
                artist_id INTEGER NOT NULL,
                name TEXT NOT NULL,
                cover_path TEXT,
                FOREIGN KEY (artist_id) REFERENCES artists(id) ON DELETE CASCADE,
                UNIQUE (artist_id, name COLLATE NOCASE)
            );

            CREATE TABLE IF NOT EXISTS tracks (
                id INTEGER PRIMARY KEY,
                album_id INTEGER NOT NULL,
                artist_id INTEGER NOT NULL,
                title TEXT NOT NULL,
                track_num INTEGER,
                path TEXT NOT NULL UNIQUE,
                duration REAL,
                rating INTEGER NOT NULL DEFAULT 0 CHECK (rating >= 0 AND rating <= 5),
                cover_path TEXT,
                FOREIGN KEY (album_id) REFERENCES albums(id) ON DELETE CASCADE,
                FOREIGN KEY (artist_id) REFERENCES artists(id) ON DELETE CASCADE
            );

            CREATE TABLE IF NOT EXISTS meta (
                key TEXT PRIMARY KEY,
                value TEXT NOT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_tracks_album ON tracks(album_id);
            CREATE INDEX IF NOT EXISTS idx_tracks_artist ON tracks(artist_id);
            CREATE INDEX IF NOT EXISTS idx_tracks_rating ON tracks(rating DESC);
            CREATE INDEX IF NOT EXISTS idx_albums_artist ON albums(artist_id);
            """
        )
        migrate_schema(conn)


def get_or_create_artist(conn: sqlite3.Connection, name: str) -> int:
    name = (name or "Unknown Artist").strip() or "Unknown Artist"
    row = conn.execute(
        "SELECT id FROM artists WHERE name = ? COLLATE NOCASE", (name,)
    ).fetchone()
    if row:
        return int(row["id"])
    cur = conn.execute("INSERT INTO artists (name) VALUES (?)", (name,))
    return int(cur.lastrowid)


def get_or_create_album(
    conn: sqlite3.Connection, artist_id: int, name: str
) -> int:
    name = (name or "Unknown Album").strip() or "Unknown Album"
    row = conn.execute(
        "SELECT id FROM albums WHERE artist_id = ? AND name = ? COLLATE NOCASE",
        (artist_id, name),
    ).fetchone()
    if row:
        return int(row["id"])
    cur = conn.execute(
        "INSERT INTO albums (artist_id, name) VALUES (?, ?)",
        (artist_id, name),
    )
    return int(cur.lastrowid)


def upsert_track(
    conn: sqlite3.Connection,
    *,
    path: str,
    artist_id: int,
    album_id: int,
    title: str,
    track_num: Optional[int],
    duration: Optional[float],
    genre: Optional[str] = None,
    year: Optional[int] = None,
    codec: Optional[str] = None,
    sample_rate: Optional[int] = None,
    bit_depth: Optional[int] = None,
    channels: Optional[int] = None,
    bytes_: Optional[int] = None,
    lossless: Optional[int] = None,
    bitrate: Optional[int] = None,
) -> int:
    existing = conn.execute(
        "SELECT id, rating, cover_path FROM tracks WHERE path = ?", (path,)
    ).fetchone()
    if existing:
        conn.execute(
            """
            UPDATE tracks
            SET album_id = ?, artist_id = ?, title = ?, track_num = ?, duration = ?,
                genre = COALESCE(?, genre), year = COALESCE(?, year),
                codec = ?, sample_rate = COALESCE(?, sample_rate),
                bit_depth = COALESCE(?, bit_depth), channels = COALESCE(?, channels),
                bytes = COALESCE(?, bytes), lossless = ?,
                bitrate = COALESCE(?, bitrate)
            WHERE id = ?
            """,
            (
                album_id, artist_id, title, track_num, duration, genre, year,
                codec, sample_rate, bit_depth, channels, bytes_, lossless, bitrate,
                existing["id"],
            ),
        )
        return int(existing["id"])
    cur = conn.execute(
        """
        INSERT INTO tracks (
            album_id, artist_id, title, track_num, path, duration, genre, year,
            codec, sample_rate, bit_depth, channels, bytes, lossless, bitrate
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            album_id, artist_id, title, track_num, path, duration, genre, year,
            codec, sample_rate, bit_depth, channels, bytes_, lossless, bitrate,
        ),
    )
    return int(cur.lastrowid)


def set_meta(conn: sqlite3.Connection, key: str, value: str) -> None:
    conn.execute(
        "INSERT INTO meta (key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value",
        (key, value),
    )


def get_meta(conn: sqlite3.Connection, key: str, default: str = "") -> str:
    row = conn.execute("SELECT value FROM meta WHERE key = ?", (key,)).fetchone()
    return str(row["value"]) if row else default


def row_to_dict(row: Optional[sqlite3.Row]) -> Optional[dict[str, Any]]:
    if row is None:
        return None
    return dict(row)


def resolve_path(rel_or_abs: str) -> Path:
    """Join catalog path with MUSIC_DIR; never escape the library root."""
    music = settings.music_dir.resolve()
    p = Path(rel_or_abs)
    if p.is_absolute():
        resolved = p.resolve()
        try:
            resolved.relative_to(music)
        except ValueError as exc:
            raise ValueError("path outside library") from exc
        return resolved
    direct = (music / p).resolve()
    try:
        direct.relative_to(music)
    except ValueError as exc:
        raise ValueError("path outside library") from exc
    try:
        if direct.is_file():
            return direct
    except OSError:
        return direct
    if p.parts and music.name == p.parts[0]:
        stripped = (music / Path(*p.parts[1:])).resolve()
        try:
            stripped.relative_to(music)
            if stripped.is_file():
                return stripped
        except (ValueError, OSError):
            pass
    return direct
