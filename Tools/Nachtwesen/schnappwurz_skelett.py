"""Schnappwurz, step 2: finds the stalks in the mesh, gives each a chain of bones and its trap two jaws.

The Tripo plant is one welded mesh. Above the leaf rosette (SCHNITT metres) the stalks are separate pieces of it:
each piece is followed upward along its surface, its middle line becomes the chain (the way of the Floromant's
plant arms), the thick end is the trap. Mound and rosette stay on "strunk" and never move.

  wurzel, strunk
  stiel_<k>_1..6, falle_<k>, kiefer_<k>_o / kiefer_<k>_u     k = 1 is the tallest stalk

Writes arbeit/stiele.json (per stalk: jaw hinge axis, whether its trap is modelled open, head centre).

    blender -b --factory-startup -P Tools/Nachtwesen/schnappwurz_skelett.py
"""
import json
import math
import os
import sys
from collections import deque

import numpy as np

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bau  # noqa: E402
import gemein  # noqa: E402
from bau import K  # noqa: E402

NAME = "Schnappwurz"
MITTE = (0.0, 0.0, 1.2)
SCHNITT = 0.74
GLIEDER = 6
MIN_PUNKTE = 120
OFFENE_FALLEN = (1, 3)


def stiele_finden(obj):
    """Pieces of the mesh above the cut, each with its points' distance along the surface from the cut"""
    P = gemein.punkte(obj)
    oben = P[:, 2] > SCHNITT
    nachbarn = [[] for _ in range(len(P))]
    for a, b in bau.kanten(obj):
        if oben[a] and oben[b]:
            nachbarn[a].append(b)
            nachbarn[b].append(a)
    marke = -np.ones(len(P), dtype=int)
    stiele = []
    for start in np.where(oben)[0]:
        if marke[start] >= 0:
            continue
        nummer = len(stiele)
        teil = [start]
        marke[start] = nummer
        schlange = deque([start])
        while schlange:
            i = schlange.popleft()
            for j in nachbarn[i]:
                if marke[j] < 0:
                    marke[j] = nummer
                    teil.append(j)
                    schlange.append(j)
        stiele.append(np.array(teil))
    ergebnis = []
    for teil in stiele:
        if len(teil) < MIN_PUNKTE:
            continue
        # distance along the surface from the points nearest the cut
        fuss = teil[P[teil, 2] < P[teil, 2].min() + 0.04]
        weg = {int(i): 0.0 for i in fuss}
        schlange = deque(int(i) for i in fuss)
        import heapq
        haufen = [(0.0, int(i)) for i in fuss]
        while haufen:
            d, i = heapq.heappop(haufen)
            if d > weg.get(i, 1e9):
                continue
            for j in nachbarn[i]:
                nd = d + float(np.linalg.norm(P[i] - P[j]))
                if nd < weg.get(j, 1e9):
                    weg[j] = nd
                    heapq.heappush(haufen, (nd, j))
        idx = np.array([i for i in teil if int(i) in weg])
        ergebnis.append((idx, np.array([weg[int(i)] for i in idx])))
    return P, ergebnis


def mittellinie(P, idx, weg, schritte=28):
    """Centres and radii of bands of equal distance along the stalk"""
    lang = weg.max()
    mitten, radien, grenzen = [], [], np.linspace(0, lang, schritte + 1)
    for a, b in zip(grenzen[:-1], grenzen[1:]):
        m = (weg >= a) & (weg <= b)
        if m.sum() < 3:
            mitten.append(mitten[-1] if mitten else P[idx].mean(axis=0))
            radien.append(radien[-1] if radien else 0.02)
            continue
        c = P[idx[m]].mean(axis=0)
        mitten.append(c)
        radien.append(float(np.linalg.norm(P[idx[m]] - c, axis=1).mean()))
    return np.array(mitten), np.array(radien), grenzen


