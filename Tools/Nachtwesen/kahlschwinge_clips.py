"""Kahlschwinge, step 3: the animations (30 frames per second, in place; the actor moves it).

It flies: the pelvis carries the body FLUG metres up. The arms are the wings (blade feathers under them), the small
back wings flutter three times as fast. Its head never glides: it snaps from one listening pose to the next.

  Flug_Idle       4 s loop    hovers, leaning a little, ten wing beats, head snapping round to listen
  Flug_Vor        2.4 s loop  flies forward, body flat, legs and tail streaming
  Lauern          4 s loop    perched in a crouch, wings folded, head and ears working
  Abheben         1.1 s       leaps out of the crouch into the air
  Sturzflug       1.5 s       rears up, folds the wings and dives talons first (hit at 0.8 s), pulls out
  Fluegelschnitt  1.0 s       winds up and spins once with the wing edges out like blades (hit at 0.45 s)
  Kreischen       1.6 s       rears back, wings wide, head thrust forward, shaking (scream from 0.5 s)
  Packen          2.4 s       swoops with the talons forward (grab at 0.6 s), heaves upward, lets go at 1.7 s
  Kreisen         3.2 s loop  circles banked like a bird of prey, gliding, looking down at its prey
  Festkrallen     1.2 s loop  clings with the feet and rakes with the hand claws in turn
  Treffer         0.5 s       flinches
  Tod             2.2 s       the wings seize, it drops and lies face down with the wings spread

    blender -b --factory-startup -P Tools/Nachtwesen/kahlschwinge_clips.py [-- <Aktion> ...]
"""
import math
import os
import sys

from mathutils import Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bau  # noqa: E402
import gemein  # noqa: E402
from bau import Pose, huegel, rampe, unruhe, weich, welle  # noqa: E402

NAME = "Kahlschwinge"
FLUG = 0.9
SCHLAG = 0.4
MITTE = (0.0, 0.0, 1.5)
SEITEN = (("l", 1), ("r", -1))


def schnapp(t, schleife, posen, dauer=0.09):
    """A value that holds still and jumps to the next pose in <dauer> seconds: posen = [(time, value), ...] over
    the loop; values are tuples"""
    t %= schleife
    n = len(posen)
    for i in range(n):
        t0, v0 = posen[i]
        t1, v1 = posen[(i + 1) % n]
        if i == n - 1:
            t1 += schleife
        if t0 <= t < t1 or (i == n - 1 and t < posen[0][0] and t + schleife < t1):
            tt = t if t >= t0 else t + schleife
            x = weich((tt - (t1 - dauer)) / dauer)
            return tuple(a + (b - a) * x for a, b in zip(v0, v1))
    return posen[0][1]


def fluegel(p, hoch, folge=None, zurueck=0.0, falten=0.0):
    """Both wings: hoch = (left, right) lift of the upper arm in degrees (negative down); folge = the same for the
    forearm on top; zurueck sweeps the arms back, falten folds the feathers against the arm"""
    folge = folge or (0.0, 0.0)
    for i, (n, s) in enumerate(SEITEN):
        p.dazu("oberarm_" + n, seite=hoch[i] * s, wende=-zurueck * s)
        p.dazu("unterarm_" + n, seite=folge[i] * s, wende=-zurueck * 0.5 * s)
        p.dazu("hand_" + n, seite=folge[i] * 0.5 * s)
        # the inner feathers keep hanging when the arm comes down, so they don't cut into the flank
        p.dazu("schwinge_innen_" + n, seite=-min(hoch[i], 0.0) * 0.8 * s - falten * 0.3 * s, wende=falten * 0.5 * s)
        p.dazu("schwinge_aussen_" + n, seite=-folge[i] * 0.3 * s, wende=falten * 0.7 * s)


def beine(p, huefte=(0.0, 0.0), knie=(0.0, 0.0), fuss=(0.0, 0.0), spreizen=(0.0, 0.0)):
    """thigh forward (+), knee bent (+), foot: toes down (+), thigh out to the side (+)"""
    for i, (n, s) in enumerate(SEITEN):
        p.dazu("oberschenkel_" + n, beuge=-huefte[i], seite=spreizen[i] * s)
        p.dazu("unterschenkel_" + n, beuge=knie[i])
        p.dazu("fuss_" + n, beuge=fuss[i])


