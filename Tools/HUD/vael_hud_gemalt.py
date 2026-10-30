"""
Vael – gemalte HUD-Teile (Blender, ohne Oberfläche)
===================================================

Baut die Teile des Spieler-HUDs für Magier und Floromant als echte 3D-Körper und rendert sie freigestellt (Cycles,
transparenter Hintergrund): Gefäße aus Glas, gehalten von der Klaue der Urdrachin, Rahmen, Fähigkeitsfelder und Medaillons.
Eigene Motive, keine Fremdgrafiken. Die Gefäße liegen genau auf den Masken von vael_hud_formen.py: Die Flüssigkeit malt das
Spiel dahinter selbst (Füllstand, Wabern), das gerenderte Glas liegt darüber.

Aufruf:
  blender -b --factory-startup -P vael_hud_gemalt.py -- <Zielordner> [Teil ...]
Teile: leben, magier_gefaess, floromant_gefaess, magier_rahmen, floromant_rahmen, magier_feld, floromant_feld,
       magier_medaillon, floromant_medaillon (ohne Angabe: alle)
"""
import math
import os
import sys

import addon_utils
import bmesh
import bpy
import numpy as np
from mathutils import Vector

FRAME_ASPECT = 880.0 / 202.0

# Vessels are rendered a little larger than their mask, so the claw has room below and beside the glass;
# the HUD draws the picture this much larger than the vessel, around the same middle
VESSEL_VIEW = 1.16


# ---------------------------------------------------------------- Szene

def reset():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for block in (bpy.data.meshes, bpy.data.curves, bpy.data.materials, bpy.data.lights, bpy.data.cameras, bpy.data.images):
        for item in list(block):
            block.remove(item)


def setup(width_px, height_px, view_width, samples=64):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = samples
    scene.cycles.use_denoising = True
    scene.render.film_transparent = True
    scene.cycles.film_transparent_glass = True
    scene.render.resolution_x = width_px
    scene.render.resolution_y = height_px
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    try:
        scene.view_settings.view_transform = "Standard"
    except Exception:
        pass
    # Flat dark parts must stay dark: a lower exposure than a lit product shot
    scene.view_settings.exposure = -1.3

    # Ortho camera in front, looking along +Y at the XZ plane: X right, Z up, like the masks
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = view_width
    cam = bpy.data.objects.new("Cam", cam_data)
    cam.location = (0.0, -6.0, 0.0)
    cam.rotation_euler = (math.radians(90.0), 0.0, 0.0)
    bpy.context.collection.objects.link(cam)
    scene.camera = cam

    # Warm key light from the upper left front, cool fill from the right, rim from behind
    def light(name, kind, location, energy, size, color):
        data = bpy.data.lights.new(name, kind)
        data.energy = energy
        data.color = color
        if kind == "AREA":
            data.size = size
            data.shape = "DISK"
        obj = bpy.data.objects.new(name, data)
        obj.location = location
        direction = Vector((0.0, 0.0, 0.0)) - Vector(location)
        obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
        bpy.context.collection.objects.link(obj)

    scale = view_width
    light("Key", "AREA", (-2.0 * scale, -3.0 * scale, 2.5 * scale), 700.0 * scale * scale, 0.6 * scale, (1.0, 0.9, 0.78))
    light("Fill", "AREA", (2.5 * scale, -2.5 * scale, -0.5 * scale), 160.0 * scale * scale, 1.2 * scale, (0.75, 0.85, 1.0))
    light("Rim", "AREA", (0.0, 2.0 * scale, 2.5 * scale), 350.0 * scale * scale, 0.8 * scale, (1.0, 0.95, 0.9))

    world = bpy.data.worlds.new("World") if not bpy.data.worlds else bpy.data.worlds[0]
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.03, 0.025, 0.022, 1.0)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.25


def render(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    print("[Vael] gerendert:", path)


# ---------------------------------------------------------------- Materialien

def material(name, color, metallic=0.0, roughness=0.5, bump=0.0, bump_scale=60.0, emission=None, glass=False, coat=0.0, tag=None):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    bsdf = nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Coat Weight"].default_value = coat
    if glass:
        bsdf.inputs["Transmission Weight"].default_value = 1.0
        bsdf.inputs["IOR"].default_value = 1.45
    if emission is not None:
        bsdf.inputs["Emission Color"].default_value = (*emission[0], 1.0)
        bsdf.inputs["Emission Strength"].default_value = emission[1]
    if bump > 0.0:
        noise = nodes.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = bump_scale
        noise.inputs["Detail"].default_value = 8.0
        bump_node = nodes.new("ShaderNodeBump")
        bump_node.inputs["Strength"].default_value = bump
        bump_node.inputs["Distance"].default_value = 0.02
        links.new(noise.outputs["Fac"], bump_node.inputs["Height"])
        links.new(bump_node.outputs["Normal"], bsdf.inputs["Normal"])
        # A little colour variation from the same noise, so surfaces don't look plastic
        mix = nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.inputs["A"].default_value = (*[c * 0.7 for c in color], 1.0)
        mix.inputs["B"].default_value = (*[min(1.0, c * 1.25) for c in color], 1.0)
        links.new(noise.outputs["Fac"], mix.inputs["Factor"])
        links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
    mat["tag"] = tag or ""
    return mat


def give(obj, mat):
    obj.data.materials.clear()
    obj.data.materials.append(mat)
    obj["tag"] = mat.get("tag", "")
    return obj


def gem_material():
    """A cut stone: every facet has its own shade from the colour attribute "Facet", so it sparkles even when small"""
    mat = bpy.data.materials.new("Edelstein")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    bsdf = nodes["Principled BSDF"]
    attribute = nodes.new("ShaderNodeAttribute")
    attribute.attribute_type = "GEOMETRY"
    attribute.attribute_name = "Facet"
    links.new(attribute.outputs["Color"], bsdf.inputs["Base Color"])
    links.new(attribute.outputs["Color"], bsdf.inputs["Emission Color"])
    bsdf.inputs["Emission Strength"].default_value = 0.85
    bsdf.inputs["Roughness"].default_value = 0.06
    bsdf.inputs["Coat Weight"].default_value = 1.0
    mat["tag"] = "gem"
    return mat


# Materials shared by the pieces
def common():
    return {
        "bronze": material("Bronze", (0.14, 0.09, 0.06), metallic=1.0, roughness=0.38, bump=0.25, bump_scale=90.0),
        "claw": material("Klaue", (0.055, 0.04, 0.032), metallic=0.9, roughness=0.36, bump=0.7, bump_scale=140.0, coat=0.5),
        "talon": material("Krallenspitze", (0.02, 0.018, 0.02), metallic=0.3, roughness=0.25, coat=0.8),
        "glass": material("Glas", (0.97, 0.98, 1.0), roughness=0.02, glass=True),
        "silver": material("Silber", (0.75, 0.76, 0.8), metallic=1.0, roughness=0.22, bump=0.08, bump_scale=200.0),
        "darksilver": material("DunkelSilber", (0.35, 0.36, 0.42), metallic=1.0, roughness=0.3),
        "cork": material("Kork", (0.16, 0.09, 0.045), roughness=0.85, bump=0.8, bump_scale=160.0),
        "wood": material("Holz", (0.16, 0.1, 0.06), roughness=0.75, bump=1.0, bump_scale=35.0),
        "darkwood": material("DunkelHolz", (0.26, 0.16, 0.08), roughness=0.6, bump=1.0, bump_scale=45.0, coat=0.25),
        "leaf": material("Blatt", (0.07, 0.24, 0.08), roughness=0.5, bump=0.3, bump_scale=80.0, coat=0.2),
        "turquoise": material("Tuerkis", (0.1, 0.42, 0.4), metallic=0.4, roughness=0.28, coat=0.7, tag="gem"),
        "blossom": material("Bluete", (0.85, 0.36, 0.56), roughness=0.45, emission=((0.9, 0.35, 0.6), 0.6), tag="gem"),
        "bud": material("Knospe", (0.2, 0.75, 0.68), roughness=0.3, emission=((0.25, 0.9, 0.8), 0.8), tag="gem"),
        "sapphire": material("Saphir", (0.08, 0.25, 0.8), roughness=0.05, emission=((0.15, 0.4, 1.0), 1.5), coat=1.0, tag="gem"),
        "wax": material("Siegelwachs", (0.1, 0.22, 0.55), roughness=0.35, bump=0.4, bump_scale=50.0, coat=0.5, tag="gem"),
        "leather": material("Leder", (0.012, 0.014, 0.024), roughness=0.7, bump=0.35, bump_scale=180.0, tag="panel"),
        "bark": material("Rinde", (0.018, 0.014, 0.01), roughness=0.85, bump=0.9, bump_scale=55.0, tag="panel"),
        "gem": gem_material(),
        "feather": material("Feder", (0.28, 0.31, 0.4), roughness=0.75),
        "slotfill": material("Feldgrund", (0.02, 0.018, 0.02), roughness=0.6, tag="panel"),
    }


# ---------------------------------------------------------------- Formen

def from_mask(x, y):
    """Point of the masks (0..1, y down) in the scene of a square piece"""
    return Vector((x - 0.5, 0.0, 0.5 - y))


def leaf(center, length, width, angle, material_, bend=0.15, curl=0.3, name="Blatt", vein=None):
    """A real leaf: pointed outline, bent along its length and curled across, with a midrib; angle turns it in the XZ plane"""
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    rows, cols = 14, 7
    grid = []
    ca, sa = math.cos(angle), math.sin(angle)
    for i in range(rows + 1):
        t = i / rows
        half = width * 0.5 * math.sin(math.pi * t) ** 0.8
        row = []
        for j in range(cols + 1):
            u = j / cols * 2 - 1
            lx, lz = t * length, u * half
            ly = -bend * length * t * t - curl * half * (1 - u * u)
            row.append(bm.verts.new((center[0] + lx * ca - lz * sa, center[1] + ly, center[2] + lx * sa + lz * ca)))
        grid.append(row)
    for i in range(rows):
        for j in range(cols):
            bm.faces.new((grid[i][j], grid[i][j + 1], grid[i + 1][j + 1], grid[i + 1][j]))
    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    for poly in mesh.polygons:
        poly.use_smooth = True
    give(obj, material_)
    solid = obj.modifiers.new("Dicke", "SOLIDIFY")
    solid.thickness = width * 0.04
    if vein is not None:
        pts = [(center[0] + length * t * ca, center[1] - bend * length * t * t - 0.004, center[2] + length * t * sa) for t in np.linspace(0.02, 0.9, 10)]
        tube(pts, [width * 0.03 * (1 - 0.7 * k / 9) for k in range(10)], vein, name="Mittelrippe")
    return obj


def revolve(profile, center, material_, segments=64, name="Dreh"):
    """Lathe: profile [(z, radius)] around the vertical axis through center (scene units)"""
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    rings = []
    for z, r in profile:
        ring = []
        for i in range(segments):
            a = 2.0 * math.pi * i / segments
            ring.append(bm.verts.new((center.x + r * math.cos(a), center.y + r * math.sin(a), z)))
        rings.append(ring)
    for ring_a, ring_b in zip(rings, rings[1:]):
        for i in range(segments):
            j = (i + 1) % segments
            bm.faces.new((ring_a[i], ring_a[j], ring_b[j], ring_b[i]))
    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    for poly in mesh.polygons:
        poly.use_smooth = True
    give(obj, material_)
    mod = obj.modifiers.new("Solid", "SOLIDIFY")
    mod.thickness = 0.008
    return obj


def tube(points, radii, material_, name="Rohr", resolution=6, cyclic=False):
    """A tube along points, each with its own radius: talons, vines, filigree"""
    curve = bpy.data.curves.new(name, "CURVE")
    curve.dimensions = "3D"
    curve.bevel_depth = 1.0
    curve.bevel_resolution = resolution
    curve.use_fill_caps = True
    spline = curve.splines.new("POLY")
    spline.points.add(len(points) - 1)
    for point, p, r in zip(spline.points, points, radii):
        point.co = (p[0], p[1], p[2], 1.0)
        point.radius = r
    spline.use_cyclic_u = cyclic
    obj = bpy.data.objects.new(name, curve)
    bpy.context.collection.objects.link(obj)
    give(obj, material_)
    return obj


def bezier(p0, p1, p2, n=24):
    return [tuple((1 - t) ** 2 * a + 2 * (1 - t) * t * b + t * t * c for a, b, c in zip(p0, p1, p2)) for t in np.linspace(0, 1, n)]


def sphere(location, radius, material_, scale=(1, 1, 1), name="Kugel", segments=32):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=segments // 2, radius=radius, location=location)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.shade_smooth()
    return give(obj, material_)


def torus(location, major, minor, material_, rotation=(math.radians(90), 0, 0), name="Ring"):
    bpy.ops.mesh.primitive_torus_add(major_radius=major, minor_radius=minor, location=location, rotation=rotation, major_segments=64, minor_segments=16)
    obj = bpy.context.active_object
    obj.name = name
    bpy.ops.object.shade_smooth()
    return give(obj, material_)


def cylinder(location, radius, depth, material_, rotation=(0, 0, 0), name="Zylinder", vertices=48):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=location, rotation=rotation)
    obj = bpy.context.active_object
    obj.name = name
    bpy.ops.object.shade_smooth()
    bev = obj.modifiers.new("Bevel", "BEVEL")
    bev.width = min(radius, depth) * 0.2
    bev.segments = 3
    return give(obj, material_)


