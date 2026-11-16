"""
The Glutpanzer (goal 16.11.): the Tripo model of the engineer's armour of Glutkriecher and Aschenroller, made in parts by Tripo
("In Teilen generieren", 59 parts), fitted onto the body of the engineer and cut into the pieces he wears.

Each Tripo part goes to one piece; each piece is laid where it belongs on the body in its reference pose (the bones read from
the game by "VaelIngenieur knochen" into Saved/Ingenieur_Knochen.json) and hung on one bone in the game
(UVaelEngineerComponent::UpdateArmorMeshes). The Tripo figure is stocky: lengths fit at x2.0, widths at x1.45, so every piece is
scaled 1.45 across and 2.0 along; the limbs are turned one by one from the A-pose of Tripo onto the bones of the body.

Out: C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/04_export/SM_Ing_GP_<Stueck>.fbx (vertices in actor space of the reference
pose, metres, Blender axes: +X forward, +Y the character's left), textures GP_Teil_<n>.png at 1024 px, a preview picture.

  blender -b --factory-startup -P vael_glutpanzer_bauen.py
"""
import bpy, bmesh, json, math, os
from mathutils import Matrix, Vector

SRC = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/02_tripo/Glutpanzer.fbx"
BONES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "MH_Ingenieur_Knochen.json")  # from "VaelIngenieur knochen" (Saved/Ingenieur_Knochen.json)
OUT = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/04_export"
TEX_SIZE = 1024
DECIMATE = 0.12
ACROSS, ALONG = 1.45, 2.0

# Which Tripo part goes to which piece (from 03_blender/teile.json and the grid pictures; +x of Tripo = the character's left)
GROUPS = {
    "Kopf": [4, 27, 45, 49, 50, 51, 52, 54],
    "Ruecken": [0, 14, 41, 46, 48, 53, 58],
    "Brust": [1],  # part 18 (the right sleeve grown into the chest in the A-pose) stuck out behind the shoulder once the arm hung down
    "Guertel": [11, 36, 42, 43],
    "Schulter_L": [9, 17, 22, 29, 31, 57],
    "Schulter_R": [6, 23, 35, 37, 40, 44, 47],
    "Oberarm_L": [12],
    "Unterarm_L": [21, 24, 28, 34],
    "Hand_L": [13, 55],
    "Unterarm_R": [10, 19],
    "Hand_R": [20, 56],
    "Oberschenkel_L": [8, 15, 38],
    "Oberschenkel_R": [7, 16, 39],
    "Unterschenkel_L": [2, 25, 32],
    "Unterschenkel_R": [3, 26, 33],
    "Fuss_L": [30],
    "Fuss_R": [5],
}

# Where each piece is fixed: Tripo anchor (and the end of its limb), the bone it lies on (and the bone at the end of that limb),
# an offset in cm from the bone in the reference pose (actor axes: x forward, z up)
T = lambda x, y, z: Vector((x, y, z))  # noqa: E731
FIT = {
    "Kopf": (T(0, -0.04, 0.80), None, "neck_01", None, (0.0, 1.5, 0.5)),
    "Brust": (T(0, -0.03, 0.52), None, "pelvis", None, (0.0, 0.0, 3.0)),
    "Ruecken": (T(0, -0.03, 0.52), None, "pelvis", None, (0.0, 0.0, 3.0)),
    "Guertel": (T(0, -0.03, 0.52), None, "pelvis", None, (0.0, 0.0, 3.0)),
}
for side, sx, s in (("L", 1.0, "l"), ("R", -1.0, "r")):
    FIT["Schulter_" + side] = (T(0.18 * sx, -0.03, 0.75), None, "upperarm_" + s, None, (0.0, 0.0, -1.0))
    FIT["Oberarm_" + side] = (T(0.18 * sx, -0.03, 0.75), T(0.23 * sx, -0.03, 0.61), "upperarm_" + s, "lowerarm_" + s, (0, 0, 0))
    FIT["Unterarm_" + side] = (T(0.23 * sx, -0.03, 0.61), T(0.235 * sx, -0.03, 0.465), "lowerarm_" + s, "hand_" + s, (0, 0, 0))
    FIT["Hand_" + side] = (T(0.235 * sx, -0.03, 0.465), T(0.235 * sx, -0.03, 0.39), "hand_" + s, "middle_03_" + s, (0, 0, 0))
    FIT["Oberschenkel_" + side] = (T(0.10 * sx, -0.03, 0.50), T(0.12 * sx, -0.03, 0.31), "thigh_" + s, "calf_" + s, (0, 0, 0))
    FIT["Unterschenkel_" + side] = (T(0.12 * sx, -0.03, 0.31), T(0.13 * sx, -0.03, 0.08), "calf_" + s, "foot_" + s, (0, 0, 0))
    FIT["Fuss_" + side] = (T(0.13 * sx, -0.03, 0.08), T(0.14 * sx, -0.13, 0.01), "foot_" + s, "ball_" + s, (0, 0, 0))

