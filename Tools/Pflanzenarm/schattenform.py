"""
Schattenform einer Pflanze aus ihrer Lichtform (Boris 2026-11-05: die erste Schattenform, die Blutrose). Die Lore (Herbarium):
Schattenformen sind schwarz-violett mit tiefroten Spitzen, violette Adern an Gelenken und Rissen; im Spiel kein eigenes Modell,
nur eine Materialvariante. Dieses Skript färbt die Tripo-Textur der Lichtform um und rendert Licht- und Schattenform nebeneinander
zum Anschauen. Die Quelle wird nur gelesen (Saved/Pflanzenquellen/<Name>, von Scripts/ExportPlantSource.py).

  blender -b --factory-startup -P Tools/Pflanzenarm/schattenform.py -- <Name> <Ausgabeordner>

Schreibt T_<Name>_Schatten_basecolor.png (für das Spiel) und <Name>_Licht_und_Schatten.png (Vorschau).

Regeln je Pflanze (Farbtöne 0..1):
  Russrose -> Blutrose: dunkelrote bis violette Blüten, das grüne Herz wird blutrot und leuchtet weiter, Blätter schwarz-violett
"""
import math
import os
import sys

import bpy
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
args = sys.argv[sys.argv.index("--") + 1:]
NAME = args[0]
OUT = args[1]
SOURCE = os.path.join(ROOT, "Saved", "Pflanzenquellen", NAME)
os.makedirs(OUT, exist_ok=True)


def rgb_to_hsv(rgb):
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    mx = np.max(rgb, axis=-1)
    mn = np.min(rgb, axis=-1)
    d = mx - mn
    h = np.zeros_like(mx)
    safe = np.where(d > 1e-6, d, 1.0)
    rc = (mx - r) / safe
    gc = (mx - g) / safe
    bc = (mx - b) / safe
    h = np.where(mx == r, bc - gc, h)
    h = np.where(mx == g, 2.0 + rc - bc, h)
    h = np.where(mx == b, 4.0 + gc - rc, h)
    h = np.where(d > 1e-6, (h / 6.0) % 1.0, 0.0)
    s = np.where(mx > 1e-6, d / np.where(mx > 1e-6, mx, 1.0), 0.0)
    return h, s, mx


def hsv_to_rgb(h, s, v):
    i = np.floor(h * 6.0).astype(int) % 6
    f = h * 6.0 - np.floor(h * 6.0)
    p = v * (1 - s)
    q = v * (1 - s * f)
    t = v * (1 - s * (1 - f))
    choices_r = [v, q, p, p, t, v]
    choices_g = [t, v, v, q, p, p]
    choices_b = [p, p, t, v, v, q]
    r = np.choose(i, choices_r)
    g = np.choose(i, choices_g)
    b = np.choose(i, choices_b)
    return np.stack([r, g, b], axis=-1)


def band(h, centre, width):
    """Wie sehr ein Farbton zu einem Bereich gehört (weich, rund um den Farbkreis)"""
    d = np.abs(((h - centre + 0.5) % 1.0) - 0.5)
    return np.clip(1.0 - d / width, 0.0, 1.0)


