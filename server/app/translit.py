"""Normalize library text for the PSP bitmap font (ASCII / Latin only)."""

from __future__ import annotations

import re
import unicodedata
from urllib.parse import unquote

_CYR = {
    "А": "A", "Б": "B", "В": "V", "Г": "G", "Д": "D", "Е": "E", "Ё": "Yo",
    "Ж": "Zh", "З": "Z", "И": "I", "Й": "Y", "К": "K", "Л": "L", "М": "M",
    "Н": "N", "О": "O", "П": "P", "Р": "R", "С": "S", "Т": "T", "У": "U",
    "Ф": "F", "Х": "Kh", "Ц": "Ts", "Ч": "Ch", "Ш": "Sh", "Щ": "Sch",
    "Ъ": "", "Ы": "Y", "Ь": "", "Э": "E", "Ю": "Yu", "Я": "Ya",
    "а": "a", "б": "b", "в": "v", "г": "g", "д": "d", "е": "e", "ё": "yo",
    "ж": "zh", "з": "z", "и": "i", "й": "y", "к": "k", "л": "l", "м": "m",
    "н": "n", "о": "o", "п": "p", "р": "r", "с": "s", "т": "t", "у": "u",
    "ф": "f", "х": "kh", "ц": "ts", "ч": "ch", "ш": "sh", "щ": "sch",
    "ъ": "", "ы": "y", "ь": "", "э": "e", "ю": "yu", "я": "ya",
}

_JUNK_ALBUMS = {
    "",
    "unknown album",
    "media.localized",
    "music",
    "unknown",
}


def _strip_combining(s: str) -> str:
    return "".join(c for c in unicodedata.normalize("NFKD", s) if not unicodedata.combining(c))


def to_latin(text: str | None, *, fallback: str = "") -> str:
    if not text:
        return fallback
    s = unquote(str(text)).replace("+", " ").strip()
    if not s:
        return fallback
    out: list[str] = []
    for ch in s:
        if ch in _CYR:
            out.append(_CYR[ch])
            continue
        o = ord(ch)
        if ch.isascii() and (32 <= o < 127):
            out.append(ch)
            continue
        # CJK / other: keep romanized letter if NFKD yields one, else drop
        decomposed = _strip_combining(ch)
        kept = False
        for d in decomposed:
            if d.isascii() and 32 <= ord(d) < 127 and (d.isalnum() or d in " -_'()[]."):
                out.append(d)
                kept = True
        if not kept and ch.isspace():
            out.append(" ")
    cleaned = re.sub(r"\s+", " ", "".join(out)).strip(" -_")
    return cleaned or fallback


def clean_album(name: str | None, *, artist: str | None = None) -> str:
    raw = unquote(str(name or "")).strip()
    if raw.lower() in _JUNK_ALBUMS:
        art = to_latin(artist, fallback="Album")
        return f"{art} Album" if art else "Album"
    return to_latin(raw, fallback="Album")


def clean_artist(name: str | None) -> str:
    return to_latin(name, fallback="Unknown Artist")


def clean_title(name: str | None, *, path_stem: str | None = None) -> str:
    t = to_latin(name, fallback="")
    if t:
        return t
    if path_stem:
        return to_latin(path_stem, fallback="Track")
    return "Track"


def folder_labels_from_relpath(rel: str | None) -> tuple[str, str]:
    """Return (artist, album) folder names as on disk (keeps Cyrillic for remote search)."""
    if not rel:
        return "", ""
    parts = [unquote(p) for p in str(rel).replace("\\", "/").split("/") if p and p not in (".", "..")]
    if not parts:
        return "", ""
    parts = parts[:-1]  # drop filename
    skip = {"music", "media.localized", "itunes music", "automatically add to music.localized"}
    while len(parts) >= 2 and parts[0].lower() in skip:
        parts = parts[1:]
    while len(parts) >= 2 and parts[0].lower() == "music":
        parts = parts[1:]
    album = parts[-1] if parts else ""
    artist = parts[-2] if len(parts) >= 2 else ""
    if album.lower() in _JUNK_ALBUMS:
        album = ""
    if artist.lower() in {"unknown artist", "unknown", "various artists", ""}:
        artist = ""
    return artist.strip(), album.strip()
