#!/usr/bin/env python3
"""gen_weather_icons.py
Download Twemoji PNG weather icons and convert to RGB565 PROGMEM arrays.
Output: src/icons_weather.h

Twemoji is copyright Twitter Inc and other contributors.
Licensed under CC BY 4.0 — https://creativecommons.org/licenses/by/4.0/
"""
import sys
import io
import urllib.request
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow not installed.  Run:  pip install Pillow")
    sys.exit(1)

ICON_W  = 70    # pixels — must match WI_W in generated header
ICON_H  = 70
# Magenta (0xF81F) is used as a transparent colour key.
# It does not appear in any of the weather emoji.
TRANSP  = 0xF81F

# Each entry: (c_var_name, twemoji_hex_codepoints, description)
# Codepoints: lowercase hex joined by '-', matching Twemoji 72x72 filenames.
ICONS = [
    ("clear",         "2600-fe0f",  "Clear sky (WMO 0 day)"),
    ("mainly_clear",  "1f324-fe0f", "Mainly clear (WMO 1 day)"),
    ("partly_cloudy", "26c5",       "Partly cloudy (WMO 2 day)"),
    ("overcast",      "2601-fe0f",  "Overcast (WMO 3)"),
    ("fog",           "1f32b-fe0f", "Fog (WMO 45-48)"),
    ("drizzle",       "1f326-fe0f", "Drizzle / Showers (WMO 51-57, 80-84)"),
    ("rain",          "1f327-fe0f", "Rain (WMO 61-67)"),
    ("snow",          "1f328-fe0f", "Snow / Snow showers (WMO 71-77, 85-86)"),
    ("thunder",       "26c8-fe0f",  "Thunderstorm (WMO 95+)"),
    ("clear_night",   "1f319",      "Clear sky night (WMO 0-1 night)"),
]

# Composite icons built by layering two Twemoji images on a transparent canvas.
# Each entry: (var_name, [(codepoint, px_size, x, y), ...], description)
COMPOSITE_ICONS = [
    ("partly_cloudy_night",
     [("1f319", 42, 2, 2), ("2601-fe0f", 54, 14, 16)],
     "Partly cloudy night (WMO 2 night)"),
    ("rain_shower",
     [("2600-fe0f", 40, 0, 0), ("1f327-fe0f", 52, 16, 18)],
     "Rain showers day (WMO 80-84) — sun + rain cloud"),
    ("rain_shower_night",
     [("1f319", 40, 0, 0), ("1f327-fe0f", 52, 16, 18)],
     "Rain showers night (WMO 80-84) — moon + rain cloud"),
    ("drizzle_night",
     [("1f319", 40, 0, 0), ("1f327-fe0f", 52, 16, 18)],
     "Drizzle night (WMO 51-57) — moon + cloud (no sun)"),
]

BASE_URL = "https://cdn.jsdelivr.net/gh/twitter/twemoji@14.0.2/assets/72x72/"

# ---------------------------------------------------------------------------

def pixel_to_rgb565(r: int, g: int, b: int, a: int) -> int:
    """Convert an RGBA pixel to RGB565, or return TRANSP if semi/fully transparent.
    Hard threshold (alpha >= 128): treat as fully opaque — avoids dark alpha-blend
    halos when the display background is not black.
    """
    if a < 128:
        return TRANSP
    val = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    # Guard: never accidentally emit the transparent key colour
    if val == TRANSP:
        val ^= 0x0001
    return val


def fetch_png(name: str, codepoint: str) -> bytes:
    """Try the given codepoint, and the variant without/with -fe0f, until one works."""
    candidates = [codepoint]
    if codepoint.endswith("-fe0f"):
        candidates.append(codepoint[:-5])   # try without variation selector
    else:
        candidates.append(codepoint + "-fe0f")  # try with variation selector

    for cp in candidates:
        url = BASE_URL + cp + ".png"
        try:
            print(f"  GET {url} ...", end=" ", flush=True)
            req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
            with urllib.request.urlopen(req, timeout=15) as resp:
                data = resp.read()
            print("OK")
            return data
        except Exception as exc:
            print(f"FAIL ({exc})")

    raise RuntimeError(f"Could not download Twemoji icon for '{name}' (tried: {candidates})")


