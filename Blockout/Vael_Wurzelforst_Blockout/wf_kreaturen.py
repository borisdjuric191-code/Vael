"""Where the creatures of the Wurzelforst (region II) live: packs of Rankenluchs, Spannhornkaefer and Schleieraffen.

  Rankenluchs     packs of three in ambush in the thickets (T_WF_Dickicht bright) of the Hain and the Forstsaum,
                  8 to 25 m beside a small trail, so walkers pass them
  Spannhornkaefer pairs in the Hain and on the Kammweg
  Schleieraffe    alone, in the forest of the Forstsaum and the Hain
  Tauraupe        the healed light form of the caterpillar, groups of three in the Hain and the Forstsaum
  Draculaupe      groups of three on dry ground of the Pfuhlmoor, near the Nid spring
All on dry ground, apart from each other, away from the start, the campaign path and the Urbaum. Writes WF_Kreaturen.json
(world cm and the ground height) for Scripts/PlaceWurzelforstCreatures.py, and a preview Saved/WF_Kreaturen_Vorschau.png.

    <wf_generator_venv>/python wf_kreaturen.py      (numpy, scipy, pillow; same seed = same places)
"""
import json
import math
import os

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
SEED = 77
CM_PER_PX = 200.0
HEIGHT_SCALE = 100.0 / 128.0
MAP_ORIGIN = -201600.0
ZONE_COLORS = {
    "saum": "6e6448", "moor": "3e6b55", "hain": "3f7f45", "kamm": "3b6670", "grat": "6a6d60", "asche": "7a4a2a",
    "jaw": "4a5f78", "grat_hoch": "9a9c90", "lagune": "2f6f74", "nid": "5a3a7a", "weg": "b9a77a",
}
# kind: (zones, how many groups, creatures per group, spread cm, min metres between groups, colour in the preview)
ARTEN = {
    # Boris 2026-10-22: ten times as many as at first (players had to look for fights); the healed Tauraupe stays rare
    "Rankenluchs": (("hain", "saum"), 90, 3, 260.0, 38.0, (230, 120, 40)),
    "Spannhornkaefer": (("hain", "kamm"), 70, 2, 220.0, 32.0, (60, 160, 230)),
    "Schleieraffe": (("saum", "hain"), 70, 1, 0.0, 30.0, (200, 60, 200)),
    "Tauraupe": (("hain", "saum"), 8, 3, 200.0, 60.0, (90, 220, 200)),
    "Draculaupe": (("moor",), 40, 3, 200.0, 34.0, (200, 20, 40)),
}


