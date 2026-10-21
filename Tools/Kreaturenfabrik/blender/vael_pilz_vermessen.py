"""Measures a giant mushroom of the requisite pipeline.

Reads <Ordner>/04_export/SM_<Name>.fbx (origin bottom centre, metres, from the 'requisite' step), follows the stem from
the foot upwards slice by slice (centre and radius) and finds the underside of the cap. Writes
<Ordner>/03_analyse/pilz.json: height, cap underside, cap radius, stem axis and radius (for the tendril sway in the
material) and the stem profile (for the stem-only collision capsules), used by Scripts/BuildRiesenpilze.py.
(UCX_ hulls in the FBX were tried: the UE 5.8 import took them as visible geometry.)

    blender -b --factory-startup -P vael_pilz_vermessen.py -- <Ordner>
"""
import json
import math
import os
import sys

import bpy
from mathutils import Vector

SCHEIBEN = 40          # slices from the foot to the top


def main(ordner):
    with open(os.path.join(ordner, "requisite.json"), encoding="utf-8") as f:
        name = json.load(f)["name"]
    fbx = os.path.join(ordner, "04_export", f"SM_{name}.fbx")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=fbx)
    for o in [o for o in bpy.data.objects if o.name.startswith("UCX_")]:
        bpy.data.objects.remove(o, do_unlink=True)
    obj = next(o for o in bpy.data.objects if o.type == "MESH")
    punkte = [obj.matrix_world @ v.co for v in obj.data.vertices]
    hoehe = max(p.z for p in punkte)
    hut_radius = sorted(math.hypot(p.x, p.y) for p in punkte if p.z > hoehe * 0.6)[-1]
    band = hoehe / SCHEIBEN

    lagen = [[Vector((p.x, p.y)) for p in punkte if i * band <= p.z < (i + 1) * band] for i in range(SCHEIBEN)]

    def mitte_und_radius(liste):
        mitte = sum(liste, Vector((0.0, 0.0))) / len(liste)
        abstaende = sorted((q - mitte).length for q in liste)
        return mitte, abstaende[int(len(abstaende) * 0.9)]

    # Start where the slice is narrowest (only the stem, no foot sheath, no tendrils), then follow the stem up and down,
    # only looking near the last centre, so the tendrils further out never count
    start = min(range(SCHEIBEN // 8, SCHEIBEN * 2 // 3), key=lambda i: max((q.length for q in lagen[i]), default=1e9))
    mitte, radius = mitte_und_radius(lagen[start])
    stiel_radius = radius
    gefunden = {start: (mitte, radius)}
    for richtung in (1, -1):
        m, r = mitte, radius
        i = start + richtung
        while 0 <= i < SCHEIBEN:
            nah = [q for q in lagen[i] if (q - m).length < r * 1.6 + band]
            if not nah:
                break
            # The cap starts where the slice suddenly holds far more points than the stem (the gills)
            if richtung > 0:
                stiel_mengen = sorted(len(lagen[k]) for k in gefunden if k >= start)
                if len(lagen[i]) > 3 * stiel_mengen[len(stiel_mengen) // 2]:
                    break
            m, r = mitte_und_radius(nah)
            r = min(r, stiel_radius * 2.5)
            gefunden[i] = (m, r)
            i += richtung
    scheiben = [{"z0": i * band, "z1": (i + 1) * band, "mitte": gefunden[i][0], "radius": gefunden[i][1]} for i in sorted(gefunden)]
    hut_unterseite = scheiben[-1]["z1"]
    # Lowest tendril tip: above the narrowest stem slice (no tendrils reach down there), away from the stem, under the cap
    achse = {i: gefunden[i] for i in gefunden}
    spitzen = [p.z for p in punkte if start * band <= p.z < hut_unterseite
               and (Vector((p.x, p.y)) - achse[min(int(p.z / band), max(achse))][0]).length > stiel_radius * 1.8]
    faden_unten = (min(spitzen) if spitzen else start * band) - 0.2

    unten, oben = scheiben[0]["mitte"], scheiben[-1]["mitte"]
    daten = {
        "hoehe_cm": round(hoehe * 100, 1),
        "hut_unterseite_cm": round(hut_unterseite * 100, 1),
        "hut_radius_cm": round(hut_radius * 100, 1),
        "stiel_radius_cm": round(stiel_radius * 100, 1),
        "faden_unten_cm": round(faden_unten * 100, 1),
        "stiel_unten_cm": [round(unten.x * 100, 1), round(unten.y * 100, 1)],
        "stiel_oben_cm": [round(oben.x * 100, 1), round(oben.y * 100, 1)],
        # Stem from the foot up: height, centre and radius of every slice; Scripts/BuildRiesenpilze.py puts capsules along it
        "stiel_profil_cm": [[round(s["z0"] * 100, 1), round(s["z1"] * 100, 1), round(s["mitte"].x * 100, 1), round(s["mitte"].y * 100, 1),
                             round(s["radius"] * 100, 1)] for s in scheiben],
    }
    os.makedirs(os.path.join(ordner, "03_analyse"), exist_ok=True)
    with open(os.path.join(ordner, "03_analyse", "pilz.json"), "w", encoding="utf-8") as f:
        json.dump(daten, f, indent=2)
    print("[pilz]", json.dumps(daten))


if __name__ == "__main__":
    main(sys.argv[sys.argv.index("--") + 1])
