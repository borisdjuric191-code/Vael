# The Keimpelz pelt of the Floromant HUD (Boris 2026-11-04): a juicy dark green fur pad that lies in the moss groove along the
# top of her painted frame, like the pelt on her forearms. Real hair strands (Blender hair curves) in clumps and tufts, dark
# at the roots, juicy green at the tips, a few cracked seeds with a germ nestled in it.
#
# Two pictures, the same size (2x the display size 408 x 40 of the bar):
#   T_HUD_Keimpelz_Pelz.png   the lit fur, RGBA
#   T_HUD_Keimpelz_Daten.png  data, linear: R = place along the strand (0 root, 1 tip), G = where the strand stands along the
#                             bar (0 left, 1 right), B = a random number per strand. M_HUD_Pelz lets each strand shrink back
#                             into the ground near the edge of the fill and grow out of it again, strand by strand.
#
# Run:  blender.exe -b --factory-startup -P Tools/HUD/vael_keimpelz_pelz.py -- <absolute output folder>

import math
import os
import random
import sys

import bpy
import numpy as np
from mathutils import Vector

WIDTH, HEIGHT = 408.0, 40.0          # display px of the bar; 1 Blender unit = 1 display px
RENDER_SCALE = 2
STRANDS = 34000
POINTS = 6
CLUMPS = 120
BASE_Z = 13.0                         # height of the groove floor above the bottom of the picture
PAD_FROM, PAD_TO = 9.0, 399.0         # where the groove holds the pad

rng = np.random.default_rng(1104)
random.seed(1104)


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def setup(samples):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = samples
    scene.cycles.use_denoising = False
    scene.render.film_transparent = True
    scene.render.resolution_x = int(WIDTH * RENDER_SCALE)
    scene.render.resolution_y = int(HEIGHT * RENDER_SCALE)
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    scene.cycles_curves.shape = "THICK"
    scene.cycles_curves.subdivisions = 2

    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = WIDTH
    cam_data.clip_end = 5000.0
    cam = bpy.data.objects.new("Cam", cam_data)
    cam.location = (WIDTH * 0.5, -1000.0, HEIGHT * 0.5)
    cam.rotation_euler = (math.radians(90.0), 0.0, 0.0)
    bpy.context.collection.objects.link(cam)
    scene.camera = cam
    return scene


def light(name, location, energy, size, color):
    data = bpy.data.lights.new(name, "AREA")
    data.energy = energy
    data.color = color
    data.size = size
    data.shape = "DISK"
    obj = bpy.data.objects.new(name, data)
    obj.location = location
    target = Vector((WIDTH * 0.5, 0.0, BASE_Z))
    obj.rotation_euler = (target - Vector(location)).to_track_quat("-Z", "Y").to_euler()
    bpy.context.collection.objects.link(obj)


def lights():
    s = WIDTH
    # Key from the upper left front as on the whole HUD, a soft fill, a strong rim from behind that makes the tips juicy
    light("Key", (-0.6 * s, -1.0 * s, 0.9 * s), 260.0 * s * s, 0.5 * s, (1.0, 0.95, 0.85))
    light("Fill", (1.2 * s, -0.9 * s, 0.1 * s), 50.0 * s * s, 0.8 * s, (0.8, 0.9, 1.0))
    light("Rim", (0.5 * s, 0.9 * s, 0.7 * s), 140.0 * s * s, 0.6 * s, (0.8, 1.0, 0.75))
    world = bpy.data.worlds.new("World")
    bpy.context.scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.02, 0.03, 0.02, 1.0)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.3


# ---------------------------------------------------------------- the fur

def end_taper(x):
    """1 in the middle of the pad, rounding off to its ends"""
    t = np.clip((x - PAD_FROM) / 14.0, 0.0, 1.0) * np.clip((PAD_TO - x) / 14.0, 0.0, 1.0)
    return np.sqrt(t)


