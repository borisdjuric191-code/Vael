"""
Pflanzenarm Rußrose: cuts the blossom out of the Tripo model of the Rußrose for the big rose on the Floromant's shoulder
(Boris 2026-10-26: the rose sits large on the shoulder, the whole arm becomes whipping thorn vines).

The model is one welded piece (blossom, stem, leaves, moss), so everything above the cut height is the blossom. It is moved so
its pivot sits in the middle of its foot, its face looking up (+Z), and scaled to 10 cm across like the heads of the other arms.
Ramp 0.97 everywhere (the material treats it as the tip). The source file is only read, never written.

Run:
  blender -b -P russrose_bluete.py -- <source fbx> <output folder> [cut share]
Writes SM_Russrose_Bluete.fbx plus a preview image <output>/Russrose_bluete.png.
"""
import bpy, bmesh, sys, os, math
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
SOURCE, OUT = args[0], args[1]
CUT = float(args[2]) if len(args) > 2 else 0.62
NAME = "Russrose"
SIZE = 0.10   # m across

os.makedirs(OUT, exist_ok=True)


def log(text):
    print("BLUETE " + text, flush=True)


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SOURCE)
model = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
bpy.context.view_layer.objects.active = model
model.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

mesh = model.data
zs = [v.co.z for v in mesh.vertices]
low, high = min(zs), max(zs)
cut = low + (high - low) * CUT

# Keep only the faces wholly above the cut
bm = bmesh.new()
bm.from_mesh(mesh)
bmesh.ops.delete(bm, geom=[f for f in bm.faces if min(v.co.z for v in f.verts) < cut], context="FACES")
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
points = [v.co.copy() for v in bm.verts]
if not points:
    log("nothing above the cut")
    sys.exit(1)

# Pivot in the middle of the foot, 10 cm across
xs = [p.x for p in points]
ys = [p.y for p in points]
foot = Vector(((min(xs) + max(xs)) * 0.5, (min(ys) + max(ys)) * 0.5, min(p.z for p in points)))
across = max(max(xs) - min(xs), max(ys) - min(ys)) or 1.0
scale = SIZE / across
for v in bm.verts:
    v.co = (v.co - foot) * scale
bm.normal_update()
bm.to_mesh(mesh)
bm.free()
model.name = mesh.name = f"SM_{NAME}_Bluete"

colours = mesh.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
ramp_uv = mesh.uv_layers.new(name="Verlauf")
for loop in mesh.loops:
    colours.data[loop.index].color = (0.97, 0.97, 0.97, 1.0)
    ramp_uv.data[loop.index].uv = (0.97, 0.97)
log(f"{model.name}: {len(mesh.polygons)} faces, cut at {CUT:.2f}")

# Preview
scene = bpy.context.scene
model.scale = (8.0, 8.0, 8.0)
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
scene.collection.objects.link(cam)
cam.location = (0.0, -1.6, 0.9)
cam.rotation_euler = (math.radians(60), 0, 0)
cam.data.type = "ORTHO"
cam.data.ortho_scale = 1.2
scene.camera = cam
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.color_type = "TEXTURE"
scene.render.resolution_x, scene.render.resolution_y = 800, 800
scene.render.filepath = os.path.join(OUT, NAME + "_bluete.png")
bpy.ops.render.render(write_still=True)
model.scale = (1.0, 1.0, 1.0)

bpy.ops.object.select_all(action="DESELECT")
model.select_set(True)
path = os.path.join(OUT, model.name + ".fbx")
bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                         mesh_smooth_type="FACE", colors_type="LINEAR", path_mode="COPY", embed_textures=False, add_leaf_bones=False)
log(f"exported {path}")
