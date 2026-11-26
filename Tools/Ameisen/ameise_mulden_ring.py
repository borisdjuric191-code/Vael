"""
Mulden der Schürfameise über ihre FORM bestimmen (Boris 26.11.: nicht nur Höhen und Tiefen, die Mulden logisch verstehen).

Jede Fassung ist ein erhabener Ring (Rand) um einen Boden; manche sind tief, manche nur angedeutet (flacher Boden, nur der
Ring). Ausgangspunkte sind die von Hand gelesenen Mulden aus mulden_saat.json (Pixel in den Prüfbildern). Von dort:

  1. Strahl aus der Prüfkamera durch das Pixel -> Punkt auf der Oberfläche.
  2. Oberflächennormale = Mittel der Ecknormalen in der weiteren Umgebung (über die Mulde hinweg).
  3. In 36 Richtungen wird von oben abgetastet, wie hoch die Oberfläche entlang des Strahls liegt (Höhenprofil).
     Die Wölbung des Körpers wird abgezogen (Parabel durch die Außenpunkte), dann ist der Rand der höchste Punkt.
  4. Durch die 36 Randpunkte wird ein Kreis gelegt (Ausreißer raus) -> echte Mitte, echter Radius.
  5. Tiefe = Rand minus Boden in der Mitte. Doppelte (aus mehreren Ansichten) werden zusammengelegt.

  6. (mit Textur) Boris 27.11.: auch alle Kreise, die in der Mitte dunkler sind. Jeder dunkle Fleck der Grundfarbe
     (dunkler als die Umgebung, nicht schwarz) wird ein weiterer Startpunkt; aufgenommen wird er nur, wenn die
     Ringabtastung dort eine echte Fassung findet und der Körper darunter dick ist (keine Beine).

  blender -b -P Tools/Ameisen/ameise_mulden_ring.py -- <ameise.fbx> <mulden_saat.json> <mulden.json> [basecolor.jpg]
"""
import json
import math
import sys

import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

args = sys.argv[sys.argv.index("--") + 1:]
ANT, SAAT, OUT = args[0], args[1], args[2]
TEX = args[3] if len(args) > 3 else None
RES, ORTHO = 1600, 1.15
ZIEL = Vector((0, 0, 0.32))
ANSICHTEN = {
    "oben": Vector((0, 0, 1)),
    "links": Vector((0, 1, 0.15)),
    "rechts": Vector((0, -1, 0.15)),
    "vorne": Vector((1, 0, 0.3)),
    "hinten": Vector((-1, 0, 0.3)),
    "schraeg_links": Vector((0.5, 0.8, 0.8)),
    "schraeg_rechts": Vector((-0.5, -0.8, 0.8)),
    "oben_links": Vector((0, 0.7, 0.7)),
    "oben_rechts": Vector((0, -0.7, 0.7)),
    "oben_hinterleib": Vector((0.7, 0, 0.7)),
    "oben_kopf": Vector((-0.7, 0, 0.7)),
}
RICHTUNGEN = 36
NACHRICHTEN = False      # Achse nach dem ersten Kreis aus dem Rand neu bestimmen (brachte Fehltreffer, aus)


def load():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=ANT)
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    pts = [v.co for v in obj.data.vertices]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    size = max(hi - lo)
    centre = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    obj.data.transform(Matrix.Scale(1.0 / size, 4) @ Matrix.Translation(-centre))
    obj.data.update()
    return obj


def kamera(d):
    d = d.normalized()
    loc = ZIEL + d * 3
    rot = (-d).to_track_quat("-Z", "Y" if abs(d.z) < 0.99 else "X").to_matrix()
    return loc, rot


def pixel_strahl(name, x, y):
    loc, rot = kamera(ANSICHTEN[name])
    right, up, fwd = rot.col[0], rot.col[1], -rot.col[2]
    u, v = x / RES - 0.5, 0.5 - y / RES
    return loc + right * u * ORTHO + up * v * ORTHO, fwd


