"""
The Panzerkugel (Boris 17.11.): with the whole Schwarzglut set the engineer rolls up like an Aschenroller to dodge, and rolls
on while the dodge is held. Built from the curled Aschenroller (SM_AschenrollerKugel) as the core, "überzogen" with the
Glutpanzer's own back shell (SKM_Ing_GP_Schild) twice, once over the top and once under the bottom, its plate rows
wrapped around the axle like the bands of the curled animal; at each end of the axle a few spikes of the pauldrons (Boris
17.11., after a try with the whole armour laid on it: "lieber doch nur Schildplatten sauber eingerollt und seitlich die paar
Zacken, weniger ist mehr"). The core shows only at the ends of the axle, around the spikes.

The figure of the rig is 2 m tall with the horns; curled up it makes a ball of 1.2 m.

Space (Unreal, cm after the FBX scale): X ahead (the way it rolls), Y the axle, Z up, origin in the middle of the ball. The
game turns it about Y by the way rolled, like the Aschenroller's ball.

Out: C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/07_kugel/SM_Ing_Panzerkugel.fbx and previews.
  blender -b --factory-startup -P vael_panzerkugel.py
"""
import bpy, bmesh, math, os
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

CORE = "C:/Projekte/Vael_Quellen/AschenrollerKugel/04_export/SM_AschenrollerKugel.fbx"
SHELL = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/05_rig/SKM_Ing_GP_Schild.fbx"
TEX = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/04_export/Texturen"
OUT = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/07_kugel"
RIG = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/05_rig/SKM_Ing_GP_{}.fbx"
RADIUS = 0.6           # metres, the rolling radius of the ball (the core's surface); the game uses the same number
SHELLS = (("Oben", 0.0), ("Unten", 180.0))  # the two shells and the PHI of their middles
BETA = 92.0            # degrees around the axle each shell covers on each side of its middle (neck to hip tip)
LAMBDA = 54.0          # degrees from the rolling plane towards each end of the axle the shell's width covers
ROW_FIT = 0.6          # 0 keeps the shell's oval outline, 1 stretches every row to the full width
THICK = 0.9            # plate relief, relative to the shell's own
GAP = 0.004            # metres between the core and the underside of the plates
PLATE = 0.03           # metres, thickness of the plates over the core
BINS = 48
CULL = -0.15         # faces turned further from a piece's outside than this are left off


def log(text):
    print("KUGEL " + text, flush=True)


