"""
Ink pictures of the Floromant's hand plants for the plant wheel (Boris 2026-11-04: "mit dieser Tuschefarbe wie beim Magier").
Every plant is its own 3D model (Vael_Quellen/<Id>/04_export/SM_<Id>.fbx), rendered in Blender with Freestyle ink lines and
soft light, then finished like the spell paintings (Tools/HUD/vael_symbole_malen.py): a very dark ground with a glow in the
plant's colour, colours pulled together and a little oily, a dark vignette. The shadow form gets its own picture: the same
plant in dark wine to black with red light (violet stays the Nid's). Light-only plants (Mondtraufe) get no shadow picture.
With the argument Faehigkeiten it paints the pictures of her ability buttons instead (T_Faehigkeit_<Name>.png).
Writes Scripts/Data/HUD/Pflanzen/T_Pflanze_<Id>.png and T_Pflanze_<Id>_Schatten.png (256 px) and _Uebersicht.png.
Run:
  blender -b -P vael_pflanzen_tusche.py -- <Vael_Quellen folder> <output folder> [Id ...]
"""
import bpy, sys, os, math
import numpy as np
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
SOURCES, OUT = args[0], args[1]
ONLY = args[2:]
SIZE = 256
RENDER = 512

# Plant: glow colour of the light form, of the shadow form (None = light only), camera turn
PLANTS = {
    "Windhalm": ((0.55, 0.95, 1.0), (0.6, 0.08, 0.1), 30),
    "Glutdistel": ((1.0, 0.42, 0.08), (0.55, 0.05, 0.05), 20),
    "Taublatt": ((0.3, 0.95, 0.9), (0.4, 0.06, 0.12), 35),
    "Glimmklee": ((1.0, 0.85, 0.4), (0.5, 0.08, 0.08), 25),
    "Russrose": ((0.35, 1.0, 0.55), (0.7, 0.04, 0.08), 30),
    "Funkenmohn": ((1.0, 0.45, 0.1), (0.45, 0.06, 0.12), 20),
    "Schlotwinde": ((1.0, 0.78, 0.25), (0.5, 0.07, 0.06), 40),
    "Harzkolben": ((1.0, 0.5, 0.1), (0.4, 0.05, 0.03), 30),
    "Feenragwurz": ((0.45, 0.9, 0.75), (0.5, 0.04, 0.1), 25),
    "Mondtraufe": ((1.0, 0.6, 0.82), None, 35),
    "Wachbeere": ((0.3, 0.95, 0.9), (0.55, 0.03, 0.08), 30),
}

os.makedirs(OUT, exist_ok=True)


def log(text):
    print("TUSCHE " + text, flush=True)


def setup_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 24
    scene.cycles.use_denoising = True
    scene.render.resolution_x = scene.render.resolution_y = RENDER
    scene.render.film_transparent = True
    scene.render.use_freestyle = True
    scene.render.line_thickness_mode = "ABSOLUTE"
    scene.render.line_thickness = 1.6
    layer = scene.view_layers[0]
    layer.use_freestyle = True
    lineset = layer.freestyle_settings.linesets[0] if layer.freestyle_settings.linesets else layer.freestyle_settings.linesets.new("Tusche")
    lineset.select_by_visibility = True
    lineset.select_silhouette = True
    lineset.select_border = True
    lineset.select_crease = True
    if lineset.linestyle is None:
        lineset.linestyle = bpy.data.linestyles.new("Tusche")
    lineset.linestyle.color = (0.03, 0.02, 0.015)
    lineset.linestyle.thickness = 1.6
    world = bpy.data.worlds.new("Welt")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.9, 0.88, 0.85, 1.0)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.6
    scene.world = world
    sun = bpy.data.objects.new("Sonne", bpy.data.lights.new("Sonne", "SUN"))
    sun.data.energy = 4.5
    sun.rotation_euler = (math.radians(50), math.radians(-20), math.radians(35))
    scene.collection.objects.link(sun)
    return scene


