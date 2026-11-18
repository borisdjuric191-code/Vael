"""
The Glutpanzer as its own body (goal 16.11., Boris: "die Teile ersetzen den MetaHuman, angelegt so, wie Tripo sie gebaut hat").

The Tripo armour keeps its own stocky proportions (scaled x2.0 evenly) and gets a skeleton with the bone names of the body, set
at its own joints. In the game it follows the body by the rotations of the bones (UVaelPoseFollowerAnimInstance), like the
MetaHuman does, but keeps its own bone lengths. Its rest pose is turned into the rest pose of the MetaHuman (the arms at 45
degrees, the forearms forward), so the rotations carry over one to one.

Four pieces, one skeletal mesh each on the same skeleton:
  Kopf    the helmet
  Brust   chest, shoulder plates, collar and the hump above the back shell
And the domed back shell itself (Tripo part 0) as SKM_Ing_GP_Schild on a small skeleton of its own (see build_shell): the
engineer's shield, which the game rolls from the back over the left shoulder to the front.
  Arme    everything below the shoulder plates: sleeves, forearms, gauntlets
  Beine   the belt and everything below it

Weights: every vertex to the bones of its piece by its distance to their segments (1/d^4, the closest three), so the joints bend
smoothly; the helmet hangs on the head alone.

Out: C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/05_rig/SKM_Ing_GP_<Teil>.fbx (armature + mesh), textures as in 04_export.
  blender -b --factory-startup -P vael_glutpanzer_rig.py
"""
import bpy, bmesh, json, math, os
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

SRC = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/02_tripo/Glutpanzer.fbx"
BONES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "MH_Ingenieur_Knochen.json")
OUT = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/05_rig"
TEX = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/04_export/Texturen"
SCALE = 2.0
DECIMATE = float(os.environ.get("GP_DECIMATE", "0.14"))

PIECES = {
    "Kopf": [4, 45, 49, 50, 52, 54],
    "Brust": [1, 9, 17, 29, 31, 57, 6, 35, 37, 40, 44, 47],
    "Arme": [12, 18, 13, 55, 10, 19, 20, 56],
    "Beine": [11, 36, 42, 43, 8, 15, 38, 7, 16, 39, 2, 25, 32, 3, 26, 33, 30, 5],
    # The plates of the left forearm between elbow and wrist (bracer 21, plates 24, 28, 34): their own piece, so the game can
    # hide them when the Panzerschleuder takes their place (Boris 16.11.: "die Schleuder sitzt statt der bestehenden Armschiene")
    "UnterarmL": [21, 24, 28, 34],
}

# Joints of the Tripo figure (its own metres, +X its left, -Y its front), read from the grid pictures
def J(x, y, z):
    return Vector((x, y, z))

JOINTS = {
    "root": J(0, -0.03, 0.0), "pelvis": J(0, -0.03, 0.52), "spine_01": J(0, -0.03, 0.555), "spine_02": J(0, -0.03, 0.60),
    "spine_03": J(0, -0.03, 0.645), "spine_04": J(0, -0.03, 0.69), "spine_05": J(0, -0.03, 0.74), "neck_01": J(0, -0.035, 0.795),
    "neck_02": J(0, -0.038, 0.82), "head": J(0, -0.04, 0.845),
}
TIPS = {"head": J(0, -0.04, 0.95)}
for s, sx in (("l", 1.0), ("r", -1.0)):
    JOINTS.update({
        "clavicle_" + s: J(0.03 * sx, -0.025, 0.775), "upperarm_" + s: J(0.18 * sx, -0.03, 0.75), "lowerarm_" + s: J(0.23 * sx, -0.03, 0.61),
        "hand_" + s: J(0.235 * sx, -0.03, 0.465), "thigh_" + s: J(0.10 * sx, -0.03, 0.50), "calf_" + s: J(0.12 * sx, -0.03, 0.31),
        "foot_" + s: J(0.13 * sx, -0.03, 0.08), "ball_" + s: J(0.14 * sx, -0.12, 0.015),
    })
    TIPS["hand_" + s] = J(0.235 * sx, -0.03, 0.39)
    TIPS["ball_" + s] = J(0.15 * sx, -0.17, 0.01)
