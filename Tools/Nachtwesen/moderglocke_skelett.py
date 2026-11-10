"""Moderglocke, step 2: stinging filaments added at the cap's rim, the skeleton and the weights.

  wurzel, becken, bauch, brust, hals, hut            the stalk body and the cap's middle
  hutrand_1..8                                       the cap's rim in eight segments: they fold down and lift (the bell pulses)
  faden_<k>_1..4 (k = 1..12)                         the filaments hanging from the rim (own geometry, not fused to the body)
  oberarm/unterarm/hand _l/_r, oberschenkel/unterschenkel/fuss _l/_r

    blender -b --factory-startup -P Tools/Nachtwesen/moderglocke_skelett.py
"""
import math
import os
import sys

import numpy as np

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bau  # noqa: E402
import gemein  # noqa: E402
from bau import K  # noqa: E402

NAME = "Moderglocke"
RAND = 8
FAEDEN = 12
GLIEDER = 4
MITTE = (0.0, 0.0, 1.35)


def hut_maske(P):
    r = np.hypot(P[:, 0], P[:, 1])
    return ((P[:, 2] > 2.28) & ~((r < 0.13) & (P[:, 2] < 2.42))) | ((r > 0.4) & (P[:, 2] > 1.82))


def arm_maske(s):
    def m(P):
        y = P[:, 1] * s
        return ~hut_maske(P) & (P[:, 2] < 2.02) & (P[:, 2] > 0.7) & (y > np.where(P[:, 2] > 1.8, 0.16, 0.29))
    return m


def bein_maske(s):
    return lambda P: (P[:, 2] < 1.46) & (P[:, 1] * s > 0.0) & ((np.abs(P[:, 1]) < 0.4) | (P[:, 2] < 0.7))


def rumpf_maske(P):
    return ~hut_maske(P) & (P[:, 2] > 1.25) & ~arm_maske(1)(P) & ~arm_maske(-1)(P)


def plan_bauen(faeden):
    plan = {"wurzel": K((0, 0, 0), (0, 0, 0.3), gewicht=False)}
    plan["becken"] = K((0.0, 0, 1.42), (0.0, 0, 1.62), "wurzel", maske=rumpf_maske)
    plan["bauch"] = K((0.0, 0, 1.62), (0.01, 0, 1.88), "becken", maske=rumpf_maske)
    plan["brust"] = K((0.01, 0, 1.88), (0.02, 0, 2.10), "bauch", maske=rumpf_maske)
    plan["hals"] = K((0.02, 0, 2.10), (0.02, 0, 2.40), "brust", maske=rumpf_maske)
    plan["hut"] = K((0.02, 0, 2.40), (0.02, 0, 2.70), "hals", gewicht=False)
    for k in range(RAND):
        a = 2 * math.pi * k / RAND
        c, s = math.cos(a), math.sin(a)
        plan["hutrand_%d" % (k + 1)] = K((0.28 * c, 0.28 * s, 2.56), (0.66 * c, 0.66 * s, 2.40), "hut", gewicht=False)
    for k, bahn in enumerate(faeden):
        a = math.atan2(bahn[0][1], bahn[0][0]) % (2 * math.pi)
        rand = int(round(a / (2 * math.pi / RAND))) % RAND + 1
        bau.kette(plan, "faden_%d" % (k + 1), bahn, "hutrand_%d" % rand, gewicht=False)
    for s, n in ((1, "l"), (-1, "r")):
        plan["oberarm_" + n] = K((0.02, 0.2 * s, 2.05), (0.02, 0.38 * s, 1.6), "brust", maske=arm_maske(s))
        plan["unterarm_" + n] = K((0.02, 0.38 * s, 1.6), (0.03, 0.53 * s, 1.2), "oberarm_" + n, maske=arm_maske(s))
        plan["hand_" + n] = K((0.03, 0.53 * s, 1.2), (0.03, 0.6 * s, 0.93), "unterarm_" + n, maske=arm_maske(s))
        plan["oberschenkel_" + n] = K((0.0, 0.1 * s, 1.42), (0.0, 0.2 * s, 0.9), "becken", maske=bein_maske(s))
        plan["unterschenkel_" + n] = K((0.0, 0.2 * s, 0.9), (0.0, 0.17 * s, 0.13), "oberschenkel_" + n, maske=bein_maske(s))
        plan["fuss_" + n] = K((0.0, 0.17 * s, 0.13), (0.22, 0.17 * s, 0.02), "unterschenkel_" + n, maske=bein_maske(s))
    return plan