def make_strands():
    # Clumps along the pad: each leans its own way and has its own height, so the top is tufty, never a clean line
    clump_x = np.sort(rng.uniform(PAD_FROM, PAD_TO, CLUMPS))
    clump_y = rng.uniform(-6.0, 4.0, CLUMPS)
    clump_len = rng.uniform(15.0, 23.0, CLUMPS) * (0.85 + 0.3 * np.sin(clump_x * 0.03 + 1.3) ** 2)
    clump_lean = rng.uniform(-0.7, 0.7, CLUMPS)
    clump_lean += 0.35 * np.sin(clump_x * 0.018)      # a slow combed flow along the bar
    clump_fore = rng.uniform(-0.5, 0.05, CLUMPS)       # most lean a little towards the viewer

    x = rng.uniform(PAD_FROM - 2.0, PAD_TO + 2.0, STRANDS)
    y = rng.uniform(-7.0, 7.0, STRANDS)
    # The pad is a soft cushion: higher in the middle of its depth
    cushion = 4.0 * np.sqrt(np.clip(1.0 - (y / 7.5) ** 2, 0.0, 1.0))
    z = BASE_Z - 3.0 + cushion + rng.uniform(-0.6, 0.6, STRANDS)

    nearest = np.clip(np.searchsorted(clump_x, x), 0, CLUMPS - 1)
    left = np.clip(nearest - 1, 0, CLUMPS - 1)
    pick = np.where(np.abs(clump_x[left] - x) < np.abs(clump_x[nearest] - x), left, nearest)

    taper = end_taper(x)
    # Each lock is a rounded tuft: longest at its middle
    spacing = (PAD_TO - PAD_FROM) / CLUMPS
    mound = np.clip(1.0 - 0.55 * ((x - clump_x[pick]) / spacing) ** 2, 0.4, 1.0)
    length = clump_len[pick] * mound * rng.uniform(0.55, 1.1, STRANDS) * (0.35 + 0.65 * taper)
    # The front rows are shorter, so the pad shows its depth
    length *= 0.75 + 0.25 * np.clip((y + 7.0) / 10.0, 0.0, 1.0)

    lean = clump_lean[pick] + rng.normal(0.0, 0.32, STRANDS)
    fore = clump_fore[pick] + rng.normal(0.0, 0.15, STRANDS)
    direction = np.stack([lean, fore, np.ones(STRANDS)], axis=1)
    direction /= np.linalg.norm(direction, axis=1, keepdims=True)

    roots = np.stack([x, y, z], axis=1)
    # Tips of a clump meet a little: the clump tip is the clump root along its own direction
    clump_dir = np.stack([clump_lean, clump_fore, np.ones(CLUMPS)], axis=1)
    clump_dir /= np.linalg.norm(clump_dir, axis=1, keepdims=True)
    clump_tip = np.stack([clump_x, clump_y, np.full(CLUMPS, BASE_Z + 1.0)], axis=1) + clump_dir * clump_len[:, None]

    points = np.zeros((STRANDS, POINTS, 3))
    curl = rng.uniform(-0.6, 0.6, STRANDS)
    for k in range(POINTS):
        t = k / (POINTS - 1)
        free = roots + direction * (length * t)[:, None]
        own_tip = roots + direction * length[:, None]
        pulled = own_tip + (clump_tip[pick] - own_tip) * 0.45
        tip_point = free + (pulled - own_tip) * t * t
        # Heavy juicy tips droop a little, and curl sideways
        tip_point[:, 2] -= length * 0.26 * t ** 2.2
        tip_point[:, 0] += curl * length * 0.4 * t ** 2
        points[:, k] = tip_point
    radius = np.linspace(0.6, 0.14, POINTS)
    thickness = rng.uniform(0.75, 1.25, STRANDS)
    return points, radius[None, :] * thickness[:, None], (x - 0.0) / WIDTH


