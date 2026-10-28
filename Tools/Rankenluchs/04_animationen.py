"""Step 5: the Rankenluchs animations as separate actions (30 frames per second).

  Stehen_Idle     4 s loop: breathing, the tail swaying, an ear twitch now and then, the head looking about
  Tarn_Ruhe       2 s loop: lying flat, legs folded under, head between the front paws, tail laid round the body,
                  the crest fanned out to all sides (a bush), its leaves swaying in the wind, very slight breathing
  Erwachen        1.0 s: from Tarn_Ruhe to standing; the crest folds onto the back and the head rises first
  Sprung_Angriff  0.8 s: crouch, leap forward, strike, land (root motion on "wurzel": 0.45 m forward)
  Laufen          1.0 s loop, cat walk (lateral sequence): paws set down flat, front wrists fold the paw up behind,
                  hips and shoulders roll, the spine snakes, the head glides level (reworked 2026-10-28, Boris: too stiff)
  Rennen          0.53 s loop, rotary gallop: the spine coils and stretches, the head stays steady

Poses are written as joint angles in the side plane (positive = swing back / nose down), sideways and turning angles,
so they read the same on every bone whatever its roll. Renders key poses to vorschau/04_*.
Saves arbeit/04_animationen.blend.

    blender -b --factory-startup -P skripte/04_animationen.py
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import vorschau  # noqa: E402

ORDNER = vorschau.ORDNER
FPS = 30
BEINE = ("vl", "vr", "hl", "hr")
SCHWEIF = ["schweif_%02d" % (i + 1) for i in range(10)]
KAMM = ["kamm_%02d" % (i + 1) for i in range(10)]
ARM = [None]


# ---------------------------------------------------------------- Pose helpers

def drehung(name, beuge=0.0, seite=0.0, wende=0.0):
    """Local rotation of a pose bone from angles about the world side (Y), length (X) and up (Z) axes, in degrees"""
    rest = ARM[0].data.bones[name].matrix_local.to_3x3()
    welt = Matrix.Rotation(math.radians(wende), 3, "Z") @ Matrix.Rotation(math.radians(seite), 3, "X") @ \
        Matrix.Rotation(math.radians(beuge), 3, "Y")
    return (rest.inverted() @ welt @ rest).to_quaternion()


def verschiebung(name, welt):
    rest = ARM[0].data.bones[name].matrix_local.to_3x3()
    return rest.inverted() @ Vector(welt)


def als_quat(name, wert):
    if wert is None:
        return Quaternion()
    return wert if isinstance(wert, Quaternion) else drehung(name, *wert)


def pose_anwenden(pose):
    """pose: {bone: (beuge, seite, wende) or a local Quaternion}, plus "_ort": {bone: world offset}, "_groesse": {bone: factor}"""
    for pb in ARM[0].pose.bones:
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = als_quat(pb.name, pose.get(pb.name))
        pb.location = verschiebung(pb.name, pose.get("_ort", {}).get(pb.name, (0, 0, 0)))
        s = pose.get("_groesse", {}).get(pb.name, 1.0)
        pb.scale = (s, s, s)


def mischen(a, b, t):
    """Blend two poses (angles and offsets linearly)"""
    out = {}
    namen = (set(a) | set(b)) - {"_ort", "_groesse"}
    for n in namen:
        wa = a.get(n, (0.0, 0.0, 0.0))
        wb = b.get(n, (0.0, 0.0, 0.0))
        if isinstance(wa, Quaternion) or isinstance(wb, Quaternion):
            out[n] = als_quat(n, wa).slerp(als_quat(n, wb), t)
        else:
            out[n] = tuple(x + (y - x) * t for x, y in zip(wa, wb))
    for key, leer in (("_ort", (0.0, 0.0, 0.0)), ("_groesse", 1.0)):
        da, db = a.get(key, {}), b.get(key, {})
        out[key] = {}
        for n in set(da) | set(db):
            va, vb = da.get(n, leer), db.get(n, leer)
            out[key][n] = tuple(x + (y - x) * t for x, y in zip(va, vb)) if isinstance(leer, tuple) else va + (vb - va) * t
    return out


def weich(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def aktion(name, rahmen, pose_bei, schritt=2, schleife=False):
    """Keys pose_bei(frame) every `schritt` frames from 0 to `rahmen` into a new action"""
    arm = ARM[0]
    act = bpy.data.actions.get(name)
    if act:
        bpy.data.actions.remove(act)
    act = bpy.data.actions.new(name)
    act.use_fake_user = True
    arm.animation_data_create()
    arm.animation_data.action = act
    frames = list(range(0, rahmen + 1, schritt))
    if frames[-1] != rahmen:
        frames.append(rahmen)
    for f in frames:
        pose_anwenden(pose_bei(f))
        for pb in arm.pose.bones:
            pb.keyframe_insert("rotation_quaternion", frame=f, group=pb.name)
            if pb.name in ("wurzel", "becken") or pb.name in KAMM:
                pb.keyframe_insert("location", frame=f, group=pb.name)
            if pb.name in KAMM:
                pb.keyframe_insert("scale", frame=f, group=pb.name)
    act.frame_range = (0, rahmen - 1 if schleife else rahmen)
    act.use_frame_range = True
    act.use_cyclic = schleife
    print("[Rankenluchs] Aktion %s: %d Bilder (%.2f s)%s" % (name, rahmen, rahmen / FPS, ", Schleife" if schleife else ""))
    return act


# ---------------------------------------------------------------- Poses

TARN_SCHWEIF = {}


def kette_ausrichten(pose, namen, richtungen):
    """Turns a chain bone by bone so each points along its world direction; returns their local quaternions"""
    arm = ARM[0]
    pose_anwenden(pose)
    ergebnis = {}
    for n, ziel in zip(namen, richtungen):
        bpy.context.view_layer.update()
        pb = arm.pose.bones[n]
        jetzt = (pb.tail - pb.head).normalized()
        welt = jetzt.rotation_difference(Vector(ziel).normalized()).to_matrix()
        lage = pb.matrix.to_3x3().normalized()
        q = (lage.inverted() @ welt @ lage).to_quaternion() @ pb.rotation_quaternion
        pb.rotation_quaternion = q
        ergebnis[n] = q.copy()
    return ergebnis


def tarn_schweif(grund):
    """Tail of the camouflage pose: down to the ground, a short way back, then forward along the outside of the right flank"""
    if not TARN_SCHWEIF:
        richtungen = [(-0.55, 0.0, -0.83), (-0.75, -0.2, -0.62)]
        for kurs in (200, 235, 268, 292, 315, 338, 352, 0):
            k = math.radians(kurs)
            richtungen.append((math.cos(k), math.sin(k), -0.03))
        TARN_SCHWEIF.update(kette_ausrichten(grund, SCHWEIF, richtungen))
    return dict(TARN_SCHWEIF)


def tarn_pose(atem=0.0, wind=None):
    p = {"_ort": {"becken": (0.0, 0.0, -0.33 + atem * 0.004)}, "_groesse": {}}
    p["wirbel_01"] = (atem * 0.8, 0, 0)
    p["wirbel_02"] = (atem * 0.5, 0, 0)
    p["hals"] = (58, 0, 0)
    p["kopf"] = (-46, 0, 0)
    p["kiefer"] = (0, 0, 0)
    for s in ("l", "r"):
        p["ohr_%s_01" % s] = (-28, 0, 0)
        p["ohr_%s_02" % s] = (-12, 0, 0)
    for b in ("vl", "vr"):
        p["bein_%s_oben" % b] = (4, 0, 0)
        p["bein_%s_unten" % b] = (-88, 0, 0)
        p["bein_%s_mittelfuss" % b] = (52, 0, 0)
        p["bein_%s_pfote" % b] = (20, 0, 0)
    for b, nach_aussen in (("hl", 1), ("hr", -1)):
        p["bein_%s_oben" % b] = (15, 6 * nach_aussen, 0)
        p["bein_%s_unten" % b] = (52, 0, 0)
        p["bein_%s_mittelfuss" % b] = (-120, 0, 0)
        p["bein_%s_pfote" % b] = (10, 0, 0)
    # Crest fanned to all sides: alternating left/right, the front ones forward, the back ones backward; bigger
    for i, n in enumerate(KAMM):
        t = i / (len(KAMM) - 1)
        seite = (1 if i % 2 == 0 else -1) * (62 + 24 * math.sin(math.pi * t))
        # Wind: every leaf sways on its own beat (two beats per loop, so the loop closes)
        wehen = math.sin(2 * math.pi * (wind * 2 + i * 0.37)) if wind is not None else 0.0
        zittern = math.sin(2 * math.pi * (wind * 4 + i * 0.61)) if wind is not None else 0.0
        p[n] = (-45 + 90 * t + 7 * wehen, seite + 9 * wehen + 3 * zittern, 0)
        p["_groesse"][n] = 1.6
    # Tail down to the ground, then laid round the right side of the body
    p.update(tarn_schweif({k: v for k, v in p.items()}))
    return p


def stehen():
    return {"_ort": {}, "_groesse": {}}


def idle(f):
    t = f / FPS
    p = {"_ort": {"becken": (0.0, 0.0, 0.004 * math.sin(2 * math.pi * t / 2.0))}, "_groesse": {}}
    atem = math.sin(2 * math.pi * t / 2.0)
    p["wirbel_01"] = (0.8 * atem, 0, 0)
    p["wirbel_02"] = (-0.6 * atem, 0, 0)
    p["hals"] = (-1.5 * atem, 0, 0)
    p["kopf"] = (0, 0, 5 * math.sin(2 * math.pi * t / 4.0))
    for i, n in enumerate(SCHWEIF):
        phase = 2 * math.pi * t / 4.0 - i * 0.38
        p[n] = (1.0 * math.sin(phase * 2), 0, (1.6 + i * 0.25) * math.sin(phase))
    # Ear twitches: left at 1.4 s, right at 3.0 s
    for s, zeit in (("l", 1.4), ("r", 3.0)):
        zuck = math.exp(-((t - zeit) / 0.07) ** 2)
        p["ohr_%s_01" % s] = (-6 * zuck, 18 * zuck * (1 if s == "l" else -1), 0)
    return p


def erwachen(f):
    t = f / 30.0
    tarn = tarn_pose()
    ziel = stehen()
    gruppen = {"kopf": (0.0, 0.35), "kamm": (0.0, 0.4), "koerper": (0.2, 0.7), "schweif": (0.4, 0.6)}

    def gruppe(n):
        if n in ("hals", "kopf", "kiefer") or n.startswith("ohr_"):
            return "kopf"
        if n in KAMM:
            return "kamm"
        if n in SCHWEIF:
            return "schweif"
        return "koerper"
    p = {"_ort": {}, "_groesse": {}}
    namen = set(tarn) - {"_ort", "_groesse"}
    for n in namen:
        start, dauer = gruppen[gruppe(n)]
        a = weich((t - start) / dauer)
        einzel = mischen({n: tarn[n], "_groesse": {n: tarn["_groesse"].get(n, 1.0)}}, {}, a)
        p[n] = einzel[n]
        if n in tarn["_groesse"]:
            p["_groesse"][n] = einzel["_groesse"][n]
    start, dauer = gruppen["koerper"]
    a = weich((t - start) / dauer)
    p["_ort"]["becken"] = tuple(x * (1 - a) for x in tarn["_ort"]["becken"])
    # The head lifts a little higher than standing on the way up
    p["hals"] = tuple(x + y for x, y in zip(p.get("hals", (0, 0, 0)), (-12 * math.sin(math.pi * weich(t / 0.5)) * (1 if t < 0.5 else 0), 0, 0)))
    return p


def sprung(f):
    schluessel = [
        (0, {}),
        (5, {"_ort": {"becken": (-0.04, 0, -0.1)}, "hals": (10, 0, 0), "ohr_l_01": (-20, 0, 0), "ohr_r_01": (-20, 0, 0),
             "bein_vl_oben": (10, 0, 0), "bein_vr_oben": (10, 0, 0), "bein_vl_unten": (-15, 0, 0), "bein_vr_unten": (-15, 0, 0),
             "bein_hl_oben": (-15, 0, 0), "bein_hr_oben": (-15, 0, 0), "bein_hl_unten": (25, 0, 0), "bein_hr_unten": (25, 0, 0),
             "bein_hl_mittelfuss": (-20, 0, 0), "bein_hr_mittelfuss": (-20, 0, 0), "schweif_01": (-10, 0, 0)}),
        (10, {"_ort": {"wurzel": (0.2, 0, 0), "becken": (0.0, 0, 0.18)}, "wirbel_01": (-5, 0, 0), "hals": (-10, 0, 0),
              "kiefer": (25, 0, 0), "ohr_l_01": (-25, 0, 0), "ohr_r_01": (-25, 0, 0),
              "bein_vl_oben": (-55, 0, 0), "bein_vr_oben": (-50, 0, 0), "bein_vl_unten": (-10, 0, 0), "bein_vr_unten": (-12, 0, 0),
              "bein_vl_mittelfuss": (-20, 0, 0), "bein_vr_mittelfuss": (-20, 0, 0),
              "bein_hl_oben": (45, 0, 0), "bein_hr_oben": (42, 0, 0), "bein_hl_unten": (-10, 0, 0), "bein_hr_unten": (-10, 0, 0),
              "bein_hl_mittelfuss": (20, 0, 0), "bein_hr_mittelfuss": (20, 0, 0), "schweif_01": (-15, 0, 0), "schweif_02": (-5, 0, 0)}),
        (16, {"_ort": {"wurzel": (0.4, 0, 0), "becken": (0.0, 0, 0.05)}, "hals": (8, 0, 0), "kiefer": (25, 0, 0),
              "bein_vl_oben": (-20, 0, 0), "bein_vr_oben": (-18, 0, 0), "bein_vl_unten": (-20, 0, 0), "bein_vr_unten": (-20, 0, 0),
              "bein_vl_mittelfuss": (10, 0, 0), "bein_vr_mittelfuss": (10, 0, 0),
              "bein_hl_oben": (30, 0, 0), "bein_hr_oben": (28, 0, 0), "schweif_01": (-8, 0, 0)}),
        (20, {"_ort": {"wurzel": (0.45, 0, 0), "becken": (0.0, 0, -0.06)}, "hals": (5, 0, 0), "kiefer": (8, 0, 0),
              "bein_vl_unten": (10, 0, 0), "bein_vr_unten": (10, 0, 0),
              "bein_hl_oben": (-20, 0, 0), "bein_hr_oben": (-20, 0, 0), "bein_hl_unten": (20, 0, 0), "bein_hr_unten": (20, 0, 0)}),
        (24, {"_ort": {"wurzel": (0.45, 0, 0)}}),
    ]
    for (fa, a), (fb, b) in zip(schluessel, schluessel[1:]):
        if fa <= f <= fb:
            a = dict(a, _ort=a.get("_ort", {}), _groesse={})
            b = dict(b, _ort=b.get("_ort", {}), _groesse={})
            return mischen(a, b, weich((f - fa) / (fb - fa)))
    return schluessel[-1][1]


def pfote_flach(p, bein):
    """Keeps the paw flat on the ground: the toes turn back by everything the leg above them turned"""
    kette = ["becken"] + (["wirbel_01", "wirbel_02", "wirbel_03"] if bein[0] == "v" else [])
    kette += ["bein_%s_%s" % (bein, n) for n in ("oben", "unten", "mittelfuss")]
    return (-sum(p.get(n, (0, 0, 0))[0] for n in kette), 0, 0)


def bein_zyklus(p, bein, q, stand, amp, hub):
    """One leg of a cat gait at phase q (0..1, the paw touches down at 0). Front legs: the forearm stays steep and the
    wrist folds the paw right up behind it, then reaches far forward before the paw is set down soft and flat.
    Hind legs: knee and hock fold, the paw is drawn up and swung forward close under the body."""
    vorn = bein[0] == "v"
    if q < stand:
        # On the ground: the leg carries the body over the paw, which stays flat; at the end the heel lifts
        s = q / stand
        winkel = -amp + 2 * amp * s
        ende = weich((s - 0.7) / 0.3)
        if vorn:
            p["bein_%s_oben" % bein] = (winkel, 0, 0)
            p["bein_%s_unten" % bein] = (-0.25 * winkel, 0, 0)
            p["bein_%s_mittelfuss" % bein] = (18 * ende, 0, 0)
        else:
            p["bein_%s_oben" % bein] = (winkel, 0, 0)
            p["bein_%s_unten" % bein] = (6 + 6 * ende, 0, 0)
            p["bein_%s_mittelfuss" % bein] = (-6 - 10 * ende, 0, 0)
        p["bein_%s_pfote" % bein] = pfote_flach(p, bein)
        return 0.0
    # In the air: lift and fold early, reach late
    s = (q - stand) / (1 - stand)
    winkel = amp - 2 * amp * weich(s)
    falten = math.sin(math.pi * min(1.0, s / 0.8)) ** 1.5
    greifen = weich((s - 0.55) / 0.45)
    if vorn:
        p["bein_%s_oben" % bein] = (winkel - 8 * greifen * (1 - greifen) * 4 * hub, 0, 0)
        p["bein_%s_unten" % bein] = (-14 * falten * hub - 10 * greifen * hub, 0, 0)
        p["bein_%s_mittelfuss" % bein] = (75 * falten * hub - 8 * greifen * hub, 0, 0)
        p["bein_%s_pfote" % bein] = (35 * falten * hub, 0, 0)
    else:
        p["bein_%s_oben" % bein] = (winkel - 6 * falten * hub, 0, 0)
        p["bein_%s_unten" % bein] = (6 + 42 * falten * hub, 0, 0)
        p["bein_%s_mittelfuss" % bein] = (-6 - 55 * falten * hub, 0, 0)
        p["bein_%s_pfote" % bein] = (30 * falten * hub, 0, 0)
    return falten


def pfoten_nachziehen(p, phasen, t, stand):
    """Once the body sways, the paws on the ground are laid flat again, and those about to touch down get ready"""
    for bein, phase in phasen.items():
        q = (t - phase) % 1.0
        if q < stand:
            p["bein_%s_pfote" % bein] = pfote_flach(p, bein)
        else:
            greifen = weich(((q - stand) / (1 - stand) - 0.55) / 0.45)
            if greifen > 0.0:
                p["bein_%s_pfote" % bein] = tuple(a + (b - a) * greifen for a, b in zip(p["bein_%s_pfote" % bein], pfote_flach(p, bein)))


def kopf_ruhig(p, tief):
    """The head stays level and steady while the body sways under it, as a hunting cat's does"""
    beuge = sum(p.get(n, (0, 0, 0))[0] for n in ("wirbel_01", "wirbel_02", "wirbel_03")) + p.get("becken", (0, 0, 0))[0]
    wende = sum(p.get(n, (0, 0, 0))[2] for n in ("wirbel_01", "wirbel_02", "wirbel_03"))
    seite = sum(p.get(n, (0, 0, 0))[1] for n in ("wirbel_01", "wirbel_02", "wirbel_03")) + p.get("becken", (0, 0, 0))[1]
    p["hals"] = (tief - 0.85 * beuge, -0.6 * seite, -0.85 * wende)
    p["kopf"] = (-tief * 0.6, 0, 0)