def schwanz(p, t, schleife, strom=0.0, schlag=1.0):
    """The tail: a wave running down it; strom streams it back and level"""
    for i in range(4):
        spaet = t - 0.09 * i
        p.dazu("schwanz_%d" % (i + 1),
               beuge=-10 * strom + 5 * schlag * welle(spaet, schleife / max(1, round(schleife / 1.3)), 0.2),
               wende=4 * strom + (6 + 3 * i) * schlag * unruhe(spaet, schleife, 3.0 + i * 0.4))


def kopf(p, t, schleife, posen, ausgleich=0.0):
    """The head snaps between listening poses (wende, seite, beuge); ausgleich lifts it against the body's lean"""
    w, s, b = schnapp(t, schleife, posen)
    p.dazu("hals", beuge=-ausgleich * 0.45 + b * 0.4, wende=w * 0.35)
    p.dazu("kopf", beuge=-ausgleich * 0.45 + b * 0.6, wende=w * 0.65, seite=s)


LAUSCHEN = [(0.0, (0.0, 0.0, 0.0)), (0.7, (28.0, 8.0, -4.0)), (1.25, (24.0, -6.0, 3.0)), (2.0, (-32.0, -10.0, 0.0)),
            (2.6, (-8.0, 12.0, 6.0)), (3.3, (10.0, -4.0, -6.0))]


def flug(t, schleife, neigung=14.0, kraft=1.0, hoehe=FLUG, lauschen=LAUSCHEN, schlag=SCHLAG):
    """Flying: wing beats (down fast, up slower), the body lifting with each downbeat, back wings fluttering"""
    p = Pose()
    phase = t / schlag
    # arm angle: up at phase 0, down quickly, back up slowly
    def hub(ph):
        ph %= 1.0
        return math.cos(math.pi * (ph / 0.4)) if ph < 0.4 else -math.cos(math.pi * (ph - 0.4) / 0.6)
    a, a_spaet, a_hand = hub(phase), hub(phase - 0.13), hub(phase - 0.24)
    ungleich = 1.0 + 0.12 * unruhe(t, schleife, 1.0)
    oben = (8 + 30 * a * kraft * ungleich, 8 + 30 * a * kraft * (2 - ungleich))
    fluegel(p, oben, (16 * a_spaet * kraft + 6 * a_hand, 16 * a_spaet * kraft + 6 * a_hand), zurueck=6 + neigung * 0.25)
    steigen = -math.sin(2 * math.pi * (phase - 0.3))        # rises just after the downbeat
    p.schieben("becken", x=0.02 * unruhe(t, schleife, 2.0), y=0.03 * unruhe(t, schleife, 3.0),
               z=hoehe + 0.045 * kraft * steigen + 0.03 * unruhe(t, schleife, 4.0))
    p.dazu("becken", beuge=neigung + 2.5 * steigen * kraft + 2 * unruhe(t, schleife, 5.0), seite=3 * unruhe(t, schleife, 6.0),
           wende=3 * unruhe(t, schleife, 7.0))
    p.dazu("bauch", beuge=-neigung * 0.15 - 2 * steigen)
    p.dazu("brust", beuge=-neigung * 0.15 + 3 * a * kraft)
    kopf(p, t, schleife, lauschen, ausgleich=neigung * 0.75)
    for n, s in SEITEN:
        flattern = welle(t, schlag / 3.0, 0.15 if s > 0 else 0.4)
        p.dazu("rueckenfluegel_" + n, seite=(10 + 16 * flattern) * s, wende=(8 * flattern - neigung * 0.3) * s)
        p.dazu("ohr_" + n, beuge=neigung * 0.3, seite=-3 * steigen * s)
    haengen = 1 - min(1.0, neigung / 55.0)
    beine(p,
          huefte=(24 * haengen - neigung * 0.45 + 5 * steigen + 4 * unruhe(t, schleife, 8.0), 12 * haengen - neigung * 0.6 + 4 * steigen + 4 * unruhe(t, schleife, 9.0)),
          knie=(46 * haengen + 10 + 6 * steigen, 34 * haengen + 14 + 5 * steigen),
          fuss=(30 + 14 * (1 - haengen), 36 + 14 * (1 - haengen)), spreizen=(4, 3))
    schwanz(p, t, schleife, strom=neigung / 55.0)
    return p


def flug_idle(t):
    return flug(t, 4.0)


