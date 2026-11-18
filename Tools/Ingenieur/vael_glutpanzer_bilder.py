"""
Vael - Bilder des Glutpanzers fuer das Menue des Ingenieurs (Boris 17.11.)
=========================================================================

1. Ein Bild je Ruestungsteil, wie beim Schmied in Monster Hunter: das echte Teil aus dem Rig (05_rig), mit seinen Texturen,
   schraeg von vorn, freigestellt. Fuer die Kacheln der Ausruestung und die grosse Ansicht auf der Anleitung.
2. Der Bauplan: der ganze Glutpanzer von vorn als Glutzeichnung, wie die Konstruktionszeichnungen bei Iron Man, aber
   geschmiedet statt digital: Kupferlinien (Freestyle) fuer Umriss und Kanten, die Glutrisse der Platten als glimmende Linien,
   freigestellt. Das Spiel legt ihn auf die geschwaerzte Platte im Deckel des Werkzeugkastens.

Aufruf:
  blender -b --factory-startup -P vael_glutpanzer_bilder.py -- <Zielordner>
"""
import json
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Vector

RIG = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/05_rig/SKM_Ing_GP_{}.fbx"
TEX = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/04_export/Texturen"

# The pieces of the set by EVaelArmorPlace and the rig files they are made of
PIECES = [
    ("Helm", ["Kopf"]),
    ("Brust", ["Brust"]),
    ("Arme", ["Arme"]),
    ("Beine", ["Beine"]),
    ("Schild", ["Schild"]),
]

# Where each piece is looked at from (rig space: +X the figure's left, -Y its front, Z up)
VIEWS = {
    "Helm": Vector((0.55, -1.0, 0.25)),
    "Brust": Vector((0.45, -1.0, 0.2)),
    "Arme": Vector((0.25, -1.0, 0.3)),
    "Beine": Vector((0.4, -1.0, 0.2)),
    "Schild": Vector((0.5, 1.0, 0.35)),
}


def log(text):
    print("BILDER " + text, flush=True)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def imported(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    return [o for o in bpy.data.objects if o not in before]


def mesh_only(objs):
    """The mesh of an import with the armature at rest applied, one UV map, the rest deleted"""
    mesh = [o for o in objs if o.type == "MESH"][0]
    deps = bpy.context.evaluated_depsgraph_get()
    data = bpy.data.meshes.new_from_object(mesh.evaluated_get(deps), preserve_all_data_layers=True, depsgraph=deps)
    data.transform(mesh.matrix_world)
    for extra in list(data.uv_layers)[1:]:
        data.uv_layers.remove(extra)
    if data.uv_layers:
        data.uv_layers[0].name = "UVMap"
    o = bpy.data.objects.new(mesh.name + "_fest", data)
    bpy.context.scene.collection.objects.link(o)
    for old in objs:
        bpy.data.objects.remove(old)
    for poly in data.polygons:
        poly.use_smooth = True
    return o


def textured(objs):
    """Every GP_Teil slot gets its own texture; the cracks glow a little"""
    done = set()
    for o in objs:
        for slot in o.material_slots:
            m = slot.material
            if m is None or m.name in done:
                continue
            done.add(m.name)
            path = os.path.join(TEX, m.name + ".png")
            m.use_nodes = True
            nodes = m.node_tree.nodes
            for other in [x for x in nodes if x.type == "TEX_IMAGE"]:
                nodes.remove(other)
            bsdf = next(n for n in nodes if n.type == "BSDF_PRINCIPLED")
            bsdf.inputs["Roughness"].default_value = 0.55
            bsdf.inputs["Metallic"].default_value = 0.25
            if not os.path.exists(path):
                continue
            tex = nodes.new("ShaderNodeTexImage")
            tex.image = bpy.data.images.load(path, check_existing=True)
            m.node_tree.links.new(tex.outputs[0], bsdf.inputs["Base Color"])
            # the red of the cracks as light: red well above green and blue
            sep = nodes.new("ShaderNodeSeparateColor")
            m.node_tree.links.new(tex.outputs[0], sep.inputs[0])
            sub = nodes.new("ShaderNodeMath")
            sub.operation = "SUBTRACT"
            m.node_tree.links.new(sep.outputs[0], sub.inputs[0])
            m.node_tree.links.new(sep.outputs[1], sub.inputs[1])
            ramp = nodes.new("ShaderNodeMapRange")
            ramp.inputs["From Min"].default_value = 0.12
            ramp.inputs["From Max"].default_value = 0.4
            m.node_tree.links.new(sub.outputs[0], ramp.inputs["Value"])
            bsdf.inputs["Emission Color"].default_value = (1.0, 0.32, 0.06, 1.0)
            m.node_tree.links.new(ramp.outputs["Result"], bsdf.inputs["Emission Strength"])


def load_piece(files):
    return [mesh_only(imported(RIG.format(f))) for f in files]


def bounds(objs):
    pts = [o.matrix_world @ v.co for o in objs for v in o.data.vertices]
    lo = Vector([min(p[i] for p in pts) for i in range(3)])
    hi = Vector([max(p[i] for p in pts) for i in range(3)])
    return lo, hi


def camera(look_from, centre, ortho, w, h):
    s = bpy.context.scene
    cd = bpy.data.cameras.new("C")
    cd.type = "ORTHO"
    cd.ortho_scale = ortho
    cam = bpy.data.objects.new("C", cd)
    s.collection.objects.link(cam)
    s.camera = cam
    cam.location = centre + look_from.normalized() * 20.0
    cam.rotation_euler = (centre - cam.location).to_track_quat("-Z", "Y").to_euler()
    s.render.resolution_x, s.render.resolution_y = w, h
    return cam


def lights(centre, size):
    s = bpy.context.scene

    def light(name, offset, energy, color, radius):
        data = bpy.data.lights.new(name, "AREA")
        data.energy = energy * size * size
        data.color = color
        data.size = radius * size
        o = bpy.data.objects.new(name, data)
        o.location = centre + offset * size
        o.rotation_euler = (centre - o.location).to_track_quat("-Z", "Y").to_euler()
        s.collection.objects.link(o)

    light("Key", Vector((-1.6, -2.2, 2.0)), 60.0, (1.0, 0.86, 0.72), 1.2)
    light("Fill", Vector((2.2, -1.6, 0.2)), 14.0, (0.7, 0.78, 1.0), 2.0)
    light("Glut", Vector((0.0, 2.4, -0.6)), 70.0, (1.0, 0.42, 0.12), 1.5)
    world = bpy.data.worlds.new("W")
    s.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.05, 0.04, 0.035, 1.0)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.15


