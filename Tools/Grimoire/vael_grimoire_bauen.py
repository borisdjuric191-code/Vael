# Builds the floating grimoire of the mage as separate parts and paints its textures.
#
#   blender -b --factory-startup -P Tools/Grimoire/vael_grimoire_bauen.py -- C:/Projekte/Vael/Scripts/Data/Grimoire
#
# Writes SM_Grimoire_{Vorderdeckel,Rueckdeckel,Ruecken,Block}.fbx, T_Grimoire_{Seite,Leder}.png and Vorschau.png.
# Style of the painted inventory book (vael_hud_gemalt.py): deep blue leather, silver work, aqua stones, seal with four element signs.
# All parts are in cm around the hinge they turn on, 1 Blender unit = 1 cm in Unreal (apply_unit_scale in the export):
# X runs across the book, away from the hinge; Y along the spine.
# Scripts/CreateGrimoireKit.py imports them, UVaelFloatingGrimoireComponent moves them; its constants must match the sizes here.

import math
import os
import sys

import bmesh
import bpy
import mathutils
import numpy as np

# Sizes in cm
PAGE_WIDTH = 30.0
PAGE_HEIGHT = 40.0
SPINE_WIDTH = 6.0
BLOCK_THICKNESS = 2.6
BOARD_THICKNESS = 0.8
BOARD_OVERHANG = 1.0

BOARD_WIDTH = PAGE_WIDTH + BOARD_OVERHANG
BOARD_HEIGHT = PAGE_HEIGHT + 2.0 * BOARD_OVERHANG

# Share of the page texture at its bottom that holds the striped edge of the pages
EDGE_SHARE = 0.06


def out_folder():
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    return arguments[0] if arguments else os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = 0.01


def slot(name, color, metallic=0.0, roughness=0.6, emission=None):
    """Material named like the slot in Unreal; the colours only serve the preview here"""
    material = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    material.use_nodes = True
    shader = material.node_tree.nodes["Principled BSDF"]
    shader.inputs["Base Color"].default_value = (*color, 1.0)
    shader.inputs["Metallic"].default_value = metallic
    shader.inputs["Roughness"].default_value = roughness
    if emission is not None:
        shader.inputs["Emission Color"].default_value = (*emission, 1.0)
        shader.inputs["Emission Strength"].default_value = 2.0
    return material


def materials():
    return {
        "Leder": slot("Leder", (0.012, 0.03, 0.11), roughness=0.55),
        "Silber": slot("Silber", (0.62, 0.65, 0.72), metallic=0.8, roughness=0.32),
        "Stein": slot("Stein", (0.02, 0.45, 0.8), roughness=0.1, emission=(0.1, 0.6, 1.0)),
        "Seite": slot("Seite", (0.5, 0.36, 0.2), roughness=0.85),
    }


def finish(mesh_data, name, material, smooth=False):
    obj = bpy.data.objects.new(name, mesh_data)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material)
    for polygon in obj.data.polygons:
        polygon.use_smooth = smooth
    return obj


def rounded_outline(x0, x1, y0, y1, radius, per_corner=6, hinge_square=True):
    """Outline of a board, counter-clockwise; the corners at the hinge stay square so the board meets the spine"""
    points = []
    corners = [(x1 - radius, y0 + radius, -90.0), (x1 - radius, y1 - radius, 0.0)]
    for cx, cy, start in corners:
        for step in range(per_corner + 1):
            angle = math.radians(start + 90.0 * step / per_corner)
            points.append((cx + radius * math.cos(angle), cy + radius * math.sin(angle)))
    if hinge_square:
        points += [(x0, y1), (x0, y0)]
    return points


