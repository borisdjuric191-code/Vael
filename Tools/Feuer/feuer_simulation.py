"""
Feuer-Baukasten: simulates real fire in Blender (Mantaflow) and renders it into a looping flipbook for the fire materials.
Our own fire, no effects of other packs: the flames lick, waver and tear like in a real fire because they are a fluid simulation.

Kinds:
  flamme  a single tall flame rising from a small source (torches, a burning spot, the tail of a spark)
  feuer   a broad fire clinging to the ground (campfires, burning patches, walls)
  kugel   a ball whose whole surface burns (the fire orb over the head, the Feuerball)

The simulation is baked, a stretch of it rendered from the front on a transparent background, and FRAMES pictures are laid out
COLUMNS x COLUMNS in one texture. The last OVERLAP frames are blended into the first ones, so the flipbook loops without a seam.
Writes <output>/T_Feuer_Flipbook_<Kind>.png (RGB = the glowing fire and its smoke, A = coverage).

Run: blender -b -P feuer_simulation.py -- <kind> <output folder> [frame size] [resolution]
"""
import bpy, sys, os, math
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:]
KIND = args[0]
OUT = args[1]
FRAME_SIZE = int(args[2]) if len(args) > 2 else 256
RESOLUTION = int(args[3]) if len(args) > 3 else 96

COLUMNS = 8
FRAMES = COLUMNS * COLUMNS
OVERLAP = 16
WARMUP = 40
WORK = os.path.join(OUT, "sim_" + KIND)

# Glow of the flames and thickness of the smoke in the render
BLACKBODY = float(os.environ.get("FEUER_GLUT", "9.0"))
SMOKE_DENSITY = 0.8
os.makedirs(WORK, exist_ok=True)


def log(text):
    print("FEUERSIM " + text, flush=True)


def scene_setup():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.frame_start = 1
    scene.frame_end = WARMUP + FRAMES + OVERLAP
    return scene


def make_source(scene):
    """The thing that burns, and the domain the fire lives in"""
    if KIND == "kugel":
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.25, location=(0, 0, 0.6), segments=32, ring_count=16)
        domain_size, domain_center = (1.5, 1.5, 1.8), (0, 0, 0.85)
    elif KIND == "feuer":
        bpy.ops.mesh.primitive_cylinder_add(radius=0.55, depth=0.08, location=(0, 0, 0.04), vertices=32)
        domain_size, domain_center = (2.0, 2.0, 1.8), (0, 0, 0.85)
    else:
        bpy.ops.mesh.primitive_cylinder_add(radius=0.2, depth=0.06, location=(0, 0, 0.03), vertices=24)
        domain_size, domain_center = (1.0, 1.0, 1.7), (0, 0, 0.8)
    source = bpy.context.active_object
    source.name = "Quelle"

    bpy.ops.mesh.primitive_cube_add(location=domain_center)
    domain = bpy.context.active_object
    domain.name = "Domaene"
    domain.scale = (domain_size[0] / 2, domain_size[1] / 2, domain_size[2] / 2)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return source, domain


