"""
Vael – HUD-Formen (Blender, ohne Oberfläche)
============================================

Zeichnet alle Formen des Spieler-HUDs aus reiner Geometrie (vorzeichenbehaftete Abstände) als kleine PNG-Masken:
Gefäße, Drachenklaue, Ecken-, Mittel- und Endornamente pro Klasse, Nid-Adern, Drachenkopf und Platzhalter-Symbole.
Keine Zeichnungen, keine Fremdgrafiken: jede Form ist hier in ein paar Zeilen beschrieben und später durch ein
gezeichnetes Bild gleichen Namens ersetzbar.

Gefäße (RGBA): R = Innenraum, in dem die Flüssigkeit steht; G = Glas, Metall, Knochen und Zierrat; B = Risse; A = alles.
Ornamente, Symbole, Klaue (RGBA): weiß mit Alpha = Form, das Spiel färbt sie ein.
Adern (RGBA): R = Aderlinien, G = Weg ab der Ecke (0 an der Ecke, 1 am Ende), damit sie mit der Verderbnis kriechen.

Aufruf:
  blender -b -P vael_hud_formen.py -- <Zielordner>
"""
import math
import os
import sys

import bpy
import numpy as np

SIZE = 256
ICON = 128


# ---------------------------------------------------------------- Abstände (Koordinaten 0..1, y nach unten)

def grid(w, h):
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    return (x + 0.5) / w, (y + 0.5) / h


def circle(x, y, cx, cy, r):
    return np.hypot(x - cx, y - cy) - r


def ellipse(x, y, cx, cy, rx, ry):
    # Näherung, genau genug für weiche Kanten
    k = np.hypot((x - cx) / rx, (y - cy) / ry)
    return (k - 1.0) * min(rx, ry)


def box(x, y, x0, y0, x1, y1, r=0.0):
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    hx, hy = (x1 - x0) / 2 - r, (y1 - y0) / 2 - r
    qx, qy = np.abs(x - cx) - hx, np.abs(y - cy) - hy
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r


def segment(x, y, ax, ay, bx, by, r):
    px, py = x - ax, y - ay
    dx, dy = bx - ax, by - ay
    t = np.clip((px * dx + py * dy) / (dx * dx + dy * dy + 1e-9), 0, 1)
    return np.hypot(px - dx * t, py - dy * t) - r


def polyline(x, y, points, r):
    d = np.full(x.shape, 1e3, np.float32)
    for (ax, ay), (bx, by) in zip(points, points[1:]):
        d = np.minimum(d, segment(x, y, ax, ay, bx, by, r))
    return d


def polygon(x, y, points):
    """Abstand zu einem einfachen Vieleck, innen negativ"""
    d = np.full(x.shape, 1e3, np.float32)
    inside = np.zeros(x.shape, bool)
    n = len(points)
    for i in range(n):
        ax, ay = points[i]
        bx, by = points[(i + 1) % n]
        d = np.minimum(d, segment(x, y, ax, ay, bx, by, 0.0))
        cond = ((ay > y) != (by > y)) & (x < (bx - ax) * (y - ay) / (by - ay + 1e-9) + ax)
        inside ^= cond
    return np.where(inside, -d, d)


def ring(d, width):
    return np.abs(d) - width


def union(*ds):
    out = ds[0]
    for d in ds[1:]:
        out = np.minimum(out, d)
    return out


def cut(a, b):
    return np.maximum(a, -b)


def fill(d, w):
    """Weiche Kante über etwa anderthalb Pixel"""
    return np.clip(0.5 - d * w * 0.75, 0.0, 1.0)


def bezier(p0, p1, p2, n=24):
    return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0],
             (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]) for t in np.linspace(0, 1, n)]


# ---------------------------------------------------------------- Speichern

