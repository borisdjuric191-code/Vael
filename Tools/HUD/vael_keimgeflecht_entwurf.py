# Entwurf (Stufe A) des Keimgeflechts, des Menüs der Floromantin (Boris 2026-11-04): sie nimmt Pflanzen in ihr Erbgut auf,
# wie eine Evolution (Anlehnung an die Zerg-Evolution in StarCraft 2) und wie alte Naturkunde-Tafeln. Nur ein Bild zum
# Besprechen, nicht im Spiel. Kein Buch (das bleibt dem Magier).
#
#   links   Herbarium: Tafeln aller Pflanzen, die sie kennt; unbekannte als Schattenriss mit "?"
#   Mitte   das Keimgeflecht: zwei verflochtene Ranken (Licht und Schatten) wie ein Erbstrang, daran die Samenkapseln
#           der Radplätze (4 offen, 5-8 verschlossen)
#   rechts  die große Tafel der gewählten Pflanze: Bild mit Fig.-Beschriftung, Schattenform, Erbanlagen, Kreuzungen
#
# blender.exe -b --factory-startup -P Tools/HUD/vael_keimgeflecht_entwurf.py -- <Ausgabedatei.png>

import math
import os
import sys

import bpy
from mathutils import Vector

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PLANTS = os.path.join(ROOT, "Scripts", "Data", "HUD", "Pflanzen")
FONTS = os.path.join(ROOT, "Scripts", "Data", "Fonts")
W, H = 1920, 1080


def P(x, y, depth=0.0):
    """Bildpunkt (von oben links) in die Szene: X rechts, Z hoch, Y in die Tiefe (kleiner = näher)"""
    return Vector((x - W / 2, depth, H / 2 - y))


# ---------------------------------------------------------------- Szene

