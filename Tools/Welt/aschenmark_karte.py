# Paints the map of the open world L_Vael (today: the Aschenmark) for the mini map and the big map, from the height map
# and the masks of aschenmark_welt.py, and writes the data CreateWorldMaps.py turns into /Game/Vael/UI/Karten/DA_Karte_Vael.
#
#   blender -b --factory-startup -P Tools/Welt/aschenmark_karte.py -- C:/Projekte/Vael/Scripts/Data/Welt
#
# Drawn like a map a scout of the camp would ink: parchment with stains, a light wash per zone, sepia contour lines,
# hatching on steep slopes, ripple lines along the coast, the roads and paths as they are walked (AM_Pfade.json from
# aschenmark_pfade.py, run that first), the settlements as little roofs.
#
# Writes T_AM_Karte.png (square, 2048 px; picture x = world X (north), picture y = world Y (east)) and AM_Karte.json.
# The picture lies over a square of the world (world_min, world_size in cm) that holds the whole land with a little sea round it.

import json
import math
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
siedlungen = json.load(open(os.path.join(folder, "AM_Siedlungen.json"), encoding="utf-8"))
pfade = json.load(open(os.path.join(folder, "AM_Pfade.json"), encoding="utf-8"))

# World metres of every picture pixel: rows go along Y (east), columns along X (north)
cols = MIN_X + (np.arange(PIXELS) + 0.5) / PIXELS * SQUARE_M
rows = MIN_Y + (np.arange(PIXELS) + 0.5) / PIXELS * SQUARE_M
WX, WY = np.meshgrid(cols, rows)                                               # [row, col]
inside = (WX >= 0) & (WX <= SIZE_X - 1) & (WY >= 0) & (WY <= SIZE_Y - 1)
ix = np.clip(WX, 0, SIZE_X - 1).astype(np.int32)
iy = np.clip(WY, 0, SIZE_Y - 1).astype(np.int32)
height = np.where(inside, h[ix, iy], -20.0)
zone = mask[np.clip(ix // 2, 0, mask.shape[0] - 1), np.clip(iy // 2, 0, mask.shape[1] - 1), 2]


def blur(a, r):
    """Three box blurs along both axes"""
    for _ in range(3):
        for axis in (0, 1):
            c = np.cumsum(np.pad(a, [(r + 1, r) if i == axis else (0, 0) for i in range(2)], mode="edge"), axis=axis)
            a = (np.take(c, range(2 * r + 1, c.shape[axis]), axis=axis) - np.take(c, range(0, c.shape[axis] - 2 * r - 1), axis=axis)) / (2 * r + 1)
    return a


def noise(scale, seed):
    """Smooth value noise with features of about scale pixels, 0 to 1"""
    r = np.random.default_rng(seed)
    small = max(PIXELS // scale, 2)
    base = r.random((small + 2, small + 2))
    coords = np.linspace(0, small, PIXELS)
    i0 = np.floor(coords).astype(np.int32)
    t = coords - i0
    t = t * t * (3 - 2 * t)
    a = base[i0][:, i0] * (1 - t)[None, :] + base[i0][:, i0 + 1] * t[None, :]
    b = base[i0 + 1][:, i0] * (1 - t)[None, :] + base[i0 + 1][:, i0 + 1] * t[None, :]
    return a * (1 - t)[:, None] + b * t[:, None]


zone = blur(zone, 40)
step = SQUARE_M / PIXELS
gy, gx = np.gradient(height, step)
slope = np.hypot(gx, gy)
shade = np.clip(0.93 + (-gx * 0.55 - gy * 0.35) * 0.45, 0.78, 1.06)
rng = np.random.default_rng(7)

# Parchment with stains and fibres
stain = noise(300, 1) * 0.55 + noise(90, 2) * 0.3 + noise(25, 3) * 0.15
fibre = 0.965 + 0.035 * rng.random((PIXELS, PIXELS))
paper = np.array([0.84, 0.76, 0.60]) * (0.86 + 0.2 * stain)[..., None] * fibre[..., None]

# A light wash per zone: ash basin grey-brown, slag slopes grey, glowing fields rust, cold pass blue-grey
palette = np.array([[0.80, 0.72, 0.58], [0.66, 0.63, 0.60], [0.86, 0.62, 0.46], [0.72, 0.74, 0.76]])
z = np.clip(zone * 3.0, 0.0, 3.0)
lo = np.floor(z).astype(np.int32)
hi = np.minimum(lo + 1, 3)
t = (z - lo)[..., None]
wash = palette[lo] * (1 - t) + palette[hi] * t
land = paper * (0.55 + 0.45 * wash / 0.8) * shade[..., None]

INK = np.array([0.24, 0.16, 0.10])
PALE = [0.90, 0.83, 0.66]


def inked(base, where, strength):
    strength = np.broadcast_to(np.asarray(strength, dtype=np.float32), where.shape)[..., None]
    return np.where(where[..., None], base * (1 - strength) + INK * strength, base)


# Contour lines every 10 m, stronger every 50 m, a little broken like a drawn line
wobble = (noise(40, 4) - 0.5) * 1.6
level = height + 1000.0 + wobble
thin = (np.abs((level % 10.0) - 5.0) > 4.6) & (height > 1.0)
thick = (np.abs((level % 50.0) - 25.0) > 24.35) & (height > 1.0)
land = inked(land, thin & (noise(12, 5) > 0.28), 0.22)
land = inked(land, thick, 0.45)

# Hatching on steep ground (mountains in the north and east, crater walls): short strokes across the slope
yy, xx = np.mgrid[0:PIXELS, 0:PIXELS]
hatch = (np.sin((xx * 0.7 + yy * 0.7) * 0.9 + noise(30, 6) * 6.0) > 0.55) & (slope > 0.35)
land = inked(land, hatch & (noise(8, 7) > 0.25), np.clip(slope - 0.25, 0, 0.6))

# Sea: a blue-grey wash on the parchment, ripple lines that follow the coast
is_sea = height < 0.4
sea_mask = is_sea.astype(np.float32)
near = blur(sea_mask, 6)
sea = paper * np.array([0.70, 0.80, 0.86]) * (0.9 + 0.1 * stain[..., None])
ripple = is_sea & (np.abs(((near * 7.0 + wobble * 0.08) % 1.0) - 0.5) < 0.035) & (near < 0.97)
sea = inked(sea, ripple, 0.35)
rgb = np.where(is_sea[..., None], sea, land)

# Coast line: ink where land meets the sea
rgb = inked(rgb, (blur(sea_mask, 1) > 0.08) & ~is_sea, 0.75)


def pixel(x_m, y_m):
    return int((y_m - MIN_Y) / SQUARE_M * PIXELS), int((x_m - MIN_X) / SQUARE_M * PIXELS)


def stamp(r, c, radius, colour, strength=1.0):
    """A soft round dab of ink at picture row r, column c"""
    rad = int(math.ceil(radius)) + 1
    r0, r1, c0, c1 = max(r - rad, 0), min(r + rad + 1, PIXELS), max(c - rad, 0), min(c + rad + 1, PIXELS)
    if r0 >= r1 or c0 >= c1:
        return
    d = np.hypot(np.arange(r0, r1)[:, None] - r, np.arange(c0, c1)[None, :] - c)
    a = (np.clip(radius + 0.5 - d, 0.0, 1.0) * strength)[..., None]
    rgb[r0:r1, c0:c1] = rgb[r0:r1, c0:c1] * (1 - a) + np.array(colour) * a


def dot(x_m, y_m, radius, colour, ring=None):
    r, c = pixel(x_m, y_m)
    if ring is not None:
        stamp(r, c, radius + 2, ring)
    stamp(r, c, radius, colour)


# Roads and paths as they are walked: the main road a pale bed between ink edges, side roads single, footpaths dashed
STYLE = {"haupt": (2.4, 0.9, None), "neben": (1.6, 0.8, None), "pfad": (1.0, 0.7, (6.0, 4.0))}
for road in pfade["wege"]:
    width, strength, dash = STYLE[road["kind"]]
    pts = np.array(road["points"])
    seg = np.linalg.norm(np.diff(pts, axis=0), axis=1)
    s = np.concatenate([[0.0], np.cumsum(seg)])
    ts = np.arange(0.0, s[-1], step * 0.5)
    xs, ys = np.interp(ts, s, pts[:, 0]), np.interp(ts, s, pts[:, 1])
    pressure = 0.85 + 0.15 * np.sin(ts / 37.0) * np.sin(ts / 13.0)
    if road["kind"] == "haupt":
        for x, y in zip(xs, ys):
            r, c = pixel(x, y)
            stamp(r, c, width + 1.5, INK, 0.85)
        for x, y in zip(xs, ys):
            r, c = pixel(x, y)
            stamp(r, c, width, PALE, 1.0)
        continue
    for k, (x, y) in enumerate(zip(xs, ys)):
        if dash is not None and (ts[k] % (dash[0] + dash[1])) > dash[0]:
            continue
        r, c = pixel(x, y)
        stamp(r, c, width * pressure[k], INK, strength)

# Places as small ink marks, the Nid source violet, the crater ember-red
for name, p in places["places"].items():
    colour = [0.42, 0.22, 0.50] if "Nid" in name else [0.72, 0.28, 0.10] if "Krater" in name else INK
    dot(p["x"], p["y"], 4, colour, ring=PALE)

# Settlements: one to three little roofs, the more people the more roofs
for settlement in siedlungen["siedlungen"]:
    if settlement["typ"] == "unterwegs":
        continue
    r, c = pixel(settlement["mitte"]["x"], settlement["mitte"]["y"])
    for dr, dc in ((0, 0), (-11, 14), (9, -14))[:1 + min(2, len(settlement["leute"]) // 3)]:
        rr, cc = r + dr, c + dc
        # a house: pale walls in an ink frame under a pointed roof
        for k in range(-6, 7):
            stamp(rr + 6, cc + k, 1.0, INK, 0.9)
        for k in range(0, 7):
            stamp(rr + k, cc - 6, 1.0, INK, 0.9)
            stamp(rr + k, cc + 6, 1.0, INK, 0.9)
        for k in range(-5, 6):
            for j in range(1, 6):
                stamp(rr + j, cc + k, 0.6, PALE, 1.0)
        for k in range(0, 9):
            stamp(rr - int(k / 1.3), cc - 8 + k, 1.1, INK, 0.95)
            stamp(rr - int(k / 1.3), cc + 8 - k, 1.1, INK, 0.95)
        stamp(rr + 4, cc, 1.3, INK, 0.8)

# Old edges: the parchment darkens and browns towards the border
v = np.hypot((xx - PIXELS / 2) / (PIXELS / 2), (yy - PIXELS / 2) / (PIXELS / 2))
burn = np.clip((v - 0.82) / 0.5, 0, 1) ** 1.5 * (0.7 + 0.3 * noise(60, 8))
rgb = rgb * (1 - burn[..., None] * 0.55) + np.array([0.32, 0.20, 0.10]) * burn[..., None] * 0.35
# The outermost band is one even colour: the map is shown clamped, and an uneven edge would smear into streaks
edge = np.maximum(np.abs(xx - PIXELS / 2), np.abs(yy - PIXELS / 2)) / (PIXELS / 2)
band = np.clip((edge - 0.95) / 0.04, 0, 1)[..., None]
rgb = rgb * (1 - band) + np.array([0.30, 0.25, 0.20]) * band

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
# Settlements that are not a place of their own get their name too, beside their roofs
known = {label["name"] for label in labels} | set(places["camps"])
for settlement in siedlungen["siedlungen"]:
    if settlement["typ"] != "unterwegs" and settlement["name"] not in known:
        labels.append({"name": settlement["name"], "world": [settlement["mitte"]["x"] * 100.0, settlement["mitte"]["y"] * 100.0 + 2500.0], "size": 0})
# The name of the whole land is drawn over the sea of the south-west, where it covers nothing
labels[0]["world"] = [300 * 100.0, 300 * 100.0]
camps = [{"name": name, "world": [p["x"] * 100.0, p["y"] * 100.0]} for name, p in places["camps"].items()]
for settlement in siedlungen["siedlungen"]:
    if settlement.get("schnellreise") and settlement["name"] not in places["camps"] and not any(
            math.hypot(c["world"][0] / 100.0 - settlement["mitte"]["x"], c["world"][1] / 100.0 - settlement["mitte"]["y"]) < 60 for c in camps):
        camps.append({"name": settlement["name"], "world": [settlement["mitte"]["x"] * 100.0, settlement["mitte"]["y"] * 100.0]})
data = {
    "title": "Die Aschenmark",
    "world_min": [MIN_X * 100.0, MIN_Y * 100.0],
    "world_size": SQUARE_M * 100.0,
    "focus_min": [0.0, 0.0],
    "focus_max": [(SIZE_X - 1) * 100.0, (SIZE_Y - 1) * 100.0],
    "fog_of_war": True,
    "camps": camps,
    "labels": labels,
}
with open(os.path.join(folder, "AM_Karte.json"), "w", encoding="utf-8") as f:
    json.dump(data, f, ensure_ascii=False, indent=1)
print("[aschenmark_karte] written", os.path.join(folder, "T_AM_Karte.png"), len(labels), "names", [c["name"] for c in camps])
