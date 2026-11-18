"""
The female Glutpanzer (Boris 17.11.): Tripo H3.0 from concept Frau B front and back, without "in parts" (40 credits).
Tripo still delivers it as loose plates (about 150 islands), so this script sorts the islands into the armour slots like the
male rig (vael_glutpanzer_rig.py): Kopf, Brust, Arme, Beine and Schild (the back shell, the engineer's shield).

The rig itself waits for a female MetaHuman base; until then the pieces are saved sorted, with a coloured preview.

Glb space after the import: Z up, height about 1, front towards -Y, X to the character's left.
Out: C:/Projekte/Vael_Quellen/Ingenieur/Ruestung_Frau/03_teile/Glutpanzer_Frau_Teile.blend, Teile.json and previews.
  blender -b --factory-startup -P vael_glutpanzer_frau_teile.py
"""
import bpy, bmesh, json, math, os
from mathutils import Vector
from mathutils.kdtree import KDTree

SRC = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung_Frau/02_tripo/Glutpanzer_Frau.glb"
OUT = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung_Frau/03_teile"
COLOURS = {"Kopf": (0.9, 0.8, 0.2), "Brust": (0.8, 0.2, 0.2), "Arme": (0.2, 0.5, 0.9), "Beine": (0.3, 0.8, 0.3),
           "Schild": (0.9, 0.4, 0.9)}
# hand-sorted islands after the candidate sheets (kandidaten_*.png): island number -> piece (numbers are by size, largest
# first, stable for this GLB). The shell: four big quadrants (8, 14, 18 and the shell half of 0), the neck tip (48, 67) and
# the rim plates at the bottom (15, 50, 59, 69, 70, 86, 119). 12 is a collar plate, not helmet.
SHELL = [8, 14, 18, 15, 48, 50, 59, 67, 69, 70, 86, 119]
OVERRIDE = {**{n: "Schild" for n in SHELL}, 12: "Brust"}
# island 0 grew together from the left pauldron and the upper left shell quadrant: a vertex goes to the shield where the
# mirrored quadrant 18 has shell within MIRROR_NEAR (the shell is symmetric, the pauldron not)
SPLIT_MIRROR = {0: 18}
MIRROR_NEAR = 0.008
# the rim islands 50, 69, 70 also carry the plate over the hips below the shell's tip: that stays on the body
SPLIT_BELOW = {50: 0.54, 69: 0.54, 70: 0.54}


def log(text):
    print("FRAU " + text, flush=True)


def classify(lo, hi, c):
    ax = abs(c.x)
    if c.z > 0.835 and ax < 0.075:
        return "Kopf"
    if ax > 0.115 and 0.38 < c.z < 0.76:
        return "Arme"
    if c.z < 0.47:
        return "Beine"
    return "Brust"


def islands(bm):
    seen = set()
    for v in bm.verts:
        if v.index in seen:
            continue
        stack, isl = [v], []
        seen.add(v.index)
        while stack:
            cur = stack.pop()
            isl.append(cur)
            for e in cur.link_edges:
                o = e.other_vert(cur)
                if o.index not in seen:
                    seen.add(o.index)
                    stack.append(o)
        yield isl


def main():
    os.makedirs(OUT, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=SRC)
    src = [o for o in bpy.data.objects if o.type == "MESH"][0]
    src.data.transform(src.matrix_world)
    src.matrix_world.identity()
    for o in [o for o in bpy.data.objects if o != src]:
        bpy.data.objects.remove(o)

    bm = bmesh.new()
    bm.from_mesh(src.data)
    found = sorted(islands(bm), key=len, reverse=True)
    mirrors = {}
    for n, ref in SPLIT_MIRROR.items():
        tree = KDTree(len(found[ref]))
        for k, v in enumerate(found[ref]):
            tree.insert(Vector((-v.co.x, v.co.y, v.co.z)), k)
        tree.balance()
        mirrors[n] = tree
    sort = {}
    table = []
    for i, isl in enumerate(found):
        cos = [v.co for v in isl]
        lo = Vector((min(p.x for p in cos), min(p.y for p in cos), min(p.z for p in cos)))
        hi = Vector((max(p.x for p in cos), max(p.y for p in cos), max(p.z for p in cos)))
        c = sum(cos, Vector()) / len(cos)
        piece = OVERRIDE.get(i) or classify(lo, hi, c)
        for v in isl:
            if i in mirrors:
                sort[v.index] = "Schild" if mirrors[i].find(v.co)[2] < MIRROR_NEAR else piece
            elif i in SPLIT_BELOW and v.co.z < SPLIT_BELOW[i]:
                sort[v.index] = "Brust"
            else:
                sort[v.index] = piece
        table.append({"insel": i, "punkte": len(isl), "teil": piece, "mitte": [round(c.x, 3), round(c.y, 3), round(c.z, 3)],
                      "min": [round(v, 3) for v in lo], "max": [round(v, 3) for v in hi]})
    bm.free()
    with open(os.path.join(OUT, "Teile.json"), "w") as f:
        json.dump(table, f, indent=1)

    pieces = {}
    for name in COLOURS:
        o = src.copy()
        o.data = src.data.copy()
        o.name = o.data.name = "GP_Frau_" + name
        bpy.context.scene.collection.objects.link(o)
        b = bmesh.new()
        b.from_mesh(o.data)
        b.verts.ensure_lookup_table()
        bmesh.ops.delete(b, geom=[v for v in b.verts if sort.get(v.index) != name], context="VERTS")
        b.to_mesh(o.data)
        b.free()
        o.color = COLOURS[name] + (1.0,)
        pieces[name] = o
        log("{}: {} islands, {} verts".format(name, sum(1 for t in table if t["teil"] == name), len(o.data.vertices)))
    bpy.data.objects.remove(src)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "Glutpanzer_Frau_Teile.blend"))

    # previews: coloured by piece, and the shell alone
    s = bpy.context.scene
    s.render.engine = "BLENDER_WORKBENCH"
    s.display.shading.light = "STUDIO"
    s.render.resolution_x, s.render.resolution_y = 900, 1200
    cd = bpy.data.cameras.new("C"); cd.type = "ORTHO"; cd.ortho_scale = 1.1
    cam = bpy.data.objects.new("C", cd); s.collection.objects.link(cam); s.camera = cam
    mid = Vector((0, 0, 0.5))
    views = (("vorne", Vector((0, -1, 0))), ("hinten", Vector((0, 1, 0))), ("seite", Vector((1, 0, 0))), ("schraeg_hinten", Vector((0.8, 1, 0.3))))
    for mode in ("farben", "textur"):
        s.display.shading.color_type = "OBJECT" if mode == "farben" else "TEXTURE"
        for name, d in views:
            cam.location = mid + d.normalized() * 10
            cam.rotation_euler = (mid - cam.location).to_track_quat("-Z", "Y").to_euler()
            s.render.filepath = os.path.join(OUT, "vorschau_{}_{}.png".format(mode, name))
            bpy.ops.render.render(write_still=True)
    s.display.shading.color_type = "TEXTURE"
    for name, o in pieces.items():
        for other in pieces.values():
            other.hide_render = other is not o
        for view, d in (("vorne", Vector((0, -1, 0))), ("hinten", Vector((0, 1, 0)))):
            cam.location = mid + d.normalized() * 10
            cam.rotation_euler = (mid - cam.location).to_track_quat("-Z", "Y").to_euler()
            s.render.filepath = os.path.join(OUT, "teil_{}_{}.png".format(name, view))
            bpy.ops.render.render(write_still=True)
    log("done")


main()