def setup():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 48
    scene.cycles.use_denoising = True
    scene.cycles.transparent_max_bounces = 64
    scene.render.resolution_x, scene.render.resolution_y = W, H
    scene.render.image_settings.file_format = "PNG"
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.exposure = -0.4
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = W
    cam_data.clip_end = 6000
    cam = bpy.data.objects.new("Cam", cam_data)
    cam.location = (0, -2000, 0)
    cam.rotation_euler = (math.radians(90), 0, 0)
    bpy.context.collection.objects.link(cam)
    scene.camera = cam

    def light(name, loc, energy, size, color):
        data = bpy.data.lights.new(name, "AREA")
        data.energy, data.size, data.color, data.shape = energy, size, color, "DISK"
        obj = bpy.data.objects.new(name, data)
        obj.location = loc
        obj.rotation_euler = (Vector((0, 0, 0)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        bpy.context.collection.objects.link(obj)
    light("Key", (-1400, -1800, 1300), 2.6e7, 1600, (1.0, 0.93, 0.82))
    light("Fill", (1600, -1600, -400), 0.9e7, 2000, (0.8, 0.9, 1.0))
    world = bpy.data.worlds.new("W")
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.02, 0.02, 0.02, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.4


# ---------------------------------------------------------------- Materialien

def mat(name, color, rough=0.6, metal=0.0, emit=0.0, alpha=1.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    if emit > 0:
        b.inputs["Emission Color"].default_value = (*color, 1)
        b.inputs["Emission Strength"].default_value = emit
    if alpha < 1:
        b.inputs["Alpha"].default_value = alpha
    return m


def noisy(name, dark, light, scale, detail=6.0, rough=0.8, stripes=0.0, emit=0.0):
    """Holz, Papier, Moos: zwei Farben, gemischt von Rauschen (mit Streifen für Holz)"""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    coord = nt.nodes.new("ShaderNodeTexCoord")
    mapping = nt.nodes.new("ShaderNodeMapping")
    mapping.inputs["Scale"].default_value = (1.0 + stripes * 12.0, 1.0, 1.0)
    nt.links.new(coord.outputs["Object"], mapping.inputs["Vector"])
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = scale
    noise.inputs["Detail"].default_value = detail
    noise.inputs["Roughness"].default_value = 0.6
    nt.links.new(mapping.outputs["Vector"], noise.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.3
    ramp.color_ramp.elements[0].color = (*dark, 1)
    ramp.color_ramp.elements[1].position = 0.75
    ramp.color_ramp.elements[1].color = (*light, 1)
    nt.links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], b.inputs["Base Color"])
    if emit > 0:
        nt.links.new(ramp.outputs["Color"], b.inputs["Emission Color"])
        b.inputs["Emission Strength"].default_value = emit
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.25
    nt.links.new(noise.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], b.inputs["Normal"])
    b.inputs["Roughness"].default_value = rough
    return m


def picture_mat(path, emit=0.35, grey=False):
    m = bpy.data.materials.new(os.path.basename(path))
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(path)
    out = tex.outputs["Color"]
    if grey:
        # Verschlossen: entfärbt und dunkel, ein Schattenriss
        hue = nt.nodes.new("ShaderNodeHueSaturation")
        hue.inputs["Saturation"].default_value = 0.0
        hue.inputs["Value"].default_value = 0.35
        nt.links.new(out, hue.inputs["Color"])
        out = hue.outputs["Color"]
    nt.links.new(out, b.inputs["Base Color"])
    nt.links.new(out, b.inputs["Emission Color"])
    b.inputs["Emission Strength"].default_value = emit
    b.inputs["Roughness"].default_value = 0.9
    return m


BRONZE = None
GOLD = None
INK = None


def aged_paper(name):
    """Altes Papier: warme Grundfarbe, große Flecken und feine Fasern, zu den Rändern hin dunkler und fleckiger"""
    m = noisy(name, (0.55, 0.42, 0.26), (0.82, 0.71, 0.52), 0.012, rough=0.95, emit=0.07)
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    ramp_out = b.inputs["Base Color"].links[0].from_socket
    coord = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(coord.outputs["Generated"], sep.inputs[0])
    edge = None
    for axis in ("X", "Z"):
        centred = nt.nodes.new("ShaderNodeMath")
        centred.operation = "SUBTRACT"
        centred.inputs[1].default_value = 0.5
        nt.links.new(sep.outputs[axis], centred.inputs[0])
        absolute = nt.nodes.new("ShaderNodeMath")
        absolute.operation = "ABSOLUTE"
        nt.links.new(centred.outputs[0], absolute.inputs[0])
        if edge is None:
            edge = absolute
        else:
            biggest = nt.nodes.new("ShaderNodeMath")
            biggest.operation = "MAXIMUM"
            nt.links.new(edge.outputs[0], biggest.inputs[0])
            nt.links.new(absolute.outputs[0], biggest.inputs[1])
            edge = biggest
    stains = nt.nodes.new("ShaderNodeTexNoise")
    stains.inputs["Scale"].default_value = 3.0
    stains.inputs["Detail"].default_value = 3.0
    spots = nt.nodes.new("ShaderNodeMath")
    spots.operation = "MULTIPLY_ADD"
    nt.links.new(stains.outputs["Fac"], spots.inputs[0])
    spots.inputs[1].default_value = 0.18
    nt.links.new(edge.outputs[0], spots.inputs[2])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.38
    ramp.color_ramp.elements[0].color = (1, 1, 1, 1)
    ramp.color_ramp.elements[1].position = 0.6
    ramp.color_ramp.elements[1].color = (0.42, 0.3, 0.18, 1)
    nt.links.new(spots.outputs[0], ramp.inputs["Fac"])
    multiply = nt.nodes.new("ShaderNodeMix")
    multiply.data_type = "RGBA"
    multiply.blend_type = "MULTIPLY"
    multiply.inputs[0].default_value = 1.0
    nt.links.new(ramp_out, multiply.inputs[6])
    nt.links.new(ramp.outputs["Color"], multiply.inputs[7])
    nt.links.new(multiply.outputs[2], b.inputs["Base Color"])
    nt.links.new(multiply.outputs[2], b.inputs["Emission Color"])
    return m


def materials():
    global BRONZE, GOLD, INK, PAPER, PAPER_DARK, WOOD, MOSS, LIGHTVINE, SHADOWVINE, GLASS, SEAL, DUSK
    global BARK, LEAF, LEAF_DARK, PETAL, POLLEN, POLLEN_GLOW, VINE_GLOW, MUSHROOM, THORN, CAPSULE_GLASS
    BRONZE = mat("Bronze", (0.16, 0.30, 0.24), 0.45, 0.85)
    GOLD = mat("Gold", (0.85, 0.62, 0.25), 0.3, 1.0)
    INK = mat("Tinte", (0.13, 0.08, 0.05), 0.9)
    PAPER = noisy("Papier", (0.60, 0.48, 0.32), (0.80, 0.70, 0.52), 0.012, rough=0.95, emit=0.08)
    PAPER_DARK = noisy("PapierDunkel", (0.08, 0.07, 0.05), (0.15, 0.12, 0.09), 0.03)
    WOOD = noisy("Wurzelholz", (0.035, 0.025, 0.018), (0.11, 0.075, 0.05), 0.006, stripes=1.0)
    MOSS = noisy("Moos", (0.02, 0.06, 0.02), (0.06, 0.16, 0.04), 0.05, rough=0.95)
    LIGHTVINE = mat("Lichtranke", (0.25, 0.62, 0.30), 0.5, 0.0, emit=0.5)
    SHADOWVINE = mat("Schattenranke", (0.18, 0.05, 0.16), 0.5, 0.0, emit=0.25)
    GLASS = mat("Kapsel", (0.55, 0.9, 0.7), 0.15, 0.0, emit=0.15)
    SEAL = mat("Siegel", (0.35, 0.08, 0.08), 0.5)
    DUSK = mat("Daemmer", (0.45, 0.35, 0.55), 0.5, 0.0, emit=0.6)
    PAPER = aged_paper("AltesPapier")
    WOOD = noisy("Wurzelholz", (0.025, 0.018, 0.012), (0.10, 0.068, 0.045), 0.004, detail=10.0, stripes=1.6)
    BARK = noisy("Rinde", (0.05, 0.035, 0.022), (0.16, 0.11, 0.07), 0.08, detail=8.0, stripes=0.6)
    LIGHTVINE = noisy("Lichtranke", (0.12, 0.34, 0.12), (0.32, 0.66, 0.30), 0.12, rough=0.5, stripes=0.4, emit=0.35)
    SHADOWVINE = noisy("Schattenranke", (0.06, 0.015, 0.05), (0.22, 0.06, 0.18), 0.12, rough=0.45, stripes=0.4, emit=0.15)
    LEAF = noisy("Blatt", (0.08, 0.26, 0.07), (0.25, 0.55, 0.18), 0.2, rough=0.5, emit=0.25)
    LEAF_DARK = noisy("BlattDunkel", (0.03, 0.09, 0.03), (0.09, 0.22, 0.07), 0.2, rough=0.6)
    PETAL = mat("Bluete", (0.95, 0.55, 0.72), 0.5, emit=0.6)
    POLLEN = mat("Pollen", (1.0, 0.95, 0.7), 0.5, emit=6.0)
    POLLEN_GLOW = glow_mat("Pollenschein", (0.9, 1.0, 0.6), 2.0)
    VINE_GLOW = glow_mat("Strangschein", (0.35, 0.9, 0.5), 0.45)
    MUSHROOM = mat("Pilz", (0.35, 0.95, 0.85), 0.4, emit=2.5)
    THORN = mat("Dorn", (0.12, 0.07, 0.05), 0.4)
    CAPSULE_GLASS = bpy.data.materials.new("Kapselglas")
    CAPSULE_GLASS.use_nodes = True
    b = CAPSULE_GLASS.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (0.8, 1.0, 0.9, 1)
    b.inputs["Roughness"].default_value = 0.05
    b.inputs["Transmission Weight"].default_value = 1.0
    b.inputs["IOR"].default_value = 1.2


# ---------------------------------------------------------------- Formen

def quad(name, x0, y0, x1, y1, material, depth=0.0):
    mesh = bpy.data.meshes.new(name)
    a, b, c, d = P(x0, y1, depth), P(x1, y1, depth), P(x1, y0, depth), P(x0, y0, depth)
    mesh.from_pydata([a, b, c, d], [], [(0, 1, 2, 3)])
    mesh.uv_layers.new()
    for loop, uv in zip(mesh.uv_layers[0].data, [(0, 0), (1, 0), (1, 1), (0, 1)]):
        loop.uv = uv
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material)
    return obj


