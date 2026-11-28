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

4. (Boris 28.11.) formangepasst wie bei den Toechtern: Kristalle biegen sich zur Oberflaeche und fliessen nach hinten
   oben zum Grat; Augen leben (milchig, atmend, wandernder Kern); das Herz pocht und pumpt eine Lichtwelle in die
   Kristalle.

  blender -b -P Tools/Ameisen/erzmutter_kristalle.py -- <mutter.fbx> <ausgabe-praefix> [saat] [nur_flecken|<filmbilder>]
Ausgabe: <praefix>_0..3.png, _kopf_0..1.png, _film_###.png, _augenfilm_###.png, _flecken.json, .blend
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
FILM = int(args[3]) if len(args) > 3 and args[3].isdigit() else 0   # Bilder fuer den Herzschlag-Film (24 je Sekunde)
QUELLEN = "C:/Projekte/Vael_Quellen"
ERZ = "Kernerz"
KERN = []        # Mitte des tuerkisen Kerns (liegt im Hinterleib)
KERN_FENSTER = [] # Mitte der tuerkisen Flaeche je Koerperseite (dort scheint das Herz durch)
AUGEN_ORTE = []  # Augenmitten (dort keine Kristalle)
FARBE = {}       # Ecken-Farben der Grundfarbe (rgb, lum) fuer Augen-Suche


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
        for seite in (-1, 1):
            k2 = [c for c in kern if c.y * seite > 0]
            if k2:
                KERN_FENSTER.append(sum(k2, Vector()) / len(k2))
    FARBE["rgb"], FARBE["lum"] = rgb, lum
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


def biegen(vorlagen):
    """Gebogene Kopien jeder Spitze (wie kristalle_biegen.py): Bogen zur Seite -X, damit sich die Spitze an den Koerper
    legt. Je Vorlage Stufen 0/12/25/40/55 Grad."""
    out = []
    for o, b, l in vorlagen:
        stufen = {}
        for grad in BIEGE_STUFEN:
            if grad == 0:
                stufen[0] = o
                continue
            me = o.data.copy()
            R = l / math.radians(grad)
            for v in me.vertices:
                x, y, z = v.co
                a = max(0.0, z) / R
                rr = R + x
                v.co = (-R + rr * math.cos(a), y, rr * math.sin(a))
            k = bpy.data.objects.new("{}_B{}".format(o.name, grad), me)
            bpy.context.scene.collection.objects.link(k)
            stufen[grad] = k
        out.append((stufen, b, l))
    return out


BIEGE_STUFEN = (0, 12, 25, 40, 55)


