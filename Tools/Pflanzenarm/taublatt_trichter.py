"""
Pflanzenarm Taublatt: builds the funnel leaf and the stems of the Taublatt arm (Boris 2026-10-24).

The Tripo model of the Taublatt is one welded clump, its funnels grown into each other, so halme_zerlegen.py only cuts
lumps out of it. The pieces are built here instead and take their colours from the model's own picture: the texture
coordinates of the funnel lie in the part of the picture the funnels of the model use, those of the stems in the part
their necks use. The source file is only read, never written.

- SM_Taublatt_Trichter: one funnel leaf, a narrow neck flaring into a wide cup whose rim is wavy and drawn out into a lip
  on one side (+X), a drop hanging at the lip. Pivot in the middle of the neck's foot, the opening up (+Z), 10 cm high like
  the heads of the other arms. Ramp 0.97 everywhere (the material treats it as the tip).
- SM_Taublatt_Halm_1/2: stems 100 cm long on +Z, gently bent, thin, with a few knots, ramp from 0 at the base to 1 at the tip.

Run:
  blender -b -P taublatt_trichter.py -- <source fbx> <output folder>
Writes the FBX files plus a preview image <output>/Taublatt_trichter.png.
"""
import bpy, bmesh, sys, os, math, random
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
SOURCE, OUT = args[0], args[1]
NAME = "Taublatt"

FUNNEL_HEIGHT = 0.10      # m
FUNNEL_RINGS = 22
FUNNEL_SIDES = 28
STEM_LENGTH = 1.0         # m, the arm stretches it along its spline
STEM_RINGS = 40
STEM_SIDES = 8

random.seed(11)
os.makedirs(OUT, exist_ok=True)


def log(text):
    print("TRICHTER " + text, flush=True)


