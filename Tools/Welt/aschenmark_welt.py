"""
Vael – Gelände der Aschenmark (Blender, ohne Oberfläche, nur numpy)
====================================================================

Baut aus dem Zonenprofil (Docs/Karte/aschenmark_zonen.svg, von Boris freigegeben am 2. November 2026) die Höhenkarte und
die Lage aller Orte der Region I für die offene Welt:

- AM_Hoehen.r16       16 Bit, 2017 x 2521 Punkte (X Norden x Y Osten), 1 Punkt = 1 m, Höhe = (Wert - 32768) / 128 m
- T_AM_Wege.png       Wege (R), Kampfflächen (G), Zonen-Tönung (B: 0 Kesselgrund, 1/3 Schlacke, 2/3 Glut, 1 Pass), 1 px = 2 m
- AM_Orte.json        Orte, Lager, Kampfflächen, Wege und Quellen in Weltmetern mit Bodenhöhe
- AM_Vorschau.png     Schummerung zum Ansehen

Koordinaten: Karte in Pixeln des Zonenprofils (1 km = 520 px). Welt: X zeigt nach Norden, Y nach Osten, Ursprung = Ecke
im Südwesten der Landschaft. Die Kamera schaut mit Gierwinkel 45° schräg nach Nordosten.

Aufruf:
  blender -b --factory-startup -P aschenmark_welt.py -- <Zielordner>
"""
import json
import math
import os
import sys

import bpy
import numpy as np

M_PER_PX = 1000.0 / 520.0
MAP_X0, MAP_Y1 = 150.0, 1108.0       # Kartenpixel der Südwestecke der Landschaft
NX, NY = 2017, 2521                  # Punkte nach Norden (X) und Osten (Y)
SEA = -12.0
rng = np.random.default_rng(1102)


def world(px, py):
    """Kartenpixel → Weltmeter (X Norden, Y Osten)"""
    return ((MAP_Y1 - py) * M_PER_PX, (px - MAP_X0) * M_PER_PX)


# ---------------------------------------------------------------- Inhalt des Zonenprofils (Kartenpixel)

MAIN_ROAD = [(690, 820), (700, 760), (715, 700), (730, 620), (680, 580), (600, 545), (540, 500), (530, 450), (560, 390),
             (610, 330), (632, 270), (640, 200), (648, 120), (652, 50)]
SIDE_ROADS = {
    "Schlackenweg": [(730, 620), (820, 612), (920, 602), (1010, 620), (1100, 640), (1170, 600), (1230, 560), (1290, 525), (1340, 520)],
    "Kraterpfad": [(540, 500), (500, 450), (470, 410), (440, 385)],
    "Teichpfad": [(690, 820), (620, 850), (560, 880)],
    "Dorfpfad": [(690, 820), (740, 790), (790, 760)],
    "Ackerpfad": [(690, 820), (760, 860), (820, 900)],
    "Kathedralenpfad": [(700, 760), (630, 720), (560, 690)],
}
CLEARINGS = [(610, 760), (820, 900), (560, 900), (500, 560), (340, 560), (380, 460), (560, 430), (1010, 690), (1150, 700), (1210, 610),
             (960, 560), (700, 300), (580, 240)]
CAMPS = {"Lager der Überlebenden": (690, 815), "Gildenkontor": (730, 617), "Passwache": (600, 297)}
PLACES = {
    "Ruinen von Kesselgrund": (790, 760), "Teich": (535, 880), "Ascheäcker": (840, 900), "Kathedrale des Ordens": (560, 690),
    "Nid-Quelle": (545, 705), "Gildenspeicher": (1110, 560), "Obsidianbruch": (1180, 680), "Schlackenhalden": (1010, 690),
    "Schlackentor": (1340, 520), "Harpyienfelsen": (380, 460), "Glutkriecher-Nester": (500, 560), "Krater der Glutkönigin": (420, 360),
    "Predigerlager": (700, 300), "Windkante": (740, 250), "Forstsaum": (652, 60),
}
SOURCES = {
    "Feuer": [(460, 520), (350, 510), (430, 300), (1060, 570), (690, 800)],
    "Wasser": [(535, 880), (790, 760), (905, 840)],
    "Erde": [(1180, 680), (1110, 560), (620, 280), (300, 420)],
    "Luft": [(680, 210), (630, 160), (740, 250)],
}
# Orte, an die PortAschenmarkToWelt.py die Gruppen der alten Karte setzt: eben, Halbmesser der Gruppe + Rand
PADS = [(CAMPS["Lager der Überlebenden"], 24.0), (PLACES["Ruinen von Kesselgrund"], 36.0), (PLACES["Gildenspeicher"], 18.0),
        (CAMPS["Gildenkontor"], 28.0), (PLACES["Teich"], 24.0), (PLACES["Kathedrale des Ordens"], 20.0), (PLACES["Nid-Quelle"], 18.0),
        (PLACES["Krater der Glutkönigin"], 30.0), (PLACES["Ascheäcker"], 20.0)]
