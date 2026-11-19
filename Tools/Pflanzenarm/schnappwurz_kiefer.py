"""
Pflanzenarm Schnappwurz (Boris 2026-11-19): splits the closed bud of the Schnappwurz (SM_Schnappwurz_Kopf from halme_zerlegen.py,
cut from the Tripo model) into the two halves of the snapping jaw.

The bud stands on +Z with its pivot at the bottom, where the stalk joins: that is the hinge. It is cut along the plane through
its axis; each half keeps its outside and gets its cut face filled as the inside of the jaw. Both halves keep the pivot at the
hinge, so the arm only has to turn them about it to open and shut the jaw. Vertex colour and UV channel 1 carry a constant 1 like
every head (the stalk material reads it as "fully grown").

Run:
  blender -b --factory-startup -P schnappwurz_kiefer.py -- <folder with SM_Schnappwurz_Kopf.fbx>
Writes <folder>/SM_Schnappwurz_Kiefer_Oben.fbx and SM_Schnappwurz_Kiefer_Unten.fbx plus a preview Schnappwurz_kiefer.png.
"""
import bpy, bmesh, sys, os, math
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
FOLDER = args[0]


def log(text):
    print("KIEFER " + text, flush=True)


def load_head():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=os.path.join(FOLDER, "SM_Schnappwurz_Kopf.fbx"))
    obj = [o for o in bpy.context.selected_objects if o.type == "MESH"][0]
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return obj


def paint_constant(obj):
    mesh = obj.data
    for attr in list(mesh.color_attributes):
        mesh.color_attributes.remove(attr)
    attr = mesh.color_attributes.new("Col", "BYTE_COLOR", "CORNER")
    for loop in mesh.loops:
        attr.data[loop.index].color = (1.0, 1.0, 1.0, 1.0)
    mesh.color_attributes.active_color = attr
    if "Verlauf" in mesh.uv_layers:
        mesh.uv_layers.remove(mesh.uv_layers["Verlauf"])
    uv = mesh.uv_layers.new(name="Verlauf")
    for loop in mesh.loops:
        uv.data[loop.index].uv = (1.0, 1.0)


def half(source, keep_positive, name):
    """A copy of the bud cut along the plane x = 0 through its axis, keeping one side, the cut face filled"""
    obj = source.copy()
    obj.data = source.data.copy()
    bpy.context.scene.collection.objects.link(obj)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
    bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0.0, 0.0, 0.0), plane_no=(1.0, 0.0, 0.0),
                           clear_inner=keep_positive, clear_outer=not keep_positive)
    # Close the cut: the edges lying on the plane become the inside of the jaw
    cut = [e for e in bm.edges if abs(e.verts[0].co.x) < 1e-5 and abs(e.verts[1].co.x) < 1e-5 and e.is_boundary]
    if cut:
        try:
            bmesh.ops.holes_fill(bm, edges=cut, sides=0)
        except Exception as error:
            log(f"{name}: could not fill the cut ({error})")
    # The filled cut turned some faces inwards (they rendered black in the game): all normals point outwards again
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    # The lower half turned over, so both open upwards from the hinge the same way: the arm mirrors the lower one by turning it
    obj.name = name
    obj.data.name = name
    paint_constant(obj)
    return obj


def export(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(FOLDER, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                             mesh_smooth_type="FACE", colors_type="LINEAR", path_mode="COPY", embed_textures=False, add_leaf_bones=False)
    log(f"exported {path}")


def preview(objs):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
    scene.render.resolution_x = 600
    scene.render.resolution_y = 600
    scene.render.film_transparent = True
    for i, obj in enumerate(objs):
        obj.location = Vector(((i - 0.5) * 0.09, 0.0, 0.0))
        obj.rotation_euler = (0.0, 0.0, math.radians(90 if i == 0 else -90))
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    cam.location = (0.0, -0.35, 0.06)
    cam.rotation_euler = (math.radians(88), 0.0, 0.0)
    scene.camera = cam
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 4.0
    sun.rotation_euler = (math.radians(50), 0.0, math.radians(30))
    scene.collection.objects.link(sun)
    scene.render.filepath = os.path.join(FOLDER, "Schnappwurz_kiefer.png")
    bpy.ops.render.render(write_still=True)


bud = load_head()
upper = half(bud, True, "SM_Schnappwurz_Kiefer_Oben")
lower = half(bud, False, "SM_Schnappwurz_Kiefer_Unten")
bpy.data.objects.remove(bud)
for part in (upper, lower):
    log(f"{part.name}: {len(part.data.polygons)} faces, size {tuple(round(d, 3) for d in part.dimensions)}")
    export(part)
preview([upper, lower])
log("done")