def save(path, rgba):
    h, w = rgba.shape[:2]
    image = bpy.data.images.new(os.path.basename(path), w, h, alpha=True)
    image.pixels.foreach_set(np.ascontiguousarray(rgba[::-1]).astype(np.float32).ravel())
    image.filepath_raw = path
    image.file_format = "PNG"
    image.save()
    bpy.data.images.remove(image)


def save_shape(path, d, w):
    a = fill(d, w)
    save(path, np.dstack([np.ones_like(a), np.ones_like(a), np.ones_like(a), a]))


def cracks(x, y, seed, inner):
    rng = np.random.default_rng(seed)
    d = np.full(x.shape, 1e3, np.float32)
    for _ in range(5):
        px, py = rng.uniform(0.3, 0.7), rng.uniform(0.45, 0.85)
        points = [(px, py)]
        for _ in range(4):
            px += rng.uniform(-0.09, 0.09)
            py += rng.uniform(-0.1, 0.04)
            points.append((px, py))
        d = np.minimum(d, polyline(x, y, points, 0.004))
    return np.where(inner < 0, d, 1e3)


def save_vessel(folder, name, inner, glass, seed):
    x, y = grid(SIZE, SIZE)
    r = fill(inner, SIZE)
    g = fill(glass, SIZE)
    b = fill(cracks(x, y, seed, inner), SIZE)
    a = np.maximum(r, g)
    save(os.path.join(folder, name + ".png"), np.dstack([r, g, b, a]))


# ---------------------------------------------------------------- Gefäße

