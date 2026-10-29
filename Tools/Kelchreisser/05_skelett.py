"""Step 5: skeleton and weights of the Kelchreisser.

Bones (German names, lower case; l = the creature's left = +Y, r = right):
  wurzel (root on the ground) > becken > wirbel_01 > wirbel_02 > brust > hals_01 > hals_02 > kopf
  kopf > blatt_01..05 (the five big outer petals, each folds open/shut), kiefer_ol/or/ul/ur (the four inner
    leaf jaws with the thorn teeth: upper/lower, left/right)
  brust > schulter_X > oberarm_X > unterarm_X > hand_X > finger_X          (the long ape arms)
  becken > oberschenkel_X > unterschenkel_X > mittelfuss_X > zehen_X       (the hind legs)
  per vine i (ranken.json): ranke_i_01..NN (a chain like the Pfuhlnatter's spine, hung from the nearest spine bone)
    > ranke_i_maul (the bud's base) > ranke_i_kiefer_oben / ranke_i_kiefer_unten (the two halves that snap)

Weights: the body's from Blender's heat weighting on a closed voxel copy (as Tools/Rankenluchs), carried over to the
real mesh; vines along their chain (two neighbouring bones blend at each joint, the root blends into the back), the
buds by the jaw layer of step 4, the head's petals and inner jaws by where they sit round the head's axis.

Writes arbeit/05_skelett.blend, renders the bones in the body (vorschau/05_knochen_*) and a test pose (05_pose_*).

    blender -b --factory-startup -P Tools/Kelchreisser/05_skelett.py
"""
import json
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector
from mathutils.kdtree import KDTree

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import vorschau  # noqa: E402

# Joints read off the side, front and top views of the body after step 1 (m)
WIRBELSAEULE = [("becken", (-1.7, 0, 3.0), (-1.0, 0, 3.35)), ("wirbel_01", None, (-0.2, 0, 3.5)),
                ("wirbel_02", None, (0.6, 0, 3.65)), ("brust", None, (1.2, 0, 3.8)),
                ("hals_01", None, (1.6, 0, 3.8)), ("hals_02", None, (1.95, 0, 3.7)), ("kopf", None, (2.6, 0, 3.65))]
ARM = [("schulter", (0.6, 0.45, 3.5)), ("oberarm", (0.3, 1.0, 3.2)), ("unterarm", (0.8, 1.6, 2.25)),
       ("hand", (1.5, 1.75, 1.25)), ("finger", (2.2, 1.85, 1.0)), (None, (2.85, 1.9, 0.65))]
BEIN = [("oberschenkel", (-1.55, 0.6, 2.9)), ("unterschenkel", (-1.75, 0.95, 1.6)),
        ("mittelfuss", (-2.85, 1.0, 1.45)), ("zehen", (-2.65, 1.1, 0.25)), (None, (-2.0, 1.2, 0.15))]
KOPF_ACHSE = (2.0, 0.0, 3.65)        # the flower's middle where the neck ends; petals spread round the X axis here
BLATT_WINKEL = [-125, -55, 15, 85, 150]   # petals round the head (degrees, 0 = +Y, 90 = up)
BLATT_INNEN, BLATT_AUSSEN = 0.55, 0.85  # radius round the axis where a petal starts moving / moves alone
KIEFER_AB_X = (2.05, 2.3)            # the inner jaws move from here (alone from the second value)
KIEFER = {"kiefer_ol": ((2.05, 0.2, 3.85), (3.15, 0.25, 3.55)), "kiefer_or": ((2.05, -0.2, 3.85), (3.15, -0.25, 3.55)),
          "kiefer_ul": ((2.05, 0.2, 3.35), (2.95, 0.25, 2.9)), "kiefer_ur": ((2.05, -0.2, 3.35), (2.95, -0.25, 2.9))}
