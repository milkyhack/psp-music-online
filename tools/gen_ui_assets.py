#!/usr/bin/env python3
"""Regenerate AAA UI atlas / font / labels for PSP embed."""
from __future__ import annotations

import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "psp" / "assets" / "ui"
FONT_PATHS = [
    "/System/Library/Fonts/SFNS.ttf",
    "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
    "/System/Library/Fonts/Helvetica.ttc",
]


def load_font(size: float) -> ImageFont.FreeTypeFont:
    last = None
    for p in FONT_PATHS:
        try:
            return ImageFont.truetype(p, size=size)
        except Exception as e:  # noqa: BLE001
            last = e
    raise RuntimeError(f"no font: {last}")


def to_abgr_bytes(im: Image.Image) -> bytes:
    """Pillow RGBA -> PSP-ish ABGR8888 little-endian u32 bytes (A B G R in memory as R,G,B,A on LE write of ABGR)."""
    rgba = im.convert("RGBA")
    px = rgba.tobytes()
    out = bytearray(len(px))
    # Store as little-endian 0xAABBGGRR => bytes R,G,B,A (matches ui_gpu tint math)
    for i in range(0, len(px), 4):
        r, g, b, a = px[i : i + 4]
        out[i : i + 4] = bytes((r, g, b, a))
    return bytes(out)


def save_rgba(im: Image.Image, stem: str) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    im.convert("RGBA").save(OUT / f"{stem}.png")
    (OUT / f"{stem}.rgba").write_bytes(to_abgr_bytes(im))
    print(f"wrote {stem}.png/.rgba {im.size}")


def harden_alpha(im: Image.Image, thr: int = 110) -> Image.Image:
    """Near-binary alpha — keep glyph body, kill soft mush fringes."""
    _r, _g, _b, a = im.convert("RGBA").split()
    a = a.point(lambda v: 255 if v >= thr else 0)
    solid = Image.new("L", im.size, 255)
    return Image.merge("RGBA", (solid, solid, solid, a))


def gen_font() -> None:
    """512x256, 16x6 grid, 32px cells — bold stems for PSP readability."""
    W, H, CELL, COLS = 512, 256, 32, 16
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    SS = 3
    big = CELL * SS
    font = load_font(22 * SS)
    advances = []
    for code in range(32, 128):
        ch = chr(code)
        idx = code - 32
        col, row = idx % COLS, idx // COLS
        cell = Image.new("RGBA", (big, big), (0, 0, 0, 0))
        d = ImageDraw.Draw(cell)
        if ch != " ":
            bbox = d.textbbox((0, 0), ch, font=font)
            th = bbox[3] - bbox[1]
            x = 2 * SS - bbox[0]
            y = (big - th) // 2 - bbox[1] - SS
            d.text((x, y), ch, font=font, fill=(255, 255, 255, 255))
            d.text((x + 1, y), ch, font=font, fill=(255, 255, 255, 200))
        cell = cell.resize((CELL, CELL), Image.Resampling.BILINEAR)
        cell = harden_alpha(cell, thr=100)
        a = cell.split()[-1]
        bb = a.getbbox()
        ink_w = bb[2] if bb else CELL // 3
        if ch == ".":
            cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
            ImageDraw.Draw(cell).rectangle((6, 22, 14, 30), fill=(255, 255, 255, 255))
            ink_w = 16
        elif ch == ":":
            cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
            d2 = ImageDraw.Draw(cell)
            d2.rectangle((7, 6, 15, 14), fill=(255, 255, 255, 255))
            d2.rectangle((7, 18, 15, 26), fill=(255, 255, 255, 255))
            ink_w = 16
        elif ch == "-":
            cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
            ImageDraw.Draw(cell).rectangle((4, 14, 22, 18), fill=(255, 255, 255, 255))
            ink_w = 24
        advances.append(max(4, min(CELL - 1, ink_w + 2)))
        img.paste(cell, (col * CELL, row * CELL), cell)

    save_rgba(img, "font")
    (OUT / "font_adv.bin").write_bytes(bytes(advances))
    print("advances", advances[:16], "...", "avg", sum(advances) / len(advances))


def aa_circle(draw: ImageDraw.ImageDraw, xy, fill, width=0):
    draw.ellipse(xy, fill=fill, outline=None, width=width)


