"""
Pflanzenarm Schlotwinde: builds the golden trumpet flower that grows along the bindweed arm, and the heart-shaped leaf at the ends
of its tendrils (Boris 2026-10-27: the hand stays, bindweed winds from the wrist to the shoulder with golden trumpets that turn
forward like horns while aiming; three long tendrils from the fingers whip and lasso).

The Tripo model of the Schlotwinde is one welded bush, so its flowers and leaves cannot be cut out. They are built here and take
their colours from the model's own picture: fixed spots of the picture that show an orange-gold flower and a light green leaf.

- SM_Schlotwinde_Trichter: a trumpet, a thin green tube at the pivot flaring along +Z into a wide five-lobed rim, 10 cm high, the
  mouth facing +Z (like the funnel of the Taublatt, which the arm lays along itself the same way).
- SM_Schlotwinde_Blatt: a heart-shaped leaf on a short stalk along +Z, 10 cm high, cupped a little, the face towards -X.
Ramp 0.97 everywhere (the material treats them as the tip, so they glow and flash).

Run:
  blender -b -P schlotwinde_trichter.py -- <source fbx> <output folder (absolute)>
Writes both FBX, the stems SM_Schlotwinde_Halm_1 (bare) and _2 (with small leaves), and a preview image
<output>/Schlotwinde_trichter.png.
"""
import bpy, bmesh, sys, os, math
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
SOURCE, OUT = args[0], os.path.abspath(args[1])
NAME = "Schlotwinde"

HEIGHT = 0.10   # m

# Spots of the picture (u, v of Blender, v up) and their size: an orange-gold flower with its darker heart, a light green leaf
FLOWER_UV = (Vector((0.818, 0.591)), Vector((0.06, 0.05)))
LEAF_UV = (Vector((0.616, 0.487)), Vector((0.035, 0.035)))
STEM_UV = (Vector((0.30, 0.30)), Vector((0.02, 0.02)))

os.makedirs(OUT, exist_ok=True)


def log(text):
    print("TRICHTER " + text, flush=True)


def source_material():
    bpy.ops.import_scene.fbx(filepath=SOURCE)
    model = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
    material = model.data.materials[0] if model.data.materials else None
    bpy.data.objects.remove(model)
    return material


def uv_in(region, a, b):
    centre, spread = region
    return (centre.x + (a - 0.5) * spread.x, centre.y + (b - 0.5) * spread.y)


def finish(bm, mesh, material, length=None):
    """Ramp 0.97 everywhere, or (stalks) from 0 at the foot to 1 at the given length up +Z"""
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
        ramp = 0.97 if length is None else max(0.0, min(1.0, mesh.vertices[loop.vertex_index].co.z / length))
        colours.data[loop.index].color = (ramp, ramp, ramp, 1.0)
        ramp_uv.data[loop.index].uv = (ramp, ramp)
    log(f"{obj.name}: {len(mesh.polygons)} faces")
    return obj


def build_trumpet(material):
    """A thin tube that opens into a wide bell; the rim has five soft lobes and folds a little outwards"""
    mesh = bpy.data.meshes.new(f"SM_{NAME}_Trichter")
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UVMap")
    rings, sides = 16, 30
    h = HEIGHT
    lobes = 5

    def radius(t, angle):
        # Narrow tube for the lower half, then the bell flares out fast
        tube = 0.07 * h
        bell = 0.52 * h * t ** 3.2
        lobe = 1.0 + 0.16 * math.cos(angle * lobes) * t ** 4
        return (tube + bell) * lobe

    grid = []
    for r in range(rings + 1):
        t = r / rings
        row = []
        for c in range(sides + 1):
            angle = 2.0 * math.pi * c / sides
            rr = radius(t, angle)
            # The rim folds back a little, more at the lobes' tips
            z = t * h - 0.08 * h * t ** 6 * (0.6 + 0.4 * math.cos(angle * lobes))
            row.append(bm.verts.new((math.cos(angle) * rr, math.sin(angle) * rr, z)))
        grid.append(row)
    for row in grid:
        bm.verts.remove(row[-1])
        row[-1] = row[0]
    for r in range(rings):
        for c in range(sides):
            face = bm.faces.new((grid[r][c], grid[r][c + 1], grid[r + 1][c + 1], grid[r + 1][c]))
            for k, loop in enumerate(face.loops):
                t = (r + (1 if k >= 2 else 0)) / rings
                a = 2.0 * math.pi * (c + (1 if k in (1, 2) else 0)) / sides
                # The tube is green, the bell golden; the picture's flower spot runs from its orange heart (middle) out to the rim
                if t < 0.35:
                    loop[uv_layer].uv = uv_in(STEM_UV, 0.5 + 0.3 * math.cos(a), t / 0.35)
                else:
                    reach = (t - 0.35) / 0.65 * 0.5
                    loop[uv_layer].uv = uv_in(FLOWER_UV, 0.5 + reach * math.cos(a), 0.5 + reach * math.sin(a))

    # The foot closed
    foot = bm.verts.new((0.0, 0.0, 0.0))
    for c in range(sides):
        face = bm.faces.new((foot, grid[0][c + 1], grid[0][c]))
        for loop in face.loops:
            loop[uv_layer].uv = uv_in(STEM_UV, 0.5, 0.0)
    return finish(bm, mesh, material)