# Küste im Westen und Süden (Meer); Norden und Osten schließen Berge ab
COAST = [(150, 1110), (150, 1060), (160, 900), (230, 640), (300, 420), (360, 300), (470, 170), (640, 80), (640, 0), (1600, 0),
         (1600, 1110)]
COAST_SOUTH = [(150, 1060), (400, 1072), (700, 1075), (1000, 1050), (1260, 960), (1400, 880), (1490, 620), (1600, 600)]


# ---------------------------------------------------------------- Raster

xs = np.arange(NX, dtype=np.float32)          # Norden in m
ys = np.arange(NY, dtype=np.float32)          # Osten in m
X, Y = np.meshgrid(xs, ys, indexing="ij")


def box_blur(a, r):
    """Kastenunschärfe mit Summen, zweimal: fast ein Gauß"""
    if r < 1:
        return a
    for _ in range(2):
        for axis in (0, 1):
            pad = [(0, 0), (0, 0)]
            pad[axis] = (r + 1, r)
            c = np.cumsum(np.pad(a, pad, mode="edge"), axis=axis, dtype=np.float64)
            n = a.shape[axis]
            hi = np.take(c, np.arange(2 * r + 1, 2 * r + 1 + n), axis=axis)
            lo = np.take(c, np.arange(0, n), axis=axis)
            a = ((hi - lo) / (2 * r + 1)).astype(np.float32)
    return a


def noise(cell, octaves=4, seed=0):
    """Wertrauschen um 0 (etwa -1..1) mit Strukturgröße cell in m"""
    g = np.random.default_rng(seed)
    total = np.zeros((NX, NY), np.float32)
    amp, norm = 1.0, 0.0
    for o in range(octaves):
        c = max(int(cell / (2 ** o)), 2)
        gx, gy = NX // c + 2, NY // c + 2
        grid = g.uniform(-1, 1, (gx, gy)).astype(np.float32)
        fx, fy = xs / c, ys / c
        ix, iy = fx.astype(int), fy.astype(int)
        tx, ty = (fx - ix)[:, None], (fy - iy)[None, :]
        tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
        a = grid[ix][:, iy]
        b = grid[ix + 1][:, iy]
        cc = grid[ix][:, iy + 1]
        d = grid[ix + 1][:, iy + 1]
        total += amp * ((a * (1 - tx) + b * tx) * (1 - ty) + (cc * (1 - tx) + d * tx) * ty)
        norm += amp
        amp *= 0.5
    return total / norm


def bump(px, py, radius, power=2.0):
    """1 in der Mitte, 0 ab radius (m), weich"""
    wx, wy = world(px, py)
    t = np.clip(1.0 - np.hypot(X - wx, Y - wy) / radius, 0, 1)
    return t ** power * (3 - 2 * t) if power == 2.0 else t ** power


def smooth(t):
    t = np.clip(t, 0, 1)
    return t * t * (3 - 2 * t)


def polyline_distance(points_px):
    """Abstand zu einer Linie aus Kartenpixeln, in m; auch: Anteil entlang (0..1)"""
    pts = [world(px, py) for px, py in points_px]
    d = np.full((NX, NY), 1e6, np.float32)
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        pad = 120.0
        x0, x1 = int(max(min(ax, bx) - pad, 0)), int(min(max(ax, bx) + pad, NX - 1)) + 1
        y0, y1 = int(max(min(ay, by) - pad, 0)), int(min(max(ay, by) + pad, NY - 1)) + 1
        if x0 >= x1 or y0 >= y1:
            continue
        sx, sy = X[x0:x1, y0:y1], Y[x0:x1, y0:y1]
        dx, dy = bx - ax, by - ay
        t = np.clip(((sx - ax) * dx + (sy - ay) * dy) / (dx * dx + dy * dy + 1e-9), 0, 1)
        np.minimum(d[x0:x1, y0:y1], np.hypot(sx - ax - dx * t, sy - ay - dy * t), out=d[x0:x1, y0:y1])
    return d


def inside(poly_px):
    """Gerade-Ungerade-Test für ein Vieleck aus Kartenpixeln"""
    pts = [world(px, py) for px, py in poly_px]
    result = np.zeros((NX, NY), bool)
    n = len(pts)
    for i in range(n):
        (ax, ay), (bx, by) = pts[i], pts[(i + 1) % n]
        if ay == by:
            continue
        cond = ((ay > Y) != (by > Y)) & (X < (bx - ax) * (Y - ay) / (by - ay) + ax)
        result ^= cond
    return result


