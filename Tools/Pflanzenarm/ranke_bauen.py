"""
Pflanzenarm: builds a thick twisted vine for the overgrowth of the Floromant's arms while it aims (SM_<name>_Ranke).

Three strands wind around each other along +Z (100 cm, pivot at the base), tapering towards the tip, with thorns or small
blades along them and a few curling tendrils. Its texture coordinates lie in the stem part of the plant's own picture
(taken from the UVs of a stalk cut out by halme_zerlegen.py), so the vine has the colours of the plant. Like the stalks it
carries the ramp from 0 at the base to 1 at the tip in vertex colour R and UV channel 1.

Run:
  blender -b -P ranke_bauen.py -- <stalk fbx for the colours> <output folder> <name> [thorns] [seed]
"""
import bpy, bmesh, sys, os, math, random
from mathutils import Vector, Matrix

args = sys.argv[sys.argv.index("--") + 1:]
STALK, OUT, NAME = args[0], args[1], args[2]
THORNS = int(args[3]) if len(args) > 3 else 18
SEED = int(args[4]) if len(args) > 4 else 7

LENGTH = 1.0          # m, the arm stretches it along its spline
STRANDS = 3
STRAND_RADIUS = 0.022
TWIST_TURNS = 2.5
BUNDLE_RADIUS = 0.022
RINGS = 48
SIDES = 8

random.seed(SEED)
os.makedirs(OUT, exist_ok=True)


def log(text):
    print("RANKE " + text, flush=True)


