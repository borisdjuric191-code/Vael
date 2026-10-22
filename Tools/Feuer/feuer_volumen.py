"""
Feuer-Baukasten in 3D: simulates fire in Blender (Mantaflow) and writes it as a sequence of OpenVDB volumes, which Unreal
imports as an animated Sparse Volume Texture and renders as a real volume (UHeterogeneousVolumeComponent). Seen from any side,
the fire is a body of flame and smoke, not a picture on a card.

Kinds:
  lager   a campfire: a few logs over a bed of embers
  glut, brand, gross   the stages a Brandherd grows through as it is fed: the fire comes out of a star of roots on the ground
          (feuer_wurzeln.py, the same star the picture under it shows), the bigger the stage the further the roots reach
  kugel   a ball whose whole surface burns (the fire orb over the head, the Feuerball)
All burn calmly; a slow turbulence field makes the tongues wind and tear apart at their tips.
Every domain is so big the flames never touch its walls, and its floor lies 0.1 m under the foot of the fire.

The loop is seamless: the last frames are blended into the first ones. Every frame becomes
<output>/<kind>/vdb/feuer_<kind>_<nnnn>.vdb with the grids density (smoke) and flame, in this order: Unreal puts the first
into attributes A and the second into attributes B. A preview of a few frames, seen from above at the angle of the game
camera, goes to <output>/<kind>/vorschau.png.

Run: blender -b -P feuer_volumen.py -- <kind> <output folder> [resolution] [frames]
"""
import bpy, sys, os, math, random
import numpy as np
import openvdb

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from feuer_wurzeln import root_segments

args = sys.argv[sys.argv.index("--") + 1:]
KIND = args[0]
OUT = os.path.join(args[1], KIND)
RESOLUTION = int(args[2]) if len(args) > 2 else 112
FRAMES = int(args[3]) if len(args) > 3 else 96
BLEND = FRAMES // 4
WARMUP = 50
os.makedirs(os.path.join(OUT, "vdb"), exist_ok=True)
random.seed(11)

# Domain size and middle, and how far the turbulence reaches, per kind
DOMAINS = {
    "lager": ((2.0, 2.0, 2.4), (0, 0, 1.1), 0.3),
    "glut": ((1.4, 1.4, 1.3), (0, 0, 0.55), 0.18),
    "brand": ((2.2, 2.2, 2.2), (0, 0, 1.0), 0.3),
    "gross": ((3.6, 3.6, 3.0), (0, 0, 1.4), 0.45),
    "kugel": ((1.6, 1.6, 2.0), (0, 0, 0.9), 0.2),
}

# Radius of the star of roots of a Brandherd, per stage, in metres
STARS = {"glut": 0.35, "brand": 0.6, "gross": 1.1}


def log(text):
    print("FEUER3D " + text, flush=True)


def scene_setup():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.frame_start = 1
    scene.frame_end = WARMUP + FRAMES
    return scene


def fuel_settings(obj, fuel, surface, density=0.06):
    flow = obj.modifiers.new("Feuer", "FLUID")
    flow.fluid_type = "FLOW"
    s = flow.flow_settings
    s.flow_type = "BOTH"
    s.flow_behavior = "INFLOW"
    s.flow_source = "MESH"
    s.surface_distance = surface
    s.fuel_amount = fuel
    # Little smoke: the fire is the body, the smoke only a thin veil above it
    s.density = density
    s.temperature = 2.0
    s.smoke_color = (0.12, 0.11, 0.1)
    # Uneven fuel breaks the fire into separate tongues
    noise = bpy.data.textures.new("Brennstoff_" + obj.name, type="CLOUDS")
    noise.noise_scale = 0.12
    s.use_texture = True
    s.noise_texture = noise
    s.texture_size = 1.0
    return s


def add_log(x, y, z, angle, radius, length, fuel):
    bpy.ops.mesh.primitive_cylinder_add(radius=radius, depth=length, location=(x, y, z), rotation=(math.pi / 2, 0, angle), vertices=12)
    fuel_settings(bpy.context.active_object, fuel, 1.0)
    return bpy.context.active_object


def add_bed(radius, height, fuel, surface):
    bpy.ops.mesh.primitive_ico_sphere_add(radius=1.0, location=(0, 0, 0.0), subdivisions=4)
    bed = bpy.context.active_object
    bed.scale = (radius, radius, height)
    # An uneven, ragged bed: the fire does not burn as a disc
    for vertex in bed.data.vertices:
        vertex.co *= random.uniform(0.75, 1.15)
    fuel_settings(bed, fuel, surface)
    return bed