def rounded_plate(width, height, radius, depth, material_, inset=0.0, y=0.0, name="Platte"):
    """A flat plate in the XZ plane with rounded corners and bevelled edges"""
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0.0, y, 0.0))
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = (width - 2 * inset, depth, height - 2 * inset)
    bpy.ops.object.transform_apply(scale=True)
    bev = obj.modifiers.new("Rund", "BEVEL")
    bev.affect = "EDGES"
    bev.width = radius
    bev.segments = 8
    bev.limit_method = "ANGLE"
    return give(obj, material_)


def rounded_rect_path(width, height, radius, y, n_corner=10):
    """Points around a rounded rectangle in the XZ plane, closed"""
    hw, hh = width / 2 - radius, height / 2 - radius
    points = []
    for cx, cz, a0 in ((hw, hh, 0.0), (-hw, hh, 90.0), (-hw, -hh, 180.0), (hw, -hh, 270.0)):
        for k in range(n_corner + 1):
            a = math.radians(a0 + 90.0 * k / n_corner)
            points.append((cx + radius * math.cos(a), y, cz + radius * math.sin(a)))
    return points


def claw(m, cx, cy, rx, ry, wrap_vines=False):
    """The claw of the Urdrachin holding a vessel from below: a scaled palm under it, two long talons that follow the curve
    of the glass up its sides and curl their dark tips in over the front, two short talons gripping the bottom in front.
    The vessel's outline as an ellipse in mask coordinates: middle cx, cy and radii rx, ry."""
    def around(theta, grow, forward):
        x = cx + (rx + grow) * math.cos(theta)
        y = cy + (ry + grow) * math.sin(theta)
        return from_mask(x, y) + Vector((0.0, -forward, 0.0))

    palm = sphere(tuple(from_mask(cx, cy + ry + 0.035) + Vector((0, 0.02, 0))), 1.0, m["claw"], scale=(rx * 1.05, 0.12, 0.075), name="Ballen")
    for side in (1, -1):
        # Long talon: from the palm up the side, over the widest point, the tip bends in and forward over the glass
        thetas = np.linspace(math.radians(90 - side * 25), math.radians(90 - side * 158), 22)
        pts, radii = [], []
        for i, theta in enumerate(thetas):
            t = i / (len(thetas) - 1)
            grow = 0.03 - 0.07 * max(0.0, t - 0.7) / 0.3
            pts.append(tuple(around(theta, grow, 0.06 + 0.14 * t)))
            radii.append(0.046 * (1.0 - 0.88 * t ** 1.2))
        tube(pts[:-4], radii[:-4], m["claw"], name="Kralle")
        tube(pts[-5:], radii[-5:], m["talon"], name="Krallenspitze")
        for k in (5, 11):
            sphere(pts[k], radii[k] * 1.25, m["claw"], scale=(1.0, 0.85, 0.75), name="Knoechel", segments=16)
        # Short talon gripping the bottom in front
        thetas = np.linspace(math.radians(90 - side * 10), math.radians(90 - side * 60), 12)
        pts = [tuple(around(theta, 0.01, 0.16 + 0.06 * (i / 11))) for i, theta in enumerate(thetas)]
        radii = [0.032 * (1.0 - 0.8 * (i / 11) ** 1.3) for i in range(12)]
        tube(pts[:-3], radii[:-3], m["claw"], name="Kralle")
        tube(pts[-4:], radii[-4:], m["talon"], name="Krallenspitze")
    if wrap_vines:
        # A young vine has wound round the claw of the Floromant
        pts = [tuple(around(theta, 0.035, 0.12 + 0.03 * math.sin(theta * 5))) for theta in np.linspace(math.radians(-15), math.radians(195), 40)]
        tube(pts, [0.011] * len(pts), m["leaf"], name="Ranke")
        for theta in (0.2, 1.1, 2.0, 2.9):
            leaf = sphere(tuple(around(theta, 0.06, 0.16)), 0.028, m["leaf"], scale=(1.6, 0.35, 0.8), name="Blatt", segments=12)
            leaf.rotation_euler = (0, -theta, 0)
    return palm


# ---------------------------------------------------------------- Gefäße (auf den Masken)

GEM_RANDOM = np.random.default_rng(11)

# Shades of the facets: the depth of the stone, its body, its light, and a rare flash
GEM_SHADES = ((0.004, 0.05, 0.22), (0.01, 0.16, 0.5), (0.03, 0.42, 0.82), (0.25, 0.8, 1.0), (0.9, 1.0, 1.0))


def facet_stone(m, x, y, z, radius, squash=1.0):
    """A cut stone lying on the surface, seen from above: a ring of crown facets around a flat table. The stone rises towards -y."""
    sides = 10
    bm = bmesh.new()

    def ring(r, height, turn):
        return [bm.verts.new((x + r * math.cos((k + turn) / sides * math.tau), y - height * radius, z + r * squash * math.sin((k + turn) / sides * math.tau))) for k in range(sides)]

    girdle = ring(radius, 0.0, 0.0)
    crown = ring(radius * 0.78, 0.34, 0.5)
    table = ring(radius * 0.46, 0.55, 0.0)
    middle = bm.verts.new((x, y - 0.6 * radius, z))
    faces = []
    for k in range(sides):
        n = (k + 1) % sides
        faces.append((bm.faces.new((girdle[k], girdle[n], crown[k])), None))
        faces.append((bm.faces.new((crown[k], girdle[n], crown[n])), None))
        faces.append((bm.faces.new((crown[k], crown[n], table[n])), None))
        faces.append((bm.faces.new((crown[k], table[n], table[k])), None))
    for k in range(sides):
        faces.append((bm.faces.new((table[k], table[(k + 1) % sides], middle)), 2))
    bmesh.ops.recalc_face_normals(bm, faces=[f for f, _ in faces])
    layer = bm.loops.layers.float_color.new("Facet")
    for index, (face, fixed) in enumerate(faces):
        # Light falls from the upper left: facets on that side are brighter, the opposite ones sink into the depth
        center = face.calc_center_median()
        light = 0.5 + 0.5 * ((center.z - z) - (center.x - x)) / (radius * 1.4)
        step = fixed if fixed is not None else light * 3.0 + GEM_RANDOM.normal(0.0, 0.7)
        shade = GEM_SHADES[int(np.clip(step, 0, 3.4))]
        if fixed is None and GEM_RANDOM.random() < 0.07:
            shade = GEM_SHADES[4]
        for loop in face.loops:
            loop[layer] = (*shade, 1.0)
    mesh = bpy.data.meshes.new("Stein")
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new("Stein", mesh)
    bpy.context.collection.objects.link(obj)
    return give(obj, m["gem"])


