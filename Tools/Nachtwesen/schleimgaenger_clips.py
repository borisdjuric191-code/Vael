"""Schleimgaenger, step 3: the movements of the walking skeleton host (30 frames per second, in place; the actor
moves it). The Tropfseuche moves the dead like Cordyceps moves an ant: the body is pulled, not carried. It hangs
forward from the chest, the head lolls, and now and then a limb jerks as if yanked by a string.

The bones are the Unreal mannequin's (shared by every humanlike Schleimgaenger). The body looks along +X, +Y is its
left. Hands and feet are placed by IK; lying and standing are one pose blended by "liegen" (0 = up, 1 = on its back),
so the corpse it dies into is the pose it lies in and rises from.

  Liegen       4 s loop    on its back, limbs splayed, now and then a finger or the jaw twitches
  Aufstehen    2.6 s       the Seuche wakes it: twitches, is hauled up by the chest, the legs fold under, it rises, the head snaps up last
  Stehen_Idle  4 s loop    hunched, head lolling, arms hanging, sudden jerks
  Gehen        1.4 s loop  a stiff stagger, the right foot drags (0.79 m/s at its own pace)
  Schlag       1.2 s       the right arm goes up and comes down across (hit at 0.6 s)
  Packen       1.4 s       both arms shoot forward, it lunges and grabs (0.65 s), pulls in and snaps with the jaw
  Treffer      0.5 s       jolts back, the head snaps aside
  Tod          1.6 s       the knees give way, it folds back and lies as in Liegen

    blender -b --factory-startup -P Tools/Nachtwesen/schleimgaenger_clips.py [-- <Aktion> ...]
"""
import math

import bpy
import os
import sys

from mathutils import Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bau  # noqa: E402
import gemein  # noqa: E402
from bau import Pose, huegel, rampe, unruhe, weich, welle  # noqa: E402

NAME = "Schleimgaenger"
MITTE = (0.15, 0.0, 0.9)
SEITEN = (("l", 1), ("r", -1))

# Where hands and feet go standing and lying (armature space, metres)
FUSS_STEHEN = {"l": Vector((-0.06, 0.16, 0.135)), "r": Vector((-0.06, -0.16, 0.135))}
FUSS_LIEGEN = {"l": Vector((0.8, 0.26, 0.08)), "r": Vector((0.78, -0.22, 0.08))}
HAND_STEHEN = {"l": Vector((0.06, 0.25, 0.95)), "r": Vector((0.06, -0.25, 0.95))}
HAND_LIEGEN = {"l": Vector((-0.4, 0.6, 0.06)), "r": Vector((-0.3, -0.62, 0.06))}
NULL = Vector((0.0, 0.0, 0.0))
# Hands turn with the forearm (2026-11-23, Kobold rigs K_*); the old Schleimgaenger skeleton was baked without it
HAND_FOLGT = False
# Fingers closed round a haft per side (K_ rigs with gear in the hand): angles for the middle_01_ bone
GRIFF = {}

def _drehen(pb, drehung, um):
    """Turns a pose bone (armature space) by a rotation about a point"""
    m = pb.matrix.copy()
    neu = (drehung.to_matrix() @ m.to_3x3()).to_4x4()
    neu.translation = um + drehung @ (m.translation - um)
    pb.matrix = neu


def pose_setzen_mannequin(arm, p):
    """bau.pose_setzen with an IK that works on joints: the Unreal mannequin's bones don't point at their child (their
    tails stick out sideways), so the limb runs from bone head to bone head, not along the bone"""
    ik = p.ik
    p.ik = []
    _pose_setzen_bau(arm, p)
    p.ik = ik
    lage = bpy.context.view_layer
    for oben, unten, ziel, pol, ende, ende_winkel in ik:
        lage.update()
        po, pu = arm.pose.bones[oben], arm.pose.bones[unten]
        pe = arm.pose.bones[ende]
        a, b, c = po.head.copy(), pu.head.copy(), pe.head.copy()
        l1, l2 = (b - a).length, (c - b).length
        zu = ziel - a
        dist = min(max(zu.length, abs(l1 - l2) + 1e-4), l1 + l2 - 1e-4)
        richt = zu.normalized()
        quer = pol - richt * pol.dot(richt)
        quer = quer.normalized() if quer.length > 1e-6 else Vector((0, 0, 1))
        cos_ = (l1 * l1 + dist * dist - l2 * l2) / (2 * l1 * dist)
        knie = a + richt * (l1 * cos_) + quer * (l1 * math.sqrt(max(0.0, 1 - cos_ * cos_)))
        _drehen(po, (b - a).rotation_difference(knie - a), a)
        lage.update()
        b2, c2 = pu.head.copy(), pe.head.copy()
        _drehen(pu, (c2 - b2).rotation_difference((a + richt * dist) - b2), b2)
        lage.update()
        rest = arm.data.bones[ende].matrix_local.to_3x3()
        if ende.startswith("hand_") and HAND_FOLGT:
            # A hand turns with its forearm (it used to keep its rest turn in the world, so a raised arm ended in a
            # hand bent back at the wrist); ende_winkel is a small extra turn on top
            unterarm = pu.matrix.to_3x3() @ arm.data.bones[unten].matrix_local.to_3x3().inverted()
            m = (bau._welt(ende_winkel) @ unterarm @ rest).to_4x4()
        else:
            m = (bau._welt(ende_winkel) @ rest).to_4x4()
        m.translation = pe.head
        pe.matrix = m
    lage.update()


