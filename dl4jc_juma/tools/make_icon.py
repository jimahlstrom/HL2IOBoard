#!/usr/bin/env python3
"""Draws the application icon and writes it in every shape the three systems want.

    python3 make_icon.py [outdir]

Needs Pillow, but only here: the icon is built once and checked in, so the
window itself keeps its "standard library plus pyserial" footing.

The picture is the instrument the window is built around - a 180 degree meter
with the amplifier's own zones, green through amber to red, and a needle just
past the middle. Nothing that has to be read at 16 pixels, because nothing can.
"""

import math
import os
import subprocess
import sys

from PIL import Image, ImageDraw

BG = (30, 34, 40)          # --card of the dark theme
RING = (58, 64, 73)        # --line
OK = (0, 179, 60)          # --ok
WARN = (255, 153, 0)       # --warn
BAD = (230, 0, 0)          # --bad
NEEDLE = (238, 238, 238)   # --fg

SIZES = [16, 32, 64, 128, 256, 512, 1024]


def draw(px):
    """One square icon, px by px, drawn at 4x and shrunk for smooth edges."""
    s = px * 4
    im = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)

    pad = s * 0.06
    d.rounded_rectangle([pad, pad, s - pad, s - pad],
                        radius=s * 0.22, fill=BG, outline=RING,
                        width=max(1, int(s * 0.012)))

    cx, cy, r = s / 2.0, s * 0.60, s * 0.30
    w = max(2, int(s * 0.10))
    box = [cx - r, cy - r, cx + r, cy + r]
    # 180 degrees split the way the temperature dial is: green, amber, red.
    for a0, a1, col in ((180, 265, OK), (265, 310, WARN), (310, 360, BAD)):
        d.arc(box, a0, a1, fill=col, width=w)

    ang = math.radians(180 - 0.62 * 180)        # a needle just past the middle
    d.line([cx, cy, cx + (r - w) * math.cos(ang), cy - (r - w) * math.sin(ang)],
           fill=NEEDLE, width=max(2, int(s * 0.035)))
    hub = s * 0.035
    d.ellipse([cx - hub, cy - hub, cx + hub, cy + hub], fill=NEEDLE)

    return im.resize((px, px), Image.LANCZOS)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "icons")
    os.makedirs(out, exist_ok=True)

    pngs = {}
    for px in SIZES:
        im = draw(px)
        path = os.path.join(out, "juma-pa-%d.png" % px)
        im.save(path)
        pngs[px] = im
        print("wrote", path)

    # Windows: one .ico carrying the small sizes
    ico = os.path.join(out, "juma-pa.ico")
    pngs[256].save(ico, sizes=[(s, s) for s in (16, 32, 48, 64, 128, 256)])
    print("wrote", ico)

    # macOS: an .iconset turned into .icns by iconutil
    iconset = os.path.join(out, "juma-pa.iconset")
    os.makedirs(iconset, exist_ok=True)
    for px in (16, 32, 128, 256, 512):
        pngs[px].save(os.path.join(iconset, "icon_%dx%d.png" % (px, px)))
        pngs[px * 2].save(os.path.join(iconset, "icon_%dx%d@2x.png" % (px, px)))
    icns = os.path.join(out, "juma-pa.icns")
    try:
        subprocess.check_call(["iconutil", "-c", "icns", iconset, "-o", icns])
        print("wrote", icns)
    except (OSError, subprocess.CalledProcessError) as e:
        print("no .icns (iconutil is macOS only):", e)


if __name__ == "__main__":
    main()
