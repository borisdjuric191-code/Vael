"""Step 3: skeleton and weights of the Pfuhlnatter.

Bones (German names, lower case):
  wurzel (root on the ground) > rumpf_01 .. rumpf_NN (the spine from the tail root to the neck top, a chain) > kopf
  fins, each one bone that folds the fin flat against the body and raises it again:
    krone_01 .. (the dahlia crown round the head, children of kopf)
    flosse_<NN>_<seite> (crest on the back and fins on the flanks, children of the spine bone they grow from;
      seite = oben, links, rechts, unten)
    the tail fan comes out as flosse_01_* on the first spine bone

The centre line is found by refining a rough path (read off the side and top views of step 1) a few times: each
point moves to the median of the body texels in a thin slab across the line. Fins are what sticks out of the body
further than its radius; around the head the crown is what sticks out behind and above, not the fangs below the
snout. Weights: along the line between the two nearest spine bones (a snake is a tube), fins fully on their bone a
little away from the body.

Writes arbeit/03_skelett.blend and arbeit/skelett.json (bone lengths, fins with their fold directions), renders the
bones in the body (vorschau/03_knochen_*) and the fins folded (vorschau/03_gefaltet_*).

    blender -b --factory-startup -P Tools/Pfuhlnatter/03_skelett.py
"""
import json
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import vorschau  # noqa: E402

RUMPF_KNOCHEN = 26
# Rough path from the tail root (base of the tail fan) along the body, up the reared neck to the back of the head,
# read off vorschau/01_seite and 01_oben (m)
GROBER_WEG = [(-1.70, 0.25, 0.80), (-1.62, 0.12, 0.48), (-1.46, -0.04, 0.27), (-1.22, -0.26, 0.17), (-1.0, -0.28, 0.2),
              (-0.8, -0.1, 0.22), (-0.6, 0.07, 0.26), (-0.33, 0.21, 0.2), (-0.1, 0.23, 0.27), (0.1, 0.15, 0.47),
              (0.35, 0.05, 0.58), (0.62, 0.04, 0.5), (0.9, 0.03, 0.4), (1.2, 0.0, 0.22), (1.5, -0.02, 0.24),
              (1.74, -0.05, 0.36), (1.82, -0.06, 0.6), (1.76, -0.07, 0.86), (1.64, -0.09, 1.07), (1.6, -0.12, 1.3),
              (1.7, -0.17, 1.48)]
SCHNAUZE = (2.07, -0.18, 1.47)
KOPF_RADIUS = 0.42      # m around the head's middle that belong to the head
FLOSSE_ABSTAND = 1.18   # a vertex further out than this many body radii is fin
FLOSSE_VOLL = 1.45      # from here on it moves with its fin alone
ABSCHNITT_M = 0.24      # fins along the body are grouped in pieces this long (one spine bone: a longer piece
                        # stays rigid while the spine bends under it and tears into planks)
MIN_FLOSSE = 120        # vertices a fin needs to get a bone
KRONE_SEKTOREN = 9
FALTEN_MAX = 50.0       # degrees a fin turns at most when it folds (a big membrane on one bone swinging further
                        # turns into a plank); the rest of the folding is done by drawing it in
FALTEN_GROESSE = (0.7, 0.38, 0.7)   # scale of a folded fin bone (x, length, z): drawn in towards the body


def glatt(punkte, n):
    """Evenly spaced points along a polyline"""
    punkte = np.asarray(punkte, dtype=float)
    laenge = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(punkte, axis=0), axis=1))])
    ziel = np.linspace(0.0, laenge[-1], n)
    return np.stack([np.interp(ziel, laenge, punkte[:, i]) for i in range(3)], axis=1)


def tangenten(linie):
    t = np.gradient(linie, axis=0)
    return t / np.linalg.norm(t, axis=1)[:, None]