def board(name, inside_up, material):
    """Slab with its hinge side at x = 0; a slightly rounded edge catches the light"""
    outline = rounded_outline(0.0, BOARD_WIDTH, -BOARD_HEIGHT / 2.0, BOARD_HEIGHT / 2.0, 2.2)
    bm = bmesh.new()
    verts = [bm.verts.new((x, y, 0.0)) for x, y in outline]
    face = bm.faces.new(verts)
    result = bmesh.ops.extrude_face_region(bm, geom=[face])
    top = [element for element in result["geom"] if isinstance(element, bmesh.types.BMVert)]
    outer = -BOARD_THICKNESS if inside_up else BOARD_THICKNESS
    bmesh.ops.translate(bm, verts=top, vec=(0.0, 0.0, outer))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)

    # Leather texture straight from above, in cm
    uv_layer = bm.loops.layers.uv.new("UV")
    for bm_face in bm.faces:
        for loop in bm_face.loops:
            co = loop.vert.co
            loop[uv_layer].uv = (co.x / BOARD_WIDTH, co.y / BOARD_HEIGHT + 0.5)
    mesh_data = bpy.data.meshes.new(name)
    bm.to_mesh(mesh_data)
    bm.free()
    return finish(mesh_data, name, material)


def tube(name, points, radius, material, closed=False, sides=6):
    """Rounded silver line along points; the radius may be one number or one per point"""
    radii = radius if isinstance(radius, (list, tuple)) else [radius] * len(points)
    bm = bmesh.new()
    rings = []
    count = len(points)
    for index, point in enumerate(points):
        before = points[(index - 1) % count] if closed or index > 0 else point
        after = points[(index + 1) % count] if closed or index < count - 1 else point
        tangent = np.array(after) - np.array(before)
        tangent = tangent / max(np.linalg.norm(tangent), 1e-6)
        side = np.cross(tangent, (0.0, 0.0, 1.0))
        side = side / max(np.linalg.norm(side), 1e-6)
        ring = []
        for step in range(sides):
            angle = 2.0 * math.pi * step / sides
            offset = (side * math.cos(angle) + np.array((0.0, 0.0, 1.0)) * math.sin(angle) * 0.75) * radii[index]
            ring.append(bm.verts.new(tuple(np.array(point) + offset)))
        rings.append(ring)
    pairs = count if closed else count - 1
    for index in range(pairs):
        a = rings[index]
        b = rings[(index + 1) % count]
        for step in range(sides):
            bm.faces.new((a[step], a[(step + 1) % sides], b[(step + 1) % sides], b[step]))
    if not closed:
        bm.faces.new(rings[0][::-1])
        bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh_data = bpy.data.meshes.new(name)
    bm.to_mesh(mesh_data)
    bm.free()
    return finish(mesh_data, name, material, smooth=True)


def ring_points(cx, cy, z, radius, count=40):
    return [(cx + radius * math.cos(2.0 * math.pi * i / count), cy + radius * math.sin(2.0 * math.pi * i / count), z) for i in range(count)]


def stone(name, x, y, z, radius, material, up):
    """Cut stone: a flat-shaded dome that stands out of the board"""
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=1, radius=radius, matrix=mathutils.Matrix.Translation((x, y, z)))
    for vert in bm.verts:
        vert.co.z = z + (vert.co.z - z) * 0.6
    mesh_data = bpy.data.meshes.new(name)
    bm.to_mesh(mesh_data)
    bm.free()
    return finish(mesh_data, name, material)


def disc(name, cx, cy, z, radius, height, material, up):
    """Low round plate on the outside of the board"""
    bm = bmesh.new()
    top = z + (height if up else -height)
    rim = []
    cap = []
    for step in range(36):
        angle = 2.0 * math.pi * step / 36
        rim.append(bm.verts.new((cx + radius * math.cos(angle), cy + radius * math.sin(angle), z)))
        cap.append(bm.verts.new((cx + radius * 0.94 * math.cos(angle), cy + radius * 0.94 * math.sin(angle), top)))
    for step in range(36):
        bm.faces.new((rim[step], rim[(step + 1) % 36], cap[(step + 1) % 36], cap[step]))
    bm.faces.new(cap)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh_data = bpy.data.meshes.new(name)
    bm.to_mesh(mesh_data)
    bm.free()
    return finish(mesh_data, name, material)