RANKE_GLIED_M = 0.35                 # length of one vine bone
RANKE_WURZEL_M = 0.3                 # the vine's first stretch blends into the back's weights
GELENK = 0.25                        # share of a chain bone round each joint that blends with its neighbour
DORN_WEITE = 0.45                    # m round a vine bone (not the two at the root) where its thorns may reach
DORN_SCHRITTE = 14                   # edges a thorn is followed out from the vine
INSEL_MAX = 2000                     # smaller loose mesh pieces (thorns, claws, teeth) ride on the skin below them
GELENK_ANTEIL = 0.3                  # as in step 4: the bud's jaws part this far along its length


def spiegeln(p, seite):
    return (p[0], p[1] if seite == "l" else -p[1], p[2])


def knochen_plan(ranken, maeuler):
    """name -> (head, tail, parent, roll-up vector or None)"""
    plan = {"wurzel": ((0, 0, 0), (0.5, 0, 0), None, None)}
    vorher, eltern = None, "wurzel"
    for name, kopf, ende in WIRBELSAEULE:
        plan[name] = (kopf or vorher, ende, eltern, None)
        vorher, eltern = ende, name
    for seite in ("l", "r"):
        eltern = "brust"
        for (name, kopf), (_, ende) in zip(ARM, ARM[1:]):
            plan["%s_%s" % (name, seite)] = (spiegeln(kopf, seite), spiegeln(ende, seite), eltern, None)
            eltern = "%s_%s" % (name, seite)
        eltern = "becken"
        for (name, kopf), (_, ende) in zip(BEIN, BEIN[1:]):
            plan["%s_%s" % (name, seite)] = (spiegeln(kopf, seite), spiegeln(ende, seite), eltern, None)
            eltern = "%s_%s" % (name, seite)
    mitte = np.array(KOPF_ACHSE)
    for k, winkel in enumerate(BLATT_WINKEL):
        w = math.radians(winkel)
        richtung = np.array([0.0, math.cos(w), math.sin(w)])
        plan["blatt_%02d" % (k + 1)] = (tuple(mitte + richtung * 0.5), tuple(mitte + richtung * 1.5 + (0.2, 0, 0)),
                                        "kopf", (1.0, 0.0, 0.0))
    for name, (kopf, ende) in KIEFER.items():
        plan[name] = (kopf, ende, "kopf", (0.0, 0.0, 1.0))

    wirbel = [(n, np.array(plan[n][0]), np.array(plan[n][1])) for n, _, _ in WIRBELSAEULE]
    for i, rk in enumerate(ranken):
        m = maeuler[str(i)]
        linie = np.array(m["linie"])
        lang = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(linie, axis=0), axis=1))])
        n = max(4, int(round(lang[-1] / RANKE_GLIED_M)))
        punkte = [np.array([np.interp(s, lang, linie[:, a]) for a in range(3)]) for s in np.linspace(0, lang[-1], n + 1)]
        wurzel = punkte[0]
        eltern = min(wirbel, key=lambda w: np.linalg.norm(wurzel - (w[1] + w[2]) / 2))[0]
        for k in range(n):
            name = "ranke_%d_%02d" % (i, k + 1)
            plan[name] = (tuple(punkte[k]), tuple(punkte[k + 1]), eltern, None)
            eltern = name
        basis, achse, spalt = np.array(m["basis"]), np.array(m["achse"]), np.array(m["spalt"])
        gelenk = basis + achse * m["laenge"] * GELENK_ANTEIL
        spitze = basis + achse * m["laenge"] * 0.95
        plan["ranke_%d_maul" % i] = (tuple(basis), tuple(gelenk), eltern, tuple(spalt))
        for seite, vz in (("oben", 1), ("unten", -1)):
            versatz = spalt * vz * m["laenge"] * 0.12
            plan["ranke_%d_kiefer_%s" % (i, seite)] = (tuple(gelenk + versatz * 0.5), tuple(spitze + versatz),
                                                      "ranke_%d_maul" % i, tuple(spalt))
    return plan