def besetzen(obj, fass, vorlagen):
    """Formangepasst wie bei den Toechtern: jeder Kristall waechst mit dem Koerper nach hinten oben (Schuppenfluss zum
    Grat), biegt sich zur Oberflaeche hin und legt sich an. Grat: steile, fast gerade Saeulen. Flanken: maessig gebogen.
    Beine: klein, laufen am Bein entlang nach unten. Kopf: klein, nach hinten angelegt (Krone). Unterseite frei."""
    rnd = random.Random(SAAT)
    ax, hinten, lo, hi = koerper_achse(obj)
    laenge = hi[ax] - lo[ax]
    zmax = hi.z
    gebogen = biegen(vorlagen)
    oben_v = Vector((0, 0, 1))
    gesetzt = 0
    for m in fass:
        p, n, r = Vector(m["p"]), Vector(m["n"]).normalized(), m["r"]
        t = ((p[ax] - lo[ax]) / laenge) if hinten[ax] > 0 else ((hi[ax] - p[ax]) / laenge)
        hoch = p.z / zmax
        oben = max(0.0, n.z)
        hinterleib = max(0.0, min(1.0, (t - 0.42) / 0.2))
        grat = hinterleib * oben ** 1.5 * max(0.0, (hoch - 0.35) / 0.65)
        bein = hoch < 0.32 and hinterleib < 0.5
        kopf = t < 0.22 and not bein
        klein = bein or kopf
        if n.z < -0.55 and not bein:
            continue                       # Bauchseite bleibt frei (liegt am Boden, sieht man nicht)
        if klein and rnd.random() < (0.55 if bein else 0.35):
            continue
        if KERN and (p - KERN[0]).length < 0.1:
            continue                       # Kernfenster frei
        if t < 0.1 or any((p - a).length < 0.05 for a in AUGEN_ORTE):
            continue                       # Kiefer und Augen frei
        brust = (not klein) and t < 0.42
        flanke = 0.45 + 0.55 * max(0.0, min(1.0, (hoch - 0.2) / 0.6))
        if bein:
            # am Bein entlang nach unten, leicht nach aussen
            rich = (n * 0.55 + Vector((0, 0, -1.0)) + hinten * 0.2).normalized()
            grad_ziel = 40
        elif kopf:
            rich = (n * 0.6 + hinten * 0.9 + oben_v * 0.35).normalized()
            grad_ziel = 40
        else:
            # Schuppenfluss: nach hinten und zum Grat hinauf; am Grat steil
            steil = min(1.0, grat * 1.4)
            rich = (n * (0.6 + 1.4 * steil) + hinten * (0.95 - 0.65 * steil) + oben_v * (0.35 + 0.1 * steil)).normalized()
            grad_ziel = 8 + 40 * (1.0 - steil) * (1.0 if not brust else 0.75)
        grad_ziel *= rnd.uniform(0.8, 1.2)
        stufe = min(BIEGE_STUFEN, key=lambda g: abs(g - grad_ziel))
        stufen, b, l = rnd.choice(gebogen)
        o = stufen[stufe]
        dick = 2 * r * (0.62 + 0.88 * grat) * rnd.uniform(0.92, 1.08)   # schlanker an den Flanken
        s_xy = dick / b
        ziel = r * (2.6 + 10.0 * grat + 1.5 * hinterleib) * (0.55 if klein else (1.3 if brust else flanke))             * rnd.uniform(0.9, 1.15)
        s_z = s_xy * min(3.2, max(1.0, ziel / (l * s_xy)))
        stellen = [(p - n * r * 0.15, rich, s_xy, s_z, o)]
        t1 = n.orthogonal().normalized()
        t2 = n.cross(t1)
        start = rnd.uniform(0, 2 * math.pi)
        neben = 0 if klein else (2 if grat > 0.3 or brust else 1)
        for i in range(neben):
            a = start + 2 * math.pi * i / max(1, neben) + rnd.uniform(-0.4, 0.4)
            e = t1 * math.cos(a) + t2 * math.sin(a)
            nr = (rich + e * math.tan(math.radians(rnd.uniform(8, 14)))).normalized()
            f = rnd.uniform(0.45, 0.62)
            st2 = min(BIEGE_STUFEN, key=lambda g: abs(g - min(55, grad_ziel * 1.25)))
            stellen.append((p + e * r * 0.7, nr, s_xy * f, s_z * f * rnd.uniform(0.85, 1.1), stufen[st2]))
        for ort, rr, sx, sz, vorlage in stellen:
            k = vorlage.copy()
            bpy.context.scene.collection.objects.link(k)
            # +X zeigt von der Oberflaeche weg -> der Bogen zu -X legt die Spitze an den Koerper
            x = n - rr * n.dot(rr)
            x = x.normalized() if x.length > 1e-4 else rr.orthogonal().normalized()
            dreh = Quaternion(rr, math.radians(rnd.uniform(-12, 12)))
            x = dreh @ x
            y = rr.cross(x)
            rot = Matrix((x, y, rr)).transposed().to_4x4()
            k.matrix_world = Matrix.Translation(ort) @ rot @ Matrix.Diagonal((sx, sx, sz, 1))
            k["kern_abstand"] = (ort - KERN[0]).length if KERN else 0.0
            gesetzt += 1
    for stufen, b, l in gebogen:
        for o in stufen.values():
            bpy.data.objects.remove(o)
    print("KRISTALLE", gesetzt, "in", len(fass), "Fassungen")


def markieren(fass):
    mat = bpy.data.materials.new("Marke")
    mat.use_nodes = True
    mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (1, 0, 0, 1)
    for m in fass:
        bpy.ops.mesh.primitive_uv_sphere_add(radius=m["r"] * 0.7, location=m["p"], segments=8, ring_count=6)
        bpy.context.object.data.materials.append(mat)