def slab(name, x0, y0, x1, y1, material, depth=0.0, thick=8.0, bevel=10.0):
    """Eine Tafel mit runden Ecken: flacher Quader mit Fase"""
    bpy.ops.mesh.primitive_cube_add(size=1)
    obj = bpy.context.object
    obj.name = name
    obj.location = P((x0 + x1) / 2, (y0 + y1) / 2, depth)
    obj.scale = (x1 - x0, thick, y1 - y0)
    bpy.ops.object.transform_apply(scale=True)
    mod = obj.modifiers.new("Fase", "BEVEL")
    mod.width = bevel
    mod.segments = 4
    obj.data.materials.append(material)
    return obj


def tube(name, points, radius, material, depth=0.0, taper=False):
    curve = bpy.data.curves.new(name, "CURVE")
    curve.dimensions = "3D"
    curve.bevel_depth = radius
    curve.bevel_resolution = 4
    spline = curve.splines.new("POLY")
    spline.points.add(len(points) - 1)
    for i, (x, y) in enumerate(points):
        p = P(x, y, depth)
        spline.points[i].co = (p.x, p.y, p.z, 1)
        if taper:
            spline.points[i].radius = max(0.15, 1.0 - i / len(points))
    obj = bpy.data.objects.new(name, curve)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material)
    return obj


def ring(name, x, y, r, thick, material, depth=-4.0):
    bpy.ops.mesh.primitive_torus_add(major_radius=r, minor_radius=thick, major_segments=64, minor_segments=12)
    obj = bpy.context.object
    obj.name = name
    obj.location = P(x, y, depth)
    obj.rotation_euler = (math.radians(90), 0, 0)
    obj.data.materials.append(material)
    return obj


def disc(name, x, y, r, material, depth=0.0):
    bpy.ops.mesh.primitive_circle_add(vertices=64, radius=r, fill_type="NGON")
    obj = bpy.context.object
    obj.name = name
    obj.location = P(x, y, depth)
    obj.rotation_euler = (math.radians(90), 0, 0)
    # UV wie ein Bild über den Kreis
    mesh = obj.data
    mesh.uv_layers.new()
    for poly in mesh.polygons:
        for li in poly.loop_indices:
            v = mesh.vertices[mesh.loops[li].vertex_index].co
            mesh.uv_layers[0].data[li].uv = (v.x / (2 * r) + 0.5, v.y / (2 * r) + 0.5)
    obj.data.materials.append(material)
    return obj


def text(body, x, y, size, material, font="Almendra-Regular.ttf", align="LEFT", depth=-12.0):
    curve = bpy.data.curves.new("T", "FONT")
    curve.body = body
    curve.font = bpy.data.fonts.load(os.path.join(FONTS, font), check_existing=True)
    curve.size = size
    curve.align_x = align
    curve.align_y = "TOP"
    curve.extrude = 0.4
    obj = bpy.data.objects.new("T", curve)
    obj.location = P(x, y, depth)
    obj.rotation_euler = (math.radians(90), 0, 0)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material)
    return obj


