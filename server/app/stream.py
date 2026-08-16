"""Stream audio for PSP: always PSP-safe 320k MP3 (sceMp3), unless format=flac."""

from __future__ import annotations

import hashlib
import subprocess
from pathlib import Path
from typing import Optional

from fastapi import HTTPException
from fastapi.responses import FileResponse, Response

from .config import settings
from .ffmpeg_util import resolve_ffmpeg

# Anything else is transcoded to MP3 for PSP (sceMp3).
STREAM_BITRATE = "320k"


def is_mp3(path: Path) -> bool:
    return path.suffix.lower() == ".mp3"


def is_flac(path: Path) -> bool:
    return path.suffix.lower() == ".flac"


def _cache_path(src: Path) -> Path:
    settings.cache_dir.mkdir(parents=True, exist_ok=True)
    key = hashlib.sha1(str(src.resolve()).encode("utf-8", errors="replace")).hexdigest()
    # v2 = strip container metadata so PSP sceMp3Init sees audio sooner
    return settings.cache_dir / f"{key}_v2_{STREAM_BITRATE}.mp3"


def ensure_mp3(src: Path) -> Path:
    """Always produce a PSP-safe 320k MP3 (no ID3). Source may be m4a/wav/mp3."""
    if not src.is_file():
        raise HTTPException(status_code=404, detail="audio file missing")

    out = _cache_path(src)
    try:
        if out.is_file() and out.stat().st_mtime >= src.stat().st_mtime and out.stat().st_size > 1024:
            return out
    except OSError:
        pass

    ff = resolve_ffmpeg()
    if not ff:
        raise HTTPException(
            status_code=503,
            detail="ffmpeg unavailable — cannot transcode this format for PSP",
        )

    tmp = out.with_suffix(".tmp.mp3")
    try:
        if tmp.exists():
            tmp.unlink()
    except OSError:
        pass

    cmd = [
        ff,
        "-y",
        "-i",
        str(src),
        "-vn",
        "-codec:a",
        "libmp3lame",
        "-b:a",
        STREAM_BITRATE,
        "-ar",
        "44100",
        "-ac",
        "2",
        "-map_metadata",
        "-1",
        "-write_xing",
        "0",
        "-id3v2_version",
        "0",
        str(tmp),
    ]
    try:
        proc = subprocess.run(cmd, capture_output=True, timeout=600, check=False)
    except subprocess.TimeoutExpired as exc:
        raise HTTPException(status_code=504, detail="transcode timeout") from exc

    if proc.returncode != 0 or not tmp.is_file() or tmp.stat().st_size < 1024:
        try:
            tmp.unlink(missing_ok=True)
        except OSError:
            pass
        err = (proc.stderr or b"")[-400:].decode("utf-8", errors="replace")
        raise HTTPException(status_code=500, detail=f"transcode failed: {err}")

    tmp.replace(out)
    return out


def _mp3_frame_sync(data: bytes) -> int:
    """Return offset of first MPEG frame sync in data, or 0."""
    n = len(data)
    for i in range(0, n - 1):
        if data[i] != 0xFF:
            continue
        b1 = data[i + 1]
        if (b1 & 0xE0) != 0xE0:
            continue
        if (b1 & 0x18) == 0x08:  # reserved version
            continue
        if (b1 & 0x06) == 0x00:  # reserved layer
            continue
        return i
    return 0


def _byte_offset_for_ms(size: int, start_ms: int, duration_sec: Optional[float]) -> int:
    if start_ms <= 0 or size <= 0:
        return 0
    if duration_sec and duration_sec > 0.5:
        off = int(size * (float(start_ms) / (duration_sec * 1000.0)))
    else:
        # Assume ~320 kbps
        off = int((start_ms / 1000.0) * (320000.0 / 8.0))
    if off < 0:
        off = 0
    if off > size - 1024:
        off = max(0, size - 1024)
    return off


def _mp3_from_offset(
    mp3_path: Path,
    start_ms: int,
    duration_sec: Optional[float],
) -> Response:
    size = mp3_path.stat().st_size
    if start_ms <= 0:
        return FileResponse(
            mp3_path,
            media_type="audio/mpeg",
            filename=mp3_path.name,
        )
    off = _byte_offset_for_ms(size, start_ms, duration_sec)
    with mp3_path.open("rb") as f:
        f.seek(off)
        probe = f.read(8192)
        sync = _mp3_frame_sync(probe)
        off = off + sync

    remain = max(0, size - off)

    def _iter():
        with mp3_path.open("rb") as f:
            f.seek(off)
            while True:
                chunk = f.read(64 * 1024)
                if not chunk:
                    break
                yield chunk

    from fastapi.responses import StreamingResponse

    headers = {
        "Accept-Ranges": "bytes",
        "Content-Range": f"bytes {off}-{size - 1}/{size}",
        "Content-Length": str(remain),
    }
    return StreamingResponse(
        _iter(),
        media_type="audio/mpeg",
        status_code=206 if off > 0 else 200,
        headers=headers,
    )


def stream_file(
    path: Path,
    *,
    force_mp3: bool = False,
    prefer_flac: bool = False,
    start_ms: int = 0,
    duration_sec: Optional[float] = None,
) -> Response:
    """
    Playback / download policy (PSP-first):
    - default / force_mp3 → clean 320k MP3 (hardware sceMp3); FLAC sources included
    - prefer_flac + .flac + no seek → native FLAC (optional; soft-decode on PSP is fragile)
    - start_ms>0 → always MP3, frame-aligned
    Always includes Content-Length (PSP cannot parse chunked).
    """
    if not path.is_file():
        raise HTTPException(status_code=404, detail="audio file missing")

    start_ms = int(start_ms or 0)
    if start_ms < 0:
        start_ms = 0

    # Byte-seek only works reliably on MP3 for sceMp3Init.
    if start_ms > 0:
        return _mp3_from_offset(ensure_mp3(path), start_ms, duration_sec)

    if prefer_flac and not force_mp3 and is_flac(path):
        return FileResponse(
            path,
            media_type="audio/flac",
            filename=path.name,
        )

    return FileResponse(
        ensure_mp3(path),
        media_type="audio/mpeg",
        filename=path.name,
    )