def augen(obj):
    """Augen: graue Mulden vorne am Kopf (4 je Seite). Lebendig wie bei den Toechtern: milchiger Augapfel, der Licht
    atmet, ein heller Kern, der umherwandert. Je Auge eigenes Material, damit der Kern pro Bild verschoben werden kann."""
    import numpy as np
    ax, hinten, lo, hi = koerper_achse(obj)
    me = obj.data
    rgb, lum = FARBE["rgb"], FARBE["lum"]
    sat = rgb.max(1) - rgb.min(1)
    laenge = hi[ax] - lo[ax]
    kand = []
    for i, v in enumerate(me.vertices):
        t = ((v.co[ax] - lo[ax]) / laenge) if hinten[ax] > 0 else ((hi[ax] - v.co[ax]) / laenge)
        if t < 0.2 and sat[i] < 0.08 and lum[i] > 0.3 and v.co.z > 0.3:
            kand.append(i)
    gruppen = []
    for i in kand:
        c = me.vertices[i].co
        for g in gruppen:
            if (g["c"] - c).length < 0.02:
                g["i"].append(i)
                g["c"] = sum((me.vertices[j].co for j in g["i"]), Vector()) / len(g["i"])
                break
        else:
            gruppen.append({"c": c.copy(), "i": [i]})
    out = []
    for g in gruppen:
        if len(g["i"]) < 2:
            continue
        pts = [me.vertices[j].co for j in g["i"]]
        r = max(0.006, min(0.016, math.sqrt(sum((q - g["c"]).length_squared for q in pts) / len(pts)) * 1.7))
        n = sum((me.vertices[j].normal for j in g["i"]), Vector()).normalized()
        out.append((g["c"], n, r))
    AUGEN_ORTE[:] = [c for c, n, r in out]
    print("AUGEN", len(out), [(tuple(round(x, 3) for x in c), round(r, 4)) for c, n, r in out])
    rnd = random.Random(SAAT + 5)
    objs = []
    for k, (c, n, r) in enumerate(out):
        bpy.ops.mesh.primitive_uv_sphere_add(radius=1.0, segments=24, ring_count=16)
        a = bpy.context.object
        a.name = "Auge_{}".format(k)
        bpy.ops.object.shade_smooth()
        rot = n.to_track_quat("Z", "Y").to_matrix().to_4x4()
        a.matrix_world = Matrix.Translation(c - n * r * 0.3) @ rot @ Matrix.Diagonal((1.15 * r, 0.95 * r, 0.8 * r, 1))
        mat = bpy.data.materials.new("Auge_{}".format(k))
        mat.use_nodes = True
        nt = mat.node_tree
        bsdf = nt.nodes["Principled BSDF"]
        milch = (0.30, 0.50, 0.55, 1)
        bsdf.inputs["Base Color"].default_value = milch
        bsdf.inputs["Roughness"].default_value = 0.06
        bsdf.inputs["Coat Weight"].default_value = 1.0
        bsdf.inputs["Coat Roughness"].default_value = 0.02
        bsdf.inputs["Emission Color"].default_value = (0.45, 0.9, 1.0, 1)
        tc = nt.nodes.new("ShaderNodeTexCoord")
        spot = nt.nodes.new("ShaderNodeCombineXYZ"); spot.name = "Kernlage"
        dist = nt.nodes.new("ShaderNodeVectorMath"); dist.operation = "DISTANCE"
        nt.links.new(tc.outputs["Object"], dist.inputs[0]); nt.links.new(spot.outputs[0], dist.inputs[1])
        ring = nt.nodes.new("ShaderNodeMapRange")
        ring.inputs["From Min"].default_value = 0.1; ring.inputs["From Max"].default_value = 0.55
        ring.inputs["To Min"].default_value = 6.0; ring.inputs["To Max"].default_value = 0.0
        nt.links.new(dist.outputs["Value"], ring.inputs["Value"])
        atem = nt.nodes.new("ShaderNodeValue"); atem.name = "Atem"; atem.outputs[0].default_value = 1.0
        summe = nt.nodes.new("ShaderNodeMath"); summe.operation = "MULTIPLY_ADD"
        nt.links.new(ring.outputs["Result"], summe.inputs[0]); nt.links.new(atem.outputs[0], summe.inputs[1])
        grund = nt.nodes.new("ShaderNodeMath"); grund.operation = "MULTIPLY"; grund.inputs[1].default_value = 0.25
        nt.links.new(atem.outputs[0], grund.inputs[0]); nt.links.new(grund.outputs[0], summe.inputs[2])
        nt.links.new(summe.outputs[0], bsdf.inputs["Emission Strength"])
        a.data.materials.append(mat)
        a["phase"] = rnd.uniform(0, 2 * math.pi)
        a["blick"] = rnd.uniform(0, 2 * math.pi)
        objs.append(a)
    return objs