PARENT = {
    "pelvis": "root", "spine_01": "pelvis", "spine_02": "spine_01", "spine_03": "spine_02", "spine_04": "spine_03", "spine_05": "spine_04",
    "neck_01": "spine_05", "neck_02": "neck_01", "head": "neck_02",
}
for s in ("l", "r"):
    PARENT.update({"clavicle_" + s: "spine_05", "upperarm_" + s: "clavicle_" + s, "lowerarm_" + s: "upperarm_" + s, "hand_" + s: "lowerarm_" + s,
                   "thigh_" + s: "pelvis", "calf_" + s: "thigh_" + s, "foot_" + s: "calf_" + s, "ball_" + s: "foot_" + s})
CHILD = {"root": "pelvis", "pelvis": "spine_01", "spine_01": "spine_02", "spine_02": "spine_03", "spine_03": "spine_04", "spine_04": "spine_05",
         "spine_05": "neck_01", "neck_01": "neck_02", "neck_02": "head"}
for s in ("l", "r"):
    CHILD.update({"clavicle_" + s: "upperarm_" + s, "upperarm_" + s: "lowerarm_" + s, "lowerarm_" + s: "hand_" + s,
                  "thigh_" + s: "calf_" + s, "calf_" + s: "foot_" + s, "foot_" + s: "ball_" + s})

# Which bones may hold the vertices of each piece
WEIGHT_BONES = {
    "Kopf": ["head"],
    "Brust": ["spine_03", "spine_04", "spine_05", "neck_01", "clavicle_l", "clavicle_r", "upperarm_l", "upperarm_r"],
    "Arme": ["upperarm_l", "lowerarm_l", "hand_l", "upperarm_r", "lowerarm_r", "hand_r", "spine_05", "clavicle_l", "clavicle_r"],
    "UnterarmL": ["upperarm_l", "lowerarm_l", "hand_l"],
    "Beine": ["pelvis", "spine_01", "thigh_l", "calf_l", "foot_l", "ball_l", "thigh_r", "calf_r", "foot_r", "ball_r"],
}
# The limbs turned into the rest pose of the MetaHuman, parent first: bone, the bone it points to in the body
MATCH = []
for s in ("l", "r"):
    MATCH += [("upperarm_" + s, "lowerarm_" + s), ("lowerarm_" + s, "hand_" + s), ("hand_" + s, "middle_03_" + s),
              ("thigh_" + s, "calf_" + s), ("calf_" + s, "foot_" + s), ("foot_" + s, "ball_" + s)]


def log(text):
    print("GPRIG " + text, flush=True)


def body_bone(bones, name):
    x, y, z = bones[name]["loc"]
    return Vector((x, -y, z)) / 100.0


def seg_dist(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-9)))
    return (a + ab * t - p).length


# The whole armadillo shell on the back, which leaves as one piece (Boris 16.11.: "dass sich der Panzer als ein Stück löst und
# wieder anheftet"). Tripo cut it into eleven parts (seen tinted from behind): the big lower shell (0), the plates of its middle
# row (14, 41), the plates at the nape (46, 48, 53, 58), the two upper side rows (22 left, 23 right, before on the chest) and
# the dome up to the helmet (27, 51, before on the helmet). Without the upper rows and the dome the shield was missing its top
# half, and their ragged seams showed as breaks
SHELL_PARTS = [0, 14, 41, 46, 48, 53, 58, 22, 23, 27, 51]
# parts that stand off the rest and are pushed against it (none now: in the whole shell every plate touches its neighbours)
SHELL_SNAP = []
SHELL_BANDS = 5