def mittellinie(P, koerper):
    """Refines the rough path: each point to the median of body vertices in a thin slab across the line"""
    linie = glatt(GROBER_WEG, 60)
    B = P[koerper]
    for _ in range(5):
        t = tangenten(linie)
        neu = []
        for c, ti in zip(linie, t):
            d = B - c
            entlang = d @ ti
            quer = np.linalg.norm(d - entlang[:, None] * ti, axis=1)
            s = B[(np.abs(entlang) < 0.035) & (quer < 0.32)]
            neu.append(np.median(s, axis=0) if len(s) > 30 else c)
        neu = np.array(neu)
        # Smooth and keep the ends where the rough path put them
        weich = neu.copy()
        weich[1:-1] = (neu[:-2] + 2 * neu[1:-1] + neu[2:]) / 4
        weich[0], weich[-1] = linie[0], linie[-1]
        linie = glatt(weich, 60)
    return linie


def radien(P, koerper, linie):
    B = P[koerper]
    t = tangenten(linie)
    r = []
    for c, ti in zip(linie, t):
        d = B - c
        entlang = d @ ti
        quer = np.linalg.norm(d - entlang[:, None] * ti, axis=1)
        s = quer[(np.abs(entlang) < 0.035) & (quer < 0.4)]
        r.append(np.percentile(s, 70) if len(s) > 30 else 0.12)
    r = np.array(r)
    return np.convolve(np.pad(r, 2, mode="edge"), np.ones(5) / 5, mode="valid")


def rahmen(linie):
    """Rotation-minimising frames along the line, starting with up = world Z at the tail"""
    t = tangenten(linie)
    oben = [np.array([0.0, 0.0, 1.0]) - t[0] * t[0][2]]
    oben[0] /= np.linalg.norm(oben[0])
    for i in range(1, len(t)):
        u = oben[-1] - t[i] * (oben[-1] @ t[i])
        oben.append(u / np.linalg.norm(u))
    oben = np.array(oben)
    links = np.cross(t, oben)
    return t, oben, links


def naechster(P, linie):
    """For every point: nearest segment index, fraction along it, distance to the line"""
    a, b = linie[:-1], linie[1:]
    ab = b - a
    beste_i = np.zeros(len(P), dtype=int)
    beste_f = np.zeros(len(P))
    beste_d = np.full(len(P), 1e9)
    for i in range(len(a)):
        f = np.clip(((P - a[i]) @ ab[i]) / (ab[i] @ ab[i]), 0.0, 1.0)
        d = np.linalg.norm(P - (a[i] + f[:, None] * ab[i]), axis=1)
        besser = d < beste_d
        beste_i[besser], beste_f[besser], beste_d[besser] = i, f[besser], d[besser]
    return beste_i, beste_f, beste_d


