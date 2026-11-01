"""
Pflanzenarm Harzkolben: cuts the real hood, spadix and a leaf out of the Tripo model, so the hand keeps its torn, mottled look.

The model is one welded piece. The spadix is found by its orange resin and its distance from the axis of that resin, the hood
is what stays above CUT_HOOD around that axis, the leaf the largest piece further out. All are reduced and stand at the pivot:

- SM_Harzkolben_Kapuze: the hood, foot at the pivot, 10 cm high on +Z, its inside facing -X (the arm closes them into a flower)
- SM_Harzkolben_Kolben: the spadix, foot at the pivot, 10 cm high on +Z (several form the barrel along the aim)
- SM_Harzkolben_Blatt: a leaf, stalk end at the pivot, tip on +Z, 10 cm high

Ramp 0.45 everywhere: near the tip the material would flash the whole piece while firing. The source file is only read.

Run:
  blender -b --factory-startup -P harzkolben_teile.py -- <source fbx> <basecolor png> <output folder>
"""
import bmesh
import bpy
import math
import numpy as np
import os
import sys
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
SOURCE, PICTURE, OUT = args[0], args[1], args[2]

CUT_HOOD = 0.42        # share of the height above which the hood stands
SPADIX_RADIUS = 0.105  # m around the axis of the resin
HOOD_RADIUS = 0.50     # m around that axis
LEAF_RADIUS = 0.52     # m: beyond this only leaves
SIZE = 0.10            # m, height of every piece
RAMP = 0.45
FACES = {"Kapuze": 1400, "Kolben": 500, "Blatt": 500}
os.makedirs(OUT, exist_ok=True)


def log(text):
    print("TEILE " + text, flush=True)


def copy_faces(model, keep, name):
    """A new object with the faces of the model marked in keep, its largest connected piece only"""
    bm = bmesh.new()
    bm.from_mesh(model.data)
    bm.faces.ensure_lookup_table()
    origin = bm.faces.layers.int.new("Herkunft")
    for face in bm.faces:
        face[origin] = face.index
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if not keep[f.index]], context="FACES")
    # The model is split along its picture: weld it to ask what hangs together, but cut the piece from the unwelded model,
    # so reducing it cannot smear the picture across the seams
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
    mesh = bpy.data.meshes.new("SM_Harzkolben_" + name)
    bm.to_mesh(mesh)
    bm.free()
    for material in model.data.materials:
        mesh.materials.append(material)
    obj = bpy.data.objects.new(mesh.name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def reduce(obj, faces):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    modifier = obj.modifiers.new("Weniger", "DECIMATE")
    modifier.ratio = min(1.0, faces / max(1, len(obj.data.polygons)))
    bpy.ops.object.modifier_apply(modifier=modifier.name)


def stand(obj, turn):
    """Foot at the pivot, the given turn, SIZE high"""
    mesh = obj.data
    mesh.transform(turn)
    points = np.array([v.co for v in mesh.vertices])
    # Pivot under the middle of the piece, so it stands around the hand; a few stray points don't count for the height
    low, high = np.percentile(points[:, 2], 1.0), np.percentile(points[:, 2], 99.5)
    middle = np.median(points, axis=0)
    mesh.transform(Matrix.Scale(SIZE / (high - low), 4) @ Matrix.Translation((-middle[0], -middle[1], -low)))
    mesh.update()


def paint(obj):
    mesh = obj.data
    colours = mesh.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    ramp = mesh.uv_layers.new(name="Verlauf")
    for loop in mesh.loops:
        colours.data[loop.index].color = (RAMP, RAMP, RAMP, 1.0)
        ramp.data[loop.index].uv = (RAMP, RAMP)


def export(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                             mesh_smooth_type="FACE", colors_type="LINEAR", path_mode="COPY", embed_textures=False)
    log("{}: {} faces, {}".format(obj.name, len(obj.data.polygons), path))


bpy.ops.import_scene.fbx(filepath=SOURCE)
model = next(o for o in bpy.context.scene.objects if o.type == "MESH" and o.name.startswith("SM_"))
bpy.context.view_layer.objects.active = model
model.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
mesh = model.data

image = bpy.data.images.load(PICTURE)
width, height = image.size
pixels = np.array(image.pixels[:], dtype=np.float32).reshape(height, width, -1)
uv = mesh.uv_layers[0].data
count = len(mesh.polygons)
centre = np.zeros((count, 3))
colour = np.zeros((count, 3))
for index, polygon in enumerate(mesh.polygons):
    centre[index] = polygon.center
    point = sum((uv[i].uv for i in polygon.loop_indices), Vector((0.0, 0.0))) / polygon.loop_total
    colour[index] = pixels[min(int((point.y % 1.0) * height), height - 1), min(int((point.x % 1.0) * width), width - 1), :3]

low, top = centre[:, 2].min(), centre[:, 2].max()
share = (centre[:, 2] - low) / (top - low)
resin = (colour[:, 0] - colour[:, 2] > 0.4) & (share > 0.5)
axis = centre[resin, :2].mean(axis=0)
log("resin faces {}, axis {:.3f} {:.3f}".format(int(resin.sum()), axis[0], axis[1]))
radial = np.hypot(centre[:, 0] - axis[0], centre[:, 1] - axis[1])

spadix = (radial < SPADIX_RADIUS) & (share > 0.40)
hood = (share > CUT_HOOD) & (radial < HOOD_RADIUS) & ~spadix
leaf = radial > LEAF_RADIUS

# The hood opens along -Y; the arm wants its inside on -X
turn_hood = Matrix.Rotation(math.radians(-90.0), 4, "Z")
pieces = []
for name, keep, turn in (("Kapuze", hood, turn_hood), ("Kolben", spadix, Matrix.Identity(4))):
    piece = copy_faces(model, keep, name)
    pieces.append((name, piece, turn))

leaf_piece = copy_faces(model, leaf, "Blatt")
points = np.array([v.co for v in leaf_piece.data.vertices])
middle = points.mean(axis=0)
_, _, axes = np.linalg.svd(points - middle)
along = Vector(axes[0].tolist())
# Towards the tip, away from the plant
if along.dot(Vector((middle[0] - axis[0], middle[1] - axis[1], 0.0))) < 0.0:
    along = -along
pieces.append(("Blatt", leaf_piece, along.rotation_difference(Vector((0.0, 0.0, 1.0))).to_matrix().to_4x4()))

for name, piece, turn in pieces:
    reduce(piece, FACES[name])
    stand(piece, turn)
    paint(piece)
    export(piece)
log("done")
