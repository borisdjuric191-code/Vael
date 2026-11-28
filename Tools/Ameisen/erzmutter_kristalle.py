"""
Erzmutter (Boris 28.11.): die grossen und vielen Kernerz-Kristalle in Blender ergaenzen, damit man den Unterschied zu
den Erztoechtern sieht. Look-Bilder, noch kein Spiel-Export.

1. Fassungen ueber die Textur: jede Fassung ist ein dunkler Kreis in der Grundfarbe (Tripo hat sie so gemalt). Ecken,
   die dunkler sind als ihre Umgebung, werden zu Flecken zusammengefasst -> Mitte, Radius, Normale. Anders als bei
   der Arbeiterin auch an Beinen und Kopf (die Mutter ist ueberall bewachsen). Der tuerkise Kern zaehlt nicht.
2. Kristalle: echte Kernerz-Spitzen aus dem Spiel (Vael_Quellen/Kernerz/04_export/SM_Kernerz.fbx), zerschnitten in
   lange Einzelkristalle wie in ameise_kristalle.py (Modus spitze).
3. Groesse nach Lage: auf dem Ruecken des Hinterleibs riesige Saeulen (Grat), an den Flanken gross, an Beinen und Kopf
   klein und dicht. Jede Fassung bekommt eine Hauptsaeule und Nebenspitzen aus dem Rand. Alle neigen sich etwas nach
   hinten (wachsen mit der Koerperform), nicht wirr.

  blender -b -P Tools/Ameisen/erzmutter_kristalle.py -- <mutter.fbx> <ausgabe-praefix> [saat] [nur_flecken]
Ausgabe: <praefix>_0..3.png, <praefix>_flecken.json, <praefix>.blend
"""
import json
import math
import random
import sys
from collections import deque

import bmesh
import bpy
from mathutils import Matrix, Quaternion, Vector
from mathutils.kdtree import KDTree

args = sys.argv[sys.argv.index("--") + 1:]
MUTTER, OUT = args[0], args[1]
SAAT = int(args[2]) if len(args) > 2 else 7
NUR_FLECKEN = len(args) > 3 and args[3] == "nur_flecken"
QUELLEN = "C:/Projekte/Vael_Quellen"
ERZ = "Kernerz"
KERN = []        # Mitte des tuerkisen Kerns (liegt im Hinterleib)


