"""Kahlschwinge, step 2: the skeleton and the weights (the model stands in a T-pose).

  wurzel, becken, bauch, brust, hals, kopf, ohr_l/r
  oberarm/unterarm/hand _l/_r                        the arms carry the blade feathers
  schwinge_innen/aussen _l/_r                        the feather fans under upper arm and forearm (they trail and fold)
  rueckenfluegel_l/r                                 the small upper wings on the back
  oberschenkel/unterschenkel/fuss _l/_r, schwanz_1..4

    blender -b --factory-startup -P Tools/Nachtwesen/kahlschwinge_skelett.py
"""
import os
import sys

import numpy as np

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bau  # noqa: E402
import gemein  # noqa: E402
from bau import K  # noqa: E402

NAME = "Kahlschwinge"
MITTE = (0.0, 0.0, 0.95)


def feder(s):
    return lambda P: (P[:, 1] * s > 0.19) & (P[:, 2] < 1.37) & (P[:, 2] > 0.8)


def arm(s):
    return lambda P: (P[:, 1] * s > 0.17) & (P[:, 2] >= 1.345) & (P[:, 0] > 0.08) & (P[:, 2] < 1.6)


def rueckenfluegel(s):
    return lambda P: (P[:, 2] > 1.36) & (P[:, 0] < 0.07) & (P[:, 1] * s > 0.04) & ((np.abs(P[:, 1]) > 0.11) | (P[:, 0] < 0.0))


def irgendein_fluegel(P):
    m = np.zeros(len(P), dtype=bool)
    for s in (1, -1):
        m |= feder(s)(P) | arm(s)(P) | rueckenfluegel(s)(P)
    return m


def schwanz(P):
    return (P[:, 0] < 0.03) & (P[:, 2] < 0.95) & ~((P[:, 2] > 0.8) & (np.abs(P[:, 1]) < 0.12) & (P[:, 0] > -0.04))


def bein(s):
    return lambda P: (P[:, 2] < 0.97) & (P[:, 1] * s > 0.0) & (np.abs(P[:, 1]) < 0.3) & ~schwanz(P)


def rumpf(P):
    return (P[:, 2] > 0.78) & (P[:, 2] < 1.6) & ~irgendein_fluegel(P) & ~schwanz(P)


def kopf(P):
    return (P[:, 2] > 1.47) & (P[:, 0] > 0.07) & (np.abs(P[:, 1]) < 0.3)


def ohr(s):
    return lambda P: (P[:, 2] > 1.67) & (P[:, 0] > 0.05) & (P[:, 1] * s > 0.03)


def plan_bauen():
    plan = {"wurzel": K((0, 0, 0), (0, 0, 0.3), gewicht=False)}
    plan["becken"] = K((0.08, 0, 0.95), (0.1, 0, 1.1), "wurzel", maske=rumpf)
    plan["bauch"] = K((0.1, 0, 1.1), (0.15, 0, 1.25), "becken", maske=rumpf)
    plan["brust"] = K((0.15, 0, 1.25), (0.19, 0, 1.42), "bauch", maske=rumpf)
    plan["hals"] = K((0.19, 0, 1.42), (0.21, 0, 1.52), "brust", maske=lambda P: rumpf(P) | kopf(P))
    plan["kopf"] = K((0.21, 0, 1.52), (0.23, 0, 1.68), "hals", maske=kopf, radius=1.5)
    for s, n in ((1, "l"), (-1, "r")):
        plan["ohr_" + n] = K((0.2, 0.08 * s, 1.66), (0.15, 0.17 * s, 1.9), "kopf", maske=ohr(s))
        plan["oberarm_" + n] = K((0.18, 0.17 * s, 1.38), (0.18, 0.55 * s, 1.40), "brust", maske=arm(s))
        plan["unterarm_" + n] = K((0.18, 0.55 * s, 1.40), (0.18, 0.9 * s, 1.42), "oberarm_" + n, maske=arm(s))
        plan["hand_" + n] = K((0.18, 0.9 * s, 1.42), (0.18, 1.07 * s, 1.43), "unterarm_" + n, maske=arm(s))
        plan["schwinge_innen_" + n] = K((0.1, 0.38 * s, 1.33), (-0.2, 0.55 * s, 0.98), "oberarm_" + n, maske=feder(s))
        plan["schwinge_aussen_" + n] = K((0.1, 0.75 * s, 1.35), (-0.3, 1.15 * s, 1.1), "unterarm_" + n, maske=feder(s))
        plan["rueckenfluegel_" + n] = K((0.0, 0.06 * s, 1.42), (-0.12, 0.5 * s, 1.65), "brust", maske=rueckenfluegel(s))
        plan["oberschenkel_" + n] = K((0.1, 0.1 * s, 0.95), (0.25, 0.12 * s, 0.55), "becken", maske=bein(s))
        plan["unterschenkel_" + n] = K((0.25, 0.12 * s, 0.55), (0.14, 0.12 * s, 0.17), "oberschenkel_" + n, maske=bein(s))
        plan["fuss_" + n] = K((0.14, 0.12 * s, 0.17), (0.33, 0.12 * s, 0.02), "unterschenkel_" + n, maske=bein(s))
    bau.kette(plan, "schwanz", [(0.03, 0, 0.9), (-0.12, -0.12, 0.7), (-0.25, -0.3, 0.52), (-0.35, -0.48, 0.42), (-0.42, -0.64, 0.3)],
              "becken", maske=schwanz)
    return plan


def main():
    obj, _ = gemein.oeffnen(NAME, "01_vorbereitet.blend")
    gemein.log(NAME, "Inseln:", sorted(np.unique(bau.inseln(obj), return_counts=True)[1])[-4:])
    plan = plan_bauen()
    arm_ = bau.skelett_bauen(NAME, plan)
    namen, w = bau.gewichte_rechnen(obj, plan)
    gemein.log(NAME, "ohne Gewicht:", int((w.sum(axis=1) < 0.5).sum()))
    bau.gewichte_schreiben(obj, arm_, namen, w)
    bau.knochen_json(NAME, plan)
    gemein.speichern(NAME, "02_skelett.blend")

    # Weight test: a wing beat down and one up, feathers folded, legs tucked, tail swung, head turned
    p = bau.Pose()
    p.dazu("oberarm_l", seite=35)
    p.dazu("unterarm_l", seite=20)
    p.dazu("oberarm_r", seite=40)
    p.dazu("unterarm_r", seite=25)
    p.dazu("schwinge_aussen_l", wende=30)
    p.dazu("rueckenfluegel_l", wende=40)
    p.dazu("rueckenfluegel_r", seite=30)
    p.dazu("oberschenkel_l", beuge=-50)
    p.dazu("unterschenkel_l", beuge=70)
    p.dazu("kopf", wende=40)
    p.dazu("ohr_l", seite=30)
    for i in range(4):
        p.dazu("schwanz_%d" % (i + 1), wende=-18, beuge=10)
    bau.pose_bild(NAME, arm_, "02_test", p, MITTE, 2.8, ("vorne", "hinten", "seite", "spiel"))
    bau.ruhe(arm_)


main()