def wax_seal_cap(mat):
    """Wax that ran down over the cork and the neck and set: one flowing body from metaballs, thin runs ending in heavy drops"""
    rng = np.random.default_rng(4)
    meta = bpy.data.metaballs.new("Wachs")
    meta.resolution = 0.008
    meta.render_resolution = 0.0045
    meta.threshold = 0.6

    def ball(point, radius):
        element = meta.elements.new()
        element.type = "BALL"
        element.co = tuple(point)
        element.radius = radius

    # The neck was dipped: a closed sleeve from the top of the cork down to the rim, slightly domed on top
    for y_mask, spread, size in ((0.075, 0.03, 0.06), (0.1, 0.055, 0.05), (0.125, 0.066, 0.042), (0.15, 0.075, 0.04)):
        for a in np.linspace(0, math.tau, 14, endpoint=False):
            ball(from_mask(0.5, y_mask) + Vector((spread * math.cos(a), spread * math.sin(a), 0.0)), size)
    # A thicker collar where the wax pooled on the rim of the bottle
    collar = from_mask(0.5, 0.172)
    for a in np.linspace(0, math.tau, 26, endpoint=False):
        ball(collar + Vector((0.098 * math.cos(a), 0.098 * math.sin(a), 0.005 * math.sin(a * 5.0))), 0.031)

    # Runs: where they leave the collar, how far they reach and how much they meander
    for dx, end_y, wander in ((-0.085, 0.31, 0.012), (-0.04, 0.245, 0.009), (0.012, 0.35, 0.014), (0.058, 0.26, 0.009), (0.094, 0.215, 0.005)):
        start_y = 0.18
        phase = rng.random() * math.tau
        for t in np.linspace(0.0, 1.0, 44):
            y_mask = start_y + (end_y - start_y) * t
            bulge = 0.12 if y_mask < 0.235 else 0.098 + 0.012 * math.sin(t * 7.0)
            x = dx + wander * math.sin(t * 8.0 + phase) * t
            forward = math.sqrt(max(bulge ** 2 - x ** 2, 0.0003))
            point = from_mask(0.5 + x, y_mask) + Vector((0.0, -forward, 0.0))
            # Thin along the way with beads where it paused, heavy at the end
            radius = 0.0125 + 0.004 * max(0.0, math.sin(t * 13.0 + phase)) + 0.024 * max(0.0, (t - 0.84) / 0.16) ** 1.4
            ball(point, radius)

    obj = bpy.data.objects.new("Wachs", meta)
    bpy.context.collection.objects.link(obj)
    return give(obj, mat)


def spiral(x, y, z, radius, turns, start, handed, n=26):
    """Points of a small curl that winds inwards, for filigree"""
    points = []
    for t in np.linspace(0.0, 1.0, n):
        a = start + handed * t * turns * math.tau
        r = radius * (1.0 - 0.82 * t)
        points.append((x + r * math.cos(a), y, z + r * math.sin(a)))
    return points


def bead_ring(m, center, radius, count, size, material_, tilt=False):
    """Small silver beads round a ring seen from the front: a pearled edge"""
    for a in np.linspace(0, math.tau, count, endpoint=False):
        sphere((center.x + radius * math.cos(a), center.y - radius * math.sin(a) * 0.3, center.z - abs(math.sin(a)) * 0.002 + (radius * math.sin(a) * 0.12 if tilt else 0)), size, material_, name="Perle", segments=10)


def cord(m, center, radius, turns, material_):
    """A cord wound round a neck, slightly descending"""
    points = []
    for t in np.linspace(0, turns * math.tau, int(turns * 30)):
        points.append((center.x + radius * math.cos(t), center.y - radius * math.sin(t), center.z - 0.006 * t))
    tube(points, [0.0085] * len(points), material_, name="Schnur", resolution=3)


def phial(m):
    c = from_mask(0.5, 0.62)
    # Round body and neck from one profile, the mask's numbers: body r 0.3 at y 0.62, neck x 0.42..0.58 from y 0.2 to 0.4
    profile = []
    for a in np.linspace(-math.pi / 2, math.pi / 2 - 0.3, 40):
        profile.append((c.z + 0.3 * math.sin(a), 0.3 * math.cos(a)))
    profile += [(from_mask(0, 0.38).z, 0.08), (from_mask(0, 0.22).z, 0.08)]
    revolve(profile, Vector((0, 0, 0)), m["glass"], name="Phiole")

    lip = from_mask(0.5, 0.185)
    torus(tuple(lip), 0.097, 0.024, m["silver"], rotation=(0, 0, 0), name="Lippe")
    bead_ring(m, lip + Vector((0, 0, -0.03)), 0.094, 22, 0.0095, m["silver"])
    # Cork under a cap of dark red wax that runs down in drops
    cylinder(tuple(from_mask(0.5, 0.135)), 0.07, 0.09, m["cork"], name="Korken")
    wax_seal_cap(material("Wachskappe", (0.15, 0.004, 0.008), roughness=0.14, coat=1.0))

    # Neck: silver ring, a cord, a small tag
    ring = from_mask(0.5, 0.39)
    torus(tuple(ring), 0.088, 0.015, m["silver"], rotation=(0, 0, 0), name="Halsring")
    twine = material("Hanf", (0.3, 0.2, 0.1), roughness=0.9, bump=0.5, bump_scale=300.0)
    cord(m, from_mask(0.5, 0.28), 0.092, 3.0, twine)

    # Silver filigree over the shoulder of the glass, not over the middle where the number stands
    for side in (1, -1):
        pts = []
        for t in np.linspace(0.0, 1.0, 26):
            a = math.radians(150 - 75 * t)
            pts.append((c.x + side * 0.3 * math.cos(a) * -1.0 * 1.02, -0.16 - 0.1 * t, c.z + 0.3 * math.sin(a)))
        tube(pts, [0.0075 * (1.0 - 0.5 * t) for t in np.linspace(0, 1, 26)], m["silver"], name="Filigran")
        px_, y_, pz = pts[8]
        tube(spiral(px_ - side * 0.02, y_ - 0.01, pz + 0.03, 0.035, 1.3, 0.5, side), [0.0055] * 26, m["silver"], name="Schnecke")
        px_, y_, pz = pts[-1]
        sphere((px_, y_, pz), 0.0135, m["silver"], name="Perle", segments=10)


def quill(m, a, control, b):
    """A big feather: curved ivory shaft, a closed vane of single barbs, pale at the root, deep blue towards the tip, split in places"""
    rng = np.random.default_rng(3)
    shaft = material("Kiel", (0.75, 0.68, 0.52), roughness=0.4, coat=0.3)
    shades = (material("Daune", (0.72, 0.74, 0.78), roughness=0.85),
              material("Fahne hell", (0.4, 0.5, 0.68), roughness=0.7),
              material("Fahne mittel", (0.1, 0.2, 0.42), roughness=0.6),
              material("Fahne dunkel", (0.018, 0.04, 0.12), roughness=0.55))
    tip_edge = material("Fahnenrand", (0.75, 0.8, 0.9), roughness=0.6)
    line = [Vector(p) for p in bezier(a, control, b, 90)]
    tube([tuple(p) for p in line], [0.0115 * (1.0 - 0.78 * i / 89) for i in range(90)], shaft, name="Kiel")

    splits = {(side, i) for side in (1, -1) for i in rng.integers(24, 82, 7)}
    for index in range(16, 90):
        t = index / 89
        base = line[index]
        axis = (line[min(index + 1, 89)] - line[index - 1]).normalized()
        across = Vector((axis.z, 0.0, -axis.x))
        length = 0.135 * math.sin(math.pi * min((t - 0.12) / 0.9, 1.0)) ** 0.55 + 0.012
        shade = shades[min(int(max(t - 0.18, 0.0) / 0.82 * 4), 3)]
        for side in (1, -1):
            # A wider side and a narrower side, as on a real flight feather
            reach = length * (1.0 if side > 0 else 0.62) * (0.92 + 0.14 * rng.random())
            gap = any((side, i) in splits for i in (index, index + 1))
            if gap:
                reach *= 0.55
            tip = base + (across * side * 0.86 + axis * 0.5).normalized() * reach
            middle = base + (tip - base) * 0.55 + axis * 0.008
            tube([tuple(base), tuple(middle), tuple(tip)], [0.0046, 0.0042, 0.0022], shade, name="Ast", resolution=1)
            if not gap and index > 60 and index % 3 == 0:
                edge = tip + (tip - base).normalized() * 0.006
                tube([tuple(tip), tuple(edge)], [0.0022, 0.001], tip_edge, name="Astspitze", resolution=1)
    # Down at the root
    for index in range(8, 22):
        base = line[index]
        for side in (1, -1):
            angle = rng.random() * 0.8 * side
            tip = base + Vector((side * (0.04 + 0.03 * rng.random()), 0.0, 0.015 + angle * 0.02))
            tube([tuple(base), tuple(tip)], [0.0028, 0.0008], shades[0], name="Flaum", resolution=1)


def inkwell(m):
    # Frustum: bottom y 0.9 radius 0.32, top y 0.5 radius 0.22, neck r 0.12 from y 0.38 to 0.52; cut in facets like crystal
    z = lambda y: from_mask(0, y).z
    profile = [(z(0.9), 0.0), (z(0.9), 0.31), (z(0.88), 0.32), (z(0.52), 0.22), (z(0.5), 0.13), (z(0.4), 0.12)]
    revolve(profile, Vector((0, 0, 0)), m["glass"], segments=12, name="Tintenfass")

    lip = from_mask(0.5, 0.36)
    torus(tuple(lip), 0.15, 0.027, m["silver"], rotation=(0, 0, 0), name="Lippe")
    bead_ring(m, lip + Vector((0, 0, -0.034)), 0.138, 24, 0.0095, m["silver"])
    torus(tuple(from_mask(0.5, 0.895)), 0.32, 0.02, m["silver"], rotation=(0, 0, 0), name="Bodenring")
    bead_ring(m, from_mask(0.5, 0.865), 0.316, 34, 0.0095, m["silver"])

    # Silver shoulder band with a sapphire in front and ribs running down the edges of the crystal
    shoulder = from_mask(0.5, 0.5)
    torus(tuple(shoulder), 0.222, 0.013, m["silver"], rotation=(0, 0, 0), name="Schulter")
    facet_stone(m, 0.0, -0.228, from_mask(0.5, 0.5).z, 0.038)
    for a in (-75, -45, 45, 75):
        s = math.sin(math.radians(a))
        c = math.cos(math.radians(a))
        rib = [(s * r, -c * r, z(y)) for y, r in ((0.52, 0.228), (0.7, 0.277), (0.87, 0.325))]
        tube(rib, [0.0075, 0.0065, 0.0075], m["silver"], name="Rippe")

    quill(m, (0.02, -0.05, from_mask(0, 0.5).z), (0.08, -0.05, from_mask(0, 0.2).z), (0.42, -0.06, from_mask(0, 0.03).z))


