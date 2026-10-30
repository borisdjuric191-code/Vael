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

def phial(m):
    c = from_mask(0.5, 0.62)
    # Round body and neck from one profile, the mask's numbers: body r 0.3 at y 0.62, neck x 0.42..0.58 from y 0.2 to 0.4
    profile = []
    for a in np.linspace(-math.pi / 2, math.pi / 2 - 0.3, 40):
        profile.append((c.z + 0.3 * math.sin(a), 0.3 * math.cos(a)))
    profile += [(from_mask(0, 0.38).z, 0.08), (from_mask(0, 0.22).z, 0.08)]
    revolve(profile, Vector((0, 0, 0)), m["glass"], name="Phiole")
    torus(tuple(from_mask(0.5, 0.185)), 0.095, 0.022, m["silver"], rotation=(0, 0, 0), name="Lippe")
    cylinder(tuple(from_mask(0.5, 0.11)), 0.065, 0.1, m["cork"], name="Korken")
    torus(tuple(from_mask(0.5, 0.39)), 0.085, 0.012, m["silver"], rotation=(0, 0, 0), name="Halsring")


def inkwell(m):
    # Frustum: bottom y 0.9 radius 0.32, top y 0.5 radius 0.22, neck r 0.12 from y 0.38 to 0.52
    z = lambda y: from_mask(0, y).z
    profile = [(z(0.9), 0.0), (z(0.9), 0.31), (z(0.88), 0.32), (z(0.52), 0.22), (z(0.5), 0.13), (z(0.4), 0.12)]
    revolve(profile, Vector((0, 0, 0)), m["glass"], name="Tintenfass")
    torus(tuple(from_mask(0.5, 0.36)), 0.15, 0.025, m["silver"], rotation=(0, 0, 0), name="Lippe")
    torus(tuple(from_mask(0.5, 0.895)), 0.32, 0.018, m["silver"], rotation=(0, 0, 0), name="Bodenring")
    # Quill: shaft and a soft vane, leaning out of the well to the upper right
    a, b = from_mask(0.52, 0.5), from_mask(0.86, 0.04)
    tube([tuple(a + (b - a) * t + Vector((0, -0.05, 0))) for t in np.linspace(0, 1, 12)], [0.008] * 12, m["feather"], name="Kiel")
    # Vane of single barbs on both sides of the shaft, longest in the middle, slanting towards the tip
    axis = (b - a).normalized()
    across = Vector((axis.z, 0.0, -axis.x))
    for t in np.linspace(0.25, 0.98, 46):
        base = a + (b - a) * t + Vector((0, -0.05, 0))
        length = 0.085 * math.sin(math.pi * (t - 0.2) / 0.8) ** 0.7 + 0.01
        for side in (1, -1):
            tip = base + (across * side * 0.9 + axis * 0.45).normalized() * length * (1.0 if side > 0 else 0.8)
            tube([tuple(base), tuple(base + (tip - base) * 0.5 + Vector((0, -0.004, 0))), tuple(tip)], [0.0035, 0.0028, 0.0015], m["feather"], name="Ast", resolution=2)


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
    sphere((x0 - sx * 0.075, -0.065, z0 - sz * 0.075), 0.032, m["sapphire"], scale=(1, 0.5, 1), name="Saphir", segments=20)
    for a, b in ((0.2, 0.03), (0.03, 0.2)):
        sphere((x0 - sx * a, -0.058, z0 - sz * b), 0.012, m["darksilver"], name="Niete", segments=12)


