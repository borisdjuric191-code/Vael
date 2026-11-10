"""Peitschenunke, step 3: the animations (30 frames per second, in place; the actor moves it).

Hands and toes stay planted (IK) while the body moves over them. The tongue itself is built by the engine and shoots
from the bone "zunge"; the clips only prepare and follow it. The mouth of the model is closed geometry, so the jaw
opens only a little.

  Hocken_Idle    4 s loop    squats, breathing, throat pumping, the head ticking round, a finger tapping
  Lauern         4 s loop    pressed flat to the ground, almost still
  Aufschrecken   0.7 s       jolts up out of the flat crouch
  Kriechen       1.2 s loop  sticky quick crawl, diagonal pairs, the spine bending (1 m/s at its own pace)
  Zungenschlag   1.1 s       sinks back, throat swelling, the head rears, mouth open and thrust (tongue at 0.55 s), gulp
  Anspringen     1.2 s       crouches deep, leaps (in the air 0.38 - 0.8 s, hit at 0.6 s), lands heavy on its hands
  Schnappen      0.7 s       a short lunge and bite (0.3 s)
  Treffer        0.5 s       flinches
  Tod            2.0 s       the legs give way, the belly hits the ground, limbs slide out, a last twitch

    blender -b --factory-startup -P Tools/Nachtwesen/peitschenunke_clips.py [-- <Aktion> ...]
"""
import math
import os
import sys

from mathutils import Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bau  # noqa: E402
import gemein  # noqa: E402
from bau import Pose, huegel, rampe, unruhe, weich, welle  # noqa: E402

NAME = "Peitschenunke"
MITTE = (0.0, 0.0, 0.6)
SEITEN = (("l", 1), ("r", -1))
HAND = {"l": Vector((0.6, 0.42, 0.1)), "r": Vector((0.6, -0.42, 0.1))}
ZEHEN = {"l": Vector((-0.45, 0.6, 0.08)), "r": Vector((-0.45, -0.6, 0.08))}
NULL = Vector((0.0, 0.0, 0.0))


def stehen(p, hand=None, fuss=None, nur=("hl", "hr", "fl", "fr")):
    """Plants hands and toes: hand/fuss = offsets from where they rest, per side"""
    hand = hand or {}
    fuss = fuss or {}
    for n, s in SEITEN:
        if "h" + n in nur:
            p.greifen("oberarm_" + n, "unterarm_" + n, HAND[n] + hand.get(n, NULL), (-0.3, 1.0 * s, 0.6), "hand_" + n)
        if "f" + n in nur:
            p.greifen("unterschenkel_" + n, "fuss_" + n, ZEHEN[n] + fuss.get(n, NULL), (-1.0, 0.0, 0.7), "zehen_" + n)


def kehle(p, schwellung):
    p.skala["kehle"] = (1 + schwellung, 1 + schwellung, 1 + schwellung)


def leib(p, t, schleife, atem_periode=2.0, tief=0.0, staerke=1.0):
    """Breathing and the small shifts of weight every pose builds on"""
    atem = welle(t, atem_periode)
    p.schieben("becken", x=0.012 * staerke * unruhe(t, schleife, 1.0), y=0.02 * staerke * unruhe(t, schleife, 2.0),
               z=-tief + 0.012 * staerke * atem)
    p.dazu("becken", seite=1.5 * staerke * unruhe(t, schleife, 3.0), wende=1.2 * staerke * unruhe(t, schleife, 4.0))
    p.dazu("bauch", beuge=-0.8 * atem * staerke)
    p.dazu("brust", beuge=1.0 * atem * staerke)
    return atem


def hocken_idle(t):
    p = Pose()
    leib(p, t, 4.0)
    # the head ticks from one side to the other and holds still in between
    blick = 14 * (huegel(t, 0.6, 0.68, 1.7, 1.78) - huegel(t, 2.3, 2.38, 3.3, 3.38))
    p.dazu("kopf", wende=blick, seite=3 * unruhe(t, 4.0, 5.0), beuge=2 * unruhe(t, 4.0, 6.0))
    pumpen = 0.5 + 0.5 * welle(t, 4.0 / 3)
    kehle(p, 0.12 * pumpen)
    p.dazu("kehle", beuge=-4 * pumpen)
    tippen = huegel(t, 2.6, 2.66, 2.68, 2.76) + huegel(t, 2.86, 2.92, 2.94, 3.02)
    p.dazu("hand_l", beuge=-18 * tippen)
    stehen(p, nur=("hr", "fl", "fr"))
    p.greifen("oberarm_l", "unterarm_l", HAND["l"], (-0.3, 1.0, 0.6), "hand_l", (-18 * tippen, 0, 0))
    return p