def build_hair(points, radii, root_u):
    curves = bpy.data.hair_curves.new("Pelz")
    curves.add_curves([POINTS] * STRANDS)
    curves.position_data.foreach_set("vector", points.reshape(-1).astype(np.float32))
    if "radius" not in curves.attributes:
        curves.attributes.new("radius", "FLOAT", "POINT")
    curves.attributes["radius"].data.foreach_set("value", radii.reshape(-1).astype(np.float32))
    rootx = curves.attributes.new("rootx", "FLOAT", "CURVE")
    rootx.data.foreach_set("value", root_u.astype(np.float32))
    shade = curves.attributes.new("shade", "FLOAT", "CURVE")
    shade.data.foreach_set("value", rng.uniform(0.0, 1.0, STRANDS).astype(np.float32))
    obj = bpy.data.objects.new("Pelz", curves)
    bpy.context.collection.objects.link(obj)
    return obj


def build_seeds():
    seeds = []
    places = np.linspace(PAD_FROM + 30.0, PAD_TO - 30.0, 9) + rng.uniform(-12.0, 12.0, 9)
    for index, sx in enumerate(places):
        bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=14, radius=1.0)
        seed = bpy.context.object
        seed.name = "Samen_{}".format(index)
        size = rng.uniform(1.8, 2.5)
        seed.scale = (size * 0.8, size * 0.7, size)
        seed.location = (sx, rng.uniform(-8.0, -6.0), BASE_Z + rng.uniform(1.0, 4.0))
        seed.rotation_euler = (rng.uniform(-0.4, 0.4), rng.uniform(-0.6, 0.6), rng.uniform(0, 6.28))
        bpy.ops.object.shade_smooth()
        seed["rootx"] = float(sx / WIDTH)
        seeds.append(seed)
        # Some have woken: a little green germ breaks out of the crack
        if index % 3 == 1:
            bpy.ops.mesh.primitive_cone_add(vertices=10, radius1=0.7, radius2=0.1, depth=3.2)
            germ = bpy.context.object
            germ.name = "Keim_{}".format(index)
            germ.location = seed.location + Vector((0.4, -0.6, size * 0.95))
            germ.rotation_euler = (math.radians(-25.0), math.radians(rng.uniform(-30, 30)), 0.0)
            bpy.ops.object.shade_smooth()
            germ["rootx"] = float(sx / WIDTH)
            germ["germ"] = 1.0
            seeds.append(germ)
    return seeds


# ---------------------------------------------------------------- looks

def fur_material():
    mat = bpy.data.materials.new("Pelz")
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    info = nodes.new("ShaderNodeHairInfo")
    shade = nodes.new("ShaderNodeAttribute")
    shade.attribute_name = "shade"

    # Dark, almost black green at the roots, deep juicy green in the middle, fresh sap green at the tips
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.0
    ramp.color_ramp.elements[0].color = (0.002, 0.008, 0.0015, 1.0)
    middle = ramp.color_ramp.elements.new(0.45)
    middle.color = (0.008, 0.042, 0.005, 1.0)
    ramp.color_ramp.elements[1].position = 1.0
    ramp.color_ramp.elements[1].color = (0.03, 0.2, 0.008, 1.0)
    links.new(info.outputs["Intercept"], ramp.inputs["Fac"])

    # Each strand a little different: some more yellow, some more blue-green
    hue = nodes.new("ShaderNodeHueSaturation")
    links.new(ramp.outputs["Color"], hue.inputs["Color"])
    to_hue = nodes.new("ShaderNodeMapRange")
    to_hue.inputs["To Min"].default_value = 0.47
    to_hue.inputs["To Max"].default_value = 0.54
    links.new(shade.outputs["Fac"], to_hue.inputs["Value"])
    links.new(to_hue.outputs["Result"], hue.inputs["Hue"])
    hue.inputs["Saturation"].default_value = 1.3
    to_value = nodes.new("ShaderNodeMapRange")
    to_value.inputs["To Min"].default_value = 0.75
    to_value.inputs["To Max"].default_value = 1.25
    links.new(shade.outputs["Fac"], to_value.inputs["Value"])
    links.new(to_value.outputs["Result"], hue.inputs["Value"])

    # Soft and velvety like moss fur, no white hair gloss: a matt surface with a green sheen on the edges
    hair = nodes.new("ShaderNodeBsdfPrincipled")
    links.new(hue.outputs["Color"], hair.inputs["Base Color"])
    hair.inputs["Roughness"].default_value = 0.6
    hair.inputs["Specular IOR Level"].default_value = 0.25
    hair.inputs["Sheen Weight"].default_value = 0.8
    hair.inputs["Sheen Roughness"].default_value = 0.4
    hair.inputs["Sheen Tint"].default_value = (0.35, 0.8, 0.25, 1.0)
    links.new(hair.outputs[0], out.inputs["Surface"])
    return mat


