"""Lumps of the Tropfseuche as static meshes, for whatever has no lumps of its own in its model (an infested player, later
Kobolde and Gildenleute): the game sticks them on the body bones in the colour of the strain.

Three shapes, 5 cm across before the game scales them, Z = away from the body. Writes
Vael_Quellen/Tropfseuche/export/SM_Tropfseuche_Klumpen_A/B/C.fbx.

    blender -b --factory-startup -P Tools/Nachtwesen/tropfseuche_klumpen.py
"""
import os
import sys

import bpy

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import gemein  # noqa: E402
import tropfseuche_formen as formen  # noqa: E402

ZIEL = gemein.ordner("Tropfseuche", "export")


def main():
    os.makedirs(ZIEL, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    material = bpy.data.materials.new("Seuche")
    for name, saat, flach, beulen in (("A", 1, 0.55, 0.35), ("B", 7, 0.7, 0.45), ("C", 13, 0.45, 0.3)):
        for o in list(bpy.data.objects):
            bpy.data.objects.remove(o, do_unlink=True)
        obj = formen.klumpen("SM_Tropfseuche_Klumpen_" + name, 0.025, flach=flach, saat=saat, unterteilung=3, beulen=beulen)
        bpy.context.scene.collection.objects.link(obj)
        obj.data.materials.append(material)
        for x in bpy.data.objects:
            x.select_set(x == obj)
        bpy.context.view_layer.objects.active = obj
        pfad = os.path.join(ZIEL, obj.name + ".fbx")
        bpy.ops.export_scene.fbx(filepath=pfad, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                                 apply_scale_options="FBX_SCALE_ALL", mesh_smooth_type="FACE")
        gemein.log("Tropfseuche", "exportiert", pfad, len(obj.data.polygons), "Flaechen")


main()