_pose_setzen_bau = bau.pose_setzen
bau.pose_setzen = pose_setzen_mannequin

GEHEN_DAUER = 1.4
SCHRITT = 0.55       # metres per step, two steps per loop: 0.79 m/s


def mischen(a, b, t):
    return a + (b - a) * t


def ruck(t, wann, dauer=0.14):
    """A sharp jerk at a moment: up fast, back a little slower"""
    return huegel(t, wann, wann + dauer * 0.3, wann + dauer * 0.45, wann + dauer)


def koerper(p, liegen, buckel=1.0, kopf_haengt=1.0):
    """Lying and standing as one: the pelvis tips back onto the ground, the hunch and the hanging head fade out"""
    stehen = 1.0 - liegen
    p.dazu("pelvis", beuge=-88.0 * liegen + 6.0 * buckel * stehen)
    p.schieben("pelvis", z=-0.84 * weich(liegen), x=-0.05 * liegen)
    p.dazu("spine_02", beuge=9.0 * buckel * stehen)
    p.dazu("spine_03", beuge=8.0 * buckel * stehen)
    p.dazu("neck_01", beuge=14.0 * kopf_haengt * stehen)
    p.dazu("head", beuge=-6.0 * kopf_haengt * stehen, seite=16.0 * kopf_haengt * stehen + 10.0 * liegen,
           wende=-6.0 * liegen)


def glieder(p, liegen, hand=None, fuss=None, buckel=1.0):
    """Hands and feet: blended between hanging/standing and splayed/lying, plus offsets per side"""
    hand = hand or {}
    fuss = fuss or {}
    stehen = 1.0 - liegen
    for n, s in SEITEN:
        h = mischen(HAND_STEHEN[n] + Vector((0.14 * buckel, 0.0, -0.04 * buckel)), HAND_LIEGEN[n], liegen) + hand.get(n, NULL)
        f = mischen(FUSS_STEHEN[n], FUSS_LIEGEN[n], liegen) + fuss.get(n, NULL)
        p.greifen("upperarm_" + n, "lowerarm_" + n, h, mischen(Vector((-1.0, 0.35 * s, -0.2)), Vector((-0.2, 0.3 * s, 1.0)), liegen),
                  "hand_" + n, (0.0, 0.0, 0.0))
        p.greifen("thigh_" + n, "calf_" + n, f, mischen(Vector((1.0, 0.1 * s, 0.0)), Vector((0.2, 0.25 * s, 1.0)), liegen),
                  "foot_" + n, (-80.0 * liegen, 0.0, 0.0))
        if n in GRIFF:
            p.dazu("middle_01_" + n, **GRIFF[n])


def atem(p, t, schleife, staerke=1.0, saat=0.0):
    """No breath, but the Seuche keeps the body slightly in motion: a slow uneven sway"""
    p.schieben("pelvis", y=0.025 * staerke * unruhe(t, schleife, 1.0 + saat), x=0.015 * staerke * unruhe(t, schleife, 2.0 + saat))
    p.dazu("pelvis", seite=2.5 * staerke * unruhe(t, schleife, 3.0 + saat))
    p.dazu("spine_03", wende=3.0 * staerke * unruhe(t, schleife, 4.0 + saat))
    p.dazu("head", seite=6.0 * staerke * unruhe(t, schleife, 5.0 + saat), wende=8.0 * staerke * unruhe(t, schleife, 6.0 + saat))


# ---------------------------------------------------------------- Clips

def liegen(t):
    p = Pose()
    koerper(p, 1.0)
    zuck = ruck(t, 1.3, 0.2)
    kiefer = ruck(t, 2.9, 0.25)
    p.dazu("head", wende=10.0 * kiefer, seite=-5.0 * zuck)
    p.dazu("index_01_r", beuge=35.0 * zuck)
    p.dazu("middle_01_r", beuge=30.0 * zuck)
    glieder(p, 1.0, hand={"r": Vector((0.0, 0.0, 0.05 * zuck))})
    return p