def vessels(folder):
    x, y = grid(SIZE, SIZE)

    # Leben: runde Phiole mit Hals, Lippe und Korken
    body = circle(x, y, 0.5, 0.62, 0.3)
    neck = box(x, y, 0.42, 0.2, 0.58, 0.4, 0.02)
    outer = union(body, neck)
    inner = union(circle(x, y, 0.5, 0.62, 0.265), box(x, y, 0.45, 0.24, 0.55, 0.42, 0.01))
    lip = box(x, y, 0.38, 0.15, 0.62, 0.22, 0.025)
    cork = box(x, y, 0.43, 0.06, 0.57, 0.16, 0.02)
    save_vessel(folder, "T_HUD_Gefaess_Leben", inner, union(ring(outer, 0.016), lip, cork), 1)

    # Magier: Tintenfass mit Feder
    outer = union(polygon(x, y, [(0.18, 0.9), (0.82, 0.9), (0.72, 0.5), (0.28, 0.5)]), box(x, y, 0.38, 0.38, 0.62, 0.52, 0.02))
    inner = union(polygon(x, y, [(0.23, 0.86), (0.77, 0.86), (0.69, 0.54), (0.31, 0.54)]), box(x, y, 0.42, 0.42, 0.58, 0.56, 0.01))
    lip = box(x, y, 0.34, 0.33, 0.66, 0.39, 0.02)
    shaft = segment(x, y, 0.52, 0.5, 0.86, 0.04, 0.008)
    vane = cut(ellipse(x, y, 0.74, 0.2, 0.06, 0.17), circle(x, y, 0.62, 0.36, 0.08))
    vane = np.maximum(vane, -(np.abs(segment(x, y, 0.52, 0.5, 0.86, 0.04, 0.0)) - 0.004))
    save_vessel(folder, "T_HUD_Gefaess_Magier", inner, union(ring(outer, 0.016), lip, shaft, vane), 2)

    # Tamer: Knochenhorn mit Knochenringen, Öffnung oben
    path = bezier((0.3, 0.92), (0.15, 0.55), (0.55, 0.22), 30)
    horn_outer = np.full(x.shape, 1e3, np.float32)
    horn_inner = np.full(x.shape, 1e3, np.float32)
    for i, (px, py) in enumerate(path):
        t = i / (len(path) - 1)
        horn_outer = np.minimum(horn_outer, circle(x, y, px, py, 0.04 + 0.15 * t))
        horn_inner = np.minimum(horn_inner, circle(x, y, px, py, 0.02 + 0.13 * t))
    rings = np.full(x.shape, 1e3, np.float32)
    for t in (0.35, 0.6, 0.85):
        i = int(t * (len(path) - 1))
        (ax, ay), (bx, by) = path[i - 1], path[i + 1]
        nx, ny = -(by - ay), bx - ax
        k = math.hypot(nx, ny)
        nx, ny = nx / k, ny / k
        r = 0.04 + 0.16 * t
        rings = np.minimum(rings, segment(x, y, path[i][0] - nx * r, path[i][1] - ny * r, path[i][0] + nx * r, path[i][1] + ny * r, 0.018))
    save_vessel(folder, "T_HUD_Gefaess_Tamer", horn_inner, union(ring(horn_outer, 0.014), np.maximum(rings, horn_outer - 0.02)), 3)

    # Ingenieur: Messing-Kartusche mit Spitze, Bändern und Boden
    outer = union(box(x, y, 0.32, 0.3, 0.68, 0.9, 0.04), ellipse(x, y, 0.5, 0.3, 0.18, 0.16))
    inner = union(box(x, y, 0.36, 0.33, 0.64, 0.84, 0.02), ellipse(x, y, 0.5, 0.31, 0.14, 0.12))
    bands = union(box(x, y, 0.3, 0.84, 0.7, 0.93, 0.02), box(x, y, 0.31, 0.5, 0.69, 0.54, 0.01))
    save_vessel(folder, "T_HUD_Gefaess_Ingenieur", inner, union(ring(outer, 0.016), bands), 4)

    # Prophet: Stundenglas, zwei Kammern, Rahmen mit Pfosten
    top = polygon(x, y, [(0.27, 0.16), (0.73, 0.16), (0.53, 0.5), (0.47, 0.5)])
    bottom = polygon(x, y, [(0.47, 0.5), (0.53, 0.5), (0.73, 0.84), (0.27, 0.84)])
    outer = union(top, bottom)
    inner = union(top + 0.02, bottom + 0.02)
    frame = union(box(x, y, 0.18, 0.08, 0.82, 0.15, 0.02), box(x, y, 0.18, 0.85, 0.82, 0.92, 0.02),
                  box(x, y, 0.2, 0.12, 0.24, 0.88, 0.01), box(x, y, 0.76, 0.12, 0.8, 0.88, 0.01))
    save_vessel(folder, "T_HUD_Gefaess_Prophet", inner, union(ring(outer, 0.012), frame), 5)

    # Floromant: Kannenkelch wie eine Kannenpflanze, Rand und Blattdeckel
    outer = union(ellipse(x, y, 0.5, 0.64, 0.23, 0.27), box(x, y, 0.38, 0.3, 0.62, 0.5, 0.06))
    inner = union(ellipse(x, y, 0.5, 0.64, 0.2, 0.24), box(x, y, 0.41, 0.33, 0.59, 0.52, 0.05))
    rim = ring(ellipse(x, y, 0.5, 0.31, 0.15, 0.045), 0.012)
    lid = polygon(x, y, [(0.47, 0.27), (0.62, 0.05), (0.86, 0.08), (0.7, 0.25)])
    vein = segment(x, y, 0.5, 0.27, 0.78, 0.09, 0.005)
    stem = polyline(x, y, bezier((0.5, 0.9), (0.47, 0.96), (0.36, 0.98), 10), 0.012)
    save_vessel(folder, "T_HUD_Gefaess_Floromant", inner, union(ring(outer, 0.014), rim, cut(lid, vein + 0.002), stem), 6)


# ---------------------------------------------------------------- Drachenklaue (hält das Gefäß von unten)