def stem_uv_region():
    """Middle and spread of the UVs of the stalk's lower half: the part of the picture that shows the stem"""
    bpy.ops.import_scene.fbx(filepath=STALK)
    stalk = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
    mesh = stalk.data
    uv = mesh.uv_layers[0].data
    height = max(v.co.z for v in mesh.vertices) or 1.0
    points = [uv[loop.index].uv.copy() for loop in mesh.loops if mesh.vertices[loop.vertex_index].co.z < height * 0.5]
    material = stalk.data.materials[0] if stalk.data.materials else None
    us = sorted(p.x for p in points)
    vs = sorted(p.y for p in points)
    centre = Vector((us[len(us) // 2], vs[len(vs) // 2]))
    spread = Vector((max(us[int(len(us) * 0.7)] - us[int(len(us) * 0.3)], 0.01), max(vs[int(len(vs) * 0.7)] - vs[int(len(vs) * 0.3)], 0.01)))
    bpy.data.objects.remove(stalk)
    log(f"stem uv centre {centre.x:.3f},{centre.y:.3f} spread {spread.x:.3f},{spread.y:.3f}")
    return centre, spread, material


def tube(bm, path, radii, uv_layer, centre, spread):
    """A tube along the points with a radius per point; texture coordinates wrap inside the stem region"""
    rings = []
    for i, point in enumerate(path):
        ahead = path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]
        ahead.normalize()
        side = ahead.cross(Vector((0.3, 0.7, 0.1))).normalized()
        up = ahead.cross(side).normalized()
        ring = []
        for s in range(SIDES):
            a = s / SIDES * 2.0 * math.pi
            ring.append(bm.verts.new(point + (side * math.cos(a) + up * math.sin(a)) * radii[i]))
        rings.append(ring)
    for i in range(len(rings) - 1):
        for s in range(SIDES):
            face = bm.faces.new((rings[i][s], rings[i][(s + 1) % SIDES], rings[i + 1][(s + 1) % SIDES], rings[i + 1][s]))
            for k, loop in enumerate(face.loops):
                ring_index = i + (1 if k >= 2 else 0)
                side_index = s + (1 if k in (1, 2) else 0)
                u = centre.x + (side_index / SIDES - 0.5) * spread.x
                v = centre.y + ((ring_index / max(len(rings) - 1, 1)) * 3.0 % 1.0 - 0.5) * spread.y
                loop[uv_layer].uv = (u, v)
    # Close the tip
    tip = bm.verts.new(path[-1] + (path[-1] - path[-2]).normalized() * radii[-1])
    for s in range(SIDES):
        face = bm.faces.new((rings[-1][s], rings[-1][(s + 1) % SIDES], tip))
        for loop in face.loops:
            loop[uv_layer].uv = (centre.x, centre.y)


def build():
    centre, spread, material = stem_uv_region()
    mesh = bpy.data.meshes.new(f"SM_{NAME}_Ranke")
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UVMap")

    # Strands winding around each other, thick at the base, thin at the tip
    for strand in range(STRANDS):
        phase = strand / STRANDS * 2.0 * math.pi
        path, radii = [], []
        for i in range(RINGS + 1):
            t = i / RINGS
            angle = phase + t * TWIST_TURNS * 2.0 * math.pi
            wobble = 0.004 * math.sin(t * 23.0 + strand)
            r = BUNDLE_RADIUS * (1.0 - 0.5 * t) + wobble
            path.append(Vector((math.cos(angle) * r, math.sin(angle) * r, t * LENGTH)))
            radii.append(STRAND_RADIUS * (1.0 - 0.75 * t) * (0.85 + 0.15 * strand / STRANDS))
        tube(bm, path, radii, uv_layer, centre, spread)

    # Thorns or blades sticking out of the bundle
    for _ in range(THORNS):
        t = random.uniform(0.05, 0.9)
        angle = random.uniform(0.0, 2.0 * math.pi)
        out = Vector((math.cos(angle), math.sin(angle), random.uniform(0.2, 0.8))).normalized()
        base = Vector((0.0, 0.0, t * LENGTH)) + Vector((math.cos(angle), math.sin(angle), 0.0)) * (BUNDLE_RADIUS + STRAND_RADIUS) * (1.0 - 0.6 * t)
        length = random.uniform(0.02, 0.05) * (1.0 - 0.5 * t)
        tube(bm, [base, base + out * length * 0.5, base + out * length], [0.006, 0.003, 0.0008], uv_layer, centre, spread)

    # Curling tendrils that lead nowhere
    for _ in range(3):
        t = random.uniform(0.2, 0.8)
        angle = random.uniform(0.0, 2.0 * math.pi)
        path, radii = [], []
        for i in range(12):
            k = i / 11
            curl = angle + k * 4.0
            r = 0.03 + 0.05 * k
            path.append(Vector((math.cos(curl) * r, math.sin(curl) * r, t * LENGTH + k * 0.08)))
            radii.append(0.005 * (1.0 - 0.8 * k))
        tube(bm, path, radii, uv_layer, centre, spread)

    bm.to_mesh(mesh)
    bm.free()
    if material is not None:
        mesh.materials.append(material)
    obj = bpy.data.objects.new(mesh.name, mesh)
    bpy.context.scene.collection.objects.link(obj)

    # Ramp from base to tip, as the stalks have it
    height = max(v.co.z for v in mesh.vertices)
    colours = mesh.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    ramp_uv = mesh.uv_layers.new(name="Verlauf")
    for loop in mesh.loops:
        t = max(0.0, min(1.0, mesh.vertices[loop.vertex_index].co.z / height))
        colours.data[loop.index].color = (t, t, t, 1.0)
        ramp_uv.data[loop.index].uv = (t, t)
    log(f"{obj.name}: {len(mesh.polygons)} faces")
    return obj


def export(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                             mesh_smooth_type="FACE", colors_type="LINEAR", path_mode="COPY", embed_textures=False, add_leaf_bones=False)
    log(f"exported {path}")


def preview(obj):
    scene = bpy.context.scene
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    cam.location = (0, -1.2, 0.5)
    cam.rotation_euler = (math.radians(90), 0, 0)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 1.1
    scene.camera = cam
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 500, 900
    scene.render.filepath = os.path.join(OUT, NAME + "_ranke.png")
    bpy.ops.render.render(write_still=True)


bpy.ops.wm.read_factory_settings(use_empty=True)
vine = build()
preview(vine)
export(vine)
log("done")
