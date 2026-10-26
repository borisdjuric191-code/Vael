"""
Pflanzenarm Funkenmohn: builds the petal of the poppy bud the hand becomes, and the seed capsule it throws
(Boris 2026-10-26: the whole hand is a closed bud; held, the arm is a mortar that throws glowing capsules in an arc).

The Tripo model of the Funkenmohn is one welded piece, so its petals and capsules cannot be cut out cleanly. They are built here
and take their colours from the model's own picture: fixed spots of the picture that show the red-orange petals, the ribbed
green of the buds and the golden star of the blossom's heart.

- SM_Funkenmohn_Bluetenblatt: a broad, crinkled poppy petal growing from a narrow foot at the pivot up along +Z, 10 cm high,
  cupped so its inner side faces -X (the middle of the bud), like the petal of the Taublatt rose.
- SM_Funkenmohn_Kapsel: the seed capsule, an egg with eight ribs and the flat star crown of a poppy head on top, 10 cm high,
  pivot at its foot.
Ramp 0.97 everywhere (the material treats them as the tip, so they glow and flash).

Run:
  blender -b -P funkenmohn_knospe.py -- <source fbx> <output folder (absolute)>
Writes both FBX plus a preview image <output>/Funkenmohn_knospe.png.
"""
import bpy, bmesh, sys, os, math
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
SOURCE, OUT = args[0], os.path.abspath(args[1])
NAME = "Funkenmohn"

HEIGHT = 0.10   # m

# Spots of the picture (u, v of Blender, v up) and their size: a large red-orange petal, the ribbed green of a bud, the golden
# star in the heart of the blossom
PETAL_UV = (Vector((0.50, 0.865)), Vector((0.10, 0.09)))
CAPSULE_UV = (Vector((0.455, 0.69)), Vector((0.07, 0.05)))
STAR_UV = (Vector((0.409, 0.495)), Vector((0.03, 0.03)))

os.makedirs(OUT, exist_ok=True)


def log(text):
    print("KNOSPE " + text, flush=True)


def source_material():
    bpy.ops.import_scene.fbx(filepath=SOURCE)
    model = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
    material = model.data.materials[0] if model.data.materials else None
    bpy.data.objects.remove(model)
    return material


def uv_in(region, a, b):
    centre, spread = region
    return (centre.x + (a - 0.5) * spread.x, centre.y + (b - 0.5) * spread.y)


