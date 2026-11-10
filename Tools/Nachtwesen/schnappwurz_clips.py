"""Schnappwurz, step 3: the animations (30 frames per second; the plant is rooted, mound and rosette never move).

Every stalk is a chain of six joints with its trap on the end; the two gaping traps (stalks 1 and 3) have jaws.
The far strike at a player is done by the game (reach chain on stalk 1); the clips wind up, snap and recover.

  Winden        6 s loop   the stalks wind slowly, each in its own time, traps breathing open and shut
  Lauern        5 s loop   stalks drooping outward like a harmless plant, traps shut, almost still
  Aufrichten    1.0 s      rears up out of the droop, shivering, traps tearing open
  Zuschnappen   1.1 s      stalk 1 curls back with its trap wide, lashes forward and snaps (0.5 s), shakes; 3 follows
  Nachgreifen   1.4 s      stalks 1 and 3 reach far and low after their prey, snap (0.7 s) and drag back shut
  Treffer       0.5 s      everything whips back, traps clamp shut
  Tod           2.4 s      the stalks go limp one after another and fall outward, the traps hang open

    blender -b --factory-startup -P Tools/Nachtwesen/schnappwurz_clips.py [-- <Aktion> ...]
"""
import json
import math
import os
import sys

from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bau  # noqa: E402
import gemein  # noqa: E402
from bau import Pose, huegel, rampe, unruhe, weich, welle  # noqa: E402

NAME = "Schnappwurz"
MITTE = (0.0, 0.0, 1.2)
GLIEDER = 6
STIELE = []      # per stalk: dict(nummer, offen, scharnier, aussen (axis to bend outward about), auf (+1/-1: sign that opens))


def stiel(p, s, vor=0.0, seite=0.0, aussen=0.0, falle=0.0, spaet=None):
    """Bends a stalk: degrees per joint forward (+X), sideways, outward from the plant's middle; vor/seite/aussen
    may be functions of the joint (0..5); falle tips the trap forward on top"""
    n = s["nummer"]
    for g in range(GLIEDER):
        v = vor(g) if callable(vor) else vor
        q = seite(g) if callable(seite) else seite
        a = aussen(g) if callable(aussen) else aussen
        neu = Matrix.Rotation(math.radians(a), 3, s["aussen"]) @ Matrix.Rotation(math.radians(v), 3, "Y") @ \
            Matrix.Rotation(math.radians(q), 3, "X")
        name = "stiel_%d_%d" % (n, g + 1)
        alt = p.winkel.get(name)
        p.winkel[name] = neu @ alt if alt is not None else neu
    if falle:
        name = "falle_%d" % n
        neu = Matrix.Rotation(math.radians(falle), 3, "Y")
        alt = p.winkel.get(name)
        p.winkel[name] = neu @ alt if alt is not None else neu


def kiefer(p, s, grad):
    """Opens the jaws of a gaping trap further (+) or shuts them (-), degrees per jaw"""
    if not s["offen"]:
        return
    n = s["nummer"]
    achse = Vector(s["scharnier"])
    p.winkel["kiefer_%d_o" % n] = Matrix.Rotation(math.radians(grad * s["auf"]), 3, achse)
    p.winkel["kiefer_%d_u" % n] = Matrix.Rotation(math.radians(-grad * s["auf"]), 3, achse)


def weben(p, t, schleife, staerke=1.0, haengen=0.0, maul=None):
    """The slow winding everything builds on; haengen droops the stalks outward; maul(s) overrides the jaws"""
    for s in STIELE:
        k = s["nummer"]
        stiel(p, s,
              vor=lambda g: staerke * 4.5 * unruhe(t - 0.22 * g, schleife, k * 1.3),
              seite=lambda g: staerke * 4.5 * unruhe(t - 0.22 * g, schleife, k * 2.1 + 7.0),
              aussen=lambda g: haengen * (7.0 if g >= 2 else 2.0),
              falle=-staerke * 9 * unruhe(t - 0.9, schleife, k * 1.3) + haengen * 10)
        if maul is not None:
            kiefer(p, s, maul(s))
        else:
            # breathing, and now and then a snap at nothing
            phase = (t / schleife + 0.37 * k) % 1.0
            kiefer(p, s, staerke * (6 * unruhe(t, schleife, 20 + k) - 24 * huegel(phase, 0.5, 0.52, 0.56, 0.66)))
    return p


def winden(t):
    return weben(Pose(), t, 6.0)


def lauern(t):
    return weben(Pose(), t, 5.0, staerke=0.18, haengen=1.0, maul=lambda s: -20)


def aufrichten(t):
    runter = 1 - rampe(t, 0.0, 0.45)
    ueber = huegel(t, 0.3, 0.5, 0.55, 0.9)
    p = weben(Pose(), t + 5.0, 6.0, staerke=0.18 + 0.82 * rampe(t, 0.3, 1.0), haengen=runter,
              maul=lambda s: -20 * runter + 22 * ueber)
    zittern = huegel(t, 0.25, 0.4, 0.6, 0.9) * math.sin(2 * math.pi * t * 12)
    for s in STIELE:
        stiel(p, s, aussen=-2.5 * ueber, seite=lambda g: 1.2 * zittern * (g / 5.0))
    return p


def grund(t, maul=None):
    return weben(Pose(), t + 2.0, 6.0, staerke=0.6, maul=maul)