def blutrose(rgb):
    h, s, v = rgb_to_hsv(rgb)
    out_h, out_s, out_v = h.copy(), s.copy(), v.copy()
    coloured = np.clip((s - 0.12) / 0.2, 0.0, 1.0)
    # Violett der Blüten: hell (Ränder, Spitzen) wird blutrot, dunkel bleibt schwarz-violett
    violet = band(h, 0.79, 0.12) * coloured
    bright = np.clip((v - 0.25) / 0.5, 0.0, 1.0)
    target_h = (0.80 + (1.0 - 0.80 + 0.985 - 1.0) * bright) % 1.0  # 0.80 violett -> 0.985 blutrot
    out_h = np.where(violet > 0, (h * (1 - violet) + target_h * violet) % 1.0, out_h)
    out_s = out_s * (1 - violet) + np.clip(s * 1.15, 0, 1) * violet
    out_v = out_v * (1 - violet) + v * (0.85 - 0.25 * (1 - bright)) * violet
    # Das leuchtende Türkis/Grün (Herz, innere Blätter) wird zu leuchtendem Blut
    glow = band(h, 0.44, 0.1) * coloured * np.clip((v - 0.3) / 0.3, 0.0, 1.0)
    out_h = np.where(glow > 0, (out_h * (1 - glow) + 0.995 * glow) % 1.0, out_h)
    out_s = out_s * (1 - glow) + 0.9 * glow
    out_v = out_v * (1 - glow) + np.clip(v * 0.95, 0, 1) * glow
    # Dunkles Türkis in den Schatten der Blüte (zu dunkel zum Leuchten): wird dunkles Blutrot, sonst bliebe die Rose grünstichig
    teal = band(h, 0.46, 0.12) * coloured * (1 - glow)
    out_h = np.where(teal > 0, (out_h * (1 - teal) + 0.99 * teal) % 1.0, out_h)
    out_s = out_s * (1 - teal) + 0.8 * teal
    out_v = out_v * (1 - teal) + v * 0.7 * teal
    glow = np.clip(glow, 0, 1)
    # Blattgrün wird schwarz-violett, an den hellen Stellen (Spitzen, Ränder) ein tiefrotes Glimmen
    leaf = band(h, 0.27, 0.14) * coloured * (1 - glow) * (1 - teal)
    out_h = np.where(leaf > 0, (out_h * (1 - leaf) + np.where(bright > 0.6, 0.98, 0.87) * leaf) % 1.0, out_h)
    out_s = out_s * (1 - leaf) + 0.78 * leaf
    out_v = out_v * (1 - leaf) + v * 0.26 * leaf
    # Erde und Stiel: etwas dunkler und kühler
    rest = 1 - np.clip(violet + glow + leaf + teal, 0, 1)
    out_v = out_v * (1 - 0.35 * rest)
    return hsv_to_rgb(out_h % 1.0, np.clip(out_s, 0, 1), np.clip(out_v, 0, 1)), glow


RULES = {"Russrose": blutrose}


def recolour():
    image = bpy.data.images.load(os.path.join(SOURCE, "T_{}_basecolor.png".format(NAME)))
    w, h = image.size
    pixels = np.array(image.pixels[:], np.float32).reshape(h, w, 4)
    # die Textur ist sRGB; umfärben im Bildraum, so wie ein Maler es täte
    rgb, glow = RULES[NAME](pixels[..., :3])
    pixels[..., :3] = rgb
    out = bpy.data.images.new("schatten", w, h, alpha=True)
    out.pixels = pixels.reshape(-1)
    path = os.path.join(OUT, "T_{}_Schatten_basecolor.png".format(NAME))
    out.filepath_raw = path
    out.file_format = "PNG"
    out.save()
    # die Leuchtmaske (wo das Herz glüht) für die Vorschau
    mask = np.zeros((h, w, 4), np.float32)
    mask[..., 0] = mask[..., 1] = mask[..., 2] = glow
    mask[..., 3] = 1
    m = bpy.data.images.new("glut", w, h, alpha=True)
    m.pixels = mask.reshape(-1)
    mpath = os.path.join(OUT, "T_{}_Schatten_glut.png".format(NAME))
    m.filepath_raw = mpath
    m.file_format = "PNG"
    m.save()
    return path, mpath


def material(name, color_path, glow_path=None, glow_color=(1, 0.05, 0.03), glow_strength=0.0, light_color_path=None):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(color_path, check_existing=True)
    nt.links.new(tex.outputs["Color"], b.inputs["Base Color"])
    nor = nt.nodes.new("ShaderNodeTexImage")
    nor.image = bpy.data.images.load(os.path.join(SOURCE, "T_{}_normal.png".format(NAME)), check_existing=True)
    nor.image.colorspace_settings.name = "Non-Color"
    nmap = nt.nodes.new("ShaderNodeNormalMap")
    nt.links.new(nor.outputs["Color"], nmap.inputs["Color"])
    nt.links.new(nmap.outputs[0], b.inputs["Normal"])
    b.inputs["Roughness"].default_value = 0.55
    if glow_path:
        g = nt.nodes.new("ShaderNodeTexImage")
        g.image = bpy.data.images.load(glow_path, check_existing=True)
        g.image.colorspace_settings.name = "Non-Color"
        mul = nt.nodes.new("ShaderNodeMix")
        mul.data_type = "RGBA"
        mul.blend_type = "MULTIPLY"
        mul.inputs["Factor"].default_value = 1.0
        nt.links.new(g.outputs["Color"], mul.inputs[6])
        mul.inputs[7].default_value = (*glow_color, 1)
        nt.links.new(mul.outputs[2], b.inputs["Emission Color"])
        b.inputs["Emission Strength"].default_value = glow_strength
    elif light_color_path:
        # die Lichtform: ihr Herz leuchtet grün aus den hellen Türkistönen der Textur selbst
        nt.links.new(tex.outputs["Color"], b.inputs["Emission Color"])
        b.inputs["Emission Strength"].default_value = glow_strength
    return m


