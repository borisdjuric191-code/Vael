"""Living Kobolde (no Seuche in them): movements of their own on top of the Schleimgaenger set, added to
arbeit/03_animationen.blend (kobold_clips.py must have run). Awake, upright, quick and sly instead of hauled about.

  Wach_Idle   3 s loop    half crouched, weight shifting, looking about, a sly twitch of the head
  Schleichen  1.0 s loop  quick, low, sneaking steps (0.88 m/s at skeleton size)
  Rennen      0.7 s loop  running away, bent forward, arms pumping (2 m/s at skeleton size)
  Kratzen     0.8 s       a quick swipe of the right claw from high right to low left (hit at 0.4 s)
  Zaubern     1.4 s       both hands up and forward, fingers spread, then flung down (spell at 0.8 s)

Treffer and Tod are the ones of the Schleimgaenger set. Like kobold_clips.py the Kobold is scaled up to the size the
poses are written for, baked, and scaled back.

    blender -b --factory-startup -P Tools/Nachtwesen/kobold_lebend_clips.py -- <Name>
Afterwards: 06_export_nachtrag.py -- <Name> Wach_Idle Schleichen Rennen Kratzen Zaubern, then 07_unreal_nachtrag.py anim <Name>
"""
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bau  # noqa: E402
import gemein  # noqa: E402
import kobold_clips as kc  # noqa: E402
import schleimgaenger_clips as sg  # noqa: E402
from bau import Pose, huegel, rampe, unruhe, weich, welle  # noqa: E402

SEITEN = sg.SEITEN
NULL = Vector((0.0, 0.0, 0.0))


def aufrecht(p, t, schleife, buckel=0.3, staerke=1.0):
    """Upright and awake: a light crouch, the head up, small shifts of weight"""
    sg.koerper(p, 0.0, buckel=buckel, kopf_haengt=-0.4)
    p.schieben("pelvis", z=-0.05, y=0.02 * staerke * unruhe(t, schleife, 1.0))
    p.dazu("pelvis", seite=2.0 * staerke * unruhe(t, schleife, 2.0))
    p.dazu("spine_03", wende=4.0 * staerke * unruhe(t, schleife, 3.0))


def wach_idle(t):
    p = Pose()
    s = 3.0
    aufrecht(p, t, s)
    blick = math.sin(2 * math.pi * t / s)
    p.dazu("head", wende=22.0 * blick, seite=6.0 * sg.ruck(t, 1.9, 0.2))
    hand = {n: Vector((0.05 + 0.03 * welle(t, s, 0.3 if n == "l" else 0.8), 0.04 * sv, 0.12)) for n, sv in SEITEN}
    fuss = {"l": Vector((0.06, 0.0, 0.0)), "r": Vector((-0.05, 0.0, 0.0))}
    sg.glieder(p, 0.0, hand=hand, fuss=fuss, buckel=0.3)
    return p


def schritte(p, phase, schritt, hub, schleife_auf):
    fuss = {}
    for n, versatz in (("l", 0.0), ("r", 0.5)):
        q = (phase + versatz) % 1.0
        if q < schleife_auf:
            x = schritt * (0.5 - q / schleife_auf)
            z = 0.0
        else:
            u = (q - schleife_auf) / (1 - schleife_auf)
            x = schritt * (-0.5 + weich(u))
            z = hub * math.sin(math.pi * u)
        fuss[n] = Vector((x, 0.0, z))
    return fuss


def schleichen(t):
    p = Pose()
    phase = (t / 1.0) % 1.0
    sg.koerper(p, 0.0, buckel=0.9, kopf_haengt=-0.5)
    p.schieben("pelvis", z=-0.12 - 0.02 * abs(math.sin(2 * math.pi * phase)), y=0.025 * math.sin(2 * math.pi * phase))
    p.dazu("pelvis", wende=5.0 * math.sin(2 * math.pi * phase))
    p.dazu("spine_03", wende=-6.0 * math.sin(2 * math.pi * phase))
    p.dazu("head", wende=-4.0 * math.sin(2 * math.pi * phase))
    fuss = schritte(p, phase, 0.44, 0.1, 0.55)
    hand = {n: Vector((0.2 + 0.06 * math.sin(2 * math.pi * (phase + (0.5 if n == "l" else 0.0))), 0.02 * s, 0.2)) for n, s in SEITEN}
    sg.glieder(p, 0.0, hand=hand, fuss=fuss, buckel=0.9)
    return p