def setup_render():
    s = bpy.context.scene
    s.render.engine = "BLENDER_EEVEE"
    s.render.film_transparent = True
    s.render.image_settings.file_format = "PNG"
    s.render.image_settings.color_mode = "RGBA"
    s.view_settings.view_transform = "Standard"
    s.view_settings.exposure = 0.0
    try:
        s.eevee.use_bloom = True
    except AttributeError:
        pass


def piece_pictures(folder):
    for name, files in PIECES:
        reset()
        setup_render()
        objs = load_piece(files)
        if name == "Arme":
            # one arm, as item pictures show it: the two stand a body apart
            import bmesh
            for o in objs:
                bm = bmesh.new()
                bm.from_mesh(o.data)
                bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.x > -0.25], context="VERTS")
                bm.to_mesh(o.data)
                bm.free()
                o.data.update()
        textured(objs)
        lo, hi = bounds(objs)
        centre = (lo + hi) / 2
        size = max(hi - lo)
        look = VIEWS[name]
        camera(look, centre, size * 1.12, 512, 512)
        lights(centre, size)
        path = os.path.join(folder, "T_Ruestung_Schwarzglut_{}.png".format(name))
        bpy.context.scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        log("Teil {}: {}".format(name, path))


def blueprint(folder):
    """The whole Glutpanzer from the front as an ember drawing, and where each piece sits on it (0..1 of the picture)"""
    reset()
    setup_render()
    s = bpy.context.scene
    objs = []
    places = {}
    for name, files in PIECES:
        if name == "Schild":
            continue
        made = load_piece(files + (["UnterarmL"] if name == "Arme" else []))
        objs += made
        lo, hi = bounds(made)
        places[name] = ((lo + hi) / 2, lo, hi)
    lo, hi = bounds(objs)
    centre = (lo + hi) / 2
    height = (hi.z - lo.z) * 1.04
    w, h = 900, 1100
    ortho = max(height, (hi.x - lo.x) * 1.04 * h / w)  # the taller side of the picture is the ortho scale
    span_x = ortho * w / h
    cam = camera(Vector((0.0, -1.0, 0.0)), centre, ortho, w, h)

    # pass 1: the cracks of the texture, flat, to be lifted out
    textured(objs)
    s.render.filepath = os.path.join(folder, "_bauplan_farbe.png")
    lights(centre, height)
    bpy.ops.render.render(write_still=True)

    # pass 2: lines only, every surface held out
    hold = bpy.data.materials.new("Leer")
    hold.use_nodes = True
    nt = hold.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(nt.nodes.new("ShaderNodeHoldout").outputs[0], out.inputs[0])
    for o in objs:
        o.data.materials.clear()
        o.data.materials.append(hold)
    s.render.use_freestyle = True
    s.render.line_thickness_mode = "ABSOLUTE"
    s.render.line_thickness = 1.6
    layer = s.view_layers[0]
    layer.use_freestyle = True
    fs = layer.freestyle_settings
    fs.crease_angle = math.radians(128.0)
    ls = fs.linesets[0] if fs.linesets else fs.linesets.new("Linien")
    ls.select_silhouette = True
    ls.select_border = True
    ls.select_crease = True
    ls.select_external_contour = True
    if ls.linestyle is None:
        ls.linestyle = bpy.data.linestyles.new("Glut")
    ls.linestyle.color = (1.0, 1.0, 1.0)
    ls.linestyle.thickness = 1.6
    s.render.filepath = os.path.join(folder, "_bauplan_linien.png")
    bpy.ops.render.render(write_still=True)

    # the drawing: copper lines, ember cracks with a soft glow, a faint fill where the figure stands
    def load(path):
        img = bpy.data.images.load(path)
        a = np.array(img.pixels[:], dtype=np.float32).reshape(img.size[1], img.size[0], 4)
        bpy.data.images.remove(img)
        return a

    color = load(os.path.join(folder, "_bauplan_farbe.png"))
    lines = load(os.path.join(folder, "_bauplan_linien.png"))
    line = np.clip(lines[..., 3] * lines[..., 0] * 1.4, 0.0, 1.0)
    r, g, b, a = color[..., 0], color[..., 1], color[..., 2], color[..., 3]
    crack = np.clip((r - np.maximum(g, b) - 0.1) * 4.0, 0.0, 1.0) * a
    glow = crack.copy()
    for _ in range(6):
        glow = (glow + np.roll(glow, 1, 0) + np.roll(glow, -1, 0) + np.roll(glow, 1, 1) + np.roll(glow, -1, 1)) / 5.0
    fill = a * 0.1
    copper = np.array([0.95, 0.58, 0.26])
    ember = np.array([1.0, 0.42, 0.1])
    hot = np.array([1.0, 0.82, 0.5])
    rgb = np.zeros(color.shape[:2] + (3,), np.float32)
    alpha = np.clip(fill + line + crack * 0.9 + glow * 0.8, 0.0, 1.0)
    weight = np.maximum(alpha, 1e-4)[..., None]
    rgb = (copper * line[..., None] + ember * (glow * 0.8)[..., None] + hot * (crack * 0.9)[..., None] + copper * 0.5 * fill[..., None]) / weight
    out = np.dstack([np.clip(rgb, 0, 1), alpha])
    img = bpy.data.images.new("Bauplan", out.shape[1], out.shape[0], alpha=True)
    img.pixels[:] = out.ravel()
    img.filepath_raw = os.path.join(folder, "T_Ruestung_Schwarzglut_Bauplan.png")
    img.file_format = "PNG"
    img.save()
    os.remove(os.path.join(folder, "_bauplan_farbe.png"))
    os.remove(os.path.join(folder, "_bauplan_linien.png"))

    # where each piece lies on the picture, 0..1 from its top left, for the lines from the tiles
    left = centre.x - span_x / 2
    top = centre.z + ortho / 2
    spots = {}
    for name, (mid, plo, phi) in places.items():
        spots[name] = {"x": round((mid.x - left) / span_x, 4), "y": round((top - mid.z) / ortho, 4)}
    with open(os.path.join(folder, "bauplan_stellen.json"), "w") as f:
        json.dump(spots, f, indent=1)
    log("Bauplan {}".format(spots))


def main():
    folder = sys.argv[sys.argv.index("--") + 1]
    os.makedirs(folder, exist_ok=True)
    piece_pictures(folder)
    blueprint(folder)


main()
