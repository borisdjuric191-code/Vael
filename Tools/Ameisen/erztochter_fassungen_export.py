"""
Schreibt die Kristallplätze der Erztochter für das Spiel (wie ameise_fassungen_export.py, dazu Zonen und Plätze
außerhalb der Mulden).

Quellen:
  - mulden_alle.json (ameise_mulden_ring.py auf dem Tripo-Modell: Länge 1, Kopf bei +X, Fuß unten mittig).
    Umrechnung in den Raum des Fabrik-Modells: -90° um Z drehen, mal Länge des Modells.
  - Das gebundene Fabrik-Modell (Erztochter.blend nach erztochter_skelett.py): die Spitzen der beiden Kamm-Hörner und
    die zerrissenen Kanten der Flügelstummel werden dort direkt gesucht (keine Mulden, aber Boris' Entwurf 27.11.:
    Kamm-Spitzen tragen das Leit-Erz als Krone, die Stummelkanten Splitter des Begleit-Erzes).

Zonen: Kamm, KammFuss, Kopf, Sattel (Brustpanzer), Hinterleib, Fluegel, Bein. Knochen: head, body, wing_L, wing_R, leg_*.

  blender -b <Erztochter.blend> -P Tools/Ameisen/erztochter_fassungen_export.py -- <mulden_alle.json> <Ausgabe.inl>
"""
import json
import math
import sys

import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

args = sys.argv[sys.argv.index("--") + 1:]
MULDEN, OUT = args[0], args[1]
obj = next(o for o in bpy.data.objects if o.type == "MESH" and o.name.startswith("SK_"))
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
me = obj.data
P = [obj.matrix_world @ v.co for v in me.vertices]
N = [(obj.matrix_world.to_3x3() @ v.normal).normalized() for v in me.vertices]
lo = Vector((min(p.x for p in P), min(p.y for p in P), min(p.z for p in P)))
hi = Vector((max(p.x for p in P), max(p.y for p in P), max(p.z for p in P)))
L = hi.y - lo.y
H = hi.z - lo.z
S = L / 1.1
dreh = Matrix.Rotation(math.radians(-90), 4, "Z")
bvh = BVHTree.FromPolygons(P, [tuple(p.vertices) for p in me.polygons])
knochen = {b.name: (arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local) for b in arm.data.bones}
KOPF_BIS = lo.y + 0.33 * L          # vor der Brust: Kopf und Kamm
SATTEL_BIS = lo.y + 0.50 * L        # Brustpanzer (Sattel) bis zur Taille


def zeile(bone, zone, p, n, r, tiefe):
    return '\t{{TEXT("{}"), TEXT("{}"), FVector({:.4f}, {:.4f}, {:.4f}), FVector({:.4f}, {:.4f}, {:.4f}), {:.4f}f, {:.4f}f}},'.format(
        bone, zone, p.x, p.y, p.z, n.x, n.y, n.z, r, tiefe)


zeilen, fehler, zaehl = [], [], {}
with open(MULDEN, encoding="utf-8") as f:
    mulden = json.load(f)["mulden"]
for m in mulden:
    p = (dreh @ Vector(m["p"])) * L
    n = (dreh.to_3x3() @ Vector(m["n"])).normalized()
    nah, _, _, d = bvh.find_nearest(p)
    fehler.append(d)
    if p.y < KOPF_BIS:
        bone, zone = "head", "Kopf"
    else:
        bone, zone = "body", ("Sattel" if p.y < SATTEL_BIS else "Hinterleib")
    zaehl[zone] = zaehl.get(zone, 0) + 1
    zeilen.append(zeile(bone, zone, p, n, m["r"] * L, m.get("tiefe", 0.0) * L))


def hauptachse(pts):
    c = sum(pts, Vector()) / len(pts)
    import numpy as np
    a = np.array([tuple(q - c) for q in pts])
    w, v = np.linalg.eigh(a.T @ a)
    return c, Vector(v[:, -1]).normalized()


