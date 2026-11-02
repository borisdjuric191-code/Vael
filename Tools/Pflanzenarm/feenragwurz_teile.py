"""
Pflanzenarm Feenragwurz: cuts a real blossom out of the Tripo model, so the arm keeps its holes, spots and moth-wing look.

The blossom is what lies around its middle in the front view, the largest welded piece of it. It is reduced and stands with
the pivot under its middle, 10 cm high on +Z. It keeps its UV maps; the arm draws it with the material of the
plant. It looks along +X: on the arm the outside of a piece faces away from the hand. The source file is only read.

Run:
  blender -b --factory-startup -P feenragwurz_teile.py -- <source fbx> <output folder>
"""
import bmesh
import bpy
import math
import numpy as np
import os
import sys
from mathutils import Matrix

args = sys.argv[sys.argv.index("--") + 1:]
SOURCE, OUT = args[0], args[1]

BLOSSOM = (-0.36, 1.21)   # x, z in m of a whole, open blossom, as in vael_feenragwurz_takt.py
RADIUS = 0.30             # m around it in the front view
SIZE = 0.10               # m high
FACES = 1800
os.makedirs(OUT, exist_ok=True)

bpy.ops.import_scene.fbx(filepath=SOURCE)
model = next(o for o in bpy.context.scene.objects if o.type == "MESH" and o.name.startswith("SM_"))
bpy.context.view_layer.objects.active = model
model.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

centre = np.array([p.center for p in model.data.polygons])
keep = np.hypot(centre[:, 0] - BLOSSOM[0], centre[:, 2] - BLOSSOM[1]) < RADIUS

bm = bmesh.new()
bm.from_mesh(model.data)
bm.faces.ensure_lookup_table()
origin = bm.faces.layers.int.new("Herkunft")
for face in bm.faces:
    face[origin] = face.index
bmesh.ops.delete(bm, geom=[f for f in bm.faces if not keep[f.index]], context="FACES")
# The model is split along its picture: weld to ask what hangs together, cut from the unwelded model
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.003)
seen, best = set(), []
for start in bm.faces:
    if start in seen:
        continue
    group, stack = [], [start]
    seen.add(start)
    while stack:
        face = stack.pop()
        group.append(face)
        for edge in face.edges:
            for other in edge.link_faces:
                if other not in seen:
                    seen.add(other)
                    stack.append(other)
    if len(group) > len(best):
        best = group
wanted = set(face[origin] for face in best)
bm.free()

bm = bmesh.new()
bm.from_mesh(model.data)
bm.faces.ensure_lookup_table()
bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index not in wanted], context="FACES")
mesh = bpy.data.meshes.new("SM_Feenragwurz_Bluete")
bm.to_mesh(mesh)
bm.free()
for material in model.data.materials:
    mesh.materials.append(material)
piece = bpy.data.objects.new(mesh.name, mesh)
bpy.context.scene.collection.objects.link(piece)

bpy.ops.object.select_all(action="DESELECT")
piece.select_set(True)
bpy.context.view_layer.objects.active = piece
modifier = piece.modifiers.new("Weniger", "DECIMATE")
modifier.ratio = min(1.0, FACES / max(1, len(mesh.polygons)))
bpy.ops.object.modifier_apply(modifier=modifier.name)

# The blossom opens along -Y; the arm wants it along +X
mesh.transform(Matrix.Rotation(math.radians(90.0), 4, "Z"))
points = np.array([v.co for v in mesh.vertices])
low, high = np.percentile(points[:, 2], 1.0), np.percentile(points[:, 2], 99.0)
middle = np.median(points, axis=0)
mesh.transform(Matrix.Scale(SIZE / (high - low), 4) @ Matrix.Translation((-middle[0], -middle[1], -low)))
mesh.update()

path = os.path.join(OUT, mesh.name + ".fbx")
bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                         mesh_smooth_type="FACE", colors_type="LINEAR", path_mode="COPY", embed_textures=False)
print("TEILE {} faces {}, {}".format(mesh.name, len(mesh.polygons), path), flush=True)