def convert_to_rgb565(png_bytes: bytes) -> tuple[list[int], "Image.Image"]:
    """Resize PNG to ICON_W×ICON_H and return (RGB565 pixel list, PIL image)."""
    img = (Image.open(io.BytesIO(png_bytes))
               .convert("RGBA")
               .resize((ICON_W, ICON_H), Image.LANCZOS))
    pixels = [pixel_to_rgb565(r, g, b, a) for r, g, b, a in img.getdata()]
    return pixels, img


def make_composite(layers: list) -> tuple[list[int], "Image.Image"]:
    """Build an icon by compositing multiple Twemoji images on a transparent canvas."""
    canvas = Image.new("RGBA", (ICON_W, ICON_H), (0, 0, 0, 0))
    for codepoint, size, x, y in layers:
        raw = fetch_png(codepoint, codepoint)
        img = (Image.open(io.BytesIO(raw))
                   .convert("RGBA")
                   .resize((size, size), Image.LANCZOS))
        canvas.paste(img, (x, y), img)
    pixels = [pixel_to_rgb565(r, g, b, a) for r, g, b, a in canvas.getdata()]
    return pixels, canvas


def main() -> None:
    root     = Path(__file__).resolve().parent.parent
    out      = root / "src" / "icons_weather.h"
    preview  = Path(__file__).resolve().parent / "icons_preview"
    preview.mkdir(exist_ok=True)
    print(f"Output → {out}")
    print(f"Preview → {preview}\n")

    lines = [
        "// Auto-generated by tools/gen_weather_icons.py — DO NOT EDIT.",
        "// Twemoji © Twitter Inc — CC BY 4.0  https://creativecommons.org/licenses/by/4.0/",
        "#pragma once",
        f"#define WI_W      {ICON_W}U",
        f"#define WI_H      {ICON_H}U",
        f"#define WI_TRANSP 0x{TRANSP:04X}U",
        "",
    ]

    for name, codepoint, desc in ICONS:
        print(f"[{name}] {desc}")
        raw    = fetch_png(name, codepoint)
        pixels, pil_img = convert_to_rgb565(raw)
        total  = len(pixels)
        assert total == ICON_W * ICON_H, \
            f"Expected {ICON_W * ICON_H} pixels, got {total}"
        pil_img.save(preview / f"{name}.png")

        lines.append(f"// {desc}")
        lines.append(f"static const uint16_t wi_{name}[{total}] PROGMEM = {{")
        for i in range(0, total, 16):
            chunk = pixels[i : i + 16]
            lines.append("  " + ",".join(f"0x{p:04X}" for p in chunk) + ",")
        lines.append("};")
        lines.append("")

    for name, layers, desc in COMPOSITE_ICONS:
        print(f"[{name}] {desc} (composite)")
        pixels, pil_img = make_composite(layers)
        total  = len(pixels)
        assert total == ICON_W * ICON_H, \
            f"Expected {ICON_W * ICON_H} pixels, got {total}"
        pil_img.save(preview / f"{name}.png")

        lines.append(f"// {desc}")
        lines.append(f"static const uint16_t wi_{name}[{total}] PROGMEM = {{")
        for i in range(0, total, 16):
            chunk = pixels[i : i + 16]
            lines.append("  " + ",".join(f"0x{p:04X}" for p in chunk) + ",")
        lines.append("};")
        lines.append("")

    # Alias: mainly-clear night reuses the moon icon — copy the PNG too
    lines.append("// Mainly clear night — alias of clear_night (same moon icon)")
    lines.append("#define wi_mainly_clear_night wi_clear_night")
    lines.append("")
    import shutil
    shutil.copy(preview / "clear_night.png", preview / "mainly_clear_night.png")

    out.write_text("\n".join(lines), encoding="utf-8")
    size_kb = out.stat().st_size // 1024
    png_count = len(list(preview.glob("*.png")))
    print(f"\nDone!  {size_kb} KB written to {out.name}")
    print(f"       {png_count} PNG previews saved to tools/icons_preview/")


if __name__ == "__main__":
    main()
