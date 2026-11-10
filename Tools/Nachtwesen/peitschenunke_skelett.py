"""Peitschenunke, step 2: the skeleton and the weights.

  wurzel, becken, bauch, brust, kopf, kiefer (lower jaw), kehle (throat sac)
  zunge                                          no weights: where the tongue the engine builds comes out
  oberarm/unterarm/hand _l/_r
  oberschenkel/unterschenkel/fuss/zehen _l/_r    frog legs: thigh forward, shin back up, the long foot down to the toes

    blender -b --factory-startup -P Tools/Nachtwesen/peitschenunke_skelett.py
"""
import os
import sys

import numpy as np

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bau  # noqa: E402
import gemein  # noqa: E402
from bau import K  # noqa: E402

NAME = "Peitschenunke"
MITTE = (0.0, 0.0, 0.53)


def vorderbein(s):
    return lambda P: (P[:, 0] > 0.0) & (((P[:, 1] * s > 0.24) & (P[:, 2] < 0.62)) | ((P[:, 0] > 0.1) & (P[:, 2] < 0.36) & (P[:, 1] * s > 0.08)))


def hinterbein(s):
    return lambda P: (P[:, 0] <= 0.0) & (((P[:, 1] * s > 0.25) & (P[:, 2] < 0.74)) | ((P[:, 2] < 0.36) & (P[:, 1] * s > 0.05)))


def bein(P):
    m = np.zeros(len(P), dtype=bool)
    for s in (1, -1):
        m |= vorderbein(s)(P) | hinterbein(s)(P)
    return m


def kiefer(P):
    return (P[:, 0] > 0.44) & (P[:, 2] < 0.775) & (P[:, 2] > 0.6) & ~bein(P)


def kehle(P):
    return (P[:, 0] > 0.15) & (P[:, 0] < 0.66) & (P[:, 2] < 0.64) & (np.abs(P[:, 1]) < 0.22) & ~bein(P)


def kopf(P):
    return (P[:, 0] > 0.36) & (P[:, 2] >= 0.775) & ~bein(P)


def rumpf(P):
    return ~bein(P) & ~((P[:, 0] > 0.5) & (P[:, 2] < 0.775))


def plan_bauen():
    plan = {"wurzel": K((0, 0, 0), (0, 0, 0.3), gewicht=False)}
    plan["becken"] = K((-0.5, 0, 0.7), (-0.15, 0, 0.8), "wurzel", maske=rumpf)
    plan["bauch"] = K((-0.15, 0, 0.8), (0.2, 0, 0.88), "becken", maske=rumpf)
    plan["brust"] = K((0.2, 0, 0.88), (0.45, 0, 0.9), "bauch", maske=rumpf)
    plan["kopf"] = K((0.45, 0, 0.9), (0.8, 0, 0.85), "brust", maske=kopf)
    plan["kiefer"] = K((0.45, 0, 0.74), (0.8, 0, 0.72), "kopf", maske=kiefer)
    plan["kehle"] = K((0.5, 0, 0.68), (0.42, 0, 0.5), "brust", maske=kehle)
    plan["zunge"] = K((0.72, 0, 0.77), (0.85, 0, 0.77), "kopf", gewicht=False)
    for s, n in ((1, "l"), (-1, "r")):
        plan["oberarm_" + n] = K((0.25, 0.22 * s, 0.6), (0.33, 0.4 * s, 0.42), "brust", maske=vorderbein(s))
        plan["unterarm_" + n] = K((0.33, 0.4 * s, 0.42), (0.6, 0.42 * s, 0.1), "oberarm_" + n, maske=vorderbein(s))
        plan["hand_" + n] = K((0.6, 0.42 * s, 0.1), (0.75, 0.45 * s, 0.02), "unterarm_" + n, maske=vorderbein(s))
        plan["oberschenkel_" + n] = K((-0.42, 0.2 * s, 0.62), (-0.15, 0.45 * s, 0.5), "becken", maske=hinterbein(s))
        plan["unterschenkel_" + n] = K((-0.15, 0.45 * s, 0.5), (-0.8, 0.4 * s, 0.62), "oberschenkel_" + n, maske=hinterbein(s))
        plan["fuss_" + n] = K((-0.8, 0.4 * s, 0.62), (-0.45, 0.6 * s, 0.08), "unterschenkel_" + n, maske=hinterbein(s))
        plan["zehen_" + n] = K((-0.45, 0.6 * s, 0.08), (-0.3, 0.8 * s, 0.02), "fuss_" + n, maske=hinterbein(s))
    return plan


def main():
    obj, _ = gemein.oeffnen(NAME, "01_vorbereitet.blend")
    plan = plan_bauen()
    arm = bau.skelett_bauen(NAME, plan)
    namen, w = bau.gewichte_rechnen(obj, plan, glatt=4)
    gemein.log(NAME, "ohne Gewicht:", int((w.sum(axis=1) < 0.5).sum()))
    bau.gewichte_schreiben(obj, arm, namen, w)
    bau.knochen_json(NAME, plan)
    gemein.speichern(NAME, "02_skelett.blend")

    # Weight test: jaw open, head up, body lifted off the planted hands and feet, a hind leg stretched
    p = bau.Pose()
    p.dazu("kiefer", beuge=32)
    p.dazu("kopf", beuge=-16)
    p.schieben("becken", z=0.2, x=0.1)
    for s, n in ((1, "l"), (-1, "r")):
        p.greifen("oberarm_" + n, "unterarm_" + n, (0.6, 0.42 * s, 0.1), (-0.3, 1.0 * s, 0.6), "hand_" + n)
    p.greifen("unterschenkel_l", "fuss_l", (-0.45, 0.6, 0.08), (-1.0, 0.0, 0.8), "zehen_l")
    p.dazu("oberschenkel_r", wende=-60)
    p.dazu("unterschenkel_r", wende=100)
    p.dazu("fuss_r", beuge=-60)
    bau.pose_bild(NAME, arm, "02_test", p, MITTE, 2.4, ("vorne", "hinten", "seite", "spiel"))
    bau.ruhe(arm)


main()
