"""Kobolde (Schleimkratzer, Hortwaechter, Kontoraeltester), step 2: a skeleton with the bone names of the Unreal
mannequin (the same names as the Schleimgaenger, so the same movements and code fit every humanlike host) and
distance weights.

The joints are read off the measuring pictures of step 1 (01_vorbereiten.py: front and side, orthographic; the body
looks along +X, +Y is its left, metres). Left side given, the right is mirrored. Arms, legs and hands only take the
points on their own side; arms only those standing out from the trunk, legs only those below the hip.

    blender -b --factory-startup -P Tools/Nachtwesen/kobold_skelett.py -- <Name>
"""
import os
import sys

import numpy as np
from mathutils import Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bau  # noqa: E402
import gemein  # noqa: E402
from bau import K, Pose  # noqa: E402

# Joints per Kobold (x forward, y left, z up); read 2026-11-21 off vorschau/01_vorne.png and 01_seite.png
GELENKE = {
    "Schleimkratzer": dict(
        becken=(-0.07, 0.0, 0.44), hals=(-0.01, 0.0, 0.84), kopf=(0.04, 0.0, 0.9), scheitel=(0.06, 0.0, 1.05),
        schulter=(-0.05, 0.15, 0.79), ellbogen=(-0.03, 0.21, 0.6), handgelenk=(0.035, 0.25, 0.46), finger=(0.05, 0.27, 0.29),
        huefte=(-0.07, 0.075, 0.43), knie=(-0.02, 0.105, 0.27), knoechel=(-0.055, 0.11, 0.06), zehen=(0.08, 0.11, 0.015),
        rumpfbreite=0.12),
    "Hortwaechter": dict(
        becken=(-0.056, 0.0, 0.56), hals=(0.056, 0.0, 1.03), kopf=(0.09, 0.0, 1.09), scheitel=(0.11, 0.0, 1.33),
        schulter=(-0.037, 0.297, 0.944), ellbogen=(-0.09, 0.418, 0.712), handgelenk=(0.037, 0.43, 0.545), finger=(0.06, 0.44, 0.38),
        huefte=(-0.056, 0.11, 0.545), knie=(-0.09, 0.186, 0.378), knoechel=(-0.056, 0.2, 0.14), zehen=(0.15, 0.2, 0.03),
        rumpfbreite=0.26),
    "Kontoraeltester": dict(
        becken=(0.0, 0.0, 0.48), hals=(0.047, 0.0, 0.796), kopf=(0.07, 0.0, 0.86), scheitel=(0.095, 0.0, 1.15),
        schulter=(0.016, 0.174, 0.733), ellbogen=(0.063, 0.26, 0.543), handgelenk=(0.14, 0.3, 0.464), finger=(0.17, 0.32, 0.346),
        huefte=(0.0, 0.08, 0.47), knie=(0.03, 0.1, 0.3), knoechel=(0.06, 0.11, 0.06), zehen=(0.16, 0.11, 0.01),
        rumpfbreite=0.15),
    # Second round, read 2026-11-21 the same way
    "Sklaventreiber": dict(
        becken=(-0.03, 0.0, 0.77), hals=(0.045, 0.0, 1.25), kopf=(0.07, 0.0, 1.34), scheitel=(0.07, 0.0, 1.61),
        schulter=(-0.045, 0.205, 1.22), ellbogen=(-0.01, 0.3, 0.97), handgelenk=(0.025, 0.37, 0.78), finger=(0.05, 0.385, 0.63),
        huefte=(-0.035, 0.095, 0.755), knie=(-0.01, 0.13, 0.42), knoechel=(-0.035, 0.19, 0.115), zehen=(0.145, 0.19, 0.02),
        rumpfbreite=0.165),
    "Sprengmeister": dict(
        becken=(-0.045, 0.0, 0.55), hals=(0.045, 0.0, 1.13), kopf=(0.07, 0.0, 1.21), scheitel=(0.06, 0.0, 1.58),
        schulter=(-0.065, 0.295, 1.02), ellbogen=(-0.045, 0.415, 0.82), handgelenk=(0.045, 0.46, 0.645), finger=(0.09, 0.48, 0.49),
        huefte=(-0.045, 0.11, 0.535), knie=(-0.02, 0.155, 0.27), knoechel=(-0.02, 0.22, 0.095), zehen=(0.175, 0.22, 0.02),
        rumpfbreite=0.24),
    # Hunched, the big head far forward; the pole with the lantern goes on a bone of its own on the upper back
    "Laternentraeger": dict(
        becken=(-0.06, 0.0, 0.39), hals=(0.06, 0.0, 0.605), kopf=(0.12, 0.0, 0.66), scheitel=(0.12, 0.0, 0.86),
        schulter=(0.0, 0.14, 0.57), ellbogen=(0.025, 0.2, 0.42), handgelenk=(0.06, 0.225, 0.285), finger=(0.085, 0.23, 0.19),
        huefte=(-0.06, 0.062, 0.375), knie=(0.015, 0.1, 0.237), knoechel=(-0.025, 0.108, 0.098), zehen=(0.075, 0.108, 0.01),
        rumpfbreite=0.11,
        extra={"laterne": dict(k=(-0.05, 0.0, 0.95), e=(0.22, 0.21, 0.7), eltern="spine_03", radius=50.0,
                               maske=lambda P: (P[:, 2] > 0.885) | ((P[:, 0] > 0.15) & (np.abs(P[:, 1]) > 0.15) & (P[:, 2] > 0.58))
                               | ((P[:, 0] < -0.15) & (P[:, 2] > 0.8))),
               # Where the game hangs the light: the middle of the lantern itself, an unweighted bone below the pole
               "laterne_licht": dict(eltern="laterne", mitte_von=lambda P: (P[:, 0] > 0.15) & (np.abs(P[:, 1]) > 0.15)
                                     & (P[:, 2] > 0.6) & (P[:, 2] < 0.76))}),
}