def claw(folder):
    x, y = grid(SIZE, SIZE)
    # Handballen unter dem Gefäß, drei Krallen greifen links, rechts und vorn hoch, eine Afterkralle
    palm = ellipse(x, y, 0.5, 0.93, 0.3, 0.09)
    wrist = box(x, y, 0.4, 0.92, 0.6, 1.02, 0.03)
    d = union(palm, wrist)
    for start, mid, tip, r in (((0.24, 0.9), (0.08, 0.72), (0.16, 0.5), 0.045),
                               ((0.76, 0.9), (0.92, 0.72), (0.84, 0.5), 0.045),
                               ((0.4, 0.95), (0.3, 0.86), (0.33, 0.74), 0.035),
                               ((0.6, 0.95), (0.7, 0.86), (0.67, 0.74), 0.035)):
        points = bezier(start, mid, tip, 20)
        for i, (px, py) in enumerate(points):
            t = i / (len(points) - 1)
            d = np.minimum(d, circle(x, y, px, py, r * (1.0 - 0.8 * t)))
    # Schuppenkerben auf dem Ballen
    scales = union(*[ring(circle(x, y, 0.36 + 0.07 * i, 0.94, 0.035), 0.004) for i in range(5)])
    save_shape(os.path.join(folder, "T_HUD_Klaue.png"), cut(d, scales), SIZE)
    # Schatten-Variante: Dornen an den Klauen
    thorns = union(*[polygon(x, y, [(px - 0.02, py), (px + 0.02, py), (px + dx, py - 0.06)]) for px, py, dx in
                     ((0.12, 0.68, -0.03), (0.88, 0.68, 0.03), (0.2, 0.8, -0.04), (0.8, 0.8, 0.04))])
    save_shape(os.path.join(folder, "T_HUD_Klaue_Dornen.png"), thorns, SIZE)


# ---------------------------------------------------------------- Ornamente pro Klasse

