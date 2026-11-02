# Loot kit: builds loot items as meshes, exports them and renders their inventory icons in one fixed icon studio.
#
#   blender -b --factory-startup -P Tools/Loot/vael_loot_baukasten.py -- C:/Projekte/Vael/Scripts/Data/Loot [teile]
#
# Teile: modelle (FBX), icons (512 px icons of every look), vorschau (lineup pictures to judge the shapes); none = all.
# Writes Modelle/SM_Loot_*.fbx, Icons/T_Icon_*.png and Vorschau/*.png below the folder.
#
# New items: add a shape builder (like crystal_shard or bottle) to MODELS and a look (a Blender stand-in of the
# Unreal material) to LOOKS, then a line to ICONS. The icon studio (icon_studio) is the standard for every item icon:
# same camera angle, same light, transparent background, the item fitted to the frame.
# All meshes are in cm, 1 Blender unit = 1 cm in Unreal (apply_unit_scale in the export), pivot at the bottom centre,
# Z up. Each model gets a UCX_ convex hull that Unreal takes as simple collision.

import math
import os
import random
import sys

import bmesh
import bpy
import mathutils

ICON_SIZE = 512

# Icon studio camera: looks down at the item from the front right, like the iso camera but steeper for readability
ICON_ELEVATION = 24.0
ICON_AZIMUTH = -32.0
ICON_LENS = 85.0
# Share of the frame the item fills
ICON_FILL = 0.86


def out_folder():
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    return arguments[0] if arguments else os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")


def wanted_parts():
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parts = set(arguments[1:])
    return parts or {"modelle", "icons", "vorschau"}


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = 0.01


# ---------------------------------------------------------------------------------------------------------------
# Shapes
# ---------------------------------------------------------------------------------------------------------------

def hull_object(name, points, material_name):
    """Faceted solid from the convex hull of points, flat shaded"""
    mesh = bmesh.new()
    verts = [mesh.verts.new(point) for point in points]
    result = bmesh.ops.convex_hull(mesh, input=verts, use_existing_faces=False)
    loose = {v for v in result["geom_interior"] + result["geom_unused"] if isinstance(v, bmesh.types.BMVert)}
    bmesh.ops.delete(mesh, geom=list(loose), context="VERTS")
    bmesh.ops.dissolve_limit(mesh, angle_limit=math.radians(1.0), verts=mesh.verts[:], edges=mesh.edges[:])
    bmesh.ops.triangulate(mesh, faces=mesh.faces[:])
    data = bpy.data.meshes.new(name)
    mesh.to_mesh(data)
    mesh.free()
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(slot_material(material_name))
    for polygon in obj.data.polygons:
        polygon.use_smooth = False
    return obj


def shard_points(height, radius, rng, lean=(0.0, 0.0), origin=(0.0, 0.0, 0.0)):
    """Points of one crystal shard: an irregular prism of 5-7 sides that ends in an off-centre tip.
    Rings get their own radius and turn so the facets are of uneven width; some edges are chipped."""
    sides = rng.randint(5, 7)
    turn = rng.uniform(0.0, math.tau)
    angles = sorted((turn + math.tau * (i + rng.uniform(-0.28, 0.28)) / sides) for i in range(sides))
    widths = [rng.uniform(0.78, 1.12) for _ in range(sides)]

    # Height share, radius share and jitter of each ring: broken foot, body, shoulder where the tip facets begin
    rings = [(0.0, 0.82, 0.05), (rng.uniform(0.08, 0.14), 0.95, 0.06), (rng.uniform(0.5, 0.62), 1.0, 0.07), (rng.uniform(0.72, 0.8), 0.66, 0.1)]
    points = []
    for share, ring_radius, jitter in rings:
        for angle, width in zip(angles, widths):
            r = radius * ring_radius * width * rng.uniform(1.0 - jitter, 1.0 + jitter)
            z = height * share + (height * rng.uniform(-0.025, 0.025) if share > 0.0 else 0.0)
            points.append((math.cos(angle) * r, math.sin(angle) * r, z))

    # The tip sits off the axis and has a small ridge, real shards rarely end in one point
    tip = (rng.uniform(-0.22, 0.22) * radius, rng.uniform(-0.22, 0.22) * radius, height)
    points.append(tip)
    ridge = rng.uniform(0.0, math.tau)
    points.append((tip[0] + math.cos(ridge) * radius * 0.16, tip[1] + math.sin(ridge) * radius * 0.16, height * rng.uniform(0.92, 0.97)))

    # Chips: a few points pulled inward along an edge break the regular silhouette
    for _ in range(rng.randint(2, 4)):
        index = rng.randrange(sides, len(points) - 2)
        x, y, z = points[index]
        points[index] = (x * rng.uniform(0.7, 0.85), y * rng.uniform(0.7, 0.85), z)

    lean_x, lean_y = lean
    return [(origin[0] + x + lean_x * z, origin[1] + y + lean_y * z, origin[2] + z) for x, y, z in points]