def skelett_bauen(plan):
    data = bpy.data.armatures.new("Kelchreisser_Skelett")
    arm = bpy.data.objects.new("Kelchreisser_Skelett", data)
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for name, (kopf, ende, _, rolle) in plan.items():
        b = data.edit_bones.new(name)
        b.head, b.tail = Vector(kopf), Vector(ende)
        if rolle is not None:
            b.align_roll(Vector(rolle))
        else:
            b.align_roll(Vector((0, 0, 1)) if abs((b.tail - b.head).normalized().z) < 0.9 else Vector((1, 0, 0)))
    for name, (_, _, eltern, _) in plan.items():
        if eltern:
            data.edit_bones[name].parent = data.edit_bones[eltern]
            # Chains whose bones touch move as one piece (the spine, limbs, vines)
            if (data.edit_bones[name].head - data.edit_bones[eltern].tail).length < 1e-4:
                data.edit_bones[name].use_connect = True
    bpy.ops.object.mode_set(mode="OBJECT")
    return arm


def ist_koerperknochen(name):
    return not (name.startswith("ranke_") or name.startswith("blatt_") or name.startswith("kiefer_")
                or name == "wurzel")


def koerper_gewichte(obj, arm):
    """Heat weights of the body's bones on a closed voxel copy, carried over by nearest surface"""
    for b in arm.data.bones:
        b.use_deform = ist_koerperknochen(b.name)
    # The shell comes from the untouched Tripo mesh of step 1: the cut vine tips and the copied buds leave open
    # edges that make the voxel remesh fall apart
    with bpy.data.libraries.load(os.path.join(vorschau.ARBEIT, "01_vorbereitet.blend")) as (quelle, ziel):
        ziel.meshes = ["Kelchreisser"]
    kopie = obj.copy()
    kopie.data = ziel.meshes[0]
    kopie.name = "Gewichte_Huelle"
    bpy.context.scene.collection.objects.link(kopie)
    bpy.context.view_layer.objects.active = kopie
    remesh = kopie.modifiers.new("Huelle", "REMESH")
    remesh.mode = "VOXEL"
    # No decimation afterwards: its collapsed faces leave the heat solver without a solution
    remesh.voxel_size = 0.05
    bpy.ops.object.modifier_apply(modifier="Huelle")
    # The shell falls apart into the body and dozens of loose thorn tips: with them the heat solver finds no
    # solution at all, so only the body stays (the thorns take their weights from it by nearest surface)
    bm = bmesh.new()
    bm.from_mesh(kopie.data)
    gesehen, groesste = set(), []
    for v in bm.verts:
        if v in gesehen:
            continue
        stapel, teil = [v], []
        gesehen.add(v)
        while stapel:
            x = stapel.pop()
            teil.append(x)
            for e in x.link_edges:
                o = e.other_vert(x)
                if o not in gesehen:
                    gesehen.add(o)
                    stapel.append(o)
        if len(teil) > len(groesste):
            groesste = teil
    behalten = set(groesste)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v not in behalten], context="VERTS")
    bm.to_mesh(kopie.data)
    bm.free()
    print("[Kelchreisser] Huelle: %d Flaechen" % len(kopie.data.polygons))
    for o in bpy.context.scene.objects:
        o.select_set(o in (kopie, arm))
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    kopie.parent = None
    gewichtet = sum(1 for vx in kopie.data.vertices if vx.groups)
    print("[Kelchreisser] Huelle gewichtet: %d von %d Vertices" % (gewichtet, len(kopie.data.vertices)))

    bpy.context.view_layer.objects.active = obj
    for b in arm.data.bones:
        if b.name not in obj.vertex_groups:
            obj.vertex_groups.new(name=b.name)
    transfer = obj.modifiers.new("Gewichte", "DATA_TRANSFER")
    transfer.object = kopie
    transfer.use_vert_data = True
    transfer.data_types_verts = {"VGROUP_WEIGHTS"}
    transfer.vert_mapping = "POLYINTERP_NEAREST"
    transfer.layers_vgroup_select_src = "ALL"
    transfer.layers_vgroup_select_dst = "NAME"
    for o in bpy.context.scene.objects:
        o.select_set(o == obj)
    bpy.ops.object.datalayout_transfer(modifier="Gewichte")
    bpy.ops.object.modifier_apply(modifier="Gewichte")
    bpy.data.objects.remove(kopie, do_unlink=True)
    for b in arm.data.bones:
        b.use_deform = b.name != "wurzel"


