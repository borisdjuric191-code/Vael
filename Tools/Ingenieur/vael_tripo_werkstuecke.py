"""
Tripo pieces of the engineer as static meshes (26.11.): the Doppelsichel of the Aschenjaeger and the Quarzbrille, each cut down,
set on its own origin and scaled as the game expects, with its textures.

  Doppelsichel  origin = the middle of the hub, lying flat in XY (it spins about Z when thrown), the blades 55 cm from tip to tip
  Quarzbrille   origin = the point between the eyes, +X ahead, +Z up, the lenses 6.5 cm apart (like the Blender goggles of
                vael_ingenieur_werkstuecke.py VAEL_ING_BRILLE, which the game fits to the face); the braided band behind the cups
                is widened to go round the head of the MetaHuman

Out: C:/Projekte/Vael_Quellen/Ingenieur/Tripo_Werkstuecke/SM_Ing_<Name>.fbx and T_Ing_<Name>_{Farbe,Normal,ORM}.png
  blender -b --factory-startup -P vael_tripo_werkstuecke.py
"""
import bpy, bmesh, math, os
from mathutils import Matrix, Vector

OUT = "C:/Projekte/Vael_Quellen/Ingenieur/Tripo_Werkstuecke"
Q = "C:/Projekte/Vael_Quellen/Ingenieur"


def log(text):
    print("TRIPOWS " + text, flush=True)


def save_textures(mat, name, size):
    found = {}
    for node in mat.node_tree.nodes:
        if node.type != "TEX_IMAGE" or node.image is None:
            continue
        for link in node.outputs["Color"].links:
            target = link.to_node
            if link.to_socket.name == "Base Color":
                found["Farbe"] = node.image
            elif target.type == "NORMAL_MAP":
                found["Normal"] = node.image
            elif target.type in ("SEPARATE_COLOR", "SEPRGB", "SEPARATE_RGB"):
                found["ORM"] = node.image
    for key, img in found.items():
        img.scale(size, size)
        img.filepath_raw = os.path.join(OUT, "T_Ing_{}_{}.png".format(name, key))
        img.file_format = "PNG"
        img.save()
    log("{} textures {}".format(name, sorted(found)))


def load(path, name, tris, tex):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    o = [x for x in bpy.data.objects if x.type == "MESH"][0]
    o.data.transform(o.matrix_world)
    o.matrix_world = Matrix.Identity(4)
    pts = [v.co for v in o.data.vertices]
    lo = Vector([min(p[i] for p in pts) for i in range(3)])
    hi = Vector([max(p[i] for p in pts) for i in range(3)])
    # the same placing as the measuring pictures: centred in x and y, on the ground
    o.data.transform(Matrix.Translation(Vector((-(lo.x + hi.x) / 2, -(lo.y + hi.y) / 2, -lo.z))))
    mat = o.material_slots[0].material
    save_textures(mat, name, tex)
    mat.name = "MI_Ing_" + name
    faces = len(o.data.polygons)
    mod = o.modifiers.new("Weniger", "DECIMATE")
    mod.ratio = min(1.0, tris / max(faces, 1))
    bpy.context.view_layer.objects.active = o
    o.select_set(True)
    bpy.ops.object.modifier_apply(modifier=mod.name)
    o.name = o.data.name = "SM_Ing_" + name
    log("{} {} -> {} faces".format(name, faces, len(o.data.polygons)))
    return o


def export(o):
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, o.name + ".fbx"), use_selection=True, object_types={"MESH"},
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL", mesh_smooth_type="FACE", path_mode="STRIP")
    pts = [v.co for v in o.data.vertices]
    log("{} exported, size {}".format(o.name, [round(max(p[i] for p in pts) - min(p[i] for p in pts), 3) for i in range(3)]))


def doppelsichel():
    o = load(Q + "/Ruestung/06_waffen/02_tripo/Doppelsichel.glb", "Doppelsichel", 9000, 1024)
    # the hub (read from the front picture) to the origin, the thin axis (Y) up, 55 cm high
    o.data.transform(Matrix.Translation(Vector((0.044, 0.0, -0.505))))
    o.data.transform(Matrix.Scale(0.55, 4) @ Matrix.Rotation(math.radians(90), 4, "X"))
    export(o)


def quarzbrille():
    o = load(Q + "/Quarzbrille/02_tripo/Quarzbrille.glb", "Quarzbrille", 12000, 1024)
    # point between the eyes: the lens centres 0.484 apart at z 0.172, the face side of the cups at y -0.297 (front = -Y)
    eye = Vector((0.0, -0.26, 0.172))
    scale = 0.065 / 0.484
    bm = bmesh.new()
    bm.from_mesh(o.data)
    for v in bm.verts:
        p = (v.co - eye) * scale
        # behind the cups the braided band goes round the head: wider and twice as deep, eased in from the cups backwards
        back = max(0.0, min(1.0, (p.y - 0.0) / 0.03))
        p.x *= 1.0 + 0.28 * back
        p.y *= 1.0 + 1.0 * back
        # front (-Y) ahead (+X)
        v.co = Vector((-p.y, p.x, p.z))
    bm.to_mesh(o.data)
    bm.free()
    # The glass on its own material slot "Glas" (26.11., the Wärmeglas, Rauchglas and Aderglas tint it in the game): the domes
    # in the cups and the flipped-up magnifier, found by place (game metres, origin between the eyes)
    glass = bpy.data.materials.new("Glas")
    o.data.materials.append(glass)
    centres = [(Vector((0.0, side * 0.0325, -0.0015)), 0.0168, -0.01) for side in (-1.0, 1.0)] + [(Vector((0.0, 0.0317, 0.039)), 0.0105, -1.0)]
    count = 0
    for poly in o.data.polygons:
        c = poly.center
        for centre, radius, min_x in centres:
            if Vector((0.0, c.y - centre.y, c.z - centre.z)).length < radius and c.x > min_x:
                poly.material_index = 1
                count += 1
                break
    log("Quarzbrille: {} faces of glass".format(count))
    export(o)
    # a look at which faces became glass: front view, glass red
    glass.diffuse_color = (1.0, 0.1, 0.05, 1.0)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.color_type = "MATERIAL"
    scene.render.resolution_x, scene.render.resolution_y = 800, 500
    cam_data = bpy.data.cameras.new("C"); cam_data.type = "ORTHO"; cam_data.ortho_scale = 0.18
    cam = bpy.data.objects.new("C", cam_data); scene.collection.objects.link(cam); scene.camera = cam
    cam.location = (0.5, 0.0, 0.01); cam.rotation_euler = (math.radians(90), 0.0, math.radians(90))
    scene.render.filepath = os.path.join(OUT, "Quarzbrille_Glas.png")
    bpy.ops.render.render(write_still=True)


os.makedirs(OUT, exist_ok=True)
if not os.environ.get("VAEL_NUR_BRILLE"):
    doppelsichel()
quarzbrille()
log("done")
