"""
The Aschenjaeger set (26.11., Boris: "runterladen und bauen") as its own body, like the Glutpanzer (vael_glutpanzer_rig.py).

Tripo made ONE textured mesh (task 9970026a, H3.5, from four views), not parts. It is set on the ground, cut down, scaled to the
engineer and given a skeleton with the bone names of the body at its own joints (read from 03_blender/raster_*.png, made by
vael_aschenjaeger_raster.py). Its limbs are turned into the rest pose of the MetaHuman, so the game carries the rotations of the
body over one to one (UVaelPoseFollowerAnimInstance).

The mesh is cut by place into four pieces on one skeleton, like the Glutpanzer, so the game can hide the MetaHuman under each:
  Kopf    hood, bandage mask and the round eye glasses
  Brust   the straw-bound chest, the scarf and the feather collar over the shoulders
  Arme    the bandaged arms with their feather rows and the clawed hands
  Beine   the twine belt, the straw skirt and the wrapped legs with the talon boots
A face goes to the piece of its middle; every vertex is weighted to the bones its piece may use by its distance to their
segments (1/d^4, the closest three), the sets of bones overlap at the seams.

Out: C:/Projekte/Vael_Quellen/Ingenieur/Aschenjaeger/05_rig/SKM_Ing_AJ_<Teil>.fbx and Texturen/T_Ing_AJ_{Farbe,Normal,ORM}.png
  blender -b --factory-startup -P vael_aschenjaeger_rig.py
"""
import bpy, bmesh, json, math, os
from mathutils import Matrix, Vector

SRC = "C:/Projekte/Vael_Quellen/Ingenieur/Aschenjaeger/02_tripo/Aschenjaeger.glb"
BONES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "MH_Ingenieur_Knochen.json")
OUT = "C:/Projekte/Vael_Quellen/Ingenieur/Aschenjaeger/05_rig"
TEX = os.path.join(OUT, "Texturen")
TEX_SIZE = 2048
# the engineer's head bone stands at 1.70 m, the Aschenjaeger's at 0.87 of his 1.0
SCALE = 1.95
TRIS = int(os.environ.get("AJ_TRIS", "70000"))


def J(x, y, z):
    return Vector((x, y, z))


# Joints of the Tripo figure (its own metres after setting it on the ground: +X its left, -Y its front)
JOINTS = {
    "root": J(0, 0, 0.0), "pelvis": J(0, 0.0, 0.493), "spine_01": J(0, 0.0, 0.517), "spine_02": J(0, 0.0, 0.558),
    "spine_03": J(0, 0.0, 0.60), "spine_04": J(0, 0.0, 0.65), "spine_05": J(0, 0.0, 0.716), "neck_01": J(0, -0.005, 0.80),
    "neck_02": J(0, -0.01, 0.835), "head": J(0, -0.015, 0.87),
}
TIPS = {"head": J(0, -0.015, 0.97)}
for s, sx in (("l", 1.0), ("r", -1.0)):
    JOINTS.update({
        "clavicle_" + s: J(0.01 * sx, 0.0, 0.735), "upperarm_" + s: J(0.105 * sx, 0.0, 0.745), "lowerarm_" + s: J(0.155 * sx, 0.0, 0.635),
        "hand_" + s: J(0.178 * sx, 0.0, 0.525), "thigh_" + s: J(0.06 * sx, 0.0, 0.49), "calf_" + s: J(0.08 * sx, 0.0, 0.272),
        "foot_" + s: J(0.085 * sx, 0.01, 0.07), "ball_" + s: J(0.09 * sx, -0.06, 0.02),
    })
    TIPS["hand_" + s] = J(0.185 * sx, -0.005, 0.44)
    TIPS["ball_" + s] = J(0.095 * sx, -0.10, 0.01)
PARENT = {"pelvis": "root", "spine_01": "pelvis", "spine_02": "spine_01", "spine_03": "spine_02", "spine_04": "spine_03",
          "spine_05": "spine_04", "neck_01": "spine_05", "neck_02": "neck_01", "head": "neck_02"}