def flug_vor(t):
    posen = [(0.0, (0.0, 0.0, 0.0)), (0.9, (14.0, 5.0, 4.0)), (1.6, (-12.0, -6.0, 0.0))]
    return flug(t, 2.4, neigung=52.0, kraft=1.2, lauschen=posen)


FUSS_RUHE = {"l": Vector((0.14, 0.12, 0.17)), "r": Vector((0.14, -0.12, 0.17))}


def hocke(t, schleife=4.0, tiefe=1.0):
    """Perched: crouched on its haunches, wings folded shut, tail curled round"""
    p = Pose()
    atem = welle(t, 2.0) if schleife else 0.0
    p.schieben("becken", x=-0.1 * tiefe, z=(-0.4 + 0.008 * atem) * tiefe)
    p.dazu("becken", beuge=34 * tiefe)
    p.dazu("bauch", beuge=(8 + atem) * tiefe)
    p.dazu("brust", beuge=(4 - 1.5 * atem) * tiefe)
    if schleife:
        kopf(p, t, schleife, LAUSCHEN, ausgleich=40 * tiefe)
    else:
        p.dazu("hals", beuge=-18 * tiefe)
        p.dazu("kopf", beuge=-18 * tiefe)
    for n, s in SEITEN:
        p.greifen("oberschenkel_" + n, "unterschenkel_" + n, FUSS_RUHE[n] + Vector((0.0, 0.05 * s, 0.0)), (1.0, 0.5 * s, 0.2), "fuss_" + n)
        p.dazu("oberarm_" + n, seite=-62 * tiefe * s, wende=-20 * tiefe * s)
        p.dazu("unterarm_" + n, seite=118 * tiefe * s)
        p.dazu("hand_" + n, seite=-70 * tiefe * s)
        p.dazu("schwinge_innen_" + n, seite=40 * tiefe * s, wende=30 * tiefe * s)
        p.dazu("schwinge_aussen_" + n, seite=-70 * tiefe * s, wende=40 * tiefe * s)
        p.dazu("rueckenfluegel_" + n, wende=-40 * tiefe * s, seite=(-10 + (3 * unruhe(t, schleife, 2.0 + s) if schleife else 0)) * tiefe * s)
    for i in range(4):
        p.dazu("schwanz_%d" % (i + 1), wende=-16 * tiefe + (5 * unruhe(t - 0.1 * i, schleife, 5.0) if schleife else 0), beuge=-6 * tiefe)
    return p


def lauern(t):
    return hocke(t, 4.0)


def mischen(a, b, x):
    p = Pose()
    for n in set(a.winkel) | set(b.winkel):
        wa, wb = a.winkel.get(n, (0.0, 0.0, 0.0)), b.winkel.get(n, (0.0, 0.0, 0.0))
        p.winkel[n] = tuple(wa[i] + (wb[i] - wa[i]) * x for i in range(3))
    for n in set(a.versatz) | set(b.versatz):
        p.versatz[n] = a.versatz.get(n, Vector()).lerp(b.versatz.get(n, Vector()), x)
    return p


def abheben(t):
    ducken = huegel(t, 0.0, 0.25, 0.3, 0.45)
    hoch = rampe(t, 0.3, 1.0)
    p = mischen(hocke(0.0, 0.0), flug(t + 4.0 - 1.1, 4.0, kraft=1.0 + 0.6 * (1 - hoch)), hoch)
    p.schieben("becken", z=-0.12 * ducken)
    if t < 0.42:
        for n, s in SEITEN:
            p.greifen("oberschenkel_" + n, "unterschenkel_" + n, FUSS_RUHE[n] + Vector((0.0, 0.05 * s, 0.0)), (1.0, 0.5 * s, 0.2), "fuss_" + n)
    return p


def grund(t, vor=2.9):
    return flug(vor + t, 4.0, lauschen=[(0.0, (0.0, 0.0, 0.0))])