def V(t, spiegeln=False):
    return Vector((t[0], -t[1] if spiegeln else t[1], t[2]))


def plan_bauen(g, P):
    becken, hals, kopf, scheitel = V(g["becken"]), V(g["hals"]), V(g["kopf"]), V(g["scheitel"])
    rumpf = [becken.lerp(hals, f) for f in (0.0, 0.22, 0.47, 0.72, 1.0)]
    plan = {"root": K((0, 0, 0), (-0.1, 0, 0), gewicht=False),
            "pelvis": K(rumpf[0], rumpf[1], "root"),
            "spine_01": K(rumpf[1], rumpf[2], "pelvis"),
            "spine_02": K(rumpf[2], rumpf[3], "spine_01"),
            "spine_03": K(rumpf[3], rumpf[4], "spine_02"),
            "neck_01": K(hals, kopf, "spine_03"),
            "head": K(kopf, scheitel, "neck_01", maske=lambda P, z=hals.z: P[:, 2] > z - 0.02)}
    breit = g["rumpfbreite"]
    for seite, s in (("l", 1.0), ("r", -1.0)):
        sp = s < 0
        schulter, ellbogen, handgelenk, finger = (V(g[k], sp) for k in ("schulter", "ellbogen", "handgelenk", "finger"))
        huefte, knie, knoechel, zehen = (V(g[k], sp) for k in ("huefte", "knie", "knoechel", "zehen"))
        ballen = knoechel.lerp(zehen, 0.6)
        ballen.z = zehen.z + 0.01
        hier = (lambda P, s=s: P[:, 1] * s > 0.0)
        arm = (lambda P, s=s, b=breit: P[:, 1] * s > b)
        bein = (lambda P, s=s, z=huefte.z + 0.04: (P[:, 1] * s > -0.01) & (P[:, 2] < z))
        fuss = (lambda P, s=s, z=knoechel.z + 0.05: (P[:, 1] * s > 0.0) & (P[:, 2] < z))
        plan["clavicle_" + seite] = K(Vector((hals.x - 0.02, 0.03 * s, schulter.z + 0.02)), schulter, "spine_03",
                                     maske=hier, radius=1.3)
        plan["upperarm_" + seite] = K(schulter, ellbogen, "clavicle_" + seite, maske=arm)
        plan["lowerarm_" + seite] = K(ellbogen, handgelenk, "upperarm_" + seite, maske=arm)
        plan["hand_" + seite] = K(handgelenk, finger, "lowerarm_" + seite, maske=arm)
        plan["thigh_" + seite] = K(huefte, knie, "pelvis", maske=bein)
        plan["calf_" + seite] = K(knie, knoechel, "thigh_" + seite, maske=bein)
        plan["foot_" + seite] = K(knoechel, ballen, "calf_" + seite, maske=fuss)
        plan["ball_" + seite] = K(ballen, zehen, "foot_" + seite, maske=fuss)
    # Rigid things carried on the body (the lantern pole): a bone of their own that no clip moves
    # A bone placed at the middle of some points (mitte_von) carries no weights: it only marks a place for the game
    for name, b in g.get("extra", {}).items():
        if "mitte_von" in b:
            mitte = Vector(P[b["mitte_von"](P)].mean(axis=0))
            plan[name] = K(mitte, mitte + Vector((0.0, 0.0, 0.04)), b["eltern"], gewicht=False)
            gemein.log(name, "an %s" % (tuple(round(c, 3) for c in mitte),))
        else:
            plan[name] = K(V(b["k"]), V(b["e"]), b["eltern"], maske=b["maske"], radius=b["radius"])
    return plan