def import_joined(path):
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    meshes = [o for o in bpy.context.scene.objects if o not in before and o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for o in list(bpy.context.scene.objects):
        if o not in before and o != obj and o.type != "MESH":
            bpy.data.objects.remove(o)
    return obj


def bounds(me):
    pts = [v.co for v in me.vertices]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


def mutter():
    obj = import_joined(MUTTER)
    lo, hi = bounds(obj.data)
    size = max(hi - lo)
    centre = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    obj.data.transform(Matrix.Scale(1.0 / size, 4) @ Matrix.Translation(-centre))
    obj.data.update()
    obj.name = "Erzmutter"
    return obj


def basecolor(obj):
    for slot in obj.material_slots:
        if slot.material and slot.material.use_nodes:
            for n in slot.material.node_tree.nodes:
                if n.type == "TEX_IMAGE" and n.image and "basecolor" in n.image.name.lower():
                    return n.image
    for img in bpy.data.images:
        if "basecolor" in img.name.lower():
            return img
    raise RuntimeError("keine Grundfarbe gefunden")


def flecken(obj):
    import numpy as np
    me = obj.data
    img = basecolor(obj)
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    px = px.reshape(h, w, 4)
    uv = me.uv_layers.active.data
    nv = len(me.vertices)
    rgb = np.zeros((nv, 3)); zahl = np.zeros(nv)
    for loop in me.loops:
        u, v = uv[loop.index].uv
        c = px[min(h - 1, max(0, int((v % 1.0) * h))), min(w - 1, max(0, int((u % 1.0) * w)))]
        rgb[loop.vertex_index] += c[:3]
        zahl[loop.vertex_index] += 1
    rgb /= np.maximum(zahl, 1)[:, None]
    lum = 0.3 * rgb[:, 0] + 0.59 * rgb[:, 1] + 0.11 * rgb[:, 2]
    tuerkis = (rgb[:, 2] > rgb[:, 0] + 0.12) & (rgb[:, 1] > rgb[:, 0] + 0.12)
    vs = me.vertices
    kern = [vs[i].co for i in range(nv) if tuerkis[i]]
    if kern:
        KERN[:] = [sum(kern, Vector()) / len(kern)]
    kd = KDTree(nv)
    for i, v in enumerate(vs):
        kd.insert(v.co, i)
    kd.balance()
    dunkel = np.zeros(nv, dtype=bool)
    for i, v in enumerate(vs):
        if tuerkis[i] or lum[i] < 0.06:
            continue
        hood = kd.find_range(v.co, 0.035)
        mittel = sum(lum[j] for _, j, _ in hood) / len(hood)
        dunkel[i] = lum[i] < mittel - 0.05
    nach = [[] for _ in vs]
    for e in me.edges:
        a, b = e.vertices
        nach[a].append(b); nach[b].append(a)
    gesehen = np.zeros(nv, dtype=bool)
    out = []
    for s0 in range(nv):
        if not dunkel[s0] or gesehen[s0]:
            continue
        q = deque([s0]); gesehen[s0] = True; teil = []
        while q:
            i = q.popleft(); teil.append(i)
            for j in nach[i]:
                if dunkel[j] and not gesehen[j]:
                    gesehen[j] = True; q.append(j)
        if len(teil) < 5:
            continue
        pts = [vs[i].co for i in teil]
        c = sum(pts, Vector()) / len(pts)
        r = math.sqrt(sum((p - c).length_squared for p in pts) / len(pts)) * 1.41
        # Fassung = rundlicher Fleck: grosse Ausdehnung im Verhaeltnis zum Radius heisst Spalt/Fuge, nicht Kreis
        ausd = max((p - c).length for p in pts)
        if not (0.005 <= r <= 0.045) or ausd > r * 2.6:
            continue
        n = Vector()
        for co, j, dd in kd.find_range(c, max(r * 1.6, 0.012)):
            n += vs[j].normal
        if n.length == 0:
            continue
        n.normalize()
        _, k, _ = kd.find(c)
        out.append({"p": list(vs[k].co), "n": list(n), "r": r})
    # Doppelte (ein Ring in zwei Flecken zerfallen) zusammenlegen
    out.sort(key=lambda m: -m["r"])
    rein = []
    for m in out:
        p = Vector(m["p"])
        if any((Vector(a["p"]) - p).length < 0.9 * max(a["r"], m["r"]) for a in rein):
            continue
        rein.append(m)
    print("FLECKEN", len(out), "FASSUNGEN", len(rein))
    return rein


def spitzen():
    """Lange Einzelkristalle aus dem Kernerz (siehe ameise_kristalle.py, spitzen_mit)."""
    obj = import_joined("{}/{}/04_export/SM_{}.fbx".format(QUELLEN, ERZ, ERZ))
    me = obj.data
    lo, hi = bounds(me)
    me.transform(Matrix.Scale(1.0 / max(hi - lo), 4))
    lo, hi = bounds(me)
    H = hi.z - lo.z
    for verh in (1.4, 0.8):
        teile = []
        for anteil in (0.75, 0.65, 0.55, 0.45, 0.35):
            z0 = lo.z + H * anteil
            kopie = me.copy()
            bm = bmesh.new()
            bm.from_mesh(kopie)
            bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(0, 0, z0),
                                   plane_no=(0, 0, 1), clear_inner=True)
            bm.verts.ensure_lookup_table()
            gesehen = set()
            for v0 in bm.verts:
                if v0.index in gesehen:
                    continue
                stapel, insel = [v0], []
                gesehen.add(v0.index)
                while stapel:
                    v = stapel.pop()
                    insel.append(v)
                    for e in v.link_edges:
                        w = e.other_vert(v)
                        if w.index not in gesehen:
                            gesehen.add(w.index)
                            stapel.append(w)
                zs = [v.co.z for v in insel]
                hoch = max(zs) - min(zs)
                if hoch < H * 0.12 or len(insel) < 60:
                    continue
                fuss_pts = [v.co for v in insel if v.co.z < z0 + hoch * 0.08]
                if len(fuss_pts) < 3:
                    continue
                fuss = sum(fuss_pts, Vector()) / len(fuss_pts)
                spitze = max(insel, key=lambda v: v.co.z).co.copy()
                achse = (spitze - fuss).normalized()
                ab = sorted((p - fuss - achse * (p - fuss).dot(achse)).length for p in fuss_pts)
                breite = 2 * ab[int(len(ab) * 0.8)]
                if breite <= 0 or (spitze - fuss).length / breite < verh:
                    continue
                teile.append(({v.index for v in insel}, fuss, achse, (spitze - fuss).length, breite, kopie))
            bm.to_mesh(kopie)
            bm.free()
            if len(teile) >= 4:
                break
        if teile:
            break
    if not teile:
        raise RuntimeError("keine Kernerz-Spitzen gefunden")
    vorlagen = []
    teile.sort(key=lambda t: -t[3])
    for k, (insel, fuss, achse, laenge, breite, kopie) in enumerate(teile[:6]):
        nm = kopie.copy()
        nm.name = "Spitze_{}_{}".format(ERZ, k)
        bm2 = bmesh.new()
        bm2.from_mesh(nm)
        bm2.verts.ensure_lookup_table()
        bmesh.ops.delete(bm2, geom=[v for v in bm2.verts if v.index not in insel], context="VERTS")
        bm2.to_mesh(nm)
        bm2.free()
        rot = achse.to_track_quat("Z", "Y").to_matrix().to_4x4().inverted()
        nm.transform(rot @ Matrix.Translation(-fuss))
        o = bpy.data.objects.new(nm.name, nm)
        bpy.context.scene.collection.objects.link(o)
        vorlagen.append((o, breite, laenge))
    bpy.data.objects.remove(obj)
    print("SPITZEN", len(vorlagen), [round(l / b, 2) for o, b, l in vorlagen])
    return vorlagen