for s in ("l", "r"):
    PARENT.update({"clavicle_" + s: "spine_05", "upperarm_" + s: "clavicle_" + s, "lowerarm_" + s: "upperarm_" + s, "hand_" + s: "lowerarm_" + s,
                   "thigh_" + s: "pelvis", "calf_" + s: "thigh_" + s, "foot_" + s: "calf_" + s, "ball_" + s: "foot_" + s})
CHILD = {"root": "pelvis", "pelvis": "spine_01", "spine_01": "spine_02", "spine_02": "spine_03", "spine_03": "spine_04", "spine_04": "spine_05",
         "spine_05": "neck_01", "neck_01": "neck_02", "neck_02": "head"}
for s in ("l", "r"):
    CHILD.update({"clavicle_" + s: "upperarm_" + s, "upperarm_" + s: "lowerarm_" + s, "lowerarm_" + s: "hand_" + s,
                  "thigh_" + s: "calf_" + s, "calf_" + s: "foot_" + s, "foot_" + s: "ball_" + s})

ARM_BONES = {"upperarm_l", "lowerarm_l", "hand_l", "upperarm_r", "lowerarm_r", "hand_r"}
WEIGHT_BONES = {
    "Kopf": ["head", "neck_02", "neck_01"],
    "Brust": ["spine_02", "spine_03", "spine_04", "spine_05", "neck_01", "clavicle_l", "clavicle_r", "upperarm_l", "upperarm_r"],
    "Arme": ["upperarm_l", "lowerarm_l", "hand_l", "upperarm_r", "lowerarm_r", "hand_r", "clavicle_l", "clavicle_r"],
    "Beine": ["pelvis", "spine_01", "spine_02", "thigh_l", "calf_l", "foot_l", "ball_l", "thigh_r", "calf_r", "foot_r", "ball_r"],
}
MATCH = []
for s in ("l", "r"):
    MATCH += [("upperarm_" + s, "lowerarm_" + s), ("lowerarm_" + s, "hand_" + s), ("hand_" + s, "middle_03_" + s),
              ("thigh_" + s, "calf_" + s), ("calf_" + s, "foot_" + s), ("foot_" + s, "ball_" + s)]


def log(text):
    print("AJRIG " + text, flush=True)


def body_bone(bones, name):
    x, y, z = bones[name]["loc"]
    return Vector((x, -y, z)) / 100.0


def seg_dist(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-9)))
    return (a + ab * t - p).length


def segment(name):
    head = JOINTS[name]
    tail = JOINTS[CHILD[name]] if name in CHILD else TIPS.get(name, head + Vector((0, 0, 0.03)))
    return head, tail


def place_of(p):
    """Which piece a point of the Tripo figure (unscaled) belongs to"""
    if p.z > 0.80 and abs(p.x) < 0.085:
        return "Kopf"
    near = min(JOINTS, key=lambda n: seg_dist(p, *segment(n)) if n != "root" else 9.0)
    if near in ARM_BONES and p.z < 0.715:
        return "Arme"
    if p.z < 0.585:
        return "Beine"
    return "Brust"


def save_textures(mat):
    """The base colour, normal and the packed occlusion/roughness/metal maps of the glTF material, at TEX_SIZE"""
    os.makedirs(TEX, exist_ok=True)
    found = {}
    for node in mat.node_tree.nodes:
        if node.type != "TEX_IMAGE" or node.image is None:
            continue
        for link in node.outputs["Color"].links:
            to = link.to_socket.name
            target = link.to_node
            if to == "Base Color":
                found["Farbe"] = node.image
            elif target.type == "NORMAL_MAP":
                found["Normal"] = node.image
            elif target.type in ("SEPARATE_COLOR", "SEPRGB", "SEPARATE_RGB"):
                found["ORM"] = node.image
    for key, img in found.items():
        img.scale(TEX_SIZE, TEX_SIZE)
        img.filepath_raw = os.path.join(TEX, "T_Ing_AJ_{}.png".format(key))
        img.file_format = "PNG"
        img.save()
        log("texture {} {}x{}".format(key, *img.size))
    return found


