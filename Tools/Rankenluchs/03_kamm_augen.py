"""Step 3 + 4: leaf crest and glowing eyes.

Kamm: one broad lanceolate leaf modelled from scratch (18 cm long, 5 cm wide, slightly cupped, folded along the mid
rib; UV V runs from the base 0 to the tip 1). Ten copies stand along the back from the nape to the hip, leaning back
and up, the biggest in the middle. Each hangs rigidly (100 %) on its own bone kamm_01..10, which hangs on the nearest
spine bone. Material slot "Kamm".

Augen: the eyes are found on the model's own texture (the amber iris), and a small flat glow disc is laid exactly on
each, on the head bone. Material slot "Augen".

Everything is joined into the lynx mesh (one skeletal mesh for Unreal). Saves arbeit/03_kamm_augen.blend and renders
vorschau/03_* (side, game camera, the head close up for checking the eyes).

    blender -b --factory-startup -P skripte/03_kamm_augen.py
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import vorschau  # noqa: E402

ORDNER = vorschau.ORDNER
BLATT_LAENGE = 0.18
BLATT_BREITE = 0.05
KAMM = 10
KAMM_VON_X = 0.17   # nape, just behind the leaf collar
KAMM_BIS_X = -0.40  # hip


def blatt_mesh(name):
    """Lanceolate leaf: widest at about a third, pointed tip, the halves folded down from the mid rib, the blade bowed back"""
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    laengs, quer = 14, 5
    reihen = []
    for i in range(laengs + 1):
        t = i / laengs
        breite = BLATT_BREITE * 0.5 * math.sin(math.pi * min(1.0, t ** 0.72)) * (1.0 if t < 0.98 else 0.0)
        breite = max(breite, 0.0015 if i < laengs else 0.0)
        reihe = []
        for j in range(quer):
            s = (j / (quer - 1)) * 2 - 1  # -1 .. 1 across
            x = s * breite
            # Fold along the rib and a slight cup; the blade bows backward toward the tip
            y = -abs(s) * breite * 0.35 + (s * s) * breite * 0.12
            z = t * BLATT_LAENGE
            biegung = (t ** 2) * 0.035
            reihe.append((bm.verts.new((x, y - biegung, z)), (s * 0.5 + 0.5, t)))
        reihen.append(reihe)
    for i in range(laengs):
        for j in range(quer - 1):
            ecken = [reihen[i][j], reihen[i][j + 1], reihen[i + 1][j + 1], reihen[i + 1][j]]
            face = bm.faces.new([e[0] for e in ecken])
            for loop, e in zip(face.loops, ecken):
                loop[uv].uv = e[1]
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    return me


def ruecken_hoehe(P, x):
    s = [p for p in P if abs(p.x - x) < 0.015 and abs(p.y) < 0.06]
    return max(p.z for p in s) if s else 0.68


def material_platz(obj, name):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    obj.data.materials.append(mat)
    return len(obj.data.materials) - 1


def wirbel_fuer(x):
    if x > 0.1:
        return "hals"
    if x > 0.03:
        return "wirbel_03"
    if x > -0.14:
        return "wirbel_02"
    if x > -0.3:
        return "wirbel_01"
    return "becken"


def kamm_bauen(luchs, arm):
    P = [luchs.matrix_world @ vx.co for vx in luchs.data.vertices]
    teile = []
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    plan = []
    for i in range(KAMM):
        t = i / (KAMM - 1)
        x = KAMM_VON_X + (KAMM_BIS_X - KAMM_VON_X) * t
        groesse = 0.6 + 0.4 * math.sin(math.pi * t)
        basis = Vector((x, 0.0, ruecken_hoehe(P, x) - 0.012))
        # Leaning back and up, a little more at the ends
        neigung = math.radians(38 + 10 * abs(t - 0.5))
        achse = Vector((-math.sin(neigung), 0.0, math.cos(neigung)))
        name = "kamm_%02d" % (i + 1)
        b = arm.data.edit_bones.new(name)
        b.head = basis
        b.tail = basis + achse * BLATT_LAENGE * groesse * 0.6
        b.roll = 0.0
        b.parent = arm.data.edit_bones[wirbel_fuer(x)]
        plan.append((name, basis, achse, groesse))
    bpy.ops.object.mode_set(mode="OBJECT")

    for name, basis, achse, groesse in plan:
        me = blatt_mesh("Blatt_" + name)
        blatt = bpy.data.objects.new("Blatt_" + name, me)
        bpy.context.scene.collection.objects.link(blatt)
        # Leaf Z (base to tip) along the bone, its width along the spine, its flat side facing left/right
        y_achse = Vector((0.0, 1.0, 0.0))
        x_achse = y_achse.cross(achse).normalized()
        drehung = Matrix((x_achse, y_achse, achse)).transposed().to_4x4()
        blatt.matrix_world = Matrix.Translation(basis) @ drehung @ Matrix.Scale(groesse, 4)
        vg = blatt.vertex_groups.new(name=name)
        vg.add(list(range(len(me.vertices))), 1.0, "REPLACE")
        teile.append(blatt)
    return teile


def augen_finden(luchs):
    """The amber iris on the model's own texture: every head triangle is sampled at several points, the amber samples
    grouped left/right give the eye centre, size and outward normal"""
    mat = luchs.data.materials[0]
    bild = next(n.image for n in mat.node_tree.nodes if n.type == "TEX_IMAGE" and "basecolor" in n.image.name.lower())
    w, h = bild.size
    pix = bild.pixels[:]
    me = luchs.data
    uv = me.uv_layers.active.data
    nase_x = max(vx.co.x for vx in me.vertices)
    proben = {"l": [], "r": []}
    gewichte = [(a / 4, b / 4, 1 - a / 4 - b / 4) for a in range(5) for b in range(5 - a)]
    me.calc_loop_triangles()
    for tri in me.loop_triangles:
        cos = [me.vertices[i].co for i in tri.vertices]
        if min(c.x for c in cos) < nase_x - 0.16 or not (0.66 < cos[0].z < 0.86):
            continue
        uvs = [uv[li].uv for li in tri.loops]
        for a, b, c in gewichte:
            u = uvs[0].x * a + uvs[1].x * b + uvs[2].x * c
            v_ = uvs[0].y * a + uvs[1].y * b + uvs[2].y * c
            k = (min(h - 1, max(0, int(v_ * h))) * w + min(w - 1, max(0, int(u * w)))) * 4
            r, g, bl = pix[k], pix[k + 1], pix[k + 2]
            # Amber: strong red, middle green, little blue, clearly saturated
            if r > 0.5 and 0.2 < g < r * 0.8 and bl < 0.22 and r - bl > 0.4:
                punkt = cos[0] * a + cos[1] * b + cos[2] * c
                proben["l" if punkt.y > -0.08 else "r"].append((punkt, tri.normal.copy()))
    augen = {}
    for seite, liste in proben.items():
        if len(liste) < 10:
            continue
        pts = [p for p, _ in liste]
        m = sum(pts, Vector()) / len(pts)
        for _ in range(3):  # keep the main blob
            pts_n = [p for p in pts if (p - m).length < 0.025]
            if pts_n:
                pts = pts_n
                m = sum(pts, Vector()) / len(pts)
        n = sum((nn for p, nn in liste if (p - m).length < 0.025), Vector()).normalized()
        r = sorted((p - m).length for p in pts)[int(len(pts) * 0.7)]
        augen[seite] = (m, n, max(0.006, min(0.014, r)), len(pts))
    return augen


def augen_bauen(luchs, augen):
    teile = []
    for seite, (mitte_, normale, radius, anzahl) in augen.items():
        print("[Rankenluchs] Auge %s: Mitte %s, Radius %.1f mm, %d Iris-Vertices" % (seite, tuple(round(c, 3) for c in mitte_), radius * 1000, anzahl))
        bpy.ops.mesh.primitive_circle_add(vertices=16, radius=radius, fill_type="TRIFAN", location=(0, 0, 0))
        scheibe = bpy.context.active_object
        scheibe.name = "Auge_" + seite
        scheibe.matrix_world = Matrix.Translation(mitte_ + normale * 0.0025) @ normale.to_track_quat("Z", "Y").to_matrix().to_4x4()
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        scheibe.vertex_groups.new(name="kopf").add(list(range(len(scheibe.data.vertices))), 1.0, "REPLACE")
        teile.append(scheibe)
    return teile


def zusammenfuegen(luchs, teile, slot_name):
    slot = material_platz(luchs, slot_name)
    for t in teile:
        t.data.materials.clear()
        t.data.materials.append(luchs.data.materials[slot])
        for poly in t.data.polygons:
            poly.material_index = 0
    for o in bpy.context.scene.objects:
        o.select_set(o in teile or o == luchs)
    bpy.context.view_layer.objects.active = luchs
    bpy.ops.object.join()


def main():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(ORDNER, "arbeit", "02_skelett.blend"))
    luchs = bpy.data.objects["Rankenluchs"]
    arm = bpy.data.objects["Rankenluchs_Skelett"]

    augen = augen_finden(luchs)
    zusammenfuegen(luchs, kamm_bauen(luchs, arm), "Kamm")
    if augen:
        zusammenfuegen(luchs, augen_bauen(luchs, augen), "Augen")
    else:
        print("[Rankenluchs] Augen nicht gefunden")
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ORDNER, "arbeit", "03_kamm_augen.blend"))

    # Kamm green and eyes bright in the previews (Unreal gets the real materials)
    for name, farbe in (("Kamm", (0.15, 0.55, 0.12, 1.0)), ("Augen", (1.0, 0.6, 0.1, 1.0))):
        mat = bpy.data.materials.get(name)
        if mat:
            mat.diffuse_color = farbe
    vorschau.standard_ansichten("03", mitte=(-0.15, 0.0, 0.5), groesse=1.7)
    kopf = Vector((0.45, -0.05, 0.76))
    vorschau.rendern("03_augen_nah", (1.0, -0.35, 0.15), kopf, 0.32)
    vorschau.rendern("03_augen_nah_schraeg", (0.6, -0.8, 0.5), kopf, 0.32)
    print("[Rankenluchs] Knochen: %d, Dreiecke: %d" % (len(arm.data.bones), sum(len(p.vertices) - 2 for p in luchs.data.polygons)))


main()