def rennen(t):
    p = Pose()
    phase = (t / 0.7) % 1.0
    sg.koerper(p, 0.0, buckel=1.4, kopf_haengt=-0.8)
    p.dazu("pelvis", beuge=8.0, wende=7.0 * math.sin(2 * math.pi * phase))
    p.schieben("pelvis", z=-0.06 + 0.05 * abs(math.sin(2 * math.pi * phase)))
    p.dazu("spine_03", wende=-10.0 * math.sin(2 * math.pi * phase))
    fuss = schritte(p, phase, 0.7, 0.2, 0.42)
    hand = {n: Vector((0.1 + 0.22 * math.sin(2 * math.pi * (phase + (0.5 if n == "r" else 0.0))), 0.0, 0.35)) for n, _ in SEITEN}
    sg.glieder(p, 0.0, hand=hand, fuss=fuss, buckel=1.4)
    return p


def kratzen(t):
    p = Pose()
    aus = huegel(t, 0.0, 0.25, 0.3, 0.38)
    hieb = huegel(t, 0.3, 0.42, 0.5, 0.8)
    aufrecht(p, t, 0.8, buckel=0.5 + 0.7 * hieb, staerke=0.3)
    p.dazu("spine_03", wende=-25.0 * aus + 30.0 * hieb)
    p.dazu("head", beuge=8.0 * hieb)
    p.schieben("pelvis", x=0.1 * hieb)
    hand = {"r": Vector((-0.05 * aus + 0.5 * hieb, -0.15 * aus + 0.3 * hieb, 0.75 * aus + 0.1 * hieb)),
            "l": Vector((0.15, 0.0, 0.2))}
    sg.glieder(p, 0.0, hand=hand, fuss={"r": Vector((-0.08, 0.0, 0.0)), "l": Vector((0.15 * hieb, 0.0, 0.0))}, buckel=0.5)
    return p


def zaubern(t):
    p = Pose()
    heben = rampe(t, 0.1, 0.55) * (1 - rampe(t, 0.78, 0.9))
    wurf = huegel(t, 0.75, 0.82, 0.9, 1.3)
    zittern = 0.015 * math.sin(2 * math.pi * t * 9.0) * huegel(t, 0.4, 0.5, 0.7, 0.78)
    aufrecht(p, t, 1.4, buckel=0.1 - 0.3 * heben + 0.6 * wurf, staerke=0.3)
    p.dazu("head", beuge=-12.0 * heben + 10.0 * wurf)
    p.dazu("spine_03", beuge=-8.0 * heben + 12.0 * wurf)
    hand = {n: Vector((0.25 * heben + 0.45 * wurf, (0.12 * heben - 0.05 * wurf) * s, 0.6 * heben - 0.05 * wurf + zittern)) for n, s in SEITEN}
    for n, _ in SEITEN:
        for finger in ("index_01_", "middle_01_", "ring_01_", "pinky_01_"):
            p.dazu(finger + n, beuge=-20.0 * heben)
    sg.glieder(p, 0.0, hand=hand, buckel=0.2)
    return p


def werfen(t):
    """Overhand throw with the right hand: reach back high, then fling forward (release at 0.6 s)"""
    p = Pose()
    aus = huegel(t, 0.05, 0.4, 0.5, 0.6)
    wurf = huegel(t, 0.5, 0.62, 0.75, 1.1)
    aufrecht(p, t, 1.2, buckel=0.3 - 0.2 * aus + 0.6 * wurf, staerke=0.3)
    p.dazu("spine_03", wende=-30.0 * aus + 25.0 * wurf, beuge=-8.0 * aus + 15.0 * wurf)
    p.dazu("head", beuge=-6.0 * aus + 6.0 * wurf)
    p.schieben("pelvis", x=-0.05 * aus + 0.1 * wurf)
    hand = {"r": Vector((-0.25 * aus + 0.55 * wurf, -0.12 * aus + 0.15 * wurf, 0.9 * aus + 0.45 * wurf)),
            "l": Vector((0.3 * aus + 0.05 * wurf, 0.05, 0.35 * aus))}
    fuss = {"l": Vector((0.18, 0.0, 0.0)), "r": Vector((-0.1, 0.0, 0.0))}
    sg.glieder(p, 0.0, hand=hand, fuss=fuss, buckel=0.3)
    return p