def sturzflug(t):
    auf = huegel(t, 0.0, 0.35, 0.45, 0.62)       # rears up, wings high
    sturz = huegel(t, 0.45, 0.68, 0.85, 1.2)     # the dive
    krall = huegel(t, 0.62, 0.78, 0.86, 1.05)    # talons thrown forward
    raus = huegel(t, 0.95, 1.15, 1.2, 1.5)       # pulls out with hard beats
    p = flug(2.9 + t, 4.0, neigung=20 - 34 * auf + 48 * sturz - 14 * raus, kraft=1.0 - 0.9 * max(auf, sturz) + 0.7 * raus,
             lauschen=[(0.0, (0.0, 0.0, 0.0))])
    p.schieben("becken", x=-0.15 * auf + 0.35 * sturz, z=0.4 * auf - 0.62 * sturz)
    fluegel(p, (36 * auf - 10 * sturz, 36 * auf - 10 * sturz), (20 * auf, 20 * auf), zurueck=58 * sturz, falten=40 * sturz)
    beine(p, huefte=(-20 * auf + 80 * krall, -20 * auf + 86 * krall), knie=(30 * auf - 30 * krall, 30 * auf - 34 * krall),
          fuss=(-60 * krall, -60 * krall), spreizen=(10 * krall, 10 * krall))
    p.dazu("kopf", beuge=-18 * sturz)
    for n, s in SEITEN:
        p.dazu("ohr_" + n, beuge=34 * sturz)
    return p


def fluegelschnitt(t):
    aus = huegel(t, 0.0, 0.22, 0.28, 0.4)
    dreh = rampe(t, 0.28, 0.72)
    dreh = dreh * dreh * (3 - 2 * dreh)
    starr = huegel(t, 0.1, 0.3, 0.7, 0.95)
    p = flug(2.9 + t, 4.0, kraft=1.0 - starr, lauschen=[(0.0, (0.0, 0.0, 0.0))])
    p.dazu("becken", wende=-55 * aus * (1 - dreh) + 360 * dreh, seite=-16 * starr, beuge=10 * starr)
    p.schieben("becken", z=-0.12 * starr + 0.1 * aus, x=0.12 * starr)
    # wings locked out: the leading one low, the other high, so the blades cut on a slant
    fluegel(p, (22 * starr, -10 * starr), (8 * starr, 6 * starr), zurueck=-10 * starr)
    for n, s in SEITEN:
        p.dazu("schwinge_aussen_" + n, wende=-18 * starr * s)
        p.dazu("ohr_" + n, beuge=28 * starr)
    beine(p, huefte=(26 * starr, 20 * starr), knie=(40 * starr, 34 * starr))
    p.dazu("kopf", wende=30 * aus - 30 * huegel(t, 0.3, 0.4, 0.6, 0.8))
    for i in range(4):
        p.dazu("schwanz_%d" % (i + 1), wende=-22 * huegel(t - 0.04 * i, 0.3, 0.45, 0.7, 0.95))
    return p


def kreischen(t):
    hol = huegel(t, 0.0, 0.3, 0.38, 0.5)
    schrei = huegel(t, 0.4, 0.52, 1.2, 1.5)
    zittern = schrei * math.sin(2 * math.pi * t * 13)
    p = flug(2.9 + t, 4.0, neigung=20 - 20 * hol + 14 * schrei, kraft=1.0 - 0.7 * schrei, lauschen=[(0.0, (0.0, 0.0, 0.0))])
    p.schieben("becken", x=-0.1 * hol + 0.12 * schrei, z=0.1 * hol)
    p.dazu("brust", beuge=-14 * hol + 8 * schrei + 1.2 * zittern)
    p.dazu("hals", beuge=-20 * hol + 30 * schrei)
    p.dazu("kopf", beuge=-16 * hol - 6 * schrei + 2.5 * zittern, seite=3 * zittern)
    fluegel(p, (20 * hol + 26 * schrei + 2 * zittern, 20 * hol + 26 * schrei - 2 * zittern), (10 * schrei, 10 * schrei), zurueck=-26 * schrei + 20 * hol)
    for n, s in SEITEN:
        p.dazu("ohr_" + n, beuge=44 * schrei, seite=-10 * schrei * s)
        p.dazu("rueckenfluegel_" + n, seite=24 * schrei * s)
        p.dazu("hand_" + n, wende=30 * schrei * s)
    beine(p, huefte=(16 * schrei, 22 * schrei), knie=(20 * schrei, 26 * schrei), spreizen=(10 * schrei, 10 * schrei))
    return p


