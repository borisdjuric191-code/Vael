"""Check sheet of a Kobold rig: chosen clips at a few frames, side and game view, in one picture
(vorschau/pruef_<Name>.png). For looking at hands, gear and the whip without opening Blender.

    blender -b --factory-startup -P Tools/Nachtwesen/kobold_pruefbild.py -- <Name> [Aktion ...]
"""
import os
import sys

import bpy
import numpy as np

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bau  # noqa: E402
import gemein  # noqa: E402

AKTIONEN = ("Wach_Idle", "Schleichen", "Kratzen", "Schlag", "Peitschen", "Zaubern", "Werfen")
NAH = bool(os.environ.get("KOBOLD_NAH"))
FARBEN = {"Peitsche_Huefte": (0.35, 0.22, 0.12, 1), "Peitsche_Hand": (0.45, 0.28, 0.14, 1), "Ausruestung": (0.6, 0.35, 0.7, 1)}


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    name = args[0]
    aktionen = args[1:] or AKTIONEN
    obj, arm = gemein.oeffnen(name, "03_animationen.blend")
    for m in obj.data.materials:
        if m is not None and m.name in FARBEN:
            m.diffuse_color = FARBEN[m.name]
    hoehe = max(v.co.z for v in obj.data.vertices)
    scene = bpy.context.scene
    zeilen = []
    pfad = gemein.ordner(name, "vorschau", "pruef")
    for aktion in aktionen:
        act = bpy.data.actions.get(aktion)
        if act is None:
            continue
        bau.aktion_zeigen(arm, aktion)
        a, e = act.frame_range
        bilder = [int(a + (e - a) * f) for f in (0.15, 0.4, 0.55, 0.8)]
        reihe = []
        for b in bilder:
            scene.frame_set(b)
            for ansicht in ("seite", "spiel") if not NAH else ("vorne", "seite"):
                datei = os.path.join(pfad, "%s_%d_%s.png" % (aktion, b, ansicht))
                if NAH:
                    # close on the right hand
                    hand = arm.matrix_world @ arm.pose.bones["hand_r"].head
                    gemein.rendern(datei, gemein.ANSICHTEN[ansicht], tuple(hand), 0.45 * hoehe, 220)
                else:
                    gemein.rendern(datei, gemein.ANSICHTEN[ansicht], (0.2 * hoehe, 0.0, 0.55 * hoehe), 2.0 * hoehe, 220)
                img = bpy.data.images.load(datei)
                reihe.append(np.array(img.pixels[:], dtype=np.float32).reshape(img.size[1], img.size[0], 4))
        zeilen.append(np.concatenate(reihe, axis=1))
    bild = np.concatenate(zeilen[::-1], axis=0)
    out = bpy.data.images.new("pruef", bild.shape[1], bild.shape[0])
    out.pixels[:] = bild.ravel()
    out.filepath_raw = gemein.ordner(name, "vorschau", "pruef_%s%s.png" % ("nah_" if NAH else "", name))
    out.file_format = "PNG"
    out.save()
    gemein.log(name, "Pruefbild:", out.filepath_raw, "Aktionen:", list(aktionen))


main()