# Tripo axes to the axes of the body in Blender: Tripo looks to -Y with its left at +X; the body looks to +X with its left at +Y
TO_BODY = Matrix.Identity(3)  # the MetaHuman body in Blender axes looks to -Y with its left at +X, like the Tripo figure
SCALE = Matrix.Diagonal((ACROSS, ACROSS, ALONG))


def log(text):
    print("GLUTPANZER " + text, flush=True)


def bone(bones, name, offset=(0, 0, 0)):
    """A bone of the game (actor space, cm, left-handed) in Blender metres with +Y to the left"""
    x, y, z = bones[name]["loc"]
    return Vector(((x + offset[0]) / 100.0, -(y + offset[1]) / 100.0, (z + offset[2]) / 100.0))


def piece_matrix(bones, name):
    anchor, end, start_bone, end_bone, offset = FIT[name]
    at = bone(bones, start_bone, offset)
    turn = Matrix.Identity(3)
    if end is not None:
        tripo_dir = (SCALE @ TO_BODY @ (end - anchor)).normalized()
        body_dir = (bone(bones, end_bone) - bone(bones, start_bone)).normalized()
        turn = tripo_dir.rotation_difference(body_dir).to_matrix()
    # the shoulders keep the height of their plates closer to their width, or they rise over the head
    scale = Matrix.Diagonal((ACROSS, ACROSS, 1.5)) if name.startswith("Schulter") else SCALE
    if name == "Kopf":
        # the helmet a size larger than the stocky figure gives it, so the head of the MetaHuman with its hair fits inside
        scale = Matrix.Diagonal((1.85, 1.85, 2.25))
    if end is not None:
        # every limb stretched to the length of its bone (the Tripo figure has shorter thighs and longer shins than the body)
        along = (bone(bones, end_bone) - bone(bones, start_bone)).length / max((end - anchor).length, 1e-4)
        scale = Matrix.Diagonal((ACROSS, ACROSS, along))
        if name.startswith("Fuss"):
            # the boot points forward: longer along the foot (Tripo -Y), so the toes of the body stay inside
            scale = Matrix.Diagonal((1.9, 2.6, 1.9))
        tripo_dir = (scale @ TO_BODY @ (end - anchor)).normalized()
        turn = tripo_dir.rotation_difference(body_dir).to_matrix()
        log("{:<16} along x{:.2f}".format(name, along))
    lin = turn @ scale @ TO_BODY
    m = lin.to_4x4()
    m.translation = at - lin @ anchor
    return m


