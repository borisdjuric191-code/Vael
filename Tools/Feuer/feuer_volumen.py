"""
Feuer-Baukasten in 3D: simulates fire in Blender (Mantaflow) and writes it as a sequence of OpenVDB volumes, which Unreal
imports as an animated Sparse Volume Texture and renders as a real volume (UHeterogeneousVolumeComponent). Seen from any side,
the fire is a body of flame and smoke, not a picture on a card.

Kinds:
  brand   a fire on the ground fed by a few logs and an uneven bed of embers: lively, whirling, licking (campfires, burning patches)
  kugel   a ball whose whole surface burns (the fire orb over the head, the Feuerball)

Every frame becomes <output>/<kind>/vdb/feuer_<kind>_<nnnn>.vdb with only the grids the game needs, in this order:
density (smoke), flame, temperature. Unreal puts the first into attributes A and the next ones into attributes B.
A preview of a few frames, seen from above at the angle of the game camera, goes to <output>/<kind>/vorschau.png.

Run: blender -b -P feuer_volumen.py -- <kind> <output folder> [resolution] [frames]
"""
import bpy, sys, os, math, random
import numpy as np
import openvdb

args = sys.argv[sys.argv.index("--") + 1:]
KIND = args[0]
OUT = os.path.join(args[1], KIND)
RESOLUTION = int(args[2]) if len(args) > 2 else 112
FRAMES = int(args[3]) if len(args) > 3 else 96
WARMUP = 40
os.makedirs(os.path.join(OUT, "vdb"), exist_ok=True)
random.seed(11)


def log(text):
    print("FEUER3D " + text, flush=True)


def scene_setup():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.frame_start = 1
    scene.frame_end = WARMUP + FRAMES
    return scene


def fuel_settings(obj, fuel, surface):
    flow = obj.modifiers.new("Feuer", "FLUID")
    flow.fluid_type = "FLOW"
    s = flow.flow_settings
    s.flow_type = "BOTH"
    s.flow_behavior = "INFLOW"
    s.flow_source = "MESH"
    s.surface_distance = surface
    s.fuel_amount = fuel
    s.density = 0.3
    s.temperature = 3.0
    s.smoke_color = (0.12, 0.11, 0.1)
    # Uneven fuel breaks the fire into separate tongues
    noise = bpy.data.textures.new("Brennstoff_" + obj.name, type="CLOUDS")
    noise.noise_scale = 0.08
    s.use_texture = True
    s.noise_texture = noise
    s.texture_size = 1.0
    return s


def make_sources():
    sources = []
    if KIND == "kugel":
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.25, location=(0, 0, 0.55), segments=32, ring_count=16)
        ball = bpy.context.active_object
        s = fuel_settings(ball, 1.2, 0.6)
        s.use_initial_velocity = True
        s.velocity_normal = 0.5
        sources.append(ball)
        size, centre = (1.4, 1.4, 1.8), (0, 0, 0.8)
    else:
        # A few logs lying crossed over each other, and a low uneven bed of embers between them
        for index in range(6):
            angle = index * math.pi / 3 + random.uniform(-0.3, 0.3)
            bpy.ops.mesh.primitive_cylinder_add(radius=random.uniform(0.05, 0.08), depth=random.uniform(0.6, 0.9),
                                                location=(math.cos(angle) * 0.18, math.sin(angle) * 0.18, 0.08 + 0.05 * (index % 2)),
                                                rotation=(math.pi / 2, 0, angle + math.pi / 2), vertices=12)
            log_ = bpy.context.active_object
            fuel_settings(log_, random.uniform(1.0, 1.6), 1.0)
            sources.append(log_)
        bpy.ops.mesh.primitive_ico_sphere_add(radius=0.3, location=(0, 0, 0.0), subdivisions=3)
        bed = bpy.context.active_object
        bed.scale = (1.0, 1.0, 0.25)
        fuel_settings(bed, 0.8, 1.5)
        sources.append(bed)
        size, centre = (2.2, 2.2, 2.4), (0, 0, 1.1)

    bpy.ops.mesh.primitive_cube_add(location=centre)
    domain = bpy.context.active_object
    domain.name = "Domaene"
    domain.scale = (size[0] / 2, size[1] / 2, size[2] / 2)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return sources, domain


def setup_domain(domain):
    mod = domain.modifiers.new("Feuer", "FLUID")
    mod.fluid_type = "DOMAIN"
    d = mod.domain_settings
    d.domain_type = "GAS"
    d.resolution_max = RESOLUTION
    d.cache_directory = os.path.join(OUT, "cache")
    d.cache_type = "ALL"
    d.cache_data_format = "OPENVDB"
    d.cache_frame_start = 1
    d.cache_frame_end = WARMUP + FRAMES
    d.use_noise = False
    # Lively: strong whirls in the flames, buoyant hot air, fuel that burns at a middle pace
    d.vorticity = 0.55
    d.alpha = 0.35
    d.beta = 2.4
    d.burning_rate = 0.8 if KIND == "brand" else 0.95
    d.flame_smoke = 0.7
    d.flame_vorticity = 1.6
    d.flame_ignition = 1.2
    d.flame_max_temp = 2.4
    d.flame_smoke_color = (0.1, 0.09, 0.08)
    d.use_dissolve_smoke = True
    d.dissolve_speed = 22
    d.time_scale = 1.0
    return d