# The back under the shell (Boris 16.11.): Tripo modelled none, so with the shield held before him the hero was open from the
# middle of the back up to the nape. The chest plate (part 1) already wraps the back below that. Its front above BACK_FROM_Z
# (the V plates and the high collar round the helmet) is copied and mirrored front to back about BACK_MIRROR_Y, the middle of the
# chest's depth, so the hole is closed by the chest's own plates and the high collar shuts the nape behind the helmet. Hidden
# under the shield while it is on the back. (Tripo metres, before the x2 scale.)
# The chest is narrower in front than the opening behind, which reaches to the shoulder plates: the copy is widened toward its
# top (by BACK_WIDEN from BACK_TOP_Z up, where the shoulder plates begin), and it starts a little below the hole and lies a hair outside the back it overlaps.
# Between the collar and the shoulder plates the front has no chest at all, only the inner side of the shoulder plates: their
# inner front (parts 6, 17, 29 within 26 cm of the middle) is mirrored behind too, so the shoulder plates are domed
# alike front and back and close the gap up to the collar.
# (part, lowest z, largest |x| or None, widened)
# (the shoulder plates mirrored whole: cut short, holes stayed between them and their rims in the game's pose)
BACK_MIRRORS = [(1, 0.672, None, True)] + [(n, 0.68, None, False) for n in (6, 17, 29, 9, 31, 35, 44)]
BACK_TOP_Z = 0.76
BACK_FROM_Z = 0.672
BACK_MIRROR_Y = -0.040
BACK_WIDEN = 0.40
BACK_KEY = 1000
# The helmet was open at the back below its crown (there the shell's dome sat). Boris (16.11.) wants neither the dome back on
# the helmet nor a copy of the shell: the helmet's own crown closes it. Its back band (part 4 between the Z limits, behind
# NECK_FROM_Y, within NECK_MAX_X of the middle so the horn bases and ragged sides stay out) is turned down round the back of the head about the head's middle (NECK_PIVOT, y and z), lame under lame like a
# layered helmet, each a little wider so it lies over the one before and follows the curve of the head down into the collar,
# and it turns with the head. (degrees, widen)
NECK_FROM_PART = 4
NECK_FROM_Y = -0.02
NECK_MAX_X = 0.075
NECK_Z = (0.885, 0.935)
NECK_PIVOT = (-0.053, 0.875)
NECK_LAMES = [(20.0, 1.02), (40.0, 1.04), (60.0, 1.06), (80.0, 1.08), (100.0, 1.10)]
NECK_KEY = 1004


def make_back(src, n, low_z, max_x, widen):
    o = src.copy()
    o.data = src.data.copy()
    o.name = o.data.name = "Ruecken_aus_{}".format(n)
    bpy.context.scene.collection.objects.link(o)
    o.data.transform(o.matrix_world)
    o.matrix_world = Matrix.Identity(4)
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.y >= BACK_MIRROR_Y or v.co.z < low_z
                               or (max_x is not None and abs(v.co.x) > max_x)], context="VERTS")
    for v in bm.verts:
        v.co.y = 2.0 * BACK_MIRROR_Y - v.co.y
        if widen:
            up = max(0.0, min(1.0, (v.co.z - BACK_FROM_Z) / (BACK_TOP_Z - BACK_FROM_Z)))
            v.co.x *= 1.0 + BACK_WIDEN * up
    # a mirror turns the faces inside out
    bmesh.ops.reverse_faces(bm, faces=bm.faces)
    bm.to_mesh(o.data)
    bm.free()
    log("Ruecken aus Teil {}: {} verts".format(n, len(o.data.vertices)))
    return o


def make_neck_guard(src, degrees, widen):
    o = src.copy()
    o.data = src.data.copy()
    o.name = o.data.name = "Nackenschutz"
    bpy.context.scene.collection.objects.link(o)
    o.data.transform(o.matrix_world)
    o.matrix_world = Matrix.Identity(4)
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.y < NECK_FROM_Y or abs(v.co.x) > NECK_MAX_X or not NECK_Z[0] <= v.co.z <= NECK_Z[1]], context="VERTS")
    # turning by +angle about X moves the back (+Y) downward
    a = math.radians(degrees)
    cy, cz = NECK_PIVOT
    for v in bm.verts:
        dy, dz = (v.co.y - cy) * widen, (v.co.z - cz) * widen
        v.co.x *= widen
        v.co.y = cy + dy * math.cos(a) + dz * math.sin(a)
        v.co.z = cz - dy * math.sin(a) + dz * math.cos(a)
    bm.to_mesh(o.data)
    bm.free()
    log("Nackenschutz: {} verts".format(len(o.data.vertices)))
    return o