def schweif_welle(p, t, amp, hebung, verzug=0.5):
    """Low, gently S-curved tail with its tip turned up; a wave runs down it a little behind the body"""
    for i, n in enumerate(SCHWEIF):
        anteil = i / (len(SCHWEIF) - 1)
        phase = 2 * math.pi * t - i * verzug
        # Base hangs down, the last third curls up
        grund = 7 if i < 2 else (-2 if i < 6 else -9)
        p[n] = (grund + hebung * math.sin(2 * phase) * (0.3 + anteil), 0, amp * (0.4 + anteil) * math.sin(phase))


def laufen(f):
    """Walk: lateral sequence (left hind, left fore, right hind, right fore), the hips and shoulders roll over
    the paws on the ground, the spine snakes a little from side to side, the head glides level"""
    t = f / 30.0
    p = {"_ort": {}, "_groesse": {}}
    phasen = {"hl": 0.0, "vl": 0.22, "hr": 0.5, "vr": 0.72}
    for bein, phase in phasen.items():
        bein_zyklus(p, bein, (t - phase) % 1.0, 0.62, 19 if bein[0] == "v" else 21, 1.0)
    welle = 2 * math.pi * t
    # The hip drops a little on the side whose hind paw is in the air, the shoulders the same with the front
    p["becken"] = (1.2 * math.sin(2 * welle), 3.5 * math.sin(welle), 0)
    p["wirbel_01"] = (-0.8 * math.sin(2 * welle), 0, 4.0 * math.sin(welle + 0.3))
    p["wirbel_02"] = (0.4 * math.sin(2 * welle + 0.5), 0, -2.5 * math.sin(welle + 0.9))
    p["wirbel_03"] = (0.6 * math.sin(2 * welle + 1.0), -2.5 * math.sin(welle + 1.4), -3.0 * math.sin(welle + 1.4))
    p["_ort"]["becken"] = (0.004 * math.sin(2 * welle), 0.008 * math.sin(welle), 0.007 * math.cos(2 * welle + 0.4))
    kopf_ruhig(p, 9)
    schweif_welle(p, t, 3.0, 1.2)
    pfoten_nachziehen(p, phasen, t, 0.62)
    for s, k in (("l", 1), ("r", -1)):
        p["ohr_%s_01" % s] = (-4, 0, 0)
    return p