def seed_material():
    mat = bpy.data.materials.new("Samen")
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes["Principled BSDF"]
    # Dark brown, cracked, a dull sheen; the germs fresh green
    noise = nodes.new("ShaderNodeTexVoronoi")
    noise.feature = "DISTANCE_TO_EDGE"
    noise.inputs["Scale"].default_value = 1.6
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.6
    links.new(noise.outputs["Distance"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    germ = nodes.new("ShaderNodeAttribute")
    germ.attribute_type = "OBJECT"
    germ.attribute_name = "germ"
    mix = nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.inputs[6].default_value = (0.04, 0.022, 0.011, 1.0)
    mix.inputs[7].default_value = (0.12, 0.42, 0.04, 1.0)
    links.new(germ.outputs["Fac"], mix.inputs[0])
    links.new(mix.outputs[2], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.45
    return mat


def data_material(is_hair):
    """Linear data: R place along the strand, G its place along the bar, B a random number"""
    mat = bpy.data.materials.new("Daten" + ("Haar" if is_hair else "Samen"))
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    emission = nodes.new("ShaderNodeEmission")
    combine = nodes.new("ShaderNodeCombineColor")
    rootx = nodes.new("ShaderNodeAttribute")
    rootx.attribute_name = "rootx"
    if is_hair:
        info = nodes.new("ShaderNodeHairInfo")
        shade = nodes.new("ShaderNodeAttribute")
        shade.attribute_name = "shade"
        links.new(info.outputs["Intercept"], combine.inputs[0])
        links.new(shade.outputs["Fac"], combine.inputs[2])
    else:
        rootx.attribute_type = "OBJECT"
        # Seeds sink back with the middle of the strands around them
        combine.inputs[0].default_value = 0.4
        combine.inputs[2].default_value = 0.5
    links.new(rootx.outputs["Fac"], combine.inputs[1])
    links.new(combine.outputs[0], emission.inputs["Color"])
    emission.inputs["Strength"].default_value = 1.0
    links.new(emission.outputs[0], out.inputs["Surface"])
    return mat


def assign(obj, mat):
    obj.data.materials.clear()
    obj.data.materials.append(mat)


def main():
    out_dir = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else os.getcwd()
    os.makedirs(out_dir, exist_ok=True)
    clear()
    scene = setup(samples=96)
    lights()
    points, radii, root_u = make_strands()
    hair = build_hair(points, radii, root_u)
    seeds = build_seeds()

    # The lit picture
    assign(hair, fur_material())
    seed_look = seed_material()
    for seed in seeds:
        assign(seed, seed_look)
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.exposure = -1.45
    scene.render.filepath = os.path.join(out_dir, "T_HUD_Keimpelz_Pelz.png")
    bpy.ops.render.render(write_still=True)
    print("[Vael] gerendert:", scene.render.filepath)

    # The data, without any light or tone mapping, hardly blended between strands
    assign(hair, data_material(True))
    seed_data = data_material(False)
    for seed in seeds:
        assign(seed, seed_data)
    for name in ("Key", "Fill", "Rim"):
        bpy.data.objects[name].hide_render = True
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.0
    try:
        scene.view_settings.view_transform = "Raw"
    except TypeError:
        scene.display_settings.display_device = "None"
    scene.view_settings.exposure = 0.0
    scene.cycles.samples = 16
    scene.cycles.max_bounces = 0
    scene.cycles.pixel_filter_type = "BOX"
    scene.cycles.filter_width = 1.0
    scene.render.filepath = os.path.join(out_dir, "T_HUD_Keimpelz_Daten.png")
    bpy.ops.render.render(write_still=True)
    print("[Vael] gerendert:", scene.render.filepath, scene.view_settings.view_transform)


main()