def setup_fluid(source, domain):
    flow = source.modifiers.new("Feuer", "FLUID")
    flow.fluid_type = "FLOW"
    settings = flow.flow_settings
    # Fire with only a little smoke of its own; more smoke comes from the burning
    settings.flow_type = "BOTH"
    settings.density = 0.25
    settings.flow_behavior = "INFLOW"
    settings.flow_source = "MESH"
    settings.surface_distance = 0.6 if KIND == "kugel" else 1.2
    settings.fuel_amount = 1.6 if KIND == "feuer" else 1.4
    settings.temperature = 3.0
    settings.smoke_color = (0.15, 0.13, 0.12)
    settings.use_initial_velocity = KIND == "kugel"
    if KIND == "kugel":
        settings.velocity_normal = 0.6
    # Uneven fuel makes the flames break up into tongues
    noise = bpy.data.textures.new("Brennstoff", type="CLOUDS")
    noise.noise_scale = 0.12
    settings.use_texture = True
    settings.noise_texture = noise
    settings.texture_size = 1.0

    mod = domain.modifiers.new("Feuer", "FLUID")
    mod.fluid_type = "DOMAIN"
    d = mod.domain_settings
    d.domain_type = "GAS"
    d.resolution_max = RESOLUTION
    d.cache_directory = os.path.join(WORK, "cache")
    d.cache_type = "ALL"
    d.cache_frame_start = 1
    d.cache_frame_end = WARMUP + FRAMES + OVERLAP
    d.use_noise = False
    d.noise_scale = 2
    d.noise_strength = 1.2
    d.vorticity = 0.35
    d.alpha = 0.4
    d.beta = 2.2
    # Fast burning: the flames thin out and tear well below the top of the frame
    d.burning_rate = {"flamme": 1.05, "feuer": 0.95, "kugel": 1.0}[KIND]
    d.flame_smoke = 0.6
    d.flame_vorticity = 0.9
    d.flame_ignition = 1.25
    d.flame_max_temp = 2.2
    d.flame_smoke_color = (0.12, 0.11, 0.1)
    d.use_dissolve_smoke = True
    d.dissolve_speed = 18
    d.time_scale = 0.9


def bake(domain):
    # A baked cache is used again, so only the look can be rendered anew
    cache = os.path.join(WORK, "cache", "data")
    if os.path.isdir(cache) and len(os.listdir(cache)) > FRAMES:
        log("using the baked cache")
        return
    log("baking ...")
    with bpy.context.temp_override(object=domain, active_object=domain, selected_objects=[domain]):
        bpy.ops.fluid.bake_all()
    log("baked")


def fire_material(domain):
    """The fire glows in the colour of its simulated temperature (blackbody, like real flames), with a thin dark smoke"""
    mat = bpy.data.materials.new("Feuer")
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    volume = nodes.new("ShaderNodeVolumePrincipled")
    flame = nodes.new("ShaderNodeAttribute")
    flame.attribute_name = "flame"
    temperature = nodes.new("ShaderNodeAttribute")
    temperature.attribute_name = "temperature"
    density = nodes.new("ShaderNodeAttribute")
    density.attribute_name = "density"

    # The stronger the flame, the hotter: yellow-white in the core, deep orange and red at the edges (blackbody colours)
    kelvin = nodes.new("ShaderNodeMapRange")
    kelvin.inputs["From Min"].default_value = 0.0
    kelvin.inputs["From Max"].default_value = 1.0
    kelvin.inputs["To Min"].default_value = 1000.0
    kelvin.inputs["To Max"].default_value = 2400.0
    links.new(flame.outputs["Fac"], kelvin.inputs["Value"])
    colour = nodes.new("ShaderNodeBlackbody")
    links.new(kelvin.outputs["Result"], colour.inputs["Temperature"])
    links.new(colour.outputs["Color"], volume.inputs["Emission Color"])

    curve = nodes.new("ShaderNodeMath")
    curve.operation = "POWER"
    curve.inputs[1].default_value = 1.5
    links.new(flame.outputs["Fac"], curve.inputs[0])
    glow = nodes.new("ShaderNodeMath")
    glow.operation = "MULTIPLY"
    glow.inputs[1].default_value = BLACKBODY
    links.new(curve.outputs[0], glow.inputs[0])
    links.new(glow.outputs[0], volume.inputs["Emission Strength"])

    smoke = nodes.new("ShaderNodeMath")
    smoke.operation = "MULTIPLY"
    smoke.inputs[1].default_value = SMOKE_DENSITY
    links.new(density.outputs["Fac"], smoke.inputs[0])
    links.new(smoke.outputs[0], volume.inputs["Density"])
    volume.inputs["Color"].default_value = (0.04, 0.035, 0.03, 1)
    links.new(volume.outputs["Volume"], out.inputs["Volume"])
    domain.data.materials.clear()
    domain.data.materials.append(mat)