def peitschen(t):
    """A whip lash: the right arm high over the shoulder, then cracked forward and down (hit at 0.5 s)"""
    p = Pose()
    aus = huegel(t, 0.0, 0.3, 0.38, 0.45)
    hieb = huegel(t, 0.4, 0.5, 0.6, 0.95)
    aufrecht(p, t, 1.0, buckel=0.3 + 0.6 * hieb, staerke=0.3)
    p.dazu("spine_03", wende=-20.0 * aus + 20.0 * hieb, beuge=-10.0 * aus + 20.0 * hieb)
    p.dazu("head", beuge=8.0 * hieb)
    p.schieben("pelvis", x=0.08 * hieb)
    hand = {"r": Vector((-0.15 * aus + 0.6 * hieb, -0.1 * aus + 0.05 * hieb, 1.0 * aus + 0.25 * hieb)),
            "l": Vector((0.12, 0.02, 0.15))}
    fuss = {"l": Vector((0.15, 0.0, 0.0)), "r": Vector((-0.08, 0.0, 0.0))}
    sg.glieder(p, 0.0, hand=hand, fuss=fuss, buckel=0.3)
    return p


def locken(t):
    """Beckoning: both hands up in front, the long fingers curling again and again (the lure at 0.8 s)"""
    p = Pose()
    heben = rampe(t, 0.1, 0.4) * (1 - rampe(t, 1.3, 1.55))
    wink = math.sin(2 * math.pi * t * 2.5) * heben
    aufrecht(p, t, 1.6, buckel=0.5, staerke=0.4)
    p.dazu("head", seite=12.0 * heben, beuge=-6.0 * heben)
    hand = {n: Vector((0.3 * heben, 0.05 * s, 0.45 * heben + 0.03 * wink)) for n, s in SEITEN}
    for n, _ in SEITEN:
        for finger in ("index_01_", "middle_01_", "ring_01_", "pinky_01_"):
            p.dazu(finger + n, beuge=25.0 * heben + 25.0 * wink)
    sg.glieder(p, 0.0, hand=hand, buckel=0.5)
    return p


def legen(t):
    """Crouching down and setting something on the ground in front with the right hand (laid at 0.8 s)"""
    p = Pose()
    tief = huegel(t, 0.1, 0.55, 0.95, 1.35)
    aufrecht(p, t, 1.4, buckel=0.5 + 1.0 * tief, staerke=0.2)
    p.schieben("pelvis", z=-0.25 * tief, x=-0.05 * tief)
    p.dazu("head", beuge=10.0 * tief)
    hand = {"r": Vector((0.4 * tief, 0.05, -0.55 * tief)), "l": Vector((0.15 * tief, 0.0, -0.1 * tief))}
    fuss = {"l": Vector((0.1, 0.0, 0.0)), "r": Vector((-0.08, 0.0, 0.0))}
    sg.glieder(p, 0.0, hand=hand, fuss=fuss, buckel=0.5 + 1.0 * tief)
    return p


AKTIONEN = {
    "Wach_Idle": (3.0, wach_idle, True),
    "Schleichen": (1.0, schleichen, True),
    "Rennen": (0.7, rennen, True),
    "Kratzen": (0.8, kratzen, False),
    "Zaubern": (1.4, zaubern, False),
    # Second round of Kobolde (Sklaventreiber, Sprengmeister, Laternentraeger)
    "Werfen": (1.2, werfen, False),
    "Peitschen": (1.0, peitschen, False),
    "Locken": (1.6, locken, False),
    "Legen": (1.4, legen, False),
}


def main():
    name = sys.argv[sys.argv.index("--") + 1]
    obj, arm = gemein.oeffnen(name, "03_animationen.blend")
    k = kc.hochskalieren(obj, arm)
    kc.ziele_setzen(arm, name)
    for aktion, (dauer, pose, schleife) in AKTIONEN.items():
        bau.backen(name, arm, aktion, dauer, pose, schleife)
    bau.ruhe(arm)
    kc.skalieren(obj, arm, 1.0 / k)
    bpy.context.view_layer.update()
    gemein.speichern(name, "03_animationen.blend")
    hoehe = max(v.co.z for v in obj.data.vertices)
    for aktion, (dauer, _, _) in AKTIONEN.items():
        n = int(dauer * gemein.FPS)
        bau.clip_pruefen(name, arm, aktion, (n // 4, n // 2, 3 * n // 4), (0.15 * hoehe, 0.0, 0.5 * hoehe), 1.5 * hoehe, ("seite",), 240)
    gemein.log(name, "Lebend-Clips fertig; Schleichen %.1f cm/s, Rennen %.1f cm/s bei eigenem Tempo" % (
        100.0 * 2 * 0.44 / 1.0 / k, 100.0 * 2 * 0.7 / 0.7 / k))


main()