def finish(bm, mesh, material):
    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()
    if material is not None:
        mesh.materials.append(material)
    obj = bpy.data.objects.new(mesh.name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    colours = mesh.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    ramp_uv = mesh.uv_layers.new(name="Verlauf")
    for loop in mesh.loops:
        colours.data[loop.index].color = (0.97, 0.97, 0.97, 1.0)
        ramp_uv.data[loop.index].uv = (0.97, 0.97)
    log(f"{obj.name}: {len(mesh.polygons)} faces")
    return obj


def grid_faces(bm, uv_layer, grid, region):
    rows, columns = len(grid) - 1, len(grid[0]) - 1
    for r in range(rows):
        for c in range(columns):
            face = bm.faces.new((grid[r][c], grid[r][c + 1], grid[r + 1][c + 1], grid[r + 1][c]))
            for k, loop in enumerate(face.loops):
                row_index = r + (1 if k >= 2 else 0)
                column_index = c + (1 if k in (1, 2) else 0)
                loop[uv_layer].uv = uv_in(region, column_index / columns, row_index / rows)


def build_petal(material):
    """Broad like a fan, wider at the top than high, with the soft crinkles of poppy silk; the rim waves"""
    mesh = bpy.data.meshes.new(f"SM_{NAME}_Bluetenblatt")
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UVMap")
    rows, columns = 18, 16
    h = HEIGHT
    grid = []
    for r in range(rows + 1):
        t = r / rows
        # Narrow foot, widest at two thirds, round top
        half = h * max(0.05, 0.58 * math.sqrt(max(0.0, 1.0 - ((t - 0.66) / 0.68) ** 2)))
        row = []
        for c in range(columns + 1):
            s = c / columns * 2.0 - 1.0
            # Cupped towards -X, the sides curling in more towards the top; crinkles run up from the foot like silk folds
            cup = -0.26 * h * s * s * (0.3 + 0.7 * t)
            crinkle = 0.022 * h * math.sin(s * 7.0 + t * 3.0) * t + 0.012 * h * math.sin(s * 15.0 - t * 6.0) * t * t
            rim = 0.03 * h * math.sin(s * 9.0) * t ** 4
            x = cup + crinkle + rim + 0.05 * h * math.sin(t * math.pi)
            z = t * h - 0.12 * h * s * s * t ** 3
            row.append(bm.verts.new((x, s * half, z)))
        grid.append(row)
    grid_faces(bm, uv_layer, grid, PETAL_UV)
    return finish(bm, mesh, material)


def build_capsule(material):
    """An egg with eight ribs on a short neck, and the star crown of a poppy head on top"""
    mesh = bpy.data.meshes.new(f"SM_{NAME}_Kapsel")
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UVMap")
    rings, sides = 16, 32
    body = HEIGHT * 0.82
    ribs = 8

    def radius(t, angle):
        # Thin neck at the foot, belly at 55 %, a little waist under the crown
        shape = math.sin(math.pi * min(1.0, 0.08 + t * 0.92)) ** 0.75
        r = HEIGHT * 0.34 * max(0.18, shape) * (1.0 - 0.18 * max(0.0, t - 0.85) / 0.15)
        return r * (1.0 + 0.07 * math.cos(angle * ribs))

    grid = []
    for r in range(rings + 1):
        t = r / rings
        row = []
        for c in range(sides + 1):
            angle = 2.0 * math.pi * c / sides
            rr = radius(t, angle)
            row.append(bm.verts.new((math.cos(angle) * rr, math.sin(angle) * rr, t * body)))
        grid.append(row)
    for row in grid:
        bm.verts.remove(row[-1])
        row[-1] = row[0]
    grid_faces(bm, uv_layer, grid, CAPSULE_UV)

    # The foot closed
    foot = bm.verts.new((0.0, 0.0, 0.0))
    for c in range(sides):
        face = bm.faces.new((foot, grid[0][c + 1], grid[0][c]))
        for loop in face.loops:
            loop[uv_layer].uv = uv_in(CAPSULE_UV, 0.5, 0.0)

    # The crown: a flat star of rays from a raised middle out over the rim of the egg
    top = grid[rings]
    middle = bm.verts.new((0.0, 0.0, body + HEIGHT * 0.07))
    rim_r = radius(1.0, 0.0) * 1.25
    star = []
    for c in range(sides):
        angle = 2.0 * math.pi * c / sides
        ray = 0.5 + 0.5 * math.cos(angle * ribs)
        rr = rim_r * (0.75 + 0.45 * ray)
        star.append(bm.verts.new((math.cos(angle) * rr, math.sin(angle) * rr, body + HEIGHT * (0.03 + 0.05 * ray))))
    for c in range(sides):
        n = (c + 1) % sides
        face = bm.faces.new((top[c], top[n], star[n], star[c]))
        for loop in face.loops:
            loop[uv_layer].uv = uv_in(CAPSULE_UV, 0.5, 0.95)
        face = bm.faces.new((star[c], star[n], middle))
        for k, loop in enumerate(face.loops):
            a = 2.0 * math.pi * (c + (1 if k == 1 else 0)) / sides
            loop[uv_layer].uv = uv_in(STAR_UV, 0.5, 0.5) if k == 2 else uv_in(STAR_UV, 0.5 + 0.5 * math.cos(a), 0.5 + 0.5 * math.sin(a))
    return finish(bm, mesh, material)


def preview(objects):
    scene = bpy.context.scene
    for index, obj in enumerate(objects):
        obj.scale = (6.0, 6.0, 6.0)
        obj.location = (index * 0.9 - 0.45, 0.0, 0.0)
        obj.rotation_euler = (0.0, 0.0, math.radians(90.0) if index == 0 else 0.0)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    cam.location = (0.0, -2.0, 0.9)
    cam.rotation_euler = (math.radians(68), 0, 0)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 1.8
    scene.camera = cam
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 900, 600
    scene.render.filepath = os.path.join(OUT, NAME + "_knospe.png")
    bpy.ops.render.render(write_still=True)
    for obj in objects:
        obj.scale = (1.0, 1.0, 1.0)
        obj.location = (0.0, 0.0, 0.0)
        obj.rotation_euler = (0.0, 0.0, 0.0)


def export(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                             mesh_smooth_type="FACE", colors_type="LINEAR", path_mode="COPY", embed_textures=False, add_leaf_bones=False)
    log(f"exported {path}")


bpy.ops.wm.read_factory_settings(use_empty=True)
material = source_material()
petal = build_petal(material)
capsule = build_capsule(material)
preview([petal, capsule])
export(petal)
export(capsule)
log("done")
