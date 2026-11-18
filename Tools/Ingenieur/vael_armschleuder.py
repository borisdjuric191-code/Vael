"""
The Panzerschleuder (Boris 16.11.): the heavy sling mounted on the left forearm instead of the Zwille in the fist, so the hands
stay free for building. Tripo model from concept Armschleuder_A (H3.0, 40 credits):
  Vael_Quellen/Ingenieur/Ruestung/06_waffen/02_tripo/Armschleuder.glb
It lies flat in Tripo's space (Blender after the glTF import: the long axis +Y towards the fork, Z up, length 1).

Cut into two pieces, so the fork can slide forward along the bracer while he aims (Boris: "die Zwille beim Zielen nach vorne
gleiten lassen, da es im Bild weit hinten liegt"):
  SM_Ing_Panzerschleuder_Schiene  the bracer, strapped along the forearm, up to the collar at the wrist
  SM_Ing_Panzerschleuder_Gabel    the stem with the ember stone and the two horn prongs
The band and the pouch Tripo baked between the prongs are cut away: the game stretches its own band to the right hand.

Space of both pieces (Unreal, cm after the FBX scale): X ahead along the forearm, Y across, Z up (the thumb side); the origin is
the wrist on the forearm's axis, so both sit right on the forearm bone at rest. The tie points of the band are logged
(SCHLEUDER tie) for UVaelEngineerComponent.

Out: C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/06_waffen/SM_Ing_Panzerschleuder_*.fbx, T_Ing_Panzerschleuder.png, previews.
  blender -b --factory-startup -P vael_armschleuder.py
"""
import bpy, bmesh, math, os
from mathutils import Matrix, Vector

SRC = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/06_waffen/02_tripo/Armschleuder.glb"
OUT = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/06_waffen"
DECIMATE = 0.06
LENGTH = 0.66          # metres from the bracer's back end to the prong tips
CUT_Y = -0.02          # Tripo length units: the collar between bracer and stem
BAND_X = 0.115         # within this of the middle, above BAND_Y, lie the baked band and pouch
BAND_Y = 0.175
TIE_BELOW_TIP = 0.13   # the band is tied at the wraps this far below each tip (Tripo units)
# where the fork sits relative to the bracer, metres (the game uses the same numbers)
FORK_REST = Vector((-0.30, 0.0, 0.07))
FORK_AIM = Vector((0.0, 0.0, 0.07))