def rennen(f):
    """Run: a cat's rotary gallop. The spine coils (hind paws reach far forward under the belly) and uncoils (the
    body stretches long in the air), the shoulders rise and fall, the head stays steady on target"""
    rahmen = 16
    t = f / float(rahmen)
    p = {"_ort": {}, "_groesse": {}}
    phasen = {"hl": 0.0, "hr": 0.09, "vr": 0.42, "vl": 0.52}
    for bein, phase in phasen.items():
        bein_zyklus(p, bein, (t - phase) % 1.0, 0.34, 40 if bein[0] == "v" else 44, 1.15)
    # Coiled when the hind paws come down (t = 0), stretched in the air before the front paws land
    beugen = math.cos(2 * math.pi * (t + 0.04))
    p["becken"] = (-9 * beugen, 0, 0)
    p["wirbel_01"] = (-7 * beugen, 0, 0)
    p["wirbel_02"] = (5 * beugen, 0, 0)
    p["wirbel_03"] = (9 * beugen, 0, 0)
    # Flies after the push of the hind legs, lowest when the front legs take the weight
    p["_ort"]["becken"] = (0.03 * beugen, 0, 0.045 * math.sin(2 * math.pi * (t - 0.12)))
    kopf_ruhig(p, 6)
    pfoten_nachziehen(p, phasen, t, 0.34)
    for i, n in enumerate(SCHWEIF):
        anteil = i / (len(SCHWEIF) - 1)
        # Streams straight back as a rudder, whipping with the spine
        p[n] = (-2 + 6 * anteil * math.sin(2 * math.pi * t - i * 0.5), 0, 2.0 * math.sin(2 * math.pi * t - i * 0.4))
    for s in ("l", "r"):
        p["ohr_%s_01" % s] = (-14, 0, 0)
    return p