def pitcher(m):
    # Pitcher plant: bulb rx 0.23 at y 0.64, narrowing to the neck x 0.38..0.62 up to y 0.3
    z = lambda y: from_mask(0, y).z
    c = from_mask(0.5, 0.64)
    profile = []
    for t in np.linspace(-math.pi / 2, math.pi / 2, 30):
        profile.append((c.z + 0.27 * math.sin(t), 0.23 * math.cos(t) + 0.0001))
    profile = [p for p in profile if p[0] < z(0.47)]
    profile += [(z(0.45), 0.13), (z(0.36), 0.12), (z(0.31), 0.15)]
    revolve(profile, Vector((0, 0, 0)), m["glass"], name="Kannenkelch")
    torus(tuple(from_mask(0.5, 0.31)), 0.155, 0.02, m["turquoise"], rotation=(0, 0, 0), name="Rand")
    # Leaf lid above the mouth, tipped open to the upper right
    leaf(tuple(from_mask(0.47, 0.28) + Vector((0, -0.03, 0))), 0.36, 0.2, math.radians(38), m["leaf"], bend=0.12, curl=0.35, name="Deckel", vein=m["turquoise"])
    # Small stem curling below
    tube([tuple(from_mask(x, y) + Vector((0, -0.05, 0))) for x, y in bezier((0.5, 0.9), (0.46, 0.97), (0.34, 0.98), 10)], [0.014] * 10, m["leaf"], name="Stiel")


# ---------------------------------------------------------------- Rahmen

def worn(mat, scale=4.5, darkest=0.5):
    """Handled leather: large soft patches darken the colour, as on a book carried for years"""
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    bsdf = nodes["Principled BSDF"]
    noise = nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = scale
    noise.inputs["Detail"].default_value = 4.0
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.35
    ramp.color_ramp.elements[0].color = (darkest, darkest, darkest, 1.0)
    ramp.color_ramp.elements[1].position = 0.68
    links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
    mix = nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.blend_type = "MULTIPLY"
    mix.inputs["Factor"].default_value = 1.0
    links.new(bsdf.inputs["Base Color"].links[0].from_socket, mix.inputs["A"])
    links.new(ramp.outputs["Color"], mix.inputs["B"])
    links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
    return mat


def px(x, y):
    """Point of the frame picture in the design pixels of the HUD panel (x from its left, y from its top) in the scene"""
    return (x - 440.0) / 202.0, 0.5 - (y - 28.0) / 202.0


def corner_fitting(m, sx, sz, w, h, lift):
    """Silver corner of a book: two arms along the edges with a curved inner line, rivets and a small sapphire"""
    size = 0.34
    points = [(0.0, 0.0), (size, 0.0)]
    for t in np.linspace(0.0, 1.0, 16):
        points.append((size * (1.0 - t) ** 2 + 0.18 * t * (1 - t), size * t ** 2 + 0.18 * t * (1 - t)))
    x0, z0 = sx * (w / 2 - 0.012), sz * (h / 2 - 0.012) + lift
    bm = bmesh.new()
    verts = [bm.verts.new((x0 - sx * a, -0.05, z0 - sz * b)) for a, b in points]
    face = bm.faces.new(verts)
    bmesh.ops.recalc_face_normals(bm, faces=[face])
    extruded = bmesh.ops.extrude_face_region(bm, geom=[face])
    bmesh.ops.translate(bm, vec=(0.0, 0.03, 0.0), verts=[g for g in extruded["geom"] if isinstance(g, bmesh.types.BMVert)])
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    mesh = bpy.data.meshes.new("Ecke")
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new("Ecke", mesh)
    bpy.context.collection.objects.link(obj)
    bev = obj.modifiers.new("Rund", "BEVEL")
    bev.width = 0.008
    bev.segments = 3
    bev.limit_method = "ANGLE"
    give(obj, m["fitting"])
    # Engraved line along the inner edge
    line = [(x0 - sx * a * 0.8, -0.057, z0 - sz * b * 0.8) for a, b in points[2:-1]]
    tube(line, [0.0035] * len(line), m["darksilver"], name="Gravur")
    facet_stone(m, x0 - sx * 0.075, -0.062, z0 - sz * 0.075, 0.034)
    for a, b in ((0.2, 0.03), (0.03, 0.2)):
        sphere((x0 - sx * a, -0.058, z0 - sz * b), 0.012, m["darksilver"], name="Niete", segments=12)


def stitches(m, path, y, length=0.034, gap=0.022):
    """A running stitch along a closed path of (x, z) points"""
    points = np.array(list(path) + [path[0]])
    lengths = np.linalg.norm(np.diff(points, axis=0), axis=1)
    along = np.concatenate(([0.0], np.cumsum(lengths)))
    start = 0.0
    while start + length < along[-1]:
        ax, az = (float(np.interp(start, along, points[:, k])) for k in range(2))
        bx, bz = (float(np.interp(start + length, along, points[:, k])) for k in range(2))
        tube([(ax, y, az), ((ax + bx) / 2, y - 0.004, (az + bz) / 2), (bx, y, bz)], [0.0048, 0.0058, 0.0048], m["thread"], name="Stich", resolution=2)
        start += length + gap


def element_seal(m, x, z, s=1.0):
    """The seal of the grimoire: a circle with the signs of the four elements around a sapphire"""
    y = -0.07
    line = 0.0058 * s
    cylinder((x, y + 0.012, z), 0.142 * s, 0.02, m["slotfill"], rotation=(math.radians(90), 0, 0), name="Siegelgrund", vertices=48)
    torus((x, y, z), 0.145 * s, 0.013 * s, m["silver"], name="Siegelring")
    torus((x, y + 0.005, z), 0.124 * s, 0.0035 * s, m["darksilver"], name="Siegelinnen")
    facet_stone(m, x, y - 0.006, z, 0.036 * s)
    r = 0.078 * s
    # Fire above: a tongue of two arcs
    flame = [(x + j * 0.024 * s * math.sin(t * math.pi) * (1 - 0.35 * t), y, z + r + (0.064 * t - 0.032) * s)
             for j, ts in ((1, np.linspace(0, 1, 12)), (-1, np.linspace(1, 0, 12))) for t in ts]
    tube(flame, [line] * len(flame), m["silver"], name="Feuer", cyclic=True)
    # Water right: a drop
    drop = [(x + r + 0.026 * s * math.sin(a) * (1 - 0.72 * max(0.0, math.cos(a))), y, z + (0.034 * math.cos(a) - 0.008) * s) for a in np.linspace(0, math.tau, 26)]
    tube(drop, [line] * len(drop), m["silver"], name="Wasser", cyclic=True)
    # Earth below: a block
    earth = [(x + 0.031 * s * cx, y, z - r + 0.028 * s * cz) for cx, cz in ((-1, -0.75), (1, -0.75), (0.7, 0.45), (0, 1), (-0.7, 0.45))]
    tube(earth, [line] * len(earth), m["silver"], name="Erde", cyclic=True)
    # Air left: three strokes
    for k, shift in enumerate((-0.022, 0.0, 0.022)):
        wind = [(x - r + (0.06 * t - 0.03) * s, y, z + (shift + 0.006 * math.sin(t * math.tau + k)) * s) for t in np.linspace(0, 1, 12)]
        tube(wind, [line] * len(wind), m["silver"], name="Luft")


def mage_materials(m):
    """The materials of the grimoire, shared by the frame and the menus"""
    m["cover"] = worn(material("Grimoireleder", (0.0012, 0.0075, 0.05), roughness=0.74, bump=0.4, bump_scale=240.0, tag="panel"))
    m["thread"] = material("Zwirn", (0.34, 0.28, 0.2), roughness=0.8)
    m["fitting"] = material("Beschlag", (0.24, 0.25, 0.29), metallic=0.7, roughness=0.36, bump=0.14, bump_scale=120.0)
    m["groove"] = material("Praegung", (0.0008, 0.003, 0.016), roughness=0.6)
    m["aqua"] = material("Meerstein", (0.03, 0.42, 0.62), roughness=0.08, emission=((0.1, 0.65, 0.95), 1.1), coat=1.0, tag="gem")
    m["sunken"] = material("Vertiefung", (0.0035, 0.009, 0.024), roughness=0.7, bump=0.2, bump_scale=200.0, tag="panel")


def plate_from_outline(path, y, depth, material_, name="Platte", bevel=0.01):
    """A flat plate with any outline of (x, z) points, bevelled at the edge"""
    bm = bmesh.new()
    face = bm.faces.new([bm.verts.new((x, y, z)) for x, z in path])
    bmesh.ops.recalc_face_normals(bm, faces=[face])
    extruded = bmesh.ops.extrude_face_region(bm, geom=[face])
    bmesh.ops.translate(bm, vec=(0.0, depth, 0.0), verts=[g for g in extruded["geom"] if isinstance(g, bmesh.types.BMVert)])
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    mod = obj.modifiers.new("Rund", "BEVEL")
    mod.width = bevel
    mod.segments = 3
    mod.limit_method = "ANGLE"
    return give(obj, material_)


def inset(path, distance):
    """The outline moved inwards by a distance: every point along the normal of its neighbours"""
    p = np.array(path)
    area = 0.5 * np.sum(p[:, 0] * np.roll(p[:, 1], -1) - np.roll(p[:, 0], -1) * p[:, 1])
    tangent = np.roll(p, -1, axis=0) - np.roll(p, 1, axis=0)
    tangent /= np.maximum(np.linalg.norm(tangent, axis=1, keepdims=True), 1e-6)
    normal = np.stack((-tangent[:, 1], tangent[:, 0]), axis=1) * (1.0 if area > 0 else -1.0)
    return [tuple(q) for q in p + normal * distance]


