"""
Ink pictures of the Floromant's hand plants for the plant wheel (Boris 2026-11-04: "mit dieser Tuschefarbe wie beim Magier").
Every plant is its own 3D model (Vael_Quellen/<Id>/04_export/SM_<Id>.fbx), rendered in Blender with Freestyle ink lines and
soft light, then finished like the spell paintings (Tools/HUD/vael_symbole_malen.py): a very dark ground with a glow in the
plant's colour, colours pulled together and a little oily, a dark vignette. The shadow form gets its own picture: the same
plant in dark wine to black with red light (violet stays the Nid's). Light-only plants (Mondtraufe) get no shadow picture.
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


def render_plant(scene, plant, turn):
    for obj in list(scene.objects):
        if obj.type == "MESH":
            bpy.data.objects.remove(obj, do_unlink=True)
    path = os.path.join(SOURCES, plant, "04_export", "SM_{}.fbx".format(plant))
    bpy.ops.import_scene.fbx(filepath=path)
    meshes = [o for o in scene.objects if o.type == "MESH"]
    corners = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
    low = Vector((min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)))
    high = Vector((max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)))
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


scene = setup_scene()
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
