"""Step 6: the Kelchreisser animations as separate actions (30 frames per second, all in place: the creature actor
moves the root in Unreal).

  Stehen_Idle     4 s loop: crouched on all fours, palms flat, restless: the body shifts from hand to hand, the
                  flower head snaps round to listen and holds still, petals breathing, the vines weaving, buds snapping
  Galopp          22 frames loop: rotary gallop on the scent, the back coiling and stretching, one float per stride,
                  palms peeling off and folding in the air, head gliding level, vines streaming and whipping back
  Gehen_Aufrecht  1.6 s loop: half upright on the hind legs, the long arms hanging and swinging (creepy)
  Lauern          3 s loop: crouched deep like a dog about to spring, chest low, flower head forward, breathing
  Prankenhieb     1.2 s: the right arm drawn far back, the body wound up, then a flat circle with the open clawed hand
  Klaue           1.0 s: rears, the left arm high and over, raking down in front
  Biss            0.9 s: petals and jaws torn open, a lunge and the flower slams shut
  Sprung          1.2 s: sinks deep, leaps long and flat with arms reaching, lands on the hands (the game moves it)
  Schrei          2.0 s: rears, throws the flower head up, all petals and inner jaws wide open, shaking

The vines' long strikes are not in here: the game flings them out to their prey (UVaelSequenceAnimInstance reach chains).
Hands and feet are placed with IK (targets as empties; the hand and the metatarsus keep the target's turn); the
result is baked to plain bone keys and the constraints removed again. Joint angles are written about world axes in
the rest pose (beuge = about Y, positive = nose/tip down; seite = about X; wende = about Z), as in Tools/Rankenluchs.
Renders key poses to vorschau/06_*. Saves arbeit/06_animationen.blend.

    blender -b --factory-startup -P Tools/Kelchreisser/06_animationen.py
"""
import json
import math
import os
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import vorschau  # noqa: E402

FPS = 30
SEITEN = ("l", "r")
# Rest positions (step 5) of the joints the IK aims at
HANDGELENK = {"l": Vector((1.5, 1.75, 1.25)), "r": Vector((1.5, -1.75, 1.25))}
BALLEN = {"l": Vector((-2.65, 1.1, 0.25)), "r": Vector((-2.65, -1.1, 0.25))}
# The crouch: palms flat on the ground under and a little in front of the shoulders (the left arm of the model is
# shorter, so its hand sits further back), the chest lowered over them
HAND_STAND = {"l": Vector((1.6, 1.85, 0.42)), "r": Vector((1.7, -1.85, 0.42))}
HAND_NEIGUNG = -14      # degrees the hand is turned tip-up against its rest slope, so the palm lies flat
FINGER_NEIGUNG = -18
BECKEN_STAND = 7        # degrees the body tips forward round the pelvis in the crouch
ARM = [None]
RANKEN = []             # per vine: its chain bone names
GLIEDER = {}            # per hand and foot: (bone, rest points riding on it), see glieder_messen
BODEN_TIEFE = 0.02      # m the lowest claw sinks into the ground where it stands (soil gives a little)


# ---------------------------------------------------------------- Pose helpers

def drehung(name, beuge=0.0, seite=0.0, wende=0.0):
    """Local rotation of a pose bone from angles about the world axes at rest (degrees)"""
    rest = ARM[0].data.bones[name].matrix_local.to_3x3()
    welt = Matrix.Rotation(math.radians(wende), 3, "Z") @ Matrix.Rotation(math.radians(seite), 3, "X") @ \
        Matrix.Rotation(math.radians(beuge), 3, "Y")
    return (rest.inverted() @ welt @ rest).to_quaternion()


def lokal_x(grad):
    """Turn about the bone's own X axis: jaws, petals (their roll is set in step 5 so +X opens/closes them)"""
    return Quaternion((1.0, 0.0, 0.0), math.radians(grad))