def crystal_shard(name, height, seed, material_name="Kristall", side_shards=0):
    """A Nid crystal: one leaning shard, the bigger sizes with smaller shards grown from its foot (one mesh, several shells).
    The same form serves the purified crystal, only the material changes."""
    rng = random.Random(seed)
    radius = height * rng.uniform(0.21, 0.25)
    lean = (rng.uniform(-0.12, 0.12), rng.uniform(-0.12, 0.12))
    objects = [hull_object(name, shard_points(height, radius, rng, lean), material_name)]
    for index in range(side_shards):
        angle = rng.uniform(0.0, math.tau) + index * math.pi * 0.9
        side_height = height * rng.uniform(0.42, 0.6)
        side_radius = radius * rng.uniform(0.55, 0.7)
        origin = (math.cos(angle) * radius * 0.75, math.sin(angle) * radius * 0.75, 0.0)
        side_lean = (math.cos(angle) * rng.uniform(0.35, 0.55), math.sin(angle) * rng.uniform(0.35, 0.55))
        objects.append(hull_object(f"{name}_Seite{index}", shard_points(side_height, side_radius, rng, side_lean, origin), material_name))
    return join(objects, name)


def revolve(name, profile, segments, wobble, seed, material_names, profile_materials=None, cap_bottom=True):
    """Lathe body from a profile of (radius, z) points, bottom to top along the outside and back down inside.
    Points with radius 0 become one vertex on the axis. Wobble makes it hand made: each ring is slightly
    off-round and off-centre, so no two sides of the glass are alike."""
    rng = random.Random(seed)
    phases = [rng.uniform(0.0, math.tau) for _ in range(3)]
    mesh = bmesh.new()
    rings = []
    for index, (r, z) in enumerate(profile):
        if r <= 0.0:
            rings.append([mesh.verts.new((0.0, 0.0, z))])
            continue
        height_noise = math.sin(z * 0.9 + phases[0]) * wobble
        shift = (math.cos(phases[1] + z * 0.35) * wobble * r * 0.6, math.sin(phases[2] + z * 0.3) * wobble * r * 0.6)
        ring = []
        for step in range(segments):
            angle = math.tau * step / segments
            off_round = 1.0 + wobble * (0.6 * math.sin(2.0 * angle + phases[0] + z * 0.2) + 0.4 * math.sin(3.0 * angle + phases[1])) + height_noise * 0.3
            ring.append(mesh.verts.new((shift[0] + math.cos(angle) * r * off_round, shift[1] + math.sin(angle) * r * off_round, z)))
        rings.append(ring)

    for index in range(len(rings) - 1):
        lower, upper = rings[index], rings[index + 1]
        material_index = profile_materials[index] if profile_materials else 0
        faces = []
        if len(lower) == 1 and len(upper) == 1:
            continue
        if len(lower) == 1:
            faces = [mesh.faces.new((lower[0], upper[step], upper[(step + 1) % segments])) for step in range(segments)]
        elif len(upper) == 1:
            faces = [mesh.faces.new((lower[(step + 1) % segments], lower[step], upper[0])) for step in range(segments)]
        else:
            faces = [mesh.faces.new((lower[step], lower[(step + 1) % segments], upper[(step + 1) % segments], upper[step])) for step in range(segments)]
        for face in faces:
            face.material_index = material_index

    bmesh.ops.recalc_face_normals(mesh, faces=mesh.faces[:])
    data = bpy.data.meshes.new(name)
    mesh.to_mesh(data)
    mesh.free()
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    for material_name in material_names:
        obj.data.materials.append(slot_material(material_name))
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


# Bottle profile in cm: a squat belly, short neck and a lip; the glass is THICKNESS thick
BOTTLE_OUTER = [(0.0, 0.25), (2.6, 0.0), (3.6, 0.35), (4.35, 1.4), (4.7, 3.0), (4.6, 4.8), (4.05, 6.4), (3.0, 7.6),
                (1.75, 8.5), (1.3, 9.3), (1.22, 10.6), (1.3, 11.4), (1.62, 11.6), (1.66, 12.0), (1.42, 12.25)]
