"""Moderglocke, step 3: the animations as separate actions (30 frames per second, in place; the actor moves it).

It hovers: the pelvis carries the whole body SCHWEBE metres above where the model stands. The bell (cap) pulses like
a jellyfish, the body rises a moment after each beat, legs, arms and filaments trail behind.

  Schweben_Idle   6 s loop   hovers on the spot, two bell beats, limbs and filaments drifting, head listening
  Schweben_Vor    3 s loop   drifts forward, leaning, stronger beats, everything trailing
  Gehen           2 s loop   walks on the ground like a long giant, heavy, the long arms swinging
  Lauern          5 s loop   by day: squats on the ground under its cap like a plain mushroom, breathing, filaments drawn in
  Erwachen        2.0 s      rises from the squat into the hover, filaments let down
  Nesselfaeden    1.6 s      leans back, then lashes the front filaments forward (hit at 0.75 s)
  Giftregen       2.6 s      rises, spreads the bell flat and shakes the spores out (rain from 0.9 s to 1.9 s)
  Glockenschlag   3.2 s      rises, opens wide, drops over its prey (down at 1.15 s), clasps it trembling, lets go at 2.5 s
  Treffer         0.6 s      flinches
  Sporenbersten   1.3 s      hit on the cap: the bell snaps shut and bursts open (burst at 0.35 s)
  Tod             2.6 s      the beat fails, it sinks, folds and tips forward onto its cap's rim

    blender -b --factory-startup -P Tools/Nachtwesen/moderglocke_clips.py [-- <Aktion> ...]
"""
import math
import os
import sys

from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bau  # noqa: E402
import gemein  # noqa: E402
from bau import Pose, huegel, rampe, unruhe, weich, welle  # noqa: E402

NAME = "Moderglocke"
RAND, FAEDEN, GLIEDER = 8, 12, 4
SCHWEBE = 0.5
MITTE = (0.0, 0.0, 1.55)
ARM = [None]
FADEN_WINKEL = {}     # filament -> angle round the cap (radians)


def schlag(phase):
    """The bell's beat over one period (0..1): shuts fast, holds a breath, opens slowly"""
    phase %= 1.0
    return huegel(phase, 0.0, 0.14, 0.2, 0.78)


def hut(p, falten, kippen=(0.0, 0.0), flattern=0.0, t=0.0):
    """Folds the rim down by falten degrees (a number or one per segment; negative lifts it) and tips the whole
    cap (beuge, seite); flattern shakes the segments one after another"""
    p.dazu("hut", kippen[0], kippen[1])
    for k in range(RAND):
        a = 2 * math.pi * k / RAND
        grad = falten[k] if isinstance(falten, (list, tuple)) else falten
        grad += flattern * math.sin(2 * math.pi * t * 9.0 + k * 2.4)
        p.winkel["hutrand_%d" % (k + 1)] = Matrix.Rotation(math.radians(grad), 3, Vector((-math.sin(a), math.cos(a), 0.0)))


def faeden(p, bei, einzug=1.0):
    """Sets every filament joint: bei(k, g, a) -> (beuge, seite, einwaerts) in degrees per joint; einzug < 1 draws
    the filaments in (scale of the first joint)"""
    for k in range(FAEDEN):
        a = FADEN_WINKEL[k]
        achse = Vector((-math.sin(a), math.cos(a), 0.0))
        for g in range(GLIEDER):
            beuge, seite, ein = bei(k, g, a)
            p.winkel["faden_%d_%d" % (k + 1, g + 1)] = Matrix.Rotation(math.radians(beuge), 3, "Y") @ \
                Matrix.Rotation(math.radians(seite), 3, "X") @ Matrix.Rotation(math.radians(ein), 3, achse)
        if einzug < 0.999:
            p.skala["faden_%d_1" % (k + 1)] = (einzug, einzug, einzug)


def arme(p, heben=(0.0, 0.0), vor=(0.0, 0.0), ellbogen=(0.0, 0.0), hand=(0.0, 0.0)):
    """Left and right: lifted sideways, swung forward, elbow bent (forearm forward), wrist curled"""
    for i, (n, s) in enumerate((("l", 1), ("r", -1))):
        p.dazu("oberarm_" + n, beuge=-vor[i], seite=heben[i] * s)
        p.dazu("unterarm_" + n, beuge=-ellbogen[i])
        p.dazu("hand_" + n, beuge=-hand[i])


