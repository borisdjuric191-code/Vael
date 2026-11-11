"""The star a glint is drawn with (UVaelSparkleComponent): a flat four-pointed star of 1 m with four short rays between,
standing in the YZ plane so its face looks along X. U of its UVs is the distance from the middle (0) to a tip (1):
the material M_Funkeln lets it fade outward. Written to Vael_Quellen/_Effekte/SM_Funkeln.fbx.

    blender -b --factory-startup -P Tools/Kreaturenfabrik/blender/vael_funkeln.py
"""
import math
import os

import bmesh
import bpy

QUELLEN = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))),
                       "Vael_Quellen")


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")

    def strahl(winkel, laenge, breite):
        """One ray: a thin kite from the middle to its tip"""
        c, s = math.cos(winkel), math.sin(winkel)
        mitte = bm.verts.new((0.0, 0.0, 0.0))
        links = bm.verts.new((0.0, -s * breite + c * breite * 2.0, c * breite + s * breite * 2.0))
        spitze = bm.verts.new((0.0, c * laenge, s * laenge))
        rechts = bm.verts.new((0.0, s * breite + c * breite * 2.0, -c * breite + s * breite * 2.0))
        f = bm.faces.new((mitte, links, spitze, rechts))
        for loop, u in zip(f.loops, (0.0, 0.25, 1.0, 0.25)):
            loop[uv].uv = (u, 0.5)

    for k in range(4):
        strahl(k * math.pi / 2, 0.5, 0.06)
        strahl(k * math.pi / 2 + math.pi / 4, 0.24, 0.045)
    # a small bright heart
    herz = [bm.verts.new((0.0, 0.15 * math.cos(a), 0.15 * math.sin(a))) for a in (i * math.pi / 4 for i in range(8))]
    mitte = bm.verts.new((0.0, 0.0, 0.0))
    for i in range(8):
        f = bm.faces.new((mitte, herz[i], herz[(i + 1) % 8]))
        for loop, u in zip(f.loops, (0.0, 0.5, 0.5)):
            loop[uv].uv = (u, 0.5)

    mesh = bpy.data.meshes.new("SM_Funkeln")
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new("SM_Funkeln", mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    ziel = os.path.join(QUELLEN, "_Effekte")
    os.makedirs(ziel, exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=os.path.join(ziel, "SM_Funkeln.fbx"), use_selection=True, object_types={"MESH"},
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL", bake_anim=False)
    print("[Funkeln] exportiert:", os.path.join(ziel, "SM_Funkeln.fbx"))


main()