def gewichte_lesen(obj):
    """Vertex weights as a dense array (vertices x groups)"""
    n, g = len(obj.data.vertices), len(obj.vertex_groups)
    w = np.zeros((n, g), dtype=np.float32)
    for vx in obj.data.vertices:
        for e in vx.groups:
            w[vx.index, e.group] = e.weight
    return w


def gewichte_schreiben(obj, w):
    """Keeps the 4 strongest per vertex, normalised (Unreal's default), and writes them back"""
    staerkste = np.argsort(-w, axis=1)[:, :4]
    for vg in obj.vertex_groups:
        vg.remove(range(len(obj.data.vertices)))
    gruppen = list(obj.vertex_groups)
    for i in range(len(w)):
        gw = w[i, staerkste[i]]
        summe = gw.sum()
        if summe <= 0:
            continue
        for g, x in zip(staerkste[i], gw / summe):
            if x > 0.001:
                gruppen[g].add([i], float(x), "REPLACE")


def schicht(obj, name):
    werte = np.zeros(len(obj.data.vertices), dtype=np.int32)
    obj.data.attributes[name].data.foreach_get("value", werte)
    return werte


def kette(P, punkte, namen):
    """Weights along a chain of bones through 'punkte' (bone k from punkte[k] to punkte[k+1]): per vertex the bone
    index blend (index arrays a, b and the share of b) and the arc position on the chain"""
    dicht = []
    for k in range(len(namen)):
        for f in np.linspace(0, 1, 12, endpoint=False):
            dicht.append((k + f, punkte[k] + (punkte[k + 1] - punkte[k]) * f))
    dicht.append((len(namen), punkte[-1]))
    pos = np.array([d[0] for d in dicht])
    xyz = np.array([d[1] for d in dicht])
    nah = np.empty(len(P), dtype=np.int64)
    for start in range(0, len(P), 20000):
        d = np.linalg.norm(P[start:start + 20000, None, :] - xyz[None, :, :], axis=2)
        nah[start:start + 20000] = np.argmin(d, axis=1)
    s = np.clip(pos[nah], 0, len(namen) - 1e-6)
    k = np.floor(s).astype(int)
    f = s - k
    a, b, anteil = k.copy(), k.copy(), np.zeros(len(P))
    vorne = (f < GELENK) & (k > 0)
    b[vorne] = k[vorne] - 1
    anteil[vorne] = 0.5 * (1 - f[vorne] / GELENK)
    hinten = (f > 1 - GELENK) & (k < len(namen) - 1)
    b[hinten] = k[hinten] + 1
    anteil[hinten] = 0.5 * (1 - (1 - f[hinten]) / GELENK)
    return a, b, anteil, s


def glatt(x, a, b):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def inseln_anhaengen(obj, P, w):
    """Thorns, claws and teeth are loose pieces of the mesh stuck into the skin: each takes the weights of the skin
    vertex nearest to it, all of it alike, so it moves as one with what it sits on"""
    nachbarn = [[] for _ in range(len(P))]
    for e in obj.data.edges:
        a, b = e.vertices
        nachbarn[a].append(b)
        nachbarn[b].append(a)
    teil = np.full(len(P), -1)
    groessen = []
    for start in range(len(P)):
        if teil[start] >= 0:
            continue
        k = len(groessen)
        teil[start] = k
        stapel, n = [start], 0
        while stapel:
            v = stapel.pop()
            n += 1
            for o in nachbarn[v]:
                if teil[o] < 0:
                    teil[o] = k
                    stapel.append(o)
        groessen.append(n)
    groessen = np.array(groessen)
    gross = groessen[teil] >= INSEL_MAX
    baum = KDTree(int(gross.sum()))
    gross_idx = np.where(gross)[0]
    for j, v in enumerate(gross_idx):
        baum.insert(P[v], j)
    baum.balance()
    klein = np.where(groessen < INSEL_MAX)[0]
    for k in klein:
        glieder = np.where(teil == k)[0]
        # The piece's root: its vertex nearest to the skin
        abstaende = [baum.find(P[v]) for v in glieder]
        wurzel = min(abstaende, key=lambda f: f[2])
        w[glieder] = w[gross_idx[wurzel[1]]]
    print("[Kelchreisser] %d lose Teile (%d Vertices) an die Haut gehaengt" % (len(klein), (~gross).sum()))