def beine(p, huefte=(0.0, 0.0), knie=(0.0, 0.0), spitze=(50.0, 50.0), spreizen=(0.0, 0.0)):
    """Left and right: thigh forward, knee bent, toes pointed down, thigh out to the side"""
    for i, (n, s) in enumerate((("l", 1), ("r", -1))):
        p.dazu("oberschenkel_" + n, beuge=-huefte[i], seite=spreizen[i] * s)
        p.dazu("unterschenkel_" + n, beuge=knie[i])
        p.dazu("fuss_" + n, beuge=spitze[i])


def schweben(t, schleife, takt, kraft=1.0, neigung=0.0, hoehe=SCHWEBE):
    """The hover everything else builds on: bell beat every <takt> seconds, the body lifting after it, limbs trailing"""
    p = Pose()
    s0 = schlag(t / takt)
    s1 = schlag((t - 0.18) / takt)          # the body answers late
    s2 = schlag((t - 0.42) / takt)          # the limbs later still
    hub = 0.07 * kraft * (s1 - 0.35) + 0.015 * unruhe(t, schleife, 0.3)
    p.schieben("becken", x=0.012 * unruhe(t, schleife, 1.1), y=0.02 * unruhe(t, schleife, 2.3), z=hoehe + hub)
    p.dazu("becken", beuge=neigung + 1.5 * unruhe(t, schleife, 3.1), seite=1.5 * unruhe(t, schleife, 4.4))
    atem = welle(t, schleife / max(1, round(schleife / 3.0)))
    p.dazu("bauch", beuge=1.2 * atem + neigung * 0.25)
    p.dazu("brust", beuge=-1.6 * atem + neigung * 0.2)
    p.dazu("hals", beuge=-neigung * 0.8 + 2.5 * unruhe(t, schleife, 5.2), seite=3.0 * unruhe(t, schleife, 6.7))
    hut(p, 4 + 26 * kraft * s0, kippen=(2.0 * unruhe(t, schleife, 7.7) - neigung * 0.3, 2.0 * unruhe(t, schleife, 8.3)))
    # Limbs hang, drawn up a little with each beat and swinging out after it
    zug = kraft * (s2 - 0.3)
    beine(p,
          huefte=(8 + 9 * zug + 3 * unruhe(t, schleife, 9.1) - neigung * 0.9, 5 + 7 * zug + 3 * unruhe(t, schleife, 10.4) - neigung * 1.1),
          knie=(14 + 16 * zug + 4 * unruhe(t, schleife, 11.2), 20 + 12 * zug + 4 * unruhe(t, schleife, 12.9)),
          spitze=(52 + 6 * zug, 56 + 5 * zug),
          spreizen=(2 + 3 * unruhe(t, schleife, 13.3), 1 + 3 * unruhe(t, schleife, 14.6)))
    arme(p,
         heben=(6 + 9 * zug + 3 * unruhe(t, schleife, 15.5), 4 + 8 * zug + 3 * unruhe(t, schleife, 16.2)),
         vor=(4 + 4 * unruhe(t, schleife, 17.1) - neigung * 1.0, 2 + 4 * unruhe(t, schleife, 18.8) - neigung * 1.2),
         ellbogen=(14 + 8 * zug + 4 * unruhe(t, schleife, 19.4), 18 + 7 * zug + 4 * unruhe(t, schleife, 20.3)),
         hand=(10 + 8 * unruhe(t, schleife, 21.7), 12 + 8 * unruhe(t, schleife, 22.1)))

    def faden(k, g, a):
        spaet = schlag((t - 0.12 - 0.11 * g) / takt)
        # hangs plumb although the rim folds (first joint), is drawn in by the beat and swings out after it
        ein = (-(4 + 22 * kraft * s0) * 0.8 if g == 0 else 0.0) + kraft * (9 * spaet - 3.5) * (0.6 + 0.25 * g)
        ein += 3.0 * unruhe(t - 0.15 * g, schleife, 30 + k * 1.7)
        seitlich = 3.5 * unruhe(t - 0.2 * g, schleife, 50 + k * 2.3)
        nach = neigung * (0.35 + 0.2 * g)
        return (nach + seitlich * math.sin(a), seitlich * math.cos(a), ein)
    faeden(p, faden)
    return p


