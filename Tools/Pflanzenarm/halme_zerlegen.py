"""
Pflanzenarm: cuts single stalks out of a fused Tripo plant model for the Floromant's plant arms.

The Tripo mesh is one welded piece, so it is cut at a height where the stalks part, and the tallest loose pieces
above the cut become stalks: straightened along their centre line, standing on +Z with the pivot at the base,
reduced, and with a vertex colour ramp R = 0 at the base to 1 at the tip for bending in the material.
The source file is only read, never written.

Run:
  blender -b -P halme_zerlegen.py -- <source fbx> <output folder> <name> [count] [cut share] [triangles]
Writes <output>/SM_<name>_Halm_<n>.fbx plus a preview image <output>/<name>_halme.png.
"""
import bpy, bmesh, sys, os, math
from mathutils import Vector, Matrix

args = sys.argv[sys.argv.index("--") + 1:]
SOURCE, OUT, NAME = args[0], args[1], args[2]
COUNT = int(args[3]) if len(args) > 3 else 3
CUT = float(args[4]) if len(args) > 4 else 0.45
TRIANGLES = int(args[5]) if len(args) > 5 else 700
# "kopf": the stalks end in a head (Glutdistel); it is cut off into its own mesh SM_<name>_Kopf, so the arm can stretch the stalk
# without stretching the head
HEADS = len(args) > 6 and args[6] == "kopf"
HEAD_SIZE_M = 0.10
# Length of every stalk in cm; the arm stretches it along its spline anyway
LENGTH_CM = 100.0
BINS = 14

os.makedirs(OUT, exist_ok=True)


def log(text):
    print("HALME " + text, flush=True)


def load_source():
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.import_scene.fbx(filepath=SOURCE)
    obj = [o for o in bpy.context.selected_objects if o.type == "MESH"][0]
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return obj


def islands_above_cut(obj):
    """Deletes everything below the cut and returns the loose pieces as lists of vertex indices, tallest first"""
    height = obj.dimensions.z
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if sum(v.co.z for v in f.verts) / len(f.verts) < CUT * height], context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(obj.data)
    bm.free()

    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.separate(type="LOOSE")
    bpy.ops.object.mode_set(mode="OBJECT")
    pieces = [o for o in bpy.context.selected_objects if o.type == "MESH"]
    small = [p for p in pieces if len(p.data.vertices) <= 40]
    pieces = [p for p in pieces if len(p.data.vertices) > 40]
    for p in small:
        bpy.data.objects.remove(p)
    pieces.sort(key=lambda p: -p.dimensions.z)
    for p in pieces[:10]:
        log(f"piece {p.name}: {len(p.data.vertices)} verts, height {p.dimensions.z / height:.2f}")
    return pieces


def principal_axis(points):
    """Longest direction through the points, pointing upwards"""
    centre = sum(points, Vector()) / len(points)
    cov = Matrix(((0, 0, 0), (0, 0, 0), (0, 0, 0)))
    for p in points:
        d = p - centre
        for i in range(3):
            for j in range(3):
                cov[i][j] += d[i] * d[j]
    axis = Vector((0.2, 0.3, 1.0)).normalized()
    for _ in range(40):
        axis = (cov @ axis).normalized()
    return (axis if axis.z >= 0 else -axis), centre


