"""
Vael - Bilder aller Materialien des Beutels (Boris 17.11.)
=========================================================

"Probiers mal so" (3D-Renders im Licht der Ruestungsteile): jedes Material als kleiner Gegenstand, freigestellt, 512 x 512,
gross im Bild, von schraeg oben, warmes Hauptlicht, kuehles Kantenlicht von hinten, damit er sich vom Hintergrund abhebt. Den
Hintergrund malt das Spiel selbst, in der Farbe der Gruppe des Materials (UI/VaelHUD, Items/VaelItemGroups), damit sich die
Farben spaeter aendern lassen, ohne neu zu rendern.

Die Erze und Steine kommen aus ihren echten Tripo-Modellen (Vael_Quellen/<Name>/04_export), alles andere wird aus Formen
gebaut, die zu ihrer Beschreibung passen: Platten mit Glutrissen, Sehnen, Federn, Klauen, Druesen, Barren, Tuch, Fläschchen,
Samen nach Albruns Beschreibung, Traenke in Flaschen.

Aufruf:
  blender -b --factory-startup -P vael_material_bilder.py -- <material_list.json> <Zielordner> [Name ...]
Die Liste schreibt Scripts/ExportMaterialList.py.
"""
import json
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector, noise

ARGS = sys.argv[sys.argv.index("--") + 1:]
LIST, OUT = ARGS[0], ARGS[1]
ONLY = set(ARGS[2:])
SIZE = 512
FILL = 0.84
# All glow is scaled by this: small things glow hot, a whole leaf must not burn out
EMIT = 0.45
SOURCES = "C:/Projekte/Vael_Quellen"

# Materials drawn from a real model: the model folder and how the model is turned for its picture
MODELS = {
    "Funkenerz": "Funkenerz", "Kernerz": "Kernerz", "Rosteisen": "Rosteisen", "Drahtsilber": "Drahtsilber",
    "Kobaltbluete": "Kobaltbluete", "Gruenspan": "Gruenspan", "Rippenspat": "Rippenspat", "Siechglas": "Siechglas",
    "Salzspat": "Salzspat", "Schwefelkies": "Schwefelkies", "Blutzinnober": "Blutzinnober", "Glutstein": "Glutstein",
    "Schlacke": "SchlackeErz", "Russquarz": "Russquarz",
}


# ---------------------------------------------------------------- scene

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    s = bpy.context.scene
    s.render.engine = "BLENDER_EEVEE"
    s.render.resolution_x = s.render.resolution_y = SIZE
    s.render.film_transparent = True
    s.render.image_settings.file_format = "PNG"
    s.render.image_settings.color_mode = "RGBA"
    s.view_settings.view_transform = "Standard"
    s.view_settings.exposure = 0.0
    world = bpy.data.worlds.new("W")
    s.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.06, 0.05, 0.045, 1.0)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.2


def light(name, direction, energy, color, size):
    data = bpy.data.lights.new(name, "AREA")
    data.energy = energy
    data.color = color
    data.size = size
    o = bpy.data.objects.new(name, data)
    o.location = Vector(direction).normalized() * 8.0
    o.rotation_euler = (-o.location).to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.collection.objects.link(o)


def studio(elevation=30.0, azimuth=-35.0):
    """Camera from the front right above, framed to the item; warm key, cool rim from behind, soft fill"""
    s = bpy.context.scene
    el, az = math.radians(elevation), math.radians(azimuth)
    view = Vector((math.sin(az) * math.cos(el), -math.cos(az) * math.cos(el), math.sin(el)))
    cd = bpy.data.cameras.new("C")
    cd.type = "ORTHO"
    cam = bpy.data.objects.new("C", cd)
    s.collection.objects.link(cam)
    s.camera = cam
    cam.rotation_euler = (-view).to_track_quat("-Z", "Y").to_euler()
    right = cam.matrix_world.to_3x3() @ Vector((1, 0, 0))
    rot = cam.rotation_euler.to_matrix()
    right, up = rot @ Vector((1, 0, 0)), rot @ Vector((0, 1, 0))
    bpy.context.view_layer.update()
    deps = bpy.context.evaluated_depsgraph_get()
    pts = []
    for o in s.objects:
        if o.type in ("MESH", "CURVE"):
            ev = o.evaluated_get(deps)
            me = ev.to_mesh()
            pts += [ev.matrix_world @ v.co for v in me.vertices]
            ev.to_mesh_clear()
    xs = [p.dot(right) for p in pts]
    ys = [p.dot(up) for p in pts]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    extent = max(max(xs) - min(xs), max(ys) - min(ys))
    cd.ortho_scale = extent / FILL
    print("RAHMEN", len(pts), round(extent, 3), round(cd.ortho_scale, 3), flush=True)
    centre = right * cx + up * cy
    cam.location = centre + view * 30.0
    cd.clip_end = 100.0
    scale = extent
    light("Key", (-1.2, -1.6, 1.8), 170.0 * scale * scale, (1.0, 0.88, 0.74), 2.5 * scale)
    light("Rim", (0.6, 2.0, 1.2), 240.0 * scale * scale, (0.78, 0.86, 1.0), 1.5 * scale)
    light("Fill", (2.0, -1.0, 0.2), 40.0 * scale * scale, (0.9, 0.85, 0.8), 3.0 * scale)


# ---------------------------------------------------------------- materials

def mat(name, color, rough=0.5, metal=0.0, emit=0.0, alpha=1.0, bump=0.3, bump_scale=40.0, glass=False, emit_color=None, coat=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    b.inputs["Coat Weight"].default_value = coat
    if emit > 0:
        b.inputs["Emission Color"].default_value = (*(emit_color or color), 1)
        b.inputs["Emission Strength"].default_value = emit * EMIT
    if glass:
        b.inputs["Transmission Weight"].default_value = 1.0
        b.inputs["IOR"].default_value = 1.45
    if alpha < 1.0:
        b.inputs["Alpha"].default_value = alpha
        try:
            m.surface_render_method = "BLENDED"
        except AttributeError:
            pass
    if bump > 0:
        tex = nt.nodes.new("ShaderNodeTexNoise")
        tex.inputs["Scale"].default_value = bump_scale
        tex.inputs["Detail"].default_value = 8.0
        bn = nt.nodes.new("ShaderNodeBump")
        bn.inputs["Strength"].default_value = bump
        bn.inputs["Distance"].default_value = 0.01
        nt.links.new(tex.outputs["Fac"], bn.inputs["Height"])
        nt.links.new(bn.outputs["Normal"], b.inputs["Normal"])
        mix = nt.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.inputs["A"].default_value = (*[c * 0.72 for c in color], 1)
        mix.inputs["B"].default_value = (*[min(1.0, c * 1.2) for c in color], 1)
        nt.links.new(tex.outputs["Fac"], mix.inputs["Factor"])
        nt.links.new(mix.outputs["Result"], b.inputs["Base Color"])
    return m


def cracked(name, color, glow_color, glow, scale=5.0, width=0.035, rough=0.55, metal=0.2):
    """A dark plate whose cracks glow: Voronoi cells, their edges lit"""
    m = mat(name, color, rough=rough, metal=metal, bump=0.5, bump_scale=60.0)
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    vor = nt.nodes.new("ShaderNodeTexVoronoi")
    vor.feature = "DISTANCE_TO_EDGE"
    vor.inputs["Scale"].default_value = scale
    rng = nt.nodes.new("ShaderNodeMapRange")
    rng.inputs["From Min"].default_value = 0.0
    rng.inputs["From Max"].default_value = width
    rng.inputs["To Min"].default_value = glow * EMIT
    rng.inputs["To Max"].default_value = 0.0
    nt.links.new(vor.outputs["Distance"], rng.inputs["Value"])
    b.inputs["Emission Color"].default_value = (*glow_color, 1)
    nt.links.new(rng.outputs["Result"], b.inputs["Emission Strength"])
    return m


def veined(name, color, vein_color, glow, rough=0.25):
    """Glossy flesh with glowing veins (wave bands distorted)"""
    m = mat(name, color, rough=rough, bump=0.4, bump_scale=30.0, coat=0.6)
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    wave = nt.nodes.new("ShaderNodeTexWave")
    wave.inputs["Scale"].default_value = 3.0
    wave.inputs["Distortion"].default_value = 12.0
    wave.inputs["Detail"].default_value = 4.0
    rng = nt.nodes.new("ShaderNodeMapRange")
    rng.inputs["From Min"].default_value = 0.85
    rng.inputs["From Max"].default_value = 1.0
    rng.inputs["To Min"].default_value = 0.0
    rng.inputs["To Max"].default_value = glow * EMIT
    nt.links.new(wave.outputs["Fac"], rng.inputs["Value"])
    b.inputs["Emission Color"].default_value = (*vein_color, 1)
    nt.links.new(rng.outputs["Result"], b.inputs["Emission Strength"])
    return m


# ---------------------------------------------------------------- shapes

def obj(name, bm, material, smooth=True, solid=0.0, subsurf=0):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = smooth
    o = bpy.data.objects.new(name, me)
    o.data.materials.append(material)
    bpy.context.scene.collection.objects.link(o)
    if solid:
        o.modifiers.new("S", "SOLIDIFY").thickness = solid
    if subsurf:
        o.modifiers.new("D", "SUBSURF").levels = subsurf
    return o


def lathe(name, profile, material, sides=48, at=Vector((0, 0, 0)), ribs=0, rib_depth=0.0):
    bm = bmesh.new()
    rings = []
    for r, z in profile:
        ring = []
        for i in range(sides):
            a = 2 * math.pi * i / sides
            rr = r * (1.0 - rib_depth * (0.5 + 0.5 * math.cos(a * ribs))) if ribs else r
            ring.append(bm.verts.new(at + Vector((math.cos(a) * rr, math.sin(a) * rr, z))))
        rings.append(ring)
    for a, b in zip(rings, rings[1:]):
        for i in range(sides):
            try:
                bm.faces.new((a[i], a[(i + 1) % sides], b[(i + 1) % sides], b[i]))
            except ValueError:
                pass
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return obj(name, bm, material)


def blob(name, material, centre, size, rough=0.25, seed=0, subdiv=4, smooth=True):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=1.0)
    for v in bm.verts:
        n = v.co.normalized()
        v.co = Vector((n.x * size[0], n.y * size[1], n.z * size[2])) * (1.0 + rough * noise.noise(n * 2.2 + Vector((seed * 1.7, 0, 0))))
        v.co += Vector(centre)
    return obj(name, bm, material, smooth)


