"""
The Glutpanzer as its own body (goal 16.11., Boris: "die Teile ersetzen den MetaHuman, angelegt so, wie Tripo sie gebaut hat").

The Tripo armour keeps its own stocky proportions (scaled x2.0 evenly) and gets a skeleton with the bone names of the body, set
at its own joints. In the game it follows the body by the rotations of the bones (UVaelPoseFollowerAnimInstance), like the
MetaHuman does, but keeps its own bone lengths. Its rest pose is turned into the rest pose of the MetaHuman (the arms at 45
degrees, the forearms forward), so the rotations carry over one to one.

Four pieces, one skeletal mesh each on the same skeleton:
  Kopf    the helmet
  Brust   chest, shoulder plates, collar and the domed back shell
  Arme    everything below the shoulder plates: sleeves, forearms, gauntlets
  Beine   the belt and everything below it

Weights: every vertex to the bones of its piece by its distance to their segments (1/d^4, the closest three), so the joints bend
smoothly; the helmet hangs on the head alone.

Out: C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/05_rig/SKM_Ing_GP_<Teil>.fbx (armature + mesh), textures as in 04_export.
  blender -b --factory-startup -P vael_glutpanzer_rig.py
"""
import bpy, bmesh, json, math, os
from mathutils import Matrix, Vector

SRC = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/02_tripo/Glutpanzer.fbx"
BONES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "MH_Ingenieur_Knochen.json")
OUT = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/05_rig"
TEX = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/04_export/Texturen"
SCALE = 2.0
DECIMATE = float(os.environ.get("GP_DECIMATE", "0.14"))

PIECES = {
    "Kopf": [4, 27, 45, 49, 50, 51, 52, 54],
    "Brust": [1, 0, 14, 41, 46, 48, 53, 58, 9, 17, 22, 29, 31, 57, 6, 23, 35, 37, 40, 44, 47],
    "Arme": [12, 18, 21, 24, 28, 34, 13, 55, 10, 19, 20, 56],
    "Beine": [11, 36, 42, 43, 8, 15, 38, 7, 16, 39, 2, 25, 32, 3, 26, 33, 30, 5],
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