def corner_fitting(parts, m, x, y, sign_x, sign_y, z, up):
    """Silver corner: an arc from edge to edge with a curl and an aqua stone, like the corners of the painted book"""
    arc = []
    for step in range(15):
        angle = math.radians(90.0 * step / 14)
        arc.append((x - sign_x * 8.0 * (1.0 - math.cos(angle)), y - sign_y * 8.0 * (1.0 - math.sin(angle)), z))
    parts.append(tube("EckeBogen", arc, [0.42 - 0.12 * abs(i - 7) / 7 for i in range(15)], m["Silber"]))

    # Plate between the arc and the corner, a little raised
    plate = [(x - sign_x * 0.8, y - sign_y * 0.8, z)] + arc
    bm = bmesh.new()
    verts = [bm.verts.new((px, py, pz + (0.05 if up else -0.05))) for px, py, pz in plate]
    bm.faces.new(verts)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh_data = bpy.data.meshes.new("EckePlatte")
    bm.to_mesh(mesh_data)
    bm.free()
    if (mesh_data.polygons[0].normal.z > 0.0) != up:
        mesh_data.polygons[0].flip()
    parts.append(finish(mesh_data, "EckePlatte", m["Silber"]))
    parts.append(stone("EckeStein", x - sign_x * 2.2, y - sign_y * 2.2, z + (0.2 if up else -0.2), 0.8, m["Stein"], up))


def element_sign(parts, m, cx, cy, z, element, size):
    """The four signs of the seal, as thin silver lines"""
    if element == "Feuer":
        points = [(cx - 0.5 * size, cy - 0.6 * size, z), (cx, cy + 0.7 * size, z), (cx + 0.5 * size, cy - 0.6 * size, z)]
        parts.append(tube("Zeichen", points, 0.16, m["Stein"], closed=True, sides=5))
    elif element == "Wasser":
        points = [(cx - 0.5 * size, cy + 0.6 * size, z), (cx, cy - 0.7 * size, z), (cx + 0.5 * size, cy + 0.6 * size, z)]
        parts.append(tube("Zeichen", points, 0.16, m["Stein"], closed=True, sides=5))
    elif element == "Luft":
        parts.append(tube("Zeichen", ring_points(cx, cy, z, 0.55 * size, 16), 0.16, m["Stein"], closed=True, sides=5))
    else:
        points = [(cx - 0.5 * size, cy - 0.5 * size, z), (cx + 0.5 * size, cy - 0.5 * size, z), (cx + 0.5 * size, cy + 0.5 * size, z), (cx - 0.5 * size, cy + 0.5 * size, z)]
        parts.append(tube("Zeichen", points, 0.16, m["Stein"], closed=True, sides=5))


def seal(parts, m, cx, cy, z, up):
    """Seal in the middle of the front cover: rings, the four element signs and a stone as the eye"""
    parts.append(disc("Siegelplatte", cx, cy, z, 8.4, 0.18, m["Silber"], up))
    top = z + (0.22 if up else -0.22)
    parts.append(tube("SiegelRing", ring_points(cx, cy, top, 8.4), 0.34, m["Silber"], closed=True))
    parts.append(tube("SiegelRingInnen", ring_points(cx, cy, top, 3.0, 24), 0.2, m["Silber"], closed=True))
    parts.append(stone("SiegelStein", cx, cy, top, 1.8, m["Stein"], up))
    for name, angle in (("Feuer", 90.0), ("Luft", 0.0), ("Wasser", 270.0), ("Erde", 180.0)):
        radians = math.radians(angle)
        element_sign(parts, m, cx + 5.6 * math.cos(radians), cy + 5.6 * math.sin(radians), top, name, 1.9)

    # Silver tendrils from the seal to the corners
    for sign_y in (-1.0, 1.0):
        for sign_x in (-1.0, 1.0):
            points = []
            for step in range(13):
                t = step / 12.0
                curl = math.sin(t * math.pi) * 1.6
                points.append((cx + sign_x * (6.2 + 4.0 * t - curl * 0.3), cy + sign_y * (6.0 + 8.0 * t + curl), z))
            parts.append(tube("Ranke", points, [0.22 * (1.0 - 0.7 * i / 12) for i in range(13)], m["Silber"], sides=5))