def ornaments(folder):
    x, y = grid(ICON, ICON)

    def petals(cx, cy, r, n, w):
        return union(*[ellipse(x, y, cx + math.cos(a) * r, cy + math.sin(a) * r, w, w * 1.6) for a in np.linspace(0, 2 * math.pi, n, endpoint=False)])

    def cog(cx, cy, r, teeth):
        a = np.arctan2(y - cy, x - cx)
        rr = r + 0.06 * (np.cos(a * teeth) > 0.3)
        return cut(np.hypot(x - cx, y - cy) - rr, circle(x, y, cx, cy, r * 0.45))

    corners = {
        # Siegelwachs: gewellte Scheibe mit Prägung
        "Magier": cut(np.hypot(x - 0.5, y - 0.5) - (0.36 + 0.03 * np.cos(np.arctan2(y - 0.5, x - 0.5) * 9)), ring(circle(x, y, 0.5, 0.5, 0.2), 0.02)),
        # Krallen: drei gebogene Spitzen
        "Tamer": union(*[polyline(x, y, bezier((0.2 + 0.25 * i, 0.85), (0.15 + 0.25 * i, 0.45), (0.35 + 0.25 * i, 0.15), 14), 0.05 - 0.01 * i) for i in range(3)]),
        # Nieten: vier runde Köpfe auf einer Platte
        "Ingenieur": union(ring(box(x, y, 0.12, 0.12, 0.88, 0.88, 0.08), 0.03), *[circle(x, y, cx, cy, 0.09) for cx, cy in ((0.3, 0.3), (0.7, 0.3), (0.3, 0.7), (0.7, 0.7))]),
        # Kettenglieder: zwei verhakte Ringe
        "Prophet": union(ring(ellipse(x, y, 0.38, 0.5, 0.24, 0.15), 0.04), ring(ellipse(x, y, 0.62, 0.5, 0.15, 0.24), 0.04)),
        # Blüte mit Ranke
        "Floromant": union(petals(0.5, 0.45, 0.17, 5, 0.1), circle(x, y, 0.5, 0.45, 0.08), polyline(x, y, bezier((0.5, 0.55), (0.25, 0.75), (0.15, 0.95), 12), 0.03)),
    }
    middles = {
        # Siegel: Stern im Kreis
        "Magier": union(ring(circle(x, y, 0.5, 0.5, 0.36), 0.035), polygon(x, y, [(0.5 + 0.28 * math.cos(a), 0.5 + 0.28 * math.sin(a)) for a in [-math.pi / 2 + k * 4 * math.pi / 5 for k in range(5)]])),
        # Schädel eines Tieres
        "Tamer": cut(union(ellipse(x, y, 0.5, 0.42, 0.28, 0.24), box(x, y, 0.36, 0.55, 0.64, 0.8, 0.05)),
                     union(circle(x, y, 0.4, 0.44, 0.07), circle(x, y, 0.6, 0.44, 0.07), box(x, y, 0.47, 0.66, 0.53, 0.8))),
        "Ingenieur": cog(0.5, 0.5, 0.3, 10),
        # Auge
        "Prophet": cut(union(np.maximum(circle(x, y, 0.5, 0.82, 0.48), circle(x, y, 0.5, 0.18, 0.48))), circle(x, y, 0.5, 0.5, 0.1)),
        "Floromant": union(petals(0.5, 0.5, 0.2, 6, 0.11), ring(circle(x, y, 0.5, 0.5, 0.09), 0.025)),
    }
    ends = {
        # Federspitze
        "Magier": union(ellipse(x, y, 0.55, 0.5, 0.35, 0.12), segment(x, y, 0.1, 0.5, 0.9, 0.5, 0.012)),
        # Klaue
        "Tamer": polyline(x, y, bezier((0.15, 0.55), (0.6, 0.6), (0.85, 0.25), 16), 0.06),
        # Patrone mit Zahnrad
        "Ingenieur": union(box(x, y, 0.1, 0.4, 0.55, 0.6, 0.03), ellipse(x, y, 0.55, 0.5, 0.12, 0.1), cog(0.78, 0.5, 0.13, 8)),
        # Speerspitze mit Kettenglied
        "Prophet": union(polygon(x, y, [(0.45, 0.32), (0.92, 0.5), (0.45, 0.68), (0.55, 0.5)]), ring(ellipse(x, y, 0.27, 0.5, 0.14, 0.09), 0.03)),
        # Ranke mit Knospe
        "Floromant": union(polyline(x, y, bezier((0.05, 0.55), (0.4, 0.75), (0.62, 0.48), 16), 0.03), ellipse(x, y, 0.75, 0.42, 0.1, 0.15), petals(0.75, 0.33, 0.06, 3, 0.05)),
    }
    for name, d in corners.items():
        save_shape(os.path.join(folder, "T_HUD_Ecke_%s.png" % name), d, ICON)
    for name, d in middles.items():
        save_shape(os.path.join(folder, "T_HUD_Mitte_%s.png" % name), d, ICON)
    for name, d in ends.items():
        save_shape(os.path.join(folder, "T_HUD_Ende_%s.png" % name), d, ICON)

    # Kleiner Drachenkopf am Ende des Schwanzes (schaut nach links, dem Schwanz entgegen)
    head = union(polygon(x, y, [(0.1, 0.55), (0.45, 0.35), (0.85, 0.4), (0.9, 0.6), (0.45, 0.7)]),
                 polygon(x, y, [(0.55, 0.38), (0.75, 0.1), (0.7, 0.4)]), polygon(x, y, [(0.7, 0.4), (0.95, 0.2), (0.85, 0.45)]))
    head = cut(head, union(circle(x, y, 0.42, 0.48, 0.035), segment(x, y, 0.1, 0.58, 0.45, 0.6, 0.012)))
    save_shape(os.path.join(folder, "T_HUD_Drachenkopf.png"), head, ICON)

    # Weg-Abzeichen: Licht (Sonne), Schatten (Sichel), Zwielicht (halb und halb)
    sun = union(circle(x, y, 0.5, 0.5, 0.2), *[segment(x, y, 0.5 + 0.28 * math.cos(a), 0.5 + 0.28 * math.sin(a), 0.5 + 0.42 * math.cos(a), 0.5 + 0.42 * math.sin(a), 0.03) for a in np.linspace(0, 2 * math.pi, 8, endpoint=False)])
    moon = cut(circle(x, y, 0.5, 0.5, 0.36), circle(x, y, 0.64, 0.42, 0.3))
    dusk = union(np.maximum(circle(x, y, 0.5, 0.5, 0.36), x - 0.5), ring(circle(x, y, 0.5, 0.5, 0.36), 0.03))
    for name, d in (("Licht", sun), ("Schatten", moon), ("Zwielicht", dusk)):
        save_shape(os.path.join(folder, "T_HUD_Weg_%s.png" % name), d, ICON)


