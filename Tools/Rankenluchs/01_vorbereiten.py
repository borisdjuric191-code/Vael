"""Step 1: imports the Tripo GLB, turns the lynx so its head points to +X (like Unreal), scales it to 0.70 m shoulder
height, puts the origin on the ground between the paws and applies all transforms. Saves arbeit/01_vorbereitet.blend
and renders previews (vorschau/01_*).

    blender -b --factory-startup -P skripte/01_vorbereiten.py
"""
import json
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import vorschau  # noqa: E402

ORDNER = vorschau.ORDNER
SCHULTERHOEHE_M = 0.70


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=os.path.join(ORDNER, "Rankenluchs_tripo.glb"))
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    for o in bpy.data.objects:
        o.select_set(o in meshes)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    # Free it from the glTF root empty, keeping its place
    welt = obj.matrix_world.copy()
    obj.parent = None
    obj.matrix_world = welt
    for o in [o for o in bpy.data.objects if o.type != "MESH"]:
        bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    obj.name = obj.data.name = "Rankenluchs"

    punkte = [v.co.copy() for v in obj.data.vertices]
    hoehe = max(p.z for p in punkte) - min(p.z for p in punkte)
    boden = min(p.z for p in punkte)

    # Body axis: main direction of the torso points (middle height band) in the ground plane
    rumpf = [p for p in punkte if boden + hoehe * 0.35 < p.z < boden + hoehe * 0.7]
    mx = sum(p.x for p in rumpf) / len(rumpf)
    my = sum(p.y for p in rumpf) / len(rumpf)
    sxx = sum((p.x - mx) ** 2 for p in rumpf)
    syy = sum((p.y - my) ** 2 for p in rumpf)
    sxy = sum((p.x - mx) * (p.y - my) for p in rumpf)
    winkel = 0.5 * math.atan2(2 * sxy, sxx - syy)
    achse = Vector((math.cos(winkel), math.sin(winkel), 0.0))
    # The head end: where the highest points (ears) are
    oben = [p for p in punkte if p.z > boden + hoehe * 0.85]
    kopf = sum(oben, Vector()) / len(oben)
    if (kopf - Vector((mx, my, kopf.z))).dot(achse) < 0:
        achse = -achse
    drehung = -math.atan2(achse.y, achse.x)
    obj.data.transform(Matrix.Rotation(drehung, 4, "Z"))
    print("[Rankenluchs] gedreht um %.1f Grad" % math.degrees(drehung))

    # Shoulder height: top of the back at the withers, behind the head, neck and leaf collar
    punkte = [v.co.copy() for v in obj.data.vertices]
    oben = [p for p in punkte if p.z > boden + hoehe * 0.85]
    kopf_x = min(p.x for p in oben)
    fuesse = [p for p in punkte if p.z < boden + hoehe * 0.06]
    vorn_x = max(p.x for p in fuesse)
    hinten_x = min(p.x for p in fuesse if p.x > min(p.x for p in punkte) + 0.05 * hoehe) if fuesse else 0.0
    # Front leg column: the forward third of the paws
    vorderbeine = [p for p in fuesse if p.x > vorn_x - (vorn_x - hinten_x) * 0.3]
    schulter_x = sum(p.x for p in vorderbeine) / len(vorderbeine)
    # The back line just behind the leaf collar (over the front legs the collar stands higher than the back)
    band = [p for p in punkte if abs(p.x - (schulter_x - 0.2 * hoehe)) < 0.03 * hoehe and p.x < kopf_x]
    schulter_z = max(p.z for p in band) - boden
    skala = SCHULTERHOEHE_M / schulter_z
    print("[Rankenluchs] Schulterhoehe gemessen %.3f, Gesamthoehe %.3f -> Massstab %.5f" % (schulter_z, hoehe, skala))

    # Origin on the ground between the paws
    fx = (vorn_x + hinten_x) / 2
    fy = sum(p.y for p in fuesse) / len(fuesse)
    obj.data.transform(Matrix.Translation(Vector((-fx, -fy, -boden))))
    obj.data.transform(Matrix.Scale(skala, 4))
    obj.location = (0, 0, 0)

    punkte = [v.co for v in obj.data.vertices]
    lo = Vector((min(p.x for p in punkte), min(p.y for p in punkte), min(p.z for p in punkte)))
    hi = Vector((max(p.x for p in punkte), max(p.y for p in punkte), max(p.z for p in punkte)))
    masse = {"laenge_m": round(hi.x - lo.x, 3), "breite_m": round(hi.y - lo.y, 3), "hoehe_m": round(hi.z - lo.z, 3),
             "schulterhoehe_m": SCHULTERHOEHE_M, "box_min": [round(v, 3) for v in lo], "box_max": [round(v, 3) for v in hi],
             "dreiecke": sum(len(p.vertices) - 2 for p in obj.data.polygons), "flaechen": len(obj.data.polygons)}
    print("[Rankenluchs] Masse", json.dumps(masse))
    os.makedirs(os.path.join(ORDNER, "arbeit"), exist_ok=True)
    with open(os.path.join(ORDNER, "arbeit", "masse.json"), "w", encoding="utf-8") as f:
        json.dump(masse, f, indent=1)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ORDNER, "arbeit", "01_vorbereitet.blend"))
    vorschau.standard_ansichten("01", mitte=((lo.x + hi.x) / 2, 0.0, hi.z / 2), groesse=max(hi.x - lo.x, hi.z) * 1.15)


main()