def dornen_mitnehmen(obj, P, w, plan, ranke_v, maul_v):
    """The long thorns on the vines and buds reach out past the vine's capture radius (step 3) and kept the body's
    weights: grown out from the vine over the mesh, each such vertex takes the mean of its vine neighbours. Only
    away from the root (there the vine grows out of the back, which keeps its own)"""
    nachbarn = [[] for _ in range(len(P))]
    for e in obj.data.edges:
        a, b = e.vertices
        nachbarn[a].append(b)
        nachbarn[b].append(a)
    ranke = (ranke_v >= 0) | (maul_v >= 0)
    # Vertices near a vine but past its root
    nahe = np.zeros(len(P), bool)
    for n, (kopf, ende, _, _) in plan.items():
        if not n.startswith("ranke_") or n.endswith("_01") or n.endswith("_02"):
            continue
        a, b = np.array(kopf), np.array(ende)
        t = np.clip(((P - a) @ (b - a)) / max((b - a) @ (b - a), 1e-9), 0, 1)
        nahe |= np.linalg.norm(a + t[:, None] * (b - a) - P, axis=1) < DORN_WEITE
    offen = set(np.where(nahe & ~ranke)[0])
    dabei = ranke.copy()
    neu = 0
    for _ in range(DORN_SCHRITTE):
        rand = [v for v in offen if any(dabei[o] for o in nachbarn[v])]
        if not rand:
            break
        for v in rand:
            nb = [o for o in nachbarn[v] if dabei[o]]
            w[v] = w[nb].mean(axis=0)
        for v in rand:
            dabei[v] = True
            offen.discard(v)
        neu += len(rand)
    print("[Kelchreisser] %d Dorn-Vertices an ihre Ranke gehaengt" % neu)