# ---------------------------------------------------------------- Main

def bild(name, pose, mitte_=(-0.1, 0.0, 0.45), groesse=1.9, ansichten=("seite", "spiel")):
    pose_anwenden(pose)
    bpy.context.view_layer.update()
    if "seite" in ansichten:
        vorschau.rendern(name + "_seite", (0.0, -1.0, 0.05), mitte_, groesse)
    if "spiel" in ansichten:
        vorschau.rendern(name + "_spielkamera", (0.45, -0.8, math.tan(math.radians(50))), mitte_, groesse)


def main():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(ORDNER, "arbeit", "03_kamm_augen.blend"))
    arm = bpy.data.objects["Rankenluchs_Skelett"]
    ARM[0] = arm
    bpy.context.scene.render.fps = FPS
    for name, farbe in (("Kamm", (0.15, 0.55, 0.12, 1.0)), ("Augen", (1.0, 0.6, 0.1, 1.0))):
        mat = bpy.data.materials.get(name)
        if mat:
            mat.diffuse_color = farbe

    aktion("Stehen_Idle", 120, idle, schritt=3, schleife=True)
    aktion("Tarn_Ruhe", 60, lambda f: tarn_pose(math.sin(2 * math.pi * f / 60), f / 60.0), schritt=3, schleife=True)
    aktion("Erwachen", 30, erwachen, schritt=2)
    aktion("Sprung_Angriff", 24, sprung, schritt=1)
    aktion("Laufen", 30, laufen, schritt=1, schleife=True)
    aktion("Rennen", 16, rennen, schritt=1, schleife=True)
    arm.animation_data.action = None
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ORDNER, "arbeit", "04_animationen.blend"))

    # Key poses as pictures
    bild("04_tarn", tarn_pose(), mitte_=(-0.1, 0.0, 0.3))
    vorschau.rendern("04_tarn_oben", (0.0, 0.0, 1.0), (-0.1, 0.0, 0.3), 1.9)
    bild("04_erwachen_halb", erwachen(12), ansichten=("seite",))
    bild("04_sprung_flug", sprung(10), mitte_=(0.1, 0.0, 0.5), ansichten=("seite",))
    for f in (0, 8, 15, 23):
        bild("04_laufen_%02d" % f, laufen(f), ansichten=("seite",))
    for f in (0, 4, 8, 12):
        bild("04_rennen_%02d" % f, rennen(f), ansichten=("seite",))
    pose_anwenden(stehen())


main()