def setup_render(scene, source, domain):
    source.hide_render = True
    cam_data = bpy.data.cameras.new("Kamera")
    cam_data.type = "ORTHO"
    size = domain.dimensions
    cam_data.ortho_scale = max(size.x, size.z) * 1.0
    cam = bpy.data.objects.new("Kamera", cam_data)
    scene.collection.objects.link(cam)
    cam.location = (0, -6, domain.location.z)
    cam.rotation_euler = (math.radians(90), 0, 0)
    scene.camera = cam

    world = bpy.data.worlds.new("Schwarz")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0, 0, 0, 1)
    scene.world = world

    scene.render.engine = "BLENDER_EEVEE"
    # On black: the fire is pure light, its coverage comes from its brightness when the atlas is made
    scene.render.film_transparent = False
    scene.render.resolution_x = int(FRAME_SIZE * size.x / max(size.x, size.z))
    scene.render.resolution_y = int(FRAME_SIZE * size.z / max(size.x, size.z))
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.view_settings.view_transform = "Standard"
    eevee = scene.eevee
    for name, value in (("volumetric_tile_size", "2"), ("volumetric_samples", 128), ("taa_render_samples", 32), ("volumetric_end", 20.0)):
        if hasattr(eevee, name):
            setattr(eevee, name, value)


def render_frames(scene):
    paths = []
    for index in range(FRAMES + OVERLAP):
        frame = WARMUP + 1 + index
        scene.frame_set(frame)
        path = os.path.join(WORK, "f_{:03d}.png".format(index))
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        paths.append(path)
        if index % 10 == 0:
            log("rendered frame {}".format(index))
    return paths


def load(path):
    image = bpy.data.images.load(path)
    w, h = image.size
    pixels = np.array(image.pixels[:], dtype=np.float32).reshape(h, w, 4)
    bpy.data.images.remove(image)
    # Coverage from the brightness: bright flames are solid, faint glow fades out
    # (the faint haze of glow around the flames gets no coverage, so no pale box shows around them)
    pixels[:, :, 3] = np.clip((pixels[:, :, :3].max(axis=2) - 0.07) * 2.2, 0.0, 1.0)
    return pixels


def build_atlas(paths):
    frames = [load(p) for p in paths]
    h, w = frames[0].shape[:2]
    cell = FRAME_SIZE
    atlas = np.zeros((cell * COLUMNS, cell * COLUMNS, 4), dtype=np.float32)
    for i in range(FRAMES):
        # Seamless loop: the first frames are blended from the frames after the end, which follow the last frame
        if i < OVERLAP:
            t = i / OVERLAP
            picture = frames[i + FRAMES] * (1.0 - t) + frames[i] * t
        else:
            picture = frames[i]
        # Each frame sits centred in a square cell, the picture's top at the cell's top
        row, column = i // COLUMNS, i % COLUMNS
        y0 = (COLUMNS - 1 - row) * cell + (cell - h)
        x0 = column * cell + (cell - w) // 2
        atlas[y0:y0 + h, x0:x0 + w] = picture
    image = bpy.data.images.new("T_Feuer_Flipbook_" + KIND.capitalize(), cell * COLUMNS, cell * COLUMNS, alpha=True)
    image.pixels = atlas.ravel()
    path = os.path.join(OUT, "T_Feuer_Flipbook_{}.png".format(KIND.capitalize()))
    image.filepath_raw = path
    image.file_format = "PNG"
    image.save()
    log("atlas " + path)
    # A preview strip of a few frames to look at
    shown = [frames[k] for k in range(0, FRAMES, FRAMES // 6)][:6]
    preview = np.concatenate([np.dstack([f[:, :, :3], np.ones(f.shape[:2])]) for f in shown], axis=1).astype(np.float32)
    strip = bpy.data.images.new("Vorschau", preview.shape[1], preview.shape[0], alpha=True)
    strip.pixels = preview.ravel()
    strip.filepath_raw = os.path.join(OUT, "Vorschau_{}.png".format(KIND))
    strip.file_format = "PNG"
    strip.save()


scene = scene_setup()
source, domain = make_source(scene)
setup_fluid(source, domain)
bake(domain)
fire_material(domain)
setup_render(scene, source, domain)
build_atlas(render_frames(scene))
log("done")