def clasp(parts, m, z, up):
    """Silver band over the outer edge of the front cover, where the book would be locked"""
    y = 0.0
    points = [(BOARD_WIDTH - 5.5, y, z), (BOARD_WIDTH - 0.4, y, z)]
    parts.append(tube("Spange", points, 0.7, m["Silber"], sides=8))
    parts.append(stone("SpangeStein", BOARD_WIDTH - 4.6, y, z + (0.4 if up else -0.4), 0.9, m["Stein"], up))


def join(parts, name, mode):
    bpy.ops.object.select_all(action="DESELECT")
    for part in parts:
        part.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    joined = bpy.context.view_layer.objects.active
    joined.name = name
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return joined


def build_front(m):
    """Front cover: inside faces -Z, the seal looks up +Z: it lies on the book when closed"""
    z = BOARD_THICKNESS
    parts = [board("Deckel", False, m["Leder"])]
    middle = BOARD_WIDTH * 0.5
    seal(parts, m, middle, 0.0, z, True)
    clasp(parts, m, z, True)
    for sign_x, x in ((1.0, BOARD_WIDTH - 0.4), (-1.0, 0.8)):
        for sign_y in (-1.0, 1.0):
            corner_fitting(parts, m, x, sign_y * (BOARD_HEIGHT / 2.0 - 0.4), sign_x, sign_y, z, True)
    return join(parts, "SM_Grimoire_Vorderdeckel", m)


def build_back(m):
    """Back cover: inside faces +Z, the outside looks down"""
    z = -BOARD_THICKNESS
    parts = [board("Deckel", True, m["Leder"])]
    for sign_x, x in ((1.0, BOARD_WIDTH - 0.4), (-1.0, 0.8)):
        for sign_y in (-1.0, 1.0):
            corner_fitting(parts, m, x, sign_y * (BOARD_HEIGHT / 2.0 - 0.4), sign_x, sign_y, z, False)
    return join(parts, "SM_Grimoire_Rueckdeckel", m)


