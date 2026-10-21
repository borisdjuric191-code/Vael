"""Measures a tree model of the requisite pipeline for dressing it in Unreal.

Reads <Ordner>/04_export/SM_<Name>.fbx (origin bottom centre, metres) and writes <Ordner>/03_analyse/baum.json:
height, trunk radius at chest height, root reach, the branch ends at the top (position and outward direction) where
the crown and lantern blossoms are hung, and surface spots (position and normal) for moss on top and ivy on the trunk.

    blender -b --factory-startup -P vael_baum_vermessen.py -- <Ordner>
"""
import json
import math
import os
import sys

import bpy
from mathutils import Vector


def main(ordner):
    with open(os.path.join(ordner, "requisite.json"), encoding="utf-8") as f:
        name = json.load(f)["name"]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=os.path.join(ordner, "04_export", f"SM_{name}.fbx"))
    obj = next(o for o in bpy.data.objects if o.type == "MESH")
    punkte = [obj.matrix_world @ v.co for v in obj.data.vertices]
    hoehe = max(p.z for p in punkte)

    def radius_bei(z, band):
        r = [math.hypot(p.x, p.y) for p in punkte if abs(p.z - z) < band]
        return sorted(r)[int(len(r) * 0.9)] if r else 0.0

    # Branch ends: in every 20 degree slice the point of the upper third farthest from the axis
    oben = [p for p in punkte if p.z > hoehe * 0.6]
    scheiben = {}
    for p in oben:
        winkel = int(math.degrees(math.atan2(p.y, p.x)) % 360 // 20)
        guete = math.hypot(p.x, p.y) + p.z * 0.5
        if winkel not in scheiben or guete > scheiben[winkel][0]:
            scheiben[winkel] = (guete, p)
    kandidaten = sorted((v[1] for v in scheiben.values()), key=lambda p: -(math.hypot(p.x, p.y) + p.z * 0.5))
    aeste = []
    for p in kandidaten:
        if all((p - q).length > hoehe * 0.12 for q in aeste):
            aeste.append(p)
    # Direction of each branch: from the trunk axis at that height to the end, slightly upward
    ende = []
    for p in aeste[:8]:
        richtung = Vector((p.x, p.y, max(0.3, (p.z - hoehe * 0.55) / max(1e-3, math.hypot(p.x, p.y))))).normalized()
        ende.append({"ort_cm": [round(p.x * 100, 1), round(p.y * 100, 1), round(p.z * 100, 1)],
                     "richtung": [round(richtung.x, 3), round(richtung.y, 3), round(richtung.z, 3)]})

    wurzeln = max(math.hypot(p.x, p.y) for p in punkte if p.z < hoehe * 0.05)

    # Surface spots, picked by area: where moss lies (faces looking up) and where ivy climbs (trunk sides)
    import random
    zufall = random.Random(48)
    stamm_r = radius_bei(hoehe * 0.3, hoehe * 0.03)
    flaechen = []
    for poly in obj.data.polygons:
        mitte = obj.matrix_world @ poly.center
        normale = (obj.matrix_world.to_3x3() @ poly.normal).normalized()
        flaechen.append((poly.area, mitte, normale))

    def ziehen(auswahl, anzahl):
        gewicht = sum(f[0] for f in auswahl)
        if not auswahl or gewicht <= 0:
            return []
        out = []
        for _ in range(anzahl):
            ziel, summe = zufall.random() * gewicht, 0.0
            for f in auswahl:
                summe += f[0]
                if summe >= ziel:
                    out.append({"ort_cm": [round(c * 100, 1) for c in f[1]], "normale": [round(c, 3) for c in f[2]]})
                    break
        return out

    moos = ziehen([f for f in flaechen if f[2].z > 0.5 and f[1].z > 0.4], 220)
    efeu = ziehen([f for f in flaechen if abs(f[2].z) < 0.35 and 1.0 < f[1].z < hoehe * 0.7
                   and math.hypot(f[1].x, f[1].y) < stamm_r * 1.6], 110)
    daten = {
        "name": name,
        "hoehe_cm": round(hoehe * 100, 1),
        "stamm_radius_cm": round(radius_bei(hoehe * 0.3, hoehe * 0.03) * 100, 1),
        "wurzel_reichweite_cm": round(wurzeln * 100, 1),
        "astenden": ende,
        "moos_punkte": moos,
        "efeu_punkte": efeu,
    }
    os.makedirs(os.path.join(ordner, "03_analyse"), exist_ok=True)
    with open(os.path.join(ordner, "03_analyse", "baum.json"), "w", encoding="utf-8") as f:
        json.dump(daten, f, indent=2)
    print("[Vael]", json.dumps({k: v for k, v in daten.items() if not k.endswith("_punkte")}),
          len(moos), "moss spots,", len(efeu), "ivy spots")


if __name__ == "__main__":
    main(sys.argv[sys.argv.index("--") + 1])
