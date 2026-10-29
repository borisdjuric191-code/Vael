"""Step 1: imports the Tripo FBX, joins it into one mesh, turns the flower head to +X (Unreal's forward), scales it
so the shoulder stands SCHULTER_M high, puts the origin on the ground below the middle of the feet and applies all
transforms. Saves arbeit/01_vorbereitet.blend and renders previews (vorschau/01_*).

    blender -b --factory-startup -P Tools/Kelchreisser/01_vorbereiten.py
"""
import glob
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import vorschau  # noqa: E402

# Boris 2026-10-29: much bigger than everything else in the Wurzelforst, almost a boss (4-5 m shoulder)
SCHULTER_M = 4.5
# Turn round Z after the import so the flower head looks to +X (read off vorschau/00_*)
DREHUNG_Z_GRAD = 90.0
# Height of the shoulder hump (without spikes) as a share of the whole model's height (vine tips included), read off
# vorschau/00_seite; None for a first look (renders the five views only, nothing scaled or saved)
SCHULTER_ANTEIL = 0.70


def laden():
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
    obj.name = obj.data.name = "Kelchreisser"
    return obj


def grenzen(obj):
    punkte = [v.co for v in obj.data.vertices]
    lo = Vector((min(p.x for p in punkte), min(p.y for p in punkte), min(p.z for p in punkte)))
    hi = Vector((max(p.x for p in punkte), max(p.y for p in punkte), max(p.z for p in punkte)))
    return lo, hi


def main():
    obj = laden()
    obj.data.transform(Matrix.Rotation(math.radians(DREHUNG_Z_GRAD), 4, "Z"))
    lo, hi = grenzen(obj)
    obj.data.transform(Matrix.Translation(-Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))))
    lo, hi = grenzen(obj)
    groesse = hi - lo
    print("[Kelchreisser] %d Vertices, %d Flaechen, roh %.3f x %.3f x %.3f" % (
        len(obj.data.vertices), len(obj.data.polygons), groesse.x, groesse.y, groesse.z))
    if SCHULTER_ANTEIL is None:
        mitte = (0.0, 0.0, groesse.z / 2)
        weit = max(groesse) * 1.1
        for name, richtung in (("00_plus_x", (1, 0, 0.05)), ("00_minus_x", (-1, 0, 0.05)),
                               ("00_seite", (0, -1, 0.05)), ("00_plus_y", (0, 1, 0.05)), ("00_oben", (0, 0, 1))):
            vorschau.rendern(name, richtung, mitte, weit)
        return

    s = SCHULTER_M / (groesse.z * SCHULTER_ANTEIL)
    obj.data.transform(Matrix.Scale(s, 4))
    lo, hi = grenzen(obj)
    groesse = hi - lo
    print("[Kelchreisser] Groesse %.2f x %.2f x %.2f m" % tuple(groesse))
    os.makedirs(vorschau.ARBEIT, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(vorschau.ARBEIT, "01_vorbereitet.blend"))
    vorschau.standard_ansichten("01", (0.0, 0.0, groesse.z / 2), max(groesse.x, groesse.z) * 1.1)


main()
