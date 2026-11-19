"""
Pflanzenarm Schnappwurz (Boris 2026-11-19, without Tripo): builds one lobe of the snapping maw like a flytrap.

The lobe is a curved shell 10 units long along +Z with its hinge at the pivot: the outside bulges towards +X, the hollow inside
opens towards -X. Long thorny lashes stand along both side rims and lean over the gap, so two lobes shut like interlocking
teeth. Its colours come from the Tripo picture of the Schnappwurz (the model stays untouched): the outside takes the UVs of the
greenest olive patch of the picture, the inside the UVs of its reddest patch, the lashes a dark patch. Like every head it
carries a constant 1 in vertex colour and UV channel 1 ("fully grown" for the stalk material). The arm uses the same lobe for
both halves, the lower one turned over.

Run:
  blender -b --factory-startup -P schnappwurz_maul.py -- <tripo basecolor picture> <output folder>
Writes <output>/SM_Schnappwurz_Maul.fbx and a preview Schnappwurz_maul.png.
"""
import bpy, bmesh, sys, os, math, random

args = sys.argv[sys.argv.index("--") + 1:]
PICTURE, OUT = args[0], args[1]
os.makedirs(OUT, exist_ok=True)

LENGTH = 10.0
WIDTH = 4.2
DOME = 2.6
SHELL = 0.45
ROWS = 24
COLS = 16
LASHES = 11
LASH_LENGTH = 2.8


def log(text):
    print("MAUL " + text, flush=True)


# ---------------------------------------------------------------- colours from the picture