def teile_gewichte(obj, arm, plan, ranken, maeuler):
    P = np.array([vx.co[:] for vx in obj.data.vertices])
    w = gewichte_lesen(obj)
    idx = {vg.name: vg.index for vg in obj.vertex_groups}

    # Thin bits the voxel shell lost: fully on the nearest body bone
    ohne = np.where(w.sum(axis=1) < 1e-3)[0]
    koerper = [n for n in plan if ist_koerperknochen(n)]
    if len(ohne):
        best = np.full(len(ohne), np.inf)
        wahl = np.zeros(len(ohne), dtype=np.int64)
        for n in koerper:
            a, b = np.array(plan[n][0]), np.array(plan[n][1])
            ab = b - a
            t = np.clip(((P[ohne] - a) @ ab) / max(ab @ ab, 1e-9), 0, 1)
            d = np.linalg.norm(a + t[:, None] * ab - P[ohne], axis=1)
            besser = d < best
            best[besser] = d[besser]
            wahl[besser] = idx[n]
        w[ohne, wahl] = 1.0
        print("[Kelchreisser] %d Vertices ohne Huellen-Gewicht an den naechsten Knochen gehaengt" % len(ohne))

    # Head: petals and inner jaws round the flower's axis
    mitte = np.array(KOPF_ACHSE)
    kopf = (P[:, 0] > mitte[0] - 0.1) & (P[:, 2] > 1.8)
    ranke_v, maul_v, kiefer_v = schicht(obj, "ranke"), schicht(obj, "maul"), schicht(obj, "kiefer")
    kopf &= ranke_v < 0
    r = np.hypot(P[:, 1], P[:, 2] - mitte[2])
    winkel = np.degrees(np.arctan2(P[:, 2] - mitte[2], P[:, 1]))
    blatt = kopf & (r > BLATT_INNEN)
    abst = np.abs(((winkel[:, None] - np.array(BLATT_WINKEL)[None, :]) + 180) % 360 - 180)
    welches = np.argmin(abst, axis=1)
    anteil = glatt(r, BLATT_INNEN, BLATT_AUSSEN)
    for i in np.where(blatt)[0]:
        w[i] *= 1 - anteil[i]
        w[i, idx["blatt_%02d" % (welches[i] + 1)]] += anteil[i]
    innen = kopf & ~blatt & (P[:, 0] > KIEFER_AB_X[0])
    anteil = glatt(P[:, 0], *KIEFER_AB_X)
    # Each vertex to the jaws by its distance to their bones (the long upper teeth hang down past the slit and
    # still belong to the upper jaw); where two are about as near, as where the teeth meet, they share
    ii = np.where(innen)[0]
    abstand = []
    for name, (a, b) in KIEFER.items():
        a, b = np.array(a), np.array(b)
        t = np.clip(((P[ii] - a) @ (b - a)) / ((b - a) @ (b - a)), 0, 1)
        abstand.append(np.linalg.norm(a + t[:, None] * (b - a) - P[ii], axis=1))
    naehe = 1.0 / np.maximum(np.array(abstand).T, 0.02) ** 6
    naehe /= naehe.sum(axis=1, keepdims=True)
    w[ii] *= (1 - anteil[ii])[:, None]
    for k, name in enumerate(KIEFER):
        w[ii, idx[name]] += anteil[ii] * naehe[:, k]
    print("[Kelchreisser] Kopf: %d Blatt-, %d Kiefer-Vertices" % (blatt.sum(), innen.sum()))

    # Vines along their chains, the buds by their jaw layer
    for i in range(len(ranken)):
        namen = sorted(n for n in plan if n.startswith("ranke_%d_" % i) and n[-2:].isdigit())
        punkte = [np.array(plan[n][0]) for n in namen] + [np.array(plan[namen[-1]][1])]
        auf = np.where((ranke_v == i) & (maul_v < 0))[0]
        a, b, teil, s = kette(P[auf], punkte, namen)
        wurzel = glatt(s * np.linalg.norm(punkte[1] - punkte[0]), 0, RANKE_WURZEL_M)
        neu = np.zeros((len(auf), w.shape[1]), dtype=np.float32)
        ia = np.array([idx[namen[k]] for k in a])
        ib = np.array([idx[namen[k]] for k in b])
        neu[np.arange(len(auf)), ia] += 1 - teil
        neu[np.arange(len(auf)), ib] += teil
        w[auf] = w[auf] * (1 - wurzel[:, None]) + neu * wurzel[:, None]

        m = maeuler[str(i)]
        basis, achse = np.array(m["basis"]), np.array(m["achse"])
        im_maul = np.where(maul_v == i)[0]
        t = (P[im_maul] - basis) @ achse / m["laenge"]
        letzter = idx[namen[-1]]
        maul = idx["ranke_%d_maul" % i]
        w[im_maul] = 0
        stiel = t < 0
        w[im_maul[stiel], letzter] = 1.0
        rest = im_maul[~stiel]
        kiefer = kiefer_v[rest]
        auf_kiefer = glatt(t[~stiel], GELENK_ANTEIL, GELENK_ANTEIL + 0.12) * (kiefer != 0)
        w[rest, maul] = 1 - auf_kiefer
        for seite, vz in (("oben", 1), ("unten", -1)):
            j = kiefer == vz
            w[rest[j], idx["ranke_%d_kiefer_%s" % (i, seite)]] = auf_kiefer[j]
        print("[Kelchreisser] Ranke %d: %d Glieder, %d Ranken-, %d Maul-Vertices" % (
            i, len(namen), len(auf), len(im_maul)))
    inseln_anhaengen(obj, P, w)
    dornen_mitnehmen(obj, P, w, plan, ranke_v, maul_v)
    gewichte_schreiben(obj, w)


