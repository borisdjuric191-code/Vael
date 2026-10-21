"""Map picture of the Wurzelforst (region II) for the mini map and the big map in the game.

Paints the land from the zone and height maps of the kit: zone colours like an old painted map, hill shading,
water by depth with a dark coast line and a light shallow rim, forests as dark crown blobs (the grey box crowns),
the campaign path as a light road and the small trails (WF_Pfade.json) as thin dashed lines. No text: the game
writes the names itself (sharp at any size), from WF_Karte.json.

  T_WF_Karte.png   2048 x 2048, covers the landscape exactly like T_WF_Zonenkarte.png
  WF_Karte.json    world rectangle of the picture and the names with their world positions

    <wf_generator_venv>/python wf_karte.py      (numpy, scipy, pillow)
"""
import json
import math
import os

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
SIZE = 2048
CM_PER_PX = 200.0
HEIGHT_SCALE = 100.0 / 128.0
MAP_ORIGIN = -201600.0  # world cm of the landscape corner (centred landscape, 2016 quads of 200 cm)

ZONE_COLORS = {
    "saum": "6e6448", "moor": "3e6b55", "hain": "3f7f45", "kamm": "3b6670", "grat": "6a6d60", "asche": "7a4a2a",
    "jaw": "4a5f78", "grat_hoch": "9a9c90", "lagune": "2f6f74", "nid": "5a3a7a", "weg": "b9a77a",
}
# Painted colours of the zones: greens lush, ash rust, ridge stone, the Nid violet
PAINT = {
    "saum": (122, 128, 70), "moor": (70, 110, 78), "hain": (58, 112, 52), "kamm": (74, 112, 96), "grat": (128, 124, 108),
    "asche": (138, 92, 58), "jaw": (100, 108, 116), "grat_hoch": (168, 164, 150), "lagune": (66, 112, 104),
    "nid": (104, 70, 130), "weg": (122, 128, 70),
}
SHALLOW = np.array([86, 150, 150], dtype=np.float32)
DEEP = np.array([18, 44, 62], dtype=np.float32)

# Names on the map: (text, size 0 small .. 2 region, kit text it stands at); the leading numbers of the kit are left off
NAMES = {"1 Forstsaum": ("Forstsaum", 1), "2 Pfuhlmoor": ("Pfuhlmoor", 1), "3 Ewiger Hain": ("Ewiger Hain", 2),
         "4 Kammweg": ("Kammweg", 1), "Schwanzgrat": ("Schwanzgrat", 1), "Die Faenge": ("Die Fänge", 1),
         "-> III Sturmzinnen": ("Zu den Sturmzinnen", 0), "Nid-Quelle": ("Nid-Quelle", 0), "Eihoehle": ("Eihöhle", 0),
         "Drachensicht": ("Drachensicht", 0)}


def zone_index():
    rgb = np.asarray(Image.open(os.path.join(HERE, "T_WF_Zonenkarte.png")).convert("RGB"), dtype=np.float32)
    names = list(ZONE_COLORS)
    best = np.full(rgb.shape[:2], 1e12, dtype=np.float32)
    index = np.zeros(rgb.shape[:2], dtype=np.int32)
    for k, name in enumerate(names):
        color = np.array([int(ZONE_COLORS[name][i:i + 2], 16) for i in (0, 2, 4)], dtype=np.float32)
        scale = np.clip((rgb @ color) / float(color @ color), 0.6, 1.3)
        error = ((rgb - scale[..., None] * color) ** 2).sum(-1)
        better = error < best
        best[better] = error[better]
        index[better] = k
    return index, names