def curve(name, points, radius, material, radii=None, resolution=12, cyclic=False):
    """A tube along points, radius per point (tapering horns, sinews, quills)"""
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = radius
    cu.bevel_resolution = 6
    cu.resolution_u = resolution
    sp = cu.splines.new("NURBS")
    sp.points.add(len(points) - 1)
    for p, co in zip(sp.points, points):
        p.co = (*co, 1.0)
    if radii:
        for p, r in zip(sp.points, radii):
            p.radius = r
    sp.use_endpoint_u = True
    sp.order_u = 3
    sp.use_cyclic_u = cyclic
    o = bpy.data.objects.new(name, cu)
    o.data.materials.append(material)
    bpy.context.scene.collection.objects.link(o)
    return o


def sheet(name, material, outline_w, outline_h, bend, seed=0, res=24, wave=0.0, thick=0.03, shape=None):
    """A thin curved sheet: u across, v along; shape(u, v) -> half width factor"""
    bm = bmesh.new()
    rows = []
    for j in range(res + 1):
        v = j / res
        half = shape(v) if shape else 1.0
        row = []
        for i in range(res + 1):
            u = (i / res * 2 - 1) * half
            x, y = u * outline_w * 0.5, (v - 0.5) * outline_h
            z = bend * (u * u) + wave * math.sin(v * 9 + seed) * 0.5 + wave * noise.noise(Vector((u * 2, v * 3, seed)))
            row.append(bm.verts.new((x, y, z)))
        rows.append(row)
    for a, b in zip(rows, rows[1:]):
        for i in range(res):
            bm.faces.new((a[i], a[i + 1], b[i + 1], b[i]))
    return obj(name, bm, material, solid=thick)


def rotate(o, rx=0, ry=0, rz=0, at=(0, 0, 0)):
    o.rotation_euler = (math.radians(rx), math.radians(ry), math.radians(rz))
    o.location = at
    return o


# ---------------------------------------------------------------- builders by look

