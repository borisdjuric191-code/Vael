"""
Schreibt die Fassungen der Schürfameise für das Spiel: Lage, Richtung, Größe und Tiefe jeder Mulde im Raum des
fertig aufbereiteten Fabrik-Modells (Schuerfameise.blend nach 'binden', Meter, Blick nach -Y) und der Knochen, an dem
sie hängt (Kopf oder Rumpf). Ergebnis ist eine C++-Datei, die der Datensatz der Ameise einbindet.

Umrechnung aus mulden_alle.json (Länge 1, Kopf bei -X, Fuß unten mittig): 90° um Z drehen, mal Länge des Modells.

  blender -b <Schuerfameise.blend> -P Tools/Ameisen/ameise_fassungen_export.py -- <mulden_alle.json> <Ausgabe.inl>
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
dg = bpy.context.evaluated_depsgraph_get()
P = [obj.matrix_world @ v.co for v in obj.data.vertices]
lo = Vector((min(p.x for p in P), min(p.y for p in P), min(p.z for p in P)))
hi = Vector((max(p.x for p in P), max(p.y for p in P), max(p.z for p in P)))
laenge = hi.y - lo.y
dreh = Matrix.Rotation(math.radians(90), 4, "Z")
bvh = BVHTree.FromPolygons(P, [tuple(p.vertices) for p in obj.data.polygons])
knochen = {b.name: (arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local) for b in arm.data.bones}


def abstand(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / ab.length_squared))
    return (p - (a + ab * t)).length


with open(MULDEN, encoding="utf-8") as f:
    mulden = json.load(f)["mulden"]
zeilen, fehler = [], []
for m in mulden:
    p = (dreh @ Vector(m["p"])) * laenge
    n = (dreh.to_3x3() @ Vector(m["n"])).normalized()
    nah, _, _, d = bvh.find_nearest(p)
    fehler.append(d)
    bone = min([k for k in ("head", "body", "abdomen") if k in knochen], key=lambda k: abstand(p, *knochen[k]))
    zeilen.append('\t{{TEXT("{}"), FVector({:.4f}, {:.4f}, {:.4f}), FVector({:.4f}, {:.4f}, {:.4f}), {:.4f}f, {:.4f}f}},'.format(
        bone, p.x, p.y, p.z, n.x, n.y, n.z, m["r"] * laenge, m.get("tiefe", 0.0) * laenge))
with open(OUT, "w", encoding="utf-8") as f:
    f.write("// Erzeugt von Tools/Ameisen/ameise_fassungen_export.py – nicht von Hand ändern.\n")
    f.write("// Fassungen der Schürfameise im Raum des Blender-Modells (Meter, Blick nach -Y, Fuß unten mittig):\n")
    f.write("// Knochen, Boden der Mulde, Achse aus der Mulde heraus, Radius bis zum Rand, Tiefe.\n")
    f.write("// Knochenköpfe zum Abgleich in Unreal: head {:.4f} {:.4f} {:.4f}, body {:.4f} {:.4f} {:.4f}\n".format(
        *knochen["head"][0], *knochen["body"][0]))
    f.write("\n".join(zeilen) + "\n")
print("FASSUNGEN", len(zeilen), "Abstand zum Netz max", round(max(fehler) * 100, 2), "cm, Mittel",
      round(sum(fehler) / len(fehler) * 100, 2), "cm", "Länge", round(laenge, 3))