def straighten(obj):
    """Moves every vertex across so the centre line of the stalk lies on its main axis, then stands it on +Z"""
    mesh = obj.data
    points = [obj.matrix_world @ v.co for v in mesh.vertices]
    axis, centre = principal_axis(points)

    ts = [(p - centre).dot(axis) for p in points]
    t_min, t_max = min(ts), max(ts)
    span = max(t_max - t_min, 1e-5)

    # Centre of every slice along the axis, gaps filled from the neighbours
    sums = [Vector() for _ in range(BINS)]
    counts = [0] * BINS
    for p, t in zip(points, ts):
        b = min(BINS - 1, int((t - t_min) / span * BINS))
        sums[b] += p - axis * t
        counts[b] += 1
    centres = [sums[b] / counts[b] if counts[b] else None for b in range(BINS)]
    for b in range(BINS):
        if centres[b] is None:
            near = [c for c in centres if c is not None]
            centres[b] = near[0] if near else centre

    def centre_at(t):
        x = (t - t_min) / span * BINS - 0.5
        i = max(0, min(BINS - 2, int(math.floor(x))))
        a = max(0.0, min(1.0, x - i))
        return centres[i].lerp(centres[i + 1], a)

    # Rotation of the axis onto +Z
    to_z = axis.rotation_difference(Vector((0, 0, 1))).to_matrix()
    scale = LENGTH_CM * 0.01 / span
    for v, p, t in zip(mesh.vertices, points, ts):
        straight = p - axis * t - centre_at(t) + axis * (t - t_min)
        v.co = (to_z @ straight) * scale
    obj.matrix_world = Matrix.Identity(4)
    mesh.update()

    # Base onto the origin, centred in X and Y
    zs = [v.co.z for v in mesh.vertices]
    base = [v.co for v in mesh.vertices if v.co.z < min(zs) + 0.03]
    mid = sum(base, Vector()) / len(base)
    for v in mesh.vertices:
        v.co -= Vector((mid.x, mid.y, min(zs)))
    mesh.update()


def reduce(obj):
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    if tris > TRIANGLES:
        mod = obj.modifiers.new("Reduce", "DECIMATE")
        mod.ratio = TRIANGLES / tris
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=mod.name)
    log(f"{obj.name}: {sum(len(p.vertices) - 2 for p in obj.data.polygons)} triangles")


def paint_ramp(obj, constant=None):
    """Vertex colour R (and G, B) from 0 at the base to 1 at the tip, A = 1; the same in UV channel 1. A head gets one value for all of it."""
    mesh = obj.data
    height = max(v.co.z for v in mesh.vertices) or 1.0

    def ramp(loop):
        return constant if constant is not None else max(0.0, min(1.0, mesh.vertices[loop.vertex_index].co.z / height))

    attr = mesh.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    for loop in mesh.loops:
        t = ramp(loop)
        attr.data[loop.index].color = (t, t, t, 1.0)
    mesh.color_attributes.active_color = attr

    # The same ramp in a second UV channel (U = V = ramp): UVs always come through the import, vertex colours not with every importer
    uv = mesh.uv_layers.new(name="Verlauf")
    for loop in mesh.loops:
        t = ramp(loop)
        uv.data[loop.index].uv = (t, t)


def export(obj, index):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    obj.name = f"SM_{NAME}_Halm_{index}"
    obj.data.name = obj.name
    path = os.path.join(OUT, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                             mesh_smooth_type="FACE", colors_type="LINEAR", path_mode="COPY", embed_textures=False, add_leaf_bones=False)
    log(f"exported {path}")


def preview(stalks):
    """Lines the stalks up and renders them, so they can be checked by eye"""
    for i, s in enumerate(stalks):
        s.location.x = (i - (len(stalks) - 1) / 2) * 0.35
    scene = bpy.context.scene
    cam_data = bpy.data.cameras.new("Cam")
    cam = bpy.data.objects.new("Cam", cam_data)
    scene.collection.objects.link(cam)
    cam.location = (0, -3.2, 0.55)
    cam.rotation_euler = (math.radians(90), 0, 0)
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = 1.3
    scene.camera = cam
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 800, 900
    scene.render.filepath = os.path.join(OUT, NAME + "_halme.png")
    bpy.ops.render.render(write_still=True)
    for s in stalks:
        s.location.x = 0


def slenderness(obj):
    """Widest distance from the centre line per length; a single stalk is thin, a clump of blades is wide"""
    verts = obj.data.vertices
    length = max(v.co.z for v in verts) or 1.0
    return max(math.hypot(v.co.x, v.co.y) for v in verts) / length


# Cut at a few heights, straighten every long piece and keep the thinnest ones; the longest of those come first
bpy.ops.wm.read_factory_settings(use_empty=True)
candidates = []
for cut in (CUT, CUT + 0.1, CUT + 0.2):
    CUT = cut
    source = load_source()
    full_height = source.dimensions.z
    for piece in islands_above_cut(source):
        original_height = piece.dimensions.z / full_height
        if original_height < 0.25:
            bpy.data.objects.remove(piece)
            continue
        straighten(piece)
        candidates.append((slenderness(piece), original_height, piece))