def schweben_idle(t):
    return schweben(t, 6.0, 3.0)


def schweben_vor(t):
    return schweben(t, 3.0, 1.5, kraft=1.25, neigung=13.0)


def gehen(t):
    """Walks on the ground like a long giant: slow strides of 1.1 m, the weight dropping onto each planted foot, the
    body swaying over the standing leg, the long arms swinging against the legs with elbows and hands trailing, the
    cap nodding a moment late. The actor moves 0.89 m/s at the clip's own pace."""
    dauer, schritt = 2.0, 1.1
    ph = (t / dauer) % 1.0
    p = Pose()

    def fuss(f):
        f %= 1.0
        if f < 0.62:                                   # planted, carried back under the body
            return schritt * (0.5 - f / 0.62), 0.0, 0.0
        u = (f - 0.62) / 0.38                          # lifted and swung forward
        return schritt * (-0.5 + weich(u)), 0.22 * math.sin(math.pi * u), math.sin(math.pi * u)

    sinken = math.cos(4 * math.pi * (ph - 0.07))       # lowest just after each foot lands
    p.schieben("becken", x=0.03 * math.sin(4 * math.pi * ph), y=0.075 * math.cos(2 * math.pi * (ph - 0.31)),
               z=-0.2 - 0.045 * sinken)
    p.dazu("becken", beuge=8 + 2.5 * sinken, seite=-3.5 * math.cos(2 * math.pi * (ph - 0.31)), wende=7 * math.cos(2 * math.pi * ph))
    p.dazu("bauch", beuge=5 + 1.5 * sinken, wende=-5 * math.cos(2 * math.pi * ph))
    p.dazu("brust", beuge=5, wende=-7 * math.cos(2 * math.pi * ph), seite=2.5 * math.cos(2 * math.pi * (ph - 0.31)))
    p.dazu("hals", beuge=-9 + 3 * math.cos(4 * math.pi * (ph - 0.16)), seite=2 * math.cos(2 * math.pi * (ph - 0.4)))
    hut(p, 9 + 7 * math.cos(4 * math.pi * (ph - 0.2)), kippen=(3.5 * math.cos(4 * math.pi * (ph - 0.2)), 2.5 * math.cos(2 * math.pi * (ph - 0.45))))
    for i, (n, s) in enumerate((("l", 1), ("r", -1))):
        x, z, luft = fuss(ph + 0.5 * i)
        p.greifen("oberschenkel_" + n, "unterschenkel_" + n, Vector((x, 0.15 * s, 0.13 + z)), (1.0, 0.25 * s, 0.1), "fuss_" + n,
                  (18 * luft, 0, 0))
    # the arm swings forward with the opposite leg; elbow and hand follow late
    schwung = (-math.cos(2 * math.pi * (ph - 0.06)), math.cos(2 * math.pi * (ph - 0.06)))
    spaet = (-math.cos(2 * math.pi * (ph - 0.16)), math.cos(2 * math.pi * (ph - 0.16)))
    hand = (-math.cos(2 * math.pi * (ph - 0.26)), math.cos(2 * math.pi * (ph - 0.26)))
    arme(p, heben=(5, 5), vor=(4 + 27 * schwung[0], 4 + 27 * schwung[1]),
         ellbogen=(16 + 13 * spaet[0], 16 + 13 * spaet[1]), hand=(12 + 16 * hand[0], 12 + 16 * hand[1]))

    def faden(k, g, a):
        vor = 5.0 * math.cos(4 * math.pi * (ph - 0.22 - 0.07 * g)) * (0.5 + 0.3 * g)
        quer = 4.0 * math.cos(2 * math.pi * (ph - 0.4 - 0.08 * g)) * (0.5 + 0.3 * g)
        ein = -(9 + 7 * math.cos(4 * math.pi * (ph - 0.2))) * 0.8 if g == 0 else 0.0
        return (vor + 2.0 * unruhe(t - 0.1 * g, dauer, 30 + k * 1.7), quer, ein)
    faeden(p, faden)
    return p