def zone_map(shape):
    rgb = np.asarray(Image.open(os.path.join(HERE, "T_WF_Zonenkarte.png")).convert("RGB").resize(shape[::-1], Image.NEAREST), dtype=np.float32)
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
    rng = np.random.default_rng(SEED)
    data = json.load(open(os.path.join(HERE, "WF_Platzhalter.json"), encoding="utf-8"))
    silhouette = json.load(open(os.path.join(HERE, "WF_Silhouette.json"), encoding="utf-8"))
    ox, oy = data["OX"], data["OY"]
    heights = (np.asarray(Image.open(os.path.join(HERE, "WF_Hoehenkarte_2017.png")), dtype=np.float64) - 32768.0) * HEIGHT_SCALE
    n = heights.shape[0]
    zones, names = zone_map(heights.shape)
    thicket = np.asarray(Image.open(os.path.join(HERE, "T_WF_Dickicht.png")).convert("L").resize((n, n), Image.BILINEAR), dtype=np.float32) / 255.0
    trail_dist = np.asarray(Image.open(os.path.join(HERE, "T_WF_Pfade.png")).convert("L").resize((n, n), Image.BILINEAR), dtype=np.float32) / 255.0 * 10.0

    # Slope and dry ground
    gy, gx = np.gradient(heights, CM_PER_PX)
    flat = np.hypot(gx, gy) < 0.45
    dry = heights > 40.0

    # Away from the start, the campaign path and the Urbaum
    ys, xs = np.mgrid[0:n, 0:n].astype(np.float32)
    start = data["spawn"]["l"]
    sx, sy = start[0] / CM_PER_PX, start[1] / CM_PER_PX
    away = np.hypot(xs - sx, ys - sy) * 2.0 > 140.0
    urstamm = [p for p in data["prims"] if p["n"].startswith("Urstamm")]
    if urstamm:
        ux = sum(p["l"][0] for p in urstamm) / len(urstamm) / CM_PER_PX
        uy = sum(p["l"][1] for p in urstamm) / len(urstamm) / CM_PER_PX
        away &= np.hypot(xs - ux, ys - uy) * 2.0 > 70.0
    road = np.zeros((n, n), bool)
    pts = [(px + ox, py + oy) for px, py in silhouette["kampagnenweg"]]
    img = Image.new("L", (n, n), 0)
    ImageDraw.Draw(img).line(pts, fill=255, width=1)
    road_dist = ndimage.distance_transform_edt(np.asarray(img) == 0) * 2.0
    away &= road_dist > 18.0
    # Not inside grey box things (trunks, rocks, huts)
    blocked = np.zeros((n, n), bool)
    for p in data["prims"]:
        if any(k in p["n"] for k in ("_Stamm", "Felsbrocken", "Felsturm_", "Huette", "_Stiel", "Eihoehle_Fels")):
            bx, by = int(p["l"][0] / CM_PER_PX), int(p["l"][1] / CM_PER_PX)
            r = int(max(p["s"][0], p["s"][1]) * 0.5 + 4)
            blocked[max(by - r, 0):by + r, max(bx - r, 0):bx + r] = True
    base_ok = flat & dry & away & ~blocked

    out = {"_hinweis": "Kreaturen des Wurzelforsts, Gruppen in Welt-cm (x, y, Bodenhoehe). Erzeugt von wf_kreaturen.py", "gruppen": []}
    taken = []
    for kind, (kind_zones, groups, count, spread, spacing, _) in ARTEN.items():
        in_zone = np.isin(zones, [names.index(z) for z in kind_zones])
        ok = base_ok & in_zone
        score = rng.random((n, n)).astype(np.float32)
        if kind == "Rankenluchs":
            ok &= (thicket > 0.35) & (trail_dist > 4.0) & (trail_dist < 9.5)
            score += thicket
        elif kind == "Schleieraffe":
            ok &= thicket > 0.2
        elif kind == "Draculaupe":
            nid = next(p for p in data["prims"] if p["n"] == "NidQuelle_Becken")
            nid_m = np.hypot(xs - nid["l"][0] / CM_PER_PX, ys - nid["l"][1] / CM_PER_PX) * 2.0
            ok &= nid_m > 40.0
            score -= nid_m / 4000.0
        # Preferred spots first (thickets beside trails and so on), then anywhere in the kind's zones
        loose = base_ok & in_zone & ~ok
        candidates = np.concatenate([np.argwhere(ok), np.argwhere(loose)])
        order = np.concatenate([np.argsort(-score[ok]), len(np.argwhere(ok)) + np.argsort(-score[loose])])
        placed = 0
        for idx in order:
            y, x = candidates[idx]
            if any(math.hypot(x - tx, y - ty) * 2.0 < max(spacing, ts) for tx, ty, ts in taken):
                continue
            taken.append((x, y, spacing * 0.6))
            out["gruppen"].append({"art": kind, "anzahl": count, "streuung_cm": spread,
                                   "ort": [round(x * CM_PER_PX + MAP_ORIGIN, 1), round(y * CM_PER_PX + MAP_ORIGIN, 1), round(float(heights[y, x]), 1)]})
            placed += 1
            if placed >= groups:
                break
        print("%s: %d Gruppen" % (kind, placed))

    with open(os.path.join(HERE, "WF_Kreaturen.json"), "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1, ensure_ascii=False)

    karte = Image.open(os.path.join(HERE, "T_WF_Karte.png")).convert("RGB") if os.path.isfile(os.path.join(HERE, "T_WF_Karte.png")) else Image.new("RGB", (2048, 2048), (60, 80, 60))
    draw = ImageDraw.Draw(karte)
    f = karte.size[0] / n
    for g in out["gruppen"]:
        x = (g["ort"][0] - MAP_ORIGIN) / CM_PER_PX * f
        y = (g["ort"][1] - MAP_ORIGIN) / CM_PER_PX * f
        c = ARTEN[g["art"]][5]
        draw.ellipse((x - 9, y - 9, x + 9, y + 9), fill=c, outline=(0, 0, 0))
    karte.save(os.path.join(HERE, "..", "..", "Saved", "WF_Kreaturen_Vorschau.png"))


if __name__ == "__main__":
    main()