candidates.sort(key=lambda c: c[0])
for s, h, o in candidates[:12]:
    log(f"candidate slenderness {s:.3f} height {h:.2f} verts {len(o.data.vertices)}")
best = sorted(candidates[:COUNT + 2], key=lambda c: -c[1])[:COUNT]
for s, h, o in candidates:
    if all(o is not b[2] for b in best):
        bpy.data.objects.remove(o)

def split_head(obj):
    """Cuts the head off the top of a straightened stalk: the lowest slice from where on the piece stays much wider than the
    stem. The stalk keeps the rest and is stretched to its length again; the head gets its pivot at its bottom and HEAD_SIZE_M
    height. Returns the head, or None if the stalk has none."""
    mesh = obj.data
    length = max(v.co.z for v in mesh.vertices)
    slices = 24
    widths = [0.0] * slices
    for v in mesh.vertices:
        b = min(slices - 1, int(v.co.z / length * slices))
        widths[b] = max(widths[b], math.hypot(v.co.x, v.co.y))
    stem = sorted(widths[:slices // 2])[slices // 4] or 0.01

    start = None
    for b in range(slices - 1, -1, -1):
        if widths[b] > 2.5 * stem:
            start = b
        elif start is not None:
            break
    if start is None or start < slices // 3:
        return None
    cut_z = start / slices * length

    bm = bmesh.new()
    bm.from_mesh(mesh)
    head_mesh = bpy.data.meshes.new(obj.name + "_Kopf")
    head_bm = bm.copy()
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if sum(v.co.z for v in f.verts) / len(f.verts) >= cut_z], context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(mesh)
    bm.free()
    bmesh.ops.delete(head_bm, geom=[f for f in head_bm.faces if sum(v.co.z for v in f.verts) / len(f.verts) < cut_z], context="FACES")
    bmesh.ops.delete(head_bm, geom=[v for v in head_bm.verts if not v.link_faces], context="VERTS")
    head_bm.to_mesh(head_mesh)
    head_bm.free()

    # The stalk below the head stands 1 m long again
    stalk_length = max(v.co.z for v in mesh.vertices) or 1.0
    for v in mesh.vertices:
        v.co *= LENGTH_CM * 0.01 / stalk_length
    mesh.update()

    # The head: pivot at the middle of its bottom, HEAD_SIZE_M high
    head = bpy.data.objects.new(obj.name + "_Kopf", head_mesh)
    bpy.context.scene.collection.objects.link(head)
    zs = [v.co.z for v in head_mesh.vertices]
    bottom = [v.co for v in head_mesh.vertices if v.co.z < min(zs) + (max(zs) - min(zs)) * 0.15]
    mid = sum(bottom, Vector()) / len(bottom)
    size = (max(zs) - min(zs)) or 1.0
    for v in head_mesh.vertices:
        v.co = (v.co - Vector((mid.x, mid.y, min(zs)))) * (HEAD_SIZE_M / size)
    head_mesh.update()
    log(f"{obj.name}: head cut off at {cut_z / length:.2f} of the length")
    return head


stalks = []
heads = []
for s, h, obj in best:
    if HEADS:
        head = split_head(obj)
        if head is not None:
            reduce(head)
            paint_ramp(head, constant=0.97)
            heads.append(head)
    reduce(obj)
    paint_ramp(obj)
    stalks.append(obj)
preview(stalks + heads[:1])
for i, stalk in enumerate(stalks, start=1):
    export(stalk, i)
if heads:
    # The fullest head goes on the fingertips
    best_head = max(heads, key=lambda o: len(o.data.vertices))
    bpy.ops.object.select_all(action="DESELECT")
    best_head.select_set(True)
    bpy.context.view_layer.objects.active = best_head
    best_head.name = f"SM_{NAME}_Kopf"
    best_head.data.name = best_head.name
    path = os.path.join(OUT, best_head.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                             mesh_smooth_type="FACE", colors_type="LINEAR", path_mode="COPY", embed_textures=False, add_leaf_bones=False)
    log(f"exported {path}")
log("done")