# Kamm: je Seite das große Horn (Punkte des Kopfbereichs weit über dem Kopf). Boris 27.11.: die Hörner gut verteilt
# besetzen. In Scheiben entlang der Hornachse sitzen die Kristalle auf den am weitesten abstehenden Zacken (je Scheibe
# zwei, in verschiedene Richtungen), oben eine lange Spitze; die unterste Scheibe bekommt das Begleit-Erz (Kammfuß).
for seite, vz in (("L", 1), ("R", -1)):
    horn = [i for i, q in enumerate(P) if q.y < KOPF_BIS + 0.02 * L and q.z > 0.62 * H and q.x * vz > 0.01 * S]
    if len(horn) < 30:
        print("KAMM fehlt", seite, len(horn))
        continue
    pts = [P[i] for i in horn]
    c, achse = hauptachse(pts)
    if achse.z < 0:
        achse = -achse
    spitze = max(pts, key=lambda q: (q - c).dot(achse))
    fuss = min(pts, key=lambda q: (q - c).dot(achse))
    laenge = (spitze - fuss).dot(achse)
    zaehl_horn = 0
    for k, anteil in enumerate((0.12, 0.24, 0.36, 0.48, 0.60, 0.72, 0.84)):
        schicht = [i for i in horn if abs((P[i] - fuss).dot(achse) - laenge * anteil) < 0.045 * laenge]
        if not schicht:
            continue
        mitte = fuss + achse * laenge * anteil
        def radial(i):
            v = P[i] - mitte
            return v - achse * v.dot(achse)
        erste = max(schicht, key=lambda i: radial(i).length)
        r1 = radial(erste).normalized()
        andere = [i for i in schicht if radial(i).normalized().dot(r1) < -0.2]
        wahl = [erste] + ([max(andere, key=lambda i: radial(i).length)] if andere else [])
        for i in wahl:
            n = (radial(i).normalized() * 0.8 + achse * 0.7 + N[i] * 0.3).normalized()
            zone = "KammFuss" if anteil < 0.2 else "Kamm"
            zeilen.append(zeile("head", zone, P[i], n, 0.024 * S, 0.0))
            zaehl[zone] = zaehl.get(zone, 0) + 1
            zaehl_horn += 1
    # die Spitze: eine lange Spitze in Richtung des Horns
    ort = fuss + achse * laenge * 0.93
    ort = bvh.find_nearest(ort)[0]          # das Horn ist gebogen: auf die Oberfläche, nicht in die Luft
    zeilen.append(zeile("head", "Kamm", ort, achse, 0.034 * S, 0.0))
    zaehl["Kamm"] += 1
    print("KAMM", seite, len(horn), "Laenge", round(laenge, 2), "Plaetze", zaehl_horn + 1)

# Flügelstummel: Splitter an beiden zerrissenen Kanten, vom ersten Viertel bis zur Spitze (Boris 27.11.: einige,
# größer und länger)
FLUEGEL_Z = 0.6 * H
for seite, vz in (("L", 1), ("R", -1)):
    ids = [i for i, q in enumerate(P) if q.z > FLUEGEL_Z and q.y > lo.y + 0.42 * L and q.x * vz > 0.05 * S]
    if len(ids) < 30 or "wing_" + seite not in knochen:
        print("FLUEGEL fehlt", seite)
        continue
    wurzel, spitze = knochen["wing_" + seite]
    achse = (spitze - wurzel).normalized()
    lang = (spitze - wurzel).length
    def quer(i):
        v = P[i] - wurzel
        return v - achse * v.dot(achse)
    for anteil in (0.25, 0.36, 0.47, 0.58, 0.69, 0.80, 0.90):
        band = [i for i in ids if abs((P[i] - wurzel).dot(achse) - lang * anteil) < 0.035 * S]
        if not band:
            continue
        a_ = max(band, key=lambda i: quer(i).length)
        e1 = quer(a_).normalized()
        gegen = [i for i in band if quer(i).dot(e1) < 0]
        for i in [a_] + ([max(gegen, key=lambda i: -quer(i).dot(e1))] if gegen else []):
            q = quer(i).normalized()
            n = (q + N[i] * 0.5 + Vector((0, 0, 0.35))).normalized()
            zeilen.append(zeile("wing_" + seite, "Fluegel", P[i], n, 0.019 * S, 0.0))
            zaehl["Fluegel"] = zaehl.get("Fluegel", 0) + 1

# Beine (Boris 27.11.: ein paar): je Bein einer oben auf dem Oberschenkel, an Vorder- und Hinterbeinen einer am
# Schienbein; außen-oben auf dem Teil, das fast ganz an diesem Knochen hängt
gruppe = {g.name: g.index for g in obj.vertex_groups}
for seite, vz in (("L", 1), ("R", -1)):
    for nr in ("1", "2", "3"):
        for teil, anteil in (("femur", 0.55), ("tibia", 0.3)):
            name = "leg_{}{}_{}".format(seite, nr, teil)
            if teil == "tibia" and nr == "2" or name not in knochen or name not in gruppe:
                continue
            a_, b_ = knochen[name]
            ax = (b_ - a_)
            if ax.length < 0.02:
                continue
            ziel = a_ + ax * anteil
            gi = gruppe[name]
            kand = [v.index for v in me.vertices
                    if any(g.group == gi and g.weight > 0.7 for g in v.groups) and (P[v.index] - ziel).length < 0.09 * S]
            if not kand:
                continue
            richt = Vector((vz * 0.8, 0, 0.6)).normalized()
            i = max(kand, key=lambda i: N[i].dot(richt) - (P[i] - ziel).length / S * 3)
            n = (N[i] + richt * 0.5).normalized()
            zeilen.append(zeile(name, "Bein", P[i], n, 0.014 * S, 0.0))
            zaehl["Bein"] = zaehl.get("Bein", 0) + 1

with open(OUT, "w", encoding="utf-8") as f:
    f.write("// Erzeugt von Tools/Ameisen/erztochter_fassungen_export.py – nicht von Hand ändern.\n")
    f.write("// Kristallplätze der Erztochter im Raum des Blender-Modells (Meter, Blick nach -Y, Fuß unten mittig):\n")
    f.write("// Knochen, Zone, Boden der Mulde (bzw. Ansatz), Achse heraus, Radius, Tiefe.\n")
    f.write("\n".join(zeilen) + "\n")
print("PLAETZE", len(zeilen), zaehl, "Mulden-Abstand zum Netz max", round(max(fehler) * 100, 2), "cm")