def imported(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    return [o for o in bpy.data.objects if o not in before]


def mesh_only(objs):
    """The mesh of an import, with every modifier (the armature at rest) applied and the rest deleted"""
    mesh = [o for o in objs if o.type == "MESH"][0]
    deps = bpy.context.evaluated_depsgraph_get()
    data = bpy.data.meshes.new_from_object(mesh.evaluated_get(deps), preserve_all_data_layers=True, depsgraph=deps)
    data.transform(mesh.matrix_world)
    # one name for the UV map of every piece: the core calls its own "UVMap", the rig "Attribute", and joining meshes whose
    # maps differ in name keeps only the active one, which left every plate on a single texel of its atlas
    for extra in list(data.uv_layers)[1:]:
        data.uv_layers.remove(extra)
    if data.uv_layers:
        data.uv_layers[0].name = "UVMap"
    o = bpy.data.objects.new(mesh.name + "_fest", data)
    bpy.context.scene.collection.objects.link(o)
    for old in objs:
        bpy.data.objects.remove(old)
    return o


# The armour on the ball: Tripo parts of the rig (GP_Teil_<n>, rig space: +X the figure's left, -Y its front, Z up, metres),
# which side of them faces out, which way along them runs along the ball, where they lie (PHI around the axle, LAMBDA
# towards the end of the axle, + the figure's left), which way along the circle (PHI growing or shrinking), how far they sink
# into the ball, their scale, and how flat their height is pressed (limb plates go round the limb: pressed flat, they
# lie on the ball like plates instead of standing off it as half pipes)
LIMB_L = tuple(Vector((0.25, 0.0, -0.3)).normalized())
LIMB_R = tuple(Vector((-0.25, 0.0, -0.3)).normalized())
DETAILS = [
    # name,            parts,     out,             along,       PHI,  LAMBDA, way, sink, scale, flat
    ("Zacken_L_vorn",  [9],       (0.3, 0.0, 1.0), (0, -1, 0),   35.0,  66.0,  1, 0.06, 1.0, 1.0),
    ("Zacken_L_hinten", [9],      (0.3, 0.0, 1.0), (0, -1, 0),  215.0,  66.0,  1, 0.06, 1.0, 1.0),
    ("Zacken_R_vorn",  [37, 40],  (-0.3, 0.0, 1.0), (0, -1, 0),  35.0, -66.0,  1, 0.06, 1.0, 1.0),
    ("Zacken_R_hinten", [37, 40], (-0.3, 0.0, 1.0), (0, -1, 0), 215.0, -66.0,  1, 0.06, 1.0, 1.0),
]


def ball_dir(phi, lam):
    p, l = math.radians(phi), math.radians(lam)
    return Vector((math.sin(p) * math.cos(l), math.sin(l), math.cos(p) * math.cos(l)))


def lay_armour(surface):
    """Every detail cut out of the rig by its parts' material slots, turned so its outside faces out of the ball, and bent
    around the core: along it follows the ball's circle, across it the ends of the axle, its height stays relief"""
    sources = [mesh_only(imported(RIG.format(piece))) for piece in ("Brust",)]
    laid = []
    for name, parts, out, along, phi, lam, way, sink, scale, flat in DETAILS:
        wanted = {"GP_Teil_{}".format(n) for n in parts}
        made = []
        for src in sources:
            slots = [i for i, slot in enumerate(src.material_slots) if slot.material is not None and slot.material.name in wanted]
            if not slots:
                continue
            o = src.copy()
            o.data = src.data.copy()
            bpy.context.scene.collection.objects.link(o)
            bm = bmesh.new()
            bm.from_mesh(o.data)
            # only the outer half of each piece: limb plates go all round the limb, and their inner half would only stand
            # out of the ball as loops
            o_out_src = Vector(out).normalized()
            bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.material_index not in slots or f.normal.dot(o_out_src) < CULL],
                             context="FACES")
            bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
            bm.to_mesh(o.data)
            bm.free()
            made.append(o)
        if not made:
            log("no parts for " + name)
            continue
        bpy.ops.object.select_all(action="DESELECT")
        for o in made:
            o.select_set(True)
        bpy.context.view_layer.objects.active = made[0]
        if len(made) > 1:
            bpy.ops.object.join()
        o = bpy.context.view_layer.objects.active
        o.name = o.data.name = "Kugel_" + name
        o_out = Vector(out).normalized()
        o_along = (Vector(along) - o_out * Vector(along).dot(o_out)).normalized()
        o_cross = o_out.cross(o_along)
        vs = [v.co.copy() for v in o.data.vertices]
        centre = sum(vs, Vector()) / len(vs)
        heights = [(v - centre).dot(o_out) for v in vs]
        low = min(heights)
        n0 = ball_dir(phi, lam)
        t0 = (ball_dir(phi + way * 1.0, lam) - n0).normalized()
        axis0 = n0.cross(t0)
        for v, src_co, h in zip(o.data.vertices, vs, heights):
            rel = (src_co - centre) * scale
            a, c = rel.dot(o_along), rel.dot(o_cross)
            q1 = Matrix.Rotation(a / RADIUS, 3, axis0)
            d1 = q1 @ n0
            t1 = q1 @ t0
            d = (Matrix.Rotation(-c / RADIUS, 3, t1) @ d1).normalized()
            v.co = d * (surface(d) + GAP + (h - low) * scale * flat - sink)
        laid.append(o)
        log("{}: {} verts".format(name, len(o.data.vertices)))
    for src in sources:
        bpy.data.objects.remove(src)
    return laid