def bake(domain):
    log("baking ...")
    with bpy.context.temp_override(object=domain, active_object=domain, selected_objects=[domain]):
        bpy.ops.fluid.bake_all()
    log("baked")


def export_volumes(cache):
    """Writes the frames after the warm-up with only density, flame and temperature"""
    data = os.path.join(cache, "data")
    written = 0
    for index in range(FRAMES):
        source = os.path.join(data, "fluid_data_{:04d}.vdb".format(WARMUP + 1 + index))
        if not os.path.exists(source):
            continue
        grids, meta = openvdb.readAll(source)
        by_name = {g.name: g for g in grids}
        keep = [by_name[name] for name in ("density", "flame", "temperature") if name in by_name]
        openvdb.write(os.path.join(OUT, "vdb", "feuer_{}_{:04d}.vdb".format(KIND, index + 1)), grids=keep)
        written += 1
    log("exported {} volumes".format(written))


def preview(scene, sources, domain):
    """A few frames from above at the angle of the game camera, glowing in blackbody colours"""
    for source in sources:
        source.hide_render = True
    mat = bpy.data.materials.new("Feuer")
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    volume = nodes.new("ShaderNodeVolumePrincipled")
    flame = nodes.new("ShaderNodeAttribute")
    flame.attribute_name = "flame"
    density = nodes.new("ShaderNodeAttribute")
    density.attribute_name = "density"
    kelvin = nodes.new("ShaderNodeMapRange")
    kelvin.inputs["To Min"].default_value = 1000.0
    kelvin.inputs["To Max"].default_value = 2400.0
    links.new(flame.outputs["Fac"], kelvin.inputs["Value"])
    colour = nodes.new("ShaderNodeBlackbody")
    links.new(kelvin.outputs["Result"], colour.inputs["Temperature"])
    links.new(colour.outputs["Color"], volume.inputs["Emission Color"])
    glow = nodes.new("ShaderNodeMath")
    glow.operation = "MULTIPLY"
    glow.inputs[1].default_value = 9.0
    links.new(flame.outputs["Fac"], glow.inputs[0])
    links.new(glow.outputs[0], volume.inputs["Emission Strength"])
    smoke = nodes.new("ShaderNodeMath")
    smoke.operation = "MULTIPLY"
    smoke.inputs[1].default_value = 1.5
    links.new(density.outputs["Fac"], smoke.inputs[0])
    links.new(smoke.outputs[0], volume.inputs["Density"])
    volume.inputs["Color"].default_value = (0.04, 0.035, 0.03, 1)
    links.new(volume.outputs["Volume"], out.inputs["Volume"])
    domain.data.materials.clear()
    domain.data.materials.append(mat)

    bpy.ops.mesh.primitive_plane_add(size=6, location=(0, 0, 0))
    ground = bpy.context.active_object
    ground_mat = bpy.data.materials.new("Boden")
    ground_mat.diffuse_color = (0.12, 0.09, 0.06, 1)
    ground.data.materials.append(ground_mat)

    cam = bpy.data.objects.new("Kamera", bpy.data.cameras.new("Kamera"))
    scene.collection.objects.link(cam)
    cam.location = (0, -3.6, 3.4)
    cam.rotation_euler = (math.radians(50), 0, 0)
    scene.camera = cam
    world = bpy.data.worlds.new("Dunkel")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.02, 0.02, 0.025, 1)
    scene.world = world
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x, scene.render.resolution_y = 360, 360
    scene.view_settings.view_transform = "Standard"

    tiles = []
    for index in range(4):
        scene.frame_set(WARMUP + 1 + index * (FRAMES // 4))
        path = os.path.join(OUT, "vorschau_{}.png".format(index))
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        image = bpy.data.images.load(path)
        tiles.append(np.array(image.pixels[:], dtype=np.float32).reshape(360, 360, 4))
    strip = np.concatenate(tiles, axis=1)
    image = bpy.data.images.new("Vorschau", strip.shape[1], strip.shape[0], alpha=True)
    image.pixels = strip.ravel()
    image.filepath_raw = os.path.join(OUT, "vorschau.png")
    image.file_format = "PNG"
    image.save()
    log("preview " + image.filepath_raw)


scene = scene_setup()
sources, domain = make_sources()
settings = setup_domain(domain)
bake(domain)
export_volumes(settings.cache_directory)
preview(scene, sources, domain)
log("done")
