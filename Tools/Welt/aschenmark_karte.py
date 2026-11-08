# Paints the map of the open world L_Vael (today: the Aschenmark) for the mini map and the big map, from the height map
# and the masks of aschenmark_welt.py, and writes the data CreateWorldMaps.py turns into /Game/Vael/UI/Karten/DA_Karte_Vael.
#
#   blender -b --factory-startup -P Tools/Welt/aschenmark_karte.py -- C:/Projekte/Vael/Scripts/Data/Welt
#
# Writes T_AM_Karte.png (square, 2048 px; picture x = world X (north), picture y = world Y (east)) and AM_Karte.json.
# The picture lies over a square of the world (world_min, world_size in cm) that holds the whole land with a little sea round it.

import json
import os
import sys

import bpy
import numpy as np

folder = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else "C:/Projekte/Vael/Scripts/Data/Welt"
SIZE_X, SIZE_Y = 2017, 2521
PIXELS = 2048
SQUARE_M = 2600.0
MIN_X = (SIZE_X - 1) * 0.5 - SQUARE_M * 0.5
MIN_Y = (SIZE_Y - 1) * 0.5 - SQUARE_M * 0.5

raw = np.fromfile(os.path.join(folder, "AM_Hoehen.r16"), dtype="<u2")
h = ((raw.astype(np.float32) - 32768.0) / 128.0).reshape(SIZE_Y, SIZE_X).T     # h[x, y] in m

wege = bpy.data.images.load(os.path.join(folder, "T_AM_Wege.png"))
ww, wh = wege.size
mask = np.array(wege.pixels[:], dtype=np.float32).reshape(wh, ww, 4)[::-1]     # mask[x / 2, y / 2]
places = json.load(open(os.path.join(folder, "AM_Orte.json"), encoding="utf-8"))

