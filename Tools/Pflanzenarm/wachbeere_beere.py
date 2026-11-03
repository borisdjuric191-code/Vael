"""
Pflanzenarm Wachbeere: the eye berry on the fingertips of the Floromant's Wachbeere arm (SM_Wachbeere_Kopf), thrown as the
Beerenwurf and opening on the ground as a blinking eye (AVaelEyeBerry). The berries of the Tripo shrub are welded into its
leaves, so the berry is grown here: a plump, gently uneven drop, 10 cm long, standing on +Z with its pivot at the stalk end;
the eye looks along +Z. UV v runs from the stalk end (0) to the eye (1) and u around it, so M_Wachbeere_Beere paints the ombre
(turquoise at the stalk, magenta, raspberry) and the eye with its lid from v alone.
Run:
  blender -b -P wachbeere_beere.py -- <output folder>
"""
import bpy, bmesh, sys, os, math, random
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
OUT = args[0]
LENGTH = 0.10      # m
RADIUS = 0.042     # m
RINGS, SIDES = 18, 20

os.makedirs(OUT, exist_ok=True)
random.seed(11)


def berry(name):
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UVMap")
    phase = random.uniform(0.0, math.tau)
    rings = []
    for i in range(1, RINGS):
        v = i / RINGS
        angle = v * math.pi
        # Plumper towards the eye, a little uneven like grown fruit
        radius = RADIUS * math.sin(angle) * (0.85 + 0.25 * v)
        z = LENGTH * 0.5 * (1.0 - math.cos(angle))
        ring = []
        for s in range(SIDES):
            a = s / SIDES * math.tau
            wobble = 1.0 + 0.05 * math.sin(a * 3.0 + phase) * math.sin(angle)
            ring.append(bm.verts.new((math.cos(a) * radius * wobble, math.sin(a) * radius * wobble, z)))
        rings.append((v, ring))
    bottom = bm.verts.new((0.0, 0.0, 0.0))
    top = bm.verts.new((0.0, 0.0, LENGTH))

    def uv(s, v):
        return (s / SIDES, v)

    for k in range(len(rings) - 1):
        (v0, r0), (v1, r1) = rings[k], rings[k + 1]
        for s in range(SIDES):
            face = bm.faces.new((r0[s], r0[(s + 1) % SIDES], r1[(s + 1) % SIDES], r1[s]))
            for loop, coords in zip(face.loops, (uv(s, v0), uv(s + 1, v0), uv(s + 1, v1), uv(s, v1))):
                loop[uv_layer].uv = coords
    v_first, first = rings[0]
    v_last, last = rings[-1]
    for s in range(SIDES):
        face = bm.faces.new((first[(s + 1) % SIDES], first[s], bottom))
        for loop, coords in zip(face.loops, (uv(s + 1, v_first), uv(s, v_first), (s / SIDES, 0.0))):
            loop[uv_layer].uv = coords
        face = bm.faces.new((last[s], last[(s + 1) % SIDES], top))
        for loop, coords in zip(face.loops, (uv(s, v_last), uv(s + 1, v_last), (s / SIDES, 1.0))):
            loop[uv_layer].uv = coords
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    material = bpy.data.materials.new("Beere")
    mesh.materials.append(material)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    return obj


bpy.ops.wm.read_factory_settings(use_empty=True)
obj = berry("SM_Wachbeere_Kopf")
bpy.ops.object.select_all(action="DESELECT")
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
path = os.path.join(OUT, "SM_Wachbeere_Kopf.fbx")
bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                         mesh_smooth_type="FACE", add_leaf_bones=False)
print("WACHBEERE exported " + path, flush=True)
