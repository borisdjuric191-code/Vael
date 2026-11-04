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
    mapping.inputs["Scale"].default_value = (1.0, 1.0, 1.0 + stripes * 12.0)
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


def materials():
    global BRONZE, GOLD, INK, PAPER, PAPER_DARK, WOOD, MOSS, LIGHTVINE, SHADOWVINE, GLASS, SEAL, DUSK
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
                    # ein kleines Blatt, das von der Lichtranke absteht
                    side = 1 if math.sin(phase) > 0 else -1
                    tube("Blatt", [(x, y), (x + side * 10, y - 6), (x + side * 20, y - 4)], 4.0, material, depth=-12, taper=True)
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
        if i < 4:
            disc("Kapsel", px, y, 37, picture_mat(plant(wheel[i]), emit=0.5), depth=0)
            text(str(i + 1), px + side * 56, y - 13, 22, pale, "AlegreyaSans-Bold.ttf", "CENTER")
        else:
            disc("Knospe", px, y, 37, PAPER_DARK, depth=0)
            # verschlossene Knospe: drei Kelchblätter darüber und ein Siegel
            for a in (-0.6, 0.0, 0.6):
                tube("Kelch", [(px, y + 26), (px + math.sin(a) * 18, y + 4), (px + math.sin(a) * 6, y - 24)], 6.0, BRONZE, depth=-5, taper=True)
            ring("Siegel", px, y + 18, 9, 2.6, SEAL, depth=-8)
            text(str(i + 1), px + side * 56, y - 13, 22, dim, "AlegreyaSans-Bold.ttf", "CENTER")
    text("Rad", cx, 978, 22, dim, "Almendra-Regular.ttf", "CENTER")

    # ---------------- rechts: die große Tafel der gewählten Pflanze
    x0, y0, x1, y1 = 1020, 214, 1840, 990
    slab("Tafel", x0, y0, x1, y1, PAPER, depth=12, bevel=14)
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
