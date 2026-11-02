"""
Vael – Zeichen der Wetterrufe (Blender, ohne Oberfläche)
========================================================

Zeichnet 16 kleine Zauberzeichen in einen Atlas (4 x 4), die beim Wetterruf aus Mund und Händen zur Wolke steigen
(Boris 2026-11-02: die aufsteigenden Lichter sollen kleine Zeichen sein). Jedes Zeichen ist ein Stamm mit Ästen,
Bögen, Ringen und Punkten, wie eingeritzte Runen, aus festem Zufall gebaut.

Atlas (RGBA, 512 x 512): R = Strich, G = weicher Schein darum, A = beides.

Aufruf:
  blender -b -P vael_zeichen.py -- <Zielordner>
"""
import math
import os
import sys

import bpy
import numpy as np

CELL = 128
CELLS = 4


def segment(x, y, ax, ay, bx, by):
    px, py = x - ax, y - ay
    dx, dy = bx - ax, by - ay
    t = np.clip((px * dx + py * dy) / (dx * dx + dy * dy + 1e-9), 0, 1)
    return np.hypot(px - dx * t, py - dy * t)


def arc(x, y, cx, cy, r, a0, a1):
    angle = np.arctan2(y - cy, x - cx)
    inside = ((angle - a0) % (2 * math.pi)) <= ((a1 - a0) % (2 * math.pi))
    ring = np.abs(np.hypot(x - cx, y - cy) - r)
    ends = np.minimum(np.hypot(x - cx - r * math.cos(a0), y - cy - r * math.sin(a0)), np.hypot(x - cx - r * math.cos(a1), y - cy - r * math.sin(a1)))
    return np.where(inside, ring, ends)


def blur(a, r):
    for _ in range(2):
        for axis in (0, 1):
            a = sum(np.roll(a, k, axis) for k in range(-r, r + 1)) / (2 * r + 1)
    return a


def glyph(rng):
    """Abstand zum Zeichen in einer Zelle (Koordinaten 0..1, y nach unten)"""
    y, x = np.mgrid[0:CELL, 0:CELL].astype(np.float32)
    x, y = (x + 0.5) / CELL, (y + 0.5) / CELL
    d = np.full(x.shape, 1e3, np.float32)
    top, bottom = rng.uniform(0.12, 0.22), rng.uniform(0.78, 0.88)
    lean = rng.uniform(-0.08, 0.08)
    d = np.minimum(d, segment(x, y, 0.5 + lean, top, 0.5 - lean, bottom))
    for _ in range(rng.integers(1, 4)):
        # Äste schräg vom Stamm, oft paarweise gespiegelt
        t = rng.uniform(0.15, 0.85)
        sx, sy = 0.5 + lean * (1 - 2 * t), top + (bottom - top) * t
        side, length, up = rng.choice([-1, 1]), rng.uniform(0.18, 0.3), rng.uniform(-0.25, 0.2)
        d = np.minimum(d, segment(x, y, sx, sy, sx + side * length, sy + up))
        if rng.random() < 0.5:
            d = np.minimum(d, segment(x, y, sx, sy, sx - side * length, sy + up))
    kind = rng.integers(0, 4)
    if kind == 0:
        d = np.minimum(d, np.abs(np.hypot(x - 0.5, y - top - 0.08) - rng.uniform(0.08, 0.13)))
    elif kind == 1:
        a = rng.uniform(0, 2 * math.pi)
        d = np.minimum(d, arc(x, y, 0.5, 0.5, rng.uniform(0.22, 0.32), a, a + rng.uniform(2.0, 3.6)))
    elif kind == 2:
        for _ in range(rng.integers(1, 3)):
            d = np.minimum(d, np.hypot(x - rng.uniform(0.3, 0.7), y - rng.uniform(0.25, 0.75)) - 0.025)
    else:
        sy = rng.uniform(0.35, 0.65)
        d = np.minimum(d, segment(x, y, 0.3, sy, 0.7, sy + rng.uniform(-0.1, 0.1)))
    return d


def main(folder):
    os.makedirs(folder, exist_ok=True)
    rng = np.random.default_rng(23)
    size = CELL * CELLS
    stroke = np.zeros((size, size), np.float32)
    for index in range(CELLS * CELLS):
        row, column = divmod(index, CELLS)
        d = glyph(rng)
        stroke[row * CELL:(row + 1) * CELL, column * CELL:(column + 1) * CELL] = np.clip(1.0 - (d - 0.028) * CELL / 1.5, 0, 1)
    glow = np.clip(blur(stroke, 6) * 2.2, 0, 1)
    rgba = np.dstack([stroke, glow, np.zeros_like(stroke), np.maximum(stroke, glow)])
    path = os.path.join(folder, "T_Vael_Zeichen.png")
    image = bpy.data.images.new("T_Vael_Zeichen", size, size, alpha=True)
    image.pixels.foreach_set(np.ascontiguousarray(rgba[::-1]).astype(np.float32).ravel())
    image.filepath_raw = path
    image.file_format = "PNG"
    image.save()
    print("[Vael] Zeichen geschrieben:", path)


if __name__ == "__main__":
    main(sys.argv[sys.argv.index("--") + 1])