def koerper_achse(obj):
    """Laengsachse (x oder y) und Richtung zum Hinterleib: dort liegt der tuerkise Kern."""
    me = obj.data
    lo, hi = bounds(me)
    ax = 0 if (hi.x - lo.x) >= (hi.y - lo.y) else 1
    mitte = (lo[ax] + hi[ax]) / 2
    hinten = Vector((0, 0, 0))
    hinten[ax] = 1.0 if KERN and KERN[0][ax] > mitte else -1.0
    print("ACHSE", "xy"[ax], "HINTEN", hinten[ax], "KERN", KERN[0] if KERN else None)
    return ax, hinten, lo, hi


def besetzen(obj, fass, vorlagen):
    rnd = random.Random(SAAT)
    ax, hinten, lo, hi = koerper_achse(obj)
    laenge = hi[ax] - lo[ax]
    zmax = hi.z
    gesetzt = 0
    for m in fass:
        p, n, r = Vector(m["p"]), Vector(m["n"]).normalized(), m["r"]
        # Lage: 0 vorne (Kopf) .. 1 hinten (Hinterleibsende)
        t = ((p[ax] - lo[ax]) / laenge) if hinten[ax] > 0 else ((hi[ax] - p[ax]) / laenge)
        hoch = p.z / zmax
        oben = max(0.0, n.z)
        hinterleib = max(0.0, min(1.0, (t - 0.42) / 0.2))
        # Grat: oben auf dem Hinterleib die groessten Saeulen
        grat = hinterleib * oben ** 1.5 * max(0.0, (hoch - 0.35) / 0.65)
        bein = hoch < 0.32 and hinterleib < 0.5
        kopf = t < 0.22 and not bein
        klein = bein or kopf
        if klein and rnd.random() < 0.4:
            continue                       # Beine und Kopf: dicht, aber nicht jede Fassung
        if KERN and (p - KERN[0]).length < 0.1:
            continue                       # das Kernfenster bleibt frei (Schwachstelle muss sichtbar sein)
        brust = (not klein) and t < 0.42
        flanke = 0.45 + 0.55 * max(0.0, min(1.0, (hoch - 0.2) / 0.6))   # unten an den Flanken kuerzer
        # Wachstum mit dem Koerper: deutlich nach hinten geneigt (wie Schuppen), am Grat steiler nach oben
        rich = (n * (1.0 + grat) + hinten * (0.55 - 0.15 * grat) + Vector((0, 0, 0.3 * grat))).normalized()
        o, b, l = rnd.choice(vorlagen)
        dick = 2 * r * (0.8 + 0.7 * grat) * rnd.uniform(0.92, 1.08)
        s_xy = dick / b
        ziel = r * (2.6 + 10.0 * grat + 1.5 * hinterleib) * (0.6 if klein else (1.3 if brust else flanke)) \
            * rnd.uniform(0.9, 1.15)
        s_z = s_xy * min(3.2, max(1.0, ziel / (l * s_xy)))
        stellen = [(p - n * r * 0.15, rich, s_xy, s_z)]
        t1 = n.orthogonal().normalized()
        t2 = n.cross(t1)
        start = rnd.uniform(0, 2 * math.pi)
        # Nebenspitzen nur auf dem Rumpf; sie folgen der Hauptsaeule eng (Buendel statt Stern)
        neben = 0 if klein else (2 if grat > 0.3 or brust else 1)
        for i in range(neben):
            a = start + 2 * math.pi * i / max(1, neben) + rnd.uniform(-0.4, 0.4)
            e = t1 * math.cos(a) + t2 * math.sin(a)
            nr = (rich + e * math.tan(math.radians(rnd.uniform(10, 18)))).normalized()
            f = rnd.uniform(0.45, 0.62)
            stellen.append((p + e * r * 0.7, nr, s_xy * f, s_z * f * rnd.uniform(0.85, 1.1)))
        for ort, rr, sx, sz in stellen:
            k = o.copy()
            bpy.context.scene.collection.objects.link(k)
            dreh = rr.to_track_quat("Z", "Y") @ Quaternion((0, 0, 1), rnd.uniform(0, 2 * math.pi))
            neig = Quaternion(rr.orthogonal().normalized(), math.radians(rnd.uniform(-6, 6)))
            k.matrix_world = Matrix.Translation(ort) @ (neig @ dreh).to_matrix().to_4x4() @ Matrix.Diagonal((sx, sx, sz, 1))
            gesetzt += 1
    for o, b, l in vorlagen:
        bpy.data.objects.remove(o)
    print("KRISTALLE", gesetzt, "in", len(fass), "Fassungen")


