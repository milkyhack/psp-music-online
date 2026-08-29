#!/usr/bin/env python3
"""Compare PPSSPP QA captures (480x272) against docs/assets README references."""

from __future__ import annotations

import json
import sys
from datetime import datetime, timezone
from pathlib import Path

try:
    from PIL import Image, ImageChops, ImageStat
except ImportError:
    print("Install: pip install -r tools/ppsspp_qa/requirements.txt", file=sys.stderr)
    sys.exit(2)

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "docs" / "assets"
NATIVE = (480, 272)

SCREENS = {
    "home": "home.png",
    "library": "library.png",
    "now-playing": "now-playing.png",
    "coverflow": "coverflow.png",
    "setup": "setup.png",
}

# Per-screen gates — loose proxy SSIM is NOT real SSIM; keep thresholds high.
SCREEN_GATES = {
    "home": {"ssim_min": 0.82, "mae_max": 35.0},
    "library": {"ssim_min": 0.82, "mae_max": 40.0},
    "now-playing": {"ssim_min": 0.85, "mae_max": 32.0},
    "coverflow": {"ssim_min": 0.80, "mae_max": 40.0},
    "setup": {"ssim_min": 0.88, "mae_max": 25.0},
}

BG_CORNERS = [(8, 8), (471, 8), (8, 263), (471, 263)]
BG_TARGETS = [(7, 7, 10), (8, 8, 10), (0, 0, 0), (18, 18, 18)]
BG_TOL = 10
ACCENT_PROBE = (240, 248)
ACCENT_PROBES_NP = [(240, 238), (240, 242), (176, 66), (30, 10), (424, 10)]
ACCENT_PROBES_LIB = [(2, 108), (456, 108), (408, 10), (24, 37)]
ACCENT_PROBES_HOME = [(16, 8), (240, 34), (456, 18)]


def load_native(path: Path) -> Image.Image:
    im = Image.open(path).convert("RGB")
    if im.size == NATIVE:
        return im
    tw, th = NATIVE
    src_w, src_h = im.size
    target_ratio = tw / th
    src_ratio = src_w / src_h
    if src_ratio > target_ratio:
        new_w = int(src_h * target_ratio)
        left = (src_w - new_w) // 2
        box = (left, 0, left + new_w, src_h)
    else:
        new_h = int(src_w / target_ratio)
        top = (src_h - new_h) // 2
        box = (0, top, src_w, top + new_h)
    im = im.crop(box).resize(NATIVE, Image.Resampling.LANCZOS)
    return im


def load_actual(path: Path) -> Image.Image | None:
    if not path.exists():
        return None
    im = Image.open(path).convert("RGB")
    if im.size != NATIVE:
        im = im.resize(NATIVE, Image.Resampling.LANCZOS)
    return im


def corner_bg_ok(im: Image.Image) -> tuple[bool, str]:
    px = im.load()
    for x, y in BG_CORNERS:
        r, g, b = px[x, y]
        if any(
            abs(r - t[0]) <= BG_TOL and abs(g - t[1]) <= BG_TOL and abs(b - t[2]) <= BG_TOL
            for t in BG_TARGETS
        ):
            continue
        return False, f"bg corner ({x},{y})=({r},{g},{b})"
    return True, "bg corners ok"


def accent_present(im: Image.Image, name: str) -> tuple[bool, str]:
    if name not in ("now-playing", "home", "library"):
        return True, "n/a"
    px = im.load()
    if name == "now-playing":
        probes = ACCENT_PROBES_NP
    elif name == "home":
        probes = ACCENT_PROBES_HOME
    else:
        probes = ACCENT_PROBES_LIB if name == "library" else [ACCENT_PROBE, (24, 37), (456, 20)]
    for x, y in probes:
        if y >= im.height:
            y = im.height - 1
        r, g, b = px[min(x, im.width - 1), y]
        if g > r + 20 and g > b + 20:
            return True, f"accent ({r},{g},{b})"
    r, g, b = px[min(ACCENT_PROBE[0], im.width - 1), min(ACCENT_PROBE[1], im.height - 1)]
    return False, f"weak accent ({r},{g},{b})"