def aufstehen(t):
    p = Pose()
    # 0 - 0.6 twitching on the ground; 0.6 - 1.5 hauled up by the chest (sitting); 1.5 - 2.2 up onto the feet; head last
    sitzen = rampe(t, 0.6, 1.5)
    hoch = rampe(t, 1.5, 2.2)
    liegt = 1.0 - (0.55 * sitzen + 0.45 * hoch)
    zuck = ruck(t, 0.15, 0.18) + ruck(t, 0.42, 0.14) + 0.7 * ruck(t, 1.45, 0.16)
    kopf_spaet = rampe(t, 2.15, 2.45)
    koerper(p, liegt, buckel=1.0 + 0.6 * sitzen * (1 - hoch), kopf_haengt=1.0 + 0.8 * (1 - kopf_spaet))
    # Pulled by the chest: the upper body leads, the head hangs back while it comes up, then snaps forward
    p.dazu("spine_03", beuge=-14.0 * huegel(t, 0.6, 1.1, 1.4, 1.9))
    p.dazu("head", beuge=-30.0 * huegel(t, 0.7, 1.2, 1.9, 2.3) + 14.0 * ruck(t, 2.3, 0.2), seite=-12.0 * ruck(t, 2.3, 0.2))
    p.dazu("spine_02", wende=12.0 * zuck)
    # The legs fold under it while it sits, then push it up
    knie = huegel(t, 0.9, 1.4, 1.7, 2.2)
    fuss = {n: Vector((-0.25 * knie, 0.0, 0.0)) for n, _ in SEITEN}
    hand = {"l": Vector((0.0, 0.0, 0.08 * zuck)), "r": Vector((0.1 * sitzen * (1 - hoch), 0.0, 0.0))}
    glieder(p, liegt, hand=hand, fuss=fuss, buckel=1.0 + 0.5 * hoch * (1 - kopf_spaet))
    return p


def stehen_idle(t):
    p = Pose()
    s = 4.0
    koerper(p, 0.0)
    atem(p, t, s)
    j1, j2, j3 = ruck(t, 0.9), ruck(t, 2.6, 0.18), ruck(t, 3.3, 0.12)
    p.dazu("head", wende=28.0 * j1, seite=-14.0 * j1, beuge=-10.0 * j3)
    p.dazu("clavicle_l", seite=12.0 * j2)
    p.dazu("spine_03", wende=-7.0 * j3)
    glieder(p, 0.0, hand={"l": Vector((0.03 * welle(t, s, 0.2), 0.0, 0.1 * j2)), "r": Vector((0.03 * welle(t, s, 0.7), 0.0, 0.0))})
    return p


def gehen(t):
    p = Pose()
    d = GEHEN_DAUER
    phase = (t / d) % 1.0
    koerper(p, 0.0, buckel=1.25)
    # A lurch to the side of the foot that bears the weight, a dip on the dragged right foot
    p.schieben("pelvis", y=0.05 * math.sin(2 * math.pi * phase), z=-0.035 * abs(math.sin(2 * math.pi * phase))
               - 0.03 * huegel(phase, 0.55, 0.65, 0.75, 0.9))
    p.dazu("pelvis", seite=-4.0 * math.sin(2 * math.pi * phase), wende=6.0 * math.sin(2 * math.pi * phase))
    p.dazu("spine_03", wende=-9.0 * math.sin(2 * math.pi * phase))
    p.dazu("head", seite=10.0 * math.sin(2 * math.pi * phase + 1.0), wende=10.0 * ruck(phase, 0.3, 0.12))
    fuss = {}
    for n, versatz in (("l", 0.0), ("r", 0.5)):
        q = (phase + versatz) % 1.0
        # On the ground for 60 % of the loop (sliding back at walking speed), in the air for 40 %
        if q < 0.6:
            x = SCHRITT * (0.5 - q / 0.6)
            z = 0.0
        else:
            u = (q - 0.6) / 0.4
            x = SCHRITT * (-0.5 + weich(u))
            # The right foot barely lifts: it drags
            z = (0.05 if n == "r" else 0.13) * math.sin(math.pi * u)
        fuss[n] = Vector((x, 0.0, z))
    hand = {n: Vector((0.12 + 0.08 * math.sin(2 * math.pi * (phase + (0.5 if n == "l" else 0.0))), 0.0, 0.05)) for n, _ in SEITEN}
    glieder(p, 0.0, hand=hand, fuss=fuss, buckel=1.25)
    return p


