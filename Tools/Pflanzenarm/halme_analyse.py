"""Analyse of a Tripo plant model: splits it into loose parts and prints their size, so single stalks can be picked.
Run: blender -b -P halme_analyse.py -- <fbx>"""
import bpy, sys
from mathutils import Vector

fbx = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
for o in meshes:
    print("OBJ", o.name, len(o.data.vertices), "verts", tuple(round(d, 3) for d in o.dimensions), "scale", tuple(o.scale))
bpy.ops.object.select_all(action="DESELECT")
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
obj = bpy.context.view_layer.objects.active
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.separate(type="LOOSE")
bpy.ops.object.mode_set(mode="OBJECT")
parts = [o for o in bpy.context.scene.objects if o.type == "MESH"]
rows = []
for o in parts:
    vs = [o.matrix_world @ v.co for v in o.data.vertices]
    lo = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
    hi = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
    rows.append((len(vs), lo, hi))
rows.sort(key=lambda r: -r[0])
print("PARTS", len(rows))
for n, lo, hi in rows[:40]:
    d = hi - lo
    print(f"PART verts={n} size=({d.x:.3f},{d.y:.3f},{d.z:.3f}) zlo={lo.z:.3f} zhi={hi.z:.3f}")