def gen_atlas() -> None:
    """256x256 chrome — premium soft icons, no SMOOTH mush filter."""
    im = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)

    # Fallback cover card 96x96 @0,0 — framed placeholder
    d.rounded_rectangle((0, 0, 95, 95), radius=12, fill=(28, 28, 38, 255))
    d.rounded_rectangle((4, 4, 91, 91), radius=10, fill=(18, 18, 26, 255))
    d.ellipse((28, 28, 68, 68), fill=(30, 215, 96, 255))
    d.polygon([(42, 38), (42, 58), (60, 48)], fill=(8, 8, 12, 255))

    # Card chrome 128x64 @96,0
    d.rounded_rectangle((96, 0, 223, 63), radius=10, fill=(20, 20, 28, 255))

    def icon_cell(ox, oy, kind):
        cx, cy = ox + 16, oy + 16
        col = (244, 244, 248, 255)
        if kind == "prev":
            d.rectangle((ox + 8, oy + 10, ox + 11, oy + 22), fill=col)
            d.polygon([(ox + 22, oy + 9), (ox + 12, oy + 16), (ox + 22, oy + 23)], fill=col)
        elif kind == "play":
            d.polygon([(ox + 11, oy + 8), (ox + 24, oy + 16), (ox + 11, oy + 24)], fill=col)
        elif kind == "pause":
            d.rectangle((ox + 10, oy + 9, ox + 14, oy + 23), fill=col)
            d.rectangle((ox + 18, oy + 9, ox + 22, oy + 23), fill=col)
        elif kind == "stop":
            d.rounded_rectangle((ox + 10, oy + 10, ox + 22, oy + 22), radius=2, fill=col)
        elif kind == "next":
            d.polygon([(ox + 10, oy + 9), (ox + 20, oy + 16), (ox + 10, oy + 23)], fill=col)
            d.rectangle((ox + 21, oy + 10, ox + 24, oy + 22), fill=col)

    for i, k in enumerate(["prev", "play", "pause", "stop", "next"]):
        icon_cell(i * 32, 96, k)

    d.rounded_rectangle((0, 160, 255, 191), radius=8, fill=(16, 16, 22, 230))
    d.rounded_rectangle((0, 192, 63, 215), radius=12, fill=(30, 215, 96, 255))

    # big play 64x64 — soft disc
    d.ellipse((160, 192, 223, 255), fill=(30, 215, 96, 255))
    d.ellipse((168, 200, 215, 247), fill=(30, 215, 96, 255))
    d.polygon([(182, 208), (206, 224), (182, 240)], fill=(8, 8, 12, 255))

    save_rgba(im, "atlas")


def gen_labels() -> None:
    """512x128 baked chrome strings with hard alpha."""
    im = Image.new("RGBA", (512, 128), (0, 0, 0, 0))
    font = load_font(18)
    font_sm = load_font(14)
    labels = [
        (0, 0, "Now Playing", (255, 255, 255, 255), font),
        (160, 0, "Paused", (160, 160, 168, 255), font),
        (240, 0, "SHUF", (160, 160, 168, 255), font_sm),
        (300, 0, "RPT", (160, 160, 168, 255), font_sm),
        (360, 0, "Music", (255, 255, 255, 255), font),
        (0, 32, "Up Next", (255, 255, 255, 255), font),
        (120, 32, "Loading", (160, 160, 168, 255), font_sm),
        (220, 32, "Now Playing", (30, 215, 96, 255), font),
        (400, 32, "Saved", (30, 215, 96, 255), font_sm),
        (0, 64, "Library", (255, 255, 255, 255), font),
        (120, 64, "Tracks", (255, 255, 255, 255), font_sm),
        (200, 64, "Albums", (160, 160, 168, 255), font_sm),
        (280, 64, "Artists", (160, 160, 168, 255), font_sm),
    ]
    for x, y, text, fill, f in labels:
        layer = Image.new("RGBA", im.size, (0, 0, 0, 0))
        ImageDraw.Draw(layer).text((x, y), text, font=f, fill=fill)
        r, g, b, a = layer.split()
        a = a.point(lambda v: 255 if v >= 140 else 0)
        layer = Image.merge("RGBA", (r, g, b, a))
        im = Image.alpha_composite(im, layer)
    save_rgba(im, "labels")


def main() -> None:
    gen_font()
    gen_atlas()
    gen_labels()
    print("done ->", OUT)


if __name__ == "__main__":
    main()