def build_shell(objs, parts_by_number):
    """The domed back shell (Tripo part 0, the Aschenroller's back) with the plates above it as one piece SKM_Ing_GP_Schild: the engineer's
    shield (goal 16.11.: "Panzerschild bauen"). It keeps the coordinates of the armour, so on the back it sits exactly where the
    chest had it. Its own small skeleton: a root at the origin, a chain Schild_0..4 down the middle of the shell, along its bands,
    by which the game curls it into a ball like a rolling Aschenroller, and two unweighted helpers Schild_Seite (its left) and
    Schild_Innen (toward the body) from which the game reads the shell's axes."""
    # plates that stand off the rest are pushed against it along their shortest gap, so nothing floats once it is off the body
    for n in SHELL_SNAP:
        o = parts_by_number[n]
        rest = [parts_by_number[k] for k in SHELL_PARTS if k not in SHELL_SNAP]
        bm = bmesh.new()
        for r in rest:
            m = r.data.copy()
            m.transform(r.matrix_world)
            bm.from_mesh(m)
        tree = BVHTree.FromBMesh(bm)
        best = None
        for v in list(o.data.vertices)[::3]:
            w = o.matrix_world @ v.co
            hit = tree.find_nearest(w)
            if hit[0] is not None and (best is None or hit[3] < best[1]):
                best = (hit[0] - w, hit[3])
        if best is not None and best[1] > 0.001:
            o.matrix_world = Matrix.Translation(best[0]) @ o.matrix_world
            log("Schild: part {} pushed {:.1f} cm onto the shell".format(n, best[1] * 100.0))
    for o in objs:
        mod = o.modifiers.new("Weniger", "DECIMATE")
        mod.ratio = DECIMATE
        bpy.ops.object.select_all(action="DESELECT")
        o.select_set(True)
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.modifier_apply(modifier=mod.name)
        o.data.transform(Matrix.Scale(SCALE, 4) @ o.matrix_world)
        o.matrix_world = Matrix.Identity(4)
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    o = bpy.context.active_object
    o.name = o.data.name = "SKM_Ing_GP_Schild"
    verts = [v.co.copy() for v in o.data.vertices]
    top = max(v.z for v in verts)
    bottom = min(v.z for v in verts)
    # the joints down the middle of the shell (|x| small), halfway through its thickness at each height
    joints = []
    for i in range(SHELL_BANDS + 1):
        z = top - (top - bottom) * i / SHELL_BANDS
        near = [v for v in verts if abs(v.x) < 0.08 and abs(v.z - z) < 0.06] or [v for v in verts if abs(v.z - z) < 0.06]
        y = (min(v.y for v in near) + max(v.y for v in near)) * 0.5
        joints.append(Vector((0.0, y, z)))
    log("Schild {:.3f}..{:.3f} m, joints {}".format(bottom, top, [[round(c, 3) for c in j] for j in joints]))

    arm_data = bpy.data.armatures.new("GP_Schild_Skelett")
    rig = bpy.data.objects.new("GP_Schild_Skelett", arm_data)
    bpy.context.scene.collection.objects.link(rig)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    root = arm_data.edit_bones.new("schild_root")
    root.head, root.tail = Vector((0, 0, 0)), Vector((0, 0, 0.1))
    parent = root
    for i in range(SHELL_BANDS):
        eb = arm_data.edit_bones.new("Schild_{}".format(i))
        eb.head, eb.tail = joints[i], joints[i + 1]
        eb.parent = parent
        parent = eb
    for name, off in (("Schild_Seite", Vector((0.1, 0, 0))), ("Schild_Innen", Vector((0, -0.1, 0)))):
        eb = arm_data.edit_bones.new(name)
        eb.head, eb.tail = joints[0] + off, joints[0] + off + Vector((0, 0, -0.05))
        eb.parent = arm_data.edit_bones["Schild_0"]
    bpy.ops.object.mode_set(mode="OBJECT")
    groups = {i: o.vertex_groups.new(name="Schild_{}".format(i)) for i in range(SHELL_BANDS)}
    for v in o.data.vertices:
        ds = sorted((seg_dist(v.co, joints[i], joints[i + 1]), i) for i in range(SHELL_BANDS))[:2]
        ws = [(1.0 / (d ** 4 + 1e-6), i) for d, i in ds]
        total = sum(w for w, _ in ws)
        for w, i in ws:
            if w / total > 0.02:
                groups[i].add([v.index], w / total, "REPLACE")
    o.parent = rig
    am = o.modifiers.new("Skelett", "ARMATURE")
    am.object = rig
    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    o.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, o.name + ".fbx"), use_selection=True, object_types={"ARMATURE", "MESH"},
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL", add_leaf_bones=False, bake_anim=False,
                             use_armature_deform_only=False, mesh_smooth_type="FACE", path_mode="STRIP", armature_nodetype="NULL")
    log("Schild {} tris".format(sum(len(p.vertices) - 2 for p in o.data.polygons)))