def main():
    obj, _ = gemein.oeffnen(NAME, "01_vorbereitet.blend")
    P = gemein.punkte(obj)
    gemein.log(NAME, "Inseln:", len(set(bau.inseln(obj))))

    # Filaments: from just inside the rim's lower edge, uneven lengths, a small sting bead at the end
    uv = bau.uv_bei(obj, (0.1, 0.0, 1.75))
    r_p = np.hypot(P[:, 0], P[:, 1])
    w_p = np.arctan2(P[:, 1], P[:, 0])
    faeden, faden_punkte = [], []
    for k in range(FAEDEN):
        a = 2 * math.pi * (k + 0.5 + 0.25 * math.sin(k * 2.4)) / FAEDEN
        nah = (np.abs(np.angle(np.exp(1j * (w_p - a)))) < 0.2) & (r_p > 0.5) & (P[:, 2] > 2.2)
        rand_r = float(r_p[nah].max()) - 0.07 if nah.any() else 0.6
        ring = nah & (np.abs(r_p - rand_r) < 0.05)
        rand_z = float(P[ring, 2].min()) + 0.03 if ring.any() else 2.38
        laenge = 0.95 + 0.45 * (0.5 + 0.5 * math.sin(k * 1.7 + 0.6))
        gelenke = [(rand_r * math.cos(a), rand_r * math.sin(a), rand_z - laenge * i / GLIEDER) for i in range(GLIEDER + 1)]
        faeden.append(gelenke)
        ringe = GLIEDER * 3
        bahn = [(gelenke[0][0], gelenke[0][1], rand_z - laenge * i / ringe) for i in range(ringe + 1)]
        radien = [0.024 - 0.012 * i / ringe for i in range(ringe + 1)]
        radien[-2], radien[-1] = 0.034, 0.02
        faden_punkte.append(bau.schlauch_anfuegen(obj, bahn, radien, uv))
    P = gemein.punkte(obj)

    plan = plan_bauen(faeden)
    arm = bau.skelett_bauen(NAME, plan)
    namen, w = bau.gewichte_rechnen(obj, plan)
    spalte = {n: i for i, n in enumerate(namen)}
    extra = ["hut"] + ["hutrand_%d" % (k + 1) for k in range(RAND)] + \
        ["faden_%d_%d" % (k + 1, g + 1) for k in range(FAEDEN) for g in range(GLIEDER)]
    for n in extra:
        spalte[n] = len(namen)
        namen.append(n)
    w = np.hstack([w, np.zeros((len(P), len(extra)))])

    # The cap: its middle to "hut", the rim shared between the two nearest rim segments
    hut = hut_maske(P)
    hut[[i for fp in faden_punkte for i in fp]] = False
    r = np.hypot(P[:, 0], P[:, 1])
    anteil = np.clip((r - 0.2) / 0.3, 0, 1)
    anteil = anteil * anteil * (3 - 2 * anteil)
    lage = (np.arctan2(P[:, 1], P[:, 0]) % (2 * math.pi)) / (2 * math.pi / RAND)
    k0 = np.floor(lage).astype(int) % RAND
    k1 = (k0 + 1) % RAND
    f = lage - np.floor(lage)
    w[hut] = 0.0
    idx = np.where(hut)[0]
    w[idx, spalte["hut"]] = 1 - anteil[idx]
    for i in idx:
        w[i, spalte["hutrand_%d" % (k0[i] + 1)]] += anteil[i] * (1 - f[i])
        w[i, spalte["hutrand_%d" % (k1[i] + 1)]] += anteil[i] * f[i]
    # The filaments: each ring of points to its own joint, blending along the chain
    for k, fp in enumerate(faden_punkte):
        z0, z1 = faeden[k][0][2], faeden[k][-1][2]
        for i in fp:
            x = (z0 - P[i, 2]) / (z0 - z1) * GLIEDER
            g = min(GLIEDER - 1, max(0, int(x)))
            w[i] = 0.0
            teil = x - g
            if teil < 0.25 and g > 0:
                w[i, spalte["faden_%d_%d" % (k + 1, g)]] = 0.5 - teil * 2
                w[i, spalte["faden_%d_%d" % (k + 1, g + 1)]] = 0.5 + teil * 2
            else:
                w[i, spalte["faden_%d_%d" % (k + 1, g + 1)]] = 1.0
    bau.gewichte_schreiben(obj, arm, namen, w)
    bau.knochen_json(NAME, plan)
    gemein.speichern(NAME, "02_skelett.blend")

    # Weight test: bell folded down, filaments swung, an arm lifted, a knee drawn up
    p = bau.Pose()
    for k in range(RAND):
        a = 2 * math.pi * k / RAND
        p.winkel["hutrand_%d" % (k + 1)] = (30 * math.cos(a), -30 * math.sin(a), 0)
    for k in range(FAEDEN):
        for g in range(GLIEDER):
            p.dazu("faden_%d_%d" % (k + 1, g + 1), 12)
    p.dazu("oberarm_l", seite=-60)
    p.dazu("unterarm_l", seite=-30)
    p.dazu("oberarm_r", beuge=-50)
    p.dazu("oberschenkel_r", beuge=-70)
    p.dazu("unterschenkel_r", beuge=80)
    p.dazu("hals", beuge=15)
    p.dazu("bauch", beuge=10)
    bau.pose_bild(NAME, arm, "02_test", p, MITTE, 3.0)
    bau.ruhe(arm)
    gemein.ansichten(NAME, "02_ruhe", MITTE, 3.0, ("vorne", "spiel"), 500)


main()
