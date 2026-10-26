"""
Pflanzenarm Glimmklee: builds the clover leaf that overgrows the shield arm (Boris 2026-10-26).

The Tripo model of the Glimmklee is one welded clump, so its leaves cannot be cut out. The leaf is built here instead and takes
its colours from the model's own picture: the texture coordinates lie in the part of the picture the model's leaves use
(the wide middle of the plant, away from its axis), so it shows their green with the golden rim.

- SM_Glimmklee_Blatt: three heart-shaped leaflets on a short stem, spread around +Z and cupped up a little, 10 cm high like the
  heads of the other arms. Pivot at the foot of the stem. Ramp 0.97 everywhere (the material treats it as the tip).

Run:
  blender -b -P glimmklee_blatt.py -- <source fbx> <output folder>
Writes the FBX plus a preview image <output>/Glimmklee_blatt.png.
"""
import bpy, bmesh, sys, os, math
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
SOURCE, OUT = args[0], args[1]
NAME = "Glimmklee"

SIZE = 0.10       # m
STEM = 0.035      # m
LEAFLET = 0.048   # m, from the middle to the notch
RIM = 40          # points around a leaflet

os.makedirs(OUT, exist_ok=True)


def log(text):
    print("BLATT " + text, flush=True)


def leaf_region():
    """Middle and spread of the picture where the model has its leaves: the wide middle height, away from the axis"""
    bpy.ops.import_scene.fbx(filepath=SOURCE)
    model = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
    bpy.context.view_layer.objects.active = model
    model.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    mesh = model.data
    uv = mesh.uv_layers[0].data
    zs = [v.co.z for v in mesh.vertices]
    low, high = min(zs), max(zs)
    height = (high - low) or 1.0
    xs = [v.co.x for v in mesh.vertices]
    ys = [v.co.y for v in mesh.vertices]
    middle = Vector(((min(xs) + max(xs)) * 0.5, (min(ys) + max(ys)) * 0.5))
    width = max(max(xs) - min(xs), max(ys) - min(ys)) or 1.0

    def is_leaf(co):
        share = (co.z - low) / height
        return 0.2 < share < 0.5 and (Vector((co.x, co.y)) - middle).length > width * 0.15

    points = [uv[loop.index].uv.copy() for loop in mesh.loops if is_leaf(mesh.vertices[loop.vertex_index].co)]
    us = sorted(p.x for p in points)
    vs = sorted(p.y for p in points)
    centre = Vector((us[len(us) // 2], vs[len(vs) // 2]))
    spread = Vector((max(us[int(len(us) * 0.7)] - us[int(len(us) * 0.3)], 0.01),
                     max(vs[int(len(vs) * 0.7)] - vs[int(len(vs) * 0.3)], 0.01)))
    log(f"leaf uv centre {centre.x:.3f},{centre.y:.3f} spread {spread.x:.3f},{spread.y:.3f} from {len(points)} corners")
    material = mesh.materials[0] if mesh.materials else None
    bpy.data.objects.remove(model)
    return centre, spread, material


def heart(t):
    """Classic heart curve, point at the bottom (-y) and the notch at the top (+y), about 1 wide"""
    x = 16.0 * math.sin(t) ** 3
    y = 13.0 * math.cos(t) - 5.0 * math.cos(2 * t) - 2.0 * math.cos(3 * t) - math.cos(4 * t)
    return x / 34.0, (y + 17.0) / 34.0


def build_leaf(centre, spread, material):
    mesh = bpy.data.meshes.new(f"SM_{NAME}_Blatt")
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UVMap")

    def uv_of(local_x, local_y):
        return (centre.x + local_x * spread.x * 0.4, centre.y + (local_y - 0.5) * spread.y * 0.4)

    # The stem: a thin square tube up to the middle of the leaflets
    stem_radius = 0.002
    base = [bm.verts.new((math.cos(a) * stem_radius, math.sin(a) * stem_radius, 0.0)) for a in (k * math.pi / 2 for k in range(4))]
    top = [bm.verts.new((math.cos(a) * stem_radius, math.sin(a) * stem_radius, STEM)) for a in (k * math.pi / 2 for k in range(4))]
    for k in range(4):
        face = bm.faces.new((base[k], base[(k + 1) % 4], top[(k + 1) % 4], top[k]))
        for loop in face.loops:
            loop[uv_layer].uv = uv_of(0.0, 0.05)

    # Three leaflets, each a heart with its point at the middle, lobes outward, cupped up towards their rim
    for leaflet in range(3):
        turn = leaflet * 2.0 * math.pi / 3.0
        out = Vector((math.cos(turn), math.sin(turn), 0.0))
        side = Vector((-math.sin(turn), math.cos(turn), 0.0))
        middle_vert = bm.verts.new((out * LEAFLET * 0.45 + Vector((0.0, 0.0, STEM + 0.004)))[:])
        rim = []
        for k in range(RIM):
            hx, hy = heart(k / RIM * 2.0 * math.pi)
            reach = hy * LEAFLET
            cup = 0.012 * (hy ** 2) + 0.01 * abs(hx)
            point = out * reach + side * hx * LEAFLET + Vector((0.0, 0.0, STEM + cup))
            rim.append((bm.verts.new(point[:]), hx, hy))
        for k in range(RIM):
            a, ax, ay = rim[k]
            b, bx, by = rim[(k + 1) % RIM]
            face = bm.faces.new((middle_vert, a, b))
            face.loops[0][uv_layer].uv = uv_of(0.0, 0.45)
            face.loops[1][uv_layer].uv = uv_of(ax, ay)
            face.loops[2][uv_layer].uv = uv_of(bx, by)

    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()
    if material is not None:
        mesh.materials.append(material)
    obj = bpy.data.objects.new(mesh.name, mesh)
    bpy.context.scene.collection.objects.link(obj)

    colours = mesh.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    ramp_uv = mesh.uv_layers.new(name="Verlauf")
    for loop in mesh.loops:
        colours.data[loop.index].color = (0.97, 0.97, 0.97, 1.0)
        ramp_uv.data[loop.index].uv = (0.97, 0.97)
    log(f"{obj.name}: {len(mesh.polygons)} faces")
    return obj


def preview(obj):
    scene = bpy.context.scene
    obj.scale = (8.0, 8.0, 8.0)
    obj.rotation_euler = (math.radians(55), 0.0, math.radians(20))
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    cam.location = (0.0, -2.0, 0.3)
    cam.rotation_euler = (math.radians(90), 0, 0)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 1.4
    scene.camera = cam
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 800, 800
    scene.render.filepath = os.path.join(OUT, NAME + "_blatt.png")
    bpy.ops.render.render(write_still=True)
    obj.scale = (1.0, 1.0, 1.0)
    obj.rotation_euler = (0.0, 0.0, 0.0)


def export(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                             mesh_smooth_type="FACE", colors_type="LINEAR", path_mode="COPY", embed_textures=False, add_leaf_bones=False)
    log(f"exported {path}")


bpy.ops.wm.read_factory_settings(use_empty=True)
leaf_centre, leaf_spread, picture = leaf_region()
leaf = build_leaf(leaf_centre, leaf_spread, picture)
preview(leaf)
export(leaf)
log("done")