def render_plant(scene, plant, turn, crop=(0.0, 1.0), build=None):
    for obj in list(scene.objects):
        if obj.type in ("MESH", "CURVE"):
            bpy.data.objects.remove(obj, do_unlink=True)
    if build is not None:
        build()
    else:
        path = os.path.join(SOURCES, plant, "04_export", "SM_{}.fbx".format(plant))
        bpy.ops.import_scene.fbx(filepath=path)
    meshes = [o for o in scene.objects if o.type in ("MESH", "CURVE")]
    corners = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
    low = Vector((min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)))
    high = Vector((max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)))
    # A crop of the height (the foot of the Urbaum for Verwurzeln)
    low.z, high.z = low.z + (high.z - low.z) * crop[0], low.z + (high.z - low.z) * crop[1]
    centre = (low + high) * 0.5
    extent = max(high - low)
    camera_data = bpy.data.cameras.new("Kamera")
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = extent * 1.15
    camera = bpy.data.objects.new("Kamera", camera_data)
    scene.collection.objects.link(camera)
    a = math.radians(turn)
    camera.location = centre + Vector((math.sin(a) * extent * 2.0, -math.cos(a) * extent * 2.0, extent * 0.55))
    camera.rotation_euler = (centre - camera.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = camera
    target = os.path.join(OUT, "_render_{}.png".format(plant))
    scene.render.filepath = target
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(camera, do_unlink=True)
    image = bpy.data.images.load(target)
    pixels = np.array(image.pixels[:], np.float32).reshape(RENDER, RENDER, 4)[::-1]
    bpy.data.images.remove(image)
    os.remove(target)
    return pixels


def blur(field, radius):
    out = field.copy()
    for axis in (0, 1):
        acc = np.zeros_like(out)
        for shift in range(-radius, radius + 1):
            acc += np.roll(out, shift, axis=axis)
        out = acc / (2 * radius + 1)
    return out


def finish(pixels, glow, shadow):
    rgb, alpha = pixels[..., :3], pixels[..., 3:4]
    size = pixels.shape[0]
    y, x = np.mgrid[0:size, 0:size] / (size - 1.0) - 0.5
    distance = np.sqrt(x * x + y * y)
    luminance = rgb @ np.array([0.3, 0.59, 0.11], np.float32)
    if shadow:
        # The shadow form: the same plant in dark wine to black, its light parts burning red
        ramp = np.clip(luminance[..., None], 0.0, 1.0)
        rgb = np.where(ramp < 0.5, np.array([0.03, 0.005, 0.01]) + ramp * 2 * np.array([0.3, 0.02, 0.05]),
                       np.array([0.33, 0.025, 0.06]) + (ramp - 0.5) * 2 * np.array([0.65, 0.08, 0.06]))
        rgb = np.clip(rgb * 1.5, 0.0, 1.0)
    else:
        # Saturated a little and warmed like ink on parchment, never pale (healing-plants-look)
        grey = luminance[..., None]
        rgb = np.clip((grey + (rgb - grey) * 1.35) * 1.3, 0.0, 1.0) * np.array([1.05, 0.98, 0.88])
    # The ground: very dark, with a soft glow of the plant behind it
    ground = np.array([0.035, 0.028, 0.022]) + np.array(glow) * (np.clip(1.0 - distance * 2.4, 0.0, 1.0) ** 2)[..., None] * 0.55
    halo = blur(alpha[..., 0], 6)[..., None] * np.array(glow) * 0.35
    picture = ground + halo
    picture = picture * (1.0 - alpha) + rgb * alpha
    # A slight oily softening and a dark vignette, like the spell paintings
    picture = picture * 0.7 + blur(picture, 1) * 0.3
    picture *= np.clip(1.25 - distance * 1.1, 0.0, 1.0)[..., None]
    return np.clip(picture, 0.0, 1.0)


def save(picture, name):
    small = picture.reshape(SIZE, RENDER // SIZE, SIZE, RENDER // SIZE, 3).mean(axis=(1, 3))
    image = bpy.data.images.new(name, SIZE, SIZE, alpha=True)
    rgba = np.concatenate([small, np.ones((SIZE, SIZE, 1), np.float32)], axis=2)[::-1]
    image.pixels = rgba.ravel()
    image.filepath_raw = os.path.join(OUT, name + ".png")
    image.file_format = "PNG"
    image.save()
    return small


# ---------------------------------------------------------------- Ability pictures of the Floromant (Boris 2026-11-04)
# The same ink as the plants for the buttons of her panel: Verwurzeln = the foot of the Urbaum with its roots, Kreuzen = two
# stems of different plants winding into one knot with a blossom, Tuerme = a Hainschoessling grown tall, Keimpelz = the moss.

def curve_tube(name, points, radius, colour, emission=0.0, taper=True):
    data = bpy.data.curves.new(name, "CURVE")
    data.dimensions = "3D"
    data.bevel_depth = radius
    data.bevel_resolution = 4
    spline = data.splines.new("POLY")
    spline.points.add(len(points) - 1)
    for k, p in enumerate(points):
        spline.points[k].co = (*p, 1.0)
        spline.points[k].radius = (1.0 - 0.75 * k / max(len(points) - 1, 1)) if taper else 1.0
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
    bsdf.inputs["Roughness"].default_value = 0.5
    if emission > 0.0:
        bsdf.inputs["Emission Color"].default_value = (*colour, 1.0)
        bsdf.inputs["Emission Strength"].default_value = emission
    data.materials.append(mat)
    return obj


def blob(name, location, radius, colour, emission=0.0, scale=(1.0, 1.0, 1.0)):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=radius, location=location, segments=24, ring_count=12)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
    if emission > 0.0:
        bsdf.inputs["Emission Color"].default_value = (*colour, 1.0)
        bsdf.inputs["Emission Strength"].default_value = emission
    obj.data.materials.append(mat)
    return obj


def build_cross():
    """Two stems of different plants winding round each other up into one knot that opens as a blossom"""
    for side, colour, phase in ((-1, (0.55, 0.22, 0.08), 0.0), (1, (0.08, 0.4, 0.36), math.pi)):
        points = []
        for t in np.linspace(0.0, 1.0, 60):
            spread = 0.55 * (1.0 - t) ** 1.6
            points.append((side * spread + 0.07 * math.sin(t * math.tau * 2.0 + phase), 0.07 * math.cos(t * math.tau * 2.0 + phase), t * 1.25))
        curve_tube("Stiel", points, 0.035, colour)
        for t in (0.22, 0.45):
            k = int(t * 59)
            x, y, z = points[k]
            blob("Blatt", (x + side * 0.11, y, z + 0.03), 0.08, (0.12, 0.3, 0.1), scale=(1.6, 0.35, 0.7)).rotation_euler = (0.0, side * 0.6, 0.0)
    for k in range(7):
        a = k * math.tau / 7
        petal = blob("Bluetenblatt", (0.13 * math.cos(a), 0.13 * math.sin(a) * 0.3 - 0.02, 1.32 + 0.1 * math.sin(a)), 0.1, (0.85, 0.45, 0.35), 0.4, scale=(1.5, 0.35, 0.8))
        petal.rotation_euler = (0.0, -a, 0.0)
    blob("Herz", (0.0, -0.06, 1.32), 0.07, (1.0, 0.78, 0.35), 2.5)


def build_tower():
    """A tower of the Floromant as she grows them: three stalks braided into one trunk from a spread of roots, the crown a
    cluster of glowing heads on short necks, a few leaves along the trunk"""
    for k in range(3):
        phase = k * math.tau / 3
        points = []
        for t in np.linspace(0.0, 1.0, 70):
            r = 0.09 * (1.0 - 0.4 * t)
            points.append((r * math.cos(t * math.tau * 1.6 + phase), r * math.sin(t * math.tau * 1.6 + phase), t * 1.4))
        curve_tube("Stamm", points, 0.06, (0.06, 0.045, 0.028))
    for k in range(7):
        a = k * math.tau / 7 + 0.3
        root = [(0.08 * math.cos(a) * (1 + 3 * t), 0.08 * math.sin(a) * (1 + 3 * t) * 0.4, 0.05 - 0.12 * t * t) for t in np.linspace(0.0, 1.0, 12)]
        curve_tube("Wurzel", root, 0.035, (0.05, 0.035, 0.022))
    for k, (a, h) in enumerate(((0.0, 0.25), (1.6, 0.18), (3.2, 0.28), (4.6, 0.2), (2.4, 0.3))):
        neck = [(0.0, 0.0, 1.35), (0.18 * math.cos(a), 0.1 * math.sin(a), 1.35 + h * 0.6), (0.3 * math.cos(a), 0.16 * math.sin(a), 1.35 + h)]
        curve_tube("Hals", neck, 0.03, (0.18, 0.28, 0.1))
        blob("Kopf", neck[-1], 0.075, (0.8, 0.3, 0.06) if k % 2 == 0 else (0.08, 0.6, 0.5), 0.5)
    for t, side in ((0.35, 1), (0.55, -1), (0.75, 1)):
        blob("Blatt", (side * 0.15, 0.0, t * 1.4), 0.09, (0.12, 0.32, 0.1), scale=(1.7, 0.3, 0.6)).rotation_euler = (0.0, side * 0.5, 0.0)


ABILITY_PICTURES = {
    # name: (model or None, builder, glow, camera turn, height crop)
    "Verwurzeln": ("Urbaum", None, (0.45, 0.9, 0.5), 20, (0.0, 0.3)),
    "Kreuzen": (None, build_cross, (1.0, 0.72, 0.35), 0, (0.0, 1.0)),
    # Not the Hainschoessling: that is a legendary plant that comes on the arm later (Boris 2026-11-04)
    "Tuerme": (None, build_tower, (0.55, 1.0, 0.7), 0, (0.0, 1.0)),
    "Keimpelz": ("Keimpelz", None, (0.6, 0.95, 0.45), 25, (0.0, 1.0)),
}


def paint_abilities(scene, out):
    for name, (model, builder, glow, turn, crop) in ABILITY_PICTURES.items():
        pixels = render_plant(scene, model, turn, crop, builder)
        save(finish(pixels, glow, False), "T_Faehigkeit_" + name)
        log("painted ability " + name)


scene = setup_scene()
if "Faehigkeiten" in ONLY:
    paint_abilities(scene, OUT)
    log("done")
    sys.exit(0)
sheet = []
for plant, (glow, shadow_glow, turn) in PLANTS.items():
    if ONLY and plant not in ONLY:
        continue
    pixels = render_plant(scene, plant, turn)
    sheet.append(save(finish(pixels, glow, False), "T_Pflanze_" + plant))
    if shadow_glow is not None:
        sheet.append(save(finish(pixels, shadow_glow, True), "T_Pflanze_{}_Schatten".format(plant)))
    log("painted " + plant)
if sheet:
    columns = 6
    rows = (len(sheet) + columns - 1) // columns
    grid = np.zeros((rows * SIZE, columns * SIZE, 3), np.float32)
    for index, tile in enumerate(sheet):
        r, c = index // columns, index % columns
        grid[r * SIZE:(r + 1) * SIZE, c * SIZE:(c + 1) * SIZE] = tile
    image = bpy.data.images.new("Uebersicht", columns * SIZE, rows * SIZE)
    image.pixels = np.concatenate([grid, np.ones((rows * SIZE, columns * SIZE, 1), np.float32)], axis=2)[::-1].ravel()
    image.filepath_raw = os.path.join(OUT, "_Uebersicht.png")
    image.file_format = "PNG"
    image.save()
log("done")