BOTTLE_THICKNESS = 0.22


def inner_profile(outer, thickness):
    """Inside of the glass: the outside moved inward, the bottom thicker as on blown glass"""
    inner = []
    for r, z in reversed(outer[1:-3]):
        inner.append((max(r - thickness, 0.0), z + (thickness * 2.5 if z < 0.5 else 0.0)))
    inner.append((0.0, 0.25 + thickness * 2.8))
    return inner


def radius_at(profile, z):
    """Radius of an upward running profile at a height"""
    for (r0, z0), (r1, z1) in zip(profile, profile[1:]):
        if z0 <= z <= z1 and z1 > z0:
            return r0 + (r1 - r0) * (z - z0) / (z1 - z0)
    return profile[-1][0]


def bottle(name, seed=7, fill=0.72):
    """A potion bottle: glass and cork in one mesh (material slots Glas, Korken), the liquid as its own mesh
    (slot Fluessigkeit), so colour and fill can be set apart. Fill is the share of the belly the liquid reaches."""
    inner = inner_profile(BOTTLE_OUTER, BOTTLE_THICKNESS)
    rim = BOTTLE_OUTER[-1]
    glass_profile = BOTTLE_OUTER + [(rim[0] - BOTTLE_THICKNESS * 0.6, rim[1] - 0.1)] + inner[1:]
    glass = revolve(name, glass_profile, 56, 0.018, seed, ["Glas"])

    # Cork: pressed into the neck, a little wider on top, uneven and not quite straight
    cork_profile = [(0.0, 10.7), (1.02, 10.7), (1.07, 11.6), (1.2, 12.4), (1.38, 13.3), (1.34, 13.6), (1.05, 13.78), (0.0, 13.82)]
    cork = revolve(name + "_Korken", cork_profile, 28, 0.05, seed + 1, ["Korken"])
    # Tilted around the point where it sits in the neck, not around the foot of the bottle
    tilt = mathutils.Matrix.Translation((0.0, 0.0, 11.2)) @ mathutils.Euler((math.radians(2.5), math.radians(-1.5), 0.0)).to_matrix().to_4x4() @ mathutils.Matrix.Translation((0.0, 0.0, -11.2))
    cork.data.transform(tilt)
    glass = join([glass, cork], name)

    # Liquid: follows the inside a hair inward, a flat surface at the fill height
    upward_inner = [(r - 0.04, z) for r, z in reversed(inner) if r > 0.0]
    bottom = inner[-1][1] + 0.03
    top = bottom + (6.4 - bottom) * fill
    liquid_profile = ([(0.0, bottom), (radius_at(upward_inner, bottom + 0.1) * 0.97, bottom)]
                      + [(r, z) for r, z in upward_inner if bottom + 0.1 < z < top] + [(radius_at(upward_inner, top), top), (0.0, top)])
    liquid = revolve(name + "_Fluessigkeit", liquid_profile, 56, 0.0, seed, ["Fluessigkeit"])
    return glass, liquid


def join(objects, name):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    if len(objects) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = name
    obj.data.name = name
    return obj


def add_uvs(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(60.0), island_margin=0.02)
    bpy.ops.object.mode_set(mode="OBJECT")


def collision_hull(obj):
    """UCX_<name>: convex hull Unreal uses as simple collision"""
    points = [obj.matrix_world @ v.co for v in obj.data.vertices]
    hull = hull_object("UCX_" + obj.name, points, "Kollision")
    hull.data.materials.clear()
    return hull


# Models of the kit: file name → builder. Sizes of the crystals in cm (height of the main shard).
MODELS = {
    "SM_Loot_Kristall_Klein": lambda: crystal_shard("SM_Loot_Kristall_Klein", 5.0, 11),
    "SM_Loot_Kristall_Mittel": lambda: crystal_shard("SM_Loot_Kristall_Mittel", 8.5, 23, side_shards=1),
    "SM_Loot_Kristall_Gross": lambda: crystal_shard("SM_Loot_Kristall_Gross", 13.0, 37, side_shards=3),
    "SM_Loot_Flasche": lambda: bottle("SM_Loot_Flasche"),
}