def main():
    os.makedirs(OUT, exist_ok=True)
    body = json.load(open(BONES))["bones"]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=SRC)
    src = [o for o in bpy.data.objects if o.type == "MESH"][0]
    src.data.transform(src.matrix_world)
    src.matrix_world = Matrix.Identity(4)
    pts = [v.co for v in src.data.vertices]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    src.data.transform(Matrix.Translation(Vector((-(lo.x + hi.x) / 2, -(lo.y + hi.y) / 2, -lo.z))))
    mat = src.material_slots[0].material
    save_textures(mat)
    mat.name = "MI_Ing_Aschenjaeger"
    faces = len(src.data.polygons)
    mod = src.modifiers.new("Weniger", "DECIMATE")
    mod.ratio = min(1.0, TRIS / max(faces, 1))
    bpy.context.view_layer.objects.active = src
    src.select_set(True)
    bpy.ops.object.modifier_apply(modifier=mod.name)
    log("decimated {} -> {} faces".format(faces, len(src.data.polygons)))

    # cut into the four pieces by the middle of each face
    pieces = {}
    for piece in WEIGHT_BONES:
        o = src.copy()
        o.data = src.data.copy()
        o.name = o.data.name = "SKM_Ing_AJ_" + piece
        bpy.context.scene.collection.objects.link(o)
        bm = bmesh.new()
        bm.from_mesh(o.data)
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if place_of(f.calc_center_median()) != piece], context="FACES")
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
        bm.to_mesh(o.data)
        bm.free()
        pieces[piece] = o
    bpy.data.objects.remove(src)

    # the armature at the joints, then everything scaled to the engineer
    arm_data = bpy.data.armatures.new("AJ_Skelett")
    rig = bpy.data.objects.new("AJ_Skelett", arm_data)
    bpy.context.scene.collection.objects.link(rig)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="EDIT")
    for name in JOINTS:
        eb = arm_data.edit_bones.new(name)
        head, tail = segment(name)
        head, tail = head * SCALE, tail * SCALE
        if (tail - head).length < 0.01:
            tail = head + Vector((0, 0, 0.05))
        eb.head, eb.tail = head, tail
    for name, parent in PARENT.items():
        arm_data.edit_bones[name].parent = arm_data.edit_bones[parent]
    bpy.ops.object.mode_set(mode="OBJECT")

    for piece, mesh in pieces.items():
        mesh.data.transform(Matrix.Scale(SCALE, 4))
        segs = {b: (arm_data.bones[b].head_local.copy(), arm_data.bones[b].tail_local.copy()) for b in WEIGHT_BONES[piece]}
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
        log("{:<6} {:>6} tris".format(piece, sum(len(p.vertices) - 2 for p in mesh.data.polygons)))

    # the limbs turned into the rest pose of the MetaHuman, then that pose made the rest
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode="POSE")
    for bone, toward in MATCH:
        bpy.context.view_layer.update()
        pb = rig.pose.bones[bone]
        now_head = rig.matrix_world @ pb.head
        now_dir = ((rig.matrix_world @ pb.tail) - now_head).normalized()
        want = (body_bone(body, toward) - body_bone(body, bone)).normalized()
        turn = now_dir.rotation_difference(want)
        m = pb.matrix.to_quaternion()
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = (m.inverted() @ turn @ m) @ pb.rotation_quaternion
    bpy.context.view_layer.update()
    bpy.ops.object.mode_set(mode="OBJECT")
    for mesh in pieces.values():
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
        log("rest {:<8} {} body {}".format(b, [round(c, 3) for c in rig.data.bones[b].head_local], [round(c, 3) for c in body_bone(body, b)]))

    for piece, mesh in pieces.items():
        bpy.ops.object.select_all(action="DESELECT")
        rig.select_set(True)
        mesh.select_set(True)
        bpy.context.view_layer.objects.active = rig
        bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, mesh.name + ".fbx"), use_selection=True, object_types={"ARMATURE", "MESH"},
                                 apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL", add_leaf_bones=False, bake_anim=False,
                                 use_armature_deform_only=False, mesh_smooth_type="FACE", path_mode="STRIP", armature_nodetype="NULL")

    # preview in the rest pose, each piece tinted
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