def preview(shadow_path, glow_path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 96
    scene.cycles.use_denoising = True
    scene.render.resolution_x, scene.render.resolution_y = 1600, 900
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.exposure = -0.6
    world = bpy.data.worlds.new("W")
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.008, 0.01, 0.012, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 1.0
    bpy.ops.import_scene.fbx(filepath=os.path.join(SOURCE, "SM_{}.fbx".format(NAME)))
    meshes = [o for o in scene.objects if o.type == "MESH"]
    # das Modell ist das größte Netz; Kollisionsklötze und Hilfsteile aus dem Export fallen weg
    model = max(meshes, key=lambda o: len(o.data.vertices))
    for other in meshes:
        if other is not model:
            bpy.data.objects.remove(other)
    bpy.context.view_layer.objects.active = model
    model.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    # auf den Boden stellen, Größe auf 1 normieren
    zs = [(model.matrix_world @ v.co).z for v in model.data.vertices]
    xs = [(model.matrix_world @ v.co).x for v in model.data.vertices]
    ys = [(model.matrix_world @ v.co).y for v in model.data.vertices]
    size = max(max(zs) - min(zs), max(xs) - min(xs), max(ys) - min(ys))
    model.scale = (1 / size,) * 3
    bpy.ops.object.transform_apply(scale=True)
    zs = [v.co.z for v in model.data.vertices]
    cx = (max(v.co.x for v in model.data.vertices) + min(v.co.x for v in model.data.vertices)) / 2
    cy = (max(v.co.y for v in model.data.vertices) + min(v.co.y for v in model.data.vertices)) / 2
    for v in model.data.vertices:
        v.co.x -= cx
        v.co.y -= cy
        v.co.z -= min(zs)
    light_mat = material("Licht", os.path.join(SOURCE, "T_{}_basecolor.png".format(NAME)), glow_strength=0.0)
    shadow_mat = material("Schatten", shadow_path, glow_path, glow_strength=2.2)
    model.data.materials.clear()
    model.data.materials.append(light_mat)
    twin = model.copy()
    twin.data = model.data.copy()
    twin.data.materials.clear()
    twin.data.materials.append(shadow_mat)
    scene.collection.objects.link(twin)
    model.location.x = -0.62
    twin.location.x = 0.62
    # Boden: dunkle Erde
    bpy.ops.mesh.primitive_plane_add(size=8)
    ground = bpy.context.object
    gm = bpy.data.materials.new("Boden")
    gm.use_nodes = True
    gm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.006, 0.005, 0.004, 1)
    gm.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.95
    ground.data.materials.append(gm)
    # Licht: Schlaglicht von links oben, Kante von hinten, je ein Farbschein unter den Pflanzen (grün / blutrot)
    def light(name, kind, loc, energy, color, size=1.0, target=(0, 0, 0.45)):
        data = bpy.data.lights.new(name, kind)
        data.energy = energy
        data.color = color
        if kind == "AREA":
            data.size = size
        obj = bpy.data.objects.new(name, data)
        obj.location = loc
        from mathutils import Vector
        obj.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        scene.collection.objects.link(obj)
    light("Schlag", "AREA", (-2.2, -2.6, 3.0), 260, (1.0, 0.88, 0.74), 1.2)
    light("Kante", "AREA", (1.5, 2.4, 2.2), 700, (0.75, 0.85, 1.0), 0.6)
    light("GruenSchein", "POINT", (-0.62, -0.25, 0.55), 18, (0.3, 1.0, 0.5))
    light("BlutSchein", "POINT", (0.62, -0.25, 0.55), 22, (1.0, 0.08, 0.05))
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.lens = 50
    cam = bpy.data.objects.new("Cam", cam_data)
    cam.location = (0, -3.3, 1.25)
    from mathutils import Vector
    cam.rotation_euler = (Vector((0, 0, 0.42)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(cam)
    scene.camera = cam
    scene.render.filepath = os.path.join(OUT, "{}_Licht_und_Schatten.png".format(NAME))
    bpy.ops.render.render(write_still=True)


shadow_path, glow_path = recolour()
preview(shadow_path, glow_path)
print("[Schattenform] fertig:", shadow_path)