# ---------------------------------------------------------------------------------------------------------------
# Looks: Blender stand-ins of the Unreal loot material (M_Vael_Loot), only for icons and previews.
# Parameters as in Unreal: Farbe, Leuchten, Truebung (0 clear .. 1 milky), Adern (0 none .. 1 strong), AderFarbe.
# ---------------------------------------------------------------------------------------------------------------

LOOKS = {
    "Nidkristall": {"Farbe": (0.09, 0.012, 0.2), "Leuchten": 2.4, "Truebung": 0.3, "Adern": 1.0, "AderFarbe": (0.8, 0.35, 1.0)},
    "GelaeuterterKristall": {"Farbe": (0.85, 0.42, 0.06), "Leuchten": 0.8, "Truebung": 0.08, "Adern": 0.2, "AderFarbe": (1.0, 0.86, 0.5)},
    "Nidtrunk": {"Farbe": (0.2, 0.025, 0.32), "Leuchten": 0.8, "Truebung": 0.85, "Adern": 0.0, "AderFarbe": (0.6, 0.2, 1.0)},
    "Klartrunk": {"Farbe": (1.0, 0.7, 0.28), "Leuchten": 0.6, "Truebung": 0.03, "Adern": 0.0, "AderFarbe": (1.0, 0.9, 0.7)},
}


def slot_material(name):
    """Plain material named like the slot in Unreal; looks are put on in apply_look"""
    material = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    material.use_nodes = True
    return material


def look_material(look_name, kind):
    """Builds the stand-in material: kind is 'kristall' (faceted solid with veins and an inner glow) or 'fluessigkeit'"""
    look = LOOKS[look_name]
    material = bpy.data.materials.new(f"Look_{look_name}_{kind}")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    shader = nodes["Principled BSDF"]
    color = (*look["Farbe"], 1.0)
    shader.inputs["Base Color"].default_value = color
    shader.inputs["Roughness"].default_value = 0.12 if kind == "kristall" else 0.05
    shader.inputs["IOR"].default_value = 1.55 if kind == "kristall" else 1.34
    shader.inputs["Transmission Weight"].default_value = 1.0 - look["Truebung"] * 0.85
    shader.inputs["Subsurface Weight"].default_value = look["Truebung"] * 0.6
    shader.inputs["Subsurface Radius"].default_value = (look["Farbe"][0] * 2.0 + 0.1, look["Farbe"][1] * 2.0 + 0.1, look["Farbe"][2] * 2.0 + 0.1)
    shader.inputs["Subsurface Scale"].default_value = 0.6
    shader.inputs["Emission Color"].default_value = color

    # Inner glow: strongest where the eye looks into the body, weak at grazing facets
    facing = nodes.new("ShaderNodeLayerWeight")
    facing.inputs["Blend"].default_value = 0.45
    core = nodes.new("ShaderNodeMath")
    core.operation = "SUBTRACT"
    core.inputs[0].default_value = 1.0
    links.new(facing.outputs["Facing"], core.inputs[1])
    glow = nodes.new("ShaderNodeMath")
    glow.operation = "MULTIPLY"
    glow.inputs[1].default_value = look["Leuchten"]
    links.new(core.outputs[0], glow.inputs[0])

    if look["Adern"] > 0.0:
        # Veins: thin bright lines along the edges of a stretched cell noise in object space
        coords = nodes.new("ShaderNodeTexCoord")
        stretch = nodes.new("ShaderNodeMapping")
        stretch.inputs["Scale"].default_value = (0.9, 0.9, 0.45)
        links.new(coords.outputs["Object"], stretch.inputs["Vector"])
        warp = nodes.new("ShaderNodeTexNoise")
        warp.inputs["Scale"].default_value = 0.6
        warp.inputs["Detail"].default_value = 3.0
        mix = nodes.new("ShaderNodeMix")
        mix.data_type = "VECTOR"
        # Mix sockets share names across data types, so they are taken by index (vector: A 4, B 5, result 1)
        mix.inputs[0].default_value = 0.35
        links.new(stretch.outputs["Vector"], mix.inputs[4])
        links.new(warp.outputs["Color"], mix.inputs[5])
        cells = nodes.new("ShaderNodeTexVoronoi")
        cells.feature = "DISTANCE_TO_EDGE"
        cells.inputs["Scale"].default_value = 1.1
        cells.inputs["Randomness"].default_value = 1.0
        links.new(mix.outputs[1], cells.inputs["Vector"])
        line = nodes.new("ShaderNodeMapRange")
        line.inputs["From Min"].default_value = 0.0
        line.inputs["From Max"].default_value = 0.018
        line.inputs["To Min"].default_value = look["Adern"]
        line.inputs["To Max"].default_value = 0.0
        links.new(cells.outputs["Distance"], line.inputs["Value"])
        # Only some cell edges carry a vein: a coarse noise breaks the net into single branching threads
        breaks = nodes.new("ShaderNodeTexNoise")
        breaks.inputs["Scale"].default_value = 0.9
        breaks.inputs["Detail"].default_value = 2.0
        links.new(stretch.outputs["Vector"], breaks.inputs["Vector"])
        threads = nodes.new("ShaderNodeMapRange")
        threads.inputs["From Min"].default_value = 0.5
        threads.inputs["From Max"].default_value = 0.62
        links.new(breaks.outputs["Fac"], threads.inputs["Value"])
        thread_line = nodes.new("ShaderNodeMath")
        thread_line.operation = "MULTIPLY"
        links.new(line.outputs["Result"], thread_line.inputs[0])
        links.new(threads.outputs["Result"], thread_line.inputs[1])
        line = thread_line
        vein_glow = nodes.new("ShaderNodeMath")
        vein_glow.operation = "MULTIPLY"
        vein_glow.inputs[1].default_value = look["Leuchten"] * 3.0
        links.new(line.outputs[0], vein_glow.inputs[0])
        total = nodes.new("ShaderNodeMath")
        total.operation = "ADD"
        links.new(glow.outputs[0], total.inputs[0])
        links.new(vein_glow.outputs[0], total.inputs[1])
        tint = nodes.new("ShaderNodeMix")
        tint.data_type = "RGBA"
        # Colour mix by index as well (RGBA: A 6, B 7, result 2)
        tint.inputs[6].default_value = color
        tint.inputs[7].default_value = (*look["AderFarbe"], 1.0)
        links.new(line.outputs[0], tint.inputs[0])
        links.new(tint.outputs[2], shader.inputs["Emission Color"])
        links.new(total.outputs[0], shader.inputs["Emission Strength"])
    else:
        links.new(glow.outputs[0], shader.inputs["Emission Strength"])
    return material