def build_leaf(material):
    """A heart: two round lobes at the foot, a pointed tip, cupped along the midrib; a short stalk below"""
    mesh = bpy.data.meshes.new(f"SM_{NAME}_Blatt")
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UVMap")
    rows, columns = 16, 14
    h = HEIGHT
    stalk = 0.18 * h
    blade = h - stalk
    grid = []
    for r in range(rows + 1):
        t = r / rows
        # Heart outline: widest near a third, round lobes at the foot (t small), long point at the tip
        half = blade * 0.5 * (math.sin(math.pi * min(1.0, t * 1.15)) ** 0.7) * (1.0 - 0.35 * t) + (0.006 if t < 0.02 else 0.0)
        row = []
        for c in range(columns + 1):
            s = c / columns * 2.0 - 1.0
            cup = -0.18 * blade * s * s * (0.4 + 0.6 * math.sin(math.pi * t))
            # The lobes reach back below the stalk's end a little
            lobe_drop = 0.10 * blade * (1.0 - t) ** 3 * abs(s)
            row.append(bm.verts.new((cup + 0.04 * blade * math.sin(t * math.pi), s * half, stalk + t * blade - lobe_drop)))
        grid.append(row)
    for r in range(rows):
        for c in range(columns):
            face = bm.faces.new((grid[r][c], grid[r][c + 1], grid[r + 1][c + 1], grid[r + 1][c]))
            for k, loop in enumerate(face.loops):
                loop[uv_layer].uv = uv_in(LEAF_UV, (c + (1 if k in (1, 2) else 0)) / columns, (r + (1 if k >= 2 else 0)) / rows)

    # The stalk: a thin square tube from the pivot up to the leaf
    w = 0.012 * h
    corners = [(w, w), (-w, w), (-w, -w), (w, -w)]
    low = [bm.verts.new((x, y, 0.0)) for x, y in corners]
    high = [bm.verts.new((x, y, stalk + 0.02 * h)) for x, y in corners]
    for i in range(4):
        n = (i + 1) % 4
        face = bm.faces.new((low[i], low[n], high[n], high[i]))
        for loop in face.loops:
            loop[uv_layer].uv = uv_in(STEM_UV, 0.5, 0.5)
    return finish(bm, mesh, material)


def build_stalk(material, index, leaves):
    """A smooth thin bindweed stem 100 cm along +Z (pivot at the foot, the arm stretches it along its spline), winding gently,
    tapering to the tip, optionally with small heart leaves on short stalks along it"""
    mesh = bpy.data.meshes.new(f"SM_{NAME}_Halm_{index}")
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UVMap")
    length = 1.0
    rings, sides = 40, 6
    grid = []
    for r in range(rings + 1):
        t = r / rings
        wind = 0.012 * math.sin(t * math.pi * 5.0 + index)
        centre = Vector((wind, 0.012 * math.cos(t * math.pi * 5.0 + index), t * length))
        rr = 0.009 * (1.0 - 0.6 * t)
        row = [bm.verts.new(centre + Vector((math.cos(2 * math.pi * c / sides) * rr, math.sin(2 * math.pi * c / sides) * rr, 0.0)))
               for c in range(sides)]
        grid.append(row)
    for r in range(rings):
        for c in range(sides):
            n = (c + 1) % sides
            face = bm.faces.new((grid[r][c], grid[r][n], grid[r + 1][n], grid[r + 1][c]))
            for k, loop in enumerate(face.loops):
                loop[uv_layer].uv = uv_in(STEM_UV, (c + (1 if k in (1, 2) else 0)) / sides, (r + (1 if k >= 2 else 0)) / rings)

    # Small heart leaves, each a flat diamond-heart on alternating sides
    for i in range(leaves):
        t = 0.2 + 0.7 * i / max(leaves - 1, 1)
        side = 1.0 if i % 2 == 0 else -1.0
        base = Vector((0.0, 0.0, t * length))
        out = Vector((side, 0.3, 0.6)).normalized()
        size = 0.05 * (1.0 - 0.4 * t)
        across = Vector((0.0, 1.0, 0.0)).cross(out).normalized()
        points = [base, base + out * size * 0.45 + across * size * 0.35, base + out * size, base + out * size * 0.45 - across * size * 0.35]
        verts = [bm.verts.new(p) for p in points]
        face = bm.faces.new(verts)
        for k, loop in enumerate(face.loops):
            loop[uv_layer].uv = uv_in(LEAF_UV, [0.5, 1.0, 0.5, 0.0][k], [0.0, 0.5, 1.0, 0.5][k])
    return finish(bm, mesh, material, length)


def preview(objects):
    scene = bpy.context.scene
    for index, obj in enumerate(objects):
        obj.scale = (6.0, 6.0, 6.0)
        obj.location = (index * 0.9 - 0.45, 0.0, 0.0)
        obj.rotation_euler = (math.radians(-35.0) if index == 0 else 0.0, 0.0, math.radians(90.0) if index == 1 else 0.0)
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
    scene.render.filepath = os.path.join(OUT, NAME + "_trichter.png")
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
trumpet = build_trumpet(material)
leaf = build_leaf(material)
preview([trumpet, leaf])
export(trumpet)
export(leaf)
# The stems (they replace what halme_zerlegen.py cuts out of other plants: this bush is too tangled for it)
export(build_stalk(material, 1, 0))
export(build_stalk(material, 2, 6))
log("done")
