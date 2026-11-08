# The shapes that are pressed into the ground (goal step 3): one picture of four cells, 256 px each, white with the shape
# in alpha (how deep, 0..1; the middle of a print deepest, the rim soft). Read by UVaelFootprintSubsystem.
#   cell 0  boot      sole and heel of a worn boot (the players)
#   cell 1  claw      three long toes and a pad (crawlers, apes, dolls)
#   cell 2  roll      a short stretch of a wide rolling track with cross ridges (Aschenroller)
#   cell 3  paw       a round pad with four toes (beasts)
#
#   blender -b --factory-startup -P Tools/Welt/fussabdruecke.py -- C:/Projekte/Vael_Quellen/Aschenmark/boden/export

import os
import sys

import bpy
import numpy as np

out = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else "C:/Projekte/Vael_Quellen/Aschenmark/boden/export"
C = 256
yy, xx = (np.mgrid[0:C, 0:C] + 0.5) / C * 2 - 1     # x across, y along (y up = toe)
rng = np.random.default_rng(3)


def soft(d, edge=0.12):
    """Depth from a signed distance (negative inside): deep inside, soft rim"""
    return np.clip(-d / edge, 0, 1) ** 0.7


def ellipse(cx, cy, rx, ry):
    return np.hypot((xx - cx) / rx, (yy - cy) / ry) - 1.0


grain = rng.random((C, C)) * 0.08

# Boot: sole (toe wide, waist narrow) and a separate heel; the tread shows as faint ridges
sole = np.minimum(ellipse(0.0, 0.35, 0.36, 0.5), ellipse(0.0, 0.05, 0.27, 0.35))
heel = ellipse(0.0, -0.62, 0.3, 0.26)
boot = np.maximum(soft(sole * 0.35), soft(heel * 0.35))
tread = 0.85 + 0.15 * (np.sin(yy * 40) > 0.3)
boot = boot * tread

# Claw: a pad and three long toes spread forward, each ending in a deeper point
claw = soft(ellipse(0.0, -0.45, 0.28, 0.22) * 0.3)
for angle in (-0.45, 0.0, 0.45):
    ax, ay = np.sin(angle), np.cos(angle)
    t = np.clip((xx * ax + (yy + 0.3) * ay), 0, 1.0)
    px, py = ax * t, -0.3 + ay * t
    d = np.hypot(xx - px, yy - py) - (0.09 - 0.04 * t)
    claw = np.maximum(claw, soft(d, 0.06) * (0.7 + 0.3 * t))

# Roll: a band across the cell with ridges across it, the edges pushed up a little (negative = raised)
band = np.clip(1 - np.abs(xx) / 0.8, 0, 1)
roll = soft((np.abs(xx) - 0.8) * 0.5) * (0.75 + 0.25 * (np.sin(yy * 22) > 0))
roll = roll * np.clip((1 - np.abs(yy)) / 0.15, 0, 1)

# Paw: a round pad, four oval toes in a fan
paw = soft(ellipse(0.0, -0.25, 0.32, 0.27) * 0.3)
for k, angle in enumerate((-0.7, -0.24, 0.24, 0.7)):
    cx, cy = np.sin(angle) * 0.5, -0.2 + np.cos(angle) * 0.55
    paw = np.maximum(paw, soft(ellipse(cx, cy, 0.12, 0.15) * 0.3))

cells = [boot, claw, roll, paw]
atlas = np.zeros((C, C * 4, 4), dtype=np.float32)
for i, cell in enumerate(cells):
    a = np.clip(cell - grain * (cell > 0.05), 0, 1)
    atlas[:, i * C:(i + 1) * C, 0:3] = 1.0
    atlas[:, i * C:(i + 1) * C, 3] = a
image = bpy.data.images.new("T_AM_Fussabdruecke", C * 4, C, alpha=True)
image.colorspace_settings.name = "Non-Color"
image.pixels.foreach_set(np.ascontiguousarray(atlas).ravel())
image.filepath_raw = os.path.join(out, "T_AM_Fussabdruecke.png")
image.file_format = "PNG"
image.save()
print("[fussabdruecke] written", image.filepath_raw)