def herzschlag(nt, T, d):
    """Knoten: Herzschlag (lub-dub) als Welle, die vom Kern ueber den Koerper und in die Kristalle laeuft.
    T: Zeit-Socket, d: Abstand-Socket. Gibt einen Socket 0..~1.6 zurueck."""
    def m(op, a, b=None, c=None):
        n = nt.nodes.new("ShaderNodeMath"); n.operation = op
        for k, v in enumerate((a, b, c)):
            if v is None:
                continue
            if isinstance(v, (int, float)):
                n.inputs[k].default_value = v
            else:
                nt.links.new(v, n.inputs[k])
        return n.outputs[0]
    phase = m("FRACT", m("DIVIDE", m("SUBTRACT", T, m("DIVIDE", d, HERZ_TEMPO)), HERZ_TAKT))
    def stoss(mitte, hoehe):
        x = m("MAXIMUM", m("SUBTRACT", 1.0, m("DIVIDE", m("ABSOLUTE", m("SUBTRACT", phase, mitte)), 0.08)), 0.0)
        return m("MULTIPLY", m("MULTIPLY", x, x), hoehe)
    welle = m("ADD", stoss(0.08, 1.0), stoss(0.30, 0.6))
    abfall = m("DIVIDE", 1.0, m("MULTIPLY_ADD", d, 2.5, 1.0))
    return m("MULTIPLY", welle, abfall)


HERZ_TAKT = 1.1      # Sekunden pro Schlag (langsam, schwer)
HERZ_TEMPO = 0.9     # Laufgeschwindigkeit der Lichtwelle (Koerperlaengen pro Sekunde)
ZEIT = []            # Zeit-Wert-Knoten aller Materialien