def stitches(m, width, height, radius, y, lift):
    """A running stitch along a rounded rectangle"""
    path = rounded_rect_path(width, height, radius, y, n_corner=8)
    points = np.array(path + [path[0]])
    lengths = np.linalg.norm(np.diff(points, axis=0), axis=1)
    along = np.concatenate(([0.0], np.cumsum(lengths)))
    start = 0.0
    while start + 0.034 < along[-1]:
        a = [float(np.interp(start, along, points[:, k])) for k in range(3)]
        b = [float(np.interp(start + 0.034, along, points[:, k])) for k in range(3)]
        a[2] += lift
        b[2] += lift
        a[1] = b[1] = y - 0.006
        mid = [(p + q) / 2 for p, q in zip(a, b)]
        mid[1] -= 0.004
        tube([tuple(a), tuple(mid), tuple(b)], [0.0048, 0.0058, 0.0048], m["thread"], name="Stich", resolution=2)
        start += 0.056


def element_seal(m, x, z):
    """The seal of the grimoire: a circle with the signs of the four elements around a sapphire"""
    y = -0.07
    cylinder((x, y + 0.012, z), 0.142, 0.02, m["slotfill"], rotation=(math.radians(90), 0, 0), name="Siegelgrund", vertices=48)
    torus((x, y, z), 0.145, 0.013, m["silver"], name="Siegelring")
    torus((x, y + 0.005, z), 0.124, 0.0035, m["darksilver"], name="Siegelinnen")
    sphere((x, y - 0.01, z), 0.03, m["sapphire"], scale=(1, 0.55, 1), name="Saphir", segments=20)
    r = 0.078
    # Fire above: a tongue of two arcs
    flame = [(x + s * 0.024 * math.sin(t * math.pi) * (1 - 0.35 * t), y, z + r - 0.032 + 0.064 * t)
             for s, ts in ((1, np.linspace(0, 1, 12)), (-1, np.linspace(1, 0, 12))) for t in ts]
    tube(flame, [0.0052] * len(flame), m["silver"], name="Feuer", cyclic=True)
    # Water right: a drop
    drop = [(x + r + 0.026 * math.sin(a) * (1 - 0.72 * max(0.0, math.cos(a))), y, z - 0.008 + 0.034 * math.cos(a)) for a in np.linspace(0, math.tau, 26)]
    tube(drop, [0.0052] * len(drop), m["silver"], name="Wasser", cyclic=True)
    # Earth below: a block
    earth = [(x + 0.031 * cx, y, z - r + 0.028 * cz) for cx, cz in ((-1, -0.75), (1, -0.75), (0.7, 0.45), (0, 1), (-0.7, 0.45))]
    tube(earth, [0.0052] * len(earth), m["silver"], name="Erde", cyclic=True)
    # Air left: three strokes
    for k, shift in enumerate((-0.022, 0.0, 0.022)):
        wind = [(x - r - 0.03 + 0.06 * t, y, z + shift + 0.006 * math.sin(t * math.tau + k)) for t in np.linspace(0, 1, 12)]
        tube(wind, [0.0048] * len(wind), m["silver"], name="Luft")


