"""
Pflanzenarm Mondtraufe: the stalks for the Floromant's Mondtraufe arm, and the piece of the real stalk for ranke_bauen.py.
The Tripo bud hangs from one short stalk fused to its collar, too short for halme_zerlegen.py; so the stalk part of the
model (above the collar) is cut out only to learn where its picture lies, and the arm's stalks are grown as slender, gently
bent tubes with their texture coordinates in that part of the picture: mossy dark stalk, as on the plant. Like the cut stalks
they stand on +Z (100 cm, pivot at the base) and carry the ramp 0 at the base to 1 at the tip in vertex colour R and UV 1.
The source file is only read.
Run:
  blender -b -P mondtraufe_teile.py -- <SM_Mondtraufe.fbx> <output folder>
Writes SM_Mondtraufe_Stiel.fbx (the real stalk, for ranke_bauen.py) and SM_Mondtraufe_Halm_1/2.fbx.
"""
import bpy, bmesh, sys, os, math, random
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
SOURCE, OUT = args[0], args[1]
STALK_SHARE = 0.8      # the stalk is the part of the model above this share of its height
LENGTH = 1.0           # m
SIDES = 8
RINGS = 30

os.makedirs(OUT, exist_ok=True)
random.seed(5)


def log(text):
    print("MONDTRAUFE " + text, flush=True)


def export(obj, name):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                             mesh_smooth_type="FACE", colors_type="LINEAR", path_mode="COPY", embed_textures=False, add_leaf_bones=False)
    log(f"exported {path}")


def cut_stalk():
    """The real stalk: the faces above the collar, moved down to stand on 0"""
    bpy.ops.import_scene.fbx(filepath=SOURCE)
    obj = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    top = max(v.co.z for v in bm.verts)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if sum(v.co.z for v in f.verts) / len(f.verts) < top * STALK_SHARE], context="FACES")
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context="VERTS")
    low = min(v.co.z for v in bm.verts)
    for v in bm.verts:
        v.co.z -= low
    bm.to_mesh(obj.data)
    bm.free()
    obj.name = obj.data.name = "SM_Mondtraufe_Stiel"
    log(f"stalk piece {len(obj.data.vertices)} verts")
    return obj


def stem_region(obj):
    mesh = obj.data
    uv = mesh.uv_layers[0].data
    us = sorted(uv[l.index].uv.x for l in mesh.loops)
    vs = sorted(uv[l.index].uv.y for l in mesh.loops)
    centre = Vector((us[len(us) // 2], vs[len(vs) // 2]))
    spread = Vector((max(us[int(len(us) * 0.65)] - us[int(len(us) * 0.35)], 0.01), max(vs[int(len(vs) * 0.65)] - vs[int(len(vs) * 0.35)], 0.01)))
    return centre, spread


def stalk(name, material, centre, spread, bend, base_radius, tip_radius):
    """A slender stalk along +Z, bending gently to one side and a little back, unevenly thick like grown wood"""
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UVMap")
    phase = random.uniform(0.0, math.tau)
    rings = []
    for i in range(RINGS + 1):
        t = i / RINGS
        point = Vector((bend * math.sin(t * math.pi * 0.8), bend * 0.3 * math.sin(t * math.pi * 1.6 + phase), t * LENGTH))
        radius = (base_radius + (tip_radius - base_radius) * t) * (1.0 + 0.12 * math.sin(t * 17.0 + phase))
        ring = []
        for s in range(SIDES):
            a = s / SIDES * math.tau
            ring.append(bm.verts.new(point + Vector((math.cos(a), math.sin(a), 0.0)) * radius))
        rings.append(ring)
    for i in range(RINGS):
        for s in range(SIDES):
            face = bm.faces.new((rings[i][s], rings[i][(s + 1) % SIDES], rings[i + 1][(s + 1) % SIDES], rings[i + 1][s]))
            for k, loop in enumerate(face.loops):
                ring_index = i + (1 if k >= 2 else 0)
                side_index = s + (1 if k in (1, 2) else 0)
                loop[uv_layer].uv = (centre.x + (side_index / SIDES - 0.5) * spread.x, centre.y + ((ring_index / RINGS) * 2.0 % 1.0 - 0.5) * spread.y)
    tip = bm.verts.new(Vector((bend * math.sin(math.pi * 0.8), bend * 0.3 * math.sin(math.pi * 1.6 + phase), LENGTH + tip_radius)))
    for s in range(SIDES):
        face = bm.faces.new((rings[-1][s], rings[-1][(s + 1) % SIDES], tip))
        for loop in face.loops:
            loop[uv_layer].uv = (centre.x, centre.y)
    bm.to_mesh(mesh)
    bm.free()
    if material is not None:
        mesh.materials.append(material)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    colours = mesh.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    ramp_uv = mesh.uv_layers.new(name="Verlauf")
    for loop in mesh.loops:
        t = max(0.0, min(1.0, mesh.vertices[loop.vertex_index].co.z / LENGTH))
        colours.data[loop.index].color = (t, t, t, 1.0)
        ramp_uv.data[loop.index].uv = (t, t)
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    return obj


bpy.ops.wm.read_factory_settings(use_empty=True)
piece = cut_stalk()
centre, spread = stem_region(piece)
log(f"stem uv centre {centre.x:.3f},{centre.y:.3f} spread {spread.x:.3f},{spread.y:.3f}")
export(piece, "SM_Mondtraufe_Stiel")
material = piece.data.materials[0] if piece.data.materials else None
for index, (bend, base_radius, tip_radius) in enumerate(((0.05, 0.013, 0.005), (-0.07, 0.011, 0.004)), start=1):
    export(stalk(f"SM_Mondtraufe_Halm_{index}", material, centre, spread, bend, base_radius, tip_radius), f"SM_Mondtraufe_Halm_{index}")
log("done")