def weich(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def huegel(t, a, b, c, d):
    """0 before a, rising to 1 at b, 1 until c, falling to 0 at d"""
    return weich((t - a) / max(b - a, 1e-6)) * (1 - weich((t - c) / max(d - c, 1e-6)))


def welle(t, periode, phase=0.0):
    return math.sin(2 * math.pi * (t / periode + phase))


class Pose:
    """Bone angles, the pelvis offset and the IK targets of one frame"""

    def __init__(self):
        self.winkel = {}       # bone -> (beuge, seite, wende) or a local Quaternion
        self.becken = Vector()
        self.hand = {s: HAND_STAND[s].copy() for s in SEITEN}
        self.hand_drehung = {s: (HAND_NEIGUNG, 0.0, 0.0) for s in SEITEN}
        self.fuss = {s: BALLEN[s].copy() for s in SEITEN}
        self.fuss_drehung = {s: (0.0, 0.0, 0.0) for s in SEITEN}
        self.arm_ik = {s: 1.0 for s in SEITEN}

    def dazu(self, name, beuge=0.0, seite=0.0, wende=0.0):
        alt = self.winkel.get(name, (0.0, 0.0, 0.0))
        self.winkel[name] = (alt[0] + beuge, alt[1] + seite, alt[2] + wende)


def grundhaltung(t, atem=1.0):
    """The crouch on all fours with breathing"""
    p = Pose()
    a = welle(t, 2.0) * atem
    p.dazu("becken", BECKEN_STAND)
    p.becken = Vector((0.0, 0.0, -0.24 + 0.02 * a))
    p.dazu("wirbel_01", 0.8 * a)
    p.dazu("wirbel_02", -0.6 * a)
    p.dazu("brust", -1.0 * a)
    p.dazu("hals_01", -4)
    p.dazu("kopf", 6)
    p.dazu("finger_l", FINGER_NEIGUNG)
    p.dazu("finger_r", FINGER_NEIGUNG)
    return p


def blaetter(p, offen, zittern=0.0, t=0.0):
    """Petals: offen 0 = as modelled, 1 = folded right back; inner jaws open with them"""
    for k in range(1, 6):
        p.winkel["blatt_%02d" % k] = lokal_x(-55 * offen + zittern * 4 * welle(t, 0.12, k * 0.31))


def innenkiefer(p, offen):
    for n in ("kiefer_ol", "kiefer_or"):
        p.winkel[n] = lokal_x(30 * offen)
    for n in ("kiefer_ul", "kiefer_ur"):
        p.winkel[n] = lokal_x(-30 * offen)


def knospe(p, i, offen):
    """Bud of vine i: 0 shut, 1 wide open"""
    p.winkel["ranke_%d_kiefer_oben" % i] = lokal_x(-8 + 48 * offen)
    p.winkel["ranke_%d_kiefer_unten" % i] = lokal_x(8 - 48 * offen)


def ranken_weben(p, t, staerke=1.0, periode=4.0):
    """Slow weaving: a wave runs from the root to the tip, stronger towards the tip"""
    for i, kette in enumerate(RANKEN):
        n = len(kette)
        for k, name in enumerate(kette):
            anteil = (k + 1) / n
            p.dazu(name, 4 * staerke * anteil * welle(t, periode, i * 0.23 - k * 0.07),
                   3 * staerke * anteil * welle(t, periode, i * 0.41 - k * 0.05 + 0.25))


def ranken_beugen(p, beuge, seite=0.0, nur=None):
    """Bends whole vines (degrees per bone, spread evenly over the chain)"""
    for i, kette in enumerate(RANKEN):
        if nur is not None and i not in nur:
            continue
        for name in kette:
            p.dazu(name, beuge / len(kette) * 3, seite / len(kette) * 3)


# ---------------------------------------------------------------- Animations

def zielen(name, richtung):
    """Local rotation that points a bone (from its rest direction) along a world direction, in its parent's frame"""
    b = ARM[0].data.bones[name]
    rest = b.matrix_local.to_3x3()
    jetzt = (b.tail_local - b.head_local).normalized()
    welt = jetzt.rotation_difference(Vector(richtung).normalized()).to_matrix()
    return (rest.inverted() @ welt @ rest).to_quaternion()


def bogen_richtung(von, nach, t):
    """A direction swung round from one to another along the great circle, t 0..1"""
    a, b = Vector(von).normalized(), Vector(nach).normalized()
    return a.slerp(b, max(0.0, min(1.0, t))) if a.dot(b) > -0.999 else b


def raus(t):
    """Eases out hard: fast start, soft end (a strike)"""
    t = max(0.0, min(1.0, t))
    return 1 - (1 - t) ** 3


def schritt_bahn(q, auf, weite, hoehe):
    """A foot's path over one stride: on the ground for the share auf (sliding back), then lifted and swung forward.
    Returns the offset along the body, the height and how far into the swing it is (-1 on the ground)"""
    if q < auf:
        return weite / 2 - weite * q / auf, 0.0, -1.0
    u = (q - auf) / (1 - auf)
    # Up fast and high at first (the paw tucked), reaching forward and down onto the ground at the end
    x = -weite / 2 + weite * (u * u * (3 - 2 * u))
    z = hoehe * math.sin(math.pi * min(1.0, u * 1.15)) ** 0.8 if u < 0.87 else 0.0
    return x, z, u


def stehen_idle(f):
    """Crouched, restless: the body shifts from hand to hand, the flower head jerks round to listen and holds still"""
    t = f / FPS
    p = grundhaltung(t)
    wiegen = welle(t, 4.0)
    # Like a dog: sinks back onto its haunches, then rises tall on straight arms
    hocken = huegel(t, 0.3, 1.1, 1.6, 2.3)
    aufrichten = huegel(t, 2.4, 3.0, 3.3, 3.95)
    p.dazu("becken", 6 * hocken - 3 * aufrichten, 3 * wiegen, 2 * wiegen)
    p.becken += Vector((-0.25 * hocken + 0.05 * aufrichten, 0.1 * wiegen, -0.45 * hocken + 0.06 * aufrichten))
    p.dazu("wirbel_01", -3 * hocken + 2 * aufrichten)
    p.dazu("wirbel_02", -4 * hocken + 3 * aufrichten)
    p.dazu("brust", -4 * hocken + 4 * aufrichten, -2 * wiegen, -3 * welle(t, 4.0, 0.08))
    p.dazu("hals_01", 6 * hocken - 8 * aufrichten)
    # Listening: the head snaps to a new side, holds, snaps on (four times in the loop)
    ziele = [(-25, 8), (18, -4), (-8, 12), (30, 2)]
    for k, (wende, beuge) in enumerate(ziele):
        anteil = huegel((t - k * 1.0) % 4.0, 0.0, 0.12, 0.85, 1.0)
        p.dazu("hals_02", beuge * 0.4 * anteil, 0, wende * 0.4 * anteil)
        p.dazu("kopf", beuge * 0.6 * anteil, 6 * anteil * (1 if k % 2 else -1), wende * 0.6 * anteil)
    blaetter(p, 0.15 + 0.1 * welle(t, 2.0, 0.1))
    innenkiefer(p, 0.12 + 0.08 * welle(t, 2.0, 0.1))
    ranken_weben(p, t, staerke=2.2)
    for i in range(len(RANKEN)):
        # Each bud snaps twice in the loop, at its own time
        schnapp = huegel((t + i * 0.63) % 2.0, 0.0, 0.08, 0.15, 0.3)
        knospe(p, i, 0.2 + 0.8 * schnapp)
    for s in SEITEN:
        p.dazu("finger_" + s, 6 * huegel((t + (0.0 if s == "l" else 1.7)) % 4.0, 0.0, 0.3, 0.5, 0.9))
    return p


GALOPP_PERIODE = 22 / FPS
GALOPP_WEITE = 2.0


def lauern(f):
    """Lurking like a dog about to spring: haunches folded deep, chest low between the arms, flower head stretched
    forward near the ground, slow deep breaths, the shoulder blades rolling as it shifts its weight; vines raised"""
    t = f / FPS
    p = grundhaltung(t, atem=0.0)
    atem = welle(t, 3.0)
    wiegen = welle(t, 3.0, 0.2)
    p.dazu("becken", 9, 2 * wiegen)
    p.becken += Vector((-0.35, 0.08 * wiegen, -0.75 + 0.03 * atem))
    p.dazu("wirbel_01", -2 + 1.2 * atem)
    p.dazu("wirbel_02", -5 - 0.8 * atem)
    p.dazu("brust", 6 - 1.5 * atem, -2 * wiegen)
    p.dazu("hals_01", 10, 0, 4 * welle(t, 3.0, 0.35))
    p.dazu("hals_02", 6)
    p.dazu("kopf", 4 + 2 * atem, 0, 6 * welle(t, 3.0, 0.4))
    for s, gegen in (("l", 0.0), ("r", 0.5)):
        rollen = welle(t, 3.0, gegen)
        p.dazu("schulter_" + s, 5 * rollen, 0, (-6 if s == "l" else 6) * rollen)
        p.hand[s] = HAND_STAND[s] + Vector((-0.15, 0.0, 0.0))
    blaetter(p, 0.25 + 0.1 * atem)
    innenkiefer(p, 0.3 + 0.1 * atem)
    ranken_beugen(p, -10)
    ranken_weben(p, t, staerke=2.5, periode=3.0)
    for i in range(len(RANKEN)):
        knospe(p, i, 0.5 + 0.3 * welle(t, 1.5, i * 0.3))
    return p


def galopp(f):
    """Rotary gallop on the scent: the back coils as the hind feet reach under the body and stretches long as the
    hands reach out, one high float per stride after the hands; head low and level, vines streaming back"""
    t = f / FPS
    ph = (t / GALOPP_PERIODE) % 1.0
    p = grundhaltung(t, atem=0.0)
    # Gathered (back arched) at 0.95, stretched at 0.45
    biegen = math.cos(2 * math.pi * (ph - 0.95))
    p.dazu("becken", -7 * biegen, 2.5 * math.sin(2 * math.pi * ph), 2 * math.sin(2 * math.pi * (ph + 0.2)))
    p.dazu("wirbel_01", 7 * biegen)
    p.dazu("wirbel_02", 6 * biegen, 0, -2 * math.sin(2 * math.pi * ph))
    p.dazu("brust", 3 * biegen, -2 * math.sin(2 * math.pi * ph))
    # One float per stride, after the hands leave the ground; lowest while they carry the body
    p.becken = Vector((0.0, 0.0, -0.26 + 0.18 * math.cos(2 * math.pi * (ph - 0.88))))
    # The neck takes up the back's swing, so the flower glides
    p.dazu("hals_01", 8 - 9 * biegen)
    p.dazu("hals_02", 4 - 4 * biegen)
    p.dazu("kopf", 6 - 3 * biegen)
    blaetter(p, -0.2)
    innenkiefer(p, 0.35 + 0.15 * math.sin(2 * math.pi * ph))
    for i in range(len(RANKEN)):
        knospe(p, i, 0.5)
    # Vines stream back and whip with the stride, the tips late
    for i, kette in enumerate(RANKEN):
        n = len(kette)
        for k, name in enumerate(kette):
            anteil = (k + 1) / n
            p.dazu(name, -16 / n * 3 + 9 * anteil * math.sin(2 * math.pi * (ph - 0.12 * k - i * 0.1)),
                   4 * anteil * math.sin(2 * math.pi * (ph - 0.1 * k + i * 0.25)))
    # Shoulder blades slide forward with the reaching hand, the chest rolls over the hand that carries, the neck lags
    for s, versatz in (("l", 0.42), ("r", 0.5)):
        greifen = math.cos(2 * math.pi * ((ph - versatz) % 1.0 - 0.65))
        p.dazu("schulter_" + s, 6 * greifen, 0, (-12 if s == "l" else 12) * greifen)
    p.dazu("brust", 0, 4 * math.sin(2 * math.pi * (ph - 0.46)), 3 * math.sin(2 * math.pi * (ph - 0.5)))
    p.dazu("hals_02", 2 * math.cos(2 * math.pi * (ph - 0.1)), 0, -3 * math.sin(2 * math.pi * (ph - 0.6)))
    # Rotary gallop: right hind, left hind, left hand, right hand; each on the ground 30 % of the stride
    for name, versatz in (("fuss_r", 0.0), ("fuss_l", 0.08), ("hand_l", 0.42), ("hand_r", 0.5)):
        s = name[-1]
        q = (ph - versatz) % 1.0
        x, z, u = schritt_bahn(q, 0.3, GALOPP_WEITE, 0.85 if name.startswith("hand") else 0.7)
        if name.startswith("hand"):
            p.hand[s] = HAND_STAND[s] + Vector((x + 0.05, 0.0, z))
            # The palm peels off heel first and folds back in the air, then opens flat to land
            if u < 0:
                abrollen = 30 * weich((q / 0.3 - 0.7) / 0.3)
                p.hand_drehung[s] = (HAND_NEIGUNG + abrollen, 0, 0)
            else:
                p.hand_drehung[s] = (HAND_NEIGUNG + 75 * math.sin(math.pi * min(u / 0.85, 1.0)), 0, 0)
                p.dazu("finger_" + s, 45 * math.sin(math.pi * min(u / 0.85, 1.0)))
        else:
            p.fuss[s] = BALLEN[s] + Vector((x + 0.25, 0.0, z))
            if u >= 0:
                falten = math.sin(math.pi * min(u / 0.85, 1.0))
                p.fuss_drehung[s] = (50 * falten, 0, 0)
                p.dazu("zehen_" + s, 40 * falten)
    return p


def gehen_aufrecht(f):
    periode = 1.6
    t = f / FPS
    ph = (t / periode) % 1.0
    p = Pose()
    p.arm_ik = {s: 0.0 for s in SEITEN}
    wippen = math.sin(4 * math.pi * ph)
    p.dazu("becken", -38, 3 * math.sin(2 * math.pi * ph), 4 * math.sin(2 * math.pi * ph))
    p.becken = Vector((0.0, 0.0, 0.25 + 0.06 * wippen))
    p.dazu("wirbel_01", -4, 0, -3 * math.sin(2 * math.pi * ph))
    p.dazu("wirbel_02", 6, 0, -3 * math.sin(2 * math.pi * ph))
    p.dazu("brust", 10, 2 * math.sin(2 * math.pi * ph))
    # The head hangs forward and turns slowly from side to side, listening
    p.dazu("hals_01", 22)
    p.dazu("hals_02", 10)
    p.dazu("kopf", 8, 0, 12 * welle(t, 3.2))
    blaetter(p, 0.05)
    innenkiefer(p, 0.1)
    ranken_weben(p, t, staerke=2.0, periode=3.2)
    for i in range(len(RANKEN)):
        knospe(p, i, 0.3)
    # Long arms hanging down in front, swinging against the legs, the swing running out to the fingers
    for s, gegen in (("l", 0.5), ("r", 0.0)):
        schwung = math.sin(2 * math.pi * (ph + gegen))
        nach = math.sin(2 * math.pi * (ph + gegen - 0.08))
        p.dazu("oberarm_%s" % s, 38 + 14 * schwung, 8 if s == "l" else -8)
        p.dazu("unterarm_%s" % s, 12 + 10 * nach)
        p.dazu("hand_%s" % s, 20 + 8 * math.sin(2 * math.pi * (ph + gegen - 0.16)))
        p.dazu("finger_%s" % s, 25 + 12 * math.sin(2 * math.pi * (ph + gegen - 0.24)))
    # Hind legs walk, each on the ground 60 % of the stride
    for s, versatz in (("l", 0.0), ("r", 0.5)):
        x, z, u = schritt_bahn((ph - versatz) % 1.0, 0.6, 1.5, 0.35)
        p.fuss[s] = BALLEN[s] + Vector((x + 0.6, 0.0, z))
        if u >= 0:
            p.fuss_drehung[s] = (30 * math.sin(math.pi * u), 0, 0)
    return p


def prankenhieb(f):
    """Wide, felt swing of the right arm: drawn far back and out, the body wound up, then round in a flat circle in
    front of it with the open, clawed hand and through to the left; the body follows the swing"""
    t = f / FPS
    p = grundhaltung(t, atem=0.3)
    p.arm_ik["r"] = 0.0
    ausholen = huegel(t, 0.0, 0.38, 0.42, 0.5)
    schwung = raus((t - 0.42) / 0.26)
    zurueck = weich((t - 0.85) / 0.35)
    p.dazu("becken", -12 * ausholen + 6 * schwung * (1 - zurueck), 5 * ausholen - 6 * schwung * (1 - zurueck))
    p.becken += Vector((-0.25 * ausholen + 0.45 * schwung * (1 - zurueck), 0.0, 0.2 * ausholen))
    # Wound up to the right, unwound hard to the left
    drehen = (-28 * ausholen + 62 * schwung) * (1 - zurueck)
    p.dazu("wirbel_02", 0, 0, drehen * 0.35)
    p.dazu("brust", -8 * ausholen, 6 * ausholen, drehen * 0.65)
    p.dazu("kopf", 0, 0, -drehen * 0.3)
    # The arm: back and out, round through the front, through to the left; elbow almost straight
    hinten = Vector((-0.55, -0.8, 0.25))
    vorn = Vector((0.95, -0.15, -0.15))
    links = Vector((0.45, 0.85, -0.3))
    ruhe = Vector((0.4, -0.25, -0.88))
    if t < 0.42:
        richtung = bogen_richtung(ruhe, hinten, weich(t / 0.38))
    elif t < 0.56:
        richtung = bogen_richtung(hinten, vorn, raus((t - 0.42) / 0.14))
    elif t < 0.75:
        richtung = bogen_richtung(vorn, links, raus((t - 0.56) / 0.19))
    else:
        richtung = bogen_richtung(links, ruhe, weich((t - 0.85) / 0.35))
    p.winkel["oberarm_r"] = zielen("oberarm_r", richtung)
    p.dazu("unterarm_r", -15 * ausholen - 5 * schwung)
    p.dazu("hand_r", -25 * ausholen, 0, 25 * schwung * (1 - zurueck))
    p.dazu("finger_r", -30 * ausholen - 25 * schwung * (1 - zurueck))
    innenkiefer(p, 0.3 + 0.7 * huegel(t, 0.3, 0.5, 0.7, 1.0))
    blaetter(p, 0.15 + 0.35 * huegel(t, 0.3, 0.5, 0.7, 1.0))
    ranken_weben(p, t, staerke=1.5, periode=1.2)
    ranken_beugen(p, -6 * ausholen + 10 * schwung * (1 - zurueck), -10 * schwung * (1 - zurueck))
    for i in range(len(RANKEN)):
        knospe(p, i, 0.4 + 0.5 * schwung * (1 - zurueck))
    return p


def klaue(f):
    """Rears on the hind legs, the left arm high and over, then rakes down through the air in front with the claws"""
    t = f / FPS
    p = grundhaltung(t, atem=0.3)
    p.arm_ik["l"] = 0.0
    heben = huegel(t, 0.0, 0.32, 0.4, 0.58)
    harken = raus((t - 0.4) / 0.18) * (1 - weich((t - 0.7) / 0.3))
    p.dazu("becken", -16 * heben + 8 * harken)
    p.becken += Vector((0.2 * harken, 0.0, 0.3 * heben - 0.1 * harken))
    p.dazu("brust", -10 * heben + 14 * harken, -6 * heben, 8 * heben - 6 * harken)
    if t < 0.4:
        richtung = bogen_richtung(Vector((0.4, 0.25, -0.88)), Vector((0.1, 0.5, 0.86)), weich(t / 0.32))
    elif t < 0.62:
        richtung = bogen_richtung(Vector((0.1, 0.5, 0.86)), Vector((0.75, 0.1, -0.65)), raus((t - 0.4) / 0.2))
    else:
        richtung = bogen_richtung(Vector((0.75, 0.1, -0.65)), Vector((0.4, 0.25, -0.88)), weich((t - 0.7) / 0.3))
    p.winkel["oberarm_l"] = zielen("oberarm_l", richtung)
    p.dazu("unterarm_l", -20 * heben + 25 * harken)
    p.dazu("hand_l", -30 * heben + 50 * harken)
    p.dazu("finger_l", -35 * heben + 70 * harken)
    p.dazu("kopf", -12 * heben + 14 * harken)
    innenkiefer(p, 0.3 + 0.8 * heben)
    blaetter(p, 0.1 + 0.4 * heben)
    ranken_weben(p, t, staerke=2.0, periode=1.0)
    for i in range(len(RANKEN)):
        knospe(p, i, 0.3 + 0.6 * heben)
    return p


def biss(f):
    """The flower head bites: chest down, petals and jaws torn wide open, a lunge forward and the jaws slam shut"""
    t = f / FPS
    p = grundhaltung(t, atem=0.0)
    oeffnen = huegel(t, 0.0, 0.3, 0.38, 0.46)
    stoss = raus((t - 0.32) / 0.16) * (1 - weich((t - 0.55) / 0.35))
    zu = weich((t - 0.42) / 0.06) * (1 - weich((t - 0.65) / 0.25))
    p.dazu("becken", 4 * oeffnen + 6 * stoss)
    p.becken += Vector((-0.3 * oeffnen + 0.9 * stoss, 0.0, 0.1 * oeffnen - 0.15 * stoss))
    p.dazu("brust", -8 * oeffnen + 6 * stoss)
    p.dazu("hals_01", -18 * oeffnen + 14 * stoss)
    p.dazu("hals_02", -10 * oeffnen + 10 * stoss)
    p.dazu("kopf", -12 * oeffnen + 16 * stoss, 0, 4 * math.sin(2 * math.pi * t / 0.1) * zu)
    for s in SEITEN:
        # The hands stay planted, the body throws itself over them
        p.hand[s] = HAND_STAND[s] + Vector((0.3 * stoss, 0.0, 0.0))
    blaetter(p, 0.2 + 0.9 * oeffnen - 0.5 * zu)
    innenkiefer(p, 0.2 + 1.4 * oeffnen - 0.6 * zu)
    ranken_weben(p, t, staerke=1.5, periode=0.9)
    ranken_beugen(p, -8 * oeffnen + 12 * stoss)
    for i in range(len(RANKEN)):
        knospe(p, i, 0.3 + 0.6 * oeffnen)
    return p


def sprung(f):
    """Leap: sinks deep, throws itself forward long and flat with the arms reaching and claws spread, lands on the
    hands and catches itself (the game moves it forward from 0.35 to 0.8 s)"""
    t = f / FPS
    p = grundhaltung(t, atem=0.0)
    ducken = huegel(t, 0.0, 0.3, 0.32, 0.42)
    flug = huegel(t, 0.35, 0.5, 0.65, 0.82)
    landen = huegel(t, 0.78, 0.86, 0.92, 1.2)
    p.dazu("becken", 8 * ducken - 12 * flug + 10 * landen)
    p.becken += Vector((0.0, 0.0, -0.55 * ducken + 0.9 * flug - 0.35 * landen))
    p.dazu("wirbel_01", 6 * ducken - 6 * flug + 5 * landen)
    p.dazu("wirbel_02", 5 * ducken - 6 * flug + 4 * landen)
    p.dazu("hals_01", -10 * ducken - 8 * flug + 6 * landen)
    p.dazu("kopf", -8 * ducken + 10 * flug)
    for s in SEITEN:
        p.hand[s] = HAND_STAND[s] + Vector((-0.4 * ducken + 1.6 * flug + 0.4 * landen, 0.0, 1.2 * flug))
        p.hand_drehung[s] = (HAND_NEIGUNG - 25 * flug, 0, 0)
        p.dazu("finger_" + s, -35 * flug)
        p.fuss[s] = BALLEN[s] + Vector((0.3 * ducken - 1.3 * flug, 0.0, 0.9 * flug))
        p.fuss_drehung[s] = (60 * flug, 0, 0)
    blaetter(p, 0.2 + 0.7 * flug)
    innenkiefer(p, 0.3 + 1.0 * flug)
    ranken_beugen(p, -25 * flug + 10 * landen)
    ranken_weben(p, t, staerke=1.5, periode=1.2)
    for i in range(len(RANKEN)):
        knospe(p, i, 0.3 + 0.7 * flug)
    return p


def schrei(f):
    t = f / FPS
    p = grundhaltung(t, atem=0.0)
    hoch = huegel(t, 0.0, 0.45, 1.5, 1.95)
    beben = math.sin(2 * math.pi * t / 0.07) * huegel(t, 0.35, 0.5, 1.4, 1.6)
    zittern = math.sin(2 * math.pi * t / 0.11 + 1.0) * huegel(t, 0.35, 0.5, 1.4, 1.6)
    p.dazu("becken", -24 * hoch, 2 * zittern)
    p.becken += Vector((0.0, 0.0, 0.5 * hoch))
    p.dazu("brust", -12 * hoch + 2 * beben, 2 * zittern)
    p.dazu("hals_01", -28 * hoch + 2 * zittern)
    p.dazu("hals_02", -15 * hoch, 0, 4 * zittern)
    p.dazu("kopf", -22 * hoch + 4 * beben, 3 * zittern, 5 * beben)
    blaetter(p, 1.1 * hoch, zittern=hoch, t=t)
    innenkiefer(p, 1.5 * hoch)
    for i, kette in enumerate(RANKEN):
        for k, name in enumerate(kette):
            anteil = (k + 1) / len(kette)
            p.dazu(name, -10 * hoch * anteil + 4 * beben * anteil, (8 if i % 2 else -8) * hoch * anteil + 3 * zittern * anteil)
        knospe(p, i, 0.2 + 0.9 * hoch)
    for s in SEITEN:
        p.dazu("finger_" + s, -30 * hoch)
    return p


AKTIONEN = [("Stehen_Idle", 4.0, stehen_idle, True), ("Galopp", GALOPP_PERIODE, galopp, True),
            ("Gehen_Aufrecht", 1.6, gehen_aufrecht, True), ("Lauern", 3.0, lauern, True),
            ("Prankenhieb", 1.2, prankenhieb, False),
            ("Klaue", 1.0, klaue, False), ("Biss", 0.9, biss, False), ("Sprung", 1.2, sprung, False),
            ("Schrei", 2.0, schrei, False)]


# ---------------------------------------------------------------- IK rig, keying, baking

def leeres(name):
    e = bpy.data.objects.new(name, None)
    bpy.context.scene.collection.objects.link(e)
    e.rotation_mode = "QUATERNION"
    return e


def ik_aufbauen(arm):
    """Empties: hand_X at the wrist (its turn = the hand's), fuss_X at the ball of the foot (turn = the metatarsus'),
    ferse_X a child of fuss_X at the hock. IK: unterarm_X (2 bones) to hand_X, unterschenkel_X (2 bones) to ferse_X."""
    ziele = {}
    for s in SEITEN:
        hand = leeres("ziel_hand_" + s)
        fuss = leeres("ziel_fuss_" + s)
        ferse = leeres("ziel_ferse_" + s)
        ferse.parent = fuss
        # In the target's own frame, which is turned like the metatarsus at rest
        ferse.location = arm.data.bones["mittelfuss_" + s].matrix_local.to_3x3().inverted() @             (arm.data.bones["mittelfuss_" + s].head_local - BALLEN[s])
        ziele[s] = (hand, fuss, ferse)
        pb = arm.pose.bones
        c = pb["unterarm_" + s].constraints.new("IK")
        c.target, c.chain_count, c.name = hand, 2, "IK_Arm"
        c = pb["hand_" + s].constraints.new("COPY_ROTATION")
        c.target, c.name = hand, "Drehung_Hand"
        c = pb["unterschenkel_" + s].constraints.new("IK")
        c.target, c.chain_count, c.name = ferse, 2, "IK_Bein"
        c = pb["mittelfuss_" + s].constraints.new("COPY_ROTATION")
        c.target, c.name = fuss, "Drehung_Fuss"
    return ziele


def welt_drehung(arm, bone, winkel):
    rest = arm.data.bones[bone].matrix_local.to_3x3()
    beuge, seite, wende = winkel
    welt = Matrix.Rotation(math.radians(wende), 3, "Z") @ Matrix.Rotation(math.radians(seite), 3, "X") @ \
        Matrix.Rotation(math.radians(beuge), 3, "Y")
    return (welt @ rest).to_quaternion()


def pose_setzen(arm, ziele, p):
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
        w = p.winkel.get(pb.name)
        pb.rotation_quaternion = Quaternion() if w is None else (w if isinstance(w, Quaternion) else drehung(pb.name, *w))
        pb.location = (0, 0, 0)
    rest = arm.data.bones["becken"].matrix_local.to_3x3()
    arm.pose.bones["becken"].location = rest.inverted() @ p.becken
    for s in SEITEN:
        hand, fuss, _ = ziele[s]
        hand.location = p.hand[s]
        hand.rotation_quaternion = welt_drehung(arm, "hand_" + s, p.hand_drehung[s])
        fuss.location = p.fuss[s]
        fuss.rotation_quaternion = welt_drehung(arm, "mittelfuss_" + s, p.fuss_drehung[s])
        for name in ("IK_Arm", "Drehung_Hand"):
            bone = "unterarm_" + s if name == "IK_Arm" else "hand_" + s
            arm.pose.bones[bone].constraints[name].influence = p.arm_ik[s]


def glieder_messen(obj, arm):
    """Per hand and foot: the rest positions of the vertices that ride on its bones (hand + fingers, metatarsus + toes)
    and the bone each rides on, to find the lowest point of a posed hand without evaluating the whole mesh"""
    gruppen = {vg.index: vg.name for vg in obj.vertex_groups}
    teile = {"hand_" + s: ("hand_" + s, "finger_" + s) for s in SEITEN}
    teile.update({"fuss_" + s: ("mittelfuss_" + s, "zehen_" + s) for s in SEITEN})
    gemessen = {}
    for glied, knochen in teile.items():
        punkte, auf = [], []
        for vx in obj.data.vertices:
            best = max(vx.groups, key=lambda g: g.weight, default=None)
            if best is not None and best.weight > 0.5 and gruppen[best.group] in knochen:
                punkte.append(vx.co.copy())
                auf.append(gruppen[best.group])
        gemessen[glied] = [(k, [pt for pt, b in zip(punkte, auf) if b == k]) for k in knochen]
    GLIEDER.update(gemessen)


def tiefster_punkt(arm, glied):
    """Height of the lowest point of a hand or foot in the current pose"""
    tiefst = 1e9
    for knochen, punkte in GLIEDER[glied]:
        if not punkte:
            continue
        pb = arm.pose.bones[knochen]
        m = arm.matrix_world @ pb.matrix @ arm.data.bones[knochen].matrix_local.inverted()
        tiefst = min(tiefst, min((m @ pt).z for pt in punkte))
    return tiefst


def boden_halten(arm, ziele, p):
    """Sets the pose so the fingers and claws (not the wrist) touch the ground: a hand or foot meant to stand is moved
    up or down until its lowest point is on the ground, one in the air is lifted out of it if it dips in"""
    for _ in range(3):
        pose_setzen(arm, ziele, p)
        bpy.context.view_layer.update()
        geaendert = False
        for s in SEITEN:
            for glied, ziel, ruhe in (("hand_" + s, p.hand, HAND_STAND), ("fuss_" + s, p.fuss, BALLEN)):
                if glied.startswith("hand") and p.arm_ik[s] < 0.5:
                    continue
                tiefst = tiefster_punkt(arm, glied)
                steht = ziel[s].z <= ruhe[s].z + 0.02
                schub = -tiefst + BODEN_TIEFE if steht else max(0.0, -tiefst + BODEN_TIEFE)
                if abs(schub) > 0.005:
                    ziel[s] = ziel[s] + Vector((0.0, 0.0, schub))
                    geaendert = True
        if not geaendert:
            break


def backen(arm, ziele, name, dauer, pose_bei, schleife):
    """Sets every frame (feet and hands on the ground), reads the bones' final local transforms (IK included) and keys them"""
    rahmen = int(round(dauer * FPS))
    act = bpy.data.actions.get(name)
    if act:
        bpy.data.actions.remove(act)
    act = bpy.data.actions.new(name)
    act.use_fake_user = True
    arm.animation_data_create()
    arm.animation_data.action = None
    werte = []
    for f in range(rahmen + 1):
        boden_halten(arm, ziele, pose_bei(f))
        bild = {}
        for pb in arm.pose.bones:
            lokal = arm.convert_space(pose_bone=pb, matrix=pb.matrix, from_space="POSE", to_space="LOCAL")
            bild[pb.name] = (lokal.to_translation(), lokal.to_quaternion())
        werte.append(bild)
    if schleife:
        werte[-1] = werte[0]
    arm.animation_data.action = act
    for f, bild in enumerate(werte):
        for pb in arm.pose.bones:
            ort, quat = bild[pb.name]
            pb.rotation_quaternion = quat
            pb.keyframe_insert("rotation_quaternion", frame=f, group=pb.name)
            if pb.name == "becken":
                pb.location = ort
                pb.keyframe_insert("location", frame=f, group=pb.name)
    act.frame_range = (0, rahmen - 1 if schleife else rahmen)
    act.use_frame_range = True
    act.use_cyclic = schleife
    arm.animation_data.action = None
    print("[Kelchreisser] Aktion %s: %d Bilder (%.2f s)%s" % (name, rahmen, rahmen / FPS, ", Schleife" if schleife else ""))
    return act


def main():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(vorschau.ARBEIT, "05_skelett.blend"))
    bpy.context.scene.render.fps = FPS
    arm = bpy.data.objects["Kelchreisser_Skelett"]
    ARM[0] = arm
    with open(os.path.join(vorschau.ARBEIT, "ranken.json"), encoding="utf-8") as f:
        anzahl = len(json.load(f))
    for i in range(anzahl):
        RANKEN.append(sorted(b.name for b in arm.data.bones if b.name.startswith("ranke_%d_" % i) and b.name[-2:].isdigit()))
    ziele = ik_aufbauen(arm)
    glieder_messen(bpy.data.objects["Kelchreisser"], arm)
    for name, dauer, pose_bei, schleife in AKTIONEN:
        backen(arm, ziele, name, dauer, pose_bei, schleife)
    # The constraints and targets were only for baking
    for pb in arm.pose.bones:
        for c in list(pb.constraints):
            pb.constraints.remove(c)
    for s in SEITEN:
        for e in ziele[s][::-1]:
            bpy.data.objects.remove(e, do_unlink=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(vorschau.ARBEIT, "06_animationen.blend"))

    # Key poses
    for name, bilder in (("Stehen_Idle", (0, 45, 90)), ("Lauern", (0,)), ("Prankenhieb", (10, 15, 19)), ("Klaue", (11, 17)), ("Biss", (11, 14)),
                         ("Sprung", (9, 17)), ("Schrei", (30,))):
        arm.animation_data.action = bpy.data.actions[name]
        for f in bilder:
            bpy.context.scene.frame_set(f)
            vorschau.rendern("06_%s_%02d" % (name.lower(), f), (0.25, -1.0, 0.25), (0.3, 0.0, 3.0), 8.0)
    # The gallop as a strip of frames from the side, to judge its flow
    arm.animation_data.action = bpy.data.actions["Galopp"]
    for f in range(0, 22, 3):
        bpy.context.scene.frame_set(f)
        vorschau.rendern("06_galopp_%02d" % f, (0.0, -1.0, 0.05), (0.3, 0.0, 3.0), 8.5)
    arm.animation_data.action = None


main()