def packen(t):
    stoss = huegel(t, 0.15, 0.5, 0.62, 0.8)      # swoops, talons forward
    halt = huegel(t, 0.55, 0.68, 1.65, 1.8)      # talons clenched round the prey
    heben = huegel(t, 0.75, 1.3, 1.7, 2.3)       # heaves upward
    los = huegel(t, 1.7, 1.8, 1.9, 2.2)          # kicks it loose
    p = flug(2.9 + t, 4.0, neigung=20 + 14 * stoss - 10 * heben, kraft=1.0 + 0.7 * heben, lauschen=[(0.0, (0.0, 0.0, 0.0))])
    p.schieben("becken", x=0.25 * stoss, z=-0.5 * stoss + 0.45 * heben)
    beine(p, huefte=(62 * stoss + 22 * halt - 20 * los, 68 * stoss + 22 * halt - 20 * los), knie=(-20 * stoss + 30 * halt - 30 * los, -24 * stoss + 30 * halt - 30 * los),
          fuss=(-60 * stoss + 50 * halt - 40 * los, -60 * stoss + 50 * halt - 40 * los), spreizen=(12 * stoss - 8 * halt, 12 * stoss - 8 * halt))
    p.dazu("hals", beuge=20 * halt)
    p.dazu("kopf", beuge=14 * halt)
    return p


def kreisen(t):
    """Circles like a bird of prey: banked into the turn, gliding on spread wings with a slow beat now and then,
    head turned down and inward at its prey"""
    posen = [(0.0, (30.0, 10.0, 16.0)), (1.3, (38.0, 4.0, 22.0)), (2.2, (22.0, 12.0, 12.0))]
    p = flug(t, 3.2, neigung=44.0, kraft=0.3, lauschen=posen, schlag=0.8)
    p.dazu("becken", seite=-24 + 2 * unruhe(t, 3.2, 11.0))
    # the inner wing a little lower and drawn in, the outer one stretched
    p.dazu("oberarm_l", seite=-6, wende=-8)
    p.dazu("oberarm_r", seite=-8)
    for n, s in SEITEN:
        p.dazu("schwinge_aussen_" + n, wende=-8 * s)
    return p


def festkrallen(t):
    """Clings to its prey with the feet thrown forward and clenched, and rakes it with the claws of its hands,
    left and right in turn (a stroke every 0.3 s), wings half open for balance"""
    schleife = 1.2
    p = Pose()
    ruck = math.sin(2 * math.pi * t / 0.3)
    p.schieben("becken", x=-0.1 + 0.02 * ruck, z=0.42 + 0.02 * unruhe(t, schleife, 2.0))
    p.dazu("becken", beuge=-10 + 3 * ruck, wende=5 * math.sin(2 * math.pi * t / 0.6))
    p.dazu("bauch", beuge=12)
    p.dazu("brust", beuge=14 + 3 * ruck, wende=-9 * math.sin(2 * math.pi * t / 0.6))
    biss = huegel(t % 0.6, 0.0, 0.1, 0.16, 0.34)
    p.dazu("hals", beuge=18 + 14 * biss)
    p.dazu("kopf", beuge=10 + 10 * biss, seite=6 * math.sin(2 * math.pi * t / 0.6))
    beine(p, huefte=(82 + 3 * ruck, 86 - 3 * ruck), knie=(38, 42), fuss=(44, 44), spreizen=(9, 9))
    for i, (n, s) in enumerate(SEITEN):
        ph = ((t / 0.6) + 0.5 * i) % 1.0
        hol = huegel(ph, 0.0, 0.3, 0.42, 0.55)       # arm lifted and drawn back
        riss = huegel(ph, 0.42, 0.56, 0.7, 1.0)      # the stroke forward and down
        p.dazu("oberarm_" + n, seite=(-22 + 44 * hol - 26 * riss) * s, wende=(52 - 22 * hol + 22 * riss) * s)
        p.dazu("unterarm_" + n, seite=(54 - 20 * hol - 36 * riss) * s, wende=(18 + 26 * riss) * s)
        p.dazu("hand_" + n, seite=(-30 + 40 * riss) * s, wende=30 * riss * s)
        p.dazu("schwinge_innen_" + n, seite=24 * s, wende=24 * s)
        p.dazu("schwinge_aussen_" + n, seite=-30 * s, wende=34 * s)
        p.dazu("rueckenfluegel_" + n, seite=(14 + 18 * welle(t, 0.15, 0.2 * i)) * s)
        p.dazu("ohr_" + n, beuge=34)
    for i in range(4):
        p.dazu("schwanz_%d" % (i + 1), beuge=8, wende=10 * math.sin(2 * math.pi * (t - 0.05 * i) / 0.6))
    return p


