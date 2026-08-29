"""Debug NDJSON logger — disabled in release (no-op)."""
from __future__ import annotations


def dlog(hypothesis_id: str, location: str, message: str, **data) -> None:
    return