def simple_ssim(a: Image.Image, b: Image.Image) -> float:
    """Lightweight SSIM proxy (not structural SSIM — use with high thresholds)."""
    import math

    def stats(im: Image.Image) -> tuple[float, float]:
        gray = im.convert("L")
        st = ImageStat.Stat(gray)
        return st.mean[0], st.var[0]

    ma, va = stats(a)
    mb, vb = stats(b)
    diff = ImageChops.difference(a, b).convert("L")
    md = ImageStat.Stat(diff).mean[0]
    diff_score = max(0.0, 1.0 - md / 128.0)
    mean_score = 1.0 - min(1.0, abs(ma - mb) / 128.0)
    var_score = 1.0 - min(1.0, abs(math.sqrt(va) - math.sqrt(vb)) / 64.0)
    return 0.5 * diff_score + 0.25 * mean_score + 0.25 * var_score


def mae(a: Image.Image, b: Image.Image) -> float:
    diff = ImageChops.difference(a, b)
    st = ImageStat.Stat(diff)
    return sum(st.mean) / 3.0


def compare_one(name: str, actual_dir: Path, out_dir: Path) -> dict:
    ref_name = SCREENS[name]
    ref_path = ASSETS / ref_name
    act_path = actual_dir / f"{name}.bmp"
    gates = SCREEN_GATES.get(name, {"ssim_min": 0.85, "mae_max": 35.0})
    result: dict = {"screen": name, "reference": ref_name, "pass": False, "checks": []}

    if not ref_path.exists():
        result["checks"].append({"id": "ref", "pass": False, "detail": f"missing {ref_path}"})
        return result

    actual = load_actual(act_path)
    if actual is None:
        result["checks"].append({"id": "capture", "pass": False, "detail": f"missing {act_path}"})
        return result

    expected = load_native(ref_path)
    ok_size = actual.size == NATIVE
    result["checks"].append(
        {"id": "size", "pass": ok_size, "detail": f"{actual.size[0]}x{actual.size[1]}"}
    )

    ok_bg, bg_detail = corner_bg_ok(actual)
    result["checks"].append({"id": "bg", "pass": ok_bg, "detail": bg_detail})

    ok_acc, acc_detail = accent_present(actual, name)
    result["checks"].append({"id": "accent", "pass": ok_acc, "detail": acc_detail})

    ssim = simple_ssim(actual, expected)
    err = mae(actual, expected)
    ok_ssim = ssim >= gates["ssim_min"]
    ok_mae = err <= gates["mae_max"]
    result["checks"].append(
        {
            "id": "ssim",
            "pass": ok_ssim,
            "detail": f"{ssim:.3f} (min {gates['ssim_min']})",
        }
    )
    result["checks"].append(
        {"id": "mae", "pass": ok_mae, "detail": f"{err:.1f} (max {gates['mae_max']})"}
    )

    diff = ImageChops.difference(actual, expected)
    diff = diff.point(lambda p: min(255, p * 4))
    out_dir.mkdir(parents=True, exist_ok=True)
    actual.save(out_dir / f"{name}-actual.png")
    expected.save(out_dir / f"{name}-expected.png")
    diff.save(out_dir / f"{name}-diff.png")

    result["ssim"] = round(ssim, 4)
    result["mae"] = round(err, 2)
    result["pass"] = all(c["pass"] for c in result["checks"])
    return result


def main() -> int:
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <capture_dir> [report_dir]", file=sys.stderr)
        return 2

    actual_dir = Path(sys.argv[1])
    report_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else actual_dir

    report = {
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "native": {"w": NATIVE[0], "h": NATIVE[1]},
        "note": "PASS requires every check including accent/bg. SSIM is a coarse proxy only.",
        "screens": [],
        "pass": True,
    }

    for name in SCREENS:
        row = compare_one(name, actual_dir, report_dir)
        report["screens"].append(row)
        if not row["pass"]:
            report["pass"] = False

    report_path = report_dir / "report.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    print(json.dumps(report, indent=2))
    passed = sum(1 for s in report["screens"] if s["pass"])
    print(f"\nVisual QA: {passed}/{len(report['screens'])} screens PASS", file=sys.stderr)
    if not report["pass"]:
        print("FAIL — open *-diff.png in the report dir and compare to docs/assets/", file=sys.stderr)
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