def main(folder):
    os.makedirs(folder, exist_ok=True)

    # Land: Meer im Westen und Süden, Berge im Norden und Osten
    land = inside(COAST)
    land &= ~inside(COAST_SOUTH + [(1600, 1110), (150, 1110)])
    land_f = box_blur(land.astype(np.float32), 18)
    coast = smooth((land_f - 0.35) / 0.3)

    h = 18.0 + noise(320, 4, 1) * 7.0 + noise(60, 3, 2) * 1.5

    # 1 Kesselgrund: heißes Becken mit Rand
    h -= 13.0 * bump(700, 820, 300)
    h += 10.0 * np.clip(bump(700, 820, 380, 1.0) - bump(700, 820, 290, 1.0), 0, 1) * 3.0 * (0.6 + 0.4 * noise(80, 2, 3))
    # 2 Schlackenhänge: steigt nach Osten, mit Halden
    e = smooth((Y - world(0, 900)[1]) / 900.0) * smooth((X - world(0, 900)[0]) / 400.0 + 0.6)
    h += 48.0 * e
    for _ in range(14):
        px, py = rng.uniform(950, 1300), rng.uniform(560, 760)
        h += rng.uniform(6.0, 14.0) * bump(px, py, rng.uniform(25.0, 45.0))
    # 3 Glutfelder: Hochfläche mit dem Krater der Glutkönigin
    h += 12.0 * bump(430, 490, 380)
    cx, cy = world(420, 360)
    r = np.hypot(X - cx, Y - cy)
    h += 28.0 * np.exp(-((r - 75.0) / 22.0) ** 2) * (0.85 + 0.15 * noise(40, 2, 4))
    h -= 14.0 * smooth(1.0 - r / 70.0)
    # 4 Aschenpass: Bergkamm im Norden, der Weg schneidet eine enge Schlucht hinein
    north = smooth((X - world(0, 380)[0]) / 260.0)
    ridge = 95.0 * north * (0.75 + 0.25 * noise(150, 3, 5))
    road = polyline_distance(MAIN_ROAD)
    pass_width = np.where(X > world(0, 330)[0], 22.0, 60.0)
    gorge = smooth((road - pass_width) / 70.0)
    h += ridge * gorge
    # Berge am Ostrand (zum Eisenschlund) und im Nordosten
    east = smooth((Y - (NY - 380.0)) / 300.0) + smooth((X - (NX - 300.0)) / 250.0) * smooth((Y - 1300.0) / 400.0)
    h += 85.0 * np.clip(east, 0, 1) * (0.8 + 0.2 * noise(120, 2, 6))

    # Küste: zum Meer hin abfallen
    h = h * coast + SEA * (1.0 - coast)

    # Wege einebnen: der Boden folgt dem weich gezeichneten Gelände, 8 m breit, 18 m Übergang
    soft = box_blur(h, 35)
    weight = smooth(1.0 - (road - 5.0) / 18.0)
    for name, points in SIDE_ROADS.items():
        dist = polyline_distance(points)
        weight = np.maximum(weight, smooth(1.0 - (dist - 3.0) / 14.0) * 0.9)
    h = h * (1.0 - weight) + soft * weight

    # Kampfflächen: 40 m eben
    flat_mask = np.zeros_like(h)
    for px, py in CLEARINGS:
        wx, wy = world(px, py)
        dist = np.hypot(X - wx, Y - wy)
        level = float(soft[int(np.clip(wx, 0, NX - 1)), int(np.clip(wy, 0, NY - 1))])
        k = smooth(1.0 - (dist - 22.0) / 14.0)
        h = h * (1.0 - k) + level * k
        flat_mask = np.maximum(flat_mask, smooth(1.0 - (dist - 20.0) / 3.0))
    # Lager eben
    for px, py in CAMPS.values():
        wx, wy = world(px, py)
        dist = np.hypot(X - wx, Y - wy)
        level = float(soft[int(wx), int(wy)])
        k = smooth(1.0 - (dist - 18.0) / 12.0)
        h = h * (1.0 - k) + level * k

    # Ebene Flächen für die Orte aus L_Aschenmark (PortAschenmarkToWelt.py setzt sie dorthin): Halbmesser in m
    for (px, py), radius in PADS:
        wx, wy = world(px, py)
        dist = np.hypot(X - wx, Y - wy)
        level = float(h[int(wx), int(wy)]) * 0.5 + float(soft[int(wx), int(wy)]) * 0.5
        k = smooth(1.0 - (dist - radius) / 16.0)
        h = h * (1.0 - k) + level * k

    # Höhenkarte schreiben
    raw = np.clip(32768.0 + h.T * 128.0, 0, 65535).astype("<u2")    # X (Norden) läuft am schnellsten
    raw.tofile(os.path.join(folder, "AM_Hoehen.r16"))

    def ground(px, py):
        wx, wy = world(px, py)
        return float(h[int(np.clip(round(wx), 0, NX - 1)), int(np.clip(round(wy), 0, NY - 1))])

    def place(px, py):
        wx, wy = world(px, py)
        return {"x": round(wx, 1), "y": round(wy, 1), "z": round(ground(px, py), 2)}

    def line(points):
        out = []
        for (ax, ay), (bx, by) in zip(points, points[1:]):
            steps = max(int(math.hypot(bx - ax, by - ay) * M_PER_PX / 20.0), 1)
            for i in range(steps):
                t = i / steps
                out.append(place(ax + (bx - ax) * t, ay + (by - ay) * t))
        out.append(place(*points[-1]))
        return out

    data = {
        "info": "Weltmeter: X Norden, Y Osten, Z Bodenhöhe; Landschaft beginnt bei 0/0, 1 Punkt = 1 m",
        "size_m": [NX - 1, NY - 1],
        "zones": {"Kesselgrund": place(690, 860), "Schlackenhänge": place(1130, 640), "Glutfelder": place(400, 470), "Aschenpass": place(640, 230)},
        "camps": {k: place(*p) for k, p in CAMPS.items()},
        "places": {k: place(*p) for k, p in PLACES.items()},
        "clearings": [place(*p) for p in CLEARINGS],
        "sources": {k: [place(*p) for p in ps] for k, ps in SOURCES.items()},
        "main_road": line(MAIN_ROAD),
        "side_roads": {k: line(p) for k, p in SIDE_ROADS.items()},
        "coast": [line(COAST[1:8]), line(COAST_SOUTH)],
    }
    with open(os.path.join(folder, "AM_Orte.json"), "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)

    # Wege-, Kampfflächen- und Zonenmaske für das Bodenmaterial (halbe Auflösung)
    roads = np.maximum(smooth(1.0 - (road - 3.5) / 2.5), 0)
    for points in SIDE_ROADS.values():
        roads = np.maximum(roads, smooth(1.0 - (polyline_distance(points) - 2.0) / 2.0) * 0.85)
    zone = np.zeros_like(h)
    weights = []
    for value, (px, py, rad) in ((0.0, (700, 820, 420)), (1 / 3, (1120, 640, 520)), (2 / 3, (420, 470, 450)), (1.0, (640, 240, 380))):
        wx, wy = world(px, py)
        weights.append((value, np.clip(1.0 - np.hypot(X - wx, Y - wy) / rad, 0.001, 1) ** 2))
    total = sum(w for _, w in weights)
    zone = sum(v * w for v, w in weights) / total
    mask = np.dstack([roads, flat_mask, zone, np.ones_like(h)])[::2, ::2]
    image = bpy.data.images.new("T_AM_Wege", mask.shape[1], mask.shape[0], alpha=True)
    image.pixels.foreach_set(np.ascontiguousarray(mask[::-1]).astype(np.float32).ravel())
    image.filepath_raw = os.path.join(folder, "T_AM_Wege.png")
    image.file_format = "PNG"
    image.save()

    # Schummerung zum Ansehen (Norden oben)
    gx, gy = np.gradient(h)
    shade = np.clip(0.55 + (gx * 0.6 - gy * 0.6) * 0.25, 0, 1)
    tint = np.dstack([0.45 + 0.4 * zone, 0.35 + 0.1 * (1 - zone), 0.3 * np.ones_like(h)])
    water = (h < 0)[..., None]
    rgb = np.where(water, np.array([0.12, 0.18, 0.24]), tint * shade[..., None])
    rgb = np.where((roads > 0.5)[..., None], np.array([0.85, 0.78, 0.6]), rgb)
    preview = np.flipud(np.dstack([rgb, np.ones_like(h)])[::3, ::3])
    preview = preview[::-1]
    image = bpy.data.images.new("AM_Vorschau", preview.shape[1], preview.shape[0], alpha=True)
    image.pixels.foreach_set(np.ascontiguousarray(preview[::-1][::-1]).astype(np.float32).ravel())
    image.filepath_raw = os.path.join(folder, "AM_Vorschau.png")
    image.file_format = "PNG"
    image.save()
    print("[Vael] Aschenmark: Höhe {:.1f} bis {:.1f} m, geschrieben nach {}".format(float(h.min()), float(h.max()), folder))


if __name__ == "__main__":
    main(sys.argv[sys.argv.index("--") + 1])