def glow_mat(name, color, strength):
    """Weicher Lichthof: in der Mitte hell, nach außen durchsichtig (für Pollen, Leuchten hinter dem Strang)"""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    coord = nt.nodes.new("ShaderNodeTexCoord")
    mapping = nt.nodes.new("ShaderNodeMapping")
    mapping.inputs["Location"].default_value = (-1.0, -1.0, 0.0)
    mapping.inputs["Scale"].default_value = (2.0, 2.0, 0.0)
    nt.links.new(coord.outputs["UV"], mapping.inputs["Vector"])
    grad = nt.nodes.new("ShaderNodeTexGradient")
    grad.gradient_type = "SPHERICAL"
    nt.links.new(mapping.outputs["Vector"], grad.inputs["Vector"])
    power = nt.nodes.new("ShaderNodeMath")
    power.operation = "POWER"
    power.inputs[1].default_value = 2.2
    nt.links.new(grad.outputs["Fac"], power.inputs[0])
    emission = nt.nodes.new("ShaderNodeEmission")
    emission.inputs["Color"].default_value = (*color, 1)
    emission.inputs["Strength"].default_value = strength
    transparent = nt.nodes.new("ShaderNodeBsdfTransparent")
    mix = nt.nodes.new("ShaderNodeMixShader")
    nt.links.new(power.outputs[0], mix.inputs["Fac"])
    nt.links.new(transparent.outputs[0], mix.inputs[1])
    nt.links.new(emission.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    return m


def glow(x, y, r, material, depth=20.0):
    obj = quad("Schein", x - r, y - r, x + r, y + r, material, depth=depth)
    obj.visible_shadow = False
    return obj


def leaf(x, y, angle, length, width, material, depth=-12.0, bend=0.25):
    """Ein Blatt mit Spitze und Mittelrippe, leicht gewölbt; angle in Grad, 0 = nach rechts, im Bild gemessen"""
    n = 10
    verts, faces = [], []
    a = math.radians(angle)
    ax, ay = math.cos(a), math.sin(a)
    for i in range(n + 1):
        t = i / n
        half = width * 0.5 * math.sin(math.pi * min(t * 1.08, 1.0)) ** 0.8
        cx, cy = x + ax * length * t, y + ay * length * t
        # die Spitze biegt sich ein wenig
        cx += -ay * bend * length * t * t
        cy += ax * bend * length * t * t
        for side, lift in ((-1, 2.0), (0, -1.5), (1, 2.0)):
            px, py = cx - ay * half * side, cy + ax * half * side
            verts.append(P(px, py, depth + lift * (1 - abs(side) * 0.0)))
    for i in range(n):
        for j in range(2):
            a0 = i * 3 + j
            faces.append((a0, a0 + 1, a0 + 4, a0 + 3))
    mesh = bpy.data.meshes.new("Blatt")
    mesh.from_pydata(verts, [], faces)
    obj = bpy.data.objects.new("Blatt", mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material)
    for poly in mesh.polygons:
        poly.use_smooth = True
    return obj


def blossom(x, y, r, material, centre, depth=-14.0):
    for k in range(5):
        a = k * 2 * math.pi / 5 + 0.3
        leaf(x, y, math.degrees(a), r, r * 0.9, material, depth=depth, bend=0.0)
    disc("Bluetenherz", x, y, r * 0.28, centre, depth=depth - 3)


def sphere(name, x, y, r, material, depth=0.0, squash=1.0):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=r)
    obj = bpy.context.object
    obj.name = name
    obj.location = P(x, y, depth)
    obj.scale = (1.0, squash, 1.0)
    bpy.ops.object.shade_smooth()
    obj.data.materials.append(material)
    return obj


def cutout_mat(path, emit=0.4):
    """Ein Bild mit Alpha (ihre Flügel): durchsichtig, wo das Bild durchsichtig ist"""
    m = picture_mat(path, emit)
    nt = m.node_tree
    tex = [n for n in nt.nodes if n.type == "TEX_IMAGE"][0]
    nt.links.new(tex.outputs["Alpha"], nt.nodes["Principled BSDF"].inputs["Alpha"])
    return m


def moth_emblem(x, y, scale):
    """Ihre Motte als Herz des Geflechts: ihr eigenes Flügelbild, gespiegelt, mit einem Bronzeleib"""
    path = os.path.join(ROOT, "Scripts", "Data", "T_Floromant_Fluegel.png")
    wing = cutout_mat(path, emit=0.6)
    w, h = 64 * scale, 96 * scale
    right = quad("FluegelR", x, y - h * 0.45, x + w, y + h * 0.55, wing, depth=-20)
    left = quad("FluegelL", x - w, y - h * 0.45, x, y + h * 0.55, wing, depth=-20)
    for loop in left.data.uv_layers[0].data:
        loop.uv = (1.0 - loop.uv[0], loop.uv[1])
    body = mat("Leib", (0.35, 0.25, 0.12), 0.35, 0.9)
    tube("Leib", [(x, y - 26 * scale), (x, y + 30 * scale)], 4.5 * scale, body, depth=-24, taper=False)
    sphere("Kopf", x, y - 30 * scale, 5.5 * scale, body, depth=-24)
    for side in (-1, 1):
        tube("Fuehler", [(x, y - 34 * scale), (x + side * 8 * scale, y - 48 * scale), (x + side * 16 * scale, y - 54 * scale)], 1.2 * scale, GOLD, depth=-24)
    glow(x, y, 110 * scale, glow_mat("Mottenschein", (1.0, 0.75, 0.85), 1.2), depth=10)