def hocke(t, schleife=5.0, tiefe=1.0):
    """By day: kneels upright under its cap like a plain mushroom, sitting on its heels, the arms laid close, the
    rim folded down over it, filaments drawn in"""
    p = Pose()
    atem = welle(t, schleife / 2) if schleife else 0.0
    p.schieben("becken", x=-0.05 * tiefe, z=(-1.10 + 0.012 * atem) * tiefe)
    p.dazu("becken", beuge=5 * tiefe)
    p.dazu("bauch", beuge=(5 + 1.5 * atem) * tiefe)
    p.dazu("brust", beuge=(4 - 2.0 * atem) * tiefe)
    p.dazu("hals", beuge=-12 * tiefe + (1.5 * unruhe(t, schleife, 1.0) if schleife else 0.0))
    hut(p, 20 * tiefe + (1.5 * atem if schleife else 0.0))
    for n, s in (("l", 1), ("r", -1)):
        p.greifen("oberschenkel_" + n, "unterschenkel_" + n, FUSS_HOCKE(s), (1.0, 0.35 * s, -0.1), "fuss_" + n, (150, 0, 0))
    zucken = huegel((t / schleife) % 1.0, 0.62, 0.64, 0.66, 0.7) if schleife else 0.0
    arme(p, heben=(-9 * tiefe, -9 * tiefe), vor=(8 * tiefe, 10 * tiefe), ellbogen=(62 * tiefe, 56 * tiefe),
         hand=(20 * tiefe + 14 * zucken, 24 * tiefe))
    faeden(p, lambda k, g, a: (0.0, 0.0, 14.0 * tiefe if g == 0 else 6.0 * tiefe), einzug=1.0 - 0.86 * tiefe)
    return p


def FUSS_HOCKE(s):
    return Vector((-0.27, 0.16 * s, 0.09))


def lauern(t):
    return hocke(t, 5.0)


def mischen(a, b, x):
    """Blends two poses (angles, offsets, scales; IK jobs of a are kept while x < 1)"""
    p = Pose()
    for n in set(a.winkel) | set(b.winkel):
        wa, wb = a.winkel.get(n, (0.0, 0.0, 0.0)), b.winkel.get(n, (0.0, 0.0, 0.0))
        if isinstance(wa, Matrix) or isinstance(wb, Matrix):
            qa = (wa if isinstance(wa, Matrix) else bau._welt(wa)).to_quaternion()
            qb = (wb if isinstance(wb, Matrix) else bau._welt(wb)).to_quaternion()
            p.winkel[n] = qa.slerp(qb, x).to_matrix()
        else:
            p.winkel[n] = tuple(wa[i] + (wb[i] - wa[i]) * x for i in range(3))
    for n in set(a.versatz) | set(b.versatz):
        p.versatz[n] = a.versatz.get(n, Vector()).lerp(b.versatz.get(n, Vector()), x)
    for n in set(a.skala) | set(b.skala):
        sa, sb = a.skala.get(n, (1, 1, 1)), b.skala.get(n, (1, 1, 1))
        p.skala[n] = tuple(sa[i] + (sb[i] - sa[i]) * x for i in range(3))
    return p


def erwachen(t):
    # The legs push it up off the ground (feet stay planted until 0.9 s), then it drifts up into the hover
    hoch = rampe(t, 0.15, 1.5)
    p = mischen(hocke(0.0, 0.0), schweben(t + 4.0, 6.0, 3.0), hoch)
    if t < 0.95:
        for n, s in (("l", 1), ("r", -1)):
            p.greifen("oberschenkel_" + n, "unterschenkel_" + n, FUSS_HOCKE(s) + Vector((0.2 * hoch, 0.0, 0.3 * rampe(t, 0.7, 0.95))),
                      (1.0, 0.35 * s, -0.1), "fuss_" + n, (150 - 100 * rampe(t, 0.3, 0.95), 0, 0))
    # a first wide beat as it comes free
    oeffnen = huegel(t, 0.5, 0.9, 1.0, 1.5)
    for k in range(RAND):
        a = 2 * math.pi * k / RAND
        alt = p.winkel["hutrand_%d" % (k + 1)]
        p.winkel["hutrand_%d" % (k + 1)] = Matrix.Rotation(math.radians(-16 * oeffnen), 3, Vector((-math.sin(a), math.cos(a), 0.0))) @ alt
    return p


def mit_grund(t, vor=4.0):
    """The hover of the idle at loop time vor + t, as the ground attacks start from and return to"""
    return schweben(vor + t, 6.0, 3.0)