def main():
    rng = np.random.default_rng(7)
    data = json.load(open(os.path.join(HERE, "WF_Platzhalter.json"), encoding="utf-8"))
    silhouette = json.load(open(os.path.join(HERE, "WF_Silhouette.json"), encoding="utf-8"))
    n = data["N"]
    scale = SIZE / float(n)

    heights = (np.asarray(Image.open(os.path.join(HERE, "WF_Hoehenkarte_2017.png")), dtype=np.float64) - 32768.0) * HEIGHT_SCALE
    h = np.asarray(Image.fromarray(heights.astype(np.float32)).resize((SIZE, SIZE), Image.BILINEAR), dtype=np.float64)
    zones, names = zone_index()
    zones = np.asarray(Image.fromarray(zones.astype(np.uint8)).resize((SIZE, SIZE), Image.NEAREST))

    # Land: painted zone colours, blurred so the borders run soft, then hill shading from the north-west
    paint = np.zeros((SIZE, SIZE, 3), dtype=np.float32)
    for k, name in enumerate(names):
        paint[zones == k] = PAINT[name]
    for c in range(3):
        paint[..., c] = ndimage.gaussian_filter(paint[..., c], 3.0)
    gy, gx = np.gradient(h, CM_PER_PX / scale)
    shade = np.clip(1.0 + (-gx * 0.7 - gy * 0.7) * 1.6, 0.6, 1.35)
    land = paint * shade[..., None]

    # Water by depth, with a light shallow rim and a dark coast line
    depth = np.clip(-h / 1800.0, 0.0, 1.0)
    water_color = SHALLOW[None, None, :] * (1 - depth[..., None]) + DEEP[None, None, :] * depth[..., None]
    wet = h < 0.0
    image = np.where(wet[..., None], water_color, land)
    coast = wet ^ ndimage.binary_erosion(wet, iterations=2)
    image[coast] = image[coast] * 0.45
    rim = ndimage.binary_dilation(wet, iterations=4) & wet & ~coast
    image[rim] = image[rim] * 0.7 + np.array([200, 225, 215]) * 0.3

    # Forests: every grey box crown a dark green blob, mushroom caps in their colours
    canvas = Image.fromarray(np.clip(image, 0, 255).astype(np.uint8), "RGB")
    draw = ImageDraw.Draw(canvas, "RGBA")
    for prim in data["prims"]:
        name = prim["n"]
        x, y = prim["l"][0] / CM_PER_PX * scale, prim["l"][1] / CM_PER_PX * scale
        r = max(prim["s"][0], prim["s"][1]) * 50 / CM_PER_PX * scale
        if name.endswith("_Krone") or name.startswith("Urstamm_Krone"):
            draw.ellipse((x - r, y - r, x + r, y + r), fill=(28, 66, 30, 150))
            draw.ellipse((x - r * 0.6, y - r * 0.7, x + r * 0.4, y + r * 0.2), fill=(70, 120, 52, 90))
        elif name.endswith("_Hut") and name.startswith(("Riesenpilz", "Feenkreis")):
            draw.ellipse((x - r, y - r, x + r, y + r), fill=(80, 190, 190, 170))
    # Campaign path: a light road with a dark edge
    route = [((px + data["OX"]) * scale, (py + data["OY"]) * scale) for px, py in silhouette["kampagnenweg"]]
    draw.line(route, fill=(60, 44, 28, 200), width=7, joint="curve")
    draw.line(route, fill=(226, 206, 160, 255), width=4, joint="curve")
    # Small trails: thin dashed brown lines
    trails_file = os.path.join(HERE, "WF_Pfade.json")
    if os.path.isfile(trails_file):
        for trail in json.load(open(trails_file, encoding="utf-8"))["pfade"]:
            pts = [(px * scale, py * scale) for px, py in trail["punkte"]]
            run = 0.0
            for a, b in zip(pts, pts[1:]):
                run += math.hypot(b[0] - a[0], b[1] - a[1])
                if int(run / 5.0) % 2 == 0:
                    draw.line((a, b), fill=(176, 140, 92, 230), width=2)

    # A little paper grain
    out = np.asarray(canvas).astype(np.float32)
    grain = ndimage.gaussian_filter(rng.random((SIZE, SIZE)), 1.0)
    out *= (0.94 + 0.12 * grain)[..., None]
    # The outer rim fades to plain deep sea: the game stretches the edge pixels beyond the picture
    edge = np.minimum.outer(np.minimum(np.arange(SIZE), np.arange(SIZE)[::-1]), np.minimum(np.arange(SIZE), np.arange(SIZE)[::-1]))
    fade = np.clip((edge - 4.0) / 60.0, 0.0, 1.0)[..., None]
    sea = np.median(out[h < -900.0], axis=0)
    out = out * fade + sea[None, None, :] * (1.0 - fade)
    Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGB").save(os.path.join(HERE, "T_WF_Karte.png"))

    labels = []
    for text in data["texts"]:
        if text["t"] in NAMES:
            label, size = NAMES[text["t"]]
            labels.append({"name": label, "size": size, "world": [text["l"][0] + MAP_ORIGIN, text["l"][1] + MAP_ORIGIN]})
    # The land of the region with its shallow water and all names, plus a margin: what the big map shows at least.
    # The jaw of the Sturmzinnen at the edge belongs to the next region.
    rows, cols = np.nonzero((h > -80.0) & (zones != names.index("jaw")))
    px_cm = n * CM_PER_PX / SIZE
    xs = list(cols * px_cm + MAP_ORIGIN) + [label["world"][0] for label in labels]
    ys = list(rows * px_cm + MAP_ORIGIN) + [label["world"][1] for label in labels]
    margin = 0.04 * n * CM_PER_PX
    focus_min = [float(min(xs) - margin), float(min(ys) - margin)]
    focus_max = [float(max(xs) + margin), float(max(ys) + margin)]
    with open(os.path.join(HERE, "WF_Karte.json"), "w", encoding="utf-8") as f:
        json.dump({"title": "Wurzelforst", "world_min": [MAP_ORIGIN, MAP_ORIGIN], "world_size": n * CM_PER_PX,
                   "focus_min": focus_min, "focus_max": focus_max, "labels": labels}, f, indent=1, ensure_ascii=False)
    print("map picture written,", len(labels), "names")


if __name__ == "__main__":
    main()
