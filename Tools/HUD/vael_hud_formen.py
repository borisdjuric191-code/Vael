"""
Vael – HUD-Formen (Blender, ohne Oberfläche)
============================================

Zeichnet alle Formen des Spieler-HUDs aus reiner Geometrie (vorzeichenbehaftete Abstände) als kleine PNG-Masken:
Gefäße, Drachenklaue, Ecken-, Mittel- und Endornamente pro Klasse, Nid-Adern, Drachenkopf und Platzhalter-Symbole.
Keine Zeichnungen, keine Fremdgrafiken: jede Form ist hier in ein paar Zeilen beschrieben und später durch ein
gezeichnetes Bild gleichen Namens ersetzbar.

Gefäße (RGBA): R = Innenraum, in dem die Flüssigkeit steht; G = Glas, Metall, Knochen und Zierrat; B = Risse; A = alles.
Ornamente, Symbole, Klaue (RGBA): weiß mit Alpha = Form, das Spiel färbt sie ein.
Adern (RGBA): R = Lage im Gefäß (1 Mitte, 0 Rand, so kann das Spiel es dünner zeigen), G = Weg ab dem Ursprung (Nid-Slot
0..1, ohne ihn erstes Bündel 0..0,5 von links unten, zweites 0,5..1 von rechts oben),
damit sie mit der Verderbnis kriechen; B = eingebackenes Licht der gewölbten Ader (0,5 neutral, oben links beleuchtet,
nasser Glanz ganz oben); A = weicher Schatten der Ader auf dem Rahmen.

Aufruf:
  blender -b -P vael_hud_formen.py -- <Zielordner> [Teile: gefaesse klaue ornamente adern symbole]
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


# ---------------------------------------------------------------- Nid-Adern

def blur(a, r):
    """Kastenunschärfe, zweimal je Achse: fast wie ein Gauß"""
    for _ in range(2):
        for axis in (0, 1):
            a = sum(np.roll(a, k, axis) for k in range(-r, r + 1)) / (2 * r + 1)
    return a


def vein_net(path, w, h, seed, sources, keep=None, pull=0.06, avoid=None, margin=0.07, curls=False):
    """Gefäße wie unter der Haut: dicke Stämme, die sich nach Murrays Gesetz in immer feinere Adern verzweigen,
    jede Ader ein runder Schlauch, dessen Wölbung von oben links beleuchtet wird.
    sources: (x, y in Höhen-Einheiten, [(Winkel, Radius)], Weg-Funktion über die Länge[, Lenkung (x, y, Richtung) -> Zielrichtung]);
    keep: Maske (x, y) -> 0..1, wo Adern sein dürfen; avoid: Abstand (x, y) zu den Symbolen des HUDs, innen negativ: die Adern
    legen sich ab margin um sie herum und überdecken keines; curls: dünne Enden rollen sich zu Schnörkeln ein"""
    span = w / h
    x, y = grid(w, h)
    x = x * span                        # in Höhen-Einheiten, damit die Adern nicht gestreckt werden
    rng = np.random.default_rng(seed)
    nearest = np.full(x.shape, 1e3, np.float32)
    ratio = np.zeros(x.shape, np.float32)
    height = np.zeros(x.shape, np.float32)
    along = np.ones(x.shape, np.float32)
    reach = 0.035                       # so weit trägt eine Ader ihren Weg nach außen (für ihren Schatten)

    def stamp(ax, ay, bx, by, ra, rb, ta, tb):
        pad = max(ra, rb) + reach
        x0, x1 = max(int((min(ax, bx) - pad) * h), 0), min(int((max(ax, bx) + pad) * h) + 1, w)
        y0, y1 = max(int((min(ay, by) - pad) * h), 0), min(int((max(ay, by) + pad) * h) + 1, h)
        if x0 >= x1 or y0 >= y1:
            return
        sx, sy = x[y0:y1, x0:x1], y[y0:y1, x0:x1]
        dx, dy = bx - ax, by - ay
        t = np.clip(((sx - ax) * dx + (sy - ay) * dy) / (dx * dx + dy * dy + 1e-9), 0, 1)
        dist = np.hypot(sx - ax - dx * t, sy - ay - dy * t)
        r = ra + (rb - ra) * t
        d = dist - r
        np.maximum(ratio[y0:y1, x0:x1], 1.0 - dist / r, out=ratio[y0:y1, x0:x1])
        np.maximum(height[y0:y1, x0:x1], np.sqrt(np.maximum(r * r - dist * dist, 0)), out=height[y0:y1, x0:x1])
        closer = d < nearest[y0:y1, x0:x1]
        nearest[y0:y1, x0:x1] = np.where(closer, d, nearest[y0:y1, x0:x1])
        along[y0:y1, x0:x1] = np.where(closer & (d < reach), ta + (tb - ta) * t, along[y0:y1, x0:x1])

    def grow(px, py, angle, r, travelled, depth, way, steer):
        step, curve, heading = 0.016, rng.uniform(-0.03, 0.03), angle
        # Unter einem Symbol geboren (der Nid-Slot) läuft sie darunter weiter, bis sie draußen ist
        under = avoid is not None and avoid(px, py) < 0.0
        curl, sense, curly = 0.0, rng.choice([-1, 1]), curls and rng.random() < 0.3
        while r > 0.0056:
            # Sanft geschlängelt, aber jede Ader hält ihre Richtung
            curve = curve * 0.8 + rng.normal(0.0, 0.03)
            target, gain = (steer(px, py, heading), pull) if steer else (heading, 0.06)
            if curly and r < 0.0105:
                # Manche dünnen Enden rollen sich zu Schnörkeln ein, immer enger
                curl = min(curl + 0.01, 0.38)
                target, gain = angle + sense * curl * 6.0, 0.0
                angle += sense * curl
            if avoid is not None and not under and not steer:
                d = avoid(px, py)
                if d < margin:
                    # Am Symbol entlang: die Tangente, die der Ader am nächsten liegt, leicht nach außen gedreht
                    e = 0.002
                    gx, gy = avoid(px + e, py) - avoid(px - e, py), avoid(px, py + e) - avoid(px, py - e)
                    normal = math.atan2(gy, gx)
                    side = 1 if math.sin(angle - normal) > 0 else -1
                    target = normal + side * (math.pi / 2 - 0.5 * (margin - d) / margin)
                    gain = 0.3
            angle += curve + math.remainder(target - angle, 2 * math.pi) * gain
            nx, ny = px + math.cos(angle) * step, py + math.sin(angle) * step
            if avoid is not None and not under and avoid(nx, ny) < 0.004:
                # Vor einem Symbol: so wenig wie möglich abbiegen, bis der Weg frei ist, sonst endet die Ader hier
                for turn in (0.25, -0.25, 0.5, -0.5, 0.8, -0.8, 1.2, -1.2, 1.6, -1.6):
                    tx, ty = px + math.cos(angle + turn) * step, py + math.sin(angle + turn) * step
                    if avoid(tx, ty) >= 0.004:
                        angle, nx, ny = angle + turn, tx, ty
                        break
                else:
                    return
            nr = r * 0.9945
            stamp(px, py, nx, ny, r, nr, way(travelled), way(travelled + step))
            px, py, r, travelled = nx, ny, nr, travelled + step
            if under and avoid(px, py) > 0.01:
                under = False
            if not (-0.05 < px < span + 0.05 and -0.05 < py < 1.05):
                return
            if depth > 0 and curl == 0.0 and rng.random() < 0.075:
                # Murray: r³ = r1³ + r2³, der Ast nimmt dem Stamm etwas ab, beide biegen auseinander
                side = rng.choice([-1, 1])
                child = r * rng.uniform(0.5, 0.75)
                r = max(r ** 3 - child ** 3, (0.62 * r) ** 3) ** (1.0 / 3.0)
                grow(px, py, angle + side * rng.uniform(0.45, 1.0), child, travelled, depth - 1, way, None)
                angle -= side * rng.uniform(0.05, 0.2)
                heading -= side * 0.08

    for ox, oy, trunks, way, *steer in sources:
        for angle, radius in trunks:
            grow(ox, oy, angle, radius, 0.0, 3, way, steer[0] if steer else None)

    ratio = np.clip(ratio, 0, 1)
    if keep is not None:
        ratio *= keep(x, y)
    cover = fill(nearest, h)
    if keep is not None:
        cover *= keep(x, y)
    if avoid is not None:
        free = np.clip(avoid(x, y) / 0.006, 0, 1)
        ratio *= free
        cover *= free
    # Licht auf dem gewölbten Schlauch: Normalen aus der Höhe, Licht von oben links, nasser Glanz
    gy, gx = np.gradient(height * 0.8)
    n = np.dstack([-gx * h, -gy * h, np.ones_like(height)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    l = np.array([-0.5, -0.7, 0.8]) / np.linalg.norm([-0.5, -0.7, 0.8])
    half = (l + [0, 0, 1]) / np.linalg.norm(l + [0, 0, 1])
    diffuse = np.clip(n @ l, 0, 1)
    gloss = np.clip(n @ half, 0, 1) ** 50
    light = np.where(cover > 0, np.clip(diffuse * 0.75 + gloss * 0.45, 0, 1), 0.5)
    # Weicher Schatten nach unten rechts
    ox, oy = 5, 7
    shadow = np.zeros_like(cover)
    shadow[oy:, ox:] = cover[:-oy, :-ox]
    shadow = np.clip(blur(shadow, 5) * 0.9, 0, 1)
    save(path, np.dstack([ratio, np.clip(along, 0, 1), light, shadow]))


# Rahmenfeld 880 x 202 Designpixel (ab FrameTop 28); der Nid-Slot des Magiers ist der fünfte von sechs Slots (76 + 6 Abstand)
FRAME_W, FRAME_H = 880.0, 202.0
NID_SLOT_MAGIER = (197.0 + 4 * 82.0 + 38.0, 116.0 + 38.0 - 28.0)
# Symbole des Magier-HUDs in Designpixeln des Panels (880 x 230); siehe Layout in VaelPlayerHUDWidget.cpp.
# Runde Symbole (x, y, Radius) liegen über dem Rahmen: die Adern ziehen hinter ihnen durch und schlingen sich um sie.
# Schrift (x0, y0, x1, y1) und das ins Rahmenbild gemalte Siegel liegen nicht darüber: denen weichen die Adern aus.
SYMBOLS_MAGIER_CIRCLES = [(94, 132, 96), (786, 132, 96), (440, 66, 40), (626, 48, 17), (656, 48, 17), (686, 48, 17),
                          (712, 164, 21), (160, 190, 20), (838, 62, 17)]
SEAL_MAGIER = (440, 66, 42)
TEXT_MAGIER_BOXES = [(196, 44, 318, 82), (480, 46, 622, 74), (284, 88, 432, 110)] + \
                    [(197 + 82 * i + 6, 192, 197 + 82 * i + 70, 207) for i in range(6)]


def _field(x, y):
    return x * FRAME_H, y * FRAME_H + 28.0, (np.full(np.shape(x), 1e3, np.float32) if np.ndim(x) else 1e3)


def symbols_magier(x, y):
    """Abstand zum nächsten runden Symbol, in Höhen-Einheiten des Rahmenfelds (Höhe 202 ab FrameTop 28), innen negativ"""
    px, py, d = _field(x, y)
    for cx, cy, r in SYMBOLS_MAGIER_CIRCLES:
        d = np.minimum(d, np.hypot(px - cx, py - cy) - r)
    return d / FRAME_H


def text_magier(x, y):
    """Abstand zur nächsten Schrift oder zum Siegel, wie symbols_magier"""
    px, py, d = _field(x, y)
    d = np.minimum(d, np.hypot(px - SEAL_MAGIER[0], py - SEAL_MAGIER[1]) - SEAL_MAGIER[2])
    for x0, y0, x1, y1 in TEXT_MAGIER_BOXES:
        d = np.minimum(d, box(px, py, x0, y0, x1, y1, 3.0))
    return d / FRAME_H


def veins(folder):
    # Ohne Nid-Slot: erstes Bündel aus der linken unteren Ecke (Weg 0..0,5), ab 50 % das zweite aus der rechten oberen (0,5..1)
    w, h = 1536, 408
    span = w / h
    full = 2.6
    trunks = [(-0.08, 0.04), (-0.32, 0.032), (-0.62, 0.027), (-1.05, 0.022)]
    vein_net(os.path.join(folder, "T_HUD_Adern.png"), w, h, 11, [
        (0.0, 1.0, trunks, lambda s: 0.5 * min(s / full, 1.0)),
        (span, 0.0, [(a + math.pi, r * 0.9) for a, r in trunks], lambda s: 0.5 + 0.5 * min(s / full, 1.0))])

    # Magier: das Herz ist der Nid-Slot, von dort ziehen die Adern in alle Richtungen über das HUD
    w, h = 1760, 404
    cx, cy = NID_SLOT_MAGIER[0] / FRAME_H, NID_SLOT_MAGIER[1] / FRAME_H
    # Hinter Slots, Siegel und Gefäßen hindurch, zwischen ihnen wieder sichtbar; nur der Schrift weichen sie aus
    trunks = [(math.pi, 0.034), (0.0, 0.032), (math.pi + 0.5, 0.028), (-0.5, 0.028), (-math.pi / 2 - 0.25, 0.026), (-math.pi / 2 + 0.2, 0.024),
              (math.pi + 1.0, 0.022), (-1.0, 0.022), (math.pi + 0.15, 0.02), (-0.15, 0.02)]
    nid = (cx, cy)

    def from_nid(px, py, scale=0.8):
        # Weg der Verderbnis: je weiter vom Nid-Slot, desto später; so bleibt der Nid-Slot das Herz
        start = scale * min(math.hypot(px - nid[0], py - nid[1]) / 3.2, 1.0)
        return lambda s: min(start + s / 3.0, 1.0)

    def hug(sense, off=0.02):
        # Am nächsten Symbol entlang, off davon entfernt: zu weit weg nach innen, zu nah nach außen lenken
        def steer(px, py, heading):
            e = 0.002
            d = symbols_magier(px, py)
            normal = math.atan2(symbols_magier(px, py + e) - symbols_magier(px, py - e), symbols_magier(px + e, py) - symbols_magier(px - e, py))
            return normal + sense * (math.pi / 2 + float(np.clip((d - off) * 25.0, -0.7, 0.7)))
        return steer

    sources = [(cx, cy, trunks, lambda s: min(s / 2.9, 1.0))]
    rng = np.random.default_rng(31)
    # Um jedes runde Symbol (Gefäße, Siegel, Medaillons, Trank, Ausweichen, Stufe) schlingen sich eigene Adern; Schrift wird nur umgangen
    for x0, y0, r in SYMBOLS_MAGIER_CIRCLES:
        wanted, found = (6 if r > 60 else 3), 0
        for phi in rng.permutation(np.linspace(0, 2 * math.pi, 24, endpoint=False)):
            px, py = (x0 + math.cos(phi) * (r + 4)) / FRAME_H, (y0 + math.sin(phi) * (r + 4) - 28.0) / FRAME_H
            if found == wanted or not (0.02 < px < w / h - 0.02 and 0.02 < py < 0.98) or text_magier(px, py) < 0.006:
                continue
            found += 1
            sense = rng.choice([-1, 1])
            sources.append((px, py, [(phi + sense * math.pi / 2, rng.uniform(0.012, 0.018))], from_nid(px, py), hug(sense, rng.uniform(0.008, 0.05))))
    vein_net(os.path.join(folder, "T_HUD_Adern_Magier.png"), w, h, 23, sources, avoid=text_magier, margin=0.03, curls=True, pull=0.15)

    # Das Herz: ein Kranz aus Adern, der den silbernen Rand des Nid-Slots umwächst (130 Designpixel um die Slotmitte,
    # Rand bei etwa 40); sie kommen unter dem Bild hervor, einige laufen zum Rahmen hinaus, die Mitte bleibt frei
    size = 512
    c, ring = 0.5, 40.0 / 130.0
    rng = np.random.default_rng(5)
    sources = []

    def around(sense):
        # Am Rand entlang: drinnen nach außen, draußen nach innen lenken, so legen sie sich um den Slot
        def steer(px, py, heading):
            off = np.clip((np.hypot(px - c, py - c) - ring) * 8.0, -0.7, 0.7)
            return math.atan2(py - c, px - c) + sense * (math.pi / 2 + off)
        return steer

    def way(s):
        return 0.8 * min(s / 1.4, 1.0)

    for k in range(8):
        phi = k * 2 * math.pi / 8 + rng.uniform(-0.25, 0.25)
        sense = 1 if k % 2 == 0 else -1
        sources.append((c + math.cos(phi) * 0.2, c + math.sin(phi) * 0.2, [(phi + sense * 0.9, rng.uniform(0.014, 0.02))], way, around(sense)))
    for phi in (math.pi, 0.0, -math.pi / 2 - 0.4, -0.5, 2.3):
        sources.append((c + math.cos(phi) * 0.2, c + math.sin(phi) * 0.2, [(phi, 0.022)], way))
    def heart_keep(x, y):
        # Auf dem Rand des Nid-Slots, nicht auf seinem Bild, nicht auf den Nachbar-Slots links und rechts, nicht auf seinem Namen
        dx, dy, d = (x - c) * 130.0, (y - c) * 130.0, np.hypot(x - c, y - c)
        mine = np.clip((d - 0.255) / 0.02, 0, 1) * np.clip((0.5 - d) / 0.1, 0, 1)
        neighbours = np.clip((np.abs(dx) - 39.0) / 2.0, 0, 1) * np.clip((44.0 - np.abs(dy)) / 2.0, 0, 1)
        name = np.clip((32.0 - np.abs(dx)) / 2.0, 0, 1) * np.clip((dy - 37.0) / 2.0, 0, 1) * np.clip((54.0 - dy) / 2.0, 0, 1)
        return mine * (1.0 - neighbours) * (1.0 - name)

    vein_net(os.path.join(folder, "T_HUD_Adern_Herz.png"), size, size, 5, sources,
             keep=heart_keep, pull=0.3)

    # Griffe: Nid nimmt die übrigen Knöpfe in Besitz. Adern kommen von der Seite des Nid-Slots und schlängeln sich eng am
    # silbernen Rand entlang um den Knopf, ohne Bild, Namen oder Nachbarn zu bedecken.
    # Slot (100 Designpixel um die Slotmitte; Rand von 31 bis 43, Bild bis 31): A kommt von rechts, B von links
    def along_rim(shape, sense):
        def steer(px, py, heading):
            e = 0.002
            d = shape(px, py)
            normal = math.atan2(shape(px, py + e) - shape(px, py - e), shape(px + e, py) - shape(px - e, py))
            return normal + sense * (math.pi / 2 + float(np.clip(d * 25.0, -0.7, 0.7)))
        return steer

    rim = lambda px, py: box(px, py, 0.13, 0.13, 0.87, 0.87, 0.08)

    def slot_keep(x, y):
        dx, dy = x - 0.5, y - 0.5
        picture = np.clip((box(x, y, 0.19, 0.19, 0.81, 0.81, 0.04)) / 0.012, 0, 1)
        neighbours = np.clip((np.abs(dx) - 0.41) / 0.02, 0, 1) * np.clip((0.43 - np.abs(dy)) / 0.02, 0, 1)
        name = np.clip((0.33 - np.abs(dx)) / 0.02, 0, 1) * np.clip((dy - 0.37) / 0.02, 0, 1)
        edge = np.clip((0.5 - np.maximum(np.abs(dx), np.abs(dy))) / 0.04, 0, 1)
        return picture * (1.0 - neighbours) * (1.0 - name) * edge

    for name, side in (("A", 1), ("B", -1)):
        rng = np.random.default_rng(41 if side > 0 else 43)
        sources = []
        for k in range(5):
            sy = rng.uniform(0.25, 0.75)
            sense = 1 if k % 2 == 0 else -1
            sources.append((0.5 + side * 0.48, sy, [(math.pi if side > 0 else 0.0, rng.uniform(0.02, 0.028))], lambda s: min(s / 3.0, 1.0), along_rim(rim, sense * side)))
        vein_net(os.path.join(folder, "T_HUD_Adern_Griff_%s.png" % name), 512, 512, 41 if side > 0 else 43, sources, keep=slot_keep, pull=0.3, curls=True)

    # Runde Knöpfe (Medaillons, Trank, Ausweichen): Ring bei 0,3 der Bildgröße, die Mitte mit dem Symbol bleibt frei
    rng = np.random.default_rng(47)
    sources = []
    ring_shape = lambda px, py: math.hypot(px - 0.5, py - 0.5) - 0.3
    for k in range(4):
        sense = 1 if k % 2 == 0 else -1
        sources.append((0.98, rng.uniform(0.35, 0.65), [(math.pi, rng.uniform(0.018, 0.024))], lambda s: min(s / 2.2, 1.0), along_rim(ring_shape, sense)))
    vein_net(os.path.join(folder, "T_HUD_Adern_Ring.png"), 512, 512, 47, sources, pull=0.3, curls=True,
             keep=lambda x, y: np.clip((np.hypot(x - 0.5, y - 0.5) - 0.22) / 0.02, 0, 1) * np.clip((0.5 - np.hypot(x - 0.5, y - 0.5)) / 0.06, 0, 1))


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


PARTS = {"gefaesse": vessels, "klaue": claw, "ornamente": ornaments, "adern": veins, "symbole": icons}


def main(folder, parts):
    os.makedirs(folder, exist_ok=True)
    for name, make in PARTS.items():
        if not parts or name in parts:
            make(folder)
    print("[Vael] HUD-Formen geschrieben:", len([f for f in os.listdir(folder) if f.endswith(".png")]))


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:]
    main(args[0], args[1:])