def nesselfaeden(t):
    p = mit_grund(t)
    aus = huegel(t, 0.0, 0.5, 0.58, 0.9)       # winds up: leans back
    hieb = huegel(t, 0.58, 0.75, 0.85, 1.5)    # lashes forward
    p.dazu("becken", beuge=-14 * aus + 20 * hieb)
    p.schieben("becken", x=-0.1 * aus + 0.22 * hieb, z=0.08 * aus - 0.06 * hieb)
    p.dazu("hals", beuge=-8 * aus + 6 * hieb)
    hut(p, -10 * aus + 22 * hieb, kippen=(-10 * aus + 16 * hieb, 0))
    arme(p, heben=(30 * aus + 8 * hieb, 30 * aus + 8 * hieb), vor=(-25 * aus + 50 * hieb, -20 * aus + 55 * hieb),
         ellbogen=(30 * aus - 10 * hieb, 30 * aus - 10 * hieb))
    beine(p, huefte=(-12 * aus + 14 * hieb, -8 * aus + 18 * hieb), knie=(18 * aus, 14 * aus), spitze=(0, 0))

    def faden(k, g, a):
        vorn = max(0.0, math.cos(a)) ** 0.7        # the filaments at the front do the lashing
        spaet_aus = huegel(t - 0.03 * g, 0.0, 0.5, 0.58, 0.9)
        spaet_hieb = huegel(t - 0.035 * g, 0.58, 0.74, 0.8, 1.45)
        nach = 14 * spaet_aus * (0.5 + 0.5 * vorn) - (30 + 9 * g) * spaet_hieb * (0.25 + 0.75 * vorn)
        # after the hit the tips curl back on themselves
        nach += 16 * huegel(t - 0.05 * g, 0.8, 0.95, 1.0, 1.5) * (g / 3.0) * vorn
        return (nach, 0.0, -6 * spaet_hieb * vorn)
    grund = {n: p.winkel[n] for n in p.winkel if n.startswith("faden_")}
    faeden(p, faden)
    for n, alt in grund.items():
        p.winkel[n] = p.winkel[n] @ alt
    return p


def giftregen(t):
    p = mit_grund(t)
    hoch = huegel(t, 0.0, 0.8, 1.9, 2.6)
    schuetteln = huegel(t, 0.8, 0.95, 1.8, 2.05)
    p.schieben("becken", z=0.4 * hoch + 0.012 * schuetteln * math.sin(2 * math.pi * t * 11))
    p.dazu("becken", beuge=9 * hoch)
    p.dazu("hals", beuge=14 * hoch, seite=3.5 * schuetteln * math.sin(2 * math.pi * t * 7.3))
    hut(p, -20 * hoch, kippen=(10 * hoch, 4 * schuetteln * math.sin(2 * math.pi * t * 5.1)), flattern=7 * schuetteln, t=t)
    arme(p, heben=(48 * hoch, 44 * hoch), vor=(-6 * hoch, -8 * hoch), ellbogen=(26 * hoch, 22 * hoch), hand=(-30 * hoch, -30 * hoch))
    beine(p, huefte=(10 * hoch, -4 * hoch), knie=(26 * hoch, 10 * hoch), spitze=(0, 0), spreizen=(6 * hoch, 4 * hoch))
    grund = {n: p.winkel[n] for n in p.winkel if n.startswith("faden_")}
    faeden(p, lambda k, g, a: (0.0, 0.0, 16 * hoch * (1 if g == 0 else 0) - 5 * hoch
                                 + 5 * schuetteln * math.sin(2 * math.pi * t * 9 + k * 1.9 - g * 0.9)))
    for n, alt in grund.items():
        p.winkel[n] = p.winkel[n] @ alt
    return p