def mage_outline():
    """Silhouette of the frame in design pixels: round plates around the vessels, a body between them, an arch over the seal"""
    radius, cy, top, bottom = 100.0, 130.0, 48.0, 227.0
    points = []
    # Left plate from where the top edge meets it, round the outside to the bottom edge
    a0 = math.atan2(top - cy, math.sqrt(radius ** 2 - (top - cy) ** 2))
    a1 = math.atan2(bottom - cy, math.sqrt(radius ** 2 - (bottom - cy) ** 2)) - math.tau
    for a in np.linspace(a0, a1, 70):
        points.append((96.0 + radius * math.cos(a), cy + radius * math.sin(a)))
    # Bottom edge
    for x in np.linspace(points[-1][0] + 8.0, 880.0 - points[-1][0] - 8.0, 60):
        points.append((x, bottom))
    # Right plate, mirrored
    for a in np.linspace(math.pi - a1 - math.tau, math.pi - a0 - math.tau, 70):
        points.append((784.0 + radius * math.cos(a), cy + radius * math.sin(a)))
    # Top edge with the arch over the seal
    start = points[-1][0] - 8.0
    for x in np.linspace(start, 880.0 - start, 90):
        points.append((x, top - 19.0 * math.exp(-((x - 440.0) / 58.0) ** 2)))
    return points


def scroll(m, x_px, y_px, direction, up):
    """A silver tendril that grows out of the notch between plate and body along the edge and curls in"""
    pts, radii = [], []
    for t in np.linspace(0.0, 1.0, 40):
        reach = 62.0 * t
        curl = max(0.0, t - 0.55) / 0.45
        a = curl * 4.2
        x = x_px + direction * (reach * (1.0 - 0.45 * curl) + 9.0 * math.sin(a) * curl)
        y = y_px + up * (5.0 * math.sin(t * math.pi) + 9.0 * (1.0 - math.cos(a)) * curl)
        sx, sz = px(x, y)
        pts.append((sx, -0.052, sz))
        radii.append(0.0115 * (1.0 - 0.7 * t))
    tube(pts, radii, m["silver"], name="Ranke")
    sphere(pts[-1], 0.0075, m["silver"], name="Rankenende", segments=12)


def cabochon(m, x, z, size=1.0):
    """A sea blue stone in a pointed silver mount, as on the old books"""
    y = -0.056
    points = []
    for a in np.linspace(0, math.tau, 48, endpoint=False):
        star = 0.07 + 0.038 * abs(math.cos(2 * a)) ** 3
        points.append((x + star * size * math.cos(a), z + star * size * math.sin(a)))
    plate_from_outline(points, y + 0.012, 0.012, m["fitting"], name="Fassung", bevel=0.004)
    torus((x, y - 0.002, z), 0.054 * size, 0.008 * size, m["silver"], name="Fassring")
    facet_stone(m, x, y - 0.006, z, 0.052 * size)
    for a in np.linspace(0, math.tau, 4, endpoint=False):
        sphere((x + 0.088 * size * math.cos(a), y - 0.002, z + 0.088 * size * math.sin(a)), 0.0085 * size, m["silver"], name="Krappe", segments=10)


def plate_vine(m, cx, cz, from_deg, to_deg):
    """Silver vine along the outside of a round plate: a wave with a curl at every bend"""
    radius = 0.4
    pts = []
    for t in np.linspace(0.0, 1.0, 120):
        a = math.radians(from_deg + (to_deg - from_deg) * t)
        r = radius + 0.017 * math.sin(t * math.tau * 3.0)
        pts.append((cx + r * math.cos(a), -0.044, cz + r * math.sin(a)))
    tube(pts, [0.0058] * len(pts), m["silver"], name="Ranke")
    for k in range(7):
        t = (k + 0.5) / 7
        a = math.radians(from_deg + (to_deg - from_deg) * t)
        outward = 1 if k % 2 == 0 else -1
        r = radius + 0.03 * outward
        tube(spiral(cx + r * math.cos(a), -0.046, cz + r * math.sin(a), 0.027, 1.25, a, outward), [0.0046] * 26, m["silver"], name="Schnecke")


def clasp_band(m, seal_x, seal_z, side):
    """A curved silver clasp from the seal out along the arch of the cover, ending in a curl and a stone"""
    start = (seal_x + side * 0.17, seal_z + 0.03)
    middle = (seal_x + side * 0.42, seal_z + 0.12)
    end = (seal_x + side * 0.74, seal_z + 0.075)
    for offset, radius in ((0.0, 0.0125), (-0.024, 0.0045)):
        pts = [(x, -0.05, z + offset) for x, z, _ in bezier(Vector((*start, 0)), Vector((*middle, 0)), Vector((*end, 0)), 34)]
        tube(pts, [radius] * len(pts), m["silver"], name="Spange")
    tube(spiral(end[0] + side * 0.012, -0.05, end[1] - 0.03, 0.04, 1.4, math.pi / 2, -side), [0.0075] * 26, m["silver"], name="Schnecke")
    cabochon(m, seal_x + side * 0.42, seal_z + 0.075, 0.42)


def tray_corner(m, x, z, sx, sz):
    """Silver bracket on a corner of the sunken field"""
    arm = 0.1
    tube([(x + sx * arm, -0.05, z), (x, -0.05, z), (x, -0.05, z + sz * arm)], [0.009, 0.0095, 0.009], m["silver"], name="Winkel")
    sphere((x, -0.056, z), 0.0165, m["silver"], name="Niete", segments=14)
    tube(spiral(x - sx * 0.002, -0.05, z + sz * 0.002, 0.026, 1.15, 0.0, sz), [0.0045] * 26, m["silver"], name="Schnecke") if False else None


def shingle(x, z, length, width, angle, y, material_, ridge=None):
    """One scale: a pointed plate with a raised middle line, the tip along the angle"""
    c, s = math.cos(angle), math.sin(angle)
    outline = [(-0.42, 0.56), (-0.1, 0.64), (0.3, 0.46), (0.62, 0.0), (0.3, -0.46), (-0.1, -0.64), (-0.42, -0.56), (-0.5, 0.0)]
    points = [(x + (u * c * length - v * s * width), z + (u * s * length + v * c * width)) for u, v in outline]
    plate = plate_from_outline(points, y, 0.008, material_, name="Schuppe", bevel=min(length, width) * 0.13)
    if ridge is not None:
        tube([(x - 0.3 * c * length, y - 0.003, z - 0.3 * s * length), (x + 0.5 * c * length, y - 0.003, z + 0.5 * s * length)], [min(length, width) * 0.075, min(length, width) * 0.03], ridge, name="Grat", resolution=2)
    return plate


def flowing(points, per_segment=14):
    """A smooth line through (x, y, radius) points of the HUD panel, as (x, y, radius) again"""
    p = np.array(points, dtype=float)
    p = np.vstack((2 * p[0] - p[1], p, 2 * p[-1] - p[-2]))
    out = []
    for i in range(1, len(p) - 2):
        for t in np.linspace(0.0, 1.0, per_segment, endpoint=False):
            out.append(0.5 * ((2 * p[i]) + (p[i + 1] - p[i - 1]) * t + (2 * p[i - 1] - 5 * p[i] + 4 * p[i + 1] - p[i + 2]) * t * t
                              + (3 * p[i] - p[i - 1] - 3 * p[i + 1] + p[i + 2]) * t ** 3))
    out.append(p[-2])
    return out


def rolled(points, turns, hand, shrink=0.8):
    """Goes on from the end of a line of (x, y, radius) points in a spiral that winds inwards: the curl of a horn or a tendril"""
    x0, y0, r0 = points[-1]
    x1, y1, _ = points[-2]
    heading = math.atan2(y0 - y1, x0 - x1)
    step = math.hypot(x0 - x1, y0 - y1)
    out = list(points)
    x, y = x0, y0
    count = int(turns * 40)
    for i in range(count):
        heading += hand * math.tau / 40 * (1.0 + 2.2 * i / count)
        x += math.cos(heading) * step * (1.0 - 0.55 * i / count)
        y += math.sin(heading) * step * (1.0 - 0.55 * i / count)
        out.append((x, y, r0 * (1.0 - shrink * (i + 1) / count)))
    return out


class Panel:
    """Places a picture cut out of the HUD panel in the scene: one unit is a hundred design pixels"""

    def __init__(self, cx, cy):
        self.cx, self.cy = cx, cy
        self.pivot = (cx, cy)
        self.zoom = 1.0

    def zoom_at(self, pivot, zoom):
        """Everything from now on is drawn this much larger around the pivot"""
        self.pivot = pivot
        self.zoom = zoom

    def at(self, x, y, depth=-0.05):
        x = self.pivot[0] + (x - self.pivot[0]) * self.zoom
        y = self.pivot[1] + (y - self.pivot[1]) * self.zoom
        return ((x - self.cx) / 100.0, depth, (self.cy - y) / 100.0)

    def line(self, points, material_, name, depth=-0.05, resolution=6):
        tube([self.at(x, y, depth) for x, y, _ in points], [r * self.zoom / 100.0 for _, _, r in points], material_, name=name, resolution=resolution)

    def segments(self, points, material_, every=6):
        """Engraved bands across a body, curved like the scales of a snake: they read as scales even when small"""
        for i in range(every, len(points) - every, every):
            x, y, r = points[i]
            nx, ny, _ = points[i + 1]
            px_, py, _ = points[i - 1]
            ax, ay = nx - px_, ny - py
            size = math.hypot(ax, ay) or 1.0
            ax, ay = ax / size, ay / size
            across = (-ay, ax)
            arc = []
            for t in np.linspace(-1.0, 1.0, 9):
                back = (1.0 - t * t) * r * 0.75
                arc.append(self.at(x + across[0] * t * r - ax * back, y + across[1] * t * r - ay * back, -0.05 - (1.0 - t * t) ** 0.5 * r / 100.0 - 0.002))
            tube(arc, [0.0052 * min(r / 4.0, 1.0)] * 9, material_, name="Band", resolution=2)

    def crest(self, points, side, material_, every=7):
        """Curved thorns along the back of a body, each leaning backwards"""
        for i in range(every, len(points) - 2, every):
            x, y, r = points[i]
            nx, ny, _ = points[i + 1]
            ax, ay = nx - x, ny - y
            size = math.hypot(ax, ay) or 1.0
            ax, ay = ax / size, ay / size
            ox, oy = -ay * side, ax * side
            height = 2.2 + r * 0.8
            root = (x + ox * r * 0.8, y + oy * r * 0.8, r * 0.34)
            mid = (root[0] + ox * height * 0.6 - ax * height * 0.25, root[1] + oy * height * 0.6 - ay * height * 0.25, r * 0.2)
            tip = (root[0] + ox * height - ax * height * 0.8, root[1] + oy * height - ay * height * 0.8, 0.14)
            self.line(flowing([root, mid, tip], 4), material_, "Dorn", depth=-0.045, resolution=3)