def hoehe(bvh, p, n, punkt):
    """Höhe der Oberfläche über der Ebene (p, n) an der Stelle punkt (in der Ebene), von oben abgetastet."""
    loc, nor, idx, dist = bvh.ray_cast(punkt + n * 0.12, -n, 0.3)
    if loc is None:
        return None
    return (loc - p).dot(n)


def kreis(pts):
    """Kreis durch 2D-Punkte (Kasa), gibt (cx, cy, r) zurück."""
    n = len(pts)
    sx = sum(p[0] for p in pts); sy = sum(p[1] for p in pts)
    sxx = sum(p[0] * p[0] for p in pts); syy = sum(p[1] * p[1] for p in pts); sxy = sum(p[0] * p[1] for p in pts)
    sz = [p[0] * p[0] + p[1] * p[1] for p in pts]
    szx = sum(z * p[0] for z, p in zip(sz, pts)); szy = sum(z * p[1] for z, p in zip(sz, pts)); szz = sum(sz)
    # [sxx sxy sx][a]   [szx]
    # [sxy syy sy][b] = [szy]   x² + y² = a x + b y + c
    # [sx  sy  n ][c]   [szz]
    m = Matrix(((sxx, sxy, sx), (sxy, syy, sy), (sx, sy, n)))
    try:
        a, b, c = m.inverted() @ Vector((szx, szy, szz))
    except ValueError:
        return None
    cx, cy = a / 2, b / 2
    r2 = c + cx * cx + cy * cy
    return (cx, cy, math.sqrt(r2)) if r2 > 0 else None