def treffer(t):
    ruck = huegel(t, 0.0, 0.06, 0.1, 0.5)
    p = flug(1.0 + t, 4.0, kraft=1.0 - 0.6 * ruck, lauschen=[(0.0, (0.0, 0.0, 0.0))])
    p.schieben("becken", x=-0.16 * ruck, z=-0.1 * ruck)
    p.dazu("becken", beuge=-20 * ruck, seite=10 * ruck)
    p.dazu("hals", beuge=-14 * ruck, wende=24 * ruck)
    p.dazu("kopf", seite=14 * ruck)
    fluegel(p, (-16 * ruck, 12 * ruck), (-10 * ruck, 8 * ruck), zurueck=24 * ruck, falten=14 * ruck)
    beine(p, huefte=(30 * ruck, 20 * ruck), knie=(30 * ruck, 20 * ruck))
    for n, s in SEITEN:
        p.dazu("ohr_" + n, beuge=36 * ruck)
    return p


def tod(t):
    krampf = huegel(t, 0.0, 0.12, 0.3, 0.6)
    faellt = rampe(t, 0.25, 1.05)
    faellt = faellt * faellt
    liegt = rampe(t, 0.95, 1.2)
    prall = huegel(t, 1.02, 1.1, 1.12, 1.5)
    schlaff = rampe(t, 1.0, 2.0)
    p = flug(1.0 + t * (1 - 0.6 * rampe(t, 0.0, 0.5)), 4.0, kraft=(1 - rampe(t, 0.1, 0.5)), lauschen=[(0.0, (0.0, 0.0, 0.0))],
             neigung=20 + 64 * faellt)
    for n in list(p.versatz):
        p.versatz[n] = p.versatz[n] * (1 - faellt)
    p.schieben("becken", x=0.3 * faellt, z=FLUG * (1 - faellt) - 0.77 * faellt + 0.05 * prall)
    p.dazu("becken", seite=14 * krampf * (1 - liegt))
    p.dazu("bauch", beuge=-16 * krampf + 4 * liegt)
    p.dazu("hals", beuge=20 * liegt, wende=40 * liegt)
    p.dazu("kopf", wende=30 * liegt, seite=20 * liegt)
    fluegel(p, (40 * krampf - 4 * liegt, 30 * krampf - 8 * liegt), (30 * krampf, 20 * krampf), zurueck=10 * krampf + 16 * schlaff, falten=20 * schlaff)
    beine(p, huefte=(40 * krampf + 44 * liegt, 30 * krampf + 50 * liegt), knie=(50 * krampf - 14 * liegt, 40 * krampf - 8 * liegt),
          fuss=(30 * liegt, 30 * liegt), spreizen=(10 * liegt, 16 * liegt))
    for n, s in SEITEN:
        p.dazu("ohr_" + n, beuge=30 * liegt)
        p.dazu("rueckenfluegel_" + n, wende=-30 * schlaff * s)
    return p


AKTIONEN = {
    "Flug_Idle": (4.0, flug_idle, True, (0, 3, 6, 9, 21, 60)),
    "Flug_Vor": (2.4, flug_vor, True, (0, 3, 6, 9, 30, 50)),
    "Lauern": (4.0, lauern, True, (0, 22, 60, 80)),
    "Abheben": (1.1, abheben, False, (0, 8, 13, 18, 24, 33)),
    "Sturzflug": (1.5, sturzflug, False, (0, 11, 17, 22, 25, 36)),
    "Fluegelschnitt": (1.0, fluegelschnitt, False, (0, 7, 11, 14, 18, 24)),
    "Kreischen": (1.6, kreischen, False, (0, 9, 16, 24, 36, 46)),
    "Packen": (2.4, packen, False, (0, 12, 18, 30, 45, 56)),
    "Kreisen": (3.2, kreisen, True, (0, 12, 24, 48, 72)),
    "Festkrallen": (1.2, festkrallen, True, (0, 4, 8, 13, 18, 26)),
    "Treffer": (0.5, treffer, False, (0, 2, 5, 10)),
    "Tod": (2.2, tod, False, (0, 6, 18, 28, 36, 60)),
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
            bau.clip_pruefen(NAME, arm, aktion, bilder, MITTE, 3.6)


main()