def main():
    os.makedirs(OUT, exist_ok=True)
    body = json.load(open(BONES))["bones"]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=SRC)
    parts = {}
    for o in bpy.data.objects:
        if o.type == "MESH" and "_part_" in o.name:
            parts[int(o.name.split("_part_")[1].split(".")[0])] = o
    # Tripo gives a second UV layer ("Attribute") besides the real one ("UVMap"); joined and skinned only the first survived and
    # the textures came out as speckles: keep the real one alone
    for o in parts.values():
        layers = o.data.uv_layers
        if "UVMap" in layers and "Attribute" in layers:
            layers.remove(layers["Attribute"])
        if "UVMap" in layers:
            layers.active = layers["UVMap"]
            layers["UVMap"].active_render = True
    # materials named after their part, pointing at the 1K textures
    for n, o in parts.items():
        for slot in o.material_slots:
            if slot.material is not None:
                slot.material.name = "GP_Teil_{}".format(n)

    # the back under the shell, closed with the chest's own front plates mirrored behind
    for n, low_z, max_x, widen in BACK_MIRRORS:
        parts[BACK_KEY + n] = make_back(parts[n], n, low_z, max_x, widen)
        PIECES["Brust"].append(BACK_KEY + n)
    for index, (degrees, widen) in enumerate(NECK_LAMES):
        parts[NECK_KEY + 100 * index] = make_neck_guard(parts[NECK_FROM_PART], degrees, widen)
        PIECES["Kopf"].append(NECK_KEY + 100 * index)

    # the armature at the joints of the figure, scaled like the mesh
    arm_data = bpy.data.armatures.new("GP_Skelett")
    rig = bpy.data.objects.new("GP_Skelett", arm_data)
    bpy.context.scene.collection.objects.link(rig)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    for name in JOINTS:
        eb = arm_data.edit_bones.new(name)
        head = JOINTS[name] * SCALE
        tail = (JOINTS[CHILD[name]] if name in CHILD else TIPS.get(name, JOINTS[name] + Vector((0, 0, 0.05)))) * SCALE
        if (tail - head).length < 0.01:
            tail = head + Vector((0, 0, 0.05))
        eb.head, eb.tail = head, tail
    for name, parent in PARENT.items():
        arm_data.edit_bones[name].parent = arm_data.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")

    # the pieces: decimated, scaled, joined, weighted to the bones of their piece
    pieces = {}
    for piece, members in PIECES.items():
        objs = []
        for n in members:
            o = parts[n]
            mod = o.modifiers.new("Weniger", "DECIMATE")
            mod.ratio = DECIMATE
            bpy.ops.object.select_all(action="DESELECT")
            o.select_set(True)
            bpy.context.view_layer.objects.active = o
            bpy.ops.object.modifier_apply(modifier=mod.name)
            o.data.transform(Matrix.Scale(SCALE, 4) @ o.matrix_world)
            o.matrix_world = Matrix.Identity(4)
            objs.append(o)
        bpy.ops.object.select_all(action="DESELECT")
        for o in objs:
            o.select_set(True)
        bpy.context.view_layer.objects.active = objs[0]
        if len(objs) > 1:
            bpy.ops.object.join()
        mesh = bpy.context.active_object
        mesh.name = "SKM_Ing_GP_" + piece
        mesh.data.name = mesh.name
        segs = {}
        for b in WEIGHT_BONES[piece]:
            pb = arm_data.bones[b]
            segs[b] = (pb.head_local.copy(), pb.tail_local.copy())
        groups = {b: mesh.vertex_groups.new(name=b) for b in segs}
        for v in mesh.data.vertices:
            ds = sorted(((seg_dist(v.co, a, c), b) for b, (a, c) in segs.items()))[:3]
            ws = [(1.0 / (d ** 4 + 1e-6), b) for d, b in ds]
            total = sum(w for w, _ in ws)
            for w, b in ws:
                if w / total > 0.02:
                    groups[b].add([v.index], w / total, "REPLACE")
        mesh.parent = rig
        am = mesh.modifiers.new("Skelett", "ARMATURE")
        am.object = rig
        pieces[piece] = mesh
        log("{:<6} {:>6} tris".format(piece, sum(len(p.vertices) - 2 for p in mesh.data.polygons)))

    # turn the limbs into the rest pose of the MetaHuman, then make that pose the rest
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="POSE")
    for bone, toward in MATCH:
        bpy.context.view_layer.update()
        pb = rig.pose.bones[bone]
        child = CHILD.get(bone)
        now_head = rig.matrix_world @ pb.head
        now_dir = ((rig.matrix_world @ pb.tail) - now_head).normalized()
        want = (body_bone(body, toward) - body_bone(body, bone)).normalized()
        turn = now_dir.rotation_difference(want)
        # rotation in the bone's own space: R_local = M^-1 * turn * M (M = bone's armature matrix rotation)
        m = pb.matrix.to_quaternion()
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = (m.inverted() @ turn @ m) @ pb.rotation_quaternion
    bpy.context.view_layer.update()
    bpy.ops.object.mode_set(mode="OBJECT")
    for piece, mesh in pieces.items():
        bpy.ops.object.select_all(action="DESELECT")
        mesh.select_set(True)
        bpy.context.view_layer.objects.active = mesh
        bpy.ops.object.modifier_apply(modifier="Skelett")
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="POSE")
    bpy.ops.pose.select_all(action="SELECT")
    bpy.ops.pose.armature_apply(selected=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    for mesh in pieces.values():
        am = mesh.modifiers.new("Skelett", "ARMATURE")
        am.object = rig

    for b in ("hand_l", "pelvis", "head", "foot_l"):
        log("rest {:<8} {}".format(b, [round(c, 3) for c in rig.data.bones[b].head_local]))
    log("body hand_l {}".format([round(c, 3) for c in body_bone(body, "hand_l")]))

    # one FBX per piece, each with the whole skeleton
    for piece, mesh in pieces.items():
        bpy.ops.object.select_all(action="DESELECT")
        rig.select_set(True)
        mesh.select_set(True)
        bpy.context.view_layer.objects.active = rig
        bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, mesh.name + ".fbx"), use_selection=True, object_types={"ARMATURE", "MESH"},
                                 apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL", add_leaf_bones=False, bake_anim=False,
                                 use_armature_deform_only=False, mesh_smooth_type="FACE", path_mode="STRIP", armature_nodetype="NULL")

    build_shell([parts[n] for n in SHELL_PARTS], parts)

    # preview in the rest pose
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 900, 1100
    cam_data = bpy.data.cameras.new("C"); cam_data.type = "ORTHO"; cam_data.ortho_scale = 2.3
    cam = bpy.data.objects.new("C", cam_data); scene.collection.objects.link(cam); scene.camera = cam
    for view, loc, rot in (("vorne", (0, -4.0, 1.0), (math.radians(90), 0, 0)), ("seite", (4.0, 0, 1.0), (math.radians(90), 0, math.radians(90)))):
        cam.location = loc; cam.rotation_euler = rot
        scene.render.filepath = os.path.join(OUT, "rig_" + view + ".png")
        bpy.ops.render.render(write_still=True)
    log("done")


main()