# ---------------------------------------------------------------- Nid-Adern (von der linken unteren Ecke)

def veins(folder):
    w, h = 512, 136
    x, y = grid(w, h)
    x = x * (w / h)                     # in Höhen-Einheiten, damit die Adern nicht gestreckt werden
    rng = np.random.default_rng(7)
    lines = np.full(x.shape, 1e3, np.float32)
    along = np.full(x.shape, 1.0, np.float32)
    span = w / h

    def branch(px, py, angle, length, width, start, depth):
        nonlocal lines, along
        steps = 9
        points = [(px, py)]
        for _ in range(steps):
            angle += rng.uniform(-0.35, 0.35)
            px += math.cos(angle) * length / steps
            py += math.sin(angle) * length / steps
            points.append((px, py))
        for i, ((ax, ay), (bx, by)) in enumerate(zip(points, points[1:])):
            t0 = start + (i / steps) * length / span
            d = segment(x, y, ax, ay, bx, by, width * (1.0 - 0.6 * i / steps))
            closer = d < lines
            lines = np.where(closer, d, lines)
            along = np.where(closer & (d < 0.02), np.minimum(along, t0), along)
            if depth > 0 and rng.random() < 0.35:
                branch(bx, by, angle + rng.choice([-1, 1]) * rng.uniform(0.5, 1.0), length * 0.45, width * 0.7, t0, depth - 1)

    for angle in (-0.25, -0.6, -1.0, -0.05):
        branch(0.02, 0.98, angle, span * 0.55, 0.012, 0.0, 2)
    r = fill(lines, h)
    save(os.path.join(folder, "T_HUD_Adern.png"), np.dstack([r, np.clip(along, 0, 1), np.zeros_like(r), r]))


# ---------------------------------------------------------------- Platzhalter-Symbole (Strich)