def alive_room():
    """Die Kammer lebt: Wurzeln kriechen am Rahmen entlang, Moos und leuchtende Pilze in den Ecken, Pollen schweben"""
    import random
    rnd = random.Random(1104)
    # Wurzeln am Rahmen: aus den Ecken, knorrig, hinter den Tafeln
    for (sx, sy), (ex, ey), (bx, by) in [((40, 1040), (520, 1060), (240, 1000)), ((1880, 1040), (1380, 1062), (1640, 990)),
                                         ((40, 40), (40, 520), (110, 260)), ((1880, 40), (1880, 600), (1810, 300)),
                                         ((40, 40), (520, 30), (260, 96)), ((1880, 40), (1400, 28), (1650, 92))]:
        pts = []
        for k in range(24):
            t = k / 23
            px = (1 - t) ** 2 * sx + 2 * (1 - t) * t * bx + t * t * ex + rnd.uniform(-4, 4)
            py = (1 - t) ** 2 * sy + 2 * (1 - t) * t * by + t * t * ey + rnd.uniform(-4, 4)
            pts.append((px, py))
        tube("Kammerwurzel", pts, 13, BARK, depth=-4, taper=True)
        # feine Seitenwurzeln
        for k in range(3, 20, 5):
            px, py = pts[k]
            ang = rnd.uniform(0, 2 * math.pi)
            tube("Seitenwurzel", [(px, py), (px + math.cos(ang) * 26, py + math.sin(ang) * 26), (px + math.cos(ang + 0.5) * 50, py + math.sin(ang + 0.5) * 50)],
                 4, BARK, depth=-3, taper=True)
    # kleine, dichte Moospolster aus vielen Büscheln, dunkel mit hellen Spitzen, statt glatter Kissen
    for corner in ((80, 1020), (150, 1034), (1840, 1020), (1770, 1034), (600, 1048), (1300, 1048)):
        for k in range(55):
            px, py = corner[0] + rnd.gauss(0, 34), corner[1] + rnd.gauss(0, 9)
            sphere("Moos", px, py, rnd.uniform(2.5, 6.5), LEAF_DARK if k % 4 else LEAF, depth=-6 - rnd.uniform(0, 6), squash=0.7)
    # leuchtende Pilzchen
    for px, py, s in ((96, 1000, 1.0), (122, 1012, 0.7), (1818, 996, 0.9), (1796, 1012, 0.6), (70, 560, 0.7), (1850, 640, 0.8)):
        tube("Stiel", [(px, py + 2), (px + 2, py - 18 * s)], 2.6 * s, mat("PilzStiel", (0.7, 0.75, 0.65), 0.6, emit=0.3), depth=-10)
        sphere("Hut", px + 2, py - 20 * s, 11 * s, MUSHROOM, depth=-10, squash=0.5)
        glow(px + 2, py - 20 * s, 46 * s, glow_mat("Pilzschein", (0.3, 1.0, 0.85), 1.5), depth=-8)
    # Pollen und Feenlichter, die durch die Kammer treiben
    for k in range(40):
        px, py = rnd.uniform(70, 1850), rnd.uniform(70, 1010)
        r = rnd.uniform(1.0, 2.2)
        sphere("Pollen", px, py, r, POLLEN, depth=-30)
        glow(px, py, r * rnd.uniform(4, 7), POLLEN_GLOW, depth=-29)


def plate_dressing(x0, y0, x1, y1):
    """Die Tafel ist mit Dornen festgesteckt, hat Zierecken in Tinte und ein gepresstes Blatt"""
    for px, py in ((x0 + 14, y0 + 14), (x1 - 14, y0 + 14), (x0 + 14, y1 - 14), (x1 - 14, y1 - 14)):
        sphere("Dornkopf", px, py, 5, THORN, depth=-8)
        tube("Dorn", [(px, py), (px + 9, py + 6)], 3.0, THORN, depth=-7, taper=True)
    ink = INK
    for sx, sy, dx, dy in ((x0 + 22, y0 + 22, 1, 1), (x1 - 22, y0 + 22, -1, 1), (x0 + 22, y1 - 22, 1, -1), (x1 - 22, y1 - 22, -1, -1)):
        curl = []
        for k in range(24):
            a = k / 23 * 1.6 * math.pi
            r = 22 * (1 - k / 30)
            curl.append((sx + dx * (r + r * math.cos(a)) * 0.9, sy + dy * (r * math.sin(a) + r) * 0.9))
        tube("Zierecke", curl, 1.0, ink, depth=0)
    # gepresstes Blatt unten rechts auf der Tafel, flach und blass
    pressed = noisy("Gepresst", (0.30, 0.28, 0.12), (0.48, 0.44, 0.22), 0.3, rough=0.9, emit=0.05)
    leaf(x1 - 140, y1 - 70, -35, 110, 48, pressed, depth=4, bend=0.12)
    tube("Rippe", [(x1 - 140, y1 - 70), (x1 - 52, y1 - 132)], 0.8, ink, depth=3)


def plant(name, shadow=False):
    path = os.path.join(PLANTS, "T_Pflanze_{}{}.png".format(name, "_Schatten" if shadow else ""))
    return path if os.path.exists(path) else os.path.join(PLANTS, "T_Pflanze_{}.png".format(name))


# ---------------------------------------------------------------- Bild