def find_patches(path):
    """UV centres of the reddest, the greenest olive and the darkest patch of the picture"""
    image = bpy.data.images.load(path)
    w, h = image.size
    pixels = image.pixels[:]
    step = max(1, min(w, h) // 160)
    best = {"red": [], "green": [], "dark": []}
    for y in range(0, h, step):
        for x in range(0, w, step):
            i = (y * w + x) * 4
            r, g, b = pixels[i], pixels[i + 1], pixels[i + 2]
            light = (r + g + b) / 3.0
            uv = ((x + 0.5) / w, (y + 0.5) / h)
            if r > 0.3 and r > 1.7 * g and r > 1.7 * b:
                best["red"].append((r - g, uv))
            if g > 0.18 and g > r * 0.95 and g > b * 1.3 and light < 0.6:
                best["green"].append((g - (r + b) * 0.5, uv))
            # The lashes: a dark brown, not black (black read as flat bands in the game)
            if 0.12 < light < 0.3 and r > g * 1.1 and r > b * 1.3:
                best["dark"].append((r - b, uv))
    centres = {}
    for key, found in best.items():
        found.sort(key=lambda item: item[0], reverse=True)
        top = found[: max(1, len(found) // 20)] if found else [(0.0, (0.5, 0.5))]
        # The densest cluster of the strongest pixels: the one whose neighbours are most of the others
        anchor = max(top, key=lambda a: sum(1 for b in top if abs(a[1][0] - b[1][0]) < 0.03 and abs(a[1][1] - b[1][1]) < 0.03))[1]
        near = [b[1] for b in top if abs(anchor[0] - b[1][0]) < 0.03 and abs(anchor[1] - b[1][1]) < 0.03]
        centres[key] = (sum(p[0] for p in near) / len(near), sum(p[1] for p in near) / len(near))
        log(f"{key}: {len(found)} pixels, centre {centres[key][0]:.3f},{centres[key][1]:.3f}")
    return centres


# ---------------------------------------------------------------- the lobe

def outline(u):
    """Half width of the lobe at u (0 hinge, 1 tip): narrow at the hinge, round at the tip"""
    return WIDTH * max(0.06, math.sin(math.pi * (0.06 + 0.88 * u)) ** 0.7) * (0.55 + 0.45 * min(1.0, u * 3.0))


def bulge(u, v):
    """How far the shell stands out at (u, v), v from -1 to 1 across"""
    return DOME * (1.0 - v * v) * math.sin(math.pi * min(1.0, 0.1 + u)) ** 0.6


def build(centres):
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UVMap")
    outer, inner = [], []
    for row in range(ROWS + 1):
        u = row / ROWS
        out_row, in_row = [], []
        for col in range(COLS + 1):
            v = -1.0 + 2.0 * col / COLS
            y = v * outline(u)
            z = u * LENGTH
            x = bulge(u, v)
            out_row.append(bm.verts.new((x, y, z)))
            # The inside lies a shell's thickness within, meeting the outside at the rim
            in_row.append(bm.verts.new((max(0.0, x - SHELL * (1.0 - v * v) ** 0.5), y * 0.97, z)))
        outer.append(out_row)
        inner.append(in_row)

    def face(verts, patch, spread, coords):
        try:
            f = bm.faces.new(verts)
        except ValueError:
            return None
        cu, cv = centres[patch]
        for loop, (a, b) in zip(f.loops, coords):
            loop[uv_layer].uv = (cu + (a - 0.5) * spread, cv + (b - 0.5) * spread)
        return f

    for row in range(ROWS):
        for col in range(COLS):
            a, b = row / ROWS, (row + 1) / ROWS
            c, d = col / COLS, (col + 1) / COLS
            # Outside: olive green, normals out (+X)
            face([outer[row][col], outer[row][col + 1], outer[row + 1][col + 1], outer[row + 1][col]], "green", 0.05,
                 [(a, c), (a, d), (b, d), (b, c)])
            # Inside: red, normals in (-X), wound the other way
            face([inner[row][col], inner[row + 1][col], inner[row + 1][col + 1], inner[row][col + 1]], "red", 0.04,
                 [(a, c), (b, c), (b, d), (a, d)])
    # The rim between outside and inside along both sides and round the tip
    for row in range(ROWS):
        for col in (0, COLS):
            face([outer[row][col], outer[row + 1][col], inner[row + 1][col], inner[row][col]], "green", 0.02,
                 [(0, 0), (1, 0), (1, 1), (0, 1)])
    for col in range(COLS):
        face([outer[ROWS][col], outer[ROWS][col + 1], inner[ROWS][col + 1], inner[ROWS][col]], "green", 0.02,
             [(0, 0), (1, 0), (1, 1), (0, 1)])

    # Lashes: long thorns along both rims, leaning out, forward and over the gap (-X), the longest at the middle
    random.seed(5)
    for side in (-1, 1):
        for index in range(LASHES):
            u = 0.14 + 0.8 * index / (LASHES - 1)
            v = side * 1.0
            root = (bulge(u, v) * 0.2, v * outline(u), u * LENGTH)
            length = LASH_LENGTH * (0.6 + 0.4 * math.sin(math.pi * u)) * random.uniform(0.85, 1.1)
            way = (-0.55, side * 0.75, 0.35)
            norm = math.sqrt(sum(c * c for c in way))
            way = tuple(c / norm for c in way)
            tip = tuple(root[i] + way[i] * length for i in range(3))
            # A thin four-sided cone
            radius = 0.09
            side_a = (0.0, 0.0, 1.0) if abs(way[2]) < 0.9 else (1.0, 0.0, 0.0)
            ax = (way[1] * side_a[2] - way[2] * side_a[1], way[2] * side_a[0] - way[0] * side_a[2], way[0] * side_a[1] - way[1] * side_a[0])
            an = math.sqrt(sum(c * c for c in ax)) or 1.0
            ax = tuple(c / an for c in ax)
            bx = (way[1] * ax[2] - way[2] * ax[1], way[2] * ax[0] - way[0] * ax[2], way[0] * ax[1] - way[1] * ax[0])
            base = []
            for k in range(4):
                angle = k * math.pi / 2.0
                base.append(bm.verts.new(tuple(root[i] + (ax[i] * math.cos(angle) + bx[i] * math.sin(angle)) * radius for i in range(3))))
            top = bm.verts.new(tip)
            for k in range(4):
                face([base[k], base[(k + 1) % 4], top], "dark", 0.01, [(0, 0), (1, 0), (0.5, 1)])

    mesh = bpy.data.meshes.new("SM_Schnappwurz_Maul")
    # The shell is closed (outside, inside and rim): all faces wound outwards, so the outside faces out and the inside faces
    # into the hollow (wound the wrong way the game only showed the unlit backs)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new("SM_Schnappwurz_Maul", mesh)
    bpy.context.scene.collection.objects.link(obj)

    # Constant "fully grown" in vertex colour and UV channel 1
    attr = mesh.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    for loop in mesh.loops:
        attr.data[loop.index].color = (1.0, 1.0, 1.0, 1.0)
    mesh.color_attributes.active_color = attr
    ramp = mesh.uv_layers.new(name="Verlauf")
    for loop in mesh.loops:
        ramp.data[loop.index].uv = (1.0, 1.0)
    for poly in mesh.polygons:
        poly.use_smooth = True
    # Exported in metres like the other heads: 10 units = 0.1 m
    obj.scale = (0.01, 0.01, 0.01)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    log(f"lobe: {len(mesh.polygons)} faces")
    return obj


def material(obj, path):
    mat = bpy.data.materials.new("Schnappwurz_Maul")
    mat.use_nodes = True
    tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(path, check_existing=True)
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    obj.data.materials.append(mat)


def export(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    path = os.path.join(OUT, "SM_Schnappwurz_Maul.fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                             mesh_smooth_type="FACE", colors_type="LINEAR", path_mode="COPY", embed_textures=False, add_leaf_bones=False)
    log(f"exported {path}")


def preview(obj):
    scene = bpy.context.scene
    lower = obj.copy()
    lower.data = obj.data
    scene.collection.objects.link(lower)
    # The pair as the arm shows it, a little open: the upper one turned up about the hinge, the lower one turned over and down
    obj.rotation_euler = (0.0, math.radians(-14), 0.0)
    lower.rotation_euler = (0.0, math.radians(14), math.radians(180))
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
    scene.render.resolution_x = 700
    scene.render.resolution_y = 500
    scene.render.film_transparent = True
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    # Looking into the open maw from the front and a little to the side
    cam.location = (-0.1, -0.16, 0.2)
    target = bpy.data.objects.new("Target", None)
    target.location = (0.0, 0.0, 0.05)
    scene.collection.objects.link(target)
    track = cam.constraints.new("TRACK_TO")
    track.target = target
    track.track_axis = "TRACK_NEGATIVE_Z"
    track.up_axis = "UP_Y"
    cam.data.lens = 45
    scene.camera = cam
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 4.0
    sun.rotation_euler = (math.radians(40), math.radians(10), math.radians(30))
    scene.collection.objects.link(sun)
    world = bpy.data.worlds.new("W")
    world.color = (0.4, 0.4, 0.42)
    scene.world = world
    scene.render.filepath = os.path.join(OUT, "Schnappwurz_maul.png")
    bpy.ops.render.render(write_still=True)


bpy.ops.wm.read_factory_settings(use_empty=True)
patches = find_patches(PICTURE)
lobe = build(patches)
material(lobe, PICTURE)
export(lobe)
preview(lobe)
log("done")