def main():
    os.makedirs(OUT, exist_ok=True)
    bones = json.load(open(BONES))["bones"]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=SRC)
    parts = {}
    for o in bpy.data.objects:
        if o.type == "MESH" and "_part_" in o.name:
            parts[int(o.name.split("_part_")[1].split(".")[0])] = o
    log("{} parts".format(len(parts)))
    assigned = sorted(n for g in GROUPS.values() for n in g)
    missing = sorted(set(parts) - set(assigned))
    if missing:
        log("not assigned: {}".format(missing))

    # textures: every part its own, scaled down, its material named after it
    tex_dir = os.path.join(OUT, "Texturen")
    os.makedirs(tex_dir, exist_ok=True)
    for n, o in parts.items():
        for slot in o.material_slots:
            mat = slot.material
            if mat is None:
                continue
            mat.name = "GP_Teil_{}".format(n)
            for node in (mat.node_tree.nodes if mat.use_nodes else []):
                if node.type == "TEX_IMAGE" and node.image is not None:
                    img = node.image
                    img.scale(TEX_SIZE, TEX_SIZE)
                    img.filepath_raw = os.path.join(tex_dir, "GP_Teil_{}.png".format(n))
                    img.file_format = "PNG"
                    img.save()

    # The boots belong to the greaves in the Tripo parts, but the foot turns by itself in the game: everything below the ankle
    # goes to the foot, so the toes don't stick out of a boot that stays with the shin
    groups = {k: list(v) for k, v in GROUPS.items()}
    for n, foot in ((2, "Fuss_L"), (3, "Fuss_R")):
        shin = parts[n]
        boot = shin.copy()
        boot.data = shin.data.copy()
        bpy.context.scene.collection.objects.link(boot)
        for obj, keep_low in ((shin, False), (boot, True)):
            bm = bmesh.new()
            bm.from_mesh(obj.data)
            mw = obj.matrix_world
            gone = [f for f in bm.faces if ((mw @ f.calc_center_median()).z < 0.085) != keep_low]
            bmesh.ops.delete(bm, geom=gone, context="FACES")
            bm.to_mesh(obj.data)
            bm.free()
        parts[100 + n] = boot
        groups[foot].append(100 + n)

    totals = {}
    for name, members in groups.items():
        m = piece_matrix(bones, name)
        objs = []
        for n in members:
            o = parts[n]
            # decimate first, on the Tripo shape
            mod = o.modifiers.new("Weniger", "DECIMATE")
            mod.ratio = DECIMATE
            bpy.context.view_layer.objects.active = o
            bpy.ops.object.select_all(action="DESELECT")
            o.select_set(True)
            bpy.ops.object.modifier_apply(modifier=mod.name)
            world = o.matrix_world.copy()
            o.data.transform(m @ world)
            o.matrix_world = Matrix.Identity(4)
            objs.append(o)
        bpy.ops.object.select_all(action="DESELECT")
        for o in objs:
            o.select_set(True)
        bpy.context.view_layer.objects.active = objs[0]
        if len(objs) > 1:
            bpy.ops.object.join()
        piece = bpy.context.active_object
        if name == "Kopf":
            # The helmet of Tripo is open at the back (there the collar of the figure closed it): a cap of the same crust over the
            # back of the head, from the crown down to the nape, so neither hair nor scalp shows
            head = bone(bones, "head")
            bpy.ops.mesh.primitive_uv_sphere_add(segments=28, ring_count=16, radius=0.118, location=head + Vector((0.0, 0.015, 0.075)))
            cap = bpy.context.active_object
            cap.scale = (0.98, 1.08, 1.05)
            bpy.ops.object.transform_apply(scale=True)
            bm = bmesh.new()
            bm.from_mesh(cap.data)
            middle = head + Vector((0.0, 0.015, 0.075))
            # keep the back half (+Y is behind) and nothing below the nape
            gone = [f for f in bm.faces if f.calc_center_median().y < middle.y - 0.035 or f.calc_center_median().z < middle.z - 0.105]
            bmesh.ops.delete(bm, geom=gone, context="FACES")
            bm.to_mesh(cap.data)
            bm.free()
            cap.data.materials.append(bpy.data.materials.get("GP_Teil_4"))
            bpy.ops.object.select_all(action="DESELECT")
            cap.select_set(True)
            piece.select_set(True)
            bpy.context.view_layer.objects.active = piece
            bpy.ops.object.join()
            piece = bpy.context.active_object
        piece.name = "SM_Ing_GP_" + name
        piece.data.name = piece.name
        tris = sum(len(p.vertices) - 2 for p in piece.data.polygons)
        totals[name] = tris
        bpy.ops.object.select_all(action="DESELECT")
        piece.select_set(True)
        bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, piece.name + ".fbx"), use_selection=True, object_types={"MESH"},
                                 apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL", bake_anim=False, mesh_smooth_type="FACE",
                                 path_mode="STRIP")
        log("{:<16} {:>6} tris".format(name, tris))
    log("total {} tris".format(sum(totals.values())))

    # preview: the pieces on little balls where the bones are, front and side
    dot = bpy.data.materials.new("Knochen"); dot.diffuse_color = (0.1, 0.8, 1.0, 1)
    for bname in ("head", "neck_01", "spine_05", "spine_03", "pelvis", "upperarm_l", "lowerarm_l", "hand_l", "upperarm_r", "lowerarm_r",
                  "hand_r", "thigh_l", "calf_l", "foot_l", "thigh_r", "calf_r", "foot_r"):
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.025, location=bone(bones, bname))
        bpy.context.active_object.data.materials.append(dot)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 800, 1100
    cam_data = bpy.data.cameras.new("C"); cam_data.type = "ORTHO"; cam_data.ortho_scale = 2.3
    cam = bpy.data.objects.new("C", cam_data); scene.collection.objects.link(cam); scene.camera = cam
    for view, loc, rot in (("vorne", (4.0, 0, 0.0), (math.radians(90), 0, math.radians(90))),
                           ("seite", (0, -4.0, 0.0), (math.radians(90), 0, 0)),
                           ("hinten", (-4.0, 0, 0.0), (math.radians(90), 0, math.radians(-90)))):
        cam.location = loc; cam.rotation_euler = rot
        scene.render.filepath = os.path.join(OUT, "vorschau_" + view + ".png")
        bpy.ops.render.render(write_still=True)
    log("done")


main()