def ring(obj, bvh, kd, p, r0, n=None, tiefe_runde=0):
    """Fassung um p abtasten. Die Achse (n) ist zuerst das Mittel der Normalen in der Nähe; nach dem ersten Kreis wird sie
    aus den Normalen auf dem gefundenen Rand neu bestimmt (der Rand ist symmetrisch, sein Mittel ist die Körpernormale an
    der Mulde, auch auf stark gewölbten Stellen) und der Kreis einmal neu eingepasst."""
    vs = obj.data.vertices
    if n is None:
        near = kd.find_range(p, r0 * 1.3)
        n = Vector()
        for co, j, d in near:
            n += vs[j].normal
        n = n.normalized()
    t1 = n.orthogonal().normalized()
    t2 = n.cross(t1)
    rand = []
    schritte = [r0 * (0.15 + 0.05 * k) for k in range(40)]          # 0.15 .. 2.1 r0
    for k in range(RICHTUNGEN):
        a = 2 * math.pi * k / RICHTUNGEN
        e = t1 * math.cos(a) + t2 * math.sin(a)
        prof = []
        for t in schritte:
            h = hoehe(bvh, p, n, p + e * t)
            if h is not None:
                prof.append((t, h))
        if len(prof) < 20:
            continue
        # Wölbung abziehen: Parabel h = a + b t² durch die äußersten Punkte (jenseits 1.6 r0) und die Mitte
        aussen = [(t, h) for t, h in prof if t > r0 * 1.6]
        if len(aussen) < 3:
            continue
        h0 = prof[0][1]
        b = sum((h - h0) / (t * t) for t, h in aussen) / len(aussen)
        b = min(b, 0.0)
        best = max(((h - h0 - b * t * t), t) for t, h in prof if r0 * 0.35 < t < r0 * 1.7)
        rand.append((e, best[1], best[0]))
    if len(rand) < RICHTUNGEN * 0.6:
        return None
    pts = [((e * t).dot(t1), (e * t).dot(t2)) for e, t, hub in rand]
    c = kreis(pts)
    if not c:
        return None
    for _ in range(2):          # Ausreißer (Randpunkte, die auf eine Nachbarmulde gesprungen sind) entfernen
        cx, cy, r = c
        gut = [q for q in pts if abs(math.hypot(q[0] - cx, q[1] - cy) - r) < r * 0.25]
        if len(gut) < 10:
            return None
        c = kreis(gut)
        if not c:
            return None
    cx, cy, r = c
    mitte = p + t1 * cx + t2 * cy
    if not (0.5 * r0 < r < 1.6 * r0) or math.hypot(cx, cy) > 0.8 * r0:
        return None          # verrutscht: der Kreis gehört nicht zur angeklickten Mulde
    hm = hoehe(bvh, p, n, mitte)
    hr = sum(hoehe(bvh, p, n, mitte + (t1 * math.cos(a) + t2 * math.sin(a)) * r) or 0
             for a in [2 * math.pi * k / 12 for k in range(12)]) / 12
    if hm is None:
        return None
    if tiefe_runde == 0 and NACHRICHTEN:
        rn = Vector()
        for co, j, d in kd.find_range(mitte, r * 1.45):
            if d > r * 0.85:
                rn += vs[j].normal
        if rn.length > 0:
            rn = rn.normalized()
            if rn.angle(n) > math.radians(8):
                besser = ring(obj, bvh, kd, mitte + n * hm, r, rn, 1)
                # nur übernehmen, wenn es dieselbe Fassung bleibt (ähnliche Größe, kaum verschoben)
                if besser and 0.75 * r < besser["r"] < 1.3 * r and                         (Vector(besser["p"]) - (mitte + n * hm)).length < 0.5 * r:
                    return besser
    boden = mitte + n * hm
    hubs = sorted(hub for e, t, hub in rand)
    return {"p": list(boden), "n": list(n), "r": r, "tiefe": max(0.0, hr - hm), "gut": len(gut),
            "rand": hubs[len(hubs) // 4] / r}      # Randhöhe (unteres Viertel der Richtungen) im Verhältnis zum Radius


obj = load()
me = obj.data
bvh = BVHTree.FromPolygons([v.co.copy() for v in me.vertices], [tuple(p.vertices) for p in me.polygons])
kd = KDTree(len(me.vertices))
for i, v in enumerate(me.vertices):
    kd.insert(v.co, i)
kd.balance()
with open(SAAT, encoding="utf-8") as f:
    saat = json.load(f)
mulden, fehl = [], []
for name, liste in saat.items():
    if name.startswith("_"):
        continue
    for x, y, rp in liste:
        o, d = pixel_strahl(name, x, y)
        loc, nor, idx, dist = bvh.ray_cast(o, d)
        if loc is None:
            fehl.append((name, x, y, "kein Treffer"))
            continue
        r0 = rp / RES * ORTHO
        m = ring(obj, bvh, kd, loc, r0)
        if not m:
            fehl.append((name, x, y, "kein Ring"))
            continue
        m["quelle"] = [name, x, y]
        m["blick"] = Vector(m["n"]).dot(-d)        # wie gerade die Ansicht auf die Mulde schaut
        if m["blick"] < 0.45:
            # zweiter Versuch: Achse aus der Fläche genau an der Klickstelle (auf stark gewölbten Stellen kippt das
            # Mittel über die ganze Umgebung weg)
            nk = Vector()
            for co, j, dd in kd.find_range(loc, r0 * 0.8):
                nk += me.vertices[j].normal
            if nk.length > 0 and nk.normalized().dot(-d) > 0.4:
                m2 = ring(obj, bvh, kd, loc, r0, nk.normalized(), 1)
                if m2:
                    m2["quelle"] = [name, x, y, "achse"]
                    m2["blick"] = Vector(m2["n"]).dot(-d)
                    m = m2
        # weder Rand noch Boden: kein Ring, sondern eine glatte Stelle (der Kreis wurde ins Leere eingepasst)
        if m["rand"] < 0.15 and m["tiefe"] / m["r"] < 0.05:
            fehl.append((name, x, y, "kein Rand, kein Boden"))
            continue
        # schräg gesehen: nur nehmen, wenn die Form eindeutig eine Fassung ist (erhabener Rand, flacher Boden)
        if m["blick"] < 0.45 and (m["rand"] < 0.25 or not (0.1 <= m["tiefe"] / m["r"] <= 0.9)):
            fehl.append((name, x, y, "zu schraeg gesehen"))
            continue
        # Doppelte zusammenlegen: dieselbe Mulde aus mehreren Ansichten, die am geradesten gesehene bleibt
        p = Vector(m["p"])
        dop = [k for k, a in enumerate(mulden) if (Vector(a["p"]) - p).length < 0.8 * max(a["r"], m["r"])]
        if dop:
            k = dop[0]
            if m["blick"] > mulden[k]["blick"]:
                mulden[k] = m
            continue
        mulden.append(m)


def dicke(p, n):
    loc, nor, idx, dist = bvh.ray_cast(p - n * 0.003, -n, 1.0)
    return dist if loc is not None else 0.0


def farbflecken():
    import numpy as np
    from collections import deque
    img = bpy.data.images.load(TEX)
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    px = px.reshape(h, w, 4)
    lum = 0.3 * px[:, :, 0] + 0.59 * px[:, :, 1] + 0.11 * px[:, :, 2]
    uv = me.uv_layers.active.data
    summe = np.zeros(len(me.vertices)); zahl = np.zeros(len(me.vertices))
    for loop in me.loops:
        u, v = uv[loop.index].uv
        summe[loop.vertex_index] += lum[min(h - 1, max(0, int((v % 1.0) * h))), min(w - 1, max(0, int((u % 1.0) * w)))]
        zahl[loop.vertex_index] += 1
    L = summe / np.maximum(zahl, 1)
    vs = me.vertices
    dunkel = []
    for i, v in enumerate(vs):
        hood = kd.find_range(v.co, 0.045)
        mittel = sum(L[j] for _, j, _ in hood) / len(hood)
        dunkel.append(L[i] < mittel - 0.03 and L[i] > 0.16 and v.co.z > 0.15 and v.normal.z > -0.35)
    nach = [[] for _ in vs]
    for e in me.edges:
        a, b = e.vertices
        nach[a].append(b); nach[b].append(a)
    gesehen = [False] * len(vs)
    flecken = []
    for s0 in range(len(vs)):
        if not dunkel[s0] or gesehen[s0]:
            continue
        q = deque([s0]); gesehen[s0] = True; teil = []
        while q:
            i = q.popleft(); teil.append(i)
            for j in nach[i]:
                if dunkel[j] and not gesehen[j]:
                    gesehen[j] = True; q.append(j)
        if len(teil) < 4:
            continue
        pts = [vs[i].co for i in teil]
        c = sum(pts, Vector()) / len(pts)
        rf = math.sqrt(sum((p - c).length_squared for p in pts) / len(pts)) * 1.41
        _, k, _ = kd.find(c)
        flecken.append((vs[k].co.copy(), rf))
    return flecken


_raster_pts = []


def kd_r_naechster(co):
    return min(((co - q).length for q in _raster_pts), default=9.0)


def kd_r_add(co):
    _raster_pts.append(co.copy())


if TEX:
    flecken = farbflecken()
    dazu = 0
    for c, rf in flecken:
        for r0 in (rf * 1.3, rf * 1.0, rf * 1.7):
            r0 = min(0.07, max(0.008, r0))
            m = ring(obj, bvh, kd, c, r0)
            # echte Fassung: erhabener Rand ringsum und ein Boden, aber kein tiefes Loch (Auge, Spalt am Gelenk)
            if not m or m["rand"] < 0.25 or not (0.1 <= m["tiefe"] / m["r"] <= 0.9) or m["r"] > 0.035:
                continue
            p, n = Vector(m["p"]), Vector(m["n"])
            if dicke(p, n) < 0.07 or n.z < -0.18:
                break           # Bein, Fühler, Gelenk oder Unterseite (eingebackene Schatten)
            if any((Vector(a["p"]) - p).length < 0.8 * max(a["r"], m["r"]) for a in mulden):
                break
            m["quelle"] = ["farbe", round(rf, 3)]
            m["blick"] = 0.9
            mulden.append(m)
            dazu += 1
            break
    print("FARBFLECKEN", len(flecken), "DAZU", dazu)
for f_ in fehl:
    print("FEHL", f_)
print("MULDEN", len(mulden), "FEHL", len(fehl))
with open(OUT, "w", encoding="utf-8") as f:
    json.dump({"mulden": mulden}, f, indent=1)