def flach(t, schleife, tiefe=1.0):
    p = Pose()
    leib(p, t, schleife if schleife else 4.0, atem_periode=4.0, tief=0.2 * tiefe, staerke=0.35 if schleife else 0.0)
    p.schieben("becken", x=-0.05 * tiefe)
    p.dazu("becken", beuge=3 * tiefe)
    p.dazu("brust", beuge=4 * tiefe)
    p.dazu("kopf", beuge=6 * tiefe)
    if schleife:
        kehle(p, 0.05 * (0.5 + 0.5 * welle(t, 2.0)))
    return p


def lauern(t):
    p = flach(t, 4.0)
    stehen(p)
    return p


def aufschrecken(t):
    hoch = rampe(t, 0.05, 0.3)
    ueber = huegel(t, 0.12, 0.28, 0.32, 0.6)
    p = flach(0.0, 0.0, 1 - hoch)
    p.schieben("becken", z=0.07 * ueber, x=-0.04 * ueber)
    p.dazu("kopf", beuge=-12 * ueber)
    kehle(p, 0.2 * ueber)
    stehen(p)
    return p


def kriechen(t):
    p = Pose()
    takt = 0.6
    ph = t / takt
    weite = 0.36

    def schritt(phase):
        phase %= 1.0
        if phase < 0.6:
            return Vector((weite * (0.5 - phase / 0.6), 0.0, 0.0))
        u = (phase - 0.6) / 0.4
        return Vector((weite * (-0.5 + weich(u)), 0.0, 0.11 * math.sin(math.pi * u)))
    wiegen = math.sin(2 * math.pi * ph)
    p.schieben("becken", z=-0.07 + 0.018 * math.sin(4 * math.pi * ph) + 0.008 * unruhe(t, 1.2, 1.0), y=0.025 * wiegen)
    p.dazu("becken", wende=7 * wiegen, seite=2.5 * wiegen)
    p.dazu("bauch", wende=-6 * wiegen)
    p.dazu("brust", wende=-5 * wiegen)
    p.dazu("kopf", wende=4 * wiegen + 2 * unruhe(t, 1.2, 2.0), beuge=3)
    kehle(p, 0.08 * (0.5 + 0.5 * welle(t, 0.6)))
    stehen(p, hand={"l": schritt(ph), "r": schritt(ph + 0.5)}, fuss={"l": schritt(ph + 0.55), "r": schritt(ph + 0.05)})
    return p


def grund(t):
    p = Pose()
    leib(p, t + 1.0, 4.0)
    return p


def zungenschlag(t):
    p = grund(t)
    zurueck = huegel(t, 0.0, 0.35, 0.42, 0.55)
    offen = huegel(t, 0.38, 0.5, 0.68, 0.82)
    stoss = huegel(t, 0.44, 0.54, 0.62, 0.92)
    schluck = huegel(t, 0.84, 0.92, 0.94, 1.08)
    p.schieben("becken", x=-0.11 * zurueck + 0.15 * stoss, z=-0.06 * zurueck + 0.02 * stoss)
    p.dazu("becken", beuge=-5 * zurueck + 4 * stoss)
    p.dazu("brust", beuge=-4 * zurueck + 5 * stoss)
    p.dazu("kopf", beuge=-11 * zurueck - 5 * offen + 7 * schluck)
    p.dazu("kiefer", beuge=15 * offen)
    kehle(p, 0.34 * zurueck * (1 - stoss) + 0.2 * schluck)
    p.dazu("kehle", beuge=-8 * zurueck)
    stehen(p)
    return p


def anspringen(t):
    p = grund(t)
    ducken = huegel(t, 0.0, 0.28, 0.34, 0.42)
    luft = 0.38 <= t < 0.8
    u = min(max((t - 0.38) / 0.42, 0.0), 1.0)
    bogen = math.sin(math.pi * u) if 0 < u < 1 else 0.0
    landen = huegel(t, 0.8, 0.87, 0.92, 1.15)
    p.schieben("becken", x=-0.13 * ducken + 0.1 * bogen, z=-0.19 * ducken + 0.5 * bogen - 0.13 * landen)
    p.dazu("becken", beuge=6 * ducken - 22 * bogen * (1 - u) + 16 * bogen * u + 5 * landen)
    p.dazu("kopf", beuge=-8 * ducken + 10 * bogen * u - 6 * landen)
    p.dazu("kiefer", beuge=12 * huegel(t, 0.45, 0.55, 0.6, 0.66))
    kehle(p, 0.2 * ducken)
    if luft:
        strecken = huegel(t, 0.38, 0.46, 0.66, 0.8)
        for n, s in SEITEN:
            p.dazu("oberschenkel_" + n, wende=50 * strecken * s, beuge=8 * strecken)
            p.dazu("fuss_" + n, wende=95 * strecken * s, beuge=-20 * strecken)
            p.dazu("zehen_" + n, beuge=-20 * strecken)
            p.dazu("oberarm_" + n, beuge=-48 * strecken * (1 - 0.5 * u), seite=-10 * strecken * s)
            p.dazu("unterarm_" + n, beuge=-14 * strecken)
            p.dazu("hand_" + n, beuge=-30 * strecken)
    else:
        stehen(p)
    return p


