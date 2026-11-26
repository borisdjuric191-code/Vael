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

  blender -b -P Tools/Ameisen/ameise_mulden_ring.py -- <ameise.fbx> <mulden_saat.json> <mulden.json>
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


def ring(obj, bvh, kd, p, r0):
    vs = obj.data.vertices
    near = kd.find_range(p, r0 * 2.0)
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
    boden = mitte + n * hm
    return {"p": list(boden), "n": list(n), "r": r, "tiefe": max(0.0, hr - hm), "gut": len(gut)}


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
        if m["blick"] < 0.15 or (m["blick"] < 0.45 and m["tiefe"] < 0.003):
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
for f_ in fehl:
    print("FEHL", f_)
print("MULDEN", len(mulden), "FEHL", len(fehl))
with open(OUT, "w", encoding="utf-8") as f:
    json.dump({"mulden": mulden}, f, indent=1)