def model(folder):
    path = "%s/%s/04_export/SM_%s.fbx" % (SOURCES, folder, folder)
    bpy.ops.import_scene.fbx(filepath=path)
    for o in [o for o in bpy.data.objects if o.type == "MESH"]:
        for slot in o.material_slots:
            m = slot.material
            if m is None or not m.use_nodes:
                continue
            b = next((n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
            if b is not None:
                b.inputs["Coat Weight"].default_value = 0.2
    return [o for o in bpy.data.objects if o.type == "MESH"]


def plates(color, glow_color, glow, count=2):
    m = cracked("Platte", color, glow_color, glow)
    shape = lambda v: max(0.05, math.sin(math.pi * min(1.0, v * 1.15)) ** 0.7)  # noqa: E731
    for k in range(count):
        o = sheet("p%d" % k, m, 1.2, 1.5, 0.35, seed=k, wave=0.03, thick=0.09, shape=shape)
        o.modifiers.new("B", "BEVEL").width = 0.02
        rotate(o, 20 + k * 6, 0, -18 + k * 30, (k * 0.35 - 0.15, k * 0.25, k * 0.18))


def band(color, glow_color, glow):
    m = cracked("Band", color, glow_color, glow, scale=3.0, width=0.025)
    bm = bmesh.new()
    rows = []
    for j in range(40):
        a = math.radians(-60 + 120 * j / 39)
        row = []
        for i in range(8):
            w = (i / 7 - 0.5) * 0.8
            ridge = 0.04 * math.cos((i / 7) * math.pi * 2) if i in (0, 7) else 0.0
            r = 1.0 + ridge
            row.append(bm.verts.new((w, math.sin(a) * r, math.cos(a) * r - 0.6)))
        rows.append(row)
    for a, b in zip(rows, rows[1:]):
        for i in range(7):
            bm.faces.new((a[i], a[i + 1], b[i + 1], b[i]))
    o = obj("band", bm, m, solid=0.12)
    o.modifiers.new("B", "BEVEL").width = 0.025
    rotate(o, 0, 0, 30)


def coil(color, thick=0.06, loops=3.2, shine=0.35, tails=True, glow=0.0):
    m = mat("Sehne", color, rough=shine, bump=0.4, bump_scale=80.0, emit=glow, coat=0.3)
    pts = []
    for k in range(70):
        t = k / 69 * loops * 2 * math.pi
        r = 0.55 + 0.05 * math.sin(t * 3)
        shift = 0.5 * k / 69 - 0.25
        pts.append((math.cos(t) * r + shift, math.sin(t) * r * 0.7, 0.18 * k / 69 + 0.06 * math.sin(t * 2)))
    if tails:
        pts.append((pts[-1][0] + 0.35, pts[-1][1] - 0.5, 0.0))
        pts.insert(0, (pts[0][0] - 0.2, pts[0][1] + 0.6, 0.0))
    curve("coil", pts, thick, m)


def skein(color, seed, glow=0.0):
    """A ball of thread"""
    random.seed(seed)
    m = mat("Garn", color, rough=0.7, bump=0.6, bump_scale=200.0, emit=glow)
    blob("kern", m, (0, 0, 0), (0.52, 0.52, 0.48), rough=0.05)
    for k in range(16):
        axis = Vector((random.uniform(-1, 1), random.uniform(-1, 1), random.uniform(-1, 1))).normalized()
        rot = axis.to_track_quat("Z", "Y").to_matrix()
        pts = [tuple(rot @ Vector((math.cos(t) * 0.56, math.sin(t) * 0.56, 0))) for t in [i / 30 * 2 * math.pi for i in range(31)]]
        curve("w%d" % k, pts, 0.018, m)
    curve("ende", [(0.4, -0.3, 0.1), (0.8, -0.5, -0.2), (1.0, -0.2, -0.4)], 0.018, m)


def feather(color, count=1, quill_only=False, big=False):
    mv = mat("Fahne", color, rough=0.6, bump=0.8, bump_scale=120.0)
    mq = mat("Kiel", (0.75, 0.68, 0.55), rough=0.35, coat=0.4)
    length = 1.9 if big else 1.6
    for k in range(count):
        ang = (k - (count - 1) / 2) * 22
        rot = Matrix.Rotation(math.radians(ang), 3, "Z")
        q = [rot @ Vector((0, y, 0.15 * math.sin(y / length * math.pi))) for y in [i / 6 * length - length / 2 for i in range(7)]]
        curve("kiel%d" % k, [tuple(p) for p in q], 0.09 if quill_only else 0.03, mq, radii=[1.2, 1.1, 1.0, 0.9, 0.7, 0.5, 0.3])
        if quill_only:
            continue
        bm = bmesh.new()
        rows = []
        for j in range(30):
            v = j / 29
            y = (0.18 + 0.82 * v) * length - length / 2
            half = 0.36 * math.sin(math.pi * min(1.0, (v * 1.05))) ** 0.6
            row = []
            for i in range(9):
                u = (i / 8 * 2 - 1)
                serr = 0.015 * math.sin(j * 2.3 + i) if abs(u) > 0.7 else 0.0
                row.append(bm.verts.new(tuple(rot @ Vector((u * half + serr, y - abs(u) * 0.12, 0.15 * math.sin((y + length / 2) / length * math.pi) - abs(u) * 0.05)))))
            rows.append(row)
        for a, b in zip(rows, rows[1:]):
            for i in range(8):
                bm.faces.new((a[i], a[i + 1], b[i + 1], b[i]))
        obj("fahne%d" % k, bm, mv, solid=0.01)


def horn(name, material, start, bend_to, length, radius, curl=0.6, segments=8):
    """A tapering hooked horn, claw or spike from start along an arc"""
    pts, radii = [], []
    s = Vector(start)
    d = Vector(bend_to).normalized()
    side = d.cross(Vector((0, 0, 1))).normalized() if abs(d.z) < 0.9 else Vector((1, 0, 0))
    up = side.cross(d).normalized()
    for k in range(segments + 1):
        t = k / segments
        a = curl * t * t * math.pi / 2
        p = s + d * (length * t * math.cos(a)) + up * (length * t * math.sin(a))
        pts.append(tuple(p))
        radii.append(max(0.04, (1.0 - t) ** 0.8))
    return curve(name, pts, radius, material, radii=radii)


def claws(color, count=3, curl=0.8, length=1.2, bone=False):
    m = mat("Klaue", color, rough=0.3, coat=0.5, bump=0.2, bump_scale=60.0)
    base = mat("Ballen", tuple(c * 0.6 for c in color) if not bone else (0.35, 0.3, 0.26), rough=0.6, bump=0.6, bump_scale=60.0)
    blob("ballen", base, (0, -0.2, 0), (0.42, 0.32, 0.28), rough=0.15)
    for k in range(count):
        a = math.radians((k - (count - 1) / 2) * 28)
        horn("k%d" % k, m, (math.sin(a) * 0.25, 0.0, 0.05), (math.sin(a), math.cos(a), 0.15), length, 0.14, curl=curl)


def beak(color):
    m = mat("Schnabel", color, rough=0.25, coat=0.6, bump=0.2)
    horn("oben", m, (0, -0.6, 0.0), (0, 1, 0.1), 1.4, 0.32, curl=1.0)
    horn("unten", m, (0, -0.6, -0.12), (0, 1, -0.2), 0.9, 0.22, curl=0.15)


def spike(color, length=2.0, curl=0.35, glow=0.0):
    m = mat("Stachel", color, rough=0.3, coat=0.5, emit=glow, emit_color=(1.0, 0.4, 0.1))
    horn("s", m, (0, -length / 2, 0), (0.1, 1, 0.1), length, 0.16, curl=curl, segments=10)


def finger(color):
    m = mat("Knochen", color, rough=0.45, bump=0.4, bump_scale=50.0)
    p = Vector((0, -0.9, 0))
    d = Vector((0, 1, 0.15)).normalized()
    for k, (length, r) in enumerate(((0.7, 0.13), (0.55, 0.11), (0.4, 0.09))):
        q = p + d * length
        curve("glied%d" % k, [tuple(p), tuple((p + q) / 2), tuple(q)], r, m, radii=[1.2, 0.9, 1.1])
        blob("knoechel%d" % k, m, q, (r * 1.3, r * 1.3, r * 1.3), rough=0.1)
        p = q
        d = (Matrix.Rotation(0.5, 3, "X") @ d).normalized()
    horn("kralle", mat("Kralle", (0.08, 0.07, 0.07), rough=0.25, coat=0.6), tuple(p), tuple(d), 0.45, 0.09, curl=0.9)


def jaw(color, glow_color):
    m = cracked("Kiefer", color, glow_color, 3.0, scale=4.0)
    teeth = mat("Zahn", (0.8, 0.72, 0.6), rough=0.35, coat=0.4)
    pts = [(math.cos(a) * 0.9, math.sin(a) * 0.7, 0) for a in [math.radians(200 + 140 * k / 10) for k in range(11)]]
    curve("kiefer", pts, 0.16, m, radii=[0.7, 1, 1.1, 1.2, 1.2, 1.2, 1.2, 1.2, 1.1, 1, 0.7])
    for k in range(1, 10):
        x, y, _ = pts[k]
        horn("z%d" % k, teeth, (x * 0.95, y * 0.95, 0.12), (0, 0, 1), 0.3 + 0.1 * (k % 2), 0.06, curl=0.0, segments=3)


def crown(color, glow_color, vein=False):
    m = cracked("Krone", color, glow_color, 3.5) if not vein else veined("Krone", color, (0.6, 0.2, 1.0), 4.0, rough=0.4)
    curve("reif", [(math.cos(a) * 0.8, math.sin(a) * 0.8, 0) for a in [k / 24 * 2 * math.pi for k in range(25)]], 0.12, m, cyclic=False)
    for k in range(9):
        a = k / 9 * 2 * math.pi
        horn("zacke%d" % k, m, (math.cos(a) * 0.8, math.sin(a) * 0.8, 0.05), (math.cos(a) * 0.3, math.sin(a) * 0.3, 1), 0.6 + 0.25 * (k % 2), 0.13, curl=0.3)


def blade(color):
    m = mat("Klinge", color, rough=0.35, metal=0.8, bump=0.5, bump_scale=30.0)
    shape = lambda v: max(0.03, math.sin(math.pi * v) ** 0.5)  # noqa: E731
    bm = bmesh.new()
    rows = []
    for j in range(40):
        v = j / 39
        a = math.radians(-100 + 160 * v)
        r_out, width = 1.0, 0.32 * shape(v)
        row = [bm.verts.new((math.cos(a) * (r_out - width * t), math.sin(a) * (r_out - width * t), 0)) for t in (0.0, 0.5, 1.0)]
        rows.append(row)
    for a, b in zip(rows, rows[1:]):
        for i in range(2):
            bm.faces.new((a[i], a[i + 1], b[i + 1], b[i]))
    obj("klinge", bm, m, solid=0.04)
    curve("griff", [(0.0, -1.0, 0), (-0.2, -1.4, 0), (-0.35, -1.8, 0)], 0.07, mat("Griff", (0.25, 0.15, 0.08), rough=0.8))


def organ(color, vein_color, glow, size=(0.7, 0.6, 0.55), seed=0):
    m = veined("Organ", color, vein_color, glow)
    blob("organ", m, (0, 0, 0), size, rough=0.22, seed=seed, subdiv=5)


def eye(iris_color, glow, sclera=(0.82, 0.72, 0.55)):
    blob("auge", mat("Auge", sclera, rough=0.15, coat=1.0, bump=0.1), (0, 0, 0), (0.6, 0.6, 0.6), rough=0.02, subdiv=5)
    iris = lathe("iris", [(0.001, 0.6), (0.3, 0.57), (0.32, 0.5)], mat("Iris", iris_color, rough=0.2, emit=glow, coat=1.0))
    pupil = lathe("pupille", [(0.001, 0.62), (0.12, 0.6), (0.12, 0.57)], mat("Pupille", (0.01, 0.01, 0.01), rough=0.1))
    # the iris looks at the camera of studio() (elevation 30, azimuth -35)
    el, az = math.radians(30.0), math.radians(-35.0)
    view = Vector((math.sin(az) * math.cos(el), -math.cos(az) * math.cos(el), math.sin(el)))
    turn = Vector((0, 0, 1)).rotation_difference(view)
    for o in (iris, pupil):
        o.rotation_mode = "QUATERNION"
        o.rotation_quaternion = turn


def pearl(color, glow=0.4, count=1):
    m = mat("Perle", color, rough=0.12, coat=1.0, emit=glow, bump=0.05)
    for k in range(count):
        blob("perle%d" % k, m, (k * 0.75 - (count - 1) * 0.37, k * 0.2, 0), (0.5, 0.5, 0.52), rough=0.04, seed=k, subdiv=5)
    curve("stiel", [(0, 0, 0.45), (0.05, 0.05, 0.7), (0.15, 0.0, 0.9)], 0.035, mat("Stiel", (0.3, 0.22, 0.12), rough=0.7))


def hide(color, seed, fur=True, alpha=1.0, pockets=False):
    m = mat("Fell", color, rough=0.9 if fur else 0.4, bump=1.2 if fur else 0.3, bump_scale=300.0 if fur else 20.0, alpha=alpha)
    shape = lambda v: 0.75 + 0.25 * math.sin(v * 6 + seed)  # noqa: E731
    o = sheet("fell", m, 1.8, 1.6, 0.25, seed=seed, wave=0.12, thick=0.06, shape=shape)
    rotate(o, 10, 0, 20)
    if pockets:
        pm = mat("Tasche", tuple(c * 1.6 for c in color), rough=0.8, bump=0.8, bump_scale=200.0)
        for k, (x, y) in enumerate(((-0.35, 0.2), (0.3, -0.25))):
            p = sheet("tasche%d" % k, pm, 0.45, 0.35, 0.08, seed=k + 3, wave=0.02, thick=0.03)
            rotate(p, 10, 0, 20, (x, y, 0.12))


def cloth(color, trim):
    m = mat("Tuch", color, rough=0.85, bump=0.6, bump_scale=400.0)
    t = mat("Borte", trim, rough=0.4, metal=0.8)
    for k in range(3):
        o = sheet("lage%d" % k, m, 1.6, 1.1, 0.0, seed=k, wave=0.03, thick=0.08)
        rotate(o, 0, 0, k * 4 - 4, (k * 0.04, -k * 0.03, k * 0.1))
    b = sheet("borte", t, 1.62, 0.1, 0.0, wave=0.0, thick=0.09)
    rotate(b, 0, 0, 4, (0.08, -0.06 - 0.38, 0.22))


def seal(color, emblem=True):
    m = mat("Wachs", color, rough=0.3, coat=0.5, bump=0.4, bump_scale=15.0)
    blob("wachs", m, (0, 0, 0), (0.75, 0.7, 0.18), rough=0.12, subdiv=5)
    if emblem:
        r = mat("Praegung", tuple(c * 0.55 for c in color), rough=0.4)
        curve("ring", [(math.cos(a) * 0.45, math.sin(a) * 0.45, 0.18) for a in [k / 24 * 2 * math.pi for k in range(25)]], 0.035, r)
        curve("narbe", [(-0.3, -0.25, 0.19), (0.0, 0.05, 0.2), (0.28, 0.3, 0.19)], 0.04, r)


def ingot(color, count=2):
    m = mat("Barren", color, rough=0.45, metal=0.9, bump=0.6, bump_scale=25.0)
    for k in range(count):
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:
            v.co.x *= 1.3
            v.co.y *= 0.55
            v.co.z *= 0.35
            if v.co.z > 0:
                v.co.x *= 0.82
                v.co.y *= 0.75
        o = obj("barren%d" % k, bm, m, smooth=False)
        o.modifiers.new("B", "BEVEL").width = 0.03
        rotate(o, 0, 0, 12 + k * 20, (k * 0.25, k * 0.35, k * 0.36))


def brass_sheet(color):
    m = mat("Blech", color, rough=0.25, metal=1.0, bump=0.3, bump_scale=15.0)
    o = sheet("blech", m, 1.6, 1.2, 0.3, wave=0.02, thick=0.025)
    rotate(o, 25, 0, 18)
    curve("rolle", [(-0.8, -0.6 + 0.6 * k / 4, 0.3) for k in range(5)], 0.05, m)


def trinkets(seed):
    random.seed(seed)
    gold = mat("Messing", (0.8, 0.58, 0.24), rough=0.3, metal=1.0)
    dark = mat("Altmessing", (0.45, 0.32, 0.14), rough=0.4, metal=1.0)
    for k in range(4):
        o = lathe("ring%d" % k, [(0.22, -0.04), (0.28, -0.03), (0.28, 0.03), (0.22, 0.04), (0.22, -0.04)], gold if k % 2 else dark, sides=40)
        rotate(o, random.uniform(-40, 40), random.uniform(-40, 40), 0, (random.uniform(-0.6, 0.6), random.uniform(-0.5, 0.5), random.uniform(0, 0.2)))
    for k in range(5):
        o = lathe("knopf%d" % k, [(0.001, 0.05), (0.18, 0.04), (0.2, 0.0), (0.001, -0.02)], gold if k % 2 else dark, sides=32)
        rotate(o, random.uniform(-20, 20), random.uniform(-20, 20), 0, (random.uniform(-0.7, 0.7), random.uniform(-0.6, 0.6), random.uniform(0.0, 0.3)))


def rolled(color, alpha=1.0):
    m = mat("Latex", color, rough=0.2, coat=0.8, alpha=alpha, bump=0.1)
    pts = [(math.cos(t) * (0.15 + 0.02 * t), math.sin(t) * (0.15 + 0.02 * t), 0) for t in [k / 60 * 6 * math.pi for k in range(61)]]
    bm = bmesh.new()
    rows = []
    for x in (-0.8, 0.8):
        rows.append([bm.verts.new((x, p[0], p[1])) for p in pts])
    tail = [bm.verts.new((x, pts[-1][0] + 0.9, pts[-1][1] - 0.15)) for x in (-0.8, 0.8)]
    for i in range(len(pts) - 1):
        bm.faces.new((rows[0][i], rows[1][i], rows[1][i + 1], rows[0][i + 1]))
    bm.faces.new((rows[0][-1], rows[1][-1], tail[1], tail[0]))
    o = obj("rolle", bm, m, solid=0.02)
    rotate(o, 0, 0, 25)


def lens(glass_color):
    lathe("linse", [(0.001, -0.18), (0.5, -0.04), (0.55, 0.0), (0.5, 0.04), (0.001, 0.18)], mat("Glas", glass_color, rough=0.08, alpha=0.55, coat=1.0, bump=0.0))
    lathe("fassung", [(0.53, -0.08), (0.62, -0.08), (0.62, 0.08), (0.53, 0.08), (0.53, -0.08)], mat("Ring", (0.78, 0.56, 0.24), rough=0.3, metal=1.0))
    for o in bpy.data.objects:
        if o.type == "MESH":
            rotate(o, 70, 0, 20)


def glass_balls(color):
    m = mat("Schlackenglas", color, rough=0.05, glass=True, bump=0.05)
    for k, (x, y, r) in enumerate(((0, 0, 0.45), (0.65, 0.3, 0.35), (-0.5, 0.45, 0.3))):
        blob("kugel%d" % k, m, (x, y, r), (r, r, r), rough=0.03, seed=k, subdiv=5)


def bubble(dust):
    blob("blase", mat("Blase", (0.8, 0.82, 0.85), rough=0.05, alpha=0.25, coat=1.0, bump=0.0), (0, 0, 0.55), (0.7, 0.7, 0.65), rough=0.06, subdiv=5)
    blob("staub", mat("Staub", dust, rough=0.95, bump=1.5, bump_scale=200.0), (0, 0, 0.15), (0.5, 0.5, 0.18), rough=0.25)


def wood(color, vein, glow):
    m = veined("Holz", color, vein, glow, rough=0.75)
    pts = [(math.sin(k * 0.9) * 0.15, -1.0 + k * 0.22, math.cos(k * 0.7) * 0.12) for k in range(10)]
    curve("ast", pts, 0.2, m, radii=[1.1, 1.0, 1.05, 0.95, 1.0, 0.9, 0.85, 0.8, 0.7, 0.5])
    horn("zweig", m, pts[4], (0.8, 0.5, 0.2), 0.7, 0.09, curl=0.2)


def straw(color, glow):
    m = mat("Stroh", color, rough=0.8, bump=0.6, bump_scale=200.0, emit=glow, emit_color=(1.0, 0.35, 0.05))
    random.seed(3)
    for k in range(22):
        x, z = random.uniform(-0.18, 0.18), random.uniform(-0.15, 0.15)
        a = random.uniform(-0.25, 0.25)
        curve("halm%d" % k, [(x - 0.9 * a, -0.9, z), (x, 0, z), (x + 0.9 * a, 0.9, z)], 0.025, m)
    curve("band", [(math.cos(a) * 0.26, 0.05, math.sin(a) * 0.22) for a in [k / 16 * 2 * math.pi for k in range(17)]], 0.04, mat("Band", (0.45, 0.12, 0.08), rough=0.7))


def bell(color, glow=0.0, wilted=False):
    m = mat("Glocke", color, rough=0.45, emit=glow, coat=0.3, bump=0.4)
    prof = [(0.001, 0.9), (0.18, 0.88), (0.35, 0.6), (0.45, 0.25), (0.62, 0.0), (0.56, -0.05)]
    o = lathe("glocke", prof, m, ribs=5, rib_depth=0.12)
    o.modifiers.new("S", "SOLIDIFY").thickness = 0.03
    rotate(o, 160 if not wilted else 130, 0, 0, (0, 0, 0.4))
    curve("stiel", [(0, 0, -0.45), (0.2, 0.0, -0.8), (0.5, 0.0, -0.9)], 0.04, mat("Stiel", (0.2, 0.3, 0.12), rough=0.7))
    if glow:
        blob("glut", mat("Glut", (1.0, 0.6, 0.2), emit=6.0, rough=0.3), (0, 0, 0.15), (0.2, 0.2, 0.2), rough=0.2)


def tuft(color, seed):
    random.seed(seed)
    m = mat("Moos", color, rough=0.95, bump=1.5, bump_scale=300.0)
    for k in range(9):
        a, r = random.uniform(0, 6.28), random.uniform(0, 0.5)
        blob("flocke%d" % k, m, (math.cos(a) * r, math.sin(a) * r, 0.1), (0.28, 0.28, 0.2), rough=0.4, seed=k, subdiv=3)


def leaf(color, seed, glow=0.0, tip=None, broad=False, count=1):
    vein = mat("Ader", tuple(min(1.0, c * 1.6 + 0.05) for c in color), rough=0.5, emit=glow * 2.0)
    m = mat("Blatt", tuple(c * (0.55 if glow else 1.0) for c in color), rough=0.5, emit=glow * 0.15, coat=0.3, bump=0.3, bump_scale=60.0)
    for k in range(count):
        width = 0.65 if broad else 0.45
        shape = lambda v: max(0.02, math.sin(math.pi * v) ** (0.6 if broad else 0.8))  # noqa: E731
        o = sheet("blatt%d" % k, m, width * 2, 1.9, 0.25, seed=seed + k, wave=0.04, thick=0.015, shape=shape)
        rotate(o, 25, 0, -25 + k * 50, (k * 0.3, 0, k * 0.1))
        curve("mittelrippe%d" % k, [(0, -0.95, 0.02), (0, 0, 0.02), (0, 0.9, 0.02)], 0.018, vein).parent = o
        if tip:
            blob("spitze%d" % k, mat("Spitze", tip, emit=2.0, rough=0.4), (0, 0.85, 0.03), (0.1, 0.12, 0.03), rough=0.1).parent = o


def petals(color, seed, count=3):
    random.seed(seed)
    m = mat("Bluete", color, rough=0.5, coat=0.2, bump=0.6, bump_scale=150.0)
    shape = lambda v: max(0.05, math.sin(math.pi * v) ** 0.45 * (1.0 - 0.15 * v))  # noqa: E731
    for k in range(count):
        o = sheet("bl%d" % k, m, 1.1, 1.3, -0.25, seed=seed + k, wave=0.05, thick=0.012, shape=shape)
        rotate(o, 15 + k * 8, 0, k * 360 / count + random.uniform(-15, 15), (0, 0, k * 0.06))
        o.location = Matrix.Rotation(math.radians(k * 360 / count), 3, "Z") @ Vector((0, 0.45, k * 0.06))


def fibres(color, seed, thorns=False, thick=0.03, count=11):
    random.seed(seed)
    m = mat("Faser", color, rough=0.55, bump=0.4, bump_scale=200.0, coat=0.2)
    for k in range(count):
        off = random.uniform(-0.25, 0.25)
        pts = [(off + 0.12 * math.sin(t * 4 + k), -1.0 + 2.0 * t, 0.1 * math.sin(t * math.pi) + random.uniform(-0.03, 0.03)) for t in [i / 6 for i in range(7)]]
        curve("f%d" % k, pts, thick, m)
        if thorns:
            for i in range(1, 6):
                horn("dorn%d_%d" % (k, i), m, pts[i], (random.choice((-1, 1)), 0.4, 0.3), 0.18, 0.025, curl=0.2, segments=3)
    curve("bund", [(math.cos(a) * 0.32, 0.0, math.sin(a) * 0.14 + 0.05) for a in [i / 16 * 2 * math.pi for i in range(17)]], 0.045, mat("Bund", (0.38, 0.24, 0.12), rough=0.85))


def vine(color, seed):
    m = mat("Ranke", color, rough=0.5, coat=0.2, bump=0.3)
    random.seed(seed)
    pts = [(math.sin(t * 5) * 0.5, -1.0 + 2.0 * t, math.cos(t * 5) * 0.3) for t in [i / 14 for i in range(15)]]
    curve("ranke", pts, 0.06, m)
    for i in range(1, 14):
        horn("dorn%d" % i, mat("Dorn", (0.6, 0.15, 0.2), rough=0.3), pts[i], (random.uniform(-1, 1), 0.3, random.uniform(-1, 1)), 0.22, 0.035, curl=0.2, segments=3)


def heap(color, seed, sparkle=False, pile=True):
    random.seed(seed)
    m = mat("Haufen", tuple(c * 0.55 for c in color), rough=0.95, bump=1.6, bump_scale=260.0, emit=0.6 if sparkle else 0.0)
    blob("haufen", m, (0, 0, 0), (0.85, 0.85, 0.42), rough=0.18, seed=seed)
    grain = mat("Korn", tuple(min(1.0, c * 1.2) for c in color), rough=0.5, emit=4.0 if sparkle else 0.0, coat=0.4)
    for k in range(110 if sparkle else 60):
        a, r = random.uniform(0, 6.28), random.uniform(0, 0.8)
        z = 0.42 * (1 - (r / 0.85) ** 2) + 0.01
        g = 0.035 + 0.03 * random.random()
        blob("k%d" % k, grain, (math.cos(a) * r, math.sin(a) * r, z), (g, g, g), rough=0.3, seed=k, subdiv=1, smooth=not sparkle)


def vial(color, glow=0.4, milky=False):
    glass = mat("Glas", (0.85, 0.9, 0.92), rough=0.04, alpha=0.3, coat=1.0, bump=0.0)
    liquid = mat("Fluessigkeit", color, rough=0.1 if not milky else 0.4, emit=glow, coat=0.5, bump=0.0)
    lathe("phiole", [(0.001, 0.0), (0.22, 0.02), (0.24, 0.1), (0.24, 1.3), (0.28, 1.36), (0.24, 1.4)], glass, sides=40)
    lathe("inhalt", [(0.001, 0.02), (0.2, 0.04), (0.215, 0.1), (0.215, 0.95), (0.001, 0.96)], liquid, sides=40)
    lathe("korken", [(0.001, 1.3), (0.2, 1.3), (0.22, 1.55), (0.001, 1.57)], mat("Kork", (0.5, 0.33, 0.18), rough=0.9, bump=1.0, bump_scale=120.0), sides=24)
    for o in bpy.data.objects:
        if o.type == "MESH":
            rotate(o, 0, 25, 0)


def honey_jar(color):
    lathe("topf", [(0.001, 0.0), (0.45, 0.02), (0.55, 0.45), (0.42, 0.75), (0.45, 0.82)], mat("Topf", (0.55, 0.35, 0.2), rough=0.6, bump=0.6, bump_scale=30.0))
    lathe("nektar", [(0.001, 0.7), (0.41, 0.72), (0.001, 0.76)], mat("Nektar", color, rough=0.05, emit=0.6, coat=1.0, bump=0.0))
    curve("tropfen", [(0.42, 0.0, 0.8), (0.5, 0.0, 0.6), (0.53, 0.0, 0.4)], 0.05, mat("Nektar2", color, rough=0.05, emit=0.6, coat=1.0, bump=0.0), radii=[1.0, 0.8, 1.2])


def drop(color, glow=2.0):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=40, v_segments=30, radius=0.5)
    for v in bm.verts:
        z = v.co.z
        if z > 0:
            f = 1.0 - (z / 0.5) ** 1.2 * 0.85
            v.co.x *= f
            v.co.y *= f
            v.co.z = z * 1.9
    obj("tropfen", bm, mat("Tropfen", color, rough=0.05, emit=glow, coat=1.0, bump=0.0))


def mushroom(cap, stem=(0.85, 0.8, 0.7), tubes=False, puff=False, dust=None):
    if puff:
        blob("bovist", mat("Bovist", cap, rough=0.8, bump=1.0, bump_scale=120.0), (0, 0, 0.5), (0.6, 0.6, 0.5), rough=0.1)
    elif tubes:
        m = mat("Morchel", cap, rough=0.7, bump=0.6, bump_scale=80.0)
        for k, (x, y, h) in enumerate(((0, 0, 1.1), (0.35, 0.15, 0.8), (-0.3, 0.2, 0.7))):
            lathe("rohr%d" % k, [(0.12, 0.0), (0.15, h * 0.7), (0.2, h), (0.14, h + 0.02)], m, at=Vector((x, y, 0)), sides=24)
    else:
        lathe("stiel", [(0.13, 0.0), (0.1, 0.5), (0.09, 0.75), (0.001, 0.76)], mat("Stiel", stem, rough=0.7))
        lathe("hut", [(0.001, 0.62), (0.55, 0.64), (0.52, 0.8), (0.32, 1.02), (0.001, 1.08)], mat("Hut", cap, rough=0.5, bump=0.5))
    if dust:
        heap(dust, 2)
        for o in bpy.data.objects:
            if o.name.startswith(("haufen", "k")) and o.type == "MESH":
                o.scale = (0.6, 0.6, 0.6)
                o.location.x += 0.7
                o.location.y -= 0.3


def mycel(color):
    random.seed(9)
    m = mat("Myzel", color, rough=0.6, emit=0.3, emit_color=(0.7, 0.4, 1.0))
    for k in range(26):
        a = random.uniform(0, 6.28)
        pts = [(0, 0, 0)]
        p = Vector((0, 0, 0))
        d = Vector((math.cos(a), math.sin(a), random.uniform(-0.2, 0.4))).normalized()
        for i in range(5):
            d = (d + Vector((random.uniform(-0.5, 0.5), random.uniform(-0.5, 0.5), random.uniform(-0.3, 0.3)))).normalized()
            p = p + d * 0.2
            pts.append(tuple(p))
        curve("faden%d" % k, pts, 0.012, m)
    blob("knoten", m, (0, 0, 0), (0.18, 0.18, 0.12), rough=0.2)


def crystal(color, seed, glow=0.8, count=5):
    random.seed(seed)
    m = mat("Kristall", color, rough=0.12, emit=glow, coat=1.0, bump=0.05)
    for k in range(count):
        h = 1.7 if k == 0 else random.uniform(0.7, 1.3)
        r = 0.32 if k == 0 else random.uniform(0.15, 0.24)
        bm = bmesh.new()
        sides = 6
        base = [bm.verts.new((math.cos(2 * math.pi * i / sides) * r, math.sin(2 * math.pi * i / sides) * r, -0.2)) for i in range(sides)]
        mid = [bm.verts.new((math.cos(2 * math.pi * i / sides) * r, math.sin(2 * math.pi * i / sides) * r, h * 0.72)) for i in range(sides)]
        tip = bm.verts.new((0, 0, h))
        for i in range(sides):
            bm.faces.new((base[i], base[(i + 1) % sides], mid[(i + 1) % sides], mid[i]))
            bm.faces.new((mid[i], mid[(i + 1) % sides], tip))
        bm.faces.new(list(reversed(base)))
        tilt = Matrix.Identity(3) if k == 0 else Matrix.Rotation(random.uniform(-0.7, 0.7), 3, "X") @ Matrix.Rotation(random.uniform(-0.7, 0.7), 3, "Y")
        shift = Vector((0, 0, 0)) if k == 0 else Vector((random.uniform(-0.3, 0.3), random.uniform(-0.3, 0.3), 0))
        for v in bm.verts:
            v.co = tilt @ v.co + shift
        obj("c%d" % k, bm, m, smooth=False)


def moonstone(color):
    blob("mondstein", mat("Mondstein", color, rough=0.08, emit=1.5, coat=1.0, bump=0.0), (0, 0, 0), (0.45, 0.45, 0.75), rough=0.05, subdiv=5)
    for o in bpy.data.objects:
        if o.type == "MESH":
            for v in o.data.vertices:
                if v.co.z > 0:
                    f = 1.0 - (v.co.z / 0.75) ** 1.3 * 0.8
                    v.co.x *= f
                    v.co.y *= f


def seeds(color, seed, size=0.16, count=9, shape=(1.0, 0.75, 1.3)):
    random.seed(seed)
    m = mat("Samen", color, rough=0.4, coat=0.4, bump=0.3, bump_scale=80.0)
    for k in range(count):
        a, r = random.uniform(0, 6.28), random.uniform(0, 0.55)
        o = blob("s%d" % k, m, (0, 0, 0), (size * shape[0], size * shape[1], size * shape[2]), rough=0.08, seed=k, subdiv=3)
        rotate(o, random.uniform(-60, 60), random.uniform(-60, 60), random.uniform(0, 360), (math.cos(a) * r, math.sin(a) * r, size * 0.8 + random.uniform(0, 0.1)))


def potion(color, rarity, glow=0.9):
    glass = mat("Flasche", (0.82, 0.9, 0.92), rough=0.04, alpha=0.3, coat=1.0, bump=0.0)
    liquid = mat("Trank", color, rough=0.1, emit=glow, coat=0.6, bump=0.0)
    if rarity == "COMMON":
        prof = [(0.001, 0.0), (0.42, 0.02), (0.6, 0.4), (0.58, 0.7), (0.24, 1.05), (0.2, 1.35), (0.24, 1.42)]
        fill = [(0.001, 0.03), (0.38, 0.05), (0.54, 0.4), (0.5, 0.66), (0.001, 0.67)]
    elif rarity == "MAGIC":
        prof = [(0.001, 0.0), (0.36, 0.02), (0.38, 0.9), (0.18, 1.15), (0.15, 1.45), (0.2, 1.5)]
        fill = [(0.001, 0.03), (0.32, 0.05), (0.34, 0.75), (0.001, 0.76)]
    else:
        prof = [(0.001, 0.0), (0.3, 0.02), (0.55, 0.3), (0.38, 0.62), (0.52, 0.85), (0.18, 1.15), (0.14, 1.42), (0.26, 1.5), (0.2, 1.56)]
        fill = [(0.001, 0.03), (0.27, 0.05), (0.5, 0.3), (0.34, 0.6), (0.46, 0.8), (0.001, 0.81)]
    lathe("inhalt", fill, liquid)
    lathe("flasche", prof, glass)
    top = prof[-1][1]
    lathe("korken", [(0.001, top - 0.12), (0.17, top - 0.12), (0.2, top + 0.14), (0.001, top + 0.16)], mat("Kork", (0.5, 0.33, 0.18), rough=0.9, bump=1.0, bump_scale=120.0))
    gold = mat("Siegel", (0.85, 0.62, 0.25), rough=0.3, metal=1.0, bump=0.0)
    lathe("hals", [(0.17, top - 0.32), (0.22, top - 0.3), (0.22, top - 0.22), (0.17, top - 0.2)], gold)
    if rarity != "COMMON":
        lathe("band", [(0.39 if rarity == "MAGIC" else 0.53, 0.3), (0.42 if rarity == "MAGIC" else 0.56, 0.32), (0.42 if rarity == "MAGIC" else 0.56, 0.4), (0.39 if rarity == "MAGIC" else 0.53, 0.42)], gold)


# Albrun's seeds, each as described
def seed_special(name, color):
    if name == "Samen_Funkenmohn":
        lathe("kapsel", [(0.001, 0.0), (0.35, 0.1), (0.45, 0.5), (0.3, 0.85), (0.001, 0.9)], mat("Kapsel", color, rough=0.4, coat=0.3), ribs=8, rib_depth=0.1)
        for k in range(8):
            a = k / 8 * 2 * math.pi
            horn("kroenchen%d" % k, mat("Krone", (0.35, 0.12, 0.05), rough=0.5), (math.cos(a) * 0.25, math.sin(a) * 0.25, 0.88), (math.cos(a), math.sin(a), 0.6), 0.2, 0.04, curl=0.3, segments=3)
    elif name == "Samen_Glimmklee":
        m = mat("Klee", color, rough=0.4, emit=0.8, emit_color=(1.0, 0.6, 0.2), coat=0.3)
        for k in range(3):
            a = k / 3 * 2 * math.pi
            blob("lappen%d" % k, m, (math.cos(a) * 0.32, math.sin(a) * 0.32, 0), (0.36, 0.36, 0.14), rough=0.05)
    elif name == "Samen_Hainschoessling":
        lathe("eichel", [(0.001, 0.0), (0.32, 0.12), (0.4, 0.45), (0.38, 0.6), (0.001, 0.62)], mat("Holz", (0.45, 0.3, 0.15), rough=0.6, bump=0.8))
        lathe("kappe", [(0.38, 0.55), (0.44, 0.62), (0.3, 0.78), (0.001, 0.82)], mat("Kappe", (0.3, 0.2, 0.1), rough=0.9, bump=1.5, bump_scale=200.0))
        leaf((0.3, 0.9, 0.5), 1, glow=1.5)
        for o in bpy.data.objects:
            if o.name.startswith(("blatt", "mittel")):
                o.scale = (0.4, 0.4, 0.4)
                o.location = (0.1, 0.0, 1.0)
    elif name == "Samen_Milchstern":
        m = mat("Stern", (0.95, 0.93, 0.88), rough=0.15, coat=1.0)
        bm = bmesh.new()
        c = bm.verts.new((0, 0, 0.15))
        ring = []
        for k in range(10):
            a = k / 10 * 2 * math.pi
            r = 0.8 if k % 2 == 0 else 0.35
            ring.append(bm.verts.new((math.cos(a) * r, math.sin(a) * r, 0)))
        for k in range(10):
            bm.faces.new((c, ring[k], ring[(k + 1) % 10]))
        obj("stern", bm, m, smooth=False, solid=0.12)
    elif name == "Samen_Russrose":
        blob("samen", mat("Samt", (0.03, 0.03, 0.03), rough=0.95, bump=0.8, bump_scale=300.0), (0, 0, 0), (0.45, 0.4, 0.6), rough=0.08)
        curve("riss", [(-0.05, -0.38, -0.3), (0.05, -0.43, 0.0), (-0.03, -0.38, 0.3)], 0.04, mat("Riss", (0.3, 1.0, 0.4), emit=5.0))
    elif name == "Samen_Schlotwinde":
        m = mat("Spirale", color, rough=0.4, coat=0.3)
        curve("spirale", [(math.cos(t) * 0.08 * t, math.sin(t) * 0.08 * t, 0) for t in [k / 30 * 4 * math.pi for k in range(31)]], 0.07, m)
        curve("ranke", [(1.0, 0.0, 0), (1.3, 0.3, 0.1), (1.2, 0.6, 0.2), (0.9, 0.7, 0.2)], 0.025, m)
    elif name == "Samen_Taublatt":
        lathe("schale", [(0.001, 0.0), (0.4, 0.05), (0.6, 0.3), (0.62, 0.34)], mat("Schale", color, rough=0.5, coat=0.3))
        blob("tropfen", mat("Wasser", (0.6, 0.85, 1.0), rough=0.02, emit=0.5, coat=1.0, bump=0.0), (0, 0, 0.3), (0.25, 0.25, 0.22), rough=0.02)
    elif name == "Samen_Windhalm":
        m = mat("Silber", (0.82, 0.84, 0.86), rough=0.25, metal=0.6)
        blob("korn", m, (0, 0, 0), (0.12, 0.12, 0.6), rough=0.04)
        curve("granne", [(0, 0, 0.55), (0.02, 0, 1.2), (0.08, 0, 1.8)], 0.012, m)
        for o in bpy.data.objects:
            if o.type in ("MESH", "CURVE"):
                o.rotation_euler = (0, math.radians(55), 0)
    else:
        return False
    return True


# ---------------------------------------------------------------- what each material looks like

EMBER = (1.0, 0.38, 0.08)
VIOLET = (0.6, 0.2, 1.0)


def build(e, index):
    name = e["asset"].replace("DA_Material_", "")
    c = tuple(e["color"])
    lower = name.lower()
    if name in MODELS:
        model(MODELS[name])
        return
    if name.startswith("Trank_") or name in ("Heiltrank", "Klartrunk", "Nidtrunk"):
        potion(c, e["rarity"])
        return
    if seed_special(name, c):
        return
    table = {
        "Krustenplatte": lambda: plates((0.06, 0.055, 0.055), EMBER, 4.0),
        "GehaertetePlatte": lambda: plates((0.09, 0.08, 0.08), EMBER, 6.0, count=2),
        "Brutkrone": lambda: crown((0.06, 0.05, 0.06), VIOLET, vein=True),
        "Glutkrone": lambda: crown((0.07, 0.05, 0.04), EMBER),
        "Rollpanzer": lambda: band((0.18, 0.16, 0.15), EMBER, 2.5),
        "Rollpanzerkern": lambda: band((0.12, 0.1, 0.09), EMBER, 5.0),
        "Kriechersehne": lambda: coil((0.55, 0.25, 0.18), thick=0.08),
        "Rollsehne": lambda: coil((0.45, 0.35, 0.3), thick=0.1, loops=2.4),
        "Spannfaden": lambda: coil((0.85, 0.8, 0.6), thick=0.025, loops=4.0, shine=0.2),
        "Puppengarn": lambda: skein((0.75, 0.55, 0.25), index, glow=0.6),
        "Kokonseide": lambda: skein((0.88, 0.82, 0.72), index),
        "Russfeder": lambda: feather((0.05, 0.05, 0.06)),
        "Aeltestenschwinge": lambda: feather((0.12, 0.1, 0.1), count=3, big=True),
        "Hohlkiel": lambda: feather((0.7, 0.62, 0.5), quill_only=True),
        "Greifklaue": lambda: claws((0.1, 0.09, 0.08), count=3, curl=1.0),
        "Harpyienschnabel": lambda: beak((0.35, 0.3, 0.22)),
        "Legestachel": lambda: spike((0.12, 0.06, 0.05), length=2.2, curl=0.4, glow=0.5),
        "Klammerfinger": lambda: finger((0.55, 0.5, 0.42)),
        "Glutkiefer": lambda: jaw((0.06, 0.05, 0.05), EMBER),
        "Bruchstueck": lambda: spike((0.4, 0.35, 0.28), length=1.4, curl=0.2),
        "Sichelklinge": lambda: blade((0.12, 0.11, 0.1)),
        "Glutdruese": lambda: organ((0.35, 0.08, 0.04), EMBER, 4.0),
        "HerzDerGlut": lambda: organ((0.25, 0.05, 0.03), EMBER, 7.0, size=(0.7, 0.62, 0.8)),
        "Sichelherz": lambda: organ((0.18, 0.1, 0.2), VIOLET, 4.0, size=(0.55, 0.55, 0.6), seed=3),
        "Kelchfleisch": lambda: organ((0.55, 0.2, 0.3), VIOLET, 2.0, size=(0.75, 0.6, 0.45), seed=5),
        "Kreischbalg": lambda: organ((0.5, 0.35, 0.3), (1.0, 0.6, 0.5), 0.5, size=(0.8, 0.5, 0.5), seed=7),
        "Rotnoppe": lambda: (seal((0.7, 0.15, 0.15), emblem=False), lathe("napf", [(0.001, 0.2), (0.35, 0.17), (0.4, 0.25), (0.3, 0.3)], mat("Napf", (0.4, 0.06, 0.08), rough=0.3, coat=0.6))),
        "Glimmauge": lambda: eye((0.9, 0.7, 0.3), 2.5),
        "Koeniginnenauge": lambda: eye((1.0, 0.35, 0.05), 5.0, sclera=(0.2, 0.06, 0.03)),
        "Saftperle": lambda: pearl((0.95, 0.6, 0.15), glow=0.6),
        "Eiterperle": lambda: pearl((0.7, 0.75, 0.3), glow=0.3),
        "Schleierpelz": lambda: hide((0.05, 0.045, 0.045), index, pockets=True),
        "Blasenhaut": lambda: hide((0.6, 0.55, 0.5), index, fur=False, alpha=0.7),
        "Ordenstuch": lambda: cloth((0.25, 0.04, 0.05), (0.8, 0.6, 0.25)),
        "Ordenssiegel": lambda: seal((0.55, 0.08, 0.08)),
        "Siegelwachs": lambda: (seal((0.6, 0.1, 0.08), emblem=False), blob("tropfen", mat("Wachs2", (0.6, 0.1, 0.08), rough=0.3, coat=0.5), (0.8, 0.3, 0.05), (0.3, 0.25, 0.12), rough=0.15)),
        "Schlackeneisen": lambda: ingot((0.3, 0.29, 0.3)),
        "Messingblech": lambda: brass_sheet((0.8, 0.58, 0.24)),
        "Harpyientand": lambda: trinkets(index),
        "Latex": lambda: rolled((0.85, 0.78, 0.6)),
        "SchwarzerLatex": lambda: rolled((0.05, 0.04, 0.05)),
        "Russglas": lambda: lens((0.35, 0.33, 0.32)),
        "Schlackenglas": lambda: glass_balls((0.15, 0.12, 0.1)),
        "Rauchblase": lambda: bubble((0.4, 0.4, 0.42)),
        "Narbenholz": lambda: wood((0.25, 0.15, 0.12), VIOLET, 2.0),
        "Glimmstroh": lambda: straw((0.35, 0.22, 0.1), 0.8),
        "Glockenglut": lambda: bell((0.9, 0.55, 0.2), glow=2.0),
        "Karminglocke": lambda: bell((0.75, 0.12, 0.25)),
        "Welkglocke": lambda: bell((0.35, 0.06, 0.08), wilted=True),
        "Russmoos": lambda: tuft((0.12, 0.13, 0.1), index),
        "Nidblatt": lambda: leaf((0.12, 0.04, 0.16), index, tip=(0.9, 0.1, 0.1)),
        "Hainblatt": lambda: leaf((0.35, 0.9, 0.55), index, glow=1.5, broad=True),
        "Rankenkammblatt": lambda: leaf(c, index, broad=True),
        "Wuergranke": lambda: vine((0.35, 0.12, 0.4), index),
        "Myzelprobe": lambda: mycel((0.92, 0.9, 0.95)),
        "Pfeifensporen": lambda: mushroom((0.55, 0.22, 0.1), tubes=True, dust=(0.55, 0.2, 0.08)),
        "Staeublingspulver": lambda: mushroom((0.55, 0.52, 0.5), puff=True, dust=(0.5, 0.5, 0.5)),
        "Nidkristall": lambda: crystal((0.7, 0.05, 0.25), index, glow=1.5),
        "GelaeuterterKristall": lambda: crystal((0.75, 0.9, 1.0), index, glow=1.2),
        "Mondtropfen": lambda: moonstone((0.8, 0.85, 1.0)),
        "Lichttropfen": lambda: drop((0.95, 0.8, 0.35), glow=2.0),
        "Kelchtau": lambda: drop((0.5, 0.95, 0.85), glow=2.5),
        "Blutschuppen": lambda: heap((0.6, 0.06, 0.08), index, sparkle=True),
        "Traenenglitzer": lambda: heap((0.6, 0.85, 1.0), index, sparkle=True),
        "Sternpollen": lambda: heap((0.95, 0.92, 0.75), index, sparkle=True),
        "Gallpollen": lambda: heap((0.08, 0.06, 0.1), index, sparkle=True),
        "Glimmstaub": lambda: heap((0.9, 0.7, 0.2), index, sparkle=True),
        "Ueberraschungssaat": lambda: seeds((0.45, 0.4, 0.3), index, size=0.22, count=6, shape=(1.0, 1.0, 1.4)),
        "Kelchkapsel": lambda: lathe("kapsel", [(0.001, 0.0), (0.3, 0.1), (0.45, 0.6), (0.3, 1.0), (0.001, 1.05)], mat("Kapsel", c, rough=0.5, coat=0.3, bump=0.5), ribs=6, rib_depth=0.15),
    }
    if name in table:
        table[name]()
        return
    if "nektar" in lower:
        honey_jar(c)
    elif "tau" in lower or "wasser" in lower:
        vial(c, glow=0.5)
    elif lower in ("gallmilch", "sternmilch"):
        vial(c, glow=0.2, milky=True)
    elif lower in ("kelchschleim", "narbensaft", "verdauungssaeure"):
        vial(c, glow=1.0)
    elif "blatt" in lower and "bluete" in lower or lower in ("blutblatt", "schwarzmohnblatt"):
        petals(c, index)
    elif "blatt" in lower:
        leaf(c, index, glow=0.6 if "glimm" in lower or "schwel" in lower else 0.0)
    elif "pollen" in lower or "staub" in lower:
        heap(c, index)
    elif "samen" in lower or "funken" in lower:
        seeds(c, index)
    elif lower in ("distelflaum",):
        tuft((0.9, 0.88, 0.82), index)
    elif lower in ("fluesterfaser", "rankenfaser", "silberhalm"):
        fibres(c, index)
    else:
        blob("klumpen", mat("Klumpen", c, rough=0.6), (0, 0, 0), (0.6, 0.5, 0.45), rough=0.3, seed=index)
        print("BILD ohne eigene Form:", name, flush=True)


entries = json.load(open(LIST, encoding="utf-8"))
os.makedirs(OUT, exist_ok=True)
done = 0
for index, entry in enumerate(entries):
    name = entry["asset"].replace("DA_Material_", "")
    if ONLY and name not in ONLY:
        continue
    reset()
    try:
        build(entry, index)
        studio()
    except Exception as error:  # one broken shape must not stop the rest
        print("BILD FEHLER", name, error, flush=True)
        continue
    bpy.context.scene.render.filepath = os.path.join(OUT, "T_Icon_" + name + ".png")
    bpy.ops.render.render(write_still=True)
    done += 1
    print("BILD", name, flush=True)
print("BILDER FERTIG", done, flush=True)