def fixed_material(name):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    shader = material.node_tree.nodes["Principled BSDF"]
    if name == "Look_Glas":
        shader.inputs["Base Color"].default_value = (0.92, 0.95, 0.9, 1.0)
        shader.inputs["Roughness"].default_value = 0.04
        shader.inputs["IOR"].default_value = 1.47
        shader.inputs["Transmission Weight"].default_value = 1.0
    else:
        # Cork: brown, rough, pitted by a noise bump
        nodes = material.node_tree.nodes
        links = material.node_tree.links
        noise = nodes.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = 9.0
        noise.inputs["Detail"].default_value = 6.0
        ramp = nodes.new("ShaderNodeValToRGB")
        ramp.color_ramp.elements[0].color = (0.16, 0.09, 0.045, 1.0)
        ramp.color_ramp.elements[1].color = (0.42, 0.27, 0.14, 1.0)
        links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
        links.new(ramp.outputs["Color"], shader.inputs["Base Color"])
        bump = nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.5
        links.new(noise.outputs["Fac"], bump.inputs["Height"])
        links.new(bump.outputs["Normal"], shader.inputs["Normal"])
        shader.inputs["Roughness"].default_value = 0.85
    return material


def apply_look(obj, look_name):
    """Swaps the slot materials of an object for the look; glass and cork keep their own"""
    for index, material in enumerate(obj.data.materials):
        if material is None:
            continue
        if material.name.startswith("Kristall"):
            obj.data.materials[index] = look_material(look_name, "kristall")
        elif material.name.startswith("Fluessigkeit"):
            obj.data.materials[index] = look_material(look_name, "fluessigkeit")
        elif material.name.startswith("Glas"):
            obj.data.materials[index] = fixed_material("Look_Glas")
        elif material.name.startswith("Korken"):
            obj.data.materials[index] = fixed_material("Look_Korken")


# Icons: icon name → (look, models to show)
ICONS = {
    "T_Icon_Nidkristall": ("Nidkristall", ["SM_Loot_Kristall_Mittel"]),
    "T_Icon_GelaeuterterKristall": ("GelaeuterterKristall", ["SM_Loot_Kristall_Mittel"]),
    "T_Icon_Nidtrunk": ("Nidtrunk", ["SM_Loot_Flasche"]),
    "T_Icon_Klartrunk": ("Klartrunk", ["SM_Loot_Flasche"]),
}