def markieren(fass):
    mat = bpy.data.materials.new("Marke")
    mat.use_nodes = True
    mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (1, 0, 0, 1)
    for m in fass:
        bpy.ops.mesh.primitive_uv_sphere_add(radius=m["r"] * 0.7, location=m["p"], segments=8, ring_count=6)
        bpy.context.object.data.materials.append(mat)


def bilder():
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_EEVEE"
    scn.render.resolution_x, scn.render.resolution_y = 1400, 1000
    world = bpy.data.worlds.new("W")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.32, 0.32, 0.35, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.8
    scn.world = world
    bpy.ops.object.light_add(type="SUN", rotation=(math.radians(50), 0, math.radians(30)))
    bpy.context.object.data.energy = 3
    bpy.ops.object.light_add(type="SUN", rotation=(math.radians(60), 0, math.radians(210)))
    bpy.context.object.data.energy = 1
    cam = bpy.data.cameras.new("C")
    co = bpy.data.objects.new("C", cam)
    scn.collection.objects.link(co)
    scn.camera = co
    cam.lens = 50
    for k, (az, el) in enumerate(((35, 25), (215, 25), (125, 20), (300, 55))):
        d = 2.7
        a, e = math.radians(az), math.radians(el)
        co.location = Vector((d * math.cos(a) * math.cos(e), d * math.sin(a) * math.cos(e), 0.3 + d * math.sin(e)))
        look = Vector((0, 0, 0.33)) - co.location
        co.rotation_euler = look.to_track_quat("-Z", "Y").to_euler()
        scn.render.filepath = "{}_{}.png".format(OUT, k)
        bpy.ops.render.render(write_still=True)


bpy.ops.wm.read_factory_settings(use_empty=True)
koerper = mutter()
fass = flecken(koerper)
with open(OUT + "_flecken.json", "w", encoding="utf-8") as f:
    json.dump({"mulden": fass}, f, indent=1)
if NUR_FLECKEN:
    markieren(fass)
else:
    besetzen(koerper, fass, spitzen())
bilder()
bpy.ops.wm.save_as_mainfile(filepath=OUT + ".blend")