def main():
    obj, _ = gemein.oeffnen(NAME, "01_vorbereitet.blend")
    P, stiele = stiele_finden(obj)
    daten = []
    for idx, weg in stiele:
        mitten, radien, grenzen = mittellinie(P, idx, weg)
        duenn = float(np.median(np.sort(radien)[:len(radien) // 2]))
        # the trap is the thick run at the far end: back from the last thick band while it stays thick
        schwelle = 1.7 * duenn
        dick = np.where(radien > schwelle)[0]
        kopf_ab = len(radien) - 4
        if len(dick) and dick[-1] >= len(radien) - 6:
            kopf_ab = int(dick[-1])
            while kopf_ab > 6 and radien[kopf_ab - 1] > 1.25 * duenn:
                kopf_ab -= 1
        gemein.log(NAME, "  Radien:", " ".join("%.0f" % (r * 1000) for r in radien), "-> Falle ab", kopf_ab)
        daten.append(dict(idx=idx, weg=weg, mitten=mitten, radien=radien, grenzen=grenzen, kopf_ab=kopf_ab,
                          spitze=float(P[idx, 2].max()), duenn=duenn))
    daten.sort(key=lambda d: -d["spitze"])
    for k, d in enumerate(daten):
        gemein.log(NAME, "Stiel %d: %d Punkte, Spitze %.2f m, Laenge %.2f m, Stiel-Radius %.3f, Falle ab Band %d/%d" % (
            k + 1, len(d["idx"]), d["spitze"], d["weg"].max(), d["duenn"], d["kopf_ab"], len(d["radien"])))

    plan = {"wurzel": K((0, 0, 0), (0, 0, 0.3), gewicht=False)}
    plan["strunk"] = K((0, 0, 0.1), (0, 0, 0.6), "wurzel", gewicht=False)
    stiel_info = []
    for k, d in enumerate(daten):
        n = k + 1
        mitten, kopf_ab = d["mitten"], d["kopf_ab"]
        # joints at equal steps along the stalk up to where the trap begins
        stellen = np.linspace(0, kopf_ab, GLIEDER + 1)
        gelenke = [mitten[min(int(round(s)), len(mitten) - 1)] for s in stellen]
        gelenke[0] = gelenke[0] - np.array([0, 0, 0.03])
        namen = bau.kette(plan, "stiel_%d" % n, [tuple(g) for g in gelenke], "strunk", gewicht=False)
        kopf_idx = d["idx"][d["weg"] >= d["grenzen"][kopf_ab]]
        ansatz = gelenke[-1]
        kopf_mitte = P[kopf_idx].mean(axis=0)
        achse = kopf_mitte - ansatz
        achse /= max(np.linalg.norm(achse), 1e-6)
        fern = kopf_idx[np.argmax((P[kopf_idx] - ansatz) @ achse)]
        spitze = ansatz + achse * float((P[fern] - ansatz) @ achse)
        plan["falle_%d" % n] = K(tuple(ansatz), tuple(spitze), namen[-1], gewicht=False)
        # the two lobes: the direction across the trap's axis in which its points spread widest
        rel = P[kopf_idx] - ansatz
        quer = rel - np.outer(rel @ achse, achse)
        _, _, vt = np.linalg.svd(quer - quer.mean(axis=0), full_matrices=False)
        # of the two directions across the axis, the gape is the one with few points near its middle
        aussen = (rel @ achse) > 0.4 * float((spitze - ansatz) @ achse)
        beste = None
        for kandidat in (vt[0], vt[1]):
            kandidat = kandidat - achse * (kandidat @ achse)
            kandidat /= np.linalg.norm(kandidat)
            lage = quer @ kandidat
            lage = lage - 0.5 * (np.percentile(lage[aussen], 5) + np.percentile(lage[aussen], 95)) if aussen.sum() > 20 else lage
            mitte_anteil = float((np.abs(lage[aussen]) < 0.2 * np.abs(lage[aussen]).max()).mean()) if aussen.sum() > 20 else 1.0
            if beste is None or mitte_anteil < beste[0]:
                beste = (mitte_anteil, kandidat, lage)
        mitte_anteil, spalt, seite = beste
        if spalt[2] < 0:
            spalt, seite = -spalt, -seite
        # the two gaping traps of the model (read off the pictures): the tallest and the one leaning back
        offen = n in OFFENE_FALLEN
        gemein.log(NAME, "  Spalt-Anteil nahe der Mitte: %.2f" % mitte_anteil)
        laenge = float(np.linalg.norm(spitze - ansatz))
        weite = float(np.abs(seite).max()) * 0.5
        plan["kiefer_%d_o" % n] = K(tuple(ansatz + achse * laenge * 0.2), tuple(ansatz + achse * laenge * 0.95 + spalt * weite), "falle_%d" % n, gewicht=False)
        plan["kiefer_%d_u" % n] = K(tuple(ansatz + achse * laenge * 0.2), tuple(ansatz + achse * laenge * 0.95 - spalt * weite), "falle_%d" % n, gewicht=False)
        scharnier = np.cross(achse, spalt)
        stiel_info.append(dict(nummer=n, offen=offen, scharnier=[float(x) for x in scharnier], achse=[float(x) for x in achse],
                               kopf=[float(x) for x in kopf_mitte], laenge=float(d["weg"].max())))
        d.update(namen=namen, kopf_idx=kopf_idx, seite=seite, rel_achse=rel @ achse, kopf_laenge=laenge, stellen=stellen, offen=offen)
        gemein.log(NAME, "  Falle %d: %s, Laenge %.2f m, Mitte %s" % (n, "offen" if offen else "geschlossen", laenge, np.round(kopf_mitte, 2)))

    arm = bau.skelett_bauen(NAME, plan)
    namen = [n for n in plan if n != "wurzel"]
    spalte = {n: i for i, n in enumerate(namen)}
    w = np.zeros((len(P), len(namen)))
    w[:, spalte["strunk"]] = 1.0
    for k, d in enumerate(daten):
        n = k + 1
        grenzen, stellen = d["grenzen"], d["stellen"]
        band_weg = [float(grenzen[min(int(round(s)), len(grenzen) - 1)]) for s in stellen]
        for i, x in zip(d["idx"], d["weg"]):
            w[i] = 0.0
            if x >= band_weg[-1]:
                continue
            g = int(np.searchsorted(band_weg, x, side="right")) - 1
            g = min(max(g, 0), GLIEDER - 1)
            teil = (x - band_weg[g]) / max(band_weg[g + 1] - band_weg[g], 1e-6)
            if g == 0 and teil < 0.5:
                # the foot of the stalk blends into the unmoving base
                w[i, spalte["strunk"]] = 1 - teil * 2
                w[i, spalte[d["namen"][0]]] = teil * 2
            elif teil < 0.3 and g > 0:
                w[i, spalte[d["namen"][g - 1]]] = 0.5 - teil / 0.6
                w[i, spalte[d["namen"][g]]] = 0.5 + teil / 0.6
            else:
                w[i, spalte[d["namen"][g]]] = 1.0
        for i, s, a in zip(d["kopf_idx"], d["seite"], d["rel_achse"]):
            w[i] = 0.0
            anteil = min(max((a / d["kopf_laenge"] - 0.15) / 0.2, 0.0), 1.0) if d["offen"] else 0.0
            w[i, spalte["falle_%d" % n]] = 1 - anteil
            w[i, spalte["kiefer_%d_%s" % (n, "o" if s > 0 else "u")]] = anteil
    w = bau.nachbarn_mitteln(obj, w, 2)
    bau.gewichte_schreiben(obj, arm, namen, w)
    with open(gemein.ordner(NAME, "arbeit", "stiele.json"), "w", encoding="utf-8") as f:
        json.dump(stiel_info, f, indent=1)
    gemein.speichern(NAME, "02_skelett.blend")

    # Weight test: every stalk bent over, open traps gaping
    from mathutils import Matrix, Vector
    p = bau.Pose()
    for info in stiel_info:
        n = info["nummer"]
        for g in range(GLIEDER):
            p.dazu("stiel_%d_%d" % (n, g + 1), beuge=9, seite=5 * (1 if n % 2 else -1))
        if info["offen"]:
            p.winkel["kiefer_%d_o" % n] = Matrix.Rotation(math.radians(25), 3, Vector(info["scharnier"]))
            p.winkel["kiefer_%d_u" % n] = Matrix.Rotation(math.radians(-25), 3, Vector(info["scharnier"]))
    bau.pose_bild(NAME, arm, "02_test", p, MITTE, 3.2, ("vorne", "seite", "spiel"))
    bau.ruhe(arm)
    gemein.ansichten(NAME, "02_ruhe", MITTE, 3.2, ("seite",), 500)


main()