def knochen_bild(arm, name, mitte_, groesse):
    """Renders the mesh see-through with the bones as red sticks"""
    stoecke = []
    for b in arm.data.bones:
        kopf = arm.matrix_world @ b.head_local
        ende = arm.matrix_world @ b.tail_local
        bpy.ops.mesh.primitive_cylinder_add(radius=0.03, depth=(ende - kopf).length, location=(kopf + ende) / 2)
        stock = bpy.context.active_object
        stock.rotation_euler = (ende - kopf).to_track_quat("Z", "Y").to_euler()
        stoecke.append(stock)
    shading = bpy.context.scene.display.shading
    vorschau.szene_einrichten()
    shading.show_xray = True
    shading.xray_alpha = 0.35
    shading.color_type = "OBJECT"
    for s in stoecke:
        s.color = (1.0, 0.1, 0.1, 1.0)
    vorschau.rendern(name + "_seite", (0.0, -1.0, 0.05), mitte_, groesse)
    vorschau.rendern(name + "_vorn", (1.0, 0.0, 0.05), mitte_, groesse)
    vorschau.rendern(name + "_oben", (0.0, 0.0, 1.0), mitte_, groesse)
    shading.show_xray = False
    for s in stoecke:
        bpy.data.objects.remove(s, do_unlink=True)


def pose_test(arm, ranken, mitte_, groesse):
    """Bent limbs, curled vines with open mouths, open flower: the mesh must follow without tearing"""
    pb = arm.pose.bones
    for b in pb:
        b.rotation_mode = "XYZ"
    for name, grad in {"oberarm_l": -30, "unterarm_l": 40, "hand_l": -20, "oberschenkel_r": 25,
                       "unterschenkel_r": -30, "hals_01": -10, "kopf": 15}.items():
        pb[name].rotation_euler = (math.radians(grad), 0.0, 0.0)
    for k in range(1, 6):
        pb["blatt_%02d" % k].rotation_euler = (0.0, 0.0, math.radians(-35))
    for name in ("kiefer_ol", "kiefer_or"):
        pb[name].rotation_euler = (math.radians(25), 0.0, 0.0)
    for name in ("kiefer_ul", "kiefer_ur"):
        pb[name].rotation_euler = (math.radians(-25), 0.0, 0.0)
    for i in range(len(ranken)):
        for b in pb:
            if b.name.startswith("ranke_%d_" % i) and b.name[-2:].isdigit():
                b.rotation_euler = (math.radians(14), 0.0, math.radians(8 if i % 2 else -8))
        pb["ranke_%d_kiefer_oben" % i].rotation_euler = (math.radians(-35), 0.0, 0.0)
        pb["ranke_%d_kiefer_unten" % i].rotation_euler = (math.radians(35), 0.0, 0.0)
    bpy.context.view_layer.update()
    vorschau.rendern("05_pose_seite", (0.0, -1.0, 0.05), mitte_, groesse)
    vorschau.rendern("05_pose_spielkamera", (0.45, -0.8, math.tan(math.radians(50))), mitte_, groesse)
    vorschau.rendern("05_pose_kopf", (1.0, -0.3, 0.2), (2.4, 0.0, 3.6), 3.2)
    for b in pb:
        b.rotation_euler = (0, 0, 0)
    bpy.context.view_layer.update()


def main():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(vorschau.ARBEIT, "04_maeuler.blend"))
    obj = bpy.data.objects["Kelchreisser"]
    with open(os.path.join(vorschau.ARBEIT, "ranken.json"), encoding="utf-8") as f:
        ranken = json.load(f)
    with open(os.path.join(vorschau.ARBEIT, "maeuler.json"), encoding="utf-8") as f:
        maeuler = json.load(f)
    plan = knochen_plan(ranken, maeuler)
    arm = skelett_bauen(plan)
    print("[Kelchreisser] %d Knochen" % len(arm.data.bones))
    knochen_bild(arm, "05_knochen", (0.0, 0.0, 3.2), 7.4)
    koerper_gewichte(obj, arm)
    teile_gewichte(obj, arm, plan, ranken, maeuler)
    for m in list(obj.modifiers):
        obj.modifiers.remove(m)
    mod = obj.modifiers.new("Skelett", "ARMATURE")
    mod.object = arm
    obj.parent = arm
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(vorschau.ARBEIT, "05_skelett.blend"))
    pose_test(arm, ranken, (0.0, 0.0, 3.2), 7.4)


main()