def main():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(vorschau.ARBEIT, "01_vorbereitet.blend"))
    obj = bpy.data.objects["Pfuhlnatter"]
    roh = np.load(os.path.join(vorschau.ARBEIT, "analyse_roh.npz"))
    P = roh["punkte"]
    koerper = ~roh["flosse"]

    linie = mittellinie(P, koerper)
    r_linie = radien(P, koerper, linie)
    laenge = float(np.sum(np.linalg.norm(np.diff(linie, axis=0), axis=1)))
    print("[Pfuhlnatter] Mittellinie %.2f m, Radius %.2f-%.2f m" % (laenge, r_linie.min(), r_linie.max()))

    # Spine joints evenly along the line; the head from the neck top to the snout
    gelenke = glatt(linie, RUMPF_KNOCHEN + 1)
    schnauze = np.array(SCHNAUZE)
    kopf_mitte = gelenke[-1] + (schnauze - gelenke[-1]) * 0.45

    i_seg, f_seg, abstand = naechster(P, linie)
    s_param = (i_seg + f_seg) / (len(linie) - 1)          # 0 at the tail root, 1 at the neck top
    radius_hier = np.interp(s_param, np.linspace(0, 1, len(r_linie)), r_linie)
    t, oben, links = rahmen(linie)

    # The head: everything near its middle, ahead of the neck top
    zum_kopf = P - kopf_mitte
    kopf_achse = (schnauze - gelenke[-1]) / np.linalg.norm(schnauze - gelenke[-1])
    im_kopf = (np.linalg.norm(zum_kopf, axis=1) < KOPF_RADIUS) & (s_param > 0.9)
    # Crown: sticks out of the head behind and above the snout; fangs hang below the front of the head
    vorn = zum_kopf @ kopf_achse
    unter = zum_kopf[:, 2]
    zahn = im_kopf & (vorn > 0.05) & (unter < -0.08)
    kopf_abstand = np.linalg.norm(zum_kopf - np.outer(vorn, kopf_achse), axis=1)
    # Only round the head itself: the crest further down the neck must bend with the neck
    krone = (s_param > 0.82) & ~zahn & (kopf_abstand > 0.17) & (np.linalg.norm(zum_kopf, axis=1) < 0.6) & (vorn < 0.12) &         (vorn > -0.4) & ~koerper
    flosse = ~im_kopf & ~krone & (abstand > radius_hier * FLOSSE_ABSTAND)
    print("[Pfuhlnatter] Kopf %d, Krone %d, Flossen %d, Zaehne %d Vertices" % (im_kopf.sum(), krone.sum(), flosse.sum(), zahn.sum()))

    # ---- Bones
    data = bpy.data.armatures.new("Pfuhlnatter_Skelett")
    arm = bpy.data.objects.new("Pfuhlnatter_Skelett", data)
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    eb = data.edit_bones
    w = eb.new("wurzel")
    w.head, w.tail = (0, 0, 0), (0.25, 0, 0)
    rumpf = []
    j_t, j_oben, _ = rahmen(gelenke)
    for i in range(RUMPF_KNOCHEN):
        b = eb.new("rumpf_%02d" % (i + 1))
        b.head, b.tail = Vector(gelenke[i]), Vector(gelenke[i + 1])
        b.align_roll(Vector(j_oben[i]))
        b.parent = w if i == 0 else rumpf[-1]
        b.use_connect = i > 0
        rumpf.append(b)
    k = eb.new("kopf")
    k.head, k.tail = Vector(gelenke[-1]), Vector(schnauze)
    k.align_roll(Vector((0, 0, 1)))
    k.parent, k.use_connect = rumpf[-1], True

    flossen = []

    def flossen_knochen(name, auswahl, eltern, achse, aussen_von, anlegen=0.3):
        pts = P[auswahl]
        if len(pts) < MIN_FLOSSE:
            return
        basis_abstand = np.linalg.norm(pts - aussen_von, axis=1)
        basis = pts[basis_abstand <= np.percentile(basis_abstand, 4)].mean(axis=0)
        spitze = pts[basis_abstand >= np.percentile(basis_abstand, 94)].mean(axis=0)
        if np.linalg.norm(spitze - basis) < 0.06:
            return
        b = eb.new(name)
        b.head, b.tail = Vector(basis), Vector(spitze)
        b.align_roll(Vector(achse))
        b.parent = eb[eltern]
        richtung = (spitze - basis) / np.linalg.norm(spitze - basis)
        # Folded: laid back along the body (towards the tail) and onto it, a little outward
        aussen = richtung - achse * (richtung @ achse)
        aussen = aussen / max(np.linalg.norm(aussen), 1e-6)
        gefaltet = -achse * 0.85 + aussen * anlegen
        gefaltet /= np.linalg.norm(gefaltet)
        # Never turned further than FALTEN_MAX: a fin standing forward would otherwise swing through the body
        winkel_ = math.acos(max(-1.0, min(1.0, float(richtung @ gefaltet))))
        if winkel_ > math.radians(FALTEN_MAX):
            achse_ = np.cross(richtung, gefaltet)
            achse_ /= max(np.linalg.norm(achse_), 1e-6)
            gefaltet = np.array(Matrix.Rotation(math.radians(FALTEN_MAX), 3, Vector(achse_)) @ Vector(richtung))
        flossen.append({"name": name, "eltern": eltern, "anzahl": int(len(pts)), "auswahl": np.flatnonzero(auswahl),
                        "basis": basis.tolist(), "spitze": spitze.tolist(), "gefaltet": gefaltet.tolist()})

    # Crown: sectors round the head axis
    seiten = np.cross(kopf_achse, [0.0, 0.0, 1.0])
    seiten /= np.linalg.norm(seiten)
    hoch = np.cross(seiten, kopf_achse)
    winkel = np.arctan2(zum_kopf @ hoch, zum_kopf @ seiten)
    sektor = ((winkel + math.pi) / (2 * math.pi) * KRONE_SEKTOREN).astype(int) % KRONE_SEKTOREN
    for s in range(KRONE_SEKTOREN):
        # Folded like a closed umbrella: back along the neck, still spread round it
        flossen_knochen("krone_%02d" % (s + 1), krone & (sektor == s), "kopf", kopf_achse, kopf_mitte, anlegen=0.6)

    # Body fins: pieces along the line, sectors round it
    stueck = np.minimum((s_param * laenge / ABSCHNITT_M).astype(int), int(laenge / ABSCHNITT_M))
    zur_linie = P - (linie[i_seg] + f_seg[:, None] * (linie[np.minimum(i_seg + 1, len(linie) - 1)] - linie[i_seg]))
    rund = np.arctan2(np.einsum("ij,ij->i", zur_linie, links[i_seg]), np.einsum("ij,ij->i", zur_linie, oben[i_seg]))
    namen = {0: "oben", 1: "links", 2: "unten", 3: "rechts"}
    seite = (((rund + math.pi / 4) % (2 * math.pi)) / (math.pi / 2)).astype(int) % 4
    for st in range(stueck.max() + 1):
        for sd in range(4):
            # Below the body are belly scales, never fins
            if namen[sd] == "unten":
                continue
            auswahl = flosse & (stueck == st) & (seite == sd)
            if auswahl.sum() < MIN_FLOSSE:
                continue
            s_mitte = float(np.median(s_param[auswahl]))
            knochen = min(RUMPF_KNOCHEN, max(1, int(s_mitte * RUMPF_KNOCHEN) + 1))
            j = min(int(s_mitte * (len(linie) - 1)), len(linie) - 1)
            flossen_knochen("flosse_%02d_%s" % (st + 1, namen[sd]), auswahl, "rumpf_%02d" % knochen, t[j], linie[j])
    bpy.ops.object.mode_set(mode="OBJECT")
    print("[Pfuhlnatter] %d Knochen, davon %d Flossen: %s" % (len(data.bones), len(flossen), [f["name"] for f in flossen]))

    # ---- Weights
    gruppen = {b.name: obj.vertex_groups.new(name=b.name) for b in data.bones if b.name != "wurzel"}
    # Spine: bone b covers joints b..b+1; blend between neighbours by the position along the line
    s_gelenk = s_param * RUMPF_KNOCHEN - 0.5                 # 0 = middle of rumpf_01
    s_gelenk = np.clip(s_gelenk, 0.0, RUMPF_KNOCHEN - 1.0)
    unten_k = np.floor(s_gelenk).astype(int)
    anteil = s_gelenk - unten_k
    # Head against neck: a soft change over the neck top, measured along the head's axis
    entlang_kopf = (P - gelenke[-1]) @ kopf_achse
    kopf_w = np.clip((entlang_kopf + 0.12) / 0.2, 0.0, 1.0) * (s_param > 0.85)
    kopf_w = np.where(krone | zahn, 1.0, kopf_w)
    gewicht = {}
    for v in range(len(P)):
        g = {}
        a = unten_k[v]
        rest = 1.0 - kopf_w[v]
        g["rumpf_%02d" % (a + 1)] = rest * (1.0 - anteil[v])
        if a + 1 < RUMPF_KNOCHEN:
            g["rumpf_%02d" % (a + 2)] = rest * anteil[v]
        if kopf_w[v] > 0:
            g["kopf"] = kopf_w[v]
        gewicht[v] = g
    for f in flossen:
        for v in f["auswahl"]:
            if krone[v]:
                voll = np.clip((kopf_abstand[v] - 0.15) / 0.12, 0.0, 1.0)
            else:
                voll = np.clip((abstand[v] / radius_hier[v] - FLOSSE_ABSTAND) / (FLOSSE_VOLL - FLOSSE_ABSTAND), 0.0, 1.0)
            g = {n: x * (1.0 - voll) for n, x in gewicht[v].items()}
            g[f["name"]] = voll
            gewicht[v] = g
    for v, g in gewicht.items():
        for n, x in g.items():
            if x > 0.001:
                gruppen[n].add([v], float(x), "REPLACE")
    mod = obj.modifiers.new("Skelett", "ARMATURE")
    mod.object = arm
    obj.parent = arm

    os.makedirs(vorschau.ARBEIT, exist_ok=True)
    with open(os.path.join(vorschau.ARBEIT, "skelett.json"), "w", encoding="utf-8") as f:
        json.dump({"laenge_m": laenge, "falten_groesse": FALTEN_GROESSE, "rumpf_knochen": RUMPF_KNOCHEN, "radien": r_linie.tolist(),
                   "gelenke": gelenke.tolist(), "schnauze": SCHNAUZE,
                   "flossen": [{k: v for k, v in fl.items() if k != "auswahl"} for fl in flossen]}, f, indent=1)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(vorschau.ARBEIT, "03_skelett.blend"))

    # ---- Checks: bones in the body, then all fins folded
    stoecke = []
    for b in data.bones:
        kopf, ende = b.head_local, b.tail_local
        bpy.ops.mesh.primitive_cylinder_add(radius=0.012, depth=(ende - kopf).length, location=(kopf + ende) / 2)
        s = bpy.context.active_object
        s.rotation_euler = (ende - kopf).to_track_quat("Z", "Y").to_euler()
        s.color = (1.0, 0.1, 0.1, 1.0) if b.name.startswith(("rumpf", "kopf")) else (0.1, 1.0, 0.3, 1.0)
        stoecke.append(s)
    shading = bpy.context.scene.display.shading
    vorschau.szene_einrichten()
    shading.show_xray, shading.xray_alpha, shading.color_type = True, 0.3, "OBJECT"
    vorschau.rendern("03_knochen_seite", (0.0, -1.0, 0.05), (0.0, 0.0, 1.1), 4.6)
    vorschau.rendern("03_knochen_oben", (0.0, 0.0, 1.0), (0.0, 0.0, 1.1), 4.6)
    shading.show_xray = False
    for s in stoecke:
        bpy.data.objects.remove(s, do_unlink=True)

    for fl in flossen:
        pb = arm.pose.bones[fl["name"]]
        rest = arm.data.bones[fl["name"]].matrix_local.to_3x3()
        richtung = (Vector(fl["spitze"]) - Vector(fl["basis"])).normalized()
        welt = richtung.rotation_difference(Vector(fl["gefaltet"])).to_matrix()
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = (rest.inverted() @ welt @ rest).to_quaternion()
        pb.scale = FALTEN_GROESSE
    bpy.context.view_layer.update()
    vorschau.rendern("03_gefaltet_seite", (0.0, -1.0, 0.05), (0.0, 0.0, 1.1), 4.6)
    vorschau.rendern("03_gefaltet_spielkamera", (0.45, -0.8, math.tan(math.radians(50))), (0.0, 0.0, 1.1), 4.6)


main()
