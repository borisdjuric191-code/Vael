"""
Feuer-Baukasten: the body of a single flame tongue as a 3D mesh (Boris 2026-10-22: clean drawn flames, calm, seen from every
side; the simulated volumes looked like clouds). A round belly near the foot, tapering to a point, bent a little and twisted,
like the tongues of a drawn fire. The material (M_Feuer_Zunge) makes it lick, sway and tear apart at its tip.

Three variants, 100 cm high, about 30 cm wide at the belly, the foot at the origin:
  SM_Feuer_Zunge_0  upright, full
  SM_Feuer_Zunge_1  slim, bent to one side
  SM_Feuer_Zunge_2  short and broad, with an S-bend
UV0: u around the tongue (0..1), v from the foot (0) to the tip (1).

Run: blender -b -P feuer_zunge.py -- <output folder>
"""
import bpy, sys, os, math

OUT = sys.argv[sys.argv.index("--") + 1]
os.makedirs(OUT, exist_ok=True)

RINGS = 28
SIDES = 24

VARIANTS = [
    # name, height (m), belly radius, where the belly is (share of the height), bend (m at the tip), S-bend, twist
    ("SM_Feuer_Zunge_0", 1.0, 0.16, 0.22, 0.05, 0.0, 0.6),
    ("SM_Feuer_Zunge_1", 1.0, 0.11, 0.18, 0.18, 0.0, 0.9),
    ("SM_Feuer_Zunge_2", 1.0, 0.19, 0.28, 0.06, 0.08, 0.4),
]


def radius_at(h, belly, belly_at):
    """Round at the foot, widest at the belly, then thinning to a sharp tip"""
    if h <= belly_at:
        return belly * (0.55 + 0.45 * math.sin(h / belly_at * math.pi / 2))
    rest = (h - belly_at) / (1.0 - belly_at)
    return belly * (1.0 - rest) ** 1.15 * (1.0 + 0.12 * math.sin(rest * math.pi))


def build(name, height, belly, belly_at, bend, s_bend, twist):
    vertices, faces, uvs = [], [], []
    for ring in range(RINGS + 1):
        h = ring / RINGS
        r = radius_at(h, belly, belly_at)
        # The tongue leans over a little and swings back near its tip
        cx = bend * h * h + s_bend * math.sin(h * math.pi * 2.0)
        cy = s_bend * 0.5 * math.sin(h * math.pi)
        for side in range(SIDES + 1):
            a = side / SIDES * math.tau + twist * h
            # Slightly flattened across, like a real tongue of fire
            vertices.append((cx + math.cos(a) * r, cy + math.sin(a) * r * 0.8, h * height))
    for ring in range(RINGS):
        for side in range(SIDES):
            a = ring * (SIDES + 1) + side
            b = a + SIDES + 1
            faces.append((a, a + 1, b + 1, b))
    # A cap under the foot, so the shell is closed where it sits in the ground
    bottom = len(vertices)
    vertices.append((0.0, 0.0, -0.02))
    for side in range(SIDES):
        faces.append((bottom, side + 1, side))

    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    layer = mesh.uv_layers.new(name="UVMap")
    for poly in mesh.polygons:
        for loop_index in poly.loop_indices:
            index = mesh.loops[loop_index].vertex_index
            if index == bottom:
                layer.data[loop_index].uv = (0.5, 0.0)
            else:
                layer.data[loop_index].uv = ((index % (SIDES + 1)) / SIDES, (index // (SIDES + 1)) / RINGS)
    for poly in mesh.polygons:
        poly.use_smooth = True
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


bpy.ops.wm.read_factory_settings(use_empty=True)
for variant in VARIANTS:
    obj = build(*variant)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                             mesh_smooth_type="FACE", add_leaf_bones=False)
    print("FEUER exported", path, flush=True)
