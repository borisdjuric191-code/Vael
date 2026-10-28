"""Step 1: imports the Tripo FBX, joins it into one mesh, turns it so the reared head points to +X (Unreal's forward)
and the body lies along X, scales it to LAENGE_M along its length on the ground, puts the origin on the ground below
the middle and applies all transforms. Saves arbeit/01_vorbereitet.blend and renders previews (vorschau/01_*).

    blender -b --factory-startup -P Tools/Pfuhlnatter/01_vorbereiten.py
"""
import glob
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import vorschau  # noqa: E402

# Extent along X of the grown snake as it lies in the Tripo pose (its spine is longer, measured in step 2)
LAENGE_M = 4.2


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    fbx = glob.glob(os.path.join(vorschau.ORDNER, "02_tripo", "*.fbx"))[0]
    bpy.ops.import_scene.fbx(filepath=fbx)
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    for o in bpy.data.objects:
        o.select_set(o in meshes)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    welt = obj.matrix_world.copy()
    obj.parent = None
    obj.matrix_world = welt
    for o in [o for o in bpy.data.objects if o.type != "MESH"]:
        bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    obj.name = obj.data.name = "Pfuhlnatter"

    punkte = [v.co.copy() for v in obj.data.vertices]
    boden = min(p.z for p in punkte)
    hoehe = max(p.z for p in punkte) - boden
    # Length axis: main direction of the low points (the body lying on the ground) in the ground plane
    unten = [p for p in punkte if p.z < boden + hoehe * 0.3]
    mx = sum(p.x for p in unten) / len(unten)
    my = sum(p.y for p in unten) / len(unten)
    sxx = sum((p.x - mx) ** 2 for p in unten)
    syy = sum((p.y - my) ** 2 for p in unten)
    sxy = sum((p.x - mx) * (p.y - my) for p in unten)
    winkel = 0.5 * math.atan2(2 * sxy, sxx - syy)
    achse = Vector((math.cos(winkel), math.sin(winkel), 0.0))
    # The head end: where the highest points (the crown) are
    oben = [p for p in punkte if p.z > boden + hoehe * 0.85]
    kopf = sum(oben, Vector()) / len(oben)
    if (kopf - Vector((mx, my, kopf.z))).dot(achse) < 0:
        achse = -achse
    obj.data.transform(Matrix.Rotation(-math.atan2(achse.y, achse.x), 4, "Z"))

    punkte = [v.co.copy() for v in obj.data.vertices]
    lo = Vector((min(p.x for p in punkte), min(p.y for p in punkte), min(p.z for p in punkte)))
    hi = Vector((max(p.x for p in punkte), max(p.y for p in punkte), max(p.z for p in punkte)))
    obj.data.transform(Matrix.Translation(-Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))))
    s = LAENGE_M / (hi.x - lo.x)
    obj.data.transform(Matrix.Scale(s, 4))
    punkte = [v.co.copy() for v in obj.data.vertices]
    groesse = [max(p[i] for p in punkte) - min(p[i] for p in punkte) for i in range(3)]
    print("[Pfuhlnatter] %d Vertices, %d Flaechen, Groesse %.2f x %.2f x %.2f m" % (
        len(obj.data.vertices), len(obj.data.polygons), groesse[0], groesse[1], groesse[2]))

    os.makedirs(vorschau.ARBEIT, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(vorschau.ARBEIT, "01_vorbereitet.blend"))
    vorschau.standard_ansichten("01", (0.0, 0.0, groesse[2] / 2), max(groesse[0], groesse[2]) * 1.1)


main()