def schlag(t):
    p = Pose()
    auf = huegel(t, 0.1, 0.45, 0.5, 0.62)
    ab = huegel(t, 0.5, 0.62, 0.75, 1.15)
    koerper(p, 0.0, buckel=1.0 + 0.8 * ab)
    p.dazu("spine_03", wende=-22.0 * auf + 26.0 * ab, beuge=-10.0 * auf)
    p.dazu("head", wende=10.0 * ab, beuge=-14.0 * auf)
    p.schieben("pelvis", x=0.12 * ab)
    hand = {"r": Vector((-0.2 * auf + 0.45 * ab, -0.05 * auf + 0.22 * ab, 0.95 * auf - 0.05 * ab)),
            "l": Vector((0.05 * ab, 0.0, 0.0))}
    glieder(p, 0.0, hand=hand, fuss={"l": Vector((0.18 * ab, 0.0, 0.0))})
    return p


def packen(t):
    p = Pose()
    vor = huegel(t, 0.15, 0.6, 0.8, 1.2)
    zieh = huegel(t, 0.7, 0.95, 1.05, 1.35)
    biss = ruck(t, 0.95, 0.2)
    koerper(p, 0.0, buckel=1.0 + 1.0 * vor)
    p.schieben("pelvis", x=0.18 * vor - 0.06 * zieh)
    p.dazu("head", beuge=12.0 * biss - 10.0 * vor)
    hand = {n: Vector((0.42 * vor - 0.2 * zieh, -0.1 * s * vor, 0.45 * vor - 0.1 * zieh)) for n, s in SEITEN}
    glieder(p, 0.0, hand=hand, fuss={"l": Vector((0.22 * vor, 0.0, 0.0))})
    return p


def treffer(t):
    p = Pose()
    stoss = huegel(t, 0.0, 0.08, 0.15, 0.45)
    koerper(p, 0.0, buckel=1.0 - 0.8 * stoss)
    p.dazu("spine_03", beuge=-14.0 * stoss, wende=8.0 * stoss)
    p.dazu("head", seite=-25.0 * stoss, beuge=-12.0 * stoss)
    p.schieben("pelvis", x=-0.08 * stoss)
    glieder(p, 0.0, hand={n: Vector((-0.08 * stoss, 0.06 * s * stoss, 0.1 * stoss)) for n, s in SEITEN})
    return p


def tod(t):
    p = Pose()
    sinkt = rampe(t, 0.05, 0.6)
    faellt = rampe(t, 0.45, 1.25)
    liegt = 0.3 * sinkt * (1 - faellt) + faellt
    prall = huegel(t, 1.15, 1.22, 1.25, 1.45)
    koerper(p, liegt, buckel=1.0 + 0.8 * sinkt * (1 - faellt))
    p.schieben("pelvis", z=-0.25 * sinkt * (1 - faellt) + 0.03 * prall)
    p.dazu("head", seite=-10.0 * prall)
    knie = sinkt * (1 - faellt)
    glieder(p, liegt, fuss={n: Vector((0.15 * knie, 0.0, 0.0)) for n, _ in SEITEN},
            hand={n: Vector((0.0, 0.0, 0.06 * prall)) for n, _ in SEITEN})
    return p


AKTIONEN = {
    "Liegen": (4.0, liegen, True, (0, 39)),
    "Aufstehen": (2.6, aufstehen, False, (0, 20, 33, 45, 58, 66, 78)),
    "Stehen_Idle": (4.0, stehen_idle, True, (0, 27, 80)),
    "Gehen": (GEHEN_DAUER, gehen, True, (0, 7, 14, 21, 28, 35)),
    "Schlag": (1.2, schlag, False, (0, 13, 18, 24, 34)),
    "Packen": (1.4, packen, False, (0, 12, 19, 29, 36)),
    "Treffer": (0.5, treffer, False, (0, 4, 10)),
    "Tod": (1.6, tod, False, (0, 12, 24, 37, 47)),
}


def main():
    nur = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    datei = "03_animationen.blend" if nur and os.path.exists(gemein.ordner(NAME, "arbeit", "03_animationen.blend")) else "02_skelett.blend"
    obj, arm = gemein.oeffnen(NAME, datei)
    for aktion, (dauer, pose, schleife, _) in AKTIONEN.items():
        if not nur or aktion in nur:
            bau.backen(NAME, arm, aktion, dauer, pose, schleife)
    bau.ruhe(arm)
    gemein.speichern(NAME, "03_animationen.blend")
    for aktion, (_, _, _, bilder) in AKTIONEN.items():
        if not nur or aktion in nur:
            bau.clip_pruefen(NAME, arm, aktion, bilder, MITTE, 2.6, ("seite", "spiel"))


if __name__ == "__main__":
    main()