def main():
    os.makedirs(OUT, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)

    # the core: its narrow axis (Blender X, 0.97 against 1.2 and 1.24) is the axle; turned so the axle is Y, centred,
    # scaled to the rolling radius
    core = mesh_only(imported(CORE))
    vs = [v.co for v in core.data.vertices]
    mid = Vector([(min(v[i] for v in vs) + max(v[i] for v in vs)) * 0.5 for i in range(3)])
    roll = max(max(v.y for v in vs) - min(v.y for v in vs), max(v.z for v in vs) - min(v.z for v in vs)) * 0.5
    core.data.transform(Matrix.Rotation(math.radians(90), 4, "Z") @ Matrix.Scale(RADIUS / roll, 4) @ Matrix.Translation(-mid))
    core.name = core.data.name = "Kern"
    tree = BVHTree.FromObject(core, bpy.context.evaluated_depsgraph_get())

    def surface(d):
        hit = tree.ray_cast(d * RADIUS * 3.0, -d)
        return hit[0].length if hit[0] is not None else RADIUS

    # the shell: height along Z (hip tip low, neck high), width along X, bulging out to +Y
    shell = mesh_only(imported(SHELL))
    vs = [v.co.copy() for v in shell.data.vertices]
    x0 = (min(v.x for v in vs) + max(v.x for v in vs)) * 0.5
    z0 = (min(v.z for v in vs) + max(v.z for v in vs)) * 0.5
    half_w = (max(v.x for v in vs) - min(v.x for v in vs)) * 0.5
    half_h = (max(v.z for v in vs) - min(v.z for v in vs)) * 0.5
    size = max(half_w, half_h)
    uv = [((v.z - z0) / half_h, (v.x - x0) / half_w) for v in vs]
    # width of each row, and the outer face of the plates in each cell (only the relief above it is kept)
    row = [[1e9, -1e9] for _ in range(BINS)]
    outer = {}
    cell = lambda u, w: (min(int((u + 1) * 0.5 * BINS), BINS - 1), min(int((w + 1) * 0.5 * BINS), BINS - 1))
    for (u, w), v in zip(uv, vs):
        r = row[cell(u, w)[0]]
        r[0], r[1] = min(r[0], w), max(r[1], w)
        k = cell(u, w)
        outer[k] = max(outer.get(k, -1e9), v.y)
    for i in range(BINS):
        if row[i][0] > row[i][1]:
            row[i] = [-1, 1]

    # the shell's dome as a smooth surface over (u, w), fitted to the outer face of each cell: only what stands above or
    # below it (the plates, their rims and grooves) is kept as relief, the dome itself is replaced by the ball
    keys = list(outer)
    U = np.array([(k[0] + 0.5) / BINS * 2 - 1 for k in keys]); W = np.array([(k[1] + 0.5) / BINS * 2 - 1 for k in keys])
    terms = lambda u, w: np.stack([u ** i * w ** j for i in range(5) for j in range(5) if i + j <= 4], axis=-1)
    coef = np.linalg.lstsq(terms(U, W), np.array([outer[k] for k in keys]), rcond=None)[0]
    dome = lambda u, w: float(terms(np.array(u), np.array(w)) @ coef)

    # how much each vertex faces out of the shell (its normal towards +Y)
    normals = [v.normal.y for v in shell.data.vertices]

    pieces = [core]
    for name, middle in SHELLS:
        o = shell.copy()
        o.data = shell.data.copy()
        o.name = o.data.name = "Panzer_" + name
        bpy.context.scene.collection.objects.link(o)
        for v, (u, w), src, n in zip(o.data.vertices, uv, vs, normals):
            lo, hi = row[cell(u, w)[0]]
            fitted = (w - (lo + hi) * 0.5) / max((hi - lo) * 0.5, 0.05)
            w2 = w + (fitted - w) * ROW_FIT
            b = math.radians(middle + BETA * u)
            l = math.radians(LAMBDA) * max(-1.0, min(1.0, w2))
            d = Vector((math.sin(b) * math.cos(l), math.sin(l), math.cos(b) * math.cos(l)))
            # the plates' undersides lie on the core; their faces stand a plate's thickness above it, with the grooves and
            # rims of the shell as a bounded relief, and the sides in between become the plates' bevelled edges
            relief = max(-0.8 * PLATE, min(0.03, (src.y - dome(u, w)) / size * RADIUS * THICK))
            face = max(0.0, min(1.0, (n + 0.35) / 0.7))
            v.co = d * (surface(d) + GAP + face * (PLATE + relief))
        pieces.append(o)
    bpy.data.objects.remove(shell)
    pieces += lay_armour(surface)

    # one mesh: core slot first, then the shell's part slots (GP_Teil_<n>, the game's MI_GP_Teil_<n>)
    bpy.ops.object.select_all(action="DESELECT")
    for o in pieces:
        o.select_set(True)
    bpy.context.view_layer.objects.active = core
    bpy.ops.object.join()
    ball = bpy.context.view_layer.objects.active
    ball.name = ball.data.name = "SM_Ing_Panzerkugel"
    # the rig pieces bring every slot of theirs along, used or not
    bpy.ops.object.material_slot_remove_unused()
    log("verts {}, slots {}".format(len(ball.data.vertices), [s.material.name for s in ball.material_slots]))
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "SM_Ing_Panzerkugel.fbx"), use_selection=True, object_types={"MESH"},
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL", mesh_smooth_type="FACE",
                             path_mode="STRIP", axis_forward="X", axis_up="Z")

    for slot in ball.material_slots:
        path = os.path.join(TEX, slot.material.name + ".png")
        if slot.material.name.startswith("GP_Teil_") and os.path.exists(path):
            m = slot.material
            m.use_nodes = True
            nodes = m.node_tree.nodes
            tex = nodes.new("ShaderNodeTexImage")
            tex.image = bpy.data.images.load(path)
            bsdf = next(n for n in nodes if n.type == "BSDF_PRINCIPLED")
            m.node_tree.links.new(tex.outputs[0], bsdf.inputs["Base Color"])
            for other in [x for x in nodes if x.type == "TEX_IMAGE" and x != tex]:
                nodes.remove(other)
            nodes.active = tex
    s = bpy.context.scene
    s.render.engine = "BLENDER_WORKBENCH"
    s.display.shading.light = "STUDIO"
    s.display.shading.color_type = "TEXTURE"
    s.display.shading.show_backface_culling = True
    s.view_settings.exposure = 1.6
    s.render.resolution_x, s.render.resolution_y = 800, 800
    cd = bpy.data.cameras.new("C"); cd.type = "ORTHO"; cd.ortho_scale = RADIUS * 2.6
    cam = bpy.data.objects.new("C", cd); s.collection.objects.link(cam); s.camera = cam
    for view, d in (("vorne", Vector((1, 0, 0.15))), ("seite", Vector((0, -1, 0.15))), ("schraeg", Vector((1, -0.8, 0.6))),
                    ("unten", Vector((-0.6, 0.7, -0.5)))):
        cam.location = d.normalized() * 10
        cam.rotation_euler = (-cam.location).to_track_quat("-Z", "Y").to_euler()
        s.render.filepath = os.path.join(OUT, "vorschau_" + view + ".png")
        bpy.ops.render.render(write_still=True)
    log("done")


main()