def add_roots(star_radius, fuel):
    """The star of roots (feuer_wurzeln.py) as one flat mesh of thick strands on the ground: the fire comes out of them"""
    vertices, faces = [], []
    segments = root_segments()
    # The longest arm reaches the full radius of the star
    star_radius /= float(np.max(np.hypot(segments[:, 2], segments[:, 3])))
    for x0, y0, x1, y1, thick, _, _ in segments:
        # A low box along each piece of a root, a little wider than the root, overlapping the next piece
        dx, dy = x1 - x0, y1 - y0
        length = math.hypot(dx, dy)
        ux, uy = dx / length, dy / length
        # Never thinner than a few cells of the simulation, so the tips burn too
        half = max(thick * star_radius * 1.4, 0.05 * star_radius)
        ax, ay = x0 * star_radius - ux * half * 0.5, y0 * star_radius - uy * half * 0.5
        bx, by = x1 * star_radius + ux * half * 0.5, y1 * star_radius + uy * half * 0.5
        height = max(0.02, half * 0.8)
        first = len(vertices)
        for px, py in ((ax, ay), (bx, by)):
            for side in (-1.0, 1.0):
                vertices.append((px - uy * half * side, py + ux * half * side, 0.0))
                vertices.append((px - uy * half * side, py + ux * half * side, height))
        # Corners: 0/1 start left low/high, 2/3 start right, 4/5 end left, 6/7 end right
        faces += [(first + 0, first + 2, first + 6, first + 4), (first + 1, first + 5, first + 7, first + 3),
                  (first + 0, first + 4, first + 5, first + 1), (first + 2, first + 3, first + 7, first + 6),
                  (first + 0, first + 1, first + 3, first + 2), (first + 4, first + 6, first + 7, first + 5)]
    mesh = bpy.data.meshes.new("Wurzeln")
    mesh.from_pydata(vertices, [], faces)
    roots = bpy.data.objects.new("Wurzeln", mesh)
    bpy.context.scene.collection.objects.link(roots)
    s = fuel_settings(roots, fuel, 1.2)
    # The shape of the roots breaks the fire up by itself; a noise over the whole star would blot out whole arms
    s.use_texture = False
    print("FEUER3D roots reach", max(abs(v[0]) for v in vertices), "m")
    return roots


def make_sources():
    sources = []
    if KIND == "kugel":
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.25, location=(0, 0, 0.55), segments=32, ring_count=16)
        ball = bpy.context.active_object
        s = fuel_settings(ball, 1.1, 0.6, 0.04)
        s.use_initial_velocity = True
        s.velocity_normal = 0.4
        sources.append(ball)
    elif KIND == "lager":
        # A campfire: a few logs crossed over a bed of embers, burning calmly
        for index in range(6):
            angle = index * math.pi / 3 + random.uniform(-0.3, 0.3)
            sources.append(add_log(math.cos(angle) * 0.18, math.sin(angle) * 0.18, 0.08 + 0.05 * (index % 2), angle + math.pi / 2,
                                   random.uniform(0.05, 0.08), random.uniform(0.6, 0.9), random.uniform(1.0, 1.3)))
        sources.append(add_bed(0.35, 0.08, 0.7, 1.5))
    else:
        # A Brandherd: the fire comes out of the star of roots, the bigger the stage the further they reach
        sources.append(add_roots(STARS[KIND], 1.1 if KIND != "glut" else 0.8))

    size, centre, _ = DOMAINS[KIND]
    bpy.ops.mesh.primitive_cube_add(location=centre)
    domain = bpy.context.active_object
    domain.name = "Domaene"
    domain.scale = (size[0] / 2, size[1] / 2, size[2] / 2)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return sources, domain