def icons(folder):
    x, y = grid(ICON, ICON)
    s = 0.035   # Strichstärke
    def stroke(*shapes):
        return union(*shapes)
    def arc(cx, cy, r, a0, a1, n=20):
        return polyline(x, y, [(cx + r * math.cos(a), cy + r * math.sin(a)) for a in np.linspace(a0, a1, n)], s)

    flame = stroke(polyline(x, y, bezier((0.5, 0.88), (0.15, 0.6), (0.48, 0.12), 16), s), polyline(x, y, bezier((0.5, 0.88), (0.85, 0.6), (0.48, 0.12), 16), s),
                   polyline(x, y, bezier((0.5, 0.86), (0.38, 0.68), (0.52, 0.48), 10), s))
    drop = stroke(polyline(x, y, bezier((0.5, 0.12), (0.2, 0.55), (0.3, 0.75), 12), s), polyline(x, y, bezier((0.5, 0.12), (0.8, 0.55), (0.7, 0.75), 12), s), arc(0.5, 0.68, 0.2, 0.3, math.pi - 0.3))
    stone = polyline(x, y, [(0.2, 0.75), (0.28, 0.4), (0.5, 0.25), (0.78, 0.38), (0.82, 0.72), (0.2, 0.75)], s)
    wind = stroke(polyline(x, y, [(0.12, 0.38), (0.62, 0.38)], s), arc(0.62, 0.3, 0.08, math.pi / 2, -math.pi), polyline(x, y, [(0.12, 0.55), (0.78, 0.55)], s), arc(0.78, 0.64, 0.09, -math.pi / 2, math.pi),
                  polyline(x, y, [(0.2, 0.72), (0.55, 0.72)], s))
    nid = stroke(polyline(x, y, bezier((0.5, 0.1), (0.3, 0.5), (0.5, 0.9), 14), s), polyline(x, y, bezier((0.48, 0.45), (0.7, 0.4), (0.82, 0.25), 10), s * 0.7), polyline(x, y, bezier((0.45, 0.65), (0.25, 0.7), (0.15, 0.85), 10), s * 0.7))
    cast = stroke(arc(0.5, 0.5, 0.32, 0, 2 * math.pi, 28), polyline(x, y, [(0.5, 0.22), (0.5, 0.78)], s), polyline(x, y, [(0.22, 0.5), (0.78, 0.5)], s))
    whip = stroke(polyline(x, y, bezier((0.15, 0.85), (0.3, 0.2), (0.85, 0.25), 18), s), *[segment(x, y, px, py, px + 0.05, py - 0.06, s * 0.6) for px, py in ((0.25, 0.55), (0.35, 0.36), (0.5, 0.27), (0.68, 0.24))])
    call = stroke(arc(0.5, 0.62, 0.12, 0, 2 * math.pi), arc(0.5, 0.62, 0.24, math.pi * 1.1, math.pi * 1.9), arc(0.5, 0.62, 0.36, math.pi * 1.15, math.pi * 1.85), polyline(x, y, [(0.5, 0.74), (0.5, 0.92)], s))
    sow = stroke(polyline(x, y, [(0.15, 0.82), (0.85, 0.82)], s), polyline(x, y, bezier((0.5, 0.82), (0.45, 0.6), (0.5, 0.45), 10), s), arc(0.4, 0.42, 0.1, -0.2, math.pi * 1.2), arc(0.6, 0.38, 0.1, math.pi * 0.2 - math.pi, 0.4))
    grip = stroke(*[polyline(x, y, bezier((0.25 + 0.25 * i, 0.9), (0.15 + 0.25 * i, 0.5), (0.35 + 0.25 * i, 0.2), 14), s) for i in range(3)], polyline(x, y, [(0.1, 0.9), (0.9, 0.9)], s))
    leap = stroke(arc(0.5, 0.75, 0.32, math.pi * 1.05, math.pi * 1.95), polyline(x, y, [(0.78, 0.62), (0.82, 0.72), (0.72, 0.72)], s), polyline(x, y, [(0.18, 0.88), (0.18, 0.75)], s), polyline(x, y, [(0.82, 0.88), (0.82, 0.78)], s))
    grow = stroke(polyline(x, y, bezier((0.5, 0.92), (0.4, 0.5), (0.5, 0.1), 14), s), arc(0.35, 0.5, 0.15, -0.5, math.pi * 0.9), arc(0.66, 0.35, 0.15, math.pi * 0.1, math.pi * 1.5), polyline(x, y, [(0.15, 0.2), (0.25, 0.1), (0.35, 0.2)], s * 0.7), polyline(x, y, [(0.65, 0.88), (0.75, 0.78), (0.85, 0.88)], s * 0.7))
    potion = stroke(arc(0.5, 0.62, 0.25, 0, 2 * math.pi), polyline(x, y, [(0.42, 0.38), (0.42, 0.2), (0.58, 0.2), (0.58, 0.38)], s), polyline(x, y, [(0.3, 0.68), (0.7, 0.68)], s))
    dodge = stroke(polyline(x, y, [(0.2, 0.5), (0.8, 0.5)], s), polyline(x, y, [(0.62, 0.32), (0.8, 0.5), (0.62, 0.68)], s), polyline(x, y, [(0.2, 0.35), (0.4, 0.35)], s * 0.7), polyline(x, y, [(0.2, 0.65), (0.4, 0.65)], s * 0.7))
    tree = stroke(polyline(x, y, [(0.5, 0.9), (0.5, 0.15)], s), polyline(x, y, bezier((0.5, 0.55), (0.3, 0.45), (0.2, 0.25), 10), s), polyline(x, y, bezier((0.5, 0.45), (0.7, 0.35), (0.8, 0.15), 10), s), polyline(x, y, bezier((0.5, 0.7), (0.72, 0.65), (0.82, 0.5), 10), s))
    book = stroke(polyline(x, y, [(0.5, 0.25), (0.5, 0.85)], s), polyline(x, y, [(0.5, 0.25), (0.18, 0.2), (0.18, 0.8), (0.5, 0.85), (0.82, 0.8), (0.82, 0.2), (0.5, 0.25)], s))
    chart = stroke(polyline(x, y, [(0.15, 0.25), (0.38, 0.15), (0.62, 0.25), (0.85, 0.15), (0.85, 0.75), (0.62, 0.85), (0.38, 0.75), (0.15, 0.85), (0.15, 0.25)], s), polyline(x, y, [(0.38, 0.15), (0.38, 0.75)], s * 0.7), polyline(x, y, [(0.62, 0.25), (0.62, 0.85)], s * 0.7))
    seed = stroke(polyline(x, y, bezier((0.5, 0.15), (0.15, 0.55), (0.5, 0.88), 14), s), polyline(x, y, bezier((0.5, 0.15), (0.85, 0.55), (0.5, 0.88), 14), s), polyline(x, y, [(0.5, 0.3), (0.5, 0.75)], s * 0.6))
    paw = stroke(arc(0.5, 0.65, 0.16, 0, 2 * math.pi), *[arc(px, py, 0.07, 0, 2 * math.pi, 12) for px, py in ((0.28, 0.4), (0.42, 0.28), (0.58, 0.28), (0.72, 0.4))])
    cog = stroke(arc(0.5, 0.5, 0.22, 0, 2 * math.pi, 28), arc(0.5, 0.5, 0.08, 0, 2 * math.pi, 12), *[segment(x, y, 0.5 + 0.22 * math.cos(a), 0.5 + 0.22 * math.sin(a), 0.5 + 0.34 * math.cos(a), 0.5 + 0.34 * math.sin(a), s) for a in np.linspace(0, 2 * math.pi, 8, endpoint=False)])
    eye = stroke(arc(0.5, 0.85, 0.48, math.pi * 1.2, math.pi * 1.8), arc(0.5, 0.15, 0.48, math.pi * 0.2, math.pi * 0.8), arc(0.5, 0.5, 0.1, 0, 2 * math.pi, 14))
    for name, d in (("Feuer", flame), ("Wasser", drop), ("Erde", stone), ("Luft", wind), ("Nid", nid), ("Wirken", cast), ("Dornenpeitsche", whip),
                    ("RufDesGartens", call), ("Saeen", sow), ("Wurzelgriff", grip), ("Wurzelsprung", leap), ("Wuchern", grow), ("Trank", potion),
                    ("Ausweichen", dodge), ("Adernbaum", tree), ("Rezeptbuch", book), ("Karte", chart), ("Saat", seed), ("Tamer", paw), ("Ingenieur", cog), ("Prophet", eye)):
        save_shape(os.path.join(folder, "T_Symbol_%s.png" % name), d, ICON)


def main(folder):
    os.makedirs(folder, exist_ok=True)
    vessels(folder)
    claw(folder)
    ornaments(folder)
    veins(folder)
    icons(folder)
    print("[Vael] HUD-Formen geschrieben:", len([f for f in os.listdir(folder) if f.endswith(".png")]))


if __name__ == "__main__":
    main(sys.argv[sys.argv.index("--") + 1])