# World metres of every picture pixel: rows go along Y (east), columns along X (north)
cols = MIN_X + (np.arange(PIXELS) + 0.5) / PIXELS * SQUARE_M
rows = MIN_Y + (np.arange(PIXELS) + 0.5) / PIXELS * SQUARE_M
WX, WY = np.meshgrid(cols, rows)                                               # [row, col]
inside = (WX >= 0) & (WX <= SIZE_X - 1) & (WY >= 0) & (WY <= SIZE_Y - 1)
ix = np.clip(WX, 0, SIZE_X - 1).astype(np.int32)
iy = np.clip(WY, 0, SIZE_Y - 1).astype(np.int32)
height = np.where(inside, h[ix, iy], -20.0)
roads = np.where(inside, mask[np.clip(ix // 2, 0, mask.shape[0] - 1), np.clip(iy // 2, 0, mask.shape[1] - 1), 0], 0.0)
zone = mask[np.clip(ix // 2, 0, mask.shape[0] - 1), np.clip(iy // 2, 0, mask.shape[1] - 1), 2]


def blur(a, r):
    """Three box blurs along both axes: the zones melt into each other instead of ending in hard circles"""
    for _ in range(3):
        for axis in (0, 1):
            c = np.cumsum(np.pad(a, [(r + 1, r) if i == axis else (0, 0) for i in range(2)], mode="edge"), axis=axis)
            a = (np.take(c, range(2 * r + 1, c.shape[axis]), axis=axis) - np.take(c, range(0, c.shape[axis] - 2 * r - 1), axis=axis)) / (2 * r + 1)
    return a


zone = blur(zone, 40)

# Relief: light from the north-west of the map, contour lines every 10 m
step = SQUARE_M / PIXELS
gy, gx = np.gradient(height, step)
shade = np.clip(0.62 + (-gx * 0.55 - gy * 0.35) * 0.9, 0.25, 1.15)
contour = np.abs(((height + 1000.0) % 10.0) - 5.0) > 4.55

# Colours of the four zones: ash basin, slag slopes, glowing fields, cold pass
palette = np.array([[0.40, 0.35, 0.30], [0.31, 0.28, 0.28], [0.46, 0.27, 0.19], [0.40, 0.40, 0.42]])
z = np.clip(zone * 3.0, 0.0, 3.0)
lo = np.floor(z).astype(np.int32)
hi = np.minimum(lo + 1, 3)
t = (z - lo)[..., None]
land = palette[lo] * (1 - t) + palette[hi] * t
rng = np.random.default_rng(7)
grain = 0.94 + 0.06 * rng.random((PIXELS, PIXELS))
land = land * shade[..., None] * grain[..., None]
land = np.where(contour[..., None], land * 0.82, land)
land = np.where((height > 60)[..., None], land * (1.0 + np.clip((height - 60) / 120.0, 0, 0.35))[..., None], land)

sea = np.array([0.09, 0.12, 0.15]) * (0.9 + 0.1 * grain[..., None])
is_sea = height < 0.4
shore = (height >= 0.4) & (height < 2.0)
rgb = np.where(is_sea[..., None], sea, land)
rgb = np.where(shore[..., None], np.array([0.55, 0.50, 0.42]), rgb)
rgb = np.where((roads > 0.35)[..., None], rgb * (1 - roads[..., None] * 0.8) + np.array([0.86, 0.78, 0.60]) * roads[..., None] * 0.8, rgb)


def pixel(x_m, y_m):
    return int((y_m - MIN_Y) / SQUARE_M * PIXELS), int((x_m - MIN_X) / SQUARE_M * PIXELS)


def dot(x_m, y_m, radius, colour, ring=None):
    r, c = pixel(x_m, y_m)
    rr, cc = np.ogrid[-radius - 3:radius + 4, -radius - 3:radius + 4]
    d = np.hypot(rr, cc)
    r0, c0 = r - radius - 3, c - radius - 3
    sl = (slice(max(r0, 0), r0 + d.shape[0]), slice(max(c0, 0), c0 + d.shape[1]))
    area = rgb[sl]
    dd = d[sl[0].start - r0:sl[0].start - r0 + area.shape[0], sl[1].start - c0:sl[1].start - c0 + area.shape[1]]
    if ring is not None:
        area[dd <= radius + 2] = ring
    area[dd <= radius] = colour


# Places as small dark marks, the Nid source violet, the crater ember-red
for name, p in places["places"].items():
    colour = [0.45, 0.25, 0.55] if "Nid" in name else [0.75, 0.30, 0.12] if "Krater" in name else [0.16, 0.12, 0.10]
    dot(p["x"], p["y"], 5, colour, ring=[0.85, 0.78, 0.62])

image = bpy.data.images.new("T_AM_Karte", PIXELS, PIXELS, alpha=True)
out = np.dstack([np.clip(rgb, 0, 1), np.ones((PIXELS, PIXELS))])[::-1]
image.pixels.foreach_set(np.ascontiguousarray(out).astype(np.float32).ravel())
image.filepath_raw = os.path.join(folder, "T_AM_Karte.png")
image.file_format = "PNG"
image.save()

labels = [{"name": "Die Aschenmark", "world": [1008 * 100.0, 1260 * 100.0], "size": 2}]
for name, p in places["zones"].items():
    labels.append({"name": name, "world": [p["x"] * 100.0, p["y"] * 100.0], "size": 1})
for name, p in places["places"].items():
    labels.append({"name": name, "world": [p["x"] * 100.0, p["y"] * 100.0 + 2500.0], "size": 0})
# The name of the whole land is drawn over the sea of the south-west, where it covers nothing
labels[0]["world"] = [300 * 100.0, 300 * 100.0]
data = {
    "title": "Die Aschenmark",
    "world_min": [MIN_X * 100.0, MIN_Y * 100.0],
    "world_size": SQUARE_M * 100.0,
    "focus_min": [0.0, 0.0],
    "focus_max": [(SIZE_X - 1) * 100.0, (SIZE_Y - 1) * 100.0],
    "fog_of_war": True,
    "camps": [{"name": name, "world": [p["x"] * 100.0, p["y"] * 100.0]} for name, p in places["camps"].items()],
    "labels": labels,
}
with open(os.path.join(folder, "AM_Karte.json"), "w", encoding="utf-8") as f:
    json.dump(data, f, ensure_ascii=False, indent=1)
print("[aschenmark_karte] written", os.path.join(folder, "T_AM_Karte.png"))