# ---------------------------------------------------------------------------------------------------------------
# Icon studio
# ---------------------------------------------------------------------------------------------------------------

def icon_studio(resolution_x=ICON_SIZE, resolution_y=ICON_SIZE, transparent=True):
    """The fixed studio every item icon is rendered in: Cycles, three suns (warm key from the upper left,
    cool rim from behind, soft fill), a dim world for reflections, transparent film. Returns the camera."""
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 160
    scene.cycles.use_denoising = True
    scene.cycles.max_bounces = 16
    scene.cycles.transmission_bounces = 16
    scene.cycles.transparent_max_bounces = 16
    use_gpu(scene)
    scene.render.film_transparent = transparent
    scene.cycles.film_transparent_glass = transparent
    scene.render.resolution_x = resolution_x
    scene.render.resolution_y = resolution_y
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA" if transparent else "RGB"
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = "AgX - Medium High Contrast"

    world = bpy.data.worlds.new("Studio")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.05, 0.045, 0.05, 1.0)
    world.node_tree.nodes["Background"].inputs[1].default_value = 1.0
    scene.world = world

    for name, energy, color, elevation, azimuth in (("Schluessel", 3.2, (1.0, 0.9, 0.78), 50.0, 140.0),
                                                     ("Kante", 4.0, (0.7, 0.8, 1.0), 20.0, -10.0),
                                                     ("Fuell", 0.8, (0.85, 0.85, 1.0), 10.0, -150.0)):
        light = bpy.data.lights.new(name, "SUN")
        light.energy = energy
        light.color = color
        light.angle = math.radians(8.0)
        light_object = bpy.data.objects.new(name, light)
        direction = direction_vector(elevation, azimuth)
        light_object.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()
        bpy.context.collection.objects.link(light_object)

    camera = bpy.data.cameras.new("IconKamera")
    camera.lens = ICON_LENS
    camera.clip_start = 0.5
    camera.clip_end = 10000.0
    camera_object = bpy.data.objects.new("IconKamera", camera)
    bpy.context.collection.objects.link(camera_object)
    scene.camera = camera_object
    return camera_object


def use_gpu(scene):
    """Renders on the graphics card when there is one (OptiX, else CUDA); the CPU takes minutes per glass picture"""
    preferences = bpy.context.preferences.addons["cycles"].preferences
    for device_type in ("OPTIX", "CUDA"):
        try:
            preferences.compute_device_type = device_type
        except TypeError:
            continue
        preferences.get_devices()
        gpus = [device for device in preferences.devices if device.type == device_type]
        if gpus:
            for device in preferences.devices:
                device.use = device.type == device_type
            scene.cycles.device = "GPU"
            return
    scene.cycles.device = "CPU"


def direction_vector(elevation, azimuth):
    """Unit vector from the subject toward a point at this elevation; azimuth 0 = in front (-Y), positive = to the left"""
    e, a = math.radians(elevation), math.radians(azimuth)
    return mathutils.Vector((-math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e)))


def frame_objects(camera_object, objects, elevation=ICON_ELEVATION, azimuth=ICON_AZIMUTH, fill=ICON_FILL):
    """Points the camera at the bounding sphere of the objects from the fixed angle, at the distance that fills the frame"""
    points = [obj.matrix_world @ v.co for obj in objects for v in obj.data.vertices]
    low = mathutils.Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points)))
    high = mathutils.Vector((max(p.x for p in points), max(p.y for p in points), max(p.z for p in points)))
    centre = (low + high) * 0.5
    radius = max((p - centre).length for p in points)
    camera = camera_object.data
    scene = bpy.context.scene
    aspect = scene.render.resolution_x / scene.render.resolution_y
    sensor_fit_angle = 2.0 * math.atan(camera.sensor_width * 0.5 / camera.lens)
    half_angle = sensor_fit_angle * 0.5 if aspect >= 1.0 else math.atan(math.tan(sensor_fit_angle * 0.5) * aspect)
    distance = radius / math.sin(half_angle) / fill
    direction = direction_vector(elevation, azimuth)
    camera_object.location = centre + direction * distance
    camera_object.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()