def build():
    paper_ink = mat("TinteHell", (0.10, 0.06, 0.035), 0.9)
    pale = mat("Hell", (0.85, 0.78, 0.62), 0.6, emit=0.9)
    dim = mat("Dunkeltext", (0.45, 0.42, 0.36), 0.6, emit=0.5)
    green_glow = mat("GruenLicht", (0.45, 0.95, 0.55), 0.4, emit=2.0)
    violet = mat("Nid", (0.61, 0.36, 0.90), 0.4, emit=1.2)

    # Grund: Wurzelholz, unten Moos, Rahmen in Grünspan-Bronze mit Goldlinie
    quad("Grund", 0, 0, W, H, WOOD, depth=40)
    alive_room()
    for points in ([(40, 40), (W - 40, 40), (W - 40, H - 40), (40, H - 40), (40, 40)],):
        tube("Rahmen", points, 9, BRONZE, depth=-2)
    tube("Goldlinie", [(58, 58), (W - 58, 58), (W - 58, H - 58), (58, H - 58), (58, 58)], 2.2, GOLD, depth=-2)

    # Titel und Reiter
    text("Keimgeflecht", W / 2, 66, 60, mat("Titelgold", (0.95, 0.72, 0.32), 0.3, 0.6, emit=1.1), "AlmendraSC-Regular.ttf", "CENTER")
    for i, (label, active) in enumerate((("Pflanzen", True), ("Kreuzungen", False), ("Erbanlagen", False))):
        tx = W / 2 - 260 + i * 260
        text(label, tx, 146, 26, pale if active else dim, "Almendra-Regular.ttf", "CENTER")
        if active:
            tube("Reiter", [(tx - 70, 182), (tx + 70, 182)], 1.6, GOLD, depth=-6)
    text("L1", W / 2 - 430, 146, 24, dim, "AlegreyaSans-Bold.ttf", "CENTER")
    text("R1", W / 2 + 430, 146, 24, dim, "AlegreyaSans-Bold.ttf", "CENTER")

    # ---------------- links: Herbarium, Tafeln aller bekannten Pflanzen
    known = ["Windhalm", "Russrose", "Glutdistel", "Glimmklee", "Taublatt", "Schlotwinde", "Funkenmohn", "Harzkolben",
             "Feenragwurz", "Wachbeere", "Mondtraufe", None]
    names = {"Russrose": "Rußrose"}
    text("Herbarium", 380, 214, 30, pale, "AlmendraSC-Regular.ttf", "CENTER")
    for i, name in enumerate(known):
        col, row = i % 3, i // 3
        x0, y0 = 110 + col * 182, 262 + row * 180
        selected = name == "Russrose"
        slab("Karte", x0, y0, x0 + 160, y0 + 160, PAPER if name else PAPER_DARK, depth=10, bevel=8)
        # mit einem Dorn an die Wurzelwand gesteckt
        sphere("Nadel", x0 + 80, y0 + 6, 4, THORN, depth=-6)
        if name:
            quad("Bild", x0 + 22, y0 + 10, x0 + 138, y0 + 126, picture_mat(plant(name)), depth=2)
            text(names.get(name, name), x0 + 80, y0 + 128, 19, paper_ink, "Almendra-Regular.ttf", "CENTER", depth=2)
            # im Rad: ein kleiner grüner Keim in der Ecke
            if name in ("Windhalm", "Russrose", "Glutdistel", "Glimmklee"):
                disc("ImRad", x0 + 146, y0 + 14, 7, green_glow, depth=0)
        else:
            text("?", x0 + 80, y0 + 40, 70, dim, "AlmendraSC-Regular.ttf", "CENTER", depth=2)
        if selected:
            tube("Wahl", [(x0 - 6, y0 - 6), (x0 + 166, y0 - 6), (x0 + 166, y0 + 166), (x0 - 6, y0 + 166), (x0 - 6, y0 - 6)], 3.2, GOLD, depth=-2)

    # ---------------- Mitte: das Keimgeflecht, zwei verflochtene Ranken mit den Samenkapseln der Radplätze
    cx, top, bottom, amp = 818, 226, 962, 50
    steps = 160

    def strand(phase0, radius, material, leaves, thorns):
        # Die Ranke wird in Stücke geteilt: wo sie nach vorn dreht, liegt sie vor der anderen, sonst dahinter
        piece, front = [], None
        for k in range(steps + 1):
            t = k / steps
            y = top + (bottom - top) * t
            phase = t * 4.0 * math.pi + phase0
            # knorrig: die Dicke schwillt und der Weg wackelt ein wenig
            x = cx + amp * math.sin(phase) + 3.0 * math.sin(t * 37.0 + phase0)
            now = math.cos(phase) > 0
            if front is None:
                front = now
            piece.append((x, y))
            if now != front or k == steps:
                tube("Strang", piece, radius * (1.0 + 0.15 * math.sin(t * 23.0)), material, depth=-10 if front else 8)
                piece, front = [(x, y)], now
            if k % 9 == 4 and now:
                if leaves:
                    # Blätter, die von der Lichtranke abstehen, ab und zu eine kleine Blüte
                    side = 1 if math.sin(phase) > 0 else -1
                    leaf(x, y, (0 if side > 0 else 180) + side * (-30 + 25 * math.sin(k)), 26, 13, LEAF, depth=-14)
                    if k % 27 == 4:
                        blossom(x - side * 6, y + 8, 9, PETAL, GOLD)
                if thorns:
                    side = 1 if math.sin(phase) > 0 else -1
                    tube("Dorn", [(x, y), (x + side * 12, y + 3)], 2.6, material, depth=-12, taper=True)

    strand(0.0, 7.5, LIGHTVINE, True, False)
    strand(math.pi, 7.0, SHADOWVINE, False, True)
    # Sprossen zwischen den Ranken, wie die Stufen eines Erbstrangs: feine Wurzelfäden
    for k in range(0, steps, 7):
        t = k / steps
        y = top + (bottom - top) * t
        phase = t * 4.0 * math.pi
        a, b = cx + amp * math.sin(phase), cx + amp * math.sin(phase + math.pi)
        if abs(a - b) > 26:
            tube("Sprosse", [(a, y), ((a + b) / 2, y + 4), (b, y)], 1.8, BRONZE, depth=0)

    # Acht Kapseln, abwechselnd links und rechts am Strang; 1-4 tragen ihre Pflanze, 5-8 sind verschlossene Knospen
    wheel = ["Windhalm", "Russrose", "Glutdistel", "Glimmklee"]
    for i in range(8):
        y = top + 46 + i * 92
        side = -1 if i % 2 == 0 else 1
        px = cx + side * 112
        tube("Stiel", [(cx + side * 30, y + 12), (cx + side * 58, y + 2), (px - side * 40, y)], 3.0, LIGHTVINE if i < 4 else BRONZE, depth=2)
        ring("Fassung", px, y, 40, 4.0, GOLD if i == 1 else BRONZE)
        # kleine Bronzeblätter an der Fassung
        tube("Fassungsblatt", [(px - side * 30, y + 30), (px - side * 44, y + 40), (px - side * 52, y + 38)], 3.5, BRONZE, depth=-6, taper=True)
        # Bronzekelch: fünf Kelchblätter umfassen die Kapsel von hinten
        for a in range(5):
            ang = a * 72 + 90
            leaf(px, y, ang, 50, 22, BRONZE, depth=6, bend=0.1)
        if i < 4:
            disc("Kapsel", px, y, 37, picture_mat(plant(wheel[i]), emit=0.5), depth=0)
            # gläserne Samenkapsel darüber, mit einem Lichtschein
            sphere("Glas", px, y, 38, CAPSULE_GLASS, depth=-2, squash=0.35)
            glow(px, y, 70, VINE_GLOW, depth=14)
            text(str(i + 1), px + side * 56, y - 13, 22, pale, "AlegreyaSans-Bold.ttf", "CENTER")
        else:
            disc("Knospe", px, y, 37, PAPER_DARK, depth=0)
            # verschlossene Knospe: drei Kelchblätter darüber und ein Siegel
            for a in (-0.6, 0.0, 0.6):
                tube("Kelch", [(px, y + 26), (px + math.sin(a) * 18, y + 4), (px + math.sin(a) * 6, y - 24)], 6.0, BRONZE, depth=-5, taper=True)
            ring("Siegel", px, y + 18, 9, 2.6, SEAL, depth=-8)
            text(str(i + 1), px + side * 56, y - 13, 22, dim, "AlegreyaSans-Bold.ttf", "CENTER")
    text("Rad", cx, 978, 22, dim, "Almendra-Regular.ttf", "CENTER")
    # Der Strang leuchtet sanft von hinten, wächst oben aus ihrer Motte und unten mit Wurzeln ins Moos
    if True:
        tall = quad("Strangschein", cx - 150, top - 20, cx + 150, bottom + 20, VINE_GLOW, depth=24)
        tall.visible_shadow = False
    moth_emblem(cx, top - 18, 0.9)
    for k, (dx, end) in enumerate(((-40, -120), (-14, -60), (16, 50), (44, 130))):
        tube("Wurzel", [(cx + dx * 0.3, bottom - 10), (cx + dx, bottom + 18), (cx + end * 0.6, bottom + 34), (cx + end, bottom + 52)],
             4.5 - abs(dx) * 0.04, BARK, depth=-2, taper=True)

    # ---------------- rechts: die große Tafel der gewählten Pflanze
    x0, y0, x1, y1 = 1020, 214, 1840, 990
    slab("Tafel", x0, y0, x1, y1, PAPER, depth=12, bevel=14)
    plate_dressing(x0, y0, x1, y1)
    tube("Tafelrand", [(x0 + 22, y0 + 22), (x1 - 22, y0 + 22), (x1 - 22, y1 - 22), (x0 + 22, y1 - 22), (x0 + 22, y0 + 22)], 1.4, INK, depth=2)
    tube("Tafelrand2", [(x0 + 28, y0 + 28), (x1 - 28, y0 + 28), (x1 - 28, y1 - 28), (x0 + 28, y1 - 28), (x0 + 28, y0 + 28)], 0.6, INK, depth=2)
    text("Tafel II", x0 + 50, y0 + 44, 22, paper_ink, "IMFeENsc28P.ttf")
    text("Rußrose", x0 + 50, y0 + 70, 54, paper_ink, "AlmendraSC-Regular.ttf")
    text("Rosa fuliginosa · Lichtform", x0 + 52, y0 + 134, 21, paper_ink, "Almendra-Regular.ttf")

    # Das Bild in einem dunklen Fenster, mit Fig.-Beschriftung wie auf einer alten Tafel
    ix0, iy0, size = x0 + 50, y0 + 176, 300
    slab("Fenster", ix0 - 8, iy0 - 8, ix0 + size + 8, iy0 + size + 8, PAPER_DARK, depth=6, thick=4, bevel=6)
    quad("GrossBild", ix0, iy0, ix0 + size, iy0 + size, picture_mat(plant("Russrose"), emit=0.45), depth=2)
    figs = [((ix0 + 150, iy0 + 58), "Fig. 1  Herzblüte", "schießt ihr Herz in den Feind"),
            ((ix0 + 210, iy0 + 160), "Fig. 2  Dornenranke", "kriecht als Peitsche den Arm hinab"),
            ((ix0 + 110, iy0 + 266), "Fig. 3  Rußwurzel", "trinkt Asche, wo andere welken")]
    for k, ((fx, fy), title, note) in enumerate(figs):
        lx, ly = ix0 + size + 40, iy0 + 14 + k * 96
        tube("Zeiger", [(fx, fy), (ix0 + size + 6, ly + 14), (lx - 6, ly + 14)], 1.1, INK, depth=0)
        disc("Punkt", fx, fy, 4, INK, depth=0)
        text(title, lx, ly, 25, paper_ink, "Almendra-Bold.ttf")
        text(note, lx, ly + 32, 19, paper_ink, "Almendra-Regular.ttf")

    # Schattenform: verschlossen bis zum Schwur
    sx, sy = x1 - 190, y0 + 46
    disc("SchattenBild", sx + 60, sy + 56, 52, picture_mat(plant("Russrose", True), emit=0.3, grey=True), depth=2)
    ring("SchattenFassung", sx + 60, sy + 56, 54, 3.0, SEAL)
    text("Schattenform", sx + 60, sy + 118, 19, paper_ink, "Almendra-Bold.ttf", "CENTER")
    text("bis zum Schwur verschlossen", sx + 60, sy + 142, 15, paper_ink, "Almendra-Regular.ttf", "CENTER")

    # Erbanlagen: je Stufe eine von zwei Anlagen wählen (Evolution), Stufe 3 noch nicht gewachsen
    ey = y0 + 498
    tube("Trenner", [(x0 + 50, ey - 14), (x1 - 50, ey - 14)], 1.0, INK, depth=2)
    text("Erbanlagen", x0 + 50, ey, 28, paper_ink, "AlmendraSC-Regular.ttf")
    text("je Stufe eine von zwei", x0 + 270, ey + 6, 18, paper_ink, "Almendra-Regular.ttf")
    genes = [("Schwerer Duft", "Dichter Dorn", 0), ("Glimmende Asche", "Tiefe Wurzel", 1), ("?", "?", -1)]
    for k, (a, b, chosen) in enumerate(genes):
        gx = x0 + 60 + k * 255
        text("Stufe {}".format(["I", "II", "III"][k]), gx, ey + 40, 17, paper_ink, "IMFeENsc28P.ttf")
        for j, label in enumerate((a, b)):
            gy = ey + 64 + j * 40
            picked = chosen == j
            ring("Anlage", gx + 16, gy + 13, 14, 2.4, GOLD if picked else INK, depth=0)
            if picked:
                disc("AnlageVoll", gx + 16, gy + 13, 9, green_glow, depth=0)
            text(label, gx + 40, gy, 20, paper_ink, "Almendra-Regular.ttf")

    # Kreuzungen dieser Pflanze: bekannte mit Namen, unbekannte mit "?"
    ky = y0 + 648
    tube("Trenner2", [(x0 + 50, ky - 14), (x1 - 50, ky - 14)], 1.0, INK, depth=2)
    text("Kreuzungen", x0 + 50, ky, 28, paper_ink, "AlmendraSC-Regular.ttf")
    crosses = [("Glutrose", "mit Glutdistel"), ("Laube", "mit Glimmklee"), ("?", "mit ?"), ("?", "mit ?")]
    for k, (pair, partner) in enumerate(crosses):
        cx2, cy2 = x0 + 60 + (k % 2) * 380, ky + 40 + (k // 2) * 32
        text(pair, cx2, cy2, 22, paper_ink, "Almendra-Bold.ttf")
        text(partner, cx2 + 120, cy2 + 2, 20, paper_ink, "Almendra-Regular.ttf")

    # Steuerung unten: Knöpfe mit ihren Zeichen
    hints = [("Kreuz", "Ins Rad setzen"), ("Kreis", "Zurück"), ("Viereck", "Tafel wenden"), ("Dreieck", "Erbanlage wählen")]
    for k, (button, label) in enumerate(hints):
        hx, hy = 520 + k * 260, 1020
        ring("Knopf", hx, hy, 14, 2.2, GOLD, depth=-4)
        if button == "Kreuz":
            tube("Z", [(hx - 6, hy - 6), (hx + 6, hy + 6)], 1.4, pale, depth=-6)
            tube("Z", [(hx - 6, hy + 6), (hx + 6, hy - 6)], 1.4, pale, depth=-6)
        elif button == "Kreis":
            ring("Z", hx, hy, 6, 1.3, pale, depth=-6)
        elif button == "Viereck":
            tube("Z", [(hx - 6, hy - 6), (hx + 6, hy - 6), (hx + 6, hy + 6), (hx - 6, hy + 6), (hx - 6, hy - 6)], 1.3, pale, depth=-6)
        else:
            tube("Z", [(hx, hy - 7), (hx + 7, hy + 5), (hx - 7, hy + 5), (hx, hy - 7)], 1.3, pale, depth=-6)
        text(label, hx + 26, hy - 14, 22, pale, "Almendra-Regular.ttf")


def main():
    out = sys.argv[sys.argv.index("--") + 1]
    setup()
    materials()
    build()
    bpy.context.scene.render.filepath = out
    bpy.ops.render.render(write_still=True)
    print("[Vael] gerendert:", out)


main()