def glockenschlag(t):
    p = mit_grund(t)
    hoch = huegel(t, 0.0, 0.7, 0.9, 1.15)
    unten = huegel(t, 0.95, 1.15, 2.45, 3.1)
    klammer = huegel(t, 1.1, 1.3, 2.4, 2.7)
    beben = klammer * math.sin(2 * math.pi * t * 8.5)
    p.schieben("becken", z=0.35 * hoch - 1.2 * unten + 0.01 * beben)
    p.dazu("becken", beuge=6 * unten)
    p.dazu("bauch", beuge=-6 * hoch + 16 * unten)
    p.dazu("hals", beuge=-10 * hoch + 10 * unten)
    hut(p, -24 * hoch + 46 * klammer + 1.5 * beben)
    # The legs splay wide and land either side of the prey, the arms reach down round it
    for n, s in (("l", 1), ("r", -1)):
        if unten > 0.02:
            mix = unten
            ruhe_fuss = Vector((0.0, 0.17 * s, 0.13 + SCHWEBE))
            ziel = ruhe_fuss.lerp(Vector((0.12, 0.62 * s, 0.13)), mix)
            p.greifen("oberschenkel_" + n, "unterschenkel_" + n, ziel, (0.25, 1.0 * s, 0.5), "fuss_" + n,
                      (30 * (1 - mix), 0, 35 * s * mix))
    arme(p, heben=(40 * hoch + 26 * klammer, 40 * hoch + 26 * klammer), vor=(-10 * hoch + 40 * klammer, -10 * hoch + 44 * klammer),
         ellbogen=(20 * hoch + 50 * klammer, 20 * hoch + 46 * klammer), hand=(30 * klammer, 30 * klammer))
    grund = {n: p.winkel[n] for n in p.winkel if n.startswith("faden_")}
    faeden(p, lambda k, g, a: (0.0, 0.0, -10 * hoch * (g == 0) - 30 * klammer * (g == 0) + 20 * klammer * (g > 0)
                                 + 2.5 * beben * g), einzug=1.0 - 0.45 * unten)
    for n, alt in grund.items():
        p.winkel[n] = p.winkel[n] @ alt
    return p


def treffer(t):
    p = mit_grund(t, 1.0)
    ruck = huegel(t, 0.0, 0.07, 0.12, 0.6)
    p.schieben("becken", x=-0.14 * ruck, z=-0.05 * ruck)
    p.dazu("becken", beuge=-12 * ruck)
    p.dazu("brust", beuge=-8 * ruck)
    p.dazu("hals", beuge=-14 * ruck, seite=6 * ruck)
    hut(p, 20 * ruck, kippen=(-10 * ruck, 5 * ruck), flattern=6 * ruck, t=t)
    arme(p, heben=(34 * ruck, 28 * ruck), vor=(30 * ruck, 36 * ruck), ellbogen=(34 * ruck, 40 * ruck))
    beine(p, huefte=(24 * ruck, 14 * ruck), knie=(30 * ruck, 22 * ruck), spitze=(0, 0))
    grund = {n: p.winkel[n] for n in p.winkel if n.startswith("faden_")}
    faeden(p, lambda k, g, a: (-9 * huegel(t - 0.04 * g, 0.0, 0.1, 0.16, 0.6), 0.0, 0.0))
    for n, alt in grund.items():
        p.winkel[n] = p.winkel[n] @ alt
    return p


def sporenbersten(t):
    p = mit_grund(t, 1.0)
    zu = huegel(t, 0.0, 0.12, 0.3, 0.38)
    auf = huegel(t, 0.33, 0.42, 0.6, 1.3)
    beben = auf * math.sin(2 * math.pi * t * 10) * (1 - rampe(t, 0.5, 1.2))
    p.schieben("becken", z=-0.1 * zu + 0.16 * auf)
    p.dazu("becken", beuge=8 * zu - 10 * auf)
    p.dazu("hals", beuge=18 * zu - 16 * auf, seite=5 * beben)
    hut(p, 34 * zu - 30 * auf + 4 * beben, flattern=5 * auf, t=t)
    arme(p, heben=(-4 * zu + 62 * auf, -4 * zu + 58 * auf), vor=(30 * zu - 12 * auf, 30 * zu - 16 * auf),
         ellbogen=(50 * zu, 50 * zu), hand=(30 * zu - 30 * auf, 30 * zu - 30 * auf))
    beine(p, huefte=(40 * zu - 6 * auf, 36 * zu - 10 * auf), knie=(60 * zu, 56 * zu), spitze=(0, 0), spreizen=(14 * auf, 12 * auf))
    grund = {n: p.winkel[n] for n in p.winkel if n.startswith("faden_")}
    faeden(p, lambda k, g, a: (0.0, 0.0, 14 * zu - (20 - 4 * g) * huegel(t - 0.03 * g, 0.33, 0.43, 0.55, 1.3) + 3 * beben))
    for n, alt in grund.items():
        p.winkel[n] = p.winkel[n] @ alt
    return p