def log(text):
    print("SCHLEUDER " + text, flush=True)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=SRC)
    src = [o for o in bpy.data.objects if o.type == "MESH"][0]
    src.data.transform(src.matrix_world)
    src.matrix_world = Matrix.Identity(4)
    for o in [o for o in bpy.data.objects if o != src]:
        bpy.data.objects.remove(o)
    mod = src.modifiers.new("Weniger", "DECIMATE")
    mod.ratio = DECIMATE
    bpy.context.view_layer.objects.active = src
    src.select_set(True)
    bpy.ops.object.modifier_apply(modifier=mod.name)

    verts = [v.co.copy() for v in src.data.vertices]
    # the forearm's axis: the middle of the bracer's cross-section
    back = [v for v in verts if v.y < CUT_Y - 0.05]
    axis_x = (min(v.x for v in back) + max(v.x for v in back)) * 0.5
    axis_z = (min(v.z for v in back) + max(v.z for v in back)) * 0.5
    length = max(v.y for v in verts) - min(v.y for v in verts)
    scale = LENGTH / length
    # the prong tips and the tie points below them
    ties = []
    for side in (-1, 1):
        prong = [v for v in verts if v.x * side > 0.1]
        tip = max(prong, key=lambda v: v.y)
        near = [v for v in prong if abs(v.y - (tip.y - TIE_BELOW_TIP)) < 0.02]
        ties.append(Vector((sum(v.x for v in near) / len(near), tip.y - TIE_BELOW_TIP, sum(v.z for v in near) / len(near))))

    # Tripo (x, y, z) -> piece: X = y - cut, Y = -(x - axis), Z = z - axis, then scaled to metres
    to_piece = Matrix.Scale(scale, 4) @ Matrix(((0, 1, 0, -CUT_Y), (-1, 0, 0, axis_x), (0, 0, 1, -axis_z), (0, 0, 0, 1)))
    for t in ties:
        p = to_piece @ t
        log("tie {:.1f} {:.1f} {:.1f} cm".format(p.x * 100, p.y * 100, p.z * 100))
    log("bracer radius {:.1f} cm, length {:.1f} cm".format((max(v.z for v in back) - min(v.z for v in back)) * 0.5 * scale * 100, LENGTH * 100))

    pieces = {}
    for name, keep in (("Schiene", lambda v: v.y < CUT_Y),
                       ("Gabel", lambda v: v.y >= CUT_Y and not (abs(v.x) < BAND_X and v.y > BAND_Y))):
        o = src.copy()
        o.data = src.data.copy()
        o.name = o.data.name = "SM_Ing_Panzerschleuder_" + name
        bpy.context.scene.collection.objects.link(o)
        bm = bmesh.new()
        bm.from_mesh(o.data)
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if not keep(v.co)], context="VERTS")
        # loose crumbs left of the cut band
        loose = []
        for island in _islands(bm):
            if len(island) < 40:
                loose.extend(island)
        bmesh.ops.delete(bm, geom=loose, context="VERTS")
        bm.to_mesh(o.data)
        bm.free()
        o.data.transform(to_piece)
        pieces[name] = o
        log("{}: {} tris".format(o.name, sum(len(p.vertices) - 2 for p in o.data.polygons)))
    bpy.data.objects.remove(src)

    # the colour texture of the Tripo material, for the Glutpanzer master material in Unreal
    mat = pieces["Gabel"].material_slots[0].material
    for node in mat.node_tree.nodes:
        if node.type == "TEX_IMAGE" and any(l.to_socket.name == "Base Color" for l in node.outputs[0].links):
            img = node.image
            img.filepath_raw = os.path.join(OUT, "T_Ing_Panzerschleuder.png")
            img.file_format = "PNG"
            img.save()
            log("texture {}x{}".format(*img.size))
    for o in pieces.values():
        o.material_slots[0].material.name = "M_Ing_Panzerschleuder"
        bpy.ops.object.select_all(action="DESELECT")
        o.select_set(True)
        bpy.context.view_layer.objects.active = o
        bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, o.name + ".fbx"), use_selection=True, object_types={"MESH"},
                                 apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL", mesh_smooth_type="FACE",
                                 path_mode="STRIP", axis_forward="X", axis_up="Z")

    # preview with a grey forearm and fist, the fork once at rest and once slid forward
    bpy.ops.mesh.primitive_cylinder_add(radius=0.05, depth=0.30, location=(-0.15, 0, 0), rotation=(0, math.radians(90), 0))
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.055, location=(0.06, 0, 0))
    s = bpy.context.scene
    s.render.engine = "BLENDER_WORKBENCH"
    s.display.shading.light = "STUDIO"
    s.display.shading.color_type = "TEXTURE"
    s.render.resolution_x, s.render.resolution_y = 1100, 800
    cd = bpy.data.cameras.new("C"); cd.type = "ORTHO"; cd.ortho_scale = 1.0
    cam = bpy.data.objects.new("C", cd); s.collection.objects.link(cam); s.camera = cam
    focus = Vector((0.0, 0, 0.05))
    # as in the game (UVaelEngineerComponent, SlingFork*): the fork rides on top of the bracer, pulled back over the forearm at
    # rest and slid forward over the fist while he aims
    for slide in (FORK_REST, FORK_AIM):
        pieces["Gabel"].location = slide
        for view, d in (("schraeg", Vector((0.7, -1.0, 0.8))), ("seite", Vector((0, -1, 0.05)))):
            cam.location = focus + d.normalized() * 2.0
            cam.rotation_euler = (focus - cam.location).to_track_quat("-Z", "Y").to_euler()
            s.render.filepath = os.path.join(OUT, "vorschau_{}_{}.png".format(view, "vor" if slide == FORK_AIM else "ruhe"))
            bpy.ops.render.render(write_still=True)
    log("done")


def _islands(bm):
    seen = set()
    for v in bm.verts:
        if v.index in seen:
            continue
        stack, island = [v], []
        seen.add(v.index)
        while stack:
            cur = stack.pop()
            island.append(cur)
            for e in cur.link_edges:
                o = e.other_vert(cur)
                if o.index not in seen:
                    seen.add(o.index)
                    stack.append(o)
        yield island


main()
