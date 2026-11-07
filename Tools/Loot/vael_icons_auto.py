"""
Pictures for the bag (Boris 2026-11-07: potions must look like potions, crystals like crystals). For every material without a
picture (list from Scripts/ExportIconList.py) a small object of its kind is built and rendered on a transparent background,
256 x 256, from above at an angle like the loot kit: potions as bottles in the colour of their liquid (round flask, tall bottle,
ornate flask by rarity), crystals with facets, seeds, leaves, petals, drops, fibres, dust, embers, fungi, creature parts.

Run:
  blender -b --factory-startup -P vael_icons_auto.py -- <auto_list.json> <output folder>
"""
import bpy, bmesh, sys, os, json, math, random
from mathutils import Vector, Matrix, noise

args = sys.argv[sys.argv.index("--") + 1:]
LIST, OUT = args[0], args[1]
os.makedirs(OUT, exist_ok=True)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = scene.render.resolution_y = 256
    scene.render.film_transparent = True
    scene.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w")
    world.color = (0.08, 0.06, 0.05)
    scene.world = world
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 2.4
    cam.location = (2.6, -2.6, 2.4)
    cam.rotation_euler = (Vector((0, 0, 0.45)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(cam)
    scene.camera = cam
    for name, energy, rot, color in (("key", 3.5, (50, 0, 35), (1.0, 0.9, 0.78)), ("rim", 2.0, (120, 0, 210), (0.75, 0.8, 1.0))):
        light = bpy.data.objects.new(name, bpy.data.lights.new(name, "SUN"))
        light.data.energy = energy
        light.data.color = color
        light.rotation_euler = [math.radians(a) for a in rot]
        scene.collection.objects.link(light)


def material(name, color, rough=0.5, emit=0.0, alpha=1.0, metal=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if emit > 0:
        b.inputs["Emission Color"].default_value = (*color, 1)
        b.inputs["Emission Strength"].default_value = emit
    if alpha < 1.0:
        b.inputs["Alpha"].default_value = alpha
        try:
            m.surface_render_method = "BLENDED"
        except AttributeError:
            m.blend_method = "BLEND"
    return m


def mesh_object(name, bm, mat, smooth=True):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    if smooth:
        for p in me.polygons:
            p.use_smooth = True
    ob = bpy.data.objects.new(name, me)
    ob.data.materials.append(mat)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def lathe(name, profile, mat, sides=40, offset=Vector((0, 0, 0))):
    bm = bmesh.new()
    rings = []
    for r, z in profile:
        rings.append([bm.verts.new(offset + Vector((math.cos(2 * math.pi * i / sides) * r, math.sin(2 * math.pi * i / sides) * r, z))) for i in range(sides)])
    for a, b in zip(rings, rings[1:]):
        for i in range(sides):
            try:
                bm.faces.new((a[i], a[(i + 1) % sides], b[(i + 1) % sides], b[i]))
            except ValueError:
                pass
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return mesh_object(name, bm, mat)


def blob(name, mat, center, size, rough=0.25, seed=0, subdiv=3, smooth=True):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=1.0)
    for v in bm.verts:
        n = v.co.normalized()
        v.co = Vector((n.x * size[0], n.y * size[1], n.z * size[2])) * (1.0 + rough * noise.noise(n * 2.5 + Vector((seed, 0, 0))))
        v.co += center
    return mesh_object(name, bm, mat, smooth)


def potion(color, rarity):
    glass = material("glass", (0.75, 0.88, 0.9), rough=0.05, alpha=0.28)
    liquid = material("liquid", color, rough=0.1, emit=0.8)
    cork = material("cork", (0.45, 0.28, 0.16), rough=0.9)
    if rarity == "COMMON":
        prof = [(0.001, 0.0), (0.42, 0.02), (0.6, 0.4), (0.58, 0.7), (0.24, 1.05), (0.2, 1.35), (0.24, 1.42)]
        fill = [(0.001, 0.03), (0.38, 0.05), (0.53, 0.4), (0.5, 0.66), (0.001, 0.67)]
    elif rarity == "MAGIC":
        prof = [(0.001, 0.0), (0.36, 0.02), (0.38, 0.9), (0.18, 1.15), (0.15, 1.45), (0.2, 1.5)]
        fill = [(0.001, 0.03), (0.32, 0.05), (0.33, 0.75), (0.001, 0.76)]
    else:
        prof = [(0.001, 0.0), (0.3, 0.02), (0.55, 0.3), (0.38, 0.62), (0.52, 0.85), (0.18, 1.15), (0.14, 1.42), (0.26, 1.5), (0.2, 1.56)]
        fill = [(0.001, 0.03), (0.27, 0.05), (0.5, 0.3), (0.34, 0.6), (0.46, 0.8), (0.001, 0.81)]
    lathe("fill", fill, liquid)
    lathe("glass", prof, glass)
    top = prof[-1][1]
    lathe("cork", [(0.001, top - 0.12), (0.17, top - 0.12), (0.2, top + 0.12), (0.001, top + 0.14)], cork)
    if rarity != "COMMON":
        gold = material("gold", (0.85, 0.62, 0.25), rough=0.3, metal=1.0)
        lathe("ring", [(0.17, top - 0.32), (0.21, top - 0.3), (0.21, top - 0.24), (0.17, top - 0.22)], gold)


def crystal(color, seed):
    random.seed(seed)
    mat = material("crystal", color, rough=0.15, emit=0.5)
    for k in range(4):
        h = random.uniform(0.7, 1.4) if k else 1.6
        r = random.uniform(0.16, 0.26) if k else 0.3
        bm = bmesh.new()
        sides = 6
        base = [bm.verts.new((math.cos(2 * math.pi * i / sides) * r, math.sin(2 * math.pi * i / sides) * r, 0)) for i in range(sides)]
        mid = [bm.verts.new((math.cos(2 * math.pi * i / sides) * r, math.sin(2 * math.pi * i / sides) * r, h * 0.75)) for i in range(sides)]
        tip = bm.verts.new((0, 0, h))
        for i in range(sides):
            bm.faces.new((base[i], base[(i + 1) % sides], mid[(i + 1) % sides], mid[i]))
            bm.faces.new((mid[i], mid[(i + 1) % sides], tip))
        bm.faces.new(list(reversed(base)))
        tilt = Matrix.Rotation(random.uniform(-0.6, 0.6), 3, "X") @ Matrix.Rotation(random.uniform(-0.6, 0.6), 3, "Y") if k else Matrix.Identity(3)
        for v in bm.verts:
            v.co = tilt @ v.co + Vector((random.uniform(-0.25, 0.25), random.uniform(-0.25, 0.25), 0)) * (1 if k else 0)
        mesh_object("c%d" % k, bm, mat, smooth=False)


def seeds(color, seed):
    random.seed(seed)
    mat = material("seed", tuple(c * 0.6 + 0.12 for c in color), rough=0.45)
    for k in range(7):
        a = random.uniform(0, 6.28)
        r = random.uniform(0, 0.45)
        blob("s%d" % k, mat, Vector((math.cos(a) * r, math.sin(a) * r, 0.12 + random.uniform(0, 0.15))), (0.12, 0.09, 0.16), rough=0.1, seed=k, subdiv=2)


def leaves(color, count, seed, petal=False):
    random.seed(seed)
    mat = material("leaf", color, rough=0.6)
    for k in range(count):
        bm = bmesh.new()
        rows = []
        for j in range(9):
            t = j / 8
            w = math.sin(math.pi * t) ** (0.6 if petal else 0.8) * (0.42 if petal else 0.3)
            rows.append([bm.verts.new((t * 1.2, s * w, 0.06 * math.sin(math.pi * t) + 0.05 * abs(s) * w * 3)) for s in (-1, 0, 1)])
        for a, b in zip(rows, rows[1:]):
            bm.faces.new((a[0], a[1], b[1], b[0]))
            bm.faces.new((a[1], a[2], b[2], b[1]))
        angle = 2 * math.pi * k / count + random.uniform(-0.2, 0.2)
        rot = Matrix.Rotation(angle, 3, "Z") @ Matrix.Rotation(-0.35, 3, "Y")
        for v in bm.verts:
            v.co = rot @ (v.co - Vector((0.05, 0, 0))) + Vector((0, 0, 0.15))
        ob = mesh_object("l%d" % k, bm, mat)
        ob.modifiers.new("solid", "SOLIDIFY").thickness = 0.02


def drop(color, jar=False):
    if jar:
        lathe("jar", [(0.001, 0.0), (0.45, 0.02), (0.5, 0.6), (0.4, 0.75), (0.42, 0.8)], material("jarglass", (0.8, 0.85, 0.8), rough=0.1, alpha=0.3))
        lathe("honey", [(0.001, 0.03), (0.42, 0.05), (0.46, 0.55), (0.001, 0.56)], material("honey", color, rough=0.1, emit=0.4))
        return
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=32, v_segments=24, radius=0.5)
    for v in bm.verts:
        z = v.co.z
        if z > 0:
            f = 1.0 - (z / 0.5) ** 1.2 * 0.85
            v.co.x *= f
            v.co.y *= f
            v.co.z = z * 1.9
        v.co.z += 0.5
    mesh_object("drop", bm, material("drop", color, rough=0.05, emit=0.5, alpha=0.75))


def heap(color, seed, sparkle=False):
    random.seed(seed)
    mat = material("dust", color, rough=0.8, emit=1.5 if sparkle else 0.0)
    blob("mound", material("mound", tuple(c * 0.7 for c in color), rough=0.9), Vector((0, 0, 0)), (0.6, 0.6, 0.3), rough=0.15, seed=seed)
    for k in range(40 if sparkle else 25):
        a = random.uniform(0, 6.28)
        r = random.uniform(0, 0.55)
        z = 0.3 * (1 - (r / 0.6) ** 2) + 0.02
        blob("g%d" % k, mat, Vector((math.cos(a) * r, math.sin(a) * r, z)), (0.05, 0.05, 0.05), rough=0.3, seed=k, subdiv=1, smooth=not sparkle)


def fibres(color, seed):
    random.seed(seed)
    mat = material("fibre", color, rough=0.6)
    for k in range(9):
        curve = bpy.data.curves.new("f%d" % k, "CURVE")
        curve.dimensions = "3D"
        curve.bevel_depth = 0.025
        spline = curve.splines.new("BEZIER")
        spline.bezier_points.add(2)
        off = random.uniform(-0.2, 0.2)
        for i, p in enumerate(spline.bezier_points):
            t = i / 2
            p.co = (-0.7 + 1.4 * t, off + 0.15 * math.sin(t * 3 + k), 0.15 + 0.2 * math.sin(math.pi * t))
            p.handle_left_type = p.handle_right_type = "AUTO"
        ob = bpy.data.objects.new("f%d" % k, curve)
        ob.data.materials.append(mat)
        bpy.context.scene.collection.objects.link(ob)
    blob("tie", material("tie", (0.35, 0.22, 0.12), rough=0.9), Vector((0, 0, 0.3)), (0.09, 0.3, 0.12), rough=0.05)


def mushroom(color, seed):
    lathe("stem", [(0.12, 0.0), (0.1, 0.5), (0.09, 0.75), (0.001, 0.76)], material("stem", (0.85, 0.8, 0.7), rough=0.7))
    lathe("cap", [(0.001, 0.62), (0.5, 0.65), (0.48, 0.8), (0.3, 1.0), (0.001, 1.06)], material("cap", color, rough=0.5))


def chunk(color, seed, glow=False):
    blob("chunk", material("chunk", color, rough=0.6, emit=2.0 if glow else 0.0), Vector((0, 0, 0.45)), (0.55, 0.45, 0.42), rough=0.3, seed=seed, subdiv=3)


def build(entry, index):
    color = tuple(entry["color"])
    kind = entry["kind"]
    name = entry["asset"]
    if entry["category"] == "POTION" or "trank" in name.lower():
        potion(color, entry["rarity"])
    elif "Kristall" in name or "quarz" in name.lower():
        crystal(color, index)
    elif kind == "SEED" or "Samen" in name or "saat" in name.lower():
        seeds(color, index)
    elif kind == "LEAF":
        leaves(color, 3, index)
    elif kind == "PETAL":
        leaves(color, 5, index, petal=True)
    elif kind in ("POLLEN", "DUST"):
        heap(color, index)
    elif kind == "GLITTER":
        heap(color, index, sparkle=True)
    elif kind == "NECTAR":
        drop(color, jar=True)
    elif kind in ("SAP", "DEW"):
        drop(color)
    elif kind == "FIBER":
        fibres(color, index)
    elif kind == "CATALYST":
        mushroom(color, index)
    elif kind == "EMBER":
        chunk(color, index, glow=True)
    else:
        chunk(color, index)


entries = json.load(open(LIST, encoding="utf-8"))
for index, entry in enumerate(entries):
    reset()
    build(entry, index)
    name = "T_Icon_" + entry["asset"].replace("DA_Material_", "")
    bpy.context.scene.render.filepath = os.path.join(OUT, name + ".png")
    bpy.ops.render.render(write_still=True)
    print("ICON", name, flush=True)
print("ICONS FERTIG", len(entries), flush=True)
