"""Step 6: the Kelchreisser animations as separate actions (30 frames per second, all in place: the creature actor
moves the root in Unreal).

  Stehen_Idle     4 s loop: crouched on all fours, palms flat on the ground, breathing, the flower head swaying,
                  petals breathing, the vines weaving slowly and their buds snapping now and then
  Galopp          0.9 s loop: on the scent, transverse gallop on flat palms and hind feet, spine coils and
                  stretches, head low and steady, vines streaming back
  Gehen_Aufrecht  1.6 s loop: half upright on the hind legs, the long arms hanging and swinging (creepy)
  Schlag          1.2 s: rears a little and swings the right arm round sideways, palm first (a slap)
  Klaue           1.0 s: raises the left arm high and rakes down with the claws in front of it
  Rankenbiss      1.4 s: the vines draw back, then strike forward, also over the head, and their buds snap shut
  Schrei          2.0 s: rears, throws the flower head up, all petals and inner jaws wide open, vines spread trembling

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
# The crouch: palms flat on the ground in front of the shoulders, the chest lowered over them
HAND_STAND = {"l": Vector((1.85, 1.85, 0.42)), "r": Vector((1.85, -1.85, 0.42))}
HAND_NEIGUNG = -14      # degrees the hand is turned tip-up against its rest slope, so the palm lies flat
FINGER_NEIGUNG = -18
BECKEN_STAND = 7        # degrees the body tips forward round the pelvis in the crouch
ARM = [None]
RANKEN = []             # per vine: its chain bone names


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
    p.becken = Vector((0.0, 0.0, -0.12 + 0.02 * a))
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

def stehen_idle(f):
    t = f / FPS
    p = grundhaltung(t)
    p.dazu("kopf", 3 * welle(t, 4.0), 0, 6 * welle(t, 4.0, 0.25))
    p.dazu("hals_02", 0, 0, 4 * welle(t, 4.0, 0.2))
    blaetter(p, 0.12 + 0.08 * welle(t, 2.0, 0.1))
    innenkiefer(p, 0.1 + 0.05 * welle(t, 2.0, 0.1))
    ranken_weben(p, t)
    for i in range(len(RANKEN)):
        # Each bud snaps once in the loop, at its own time
        p.winkel.update({})
        schnapp = huegel((t + i * 0.63) % 4.0, 0.0, 0.25, 0.35, 0.45)
        knospe(p, i, 0.25 + 0.6 * schnapp)
    return p


def galopp(f):
    periode = 0.9
    t = f / FPS
    ph = (t / periode) % 1.0
    p = grundhaltung(t, atem=0.0)
    # Spine: gathered when the hind feet reach forward, stretched when the hands reach out
    biegen = math.sin(2 * math.pi * ph)
    p.dazu("becken", -6 * biegen)
    p.dazu("wirbel_01", 6 * biegen)
    p.dazu("wirbel_02", 5 * biegen)
    p.dazu("brust", -4 * biegen)
    p.becken = Vector((0.0, 0.0, -0.12 + 0.22 * math.sin(4 * math.pi * ph)))
    # Head low and steady: the neck takes up the spine's swing
    p.dazu("hals_01", 6 + 4 * biegen)
    p.dazu("kopf", 4 - 4 * biegen)
    blaetter(p, -0.15)
    innenkiefer(p, 0.25)
    for i in range(len(RANKEN)):
        knospe(p, i, 0.4)
    ranken_beugen(p, -12)
    ranken_weben(p, t, staerke=2.0, periode=periode)
    # Feet: transverse gallop, each on the ground 40 % of the stride
    schritt = {"fuss_l": 0.0, "fuss_r": 0.1, "hand_l": 0.5, "hand_r": 0.6}
    weite = 2.2
    for name, versatz in schritt.items():
        s = name[-1]
        q = (ph - versatz) % 1.0
        if q < 0.4:
            x, z = weite / 2 - weite * q / 0.4, 0.0
        else:
            u = (q - 0.4) / 0.6
            x, z = -weite / 2 + weite * weich(u), 0.6 * math.sin(math.pi * u)
        if name.startswith("hand"):
            p.hand[s] = HAND_STAND[s] + Vector((x, 0.0, z))
            # The palm rolls off: heel up as it leaves the ground, flat on landing
            p.hand_drehung[s] = (HAND_NEIGUNG - (25 * math.sin(math.pi * (q - 0.4) / 0.6) if q >= 0.4 else 0), 0, 0)
        else:
            p.fuss[s] = BALLEN[s] + Vector((x, 0.0, z))
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
    p.dazu("wirbel_01", -4)
    p.dazu("wirbel_02", 6)
    p.dazu("brust", 10)
    # The head hangs forward and turns slowly from side to side, listening
    p.dazu("hals_01", 22)
    p.dazu("hals_02", 10)
    p.dazu("kopf", 8, 0, 12 * welle(t, 3.2))
    blaetter(p, 0.05)
    innenkiefer(p, 0.1)
    ranken_weben(p, t, staerke=1.5, periode=3.2)
    for i in range(len(RANKEN)):
        knospe(p, i, 0.3)
    # Long arms hanging down in front, swinging against the legs, fingers trailing
    for s, gegen in (("l", 0.5), ("r", 0.0)):
        schwung = math.sin(2 * math.pi * (ph + gegen))
        p.dazu("schulter_%s" % s, 0, 0, 0)
        p.dazu("oberarm_%s" % s, 38 + 14 * schwung, 8 if s == "l" else -8)
        p.dazu("unterarm_%s" % s, 12 + 8 * schwung)
        p.dazu("hand_%s" % s, 20)
        p.dazu("finger_%s" % s, 25 + 10 * schwung)
    # Hind legs walk, each on the ground 60 % of the stride
    weite = 1.5
    for s, versatz in (("l", 0.0), ("r", 0.5)):
        q = (ph - versatz) % 1.0
        if q < 0.6:
            x, z = weite / 2 - weite * q / 0.6, 0.0
        else:
            u = (q - 0.6) / 0.4
            x, z = -weite / 2 + weite * weich(u), 0.35 * math.sin(math.pi * u)
        p.fuss[s] = BALLEN[s] + Vector((x + 0.6, 0.0, z))
    return p


def schlag(f):
    t = f / FPS
    p = grundhaltung(t, atem=0.3)
    p.arm_ik["r"] = 0.0
    ausholen = huegel(t, 0.0, 0.35, 0.4, 0.55)
    zuschlagen = huegel(t, 0.4, 0.62, 0.7, 1.15)
    aufrichten = huegel(t, 0.05, 0.4, 0.75, 1.15)
    p.dazu("becken", -16 * aufrichten, -4 * aufrichten)
    p.becken += Vector((0.0, 0.0, 0.25 * aufrichten))
    p.dazu("brust", 0, 0, 25 * ausholen - 35 * zuschlagen)
    p.dazu("kopf", 0, 0, -15 * zuschlagen)
    # Right arm: raised out to the side, then swung round flat in front of the body
    p.dazu("oberarm_r", -60 * ausholen - 30 * zuschlagen, 70 * ausholen + 20 * zuschlagen,
           -40 * ausholen + 95 * zuschlagen)
    p.dazu("unterarm_r", -20 * ausholen - 10 * zuschlagen)
    p.dazu("hand_r", 0, 0, 20 * zuschlagen)
    p.dazu("finger_r", -15 * ausholen)
    innenkiefer(p, 0.2 + 0.5 * zuschlagen)
    blaetter(p, 0.1 + 0.2 * zuschlagen)
    ranken_weben(p, t, staerke=1.5, periode=1.2)
    ranken_beugen(p, -10 * zuschlagen, 15 * zuschlagen)
    for i in range(len(RANKEN)):
        knospe(p, i, 0.3)
    return p


def klaue(f):
    t = f / FPS
    p = grundhaltung(t, atem=0.3)
    p.arm_ik["l"] = 0.0
    heben = huegel(t, 0.0, 0.35, 0.42, 0.6)
    harken = huegel(t, 0.42, 0.6, 0.66, 0.98)
    p.dazu("becken", -12 * heben + 6 * harken)
    p.becken += Vector((0.0, 0.0, 0.2 * heben - 0.08 * harken))
    p.dazu("brust", -8 * heben + 10 * harken, -6 * heben)
    # Left arm straight up and over, claws spread, then down through the air in front
    p.dazu("oberarm_l", -120 * heben + 10 * harken, -15 * heben)
    p.dazu("unterarm_l", -25 * heben + 30 * harken)
    p.dazu("hand_l", -20 * heben + 45 * harken)
    p.dazu("finger_l", -30 * heben + 60 * harken)
    p.dazu("kopf", -10 * heben + 12 * harken)
    innenkiefer(p, 0.2 + 0.6 * heben)
    blaetter(p, 0.1 + 0.25 * heben)
    ranken_weben(p, t, staerke=1.5, periode=1.0)
    for i in range(len(RANKEN)):
        knospe(p, i, 0.3 + 0.4 * heben)
    return p


def rankenbiss(f):
    t = f / FPS
    p = grundhaltung(t, atem=0.3)
    p.dazu("kopf", 10)
    p.dazu("hals_01", 6)
    innenkiefer(p, 0.3)
    blaetter(p, 0.15)
    for i, kette in enumerate(RANKEN):
        # Staggered: each vine strikes a little after the one before
        v = (i % 3) * 0.06
        zurueck = huegel(t, 0.0 + v, 0.4 + v, 0.45 + v, 0.6 + v)
        stoss = huegel(t, 0.45 + v, 0.62 + v, 0.8 + v, 1.3)
        for k, name in enumerate(kette):
            anteil = (k + 1) / len(kette)
            # Draw back: the chain coils up and backwards; strike: flung forward and down, over the head
            p.dazu(name, (-10 * zurueck + 14 * stoss) * (0.4 + anteil), 3 * welle(t, 0.3, i * 0.2) * anteil)
        offen = huegel(t, 0.25 + v, 0.5 + v, 0.6 + v, 0.66 + v)
        knospe(p, i, offen)
    return p


def schrei(f):
    t = f / FPS
    p = grundhaltung(t, atem=0.0)
    hoch = huegel(t, 0.0, 0.5, 1.5, 1.95)
    beben = math.sin(2 * math.pi * t / 0.09) * huegel(t, 0.4, 0.55, 1.4, 1.6)
    p.dazu("becken", -22 * hoch)
    p.becken += Vector((0.0, 0.0, 0.45 * hoch))
    p.dazu("brust", -10 * hoch + 1.5 * beben)
    p.dazu("hals_01", -25 * hoch)
    p.dazu("hals_02", -15 * hoch)
    p.dazu("kopf", -20 * hoch + 2 * beben)
    blaetter(p, 1.0 * hoch, zittern=hoch, t=t)
    innenkiefer(p, 1.3 * hoch)
    for i, kette in enumerate(RANKEN):
        for k, name in enumerate(kette):
            anteil = (k + 1) / len(kette)
            p.dazu(name, -8 * hoch * anteil + 2 * beben * anteil, (6 if i % 2 else -6) * hoch * anteil)
        knospe(p, i, 0.2 + 0.8 * hoch)
    return p


AKTIONEN = [("Stehen_Idle", 4.0, stehen_idle, True), ("Galopp", 0.9, galopp, True),
            ("Gehen_Aufrecht", 1.6, gehen_aufrecht, True), ("Schlag", 1.2, schlag, False),
            ("Klaue", 1.0, klaue, False), ("Rankenbiss", 1.4, rankenbiss, False), ("Schrei", 2.0, schrei, False)]


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


def backen(arm, ziele, name, dauer, pose_bei, schleife):
    """Sets every frame, reads the bones' final local transforms (IK included) and keys them"""
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
        pose_setzen(arm, ziele, pose_bei(f))
        bpy.context.view_layer.update()
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
    for name, bilder in (("Stehen_Idle", (0,)), ("Galopp", (0, 9, 18)), ("Gehen_Aufrecht", (0, 12)),
                         ("Schlag", (12, 19)), ("Klaue", (12, 18)), ("Rankenbiss", (12, 20)), ("Schrei", (30,))):
        arm.animation_data.action = bpy.data.actions[name]
        for f in bilder:
            bpy.context.scene.frame_set(f)
            vorschau.rendern("06_%s_%02d" % (name.lower(), f), (0.25, -1.0, 0.25), (0.3, 0.0, 3.0), 8.0)
    arm.animation_data.action = None


main()