def schnappen(t):
    p = grund(t)
    aus = huegel(t, 0.0, 0.1, 0.12, 0.2)
    stoss = huegel(t, 0.1, 0.26, 0.32, 0.62)
    offen = huegel(t, 0.06, 0.2, 0.27, 0.31)
    p.schieben("becken", x=-0.05 * aus + 0.2 * stoss, z=-0.03 * aus - 0.03 * stoss)
    p.dazu("brust", beuge=-5 * aus + 6 * stoss)
    p.dazu("kopf", beuge=-13 * offen + 5 * huegel(t, 0.3, 0.34, 0.4, 0.6))
    p.dazu("kiefer", beuge=16 * offen)
    kehle(p, 0.1 * aus)
    stehen(p)
    return p


def treffer(t):
    p = grund(t)
    ruck = huegel(t, 0.0, 0.06, 0.1, 0.5)
    p.schieben("becken", x=-0.13 * ruck, z=-0.05 * ruck)
    p.dazu("becken", seite=8 * ruck, beuge=-4 * ruck)
    p.dazu("kopf", beuge=-10 * ruck, wende=15 * ruck, seite=6 * ruck)
    kehle(p, 0.22 * ruck)
    stehen(p)
    return p


def tod(t):
    p = Pose()
    lebt = 1 - rampe(t, 0.0, 0.8)
    leib(p, t + 1.0, 4.0, staerke=lebt)
    knick = rampe(t, 0.1, 0.75)
    knick = knick * knick
    prall = huegel(t, 0.72, 0.8, 0.82, 1.1)
    schlaff = rampe(t, 0.7, 1.7)
    zucken = huegel(t, 1.35, 1.4, 1.42, 1.5) + 0.6 * huegel(t, 1.6, 1.64, 1.66, 1.74)
    p.schieben("becken", z=-0.33 * knick + 0.03 * prall, x=0.06 * knick)
    p.dazu("becken", seite=9 * schlaff, beuge=3 * knick)
    p.dazu("brust", beuge=5 * knick)
    p.dazu("kopf", beuge=12 * knick, seite=15 * schlaff, wende=-8 * schlaff)
    p.dazu("kiefer", beuge=7 * schlaff)
    kehle(p, 0.25 * huegel(t, 0.0, 0.3, 0.5, 0.9) - 0.1 * schlaff)
    hand = {n: Vector((0.1 * schlaff, 0.2 * schlaff * s, -0.05 * knick)) for n, s in SEITEN}
    fuss = {n: Vector((-0.12 * schlaff - (0.08 * zucken if n == "r" else 0.0), 0.16 * schlaff * s, -0.03 * knick)) for n, s in SEITEN}
    stehen(p, hand=hand, fuss=fuss)
    return p


AKTIONEN = {
    "Hocken_Idle": (4.0, hocken_idle, True, (0, 20, 50, 80, 82, 110)),
    "Lauern": (4.0, lauern, True, (0, 60)),
    "Aufschrecken": (0.7, aufschrecken, False, (0, 5, 9, 20)),
    "Kriechen": (1.2, kriechen, True, (0, 4, 8, 12, 16, 20)),
    "Zungenschlag": (1.1, zungenschlag, False, (0, 10, 14, 17, 22, 28)),
    "Anspringen": (1.2, anspringen, False, (0, 9, 13, 18, 22, 27)),
    "Schnappen": (0.7, schnappen, False, (0, 3, 7, 9, 14)),
    "Treffer": (0.5, treffer, False, (0, 3, 8)),
    "Tod": (2.0, tod, False, (0, 12, 22, 30, 42, 60)),
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
            bau.clip_pruefen(NAME, arm, aktion, bilder, MITTE, 2.9, ("vorne", "seite", "spiel"))


main()