def dragon_head(m):
    """The head of the Urdrachin: cast silver in one flowing line with the neck, coming out of the end of the experience bar.
    One horn winds round the dodge button, the other runs back under the last slot name and curls into the scales."""
    mage_materials(m)
    board = Panel(700.0, 185.0)
    silver, dark = m["silver"], m["darksilver"]
    ivory = material("Zahn", (0.82, 0.76, 0.62), roughness=0.3, coat=0.5)
    shadow = material("Schlund", (0.03, 0.004, 0.01), roughness=0.5)

    # Neck and head in one line: it leaves the bar, swings down and rises into the skull, narrows into the snout, flares at the nose
    neck = flowing([(646, 219, 4.2), (660, 223, 4.8), (674, 222, 5.0), (686, 214, 5.2), (695, 204, 6.0), (703, 199, 7.8), (711, 199, 6.8),
                    (719, 202, 4.5), (727, 205, 3.5), (733, 206, 4.0), (737, 207.5, 2.2)])
    # The head is larger than the neck that carries it: the line grows out of the bar into it
    pivot = (690.0, 205.0)
    head_size = 1.38
    grown = []
    for x, y, r in neck:
        size = 1.0 + (head_size - 1.0) * min(max((x - 672.0) / 20.0, 0.0), 1.0)
        grown.append((pivot[0] + (x - pivot[0]) * size, pivot[1] + (y - pivot[1]) * size, r * size))
    neck = grown
    board.line(neck, silver, "Hals", depth=-0.02)
    board.segments(neck[:58], dark)
    board.crest(neck[6:64], -1, silver)
    board.zoom_at(pivot, head_size)

    # Mouth: dark behind, the jaw as a second line below it with a hooked chin
    tube([board.at(x, y, 0.0) for x, y in ((700, 206), (714, 207), (730, 209), (728, 214), (712, 215), (700, 212))], [0.022] * 6, shadow, name="Schlund", cyclic=True, resolution=3)
    jaw = flowing([(696, 209, 3.2), (706, 214, 2.9), (716, 216, 2.2), (725, 215, 1.7), (730, 212.5, 1.0)])
    board.line(jaw, silver, "Kiefer", depth=-0.035)
    for x, y, h, lean in ((713, 205.5, 4.0, 0.8), (718, 206.5, 3.0, 0.5), (723, 207.5, 3.2, 0.5), (728, 208.5, 5.6, 1.0), (733, 209, 4.2, 0.4)):
        board.line(flowing([(x, y, 0.95), (x + lean * 0.4, y + h * 0.6, 0.62), (x + lean, y + h, 0.08)], 3), ivory, "Zahn", depth=-0.06, resolution=2)
    for x, y, h in ((710, 214, 3.2), (716, 215, 3.8), (721, 214.5, 3.0), (726, 213.5, 3.6)):
        board.line(flowing([(x, y, 0.8), (x + 0.2, y - h * 0.6, 0.5), (x + 0.6, y - h, 0.08)], 3), ivory, "Zahn", depth=-0.06, resolution=2)

    # Face: a brow that sweeps down to the snout, a ridge along the nose, cheek line and nostril
    board.line(flowing([(693, 195, 1.5), (701, 191.5, 2.4), (709, 194, 1.9), (716, 198.5, 0.9)]), silver, "Braue", depth=-0.1)
    board.line(flowing([(712, 196, 1.3), (721, 199.5, 1.1), (729, 201.5, 1.2), (733, 202, 0.5)]), silver, "Nasengrat", depth=-0.09)
    board.line(flowing([(691, 210, 0.9), (700, 204.5, 1.1), (710, 205, 0.8)]), dark, "Wange", depth=-0.1)
    tube([board.at(x, y, -0.09) for x, y in ((729.5, 203.5), (732, 203), (733, 205), (731, 206))], [0.0085] * 4, dark, name="Nuester", cyclic=True, resolution=3)
    sphere(board.at(704, 196.5, -0.085), 0.06, shadow, scale=(1.2, 0.5, 0.68), name="Augenhoehle", segments=20)
    facet_stone(m, *[board.at(704.5, 196.5, -0.112)[k] for k in (0, 1, 2)], 0.048, 0.74)
    board.zoom_at(pivot, 1.0)

    # Great horn: from behind the brow round the left of the dodge button, over its top, curling in at its right
    center = (712.0, 164.0)
    points = [(696, 190, 4.6), (691, 185, 3.8)]
    for k, a in enumerate(np.linspace(168, 318, 8)):
        rad = math.radians(a)
        points.append((center[0] + 24.5 * math.cos(rad), center[1] + 24.5 * math.sin(rad) * 1.0, 3.0 - k * 0.28))
    horn = rolled(flowing(points, 8), 1.3, 1, shrink=0.86)
    board.line(horn, silver, "Horn", depth=-0.075)
    board.segments(horn[:60], dark, every=7)

    # Second horn: back below the last slot name along the scales, then rolling in over the bar
    second = rolled(flowing([(692, 196, 3.0), (682, 204, 2.5), (670, 210.5, 2.0), (654, 212, 1.6), (640, 212.5, 1.2)], 8), 1.1, -1, shrink=0.8)
    board.line(second, silver, "Horn", depth=-0.075)

    # One tendril behind the jaw that rolls up, the rest stays calm so the head reads
    for start, ctrl, end, hand, turns in (((694, 216), (684, 228), (670, 234), 1, 1.0),):
        line = rolled(flowing([(start[0], start[1], 1.4), (ctrl[0], ctrl[1], 1.1), (end[0], end[1], 0.8)], 10), turns, hand, shrink=0.75)
        board.line(line, silver, "Bartranke", depth=-0.06, resolution=4)


def dragon_tail(m):
    """The tail of the same dragon: it leaves the start of the experience bar, loops under the potion button, climbs along
    the field of the abilities and ends in a leaf shaped barb, with thin tendrils branching off like the vines of the cover."""
    mage_materials(m)
    board = Panel(180.0, 178.0)
    silver, dark = m["silver"], m["darksilver"]

    tail = flowing([(208, 219, 4.2), (194, 224, 4.2), (178, 225, 3.9), (163, 222, 3.5), (150, 209, 3.2), (142, 192, 3.0), (150, 174, 2.7), (166, 167, 2.5),
                    (177, 158, 2.2), (182, 149, 1.9), (178, 141, 1.5)])
    board.line(tail, silver, "Schwanz", depth=-0.02)
    board.segments(tail, dark)
    board.crest(tail[:120], 1, silver, every=8)

    # Barb at the tip: two curved blades around a stone
    x, y, _ = tail[-1]
    for side in (1, -1):
        blade = flowing([(x, y + 1, 1.1), (x + side * 7, y - 6, 1.5), (x + side * 4, y - 15, 0.9), (x - side * 1, y - 21, 0.08)], 8)
        board.line(blade, silver, "Klinge", depth=-0.03, resolution=4)
    tube([board.at(*p, -0.02) for p in ((x, y + 1), (x + 6, y - 6), (x + 3, y - 14), (x, y - 20), (x - 3, y - 14), (x - 6, y - 6))], [0.03] * 6, m["cover"], name="Klingengrund", cyclic=True, resolution=3)
    facet_stone(m, *board.at(x, y - 8, -0.06), 0.03)

    # Tendrils branching off, rolling in like the vines on the plates
    for start, ctrl, end, hand, turns in (((192, 225, ), (200, 233), (214, 234), 1, 1.0), ((142, 200), (130, 205), (124, 216), 1, 0.9), ((168, 166), (180, 170), (190, 180), -1, 1.0), ((181, 146), (172, 142), (165, 150), 1, 0.9)):
        line = rolled(flowing([(start[0], start[1], 1.3), (ctrl[0], ctrl[1], 1.0), (end[0], end[1], 0.75)], 10), turns, hand, shrink=0.75)
        board.line(line, silver, "Ranke", depth=-0.04, resolution=4)


def scales_picture(path):
    """Transparent picture over the experience bar: overlapping scales pointing to the head, dark rims and a light on each"""
    width, height = 880, 28
    across, along = 12, 14
    buffer = np.zeros((height, width, 4), np.float32)
    yy, xx = np.mgrid[0:height, 0:width]
    centers = [(x + (along / 2 if row % 2 else 0), row * across * 0.55 + 2.0) for row in range(6) for x in np.arange(-along, width + along, along)]
    for cx, cy in sorted(centers):
        u = (xx - cx) / (along * 0.82)
        v = (yy - cy) / (across * 0.62)
        d = np.sqrt(np.maximum(u, 0.0) ** 2 * 1.0 + (u * (u < 0)) ** 2 * 0.35 + v * v)
        inside = d < 1.0
        rim = np.clip((d - 0.72) / 0.28, 0.0, 1.0)
        light = np.clip(1.0 - np.hypot(u - 0.1, v + 0.45) / 0.55, 0.0, 1.0)
        shade = np.clip(0.34 + 0.35 * v, 0.0, 0.6)
        color = np.where(light[..., None] > rim[..., None], np.array((0.9, 0.97, 1.0)), np.array((0.0, 0.012, 0.03)))
        alpha = np.maximum(np.maximum(rim * 0.92, light * 0.5), shade * 0.5)
        buffer[inside, 0:3] = color[inside]
        buffer[inside, 3] = alpha[inside]
    image = bpy.data.images.new("Schuppen", width, height, alpha=True)
    image.pixels.foreach_set(buffer[::-1].ravel())
    image.filepath_raw = path
    image.file_format = "PNG"
    image.save()
    print("[Vael] Schuppen:", path)


