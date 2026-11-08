# Draws the roads of the Aschenmark as they would be walked: rounded corners, a gentle meander, footpaths out to the
# settlements. The landscape keeps its flattened road corridor (aschenmark_welt.py, 5 m flat + 18 m blend); the walked
# line stays inside it, so nothing has to be rebuilt. Writes:
#   AM_Pfade.json   every road and path as a line in world metres (kind: haupt, neben, pfad) for the map and the dressing
#   T_AM_Wege.png   the road channel (R) drawn anew from these lines; clearings (G) and zones (B) stay as they are
#
#   blender -b --factory-startup -P Tools/Welt/aschenmark_pfade.py -- C:/Projekte/Vael/Scripts/Data/Welt
#
# Run after aschenmark_welt.py (which writes straight roads) and before aschenmark_karte.py.

import json
import math
import os
import sys

import bpy
import numpy as np

folder = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else "C:/Projekte/Vael/Scripts/Data/Welt"
orte = json.load(open(os.path.join(folder, "AM_Orte.json"), encoding="utf-8"))
siedlungen = json.load(open(os.path.join(folder, "AM_Siedlungen.json"), encoding="utf-8"))
rng = np.random.default_rng(1108)

# How far the walked line may leave the old straight one (m) and how wide it is drawn (m, half width, strength)
MEANDER = {"haupt": 6.0, "neben": 6.0, "pfad": 6.0}
WIDTH = {"haupt": (2.4, 1.0), "neben": (1.6, 0.85), "pfad": (0.9, 0.55)}


def corners(line, spacing=40.0):
    """The straight line of the generator thinned to its corners (points closer than spacing are dropped)"""
    pts = [np.array([p["x"], p["y"]]) for p in line]
    out = [pts[0]]
    for p in pts[1:-1]:
        if np.linalg.norm(p - out[-1]) >= spacing:
            out.append(p)
    out.append(pts[-1])
    return np.array(out)


def chaikin(pts, rounds=4):
    """Rounds every corner, keeps both ends"""
    for _ in range(rounds):
        q = [pts[0]]
        for a, b in zip(pts[:-1], pts[1:]):
            q.append(a * 0.75 + b * 0.25)
            q.append(a * 0.25 + b * 0.75)
        q.append(pts[-1])
        pts = np.array(q)
    return pts


def resample(pts, step=1.0):
    seg = np.linalg.norm(np.diff(pts, axis=0), axis=1)
    s = np.concatenate([[0.0], np.cumsum(seg)])
    t = np.arange(0.0, s[-1], step)
    return np.column_stack([np.interp(t, s, pts[:, 0]), np.interp(t, s, pts[:, 1])]), t, s[-1]


def meander(pts, amplitude):
    """Sideways drift of a few metres in long and short waves; both ends stay where they are"""
    pts, s, length = resample(pts)
    d = np.gradient(pts, axis=0)
    normal = np.column_stack([-d[:, 1], d[:, 0]]) / np.maximum(np.linalg.norm(d, axis=1), 1e-6)[:, None]
    phase = rng.uniform(0, 2 * math.pi, 3)
    wave = (np.sin(s / 95.0 + phase[0]) * 0.55 + np.sin(s / 41.0 + phase[1]) * 0.3 + np.sin(s / 14.0 + phase[2]) * 0.15)
    taper = np.clip(np.minimum(s, length - s) / 20.0, 0.0, 1.0)
    out = pts + normal * (wave * amplitude * taper)[:, None]
    return out[::2]


def nearest_on(lines, p):
    best, best_d = None, 1e9
    for line in lines:
        d = np.linalg.norm(line - p, axis=1)
        i = int(np.argmin(d))
        if d[i] < best_d:
            best, best_d = line[i], d[i]
    return best, best_d


roads = []
main = meander(chaikin(corners(orte["main_road"])), MEANDER["haupt"])
roads.append({"name": "Hauptweg", "kind": "haupt", "points": main})
for name, line in orte["side_roads"].items():
    roads.append({"name": name, "kind": "neben", "points": meander(chaikin(corners(line, 25.0)), MEANDER["neben"])})

# Footpaths: every settlement that is not on a road gets a trodden path from the nearest road
for settlement in siedlungen["siedlungen"]:
    centre = np.array([settlement["mitte"]["x"], settlement["mitte"]["y"]])
    start, distance = nearest_on([r["points"] for r in roads], centre)
    if distance < 25.0:
        continue
    # A path bends: one midpoint pushed aside, then rounded
    mid = (start + centre) / 2 + rng.normal(0, distance * 0.12, 2)
    pts = np.array([start, mid, centre])
    roads.append({"name": "Pfad " + settlement["name"], "kind": "pfad", "points": meander(chaikin(pts, 5), MEANDER["pfad"])})
    print("[aschenmark_pfade] path to {}: {:.0f} m".format(settlement["name"], distance))

# The road channel at the resolution of the mask (2 m per pixel, picture rows = world X north, columns = world Y east)
wege = bpy.data.images.load(os.path.join(folder, "T_AM_Wege.png"))
ww, wh = wege.size
mask = np.array(wege.pixels[:], dtype=np.float32).reshape(wh, ww, 4)[::-1].copy()   # [x / 2, y / 2]
channel = np.zeros((wh, ww), dtype=np.float32)
CELL = 2.0
for road in roads:
    half, strength = WIDTH[road["kind"]]
    pts, _, _ = resample(road["points"], 0.5)
    reach = int(math.ceil((half + 2.5) / CELL)) + 1
    for x, y in pts[::2]:
        cx, cy = int(x / CELL), int(y / CELL)
        x0, x1 = max(cx - reach, 0), min(cx + reach + 1, wh)
        y0, y1 = max(cy - reach, 0), min(cy + reach + 1, ww)
        if x0 >= x1 or y0 >= y1:
            continue
        gx = (np.arange(x0, x1) + 0.5) * CELL
        gy = (np.arange(y0, y1) + 0.5) * CELL
        d = np.hypot(gx[:, None] - x, gy[None, :] - y)
        v = np.clip(1.0 - (d - half) / 2.0, 0.0, 1.0)
        v = v * v * (3 - 2 * v) * strength
        channel[x0:x1, y0:y1] = np.maximum(channel[x0:x1, y0:y1], v)
mask[..., 0] = channel
image = bpy.data.images.new("T_AM_Wege_neu", ww, wh, alpha=True)
image.pixels.foreach_set(np.ascontiguousarray(mask[::-1]).astype(np.float32).ravel())
image.filepath_raw = os.path.join(folder, "T_AM_Wege.png")
image.file_format = "PNG"
image.save()

data = {
    "info": "Wege und Pfade in Weltmetern (x Norden, y Osten), so wie sie gegangen werden; kind: haupt, neben, pfad",
    "wege": [{"name": r["name"], "kind": r["kind"], "points": [[round(float(x), 2), round(float(y), 2)] for x, y in r["points"]]} for r in roads],
}
with open(os.path.join(folder, "AM_Pfade.json"), "w", encoding="utf-8") as f:
    json.dump(data, f, ensure_ascii=False, indent=0)
print("[aschenmark_pfade] {} roads and paths written".format(len(roads)))