def herz_materialien(koerper, kristalle):
    """Herz pocht und pumpt Licht: der tuerkise Kern leuchtet im Takt auf, die Welle laeuft durch den Koerper in die
    Kristalle (vom Fuss zur Spitze, nahe Kristalle zuerst)."""
    def zeit_und_abstand(nt):
        T = nt.nodes.new("ShaderNodeValue"); T.name = "Zeit"; ZEIT.append(T)
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        k = nt.nodes.new("ShaderNodeCombineXYZ")
        for i in range(3):
            k.inputs[i].default_value = KERN[0][i]
        d = nt.nodes.new("ShaderNodeVectorMath"); d.operation = "DISTANCE"
        nt.links.new(geo.outputs["Position"], d.inputs[0]); nt.links.new(k.outputs[0], d.inputs[1])
        return T.outputs[0], d.outputs["Value"]
    # Koerper: Kernflaeche (tuerkis in der Grundfarbe) leuchtet, und ein schwacher Schimmer laeuft durch den Panzer
    for slot in koerper.material_slots:
        mat = slot.material
        nt = mat.node_tree
        bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
        tex = next(n for n in nt.nodes if n.type == "TEX_IMAGE" and n.image and "basecolor" in n.image.name.lower())
        T, d = zeit_und_abstand(nt)
        puls = herzschlag(nt, T, d)
        sep = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(tex.outputs["Color"], sep.inputs[0])
        mask = nt.nodes.new("ShaderNodeMath"); mask.operation = "SUBTRACT"
        nt.links.new(sep.outputs[2], mask.inputs[0]); nt.links.new(sep.outputs[0], mask.inputs[1])
        mk = nt.nodes.new("ShaderNodeMapRange")
        mk.inputs["From Min"].default_value = 0.08; mk.inputs["From Max"].default_value = 0.3
        nt.links.new(mask.outputs[0], mk.inputs["Value"])
        # Staerke: Kern 1 + 9*Puls, Panzer 0.35*Puls
        st = nt.nodes.new("ShaderNodeMath"); st.operation = "MULTIPLY_ADD"
        nt.links.new(puls, st.inputs[0]); st.inputs[1].default_value = 5.0; st.inputs[2].default_value = 0.6
        kern_st = nt.nodes.new("ShaderNodeMath"); kern_st.operation = "MULTIPLY"
        nt.links.new(st.outputs[0], kern_st.inputs[0]); nt.links.new(mk.outputs["Result"], kern_st.inputs[1])
        hauch = nt.nodes.new("ShaderNodeMath"); hauch.operation = "MULTIPLY_ADD"
        nt.links.new(puls, hauch.inputs[0]); hauch.inputs[1].default_value = 0.08; nt.links.new(kern_st.outputs[0], hauch.inputs[2])
        farbe = nt.nodes.new("ShaderNodeMix"); farbe.data_type = "RGBA"
        nt.links.new(mk.outputs["Result"], farbe.inputs["Factor"])
        farbe.inputs[6].default_value = (0.25, 0.85, 0.8, 1)
        nt.links.new(tex.outputs["Color"], farbe.inputs[7])
        nt.links.new(farbe.outputs[2], bsdf.inputs["Emission Color"])
        nt.links.new(hauch.outputs[0], bsdf.inputs["Emission Strength"])
    # Kristalle: eigenes Leuchten im Takt, Welle von innen nach aussen
    gesehen = set()
    for o in kristalle:
        for slot in o.material_slots:
            mat = slot.material
            if mat is None or mat.name in gesehen:
                continue
            gesehen.add(mat.name)
            nt = mat.node_tree
            bsdf = next((n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"), None)
            if bsdf is None:
                continue
            T, d = zeit_und_abstand(nt)
            puls = herzschlag(nt, T, d)
            st = nt.nodes.new("ShaderNodeMath"); st.operation = "MULTIPLY_ADD"
            nt.links.new(puls, st.inputs[0]); st.inputs[1].default_value = 2.2; st.inputs[2].default_value = 0.03
            bsdf.inputs["Emission Color"].default_value = (0.2, 0.95, 0.85, 1)
            nt.links.new(st.outputs[0], bsdf.inputs["Emission Strength"])
    print("HERZ", len(ZEIT), "Materialien")


def herz_lichter():
    lichter = []
    for c in KERN_FENSTER:
        aussen = (c - KERN[0]); aussen.z = 0
        aussen = aussen.normalized() if aussen.length > 1e-4 else Vector((0, 1, 0))
        ld = bpy.data.lights.new("Herz", "POINT")
        ld.color = (0.3, 1.0, 0.9)
        ld.shadow_soft_size = 0.05
        lo = bpy.data.objects.new("Herz", ld)
        bpy.context.scene.collection.objects.link(lo)
        lo.location = c + aussen * 0.05
        ld.use_custom_distance = True
        ld.cutoff_distance = 0.22   # nur das Kernfenster und die Kristalle daneben, nicht die Brust
        lichter.append(lo)
    return lichter


def puls_wert(T, d=0.0):
    ph = ((T - d / HERZ_TEMPO) / HERZ_TAKT) % 1.0
    st = lambda mt, h: h * max(0.0, 1 - abs(ph - mt) / 0.08) ** 2
    return (st(0.08, 1.0) + st(0.30, 0.6)) / (1 + 2.5 * d)


def zeit_setzen(T, lichter, augen_objs):
    for v in ZEIT:
        v.outputs[0].default_value = T
    for lo in lichter:
        lo.data.energy = 1.0 + 9 * puls_wert(T)
    for a in augen_objs:
        nt = a.data.materials[0].node_tree
        # Atmen: langsam, leicht im Herztakt mit; Kern wandert umher
        atem = 0.9 + 0.25 * math.sin(T * 1.7 + a["phase"]) + 0.4 * puls_wert(T, 0.45)
        nt.nodes["Atem"].outputs[0].default_value = atem
        w = a["blick"] + 0.9 * math.sin(T * 0.8 + a["phase"]) + 0.5 * math.sin(T * 2.3)
        k = nt.nodes["Kernlage"]
        k.inputs[0].default_value = 0.45 * math.cos(w)
        k.inputs[1].default_value = 0.45 * math.sin(w)
        k.inputs[2].default_value = 0.88


def leuchten_nachbearbeitung(scn):
    """Weiches Glühen um helle Stellen (Compositor Glare). Faellt still weg, wenn die API anders ist."""
    try:
        ng = bpy.data.node_groups.new("Glanz", "CompositorNodeTree")
        rl = ng.nodes.new("CompositorNodeRLayers")
        gl = ng.nodes.new("CompositorNodeGlare")
        try:
            gl.glare_type = "FOG_GLOW"
        except Exception:
            gl.inputs["Type"].default_value = "Fog Glow"
        out = ng.nodes.new("NodeGroupOutput")
        ng.interface.new_socket("Image", in_out="OUTPUT", socket_type="NodeSocketColor")
        ng.links.new(rl.outputs["Image"], gl.inputs["Image"])
        ng.links.new(gl.outputs["Image"], out.inputs[0])
        scn.compositing_node_group = ng
        print("GLANZ an")
    except Exception as e:
        print("GLANZ aus", e)


def kamera(scn, az, el, d, ziel):
    co = scn.camera
    a, e = math.radians(az), math.radians(el)
    co.location = ziel + Vector((d * math.cos(a) * math.cos(e), d * math.sin(a) * math.cos(e), d * math.sin(e)))
    co.rotation_euler = (ziel - co.location).to_track_quat("-Z", "Y").to_euler()


def bilder(lichter=(), augen_objs=(), film=0):
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
    leuchten_nachbearbeitung(scn)
    cam = bpy.data.cameras.new("C")
    co = bpy.data.objects.new("C", cam)
    scn.collection.objects.link(co)
    scn.camera = co
    cam.lens = 50
    spitze = HERZ_TAKT * 0.08 + 0.01     # Standbilder im Herzschlag
    zeit_setzen(spitze, lichter, augen_objs)
    for k, (az, el) in enumerate(((35, 25), (215, 25), (125, 20), (300, 55))):
        kamera(scn, az, el, 2.7, Vector((0, 0, 0.33)))
        scn.render.filepath = "{}_{}.png".format(OUT, k)
        bpy.ops.render.render(write_still=True)
    if augen_objs:
        kopf = sum((a.location for a in augen_objs), Vector()) / len(augen_objs)
        vorne = kopf.copy(); vorne.z = 0; vorne = vorne.normalized()
        az = math.degrees(math.atan2(vorne.y, vorne.x))
        for k, (dz, el) in enumerate(((0, 10), (-40, 15))):
            kamera(scn, az + dz, el, 0.6, kopf - Vector((0, 0, 0.03)))
            scn.render.filepath = "{}_kopf_{}.png".format(OUT, k)
            bpy.ops.render.render(write_still=True)
    if film:
        scn.render.resolution_x, scn.render.resolution_y = 900, 640
        kamera(scn, 125, 18, 2.2, Vector((0, 0, 0.36)))
        for f in range(film):
            zeit_setzen(f / 24.0, lichter, augen_objs)
            scn.render.filepath = "{}_film_{:03d}.png".format(OUT, f)
            bpy.ops.render.render(write_still=True)
        if augen_objs:
            kamera(scn, az - 20, 12, 0.7, kopf - Vector((0, 0, 0.03)))
            for f in range(film):
                zeit_setzen(f / 24.0, lichter, augen_objs)
                scn.render.filepath = "{}_augenfilm_{:03d}.png".format(OUT, f)
                bpy.ops.render.render(write_still=True)


bpy.ops.wm.read_factory_settings(use_empty=True)
koerper = mutter()
fass = flecken(koerper)
with open(OUT + "_flecken.json", "w", encoding="utf-8") as f:
    json.dump({"mulden": fass}, f, indent=1)
if NUR_FLECKEN:
    markieren(fass)
    bilder()
else:
    augen_objs = augen(koerper)
    vorher = set(bpy.context.scene.objects)
    besetzen(koerper, fass, spitzen())
    kristalle = [o for o in bpy.context.scene.objects if o not in vorher and o.type == "MESH"]
    herz_materialien(koerper, kristalle)
    bilder(herz_lichter(), augen_objs, FILM)
bpy.ops.wm.save_as_mainfile(filepath=OUT + ".blend")
