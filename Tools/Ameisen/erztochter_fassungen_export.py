"""
Schreibt die Kristallplätze der Erztochter für das Spiel (wie ameise_fassungen_export.py, dazu Zonen und Plätze
außerhalb der Mulden).

Quellen:
  - mulden_alle.json (ameise_mulden_ring.py auf dem Tripo-Modell: Länge 1, Kopf bei +X, Fuß unten mittig).
    Umrechnung in den Raum des Fabrik-Modells: -90° um Z drehen, mal Länge des Modells.
  - Das gebundene Fabrik-Modell (Erztochter.blend nach erztochter_skelett.py): die Spitzen der beiden Kamm-Hörner und
    die zerrissenen Kanten der Flügelstummel werden dort direkt gesucht (keine Mulden, aber Boris' Entwurf 27.11.:
    Kamm-Spitzen tragen das Leit-Erz als Krone, die Stummelkanten Splitter des Begleit-Erzes).

Zonen: Kamm, KammBelag, KammFuss, FluegelBelag, Kopf, Sattel (Brustpanzer), Hinterleib, Fluegel, Bein. Knochen: head, body, abdomen, wing_L, wing_R, leg_*.

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


def zeile(bone, zone, p, n, r, tiefe, flaeche=None):
    # flaeche: Normale der Körperoberfläche an dieser Stelle (der Belag biegt sich zu ihr hin); sonst die Achse selbst
    f = flaeche if flaeche is not None else n
    return '\t{{TEXT("{}"), TEXT("{}"), FVector({:.4f}, {:.4f}, {:.4f}), FVector({:.4f}, {:.4f}, {:.4f}), FVector({:.4f}, {:.4f}, {:.4f}), {:.4f}f, {:.4f}f}},'.format(
        bone, zone, p.x, p.y, p.z, n.x, n.y, n.z, f.x, f.y, f.z, r, tiefe)


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
        zone = "Sattel" if p.y < SATTEL_BIS else "Hinterleib"
        # der Hinterleib hat seinen eigenen Knochen (ameise_rig.py, Taille)
        bone = "abdomen" if zone == "Hinterleib" and "abdomen" in knochen else "body"
    zaehl[zone] = zaehl.get(zone, 0) + 1
    zeilen.append(zeile(bone, zone, p, n, m["r"] * L, m.get("tiefe", 0.0) * L))


def hauptachse(pts):
    c = sum(pts, Vector()) / len(pts)
    import numpy as np
    a = np.array([tuple(q - c) for q in pts])
    w, v = np.linalg.eigh(a.T @ a)
    return c, Vector(v[:, -1]).normalized()


# Belag (Boris 27.11.): Hörner und Flügelstummel werden von Kristallen überwuchert, sauber und in ihrer Form, so dass
# das Original kaum noch zu sehen ist. Die Oberfläche wird gleichmäßig abgetastet (Mindestabstand, Reihen vom Ansatz
# zur Spitze); jeder Kristall liegt eng an und zeigt in Wuchsrichtung (Horn zur Spitze, Stummel nach außen), leicht
# von der Fläche weg, so dass sie sich wie Schuppen überdecken und eine langgezogene Form ergeben.
def belag(ids, wuchs, abstand, bone, zone_von, r):
    """ids: Ecken der Fläche; wuchs(i): Wuchsrichtung und Anteil 0..1 vom Ansatz; zone_von(anteil) -> Zone"""
    daten = [(i,) + wuchs(i) for i in ids]
    daten.sort(key=lambda d: d[2])                 # vom Ansatz zur Spitze: Reihe für Reihe
    raster = {}
    zahl = 0
    # zur Spitze hin dichter und kleiner, damit auch die dünnen Enden von Horn und Stummel ganz bedeckt sind
    zelle = abstand * 0.6
    for i, richt, anteil in daten:
        q = P[i]
        hier = abstand * (1.0 - 0.4 * max(0.0, min(1.0, anteil)))
        k = (int(q.x // zelle), int(q.y // zelle), int(q.z // zelle))
        nah = any((q - o).length < hier
                  for dx in (-2, -1, 0, 1, 2) for dy in (-2, -1, 0, 1, 2) for dz in (-2, -1, 0, 1, 2)
                  for o in raster.get((k[0] + dx, k[1] + dy, k[2] + dz), ()))
        if nah:
            continue
        raster.setdefault(k, []).append(q.copy())
        r_hier = r * hier / abstand
        n = N[i]
        # flach angelegt (Boris 27.11.: sauber anliegen); das gebogene Stück legt die Spitze zusätzlich an die Fläche
        achse = (richt + n * 0.3).normalized()
        if achse.dot(n) < 0.12:
            achse = (achse + n * 0.3).normalized()
        zone = zone_von(anteil)
        zeilen.append(zeile(bone, zone, P[i], achse, r_hier, 0.0, n))
        zaehl[zone] = zaehl.get(zone, 0) + 1
        zahl += 1
    return zahl


ABSTAND_HORN = 0.04 * S
ABSTAND_FLUEGEL = 0.05 * S
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

    def wuchs_horn(i):
        # zur Spitze hin; das Horn ist gebogen: Richtung zur Spitze, gemischt mit der Hauptachse
        zur = (spitze - P[i])
        richt = (zur.normalized() * 0.5 + achse * 0.5).normalized() if zur.length > 0.01 else achse
        return richt, (P[i] - fuss).dot(achse) / laenge

    zahl = belag(horn, wuchs_horn, ABSTAND_HORN, "head", lambda a_: "KammFuss" if a_ < 0.14 else "KammBelag", 0.9 * ABSTAND_HORN)
    # die Krone: eine lange Spitze oben auf jedem Horn
    ort = bvh.find_nearest(fuss + achse * laenge * 0.93)[0]
    zeilen.append(zeile("head", "Kamm", ort, achse, 0.034 * S, 0.0))
    zaehl["Kamm"] = zaehl.get("Kamm", 0) + 1
    print("KAMM", seite, len(horn), "Laenge", round(laenge, 2), "Belag", zahl)

FLUEGEL_Z = 0.6 * H
for seite, vz in (("L", 1), ("R", -1)):
    ids = [i for i, q in enumerate(P) if q.z > FLUEGEL_Z and q.y > lo.y + 0.42 * L and q.x * vz > 0.05 * S]
    if len(ids) < 30 or "wing_" + seite not in knochen:
        print("FLUEGEL fehlt", seite)
        continue
    wurzel, spitze_f = knochen["wing_" + seite]
    achse_f = (spitze_f - wurzel).normalized()
    lang = (spitze_f - wurzel).length

    def wuchs_fluegel(i):
        # jeder der beiden Stummel einer Seite zeigt woanders hin: von der Wurzel weg, entlang der Fläche
        v = P[i] - wurzel
        richt = (v - N[i] * v.dot(N[i]))
        richt = richt.normalized() if richt.length > 0.01 else achse_f
        return richt, v.length / lang

    zahl = belag(ids, wuchs_fluegel, ABSTAND_FLUEGEL, "wing_" + seite, lambda a_: "FluegelBelag", 0.9 * ABSTAND_FLUEGEL)
    print("FLUEGEL", seite, len(ids), "Belag", zahl)

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
            zeilen.append(zeile(name, "Bein", P[i], n, 0.014 * S, 0.0, N[i]))
            zaehl["Bein"] = zaehl.get("Bein", 0) + 1

with open(OUT, "w", encoding="utf-8") as f:
    f.write("// Erzeugt von Tools/Ameisen/erztochter_fassungen_export.py – nicht von Hand ändern.\n")
    f.write("// Kristallplätze der Erztochter im Raum des Blender-Modells (Meter, Blick nach -Y, Fuß unten mittig):\n")
    f.write("// Knochen, Zone, Boden der Mulde (bzw. Ansatz), Achse heraus, Normale der Oberfläche, Radius, Tiefe.\n")
    f.write("\n".join(zeilen) + "\n")
print("PLAETZE", len(zeilen), zaehl, "Mulden-Abstand zum Netz max", round(max(fehler) * 100, 2), "cm")
