"""Front and side picture of the Tripo Aschenjaeger with a grid every 0.05 m (orthographic, Workbench), for finding its joints.
The figure is set on the ground (lowest point z 0) and centred in x/y first, as the rig script does.
  blender -b --factory-startup -P vael_aschenjaeger_raster.py"""
import bpy, math, os
from mathutils import Vector

SRC = "C:/Projekte/Vael_Quellen/Ingenieur/Aschenjaeger/02_tripo/Aschenjaeger.glb"
OUT = "C:/Projekte/Vael_Quellen/Ingenieur/Aschenjaeger/03_blender"
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=SRC)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
pts = [o.matrix_world @ v.co for o in meshes for v in o.data.vertices]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
print("ASJ objects", [(o.name, len(o.data.vertices), [s.material.name for s in o.material_slots if s.material]) for o in meshes])
print("ASJ bbox", [round(c, 3) for c in lo], [round(c, 3) for c in hi])
shift = Vector((-(lo.x + hi.x) / 2, -(lo.y + hi.y) / 2, -lo.z))
for o in meshes:
    o.data.transform(o.matrix_world)
    o.matrix_world.identity()
    o.data.transform(__import__("mathutils").Matrix.Translation(shift))
H = hi.z - lo.z
W = (hi.x - lo.x) / 2 + 0.05
D = (hi.y - lo.y) / 2 + 0.05
print("ASJ height", round(H, 3), "half width", round(W, 3), "half depth", round(D, 3))
grid = bpy.data.materials.new("Raster"); grid.diffuse_color = (1, 0.2, 0.1, 1)
grid2 = bpy.data.materials.new("Raster2"); grid2.diffuse_color = (0.2, 0.6, 1, 1)


def bar(a, b, mat, w=0.0015 * H):
    bpy.ops.mesh.primitive_cube_add(size=1)
    o = bpy.context.active_object
    o.location = [(a[i] + b[i]) / 2 for i in range(3)]
    o.scale = [max(abs(a[i] - b[i]), w) for i in range(3)]
    o.data.materials.append(mat)


step = 0.05
n = int(H / step) + 1
for k in range(0, n + 1):
    z = k * step
    m = grid if k % 2 == 0 else grid2
    bar((-W, -D, z), (W, -D, z), m)
    bar((W, -D, z), (W, D, z), m)
for k in range(-int(W / step), int(W / step) + 1):
    bar((k * step, -D, 0), (k * step, -D, H), grid if k % 2 == 0 else grid2)
for k in range(-int(D / step), int(D / step) + 1):
    bar((W, k * step, 0), (W, k * step, H), grid if k % 2 == 0 else grid2)
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "TEXTURE"
scene.render.resolution_x, scene.render.resolution_y = 1000, 1400
cam_data = bpy.data.cameras.new("C"); cam_data.type = "ORTHO"; cam_data.ortho_scale = H * 1.08
cam = bpy.data.objects.new("C", cam_data); scene.collection.objects.link(cam); scene.camera = cam
for name, loc, rot in (("vorne", (0, -3 * H, H / 2), (math.radians(90), 0, 0)), ("seite", (3 * H, 0, H / 2), (math.radians(90), 0, math.radians(90)))):
    cam.location = loc; cam.rotation_euler = rot
    scene.render.filepath = os.path.join(OUT, "raster_" + name + ".png")
    bpy.ops.render.render(write_still=True)
print("ASJ RASTER done")