def frame_mage(m):
    """The grimoire of the mage as the frame: deep blue leather, silver edge, the seal with the four elements (Designbibel).
    No rectangle: round plates around the vessels, joined by the body of the book."""
    mage_materials(m)
    outline = [px(x, y) for x, y in mage_outline()]

    plate_from_outline(outline, -0.03, 0.06, m["cover"], name="Leder")
    closed = lambda path, y: [(x, y, z) for x, z in path]
    tube(closed(outline, -0.04), [0.017] * len(outline), m["silver"], name="Kante", cyclic=True)
    groove = inset(outline, 0.052)
    tube(closed(groove, -0.031), [0.0036] * len(groove), m["groove"], name="Praegelinie", cyclic=True)
    stitches(m, inset(outline, 0.034), -0.035)

    # Rings around the vessels and a small sapphire where each plate meets the body
    for x_px in (96.0, 784.0):
        x, z = px(x_px, 130.0)
        torus((x, -0.034, z), 0.44, 0.005, m["groove"], name="Praegering")
        direction = 1.0 if x_px < 440.0 else -1.0
        for y_px, up in ((50.0, 1.0),):
            gx, gz = px(x_px + direction * 62.0, y_px)
            torus((gx, -0.05, gz), 0.03, 0.009, m["silver"], name="Fassung")
            facet_stone(m, gx, -0.054, gz, 0.028)
            scroll(m, x_px + direction * 70.0, y_px + up * 5.0, direction, up)

    # The sunken field of the abilities
    left, tray_top = px(190.0, 110.0)
    right, tray_bottom = px(690.0, 208.0)
    tray_w, tray_h = right - left, tray_top - tray_bottom
    tray_x, tray_z = (left + right) / 2, (tray_top + tray_bottom) / 2
    tray = rounded_plate(tray_w, tray_h, 0.03, 0.012, m["sunken"], y=-0.032, name="Feldgrund")
    tray.location.x = tray_x
    tray.location.z = tray_z
    path = [(x + tray_x, y, z + tray_z) for x, y, z in rounded_rect_path(tray_w + 0.012, tray_h + 0.012, 0.032, -0.042)]
    tube(path, [0.0052] * len(path), m["darksilver"], name="Feldrand", cyclic=True)

    for sx, sz, x, z in ((-1, 1, left, tray_top), (1, 1, right, tray_top), (-1, -1, left, tray_bottom), (1, -1, right, tray_bottom)):
        tray_corner(m, x, z, -sx, -sz)

    # The body of the dragon: a dark channel for the experience, a row of silver scales above it, head and tail at its ends
    ch_left, ch_top = px(206.0, 212.0)
    ch_right, ch_bottom = px(650.0, 226.0)
    channel = rounded_plate(ch_right - ch_left, ch_top - ch_bottom, 0.012, 0.012, m["sunken"], y=-0.034, name="Rinne")
    channel.location.x = (ch_left + ch_right) / 2
    channel.location.z = (ch_top + ch_bottom) / 2
    for k, x_px in enumerate(np.arange(204.0, 654.0, 8.6)):
        x, z = px(float(x_px), 212.5)
        shingle(x, z, 0.085, 0.038, 0.0, -0.046 - k * 0.0002, m["fitting"], m["darksilver"])

    # Silver work on the leather: vines on the round plates, a stone at their outer edge, clasps out of the seal
    for x_px, from_deg, to_deg in ((96.0, 112.0, 248.0), (784.0, -68.0, 68.0)):
        cx, cz = px(x_px, 130.0)
        plate_vine(m, cx, cz, from_deg, to_deg)
    cabochon(m, *px(16.0, 130.0), 0.72)
    cabochon(m, *px(864.0, 130.0), 0.72)
    seal_x, seal_z = px(440.0, 66.0)
    element_seal(m, seal_x, seal_z, 1.22)
    for side in (1, -1):
        clasp_band(m, seal_x, seal_z, side)


def page(x0, x1, z0, z1, m, name):
    """A page of parchment that arches up from the fold of the book"""
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    nx, nz = 48, 28
    grid = []
    fold_left = abs(x0) < abs(x1)
    for i in range(nx + 1):
        u = i / nx
        rise = math.sin(math.pi * (u if fold_left else 1 - u) ** 0.6) ** 1.2
        row = []
        for k in range(nz + 1):
            row.append(bm.verts.new((x0 + (x1 - x0) * u, -0.04 - 0.05 * rise, z0 + (z1 - z0) * k / nz)))
        grid.append(row)
    for i in range(nx):
        for k in range(nz):
            bm.faces.new((grid[i][k], grid[i + 1][k], grid[i + 1][k + 1], grid[i][k + 1]))
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    for poly in mesh.polygons:
        poly.use_smooth = True
    return give(obj, m["parchment"])


def page_y(x, x0, x1):
    """Height of the page surface at a place, for what lies on it"""
    u = (x - x0) / (x1 - x0)
    fold_left = abs(x0) < abs(x1)
    return -0.04 - 0.05 * math.sin(math.pi * (u if fold_left else 1 - u) ** 0.6) ** 1.2 - 0.006


def cell(m, x, y, z, size, round_cell=False):
    """A field of the inventory, drawn into the page: dark ink with a silver rim"""
    if round_cell:
        cylinder((x, y + 0.004, z), size / 2, 0.008, m["ink"], rotation=(math.radians(90), 0, 0), name="Feld", vertices=48)
        torus((x, y, z), size / 2, 0.009, m["silver"], name="Feldring")
        torus((x, y - 0.002, z), size / 2 - 0.024, 0.0022, m["darksilver"], name="Feldlinie")
    else:
        fill = rounded_plate(size, size, 0.02, 0.008, m["ink"], y=y + 0.004, name="Feld")
        fill.location.x = x
        fill.location.z = z
        path = [(px_ + x, y, pz + z) for px_, _, pz in rounded_rect_path(size, size, 0.022, 0.0, n_corner=4)]
        tube(path, [0.0062] * len(path), m["silver"], name="Feldring", cyclic=True, resolution=3)


def inventory_mage(m):
    """The inventory of the mage: the open grimoire. Left page the equipment on a drawn figure, right page the backpack,
    tabs for bag and class compartment, a ribbon as bookmark. Text and items are drawn by the game."""
    mage_materials(m)
    m["parchment"] = worn(material("Pergament", (0.46, 0.33, 0.17), roughness=0.9, bump=0.22, bump_scale=45.0), scale=2.4, darkest=0.55)
    m["ink"] = material("Tinte", (0.002, 0.003, 0.008), roughness=0.85, bump=0.15, bump_scale=160.0)
    ink_line = material("Federstrich", (0.05, 0.035, 0.025), roughness=0.7)

    w, h = 3.42, 1.9
    cover = rounded_plate(w, h, 0.05, 0.06, m["cover"], y=0.03, name="Einband")
    tube(rounded_rect_path(w - 0.012, h - 0.012, 0.06, -0.01), [0.015] * 200, m["silver"], name="Kante", cyclic=True)
    stitches(m, [(x, z) for x, _, z in rounded_rect_path(w - 0.07, h - 0.07, 0.05, 0.0, n_corner=8)], -0.012)
    for sx, sz in ((-1, 1), (1, 1), (-1, -1), (1, -1)):
        corner_fitting(m, sx, sz, w, h, 0.0)

    # Page block under the pages, then the two pages and the fold between them
    pw, ph, top = 1.58, 1.72, 0.86
    block = rounded_plate(2 * pw + 0.06, 2 * top + 0.05, 0.03, 0.03, material("Seitenblock", (0.3, 0.24, 0.15), roughness=0.9), y=-0.02, name="Block")
    page(-pw, -0.004, -top, top, m, "Seite links")
    page(0.004, pw, -top, top, m, "Seite rechts")
    tube([(0.0, -0.05, top + 0.01), (0.0, -0.05, -top - 0.01)], [0.012] * 2, material("Falz", (0.03, 0.02, 0.014), roughness=0.8), name="Falz")

    # Thin ruled lines with a small centre mark under the headings of both pages
    for x0, x1 in ((-pw + 0.14, -0.14), (0.14, pw - 0.14)):
        for z in (top - 0.2,):
            cx = (x0 + x1) / 2
            line = [(x, page_y(x, -pw if x0 < 0 else 0.0, 0.0 if x0 < 0 else pw) - 0.002, z) for x in np.linspace(x0, x1, 30)]
            tube(line, [0.0022] * len(line), ink_line, name="Linie")
            facet_stone(m, cx, page_y(cx, -pw if x0 < 0 else 0.0, 0.0 if x0 < 0 else pw) - 0.004, z, 0.018)

    # Left page: a figure drawn in ink, the ten places around it
    fx, fz = -0.8, -0.12
    hood = [(fx + 0.1 * math.cos(a), page_y(fx, -pw, 0.0) - 0.002, fz + 0.5 + 0.12 * math.sin(a)) for a in np.linspace(0, math.tau, 30)]
    tube(hood, [0.003] * len(hood), ink_line, name="Kopf", cyclic=True)
    for side in (1, -1):
        robe = bezier((fx + side * 0.06, fz + 0.39, 0.0), (fx + side * 0.32, fz + 0.2, 0.0), (fx + side * 0.26, fz - 0.62, 0.0), 24)
        tube([(x, page_y(x, -pw, 0.0) - 0.002, z) for x, z, _ in [(p[0], p[1], 0) for p in robe]], [0.003] * 24, ink_line, name="Robe")
    tube([(x, page_y(x, -pw, 0.0) - 0.002, fz - 0.62) for x in (fx - 0.26, fx + 0.26)], [0.003] * 2, ink_line, name="Saum")
    size = 0.235
    places = ((0.0, 0.5), (0.0, 0.2), (0.0, -0.07), (0.0, -0.33),            # head, amulet, chest, belt
              (-0.5, 0.26), (0.5, 0.26), (-0.5, -0.1), (-0.5, -0.46),        # weapon, offhand, hands, feet
              (0.5, -0.1), (0.5, -0.46))                                      # rings
    for dx, dz in places:
        x = fx + dx
        cell(m, x, page_y(x, -pw, 0.0) - 0.008, fz + dz, size, round_cell=True)

    # Right page: the backpack, seven by five
    columns, rows = 7, 5
    size = 0.176
    step = 0.196
    cx, cz = 0.8, -0.16
    for c in range(columns):
        for r in range(rows):
            x = cx + (c - (columns - 1) / 2) * step
            cell(m, x, page_y(x, 0.0, pw) - 0.008, cz + ((rows - 1) / 2 - r) * step, size)

    # Tabs on the right edge and the ribbon
    for k, color in enumerate(((0.05, 0.12, 0.3), (0.12, 0.09, 0.05), (0.05, 0.08, 0.05))):
        tab = rounded_plate(0.18, 0.3, 0.05, 0.02, material("Reiter%d" % k, color, roughness=0.6, bump=0.3, bump_scale=200.0), y=-0.012, name="Reiter")
        tab.location.x = w / 2 - 0.09
        tab.location.z = 0.42 - k * 0.36
    # The ribbon lies in the fold and hangs out below
    ribbon = rounded_plate(0.075, 1.04, 0.006, 0.006, material("Band", (0.008, 0.03, 0.15), roughness=0.5), y=-0.1, name="Band")
    ribbon.location.x = 0.0
    ribbon.location.z = -0.43
    element_seal(m, 0.0, top + 0.015, 0.8)


