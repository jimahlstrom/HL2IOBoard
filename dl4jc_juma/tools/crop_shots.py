#!/usr/bin/env python3
"""Trims the desktop away from a window screenshot.

    python3 crop_shots.py docs/*.png

The window's chrome and contents are near-neutral greys; the desktop behind it
is a photograph with a colour cast. Rather than trusting one row or column -
which breaks at the first green heading or turquoise button - every row and
column votes, and the window is the largest block where most pixels are neutral.
"""

import os
import sys

from PIL import Image

NEUTRAL = 16        # how far from grey a pixel may be and still count as UI


def neutral(px):
    r, g, b = px[:3]
    return max(r, g, b) - min(r, g, b) < NEUTRAL


def longest_true(flags):
    """(start, end) of the longest run of True, inclusive."""
    best = (0, -1)
    start = None
    for i, f in enumerate(flags):
        if f:
            if start is None:
                start = i
        elif start is not None:
            if i - start > best[1] - best[0] + 1:
                best = (start, i - 1)
            start = None
    if start is not None and len(flags) - start > best[1] - best[0] + 1:
        best = (start, len(flags) - 1)
    return best


def edge(px, coords, want=True):
    """First coordinate along the path where a pixel is (or stops being) UI."""
    for x, y in coords:
        if neutral(px[x, y]) == want:
            return x, y
    return None


def crop(path, pad=0, manual=None):
    """Find the window by walking in from each edge.

    Voting over whole rows was tried first and is too clever: a screenshot has
    coloured headings, highlighted buttons and four dials in it, and every one
    of them drags a row's score down. Walking inward until the pixels turn into
    plain window is both simpler and harder to fool.

    It cannot work when the window in front and the desktop behind are both
    dark and grey - a dark window over a terminal, say. Nothing distinguishes
    them, so for that case the bounds can be given with `manual`.
    """
    im = Image.open(path).convert("RGB")
    w, h = im.size
    px = im.load()

    # Left and right along the title bar: that row is window from end to end.
    bar = 6
    left = edge(px, [(x, bar) for x in range(w)])
    right = edge(px, [(x, bar) for x in range(w - 1, -1, -1)])
    if manual:
        x0, y0, x1, y1 = manual
        out = im.crop((x0, y0, x1 + 1, y1 + 1))
        out.save(path)
        print("  %s: %dx%d -> %dx%d  (by hand)"
              % (os.path.basename(path), w, h, out.size[0], out.size[1]))
        return out.size

    if left is None or right is None or right[0] - left[0] < w // 3:
        print("  %s: no window along the title bar, left alone"
              % os.path.basename(path))
        return None
    x0, x1 = left[0], right[0]

    # Top and bottom just inside the left edge, where there is nothing but
    # window background - no headings, no buttons, no meters.
    probe = min(w - 1, x0 + 6)
    top = edge(px, [(probe, y) for y in range(h)])
    bottom = edge(px, [(probe, y) for y in range(h - 1, -1, -1)])
    if top is None or bottom is None or bottom[1] - top[1] < h // 3:
        print("  %s: no window down the edge, left alone"
              % os.path.basename(path))
        return None
    y0, y1 = top[1], bottom[1]

    box = (max(0, x0 - pad), max(0, y0 - pad),
           min(w, x1 + 1 + pad), min(h, y1 + 1 + pad))
    out = im.crop(box)
    out.save(path)
    print("  %s: %dx%d -> %dx%d  (trimmed %d left, %d right, %d top, %d bottom)"
          % (os.path.basename(path), w, h, out.size[0], out.size[1],
             x0, w - 1 - x1, y0, h - 1 - y1))
    return out.size


if __name__ == "__main__":
    # "file.png:x0,y0,x1,y1" gives the bounds by hand where the picture cannot
    # be told from its background.
    for p in sys.argv[1:]:
        if ":" in p and p.rsplit(":", 1)[1].count(",") == 3:
            path, box = p.rsplit(":", 1)
            crop(path, manual=tuple(int(v) for v in box.split(",")))
        else:
            crop(p)