def add_turbulence(scene):
    """A turbulence field that drifts up through the fire and turns: the tongues wind and whip, never twice the same"""
    size, centre, reach = DOMAINS[KIND]
    bpy.ops.object.effector_add(type="TURBULENCE", location=(0, 0, 0))
    field = bpy.context.active_object
    field.field.strength = 1.6 if KIND != "glut" else 0.9
    field.field.size = reach
    field.field.flow = 0.0
    field.field.noise = 0.2
    # The foot stays calm: the field works from a little above the ground upwards
    field.field.use_min_distance = False
    last = WARMUP + FRAMES
    field.location = (0, 0, 0)
    field.rotation_euler = (0, 0, 0)
    field.keyframe_insert("location", frame=1)
    field.keyframe_insert("rotation_euler", frame=1)
    field.location = (reach * 0.7, -reach * 0.5, -size[2] * 0.6)
    field.rotation_euler = (0.4, 0.2, 2.5)
    field.keyframe_insert("location", frame=last)
    field.keyframe_insert("rotation_euler", frame=last)
    for curve in field.animation_data.action.fcurves if hasattr(field.animation_data.action, "fcurves") else []:
        for point in curve.keyframe_points:
            point.interpolation = "LINEAR"


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
    # Calm: hot air rises gently, the body of the fire barely whirls; the tips get their life from the turbulence and a fuel
    # that burns quickly, so they tear off as separate licks
    d.vorticity = 0.15
    d.alpha = 0.0
    d.beta = 1.6
    d.burning_rate = 0.9 if KIND in ("lager", "kugel") else 1.3
    d.flame_smoke = 0.25
    d.flame_vorticity = 0.5
    d.flame_ignition = 1.2
    d.flame_max_temp = 2.2
    d.flame_smoke_color = (0.1, 0.09, 0.08)
    d.use_dissolve_smoke = True
    d.dissolve_speed = 12
    d.time_scale = 0.7
    return d


def bake(domain):
    log("baking ...")
    with bpy.context.temp_override(object=domain, active_object=domain, selected_objects=[domain]):
        bpy.ops.fluid.bake_all()
    log("baked")


def read_frame(cache, frame, shape):
    """Density and flame of a frame of the cache as dense arrays, and the transform of its grids"""
    grids, _ = openvdb.readAll(os.path.join(cache, "data", "fluid_data_{:04d}.vdb".format(frame)))
    by_name = {g.name: g for g in grids}
    arrays = {}
    for name in ("density", "flame"):
        array = np.zeros(shape, dtype=np.float32)
        if name in by_name:
            by_name[name].copyToArray(array, ijk=(0, 0, 0))
        arrays[name] = array
    return arrays, by_name["density"].transform


def export_volumes(cache, domain):
    """Writes a seamless loop of FRAMES - BLEND frames: the first BLEND frames are blended with the frames after the loop"""
    size = DOMAINS[KIND][0]
    voxel = max(size) / RESOLUTION
    shape = tuple(int(round(side / voxel)) + 2 for side in size)
    length = FRAMES - BLEND
    for index in range(length):
        frame, transform = read_frame(cache, WARMUP + 1 + index, shape)
        if index < BLEND:
            weight = index / BLEND
            later, _ = read_frame(cache, WARMUP + 1 + length + index, shape)
            frame = {name: frame[name] * weight + later[name] * (1.0 - weight) for name in frame}
        # A trace of smoke in two far corners: the volume then always spans the whole domain, so Unreal knows where its floor is
        frame["density"][0, 0, 0] = 0.003
        frame["density"][-1, -1, -1] = 0.003
        grids = []
        for name in ("density", "flame"):
            grid = openvdb.FloatGrid()
            grid.copyFromArray(frame[name], ijk=(0, 0, 0), tolerance=0.002)
            grid.name = name
            grid.transform = transform
            grids.append(grid)
        openvdb.write(os.path.join(OUT, "vdb", "feuer_{}_{:04d}.vdb".format(KIND, index + 1)), grids=grids)
    log("exported {} volumes, resolution {}".format(length, shape))


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

    size = DOMAINS[KIND][0]
    bpy.ops.mesh.primitive_plane_add(size=size[0] * 3, location=(0, 0, 0))
    ground = bpy.context.active_object
    ground_mat = bpy.data.materials.new("Boden")
    ground_mat.diffuse_color = (0.12, 0.09, 0.06, 1)
    ground.data.materials.append(ground_mat)

    cam = bpy.data.objects.new("Kamera", bpy.data.cameras.new("Kamera"))
    scene.collection.objects.link(cam)
    far = size[2] / 2.4
    cam.location = (0, -3.6 * far, 3.4 * far)
    cam.rotation_euler = (math.radians(55), 0, 0)
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



if os.environ.get("FEUER_NUR_EXPORT"):
    # Only writes the volumes again from a cache baked before
    export_volumes(os.path.join(OUT, "cache"), None)
    log("done")
    sys.exit(0)

scene = scene_setup()
sources, domain = make_sources()
add_turbulence(scene)
settings = setup_domain(domain)
bake(domain)
export_volumes(settings.cache_directory, domain)
preview(scene, sources, domain)
log("done")