def frame_mage(m):
    """The grimoire of the mage as the frame: deep blue leather, silver fittings, the seal with the four elements (Designbibel)"""
    w, h = FRAME_ASPECT, 1.0
    leather = worn(material("Grimoireleder", (0.0012, 0.0075, 0.05), roughness=0.74, bump=0.4, bump_scale=240.0, tag="panel"))
    m["thread"] = material("Zwirn", (0.34, 0.28, 0.2), roughness=0.8)
    m["fitting"] = material("Beschlag", (0.24, 0.25, 0.29), metallic=0.7, roughness=0.36, bump=0.14, bump_scale=120.0)
    groove = material("Praegung", (0.002, 0.007, 0.02), roughness=0.6)

    # Pages of parchment showing below the cover
    pages = rounded_plate(w - 0.16, h - 0.01, 0.02, 0.05, material("Seiten", (0.42, 0.34, 0.22), roughness=0.9, bump=0.5, bump_scale=120.0), y=0.02, name="Seiten")
    pages.location.z -= 0.005
    page_line = material("Blattkante", (0.14, 0.11, 0.07), roughness=0.9)
    for k in range(4):
        z = -h / 2 + 0.008 + k * 0.008
        tube([(-w / 2 + 0.1, -0.012, z), (w / 2 - 0.1, -0.012, z)], [0.0012] * 2, page_line, name="Seitenkante")

    lift = 0.02
    cover_h = h - 0.04
    cover = rounded_plate(w, cover_h, 0.05, 0.06, leather, name="Leder")
    cover.location.z += lift

    # Blind tooled lines and the stitch around the cover
    for inset, radius in ((0.22, 0.04), (0.25, 0.03)):
        path = [(x, y, z + lift) for x, y, z in rounded_rect_path(w - inset, cover_h - inset, radius, -0.031)]
        tube(path, [0.0036] * len(path), groove, name="Praegelinie", cyclic=True)
    stitches(m, w - 0.09, cover_h - 0.09, 0.04, -0.032, lift)

    # Raised bands of the spine, one to each side of the middle
    top, bottom = cover_h / 2 + lift - 0.1, -cover_h / 2 + lift + 0.1
    for x_px in (178.0, 702.0):
        x, _ = px(x_px, 0.0)
        for dx, radius, mat in ((0.0, 0.03, leather), (-0.042, 0.0065, m["silver"]), (0.042, 0.0065, m["silver"])):
            tube([(x + dx, -0.03, top), (x + dx, -0.035, 0.0), (x + dx, -0.03, bottom)], [radius] * 3, mat, name="Bund")

    # The sunken field of the abilities
    left, tray_top = px(190.0, 110.0)
    right, tray_bottom = px(690.0, 211.0)
    tray_w, tray_h = right - left, tray_top - tray_bottom
    tray_x, tray_z = (left + right) / 2, (tray_top + tray_bottom) / 2
    tray = rounded_plate(tray_w, tray_h, 0.03, 0.012, material("Vertiefung", (0.0035, 0.009, 0.024), roughness=0.7, bump=0.2, bump_scale=200.0, tag="panel"), y=-0.032, name="Feldgrund")
    tray.location.x = tray_x
    tray.location.z = tray_z
    path = [(x + tray_x, y, z + tray_z) for x, y, z in rounded_rect_path(tray_w + 0.012, tray_h + 0.012, 0.032, -0.042)]
    tube(path, [0.0052] * len(path), m["darksilver"], name="Feldrand", cyclic=True)

    # Strap with the seal in the middle of the top
    seal_x, seal_z = px(440.0, 63.0)
    strap = rounded_plate(0.13, 0.26, 0.012, 0.018, material("Riemen", (0.012, 0.02, 0.045), roughness=0.55, bump=0.5, bump_scale=220.0, tag="panel"), y=-0.036, name="Riemen")
    strap.location.x = seal_x
    strap.location.z = cover_h / 2 + lift - 0.1
    element_seal(m, seal_x, seal_z)

    for sx, sz in ((-1, 1), (1, 1), (-1, -1), (1, -1)):
        corner_fitting(m, sx, sz, w, cover_h, lift)


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
    "floromant_rahmen": (1760, 404, FRAME_ASPECT, frame_floromant),
    "magier_feld": (160, 160, 1.0, lambda m: slot(m, False)),
    "floromant_feld": (160, 160, 1.0, lambda m: slot(m, True)),
    "magier_medaillon": (128, 128, 1.0, lambda m: medallion(m, False)),
    "floromant_medaillon": (128, 128, 1.0, lambda m: medallion(m, True)),
}

FILE_NAMES = {
    "leben": "T_HUD_Gemalt_Leben", "magier_gefaess": "T_HUD_Gemalt_Gefaess_Magier", "floromant_gefaess": "T_HUD_Gemalt_Gefaess_Floromant",
    "magier_rahmen": "T_HUD_Gemalt_Rahmen_Magier", "floromant_rahmen": "T_HUD_Gemalt_Rahmen_Floromant",
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