def uv_regions():
    """Middle and spread of the picture where the model has its funnels (high up and wide) and its necks (lower, near the axis)"""
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

    def region(test, label):
        points = [uv[loop.index].uv.copy() for loop in mesh.loops if test(mesh.vertices[loop.vertex_index].co)]
        us = sorted(p.x for p in points)
        vs = sorted(p.y for p in points)
        centre = Vector((us[len(us) // 2], vs[len(vs) // 2]))
        spread = Vector((max(us[int(len(us) * 0.7)] - us[int(len(us) * 0.3)], 0.01),
                         max(vs[int(len(vs) * 0.7)] - vs[int(len(vs) * 0.3)], 0.01)))
        log(f"{label} uv centre {centre.x:.3f},{centre.y:.3f} spread {spread.x:.3f},{spread.y:.3f} from {len(points)} corners")
        return centre, spread

    funnel = region(lambda co: (co.z - low) / height > 0.6, "funnel")
    stem = region(lambda co: 0.25 < (co.z - low) / height < 0.5 and (Vector((co.x, co.y)) - middle).length < width * 0.12, "stem")
    material = mesh.materials[0] if mesh.materials else None
    bpy.data.objects.remove(model)
    return funnel, stem, material


def funnel_radius(t, angle):
    """Radius of the funnel at height share t (0 foot, 1 rim) around the angle; the rim waves and the lip side reaches out"""
    neck = 0.07
    flare = neck + 0.48 * (t ** 2.2)
    lip = 1.0 + 0.22 * max(0.0, math.cos(angle)) ** 3 * t ** 3
    wave = 1.0 + 0.06 * math.sin(angle * 5.0 + 0.7) * t ** 4
    return flare * lip * wave


def funnel_height(t, angle):
    """Height share of a point; the rim rises towards the lip and sinks a little on the far side"""
    return t + (0.16 * max(0.0, math.cos(angle)) ** 2 - 0.05 * max(0.0, -math.cos(angle))) * t ** 4


def build_funnel(region, material):
    centre, spread = region
    mesh = bpy.data.meshes.new(f"SM_{NAME}_Trichter")
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UVMap")

    rings = []
    for r in range(FUNNEL_RINGS + 1):
        t = r / FUNNEL_RINGS
        ring = []
        for s in range(FUNNEL_SIDES):
            angle = s / FUNNEL_SIDES * 2.0 * math.pi
            radius = funnel_radius(t, angle) * FUNNEL_HEIGHT
            z = funnel_height(t, angle) * FUNNEL_HEIGHT
            ring.append(bm.verts.new((math.cos(angle) * radius, math.sin(angle) * radius, z)))
        rings.append(ring)
    for r in range(FUNNEL_RINGS):
        for s in range(FUNNEL_SIDES):
            face = bm.faces.new((rings[r][s], rings[r][(s + 1) % FUNNEL_SIDES], rings[r + 1][(s + 1) % FUNNEL_SIDES], rings[r + 1][s]))
            for k, loop in enumerate(face.loops):
                ring_index = r + (1 if k >= 2 else 0)
                side_index = s + (1 if k in (1, 2) else 0)
                u = centre.x + (side_index / FUNNEL_SIDES - 0.5) * spread.x
                v = centre.y + (ring_index / FUNNEL_RINGS - 0.5) * spread.y
                loop[uv_layer].uv = (u, v)
    # The foot of the neck is closed
    foot = bm.verts.new((0.0, 0.0, 0.0))
    for s in range(FUNNEL_SIDES):
        face = bm.faces.new((rings[0][(s + 1) % FUNNEL_SIDES], rings[0][s], foot))
        for loop in face.loops:
            loop[uv_layer].uv = (centre.x, centre.y - spread.y * 0.5)

    # A drop hangs at the tip of the lip
    lip_angle = 0.0
    lip = Vector((funnel_radius(1.0, lip_angle) * FUNNEL_HEIGHT, 0.0, funnel_height(1.0, lip_angle) * FUNNEL_HEIGHT))
    drop = bmesh.ops.create_uvsphere(bm, u_segments=10, v_segments=8, radius=FUNNEL_HEIGHT * 0.045)
    for v in drop["verts"]:
        v.co = Vector((v.co.x, v.co.y, v.co.z * 1.5)) + lip + Vector((FUNNEL_HEIGHT * 0.02, 0.0, -FUNNEL_HEIGHT * 0.07))
    for face in {f for v in drop["verts"] for f in v.link_faces}:
        for loop in face.loops:
            loop[uv_layer].uv = (centre.x, centre.y + spread.y * 0.4)

    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()
    if material is not None:
        mesh.materials.append(material)
    obj = bpy.data.objects.new(mesh.name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    paint_ramp(obj, constant=0.97)
    log(f"{obj.name}: {len(mesh.polygons)} faces")
    return obj


def build_stem(region, material, index):
    centre, spread = region
    mesh = bpy.data.meshes.new(f"SM_{NAME}_Halm_{index}")
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UVMap")
    bend = random.uniform(0.03, 0.06) * (1 if index % 2 else -1)
    knots = [random.uniform(0.2, 0.8) for _ in range(3)]

    rings = []
    for r in range(STEM_RINGS + 1):
        t = r / STEM_RINGS
        point = Vector((math.sin(t * math.pi) * bend, math.sin(t * 2.0 * math.pi + index) * 0.015, t * STEM_LENGTH))
        ahead = Vector((math.cos(t * math.pi) * bend * math.pi, 0.0, STEM_LENGTH)).normalized()
        side = ahead.cross(Vector((0.0, 1.0, 0.0))).normalized()
        up = ahead.cross(side).normalized()
        knot = sum(0.35 * math.exp(-((t - k) / 0.015) ** 2) for k in knots)
        radius = (0.012 * (1.0 - 0.45 * t)) * (1.0 + knot)
        ring = [bm.verts.new(point + (side * math.cos(a) + up * math.sin(a)) * radius)
                for a in (s / STEM_SIDES * 2.0 * math.pi for s in range(STEM_SIDES))]
        rings.append(ring)
    for r in range(STEM_RINGS):
        for s in range(STEM_SIDES):
            face = bm.faces.new((rings[r][s], rings[r][(s + 1) % STEM_SIDES], rings[r + 1][(s + 1) % STEM_SIDES], rings[r + 1][s]))
            for k, loop in enumerate(face.loops):
                ring_index = r + (1 if k >= 2 else 0)
                side_index = s + (1 if k in (1, 2) else 0)
                u = centre.x + (side_index / STEM_SIDES - 0.5) * spread.x
                v = centre.y + ((ring_index / STEM_RINGS) * 4.0 % 1.0 - 0.5) * spread.y
                loop[uv_layer].uv = (u, v)
    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()
    if material is not None:
        mesh.materials.append(material)
    obj = bpy.data.objects.new(mesh.name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    paint_ramp(obj)
    log(f"{obj.name}: {len(mesh.polygons)} faces")
    return obj


def paint_ramp(obj, constant=None):
    """Ramp from 0 at the base to 1 at the tip in vertex colour R and UV channel 1, as halme_zerlegen.py paints it"""
    mesh = obj.data
    height = max(v.co.z for v in mesh.vertices) or 1.0
    colours = mesh.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    ramp_uv = mesh.uv_layers.new(name="Verlauf")
    for loop in mesh.loops:
        t = constant if constant is not None else max(0.0, min(1.0, mesh.vertices[loop.vertex_index].co.z / height))
        colours.data[loop.index].color = (t, t, t, 1.0)
        ramp_uv.data[loop.index].uv = (t, t)


def export(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                             mesh_smooth_type="FACE", colors_type="LINEAR", path_mode="COPY", embed_textures=False, add_leaf_bones=False)
    log(f"exported {path}")


def preview(objects):
    scene = bpy.context.scene
    # Funnel big on the left, the stems shrunk beside it
    objects[0].scale = (6.0, 6.0, 6.0)
    objects[0].location = (-0.3, 0.0, 0.0)
    objects[0].rotation_euler = (math.radians(20), 0.0, math.radians(-30))
    for i, stem in enumerate(objects[1:]):
        stem.scale = (0.8, 0.8, 0.8)
        stem.location = (0.35 + i * 0.2, 0.0, 0.0)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    cam.location = (0.0, -2.0, 0.45)
    cam.rotation_euler = (math.radians(90), 0, 0)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 1.3
    scene.camera = cam
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 900, 800
    scene.render.filepath = os.path.join(OUT, NAME + "_trichter.png")
    bpy.ops.render.render(write_still=True)
    for obj in objects:
        obj.scale = (1.0, 1.0, 1.0)
        obj.location = (0.0, 0.0, 0.0)
        obj.rotation_euler = (0.0, 0.0, 0.0)


bpy.ops.wm.read_factory_settings(use_empty=True)
funnel_region, stem_region, picture = uv_regions()
pieces = [build_funnel(funnel_region, picture), build_stem(stem_region, picture, 1), build_stem(stem_region, picture, 2)]
preview(pieces)
for piece in pieces:
    export(piece)
log("done")