def frame_floromant(m):
    w, h = FRAME_ASPECT, 1.0
    rounded_plate(w, h, 0.1, 0.06, m["bark"], name="Rinde")
    # Two vines twisting round each other along the edge
    path = np.array(rounded_rect_path(w - 0.07, h - 0.07, 0.09, -0.04, n_corner=14))
    closed = np.vstack([path, path[:1]])
    lengths = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(closed, axis=0), axis=1))])
    n = 480
    dense = []
    for target in np.linspace(0.0, lengths[-1], n, endpoint=False):
        k = np.searchsorted(lengths, target, side="right") - 1
        t = (target - lengths[k]) / max(lengths[k + 1] - lengths[k], 1e-9)
        dense.append(closed[k] + (closed[k + 1] - closed[k]) * t)
    for phase in (0.0, math.pi):
        pts = []
        for i, p in enumerate(dense):
            a = i / n * 2 * math.pi * 40 + phase
            pts.append((p[0], p[1] - 0.01 + 0.03 * math.cos(a), p[2] + 0.03 * math.sin(a)))
        tube(pts, [0.02] * len(pts), m["darkwood"], name="Ranke", cyclic=True)
    # Leaves along the edges, blossoms in the corners and the middle, turquoise buds between
    rng = np.random.default_rng(3)
    for i in range(0, n, 9):
        p = dense[i]
        inward = math.atan2(-p[2], -p[0] / (w / h)) + rng.uniform(-0.9, 0.9)
        leaf((p[0], -0.07, p[2]), rng.uniform(0.09, 0.14), rng.uniform(0.045, 0.07), inward, m["leaf"], bend=0.1, curl=0.25, name="Blatt")
    def blossom(cx, cz, size):
        for k in range(5):
            a = k * 2 * math.pi / 5
            petal = sphere((cx + size * 0.55 * math.cos(a), -0.08, cz + size * 0.55 * math.sin(a)), size * 0.45, m["blossom"], scale=(1.3, 0.3, 0.8), name="Bluetenblatt", segments=12)
            petal.rotation_euler = (0, -a, 0)
        sphere((cx, -0.1, cz), size * 0.25, m["bud"], name="Bluetenmitte", segments=12)
    for cx, cz in ((-w / 2 + 0.09, h / 2 - 0.09), (w / 2 - 0.09, h / 2 - 0.09), (-w / 2 + 0.09, -h / 2 + 0.09), (w / 2 - 0.09, -h / 2 + 0.09)):
        blossom(cx, cz, 0.08)
    blossom(0.0, h / 2 - 0.03, 0.1)
    for i in range(5, n, 23):
        p = dense[i]
        sphere((p[0], -0.09, p[2]), 0.028, m["bud"], scale=(0.8, 0.8, 1.25), name="Knospe", segments=12)
    for i in range(40, n, 80):
        p = dense[i]
        blossom(p[0], p[2], 0.055)


# ---------------------------------------------------------------- Felder und Medaillons

def slot(m, bud):
    # Ring only: the inside stays empty for the picture of the ability
    if bud:
        pts = []
        for t in np.linspace(0, 2 * math.pi, 80):
            x, z = 0.43 * math.cos(t), 0.43 * math.sin(t)
            z = z if z < 0 else z * 1.15 + 0.04 * math.cos(t * 3)
            pts.append((x, -0.05, z * 0.95 - 0.02))
        tube(pts, [0.035] * len(pts), m["wood"], name="Knospenrand", cyclic=True)
        for x in (-0.12, 0.12):
            leaf = sphere((x, -0.08, 0.46), 0.07, m["leaf"], scale=(1.5, 0.25, 0.6), name="Blatt", segments=12)
            leaf.rotation_euler = (0, -x * 4, 0)
    else:
        tube(rounded_rect_path(0.88, 0.88, 0.1, -0.05), [0.035] * 200, m["silver"], name="Feldrand", cyclic=True)
        for cx, cz in ((-0.42, 0.42), (0.42, 0.42), (-0.42, -0.42), (0.42, -0.42)):
            sphere((cx, -0.08, cz), 0.04, m["darksilver"], name="Niete", segments=16)


def medallion(m, wood):
    if wood:
        pts = [(0.4 * math.cos(t), -0.05 + 0.01 * math.sin(t * 9), 0.4 * math.sin(t)) for t in np.linspace(0, 2 * math.pi, 60)]
        tube(pts, [0.06] * len(pts), m["wood"], name="Holzring", cyclic=True)
        leaf = sphere((0.3, -0.1, 0.32), 0.1, m["leaf"], scale=(1.5, 0.25, 0.6), name="Blatt", segments=12)
        leaf.rotation_euler = (0, -0.7, 0)
        sphere((-0.32, -0.1, 0.3), 0.045, m["bud"], name="Knospe", segments=12)
    else:
        torus((0.0, -0.05, 0.0), 0.4, 0.06, m["silver"], name="Silberring")
        torus((0.0, -0.06, 0.0), 0.33, 0.012, m["darksilver"], name="Innenring")


# ---------------------------------------------------------------- Ablauf

PIECES = {
    # name: (width px, height px, view width, builder)
    "leben": (512, 512, VESSEL_VIEW, lambda m: (phial(m), claw(m, 0.5, 0.62, 0.3, 0.3))),
    "magier_gefaess": (512, 512, VESSEL_VIEW, lambda m: (inkwell(m), claw(m, 0.5, 0.73, 0.33, 0.18))),
    "floromant_gefaess": (512, 512, VESSEL_VIEW, lambda m: (pitcher(m), claw(m, 0.5, 0.64, 0.24, 0.27, wrap_vines=True))),
    "magier_rahmen": (1760, 404, FRAME_ASPECT, frame_mage),
    "magier_inventar": (1920, 1080, 3.5556, inventory_mage),
    "magier_drache": (280, 240, 1.4, dragon_head),
    "magier_schwanz": (280, 240, 1.4, dragon_tail),
    "floromant_rahmen": (1760, 404, FRAME_ASPECT, frame_floromant),
    "magier_feld": (160, 160, 1.0, lambda m: slot(m, False)),
    "floromant_feld": (160, 160, 1.0, lambda m: slot(m, True)),
    "magier_medaillon": (128, 128, 1.0, lambda m: medallion(m, False)),
    "floromant_medaillon": (128, 128, 1.0, lambda m: medallion(m, True)),
}

FILE_NAMES = {
    "leben": "T_HUD_Gemalt_Leben", "magier_gefaess": "T_HUD_Gemalt_Gefaess_Magier", "floromant_gefaess": "T_HUD_Gemalt_Gefaess_Floromant",
    "magier_inventar": "T_HUD_Gemalt_Inventar_Magier", "magier_drache": "T_HUD_Gemalt_Drache_Magier", "magier_schwanz": "T_HUD_Gemalt_Schwanz_Magier", "magier_rahmen": "T_HUD_Gemalt_Rahmen_Magier", "floromant_rahmen": "T_HUD_Gemalt_Rahmen_Floromant",
    "magier_feld": "T_HUD_Gemalt_Feld_Magier", "floromant_feld": "T_HUD_Gemalt_Feld_Floromant",
    "magier_medaillon": "T_HUD_Gemalt_Medaillon_Magier", "floromant_medaillon": "T_HUD_Gemalt_Medaillon_Floromant",
}


def mask_render(path_out, folder, name):
    """For frames: a second picture whose red is the inner panel and green the gems, blossoms and seals (the shadow way lets them glow)"""
    white = material("Weiss", (0, 0, 0), emission=((1, 1, 1), 1.0))
    black = material("Schwarz", (0, 0, 0), roughness=1.0)
    channels = {}
    for tag in ("panel", "gem"):
        for obj in bpy.data.objects:
            if obj.type in ("MESH", "CURVE"):
                give_keep = obj.get("tag", "")
                obj.data.materials.clear()
                obj.data.materials.append(white if give_keep == tag else black)
                obj["tag"] = give_keep
        tmp = os.path.join(folder, "_maske_%s.png" % tag)
        bpy.context.scene.cycles.samples = 8
        render(tmp)
        image = bpy.data.images.load(tmp)
        w, h = image.size
        px = np.empty(w * h * 4, np.float32)
        image.pixels.foreach_get(px)
        channels[tag] = px.reshape(h, w, 4)[..., 0] * px.reshape(h, w, 4)[..., 3]
        bpy.data.images.remove(image)
        os.remove(tmp)
    h, w = channels["panel"].shape
    out = np.dstack([channels["panel"], channels["gem"], np.zeros((h, w), np.float32), np.ones((h, w), np.float32)])
    image = bpy.data.images.new("Maske", w, h, alpha=True)
    image.pixels.foreach_set(out.ravel())
    image.filepath_raw = path_out
    image.file_format = "PNG"
    image.save()
    print("[Vael] Maske:", path_out)


def main(folder, wanted):
    addon_utils.enable("cycles", default_set=True)
    os.makedirs(folder, exist_ok=True)
    if not wanted or "magier_schuppen" in wanted:
        scales_picture(os.path.join(folder, "T_HUD_Gemalt_Schuppen_Magier.png"))
    for key, (width, height, view, build) in PIECES.items():
        if wanted and key not in wanted:
            continue
        reset()
        setup(width, height, view, samples=96 if "rahmen" in key else 64)
        mats = common()
        build(mats)
        path = os.path.join(folder, FILE_NAMES[key] + ".png")
        render(path)
        if "rahmen" in key:
            mask_render(os.path.join(folder, FILE_NAMES[key] + "_Maske.png"), folder, key)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:]
    main(args[0], set(args[1:]))
