"""Helper for step 2: measures landmarks on the prepared lynx (paws, leg columns, back line, head, ears, tail) and
prints them, so the skeleton can be placed from numbers. Writes arbeit/landmarken.json.

    blender -b --factory-startup -P skripte/02_analyse_punkte.py
"""
import json
import os
import sys

import bpy
from mathutils import Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import vorschau  # noqa: E402

ORDNER = vorschau.ORDNER


def mitte(pts):
    return sum(pts, Vector()) / len(pts) if pts else None


def main():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(ORDNER, "arbeit", "01_vorbereitet.blend"))
    obj = bpy.data.objects["Rankenluchs"]
    P = [v.co.copy() for v in obj.data.vertices]
    lo_x = min(p.x for p in P)
    hi_x = max(p.x for p in P)
    out = {}

    # Paws: points near the ground, split front/back by the gap along X, left/right by Y
    boden = [p for p in P if p.z < 0.06]
    xs = sorted(p.x for p in boden)
    luecke = max(range(1, len(xs)), key=lambda i: xs[i] - xs[i - 1])
    teil_x = (xs[luecke] + xs[luecke - 1]) / 2
    for vorn in (True, False):
        gruppe = [p for p in boden if (p.x > teil_x) == vorn]
        ym = sum(p.y for p in gruppe) / len(gruppe)
        for links in (True, False):
            seite = [p for p in gruppe if (p.y > ym) == links]
            out["pfote_" + ("v" if vorn else "h") + ("l" if links else "r")] = list(mitte(seite))
    # Leg columns: centre of the points around each paw column at several heights
    for name in [k for k in out if k.startswith("pfote_")]:
        px, py = out[name][0], out[name][1]
        saeule = []
        for z in (0.05, 0.12, 0.2, 0.28, 0.36, 0.44, 0.52):
            ring = [p for p in P if abs(p.z - z) < 0.02 and abs(p.x - px) < 0.13 and (p.y - py) * (1 if py > 0 else -1) > -0.06
                    and abs(p.y - py) < 0.12]
            m = mitte(ring)
            saeule.append([round(z, 2)] + ([round(v, 3) for v in m] if m else [None] * 3) + [len(ring)])
        out[name.replace("pfote", "bein")] = saeule
    # Back line: highest point per X slice, mid width
    ruecken = []
    for k in range(30):
        x0 = lo_x + (hi_x - lo_x) * k / 30
        s = [p for p in P if x0 <= p.x < x0 + (hi_x - lo_x) / 30 and abs(p.y) < 0.12]
        if s:
            top = max(s, key=lambda p: p.z)
            low = min(s, key=lambda p: p.z)
            ruecken.append([round(x0, 3), round(top.z, 3), round(low.z, 3), len(s)])
    out["laengsschnitt"] = ruecken
    # Ears: highest points left and right
    for links in (True, False):
        s = [p for p in P if (p.y > 0) == links and p.x > hi_x - 0.35]
        tip = max(s, key=lambda p: p.z)
        out["ohrspitze_" + ("l" if links else "r")] = [round(v, 3) for v in tip]
    # Nose: the most forward point
    nase = max(P, key=lambda p: p.x)
    out["nase"] = [round(v, 3) for v in nase]
    print("[Landmarken]", json.dumps(out))
    with open(os.path.join(ORDNER, "arbeit", "landmarken.json"), "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1)


main()