def hand_teilen(plan):
    """K_ rigs (2026-11-23): the hand is split into the palm (hand_) and the fingers (middle_01_), so the fingers can close
    round a haft or curl when beckoning; the clips' index/ring/pinky keys are left out on these bodies"""
    for seite in ("l", "r"):
        h = plan["hand_" + seite]
        mitte = h["k"].lerp(h["e"], 0.45)
        plan["middle_01_" + seite] = K(mitte, h["e"], "hand_" + seite, maske=h["maske"], radius=0.9)
        plan["hand_" + seite] = K(h["k"], mitte, h["eltern"], maske=h["maske"], radius=1.1)


def ausruestung_binden(obj, plan, namen, w):
    """Gear points (kobold_ausruestung.py) follow exactly the bones they were bound to"""
    import json
    bindung = json.loads(obj.get("bindung", "[]"))
    for i, knochen in bindung:
        w[i, :] = 0.0
        for kn, x in knochen:
            if kn in namen:
                w[i, namen.index(kn)] += x
    return len(bindung)


def main():
    name = sys.argv[sys.argv.index("--") + 1]
    if name not in GELENKE:
        # a variant (kobold_variante.py): the joints of its base, bent like its body
        import kobold_variante
        GELENKE[name] = kobold_variante.gelenke(name, GELENKE)
    obj, _ = gemein.oeffnen(name, "01_vorbereitet.blend")
    plan = plan_bauen(GELENKE[name], gemein.punkte(obj))
    if name.startswith("K_"):
        import json
        hand_teilen(plan)
        for kn, k, e, eltern in json.loads(obj.get("extra_knochen", "[]")):
            plan[kn] = K(k, e, eltern, maske=lambda P: np.zeros(len(P), dtype=bool))
    arm = bau.skelett_bauen(name, plan)
    namen, w = bau.gewichte_rechnen(obj, plan, schaerfe=0.25, glatt=4)
    if name.startswith("K_"):
        gemein.log(name, "Ausruestung gebunden: %d Punkte" % ausruestung_binden(obj, plan, namen, w))
    bau.gewichte_schreiben(obj, arm, namen, w)
    bau.knochen_json(name, plan)
    gemein.speichern(name, "02_skelett.blend")
    gemein.log(name, "Skelett: %d Knochen, %d gewichtet" % (len(plan), len(namen)))

    # Check pose: arms raised forward, right knee bent, head turned
    g = GELENKE[name]
    p = Pose()
    p.dazu("head", wende=25)
    p.dazu("spine_02", beuge=12)
    hoehe = g["scheitel"][2]
    for seite, s in (("l", 1), ("r", -1)):
        sch = V(g["schulter"], s < 0)
        p.greifen("upperarm_" + seite, "lowerarm_" + seite, sch + Vector((0.35, 0.05 * s, 0.1)) * hoehe, Vector((-1, 0.3 * s, -0.3)),
                  "hand_" + seite)
    kn = V(g["knoechel"], True)
    p.greifen("thigh_r", "calf_r", kn + Vector((0.15, 0, 0.18)) * hoehe, Vector((1, 0, 0)), "foot_r")
    p.greifen("thigh_l", "calf_l", V(g["knoechel"]), Vector((1, 0, 0)), "foot_l")
    mitte = (0.0, 0.0, hoehe / 2)
    bau.pose_bild(name, arm, "02_probe", p, mitte, hoehe * 1.2, ("vorne", "seite", "spiel"), 400)
    bau.ruhe(arm)


if __name__ == "__main__":
    main()
