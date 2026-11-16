"""Front and side picture of the Tripo Glutpanzer with a grid every 0.05 m (orthographic, Workbench), for finding its joints.
  blender -b --factory-startup -P vael_glutpanzer_raster.py"""
import bpy, math, os
SRC = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/02_tripo/Glutpanzer.fbx"
OUT = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/03_blender"
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC)
grid = bpy.data.materials.new("Raster"); grid.diffuse_color = (1, 0.2, 0.1, 1)
grid2 = bpy.data.materials.new("Raster2"); grid2.diffuse_color = (0.2, 0.6, 1, 1)
def bar(a, b, mat, w=0.0015):
    bpy.ops.mesh.primitive_cube_add(size=1)
    o = bpy.context.active_object
    o.location = [(a[i] + b[i]) / 2 for i in range(3)]
    o.scale = [max(abs(a[i] - b[i]), w) for i in range(3)]
    o.data.materials.append(mat)
for k in range(0, 21):
    z = k * 0.05
    m = grid if k % 2 == 0 else grid2
    bar((-0.32, -0.25, z), (0.32, -0.25, z), m)      # in front (for the front view)
    bar((0.33, -0.25, z), (0.33, 0.25, z), m)        # at the side (for the side view)
for k in range(-6, 7):
    x = k * 0.05
    bar((x, -0.25, 0), (x, -0.25, 1.0), grid if k % 2 == 0 else grid2)
    bar((0.33, x, 0), (0.33, x, 1.0), grid if k % 2 == 0 else grid2)
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "MATERIAL"
scene.render.resolution_x, scene.render.resolution_y = 900, 1300
cam_data = bpy.data.cameras.new("C"); cam_data.type = "ORTHO"; cam_data.ortho_scale = 1.1
cam = bpy.data.objects.new("C", cam_data); scene.collection.objects.link(cam); scene.camera = cam
for name, loc, rot in (("vorne", (0, -3, 0.5), (math.radians(90), 0, 0)), ("seite", (3, 0, 0.5), (math.radians(90), 0, math.radians(90)))):
    cam.location = loc; cam.rotation_euler = rot
    scene.render.filepath = os.path.join(OUT, "raster_" + name + ".png")
    bpy.ops.render.render(write_still=True)
print("RASTER done")