def zuschnappen(t):
    def maul(s):
        v = 0.0 if s["nummer"] == 1 else 0.12
        return 24 * huegel(t - v, 0.05, 0.3, 0.46, 0.5) - 22 * huegel(t - v, 0.48, 0.5, 0.8, 1.0)
    p = grund(t, maul)
    for s in STIELE:
        k = s["nummer"]
        haupt = 1.0 if k == 1 else 0.55 if k == 3 else 0.2
        v = 0.0 if k == 1 else 0.12 if k == 3 else 0.06 * k
        aus = huegel(t - v, 0.0, 0.34, 0.4, 0.5)
        hieb = huegel(t - v, 0.4, 0.5, 0.62, 1.05)
        schuetteln = huegel(t - v, 0.5, 0.56, 0.72, 0.85) * math.sin(2 * math.pi * t * 9)
        stiel(p, s,
              vor=lambda g: haupt * (-7.5 * huegel(t - v - 0.02 * g, 0.0, 0.34, 0.4, 0.5) + 14.5 * huegel(t - v - 0.012 * g, 0.4, 0.5, 0.62, 1.05)),
              seite=lambda g: haupt * 3.5 * schuetteln * (g / 5.0),
              falle=haupt * (-20 * aus + 26 * hieb))
    return p


def nachgreifen(t):
    def maul(s):
        v = 0.0 if s["nummer"] == 1 else 0.08
        return 26 * huegel(t - v, 0.1, 0.5, 0.66, 0.7) - 22 * huegel(t - v, 0.68, 0.7, 1.15, 1.3)
    p = grund(t, maul)
    for s in STIELE:
        k = s["nummer"]
        haupt = 1.0 if k == 1 else 0.9 if k == 3 else 0.25
        v = 0.0 if k == 1 else 0.08
        aus = huegel(t - v, 0.0, 0.45, 0.55, 0.68)
        zerren = huegel(t - v, 0.74, 0.9, 1.0, 1.3)
        stiel(p, s,
              vor=lambda g: haupt * (-6 * huegel(t - v - 0.02 * g, 0.0, 0.45, 0.55, 0.68) + 17.5 * huegel(t - v - 0.012 * g, 0.56, 0.7, 0.82, 1.35)
                                     - 4 * zerren),
              falle=haupt * (-16 * aus + 14 * huegel(t - v, 0.56, 0.7, 0.82, 1.35)))
    return p


def treffer(t):
    ruck = huegel(t, 0.0, 0.07, 0.12, 0.5)
    p = grund(t, lambda s: -20 * ruck)
    for s in STIELE:
        stiel(p, s, vor=lambda g: -6.5 * huegel(t - 0.02 * g, 0.0, 0.07, 0.12, 0.5), seite=2.0 * ruck * (1 if s["nummer"] % 2 else -1),
              falle=14 * ruck)
    return p


def tod(t):
    lebt = 1 - rampe(t, 0.0, 1.0)
    p = weben(Pose(), t + 2.0, 6.0, staerke=0.6 * lebt, maul=lambda s: 12 * rampe(t, 0.3 + 0.2 * s["nummer"], 1.2 + 0.2 * s["nummer"]))
    for s in STIELE:
        k = s["nummer"]
        start = 0.1 + 0.2 * ((k * 3) % 6)
        faellt = rampe(t, start, start + 0.9)
        faellt = faellt * faellt
        prall = huegel(t, start + 0.85, start + 0.92, start + 0.94, start + 1.2)
        stiel(p, s, aussen=lambda g: (5.0 + 3.2 * g) * faellt - 1.5 * prall * (g / 5.0), falle=18 * faellt)
    return p


AKTIONEN = {
    "Winden": (6.0, winden, True, (0, 30, 60, 90, 120, 150)),
    "Lauern": (5.0, lauern, True, (0, 75)),
    "Aufrichten": (1.0, aufrichten, False, (0, 8, 15, 22, 30)),
    "Zuschnappen": (1.1, zuschnappen, False, (0, 8, 12, 15, 19, 26)),
    "Nachgreifen": (1.4, nachgreifen, False, (0, 12, 18, 21, 27, 36)),
    "Treffer": (0.5, treffer, False, (0, 3, 8)),
    "Tod": (2.4, tod, False, (0, 15, 30, 45, 60, 72)),
}


def main():
    nur = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    datei = "03_animationen.blend" if nur and os.path.exists(gemein.ordner(NAME, "arbeit", "03_animationen.blend")) else "02_skelett.blend"
    obj, arm = gemein.oeffnen(NAME, datei)
    with open(gemein.ordner(NAME, "arbeit", "stiele.json"), encoding="utf-8") as f:
        for s in json.load(f):
            n = s["nummer"]
            kopf = Vector(s["kopf"])
            s["aussen"] = Vector((-kopf.y, kopf.x, 0.0)).normalized() if Vector((kopf.x, kopf.y)).length > 0.12 else Vector((0.0, 1.0, 0.0))
            # which way round the hinge opens: the upper jaw's tip must move away from the lower one's
            ansatz = arm.data.bones["kiefer_%d_o" % n].head_local
            oben = arm.data.bones["kiefer_%d_o" % n].tail_local - ansatz
            unten = arm.data.bones["kiefer_%d_u" % n].tail_local - ansatz
            s["auf"] = 1.0 if Vector(s["scharnier"]).cross(oben).dot(oben - unten) > 0 else -1.0
            STIELE.append(s)
    for aktion, (dauer, pose, schleife, _) in AKTIONEN.items():
        if not nur or aktion in nur:
            bau.backen(NAME, arm, aktion, dauer, pose, schleife)
    bau.ruhe(arm)
    gemein.speichern(NAME, "03_animationen.blend")
    for aktion, (_, _, _, bilder) in AKTIONEN.items():
        if not nur or aktion in nur:
            bau.clip_pruefen(NAME, arm, aktion, bilder, MITTE, 3.6, ("seite", "spiel"))


main()
