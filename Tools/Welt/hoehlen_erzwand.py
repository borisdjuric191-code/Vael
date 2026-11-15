"""
Zerschneidet ein Erz (Tripo-Modell, schon aufbereitet: Vael_Quellen/<...>/04_export/SM_<Name>.fbx) in Stücke für die Höhlenwände
des Eisenschlunds (Boris 15.11.: die Erze zerschneiden und überall in Wänden und Decken als Teil der Wandoptik verteilen, nicht
erntebar; die erntebaren Erze stehen eigens).

Jedes Stück zeigt mit +Z aus der Wand heraus; die Schnittfläche liegt bei Z = 0 und der Ursprung in ihrer Mitte. Im Spiel steckt
das Stück mit der Schnittfläche im Fels, man sieht nur, was herausragt. Die Schnittfläche bleibt offen (sie liegt im Fels).

  A  Spitzen: nur das obere Drittel, die Kristallspitzen brechen aus dem Fels
  B  Hälfte: das Erz liegt auf der Seite und ist der Länge nach halbiert, die Kristalle wachsen quer aus der Wand
  C  Ecke: schräg abgeschnitten, ein kleiner Brocken

Größe: das ganze Erz wird vorher auf 1 m (größte Ausdehnung) gebracht.

  blender -b -P Tools/Welt/hoehlen_erzwand.py -- <fbx> <Ausgabeordner> <Name>
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
SRC, OUT, NAME = args[0], args[1], args[2]

SCHNITTE = {
    # Drehung des Erzes (Achse, Grad), dann behalten, was über der Höhe liegt (Anteil der Höhe nach dem Drehen)
    "A": (("X", 0.0), 0.55),
    "B": (("Y", -90.0), 0.5),
    "C": (("Y", -45.0), 0.42),
}


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def load():
    bpy.ops.import_scene.fbx(filepath=SRC)
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return obj


def normalise(obj):
    pts = [v.co for v in obj.data.vertices]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    size = max(hi - lo)
    centre = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    m = Matrix.Scale(1.0 / size, 4) @ Matrix.Translation(-centre)
    obj.data.transform(m)


def cut(obj, axis, degrees, share, label):
    copy = obj.copy()
    copy.data = obj.data.copy()
    bpy.context.scene.collection.objects.link(copy)
    me = copy.data
    me.transform(Matrix.Rotation(math.radians(degrees), 4, axis))
    zs = [v.co.z for v in me.vertices]
    z0 = min(zs) + (max(zs) - min(zs)) * share
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(0, 0, z0), plane_no=(0, 0, 1),
                           clear_inner=True)
    bm.to_mesh(me)
    bm.free()
    pts = [v.co for v in me.vertices]
    cx = (min(p.x for p in pts) + max(p.x for p in pts)) / 2
    cy = (min(p.y for p in pts) + max(p.y for p in pts)) / 2
    me.transform(Matrix.Translation((-cx, -cy, -z0)))
    copy.name = "SM_HK_ErzWand_{}_{}".format(NAME, label)
    me.name = copy.name
    return copy


def export(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_ALL", bake_anim=False, mesh_smooth_type="FACE", path_mode="STRIP")
    print("[ErzWand]", path, len(obj.data.vertices), "Punkte")


reset()
os.makedirs(OUT, exist_ok=True)
whole = load()
normalise(whole)
for label, ((axis, deg), share) in SCHNITTE.items():
    export(cut(whole, axis, deg, share, label))