def build_spine(m):
    """Spine from the hinge at x = 0 to x = -width, with raised silver bands; its inside faces +Z"""
    bm = bmesh.new()
    columns = 8
    rows = []
    for column in range(columns + 1):
        t = column / columns
        x = -SPINE_WIDTH * t
        # Curved outwards
        z = -BOARD_THICKNESS - 0.7 * math.sin(t * math.pi)
        rows.append([bm.verts.new((x, -BOARD_HEIGHT / 2.0, z)), bm.verts.new((x, BOARD_HEIGHT / 2.0, z))])
    for column in range(columns):
        a, b = rows[column], rows[column + 1]
        bm.faces.new((a[0], a[1], b[1], b[0]))
    uv_layer = bm.loops.layers.uv.new("UV")
    for face in bm.faces:
        for loop in face.loops:
            loop[uv_layer].uv = (loop.vert.co.x / BOARD_WIDTH, loop.vert.co.y / BOARD_HEIGHT + 0.5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh_data = bpy.data.meshes.new("Ruecken")
    bm.to_mesh(mesh_data)
    bm.free()
    for polygon in mesh_data.polygons:
        if polygon.normal.z > 0.0:
            polygon.flip()
    parts = [finish(mesh_data, "Ruecken", m["Leder"], smooth=True)]

    for y in (-14.0, -5.0, 5.0, 14.0):
        points = []
        for column in range(columns + 1):
            t = column / columns
            points.append((-SPINE_WIDTH * t, y, -BOARD_THICKNESS - 0.75 * math.sin(t * math.pi) - 0.05))
        parts.append(tube("Band", points, 0.42, m["Silber"], sides=6))
    return join(parts, "SM_Grimoire_Ruecken", m)


def block_height(share):
    """Top of the pages across the book: low in the gutter, swelling and then flattening to the edge"""
    return BLOCK_THICKNESS * (0.42 + 0.58 * math.sin(min(share * 2.4, 1.0) * math.pi * 0.5)) * (1.0 - 0.12 * share)


def build_block(m):
    """One half of the pages, from the gutter at x = 0 to the fore edge; top is the open page, the three sides show the striped edge"""
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UV")
    columns = 14
    half = PAGE_HEIGHT / 2.0
    tops = []
    bottoms = []
    for column in range(columns + 1):
        t = column / columns
        x = PAGE_WIDTH * t
        z = block_height(t)
        tops.append((bm.verts.new((x, -half, z)), bm.verts.new((x, half, z))))
        bottoms.append((bm.verts.new((x, -half, 0.02)), bm.verts.new((x, half, 0.02))))

    def page_uv(vert):
        return (vert.co.x / PAGE_WIDTH, EDGE_SHARE + (vert.co.y + half) / PAGE_HEIGHT * (1.0 - EDGE_SHARE))

    def edge_uv(along, height):
        return (along, height * EDGE_SHARE)

    for column in range(columns):
        a, b = tops[column], tops[column + 1]
        face = bm.faces.new((a[0], b[0], b[1], a[1]))
        for loop in face.loops:
            loop[uv_layer].uv = page_uv(loop.vert)

        # Head and tail of the block
        for side in (0, 1):
            top_a, top_b = tops[column][side], tops[column + 1][side]
            low_a, low_b = bottoms[column][side], bottoms[column + 1][side]
            quad = (low_a, low_b, top_b, top_a) if side == 0 else (top_a, top_b, low_b, low_a)
            face = bm.faces.new(quad)
            for loop in face.loops:
                loop[uv_layer].uv = edge_uv(loop.vert.co.x / PAGE_WIDTH, 1.0 if loop.vert.co.z > 0.05 and loop.vert in (top_a, top_b) else 0.0)

    # Fore edge
    face = bm.faces.new((bottoms[columns][0], bottoms[columns][1], tops[columns][1], tops[columns][0]))
    for loop in face.loops:
        loop[uv_layer].uv = edge_uv(loop.vert.co.y / PAGE_HEIGHT + 0.5, 1.0 if loop.vert in tops[columns] else 0.0)

    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh_data = bpy.data.meshes.new("Block")
    bm.to_mesh(mesh_data)
    bm.free()
    return finish(mesh_data, "SM_Grimoire_Block", m["Seite"])


# ---------------------------------------------------------------- Textures

def noise(height, width, cell, seed):
    """Smooth value noise between 0 and 1"""
    rng = np.random.default_rng(seed)
    grid = rng.random((height // cell + 3, width // cell + 3))
    ys = np.linspace(0, grid.shape[0] - 2.001, height)
    xs = np.linspace(0, grid.shape[1] - 2.001, width)
    yi, xi = ys.astype(int), xs.astype(int)
    yf, xf = ys - yi, xs - xi
    yf, xf = yf * yf * (3 - 2 * yf), xf * xf * (3 - 2 * xf)
    top = grid[yi][:, xi] * (1 - xf) + grid[yi][:, xi + 1] * xf
    low = grid[yi + 1][:, xi] * (1 - xf) + grid[yi + 1][:, xi + 1] * xf
    return top * (1 - yf)[:, None] + low * yf[:, None]


def fractal(height, width, seed):
    return sum(noise(height, width, cell, seed + index) * weight for index, (cell, weight) in enumerate(((180, 0.5), (70, 0.28), (24, 0.14), (8, 0.08))))


def save(path, pixels):
    """pixels: height x width x 4 with row 0 on top"""
    height, width = pixels.shape[:2]
    image = bpy.data.images.new(os.path.basename(path), width, height, alpha=True)
    image.colorspace_settings.name = "sRGB"
    image.pixels.foreach_set(np.clip(pixels[::-1], 0.0, 1.0).astype(np.float32).ravel())
    image.filepath_raw = path
    image.file_format = "PNG"
    image.save()


def line_mask(distance, width):
    return np.clip(1.0 - np.abs(distance) / width, 0.0, 1.0)


def paint_page(path):
    """Parchment, in alpha the signs that glow: a seal of rings and spokes, four signs, rows of runes. The bottom strip is the striped edge of the pages."""
    size_x, size_y = 1024, 1440
    edge_rows = int(size_y * EDGE_SHARE)
    page_rows = size_y - edge_rows

    grain = fractal(size_y, size_x, 5)
    fibre = noise(size_y, size_x, 3, 90)
    paper = np.array((0.74, 0.58, 0.36)) * (0.72 + 0.48 * grain[..., None]) + np.array((0.04, 0.03, 0.02)) * (fibre[..., None] - 0.5)

    # Worn darker towards the edges of the page
    ys, xs = np.mgrid[0:size_y, 0:size_x].astype(np.float32)
    u = xs / size_x
    v = ys / page_rows
    border = np.minimum(np.minimum(u, 1 - u), np.minimum(v, 1 - v))
    paper *= (0.68 + 0.32 * np.clip(border / 0.09, 0.0, 1.0))[..., None]

    # Glowing signs in units of the page width, around the middle of the upper page
    cx, cy = 0.5, 0.42
    dx = (xs - cx * size_x) / size_x
    dy = (ys - cy * page_rows) / size_x
    radius = np.hypot(dx, dy)
    angle = np.arctan2(dy, dx)
    glow = np.zeros((size_y, size_x), dtype=np.float32)
    for ring, width in ((0.36, 0.0055), (0.325, 0.0035), (0.17, 0.0045)):
        glow = np.maximum(glow, line_mask(radius - ring, width))

    # Spokes between the rings and ticks around the outside
    spokes = line_mask(np.sin(angle * 4.0) * radius, 0.004) * ((radius > 0.17) & (radius < 0.325))
    ticks = line_mask(np.sin(angle * 36.0) * radius, 0.0035) * ((radius > 0.325) & (radius < 0.36))
    glow = np.maximum(glow, np.maximum(spokes, ticks))

    # A four-pointed star in the middle, one point for each element
    star = np.sqrt(np.abs(dx)) + np.sqrt(np.abs(dy)) - math.sqrt(0.15)
    glow = np.maximum(glow, line_mask(star, 0.012))
    glow = np.maximum(glow, np.clip(1.0 - radius / 0.03, 0.0, 1.0))

    # Rows of runes above and below the seal: short strokes at random slants
    rng = np.random.default_rng(11)
    for row_v in (0.075, 0.115, 0.82, 0.86, 0.9):
        x = 0.1
        while x < 0.9:
            stroke = rng.uniform(0.006, 0.02)
            slant = rng.uniform(-0.6, 0.6)
            for bar in range(rng.integers(1, 4)):
                bar_x = x + bar * 0.011
                px = int(bar_x * size_x)
                py = int(row_v * page_rows)
                reach = int(stroke * size_x)
                for step in range(-reach, reach + 1):
                    gx = px + int(step * slant)
                    gy = py + step
                    if 0 <= gy < size_y and 0 <= gx + 2 < size_x:
                        glow[gy - 1:gy + 2, gx:gx + 3] = 1.0
            x += rng.uniform(0.022, 0.05)

    # Ink under the glow, so the signs read as written even when dark
    ink = np.clip(glow * 1.4, 0.0, 1.0)[..., None]
    paper = paper * (1.0 - ink) + np.array((0.08, 0.05, 0.05)) * ink

    # Striped edge: thin lines of pages along the strip, rows on rows
    stripes = 0.62 + 0.38 * noise(edge_rows, size_x, 2, 44)[:, :1]
    rows = np.sin(np.linspace(0.0, 60.0 * math.pi, edge_rows))[:, None] * 0.12 + 0.88
    paper[page_rows:] = np.array((0.7, 0.57, 0.38)) * (stripes * rows)[..., None]
    glow[page_rows:] = 0.0

    save(path, np.dstack((paper, glow)))


def paint_leather(path):
    """Deep blue leather, its grain and worn lighter spots, a row of pale stitches along the edge in alpha"""
    size = 512
    grain = noise(size, size, 3, 7) * 0.6 + noise(size, size, 9, 8) * 0.4
    wear = fractal(size, size, 21)
    leather = np.array((0.035, 0.075, 0.22)) * (0.7 + 0.6 * grain[..., None]) + np.array((0.05, 0.06, 0.09)) * np.clip(wear - 0.55, 0.0, 1.0)[..., None]

    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32) / size
    border = np.minimum(np.minimum(xs, 1 - xs), np.minimum(ys, 1 - ys))
    along = np.where(np.minimum(xs, 1 - xs) < np.minimum(ys, 1 - ys), ys, xs)
    stitch = line_mask(border - 0.03, 0.004) * (np.sin(along * math.pi * 70.0) > 0.0)
    leather = leather * (1.0 - stitch[..., None]) + np.array((0.62, 0.55, 0.42)) * stitch[..., None]

    save(path, np.dstack((leather, np.ones((size, size)))))


# ---------------------------------------------------------------- Export and preview

def export(obj, folder):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(folder, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                             mesh_smooth_type="FACE", colors_type="NONE", add_leaf_bones=False, axis_forward="-Z", axis_up="Y", use_tspace=False)
    print("exported", path)


def preview(front, back, spine, block, folder):
    """The open book from above and beside it the closed front, for a first look without Unreal"""
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in [item.identifier for item in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "CYCLES"
    scene.render.resolution_x = 1500
    scene.render.resolution_y = 800
    scene.render.filepath = os.path.join(folder, "Vorschau.png")

    copy = back.copy()
    copy.location = (6.0, 0.0, 0.0)
    bpy.context.collection.objects.link(copy)

    pages = block.copy()
    pages.location = (6.0, 0.0, 0.0)
    bpy.context.collection.objects.link(pages)

    spine.location = (6.0, 0.0, 0.0)
    front.location = (-45.0, 0.0, 0.0)

    camera = bpy.data.cameras.new("Kamera")
    camera.type = "ORTHO"
    camera.ortho_scale = 100.0
    camera_object = bpy.data.objects.new("Kamera", camera)
    camera_object.location = (0.0, -60.0, 100.0)
    camera_object.rotation_euler = (math.radians(31.0), 0.0, 0.0)
    bpy.context.collection.objects.link(camera_object)
    scene.camera = camera_object

    sun = bpy.data.lights.new("Sonne", "SUN")
    sun.energy = 4.0
    sun_object = bpy.data.objects.new("Sonne", sun)
    sun_object.rotation_euler = (math.radians(40.0), math.radians(20.0), 0.0)
    bpy.context.collection.objects.link(sun_object)
    world = bpy.data.worlds.new("Welt")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.25, 0.25, 0.28, 1.0)
    scene.world = world
    bpy.ops.render.render(write_still=True)


def main():
    folder = os.path.abspath(out_folder())
    os.makedirs(folder, exist_ok=True)
    reset()
    m = materials()
    front = build_front(m)
    back = build_back(m)
    spine = build_spine(m)
    block = build_block(m)
    for part in (front, back, spine, block):
        export(part, folder)
    paint_page(os.path.join(folder, "T_Grimoire_Seite.png"))
    paint_leather(os.path.join(folder, "T_Grimoire_Leder.png"))
    preview(front, back, spine, block, folder)
    print("Grimoire built in", folder)


main()
