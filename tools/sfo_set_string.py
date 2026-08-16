#!/usr/bin/env python3
"""Patch a UTF-8 string key inside an existing PARAM.SFO in place.

Sony XMB binary-searches SFO keys; they must stay alphabetically sorted.
mksfoex -s KEY=VAL prepends keys and breaks that order, so Version shows as "-".
Generate a default SFO with mksfoex, then patch DISC_VERSION here.
"""
from __future__ import annotations

import struct
import sys


FMT_UTF8 = 0x0204


def set_string(path: str, key: str, value: str) -> None:
    raw = bytearray(open(path, "rb").read())
    if raw[0:4] != b"\0PSF":
        raise SystemExit(f"{path}: not a PARAM.SFO")
    _magic, _ver, key_table, data_table, nent = struct.unpack_from("<4sIIII", raw, 0)
    payload = (value + "\0").encode("utf-8")
    for i in range(nent):
        ko, fmt, data_len, data_max, data_off = struct.unpack_from(
            "<HHIII", raw, 20 + i * 16
        )
        name = raw[key_table + ko :].split(b"\0", 1)[0].decode("ascii")
        if name != key:
            continue
        if fmt != FMT_UTF8:
            raise SystemExit(f"{path}: {key} is not a UTF-8 string (fmt={fmt:#x})")
        if len(payload) > data_max:
            raise SystemExit(
                f"{path}: {key}={value!r} needs {len(payload)} bytes, max={data_max}"
            )
        start = data_table + data_off
        raw[start : start + data_max] = b"\0" * data_max
        raw[start : start + len(payload)] = payload
        struct.pack_into("<I", raw, 20 + i * 16 + 4, len(payload))
        open(path, "wb").write(raw)
        return
    raise SystemExit(f"{path}: key {key} not found")


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit(f"usage: {sys.argv[0]} PARAM.SFO KEY VALUE")
    set_string(sys.argv[1], sys.argv[2], sys.argv[3])
    print(f"set {sys.argv[2]}={sys.argv[3]} in {sys.argv[1]}")


if __name__ == "__main__":
    main()