def tod(t):
    # the beat stutters and fails, it sinks, the legs touch down and give way, it tips forward onto the cap's rim
    lebt = 1 - rampe(t, 0.0, 0.9)
    p = schweben(t * (1 - 0.5 * rampe(t, 0.0, 0.9)) + 1.0, 6.0, 1.1, kraft=0.8 * lebt)
    sinkt = rampe(t, 0.3, 1.5)
    kippt = rampe(t, 1.0, 2.05)
    kippt = kippt * kippt
    prall = huegel(t, 2.0, 2.08, 2.1, 2.5)
    for n in list(p.versatz):
        p.versatz[n] = p.versatz[n] * (1 - sinkt)
    p.schieben("becken", x=0.45 * kippt, z=SCHWEBE * (1 - sinkt) - 0.35 * sinkt - 0.85 * kippt + 0.04 * prall)
    p.dazu("becken", beuge=10 * sinkt + 59 * kippt - 3 * prall)
    p.dazu("bauch", beuge=10 * sinkt)
    p.dazu("hals", beuge=14 * sinkt + 4 * kippt, seite=8 * kippt)
    hut(p, 12 * sinkt + 10 * kippt, kippen=(4 * kippt, 8 * kippt))
    beine(p, huefte=(30 * sinkt - 47 * kippt, 22 * sinkt - 35 * kippt), knie=(44 * sinkt - 32 * kippt, 36 * sinkt - 30 * kippt),
          spitze=(-30 * sinkt, -30 * sinkt), spreizen=(8 * kippt, 14 * kippt))
    arme(p, heben=(12 * sinkt + 16 * kippt, 10 * sinkt + 30 * kippt), vor=(10 * sinkt + 50 * kippt, 8 * sinkt + 30 * kippt),
         ellbogen=(20 * sinkt + 20 * kippt, 24 * sinkt), hand=(20 * sinkt, 20 * sinkt))
    grund = {n: p.winkel[n] for n in p.winkel if n.startswith("faden_")}
    faeden(p, lambda k, g, a: (6 * kippt * (g > 0), 0.0, 8 * sinkt * (g == 0)), einzug=1.0 - 0.35 * kippt)
    for n, alt in grund.items():
        p.winkel[n] = p.winkel[n] @ alt
    return p


# name: (seconds, pose, loop, frames to render for the check)
AKTIONEN = {
    "Schweben_Idle": (6.0, schweben_idle, True, (0, 12, 36, 80, 100, 150)),
    "Schweben_Vor": (3.0, schweben_vor, True, (0, 8, 22, 45, 60, 80)),
    "Gehen": (2.0, gehen, True, (0, 8, 15, 22, 30, 38, 45, 52)),
    "Lauern": (5.0, lauern, True, (0, 40, 95, 100)),
    "Erwachen": (2.0, erwachen, False, (0, 12, 22, 30, 42, 60)),
    "Nesselfaeden": (1.6, nesselfaeden, False, (0, 12, 18, 22, 26, 40)),
    "Giftregen": (2.6, giftregen, False, (0, 14, 28, 34, 50, 70)),
    "Glockenschlag": (3.2, glockenschlag, False, (0, 20, 31, 36, 60, 84)),
    "Treffer": (0.6, treffer, False, (0, 3, 6, 12)),
    "Sporenbersten": (1.3, sporenbersten, False, (0, 5, 10, 13, 18, 30)),
    "Tod": (2.6, tod, False, (0, 20, 36, 50, 62, 78)),
}


def main():
    nur = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    datei = "03_animationen.blend" if nur and os.path.exists(gemein.ordner(NAME, "arbeit", "03_animationen.blend")) else "02_skelett.blend"
    obj, arm = gemein.oeffnen(NAME, datei)
    ARM[0] = arm
    for k in range(FAEDEN):
        kopf = arm.data.bones["faden_%d_1" % (k + 1)].head_local
        FADEN_WINKEL[k] = math.atan2(kopf.y, kopf.x)
    for aktion, (dauer, pose, schleife, _) in AKTIONEN.items():
        if not nur or aktion in nur:
            bau.backen(NAME, arm, aktion, dauer, pose, schleife)
    bau.ruhe(arm)
    gemein.speichern(NAME, "03_animationen.blend")
    for aktion, (_, _, _, bilder) in AKTIONEN.items():
        if not nur or aktion in nur:
            bau.clip_pruefen(NAME, arm, aktion, bilder, MITTE, 3.7)


main()
