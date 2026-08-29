"""Locate ffmpeg: PATH → local bin/ → bundled imageio-ffmpeg."""

from __future__ import annotations

import shutil
import subprocess
from pathlib import Path
from typing import Any, Optional

_SERVER_ROOT = Path(__file__).resolve().parent.parent
_LOCAL_CANDIDATES = (
    _SERVER_ROOT / "bin" / "ffmpeg.exe",
    _SERVER_ROOT / "bin" / "ffmpeg",
    _SERVER_ROOT / "tools" / "ffmpeg" / "ffmpeg.exe",
    _SERVER_ROOT / "tools" / "ffmpeg" / "bin" / "ffmpeg.exe",
)


def resolve_ffmpeg() -> Optional[str]:
    found = shutil.which("ffmpeg")
    if found:
        return found
    for path in _LOCAL_CANDIDATES:
        if path.is_file():
            return str(path)
    try:
        import imageio_ffmpeg

        return imageio_ffmpeg.get_ffmpeg_exe()
    except Exception:
        return None


def ffmpeg_info() -> dict[str, Any]:
    path = resolve_ffmpeg()
    if not path:
        return {
            "available": False,
            "path": None,
            "version": None,
            "source": None,
        }
    source = "path"
    p = Path(path)
    if any(p.resolve() == c.resolve() for c in _LOCAL_CANDIDATES if c.exists()):
        source = "local"
    elif "imageio_ffmpeg" in path.replace("\\", "/").lower():
        source = "bundled"
    version = None
    try:
        proc = subprocess.run(
            [path, "-version"],
            capture_output=True,
            text=True,
            timeout=8,
            check=False,
        )
        first = (proc.stdout or "").splitlines()[:1]
        version = first[0].strip() if first else None
    except Exception:
        version = None
    return {
        "available": True,
        "path": path,
        "version": version,
        "source": source,
    }