def render(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("rendered", path)


# ---------------------------------------------------------------------------------------------------------------
# Jobs
# ---------------------------------------------------------------------------------------------------------------

def export(objects, path):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                             mesh_smooth_type="FACE", colors_type="NONE", add_leaf_bones=False, axis_forward="-Z", axis_up="Y", use_tspace=True)
    print("exported", path)


def build_model(name):
    """Builds a model of the kit; returns its render meshes (the bottle has two: glass with cork, liquid)"""
    result = MODELS[name]()
    meshes = list(result) if isinstance(result, tuple) else [result]
    for obj in meshes:
        add_uvs(obj)
    return meshes


def make_models(folder):
    os.makedirs(folder, exist_ok=True)
    for name in MODELS:
        reset()
        meshes = build_model(name)
        # Every mesh its own FBX with its own hull, so the liquid can be a component of its own in Unreal
        for obj in meshes:
            hull = collision_hull(obj)
            export([obj, hull], os.path.join(folder, obj.name + ".fbx"))


def make_icons(folder):
    os.makedirs(folder, exist_ok=True)
    for icon, (look, models) in ICONS.items():
        reset()
        camera = icon_studio()
        shown = []
        for model in models:
            for obj in build_model(model):
                apply_look(obj, look)
                shown.append(obj)
        frame_objects(camera, shown)
        render(os.path.join(folder, icon + ".png"))


def make_previews(folder):
    """Lineups to judge shape and look before Unreal: the three crystal sizes in both looks, both potions,
    on a dark ground plane in the studio light, larger than icons"""
    os.makedirs(folder, exist_ok=True)
    for title, entries in (("Vorschau_Kristalle", [("SM_Loot_Kristall_Klein", "Nidkristall"), ("SM_Loot_Kristall_Mittel", "Nidkristall"),
                                                    ("SM_Loot_Kristall_Gross", "Nidkristall"), ("SM_Loot_Kristall_Gross", "GelaeuterterKristall"),
                                                    ("SM_Loot_Kristall_Mittel", "GelaeuterterKristall"), ("SM_Loot_Kristall_Klein", "GelaeuterterKristall")]),
                           ("Vorschau_Traenke", [("SM_Loot_Flasche", "Nidtrunk"), ("SM_Loot_Flasche", "Klartrunk")])):
        reset()
        camera = icon_studio(1600, 900, transparent=False)
        bpy.context.scene.cycles.samples = 256
        shown = []
        x = 0.0
        spacing = 16.0 if "Kristalle" in title else 14.0
        for model, look in entries:
            for obj in build_model(model):
                apply_look(obj, look)
                obj.location.x = x
                shown.append(obj)
            x += spacing
        bpy.context.view_layer.update()
        ground(shown)
        frame_objects(camera, shown, elevation=18.0, azimuth=-8.0, fill=0.8 if "Kristalle" in title else 0.55)
        render(os.path.join(folder, title + ".png"))

        # Close look at the middle-sized crystal / one bottle from the icon angle, to see facets and glass
        reset()
        camera = icon_studio(1000, 1000, transparent=False)
        bpy.context.scene.cycles.samples = 256
        model, look = entries[1] if "Kristalle" in title else entries[0]
        shown = []
        for obj in build_model(model):
            apply_look(obj, look)
            shown.append(obj)
        ground(shown)
        frame_objects(camera, shown, fill=0.8)
        render(os.path.join(folder, title + "_Nah.png"))


def ground(objects):
    """Dark stony ground under the lineup so the glow and shadows can be judged"""
    points = [obj.matrix_world @ v.co for obj in objects for v in obj.data.vertices]
    centre_x = (min(p.x for p in points) + max(p.x for p in points)) * 0.5
    bpy.ops.mesh.primitive_plane_add(size=400.0, location=(centre_x, 0.0, 0.0))
    plane = bpy.context.active_object
    material = bpy.data.materials.new("Boden")
    material.use_nodes = True
    shader = material.node_tree.nodes["Principled BSDF"]
    shader.inputs["Base Color"].default_value = (0.03, 0.028, 0.03, 1.0)
    shader.inputs["Roughness"].default_value = 0.8
    plane.data.materials.append(material)


def main():
    folder = out_folder()
    parts = wanted_parts()
    if "modelle" in parts:
        make_models(os.path.join(folder, "Modelle"))
    if "icons" in parts:
        make_icons(os.path.join(folder, "Icons"))
    if "vorschau" in parts:
        make_previews(os.path.join(folder, "Vorschau"))


main()
