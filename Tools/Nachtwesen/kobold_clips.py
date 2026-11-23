"""Kobolde, step 3: the Schleimgaenger movements (schleimgaenger_clips.py) on a Kobold body. An infested Kobold is a
Schleimgaenger like the skeleton, so it lies, is hauled up, staggers, strikes, grabs, flinches and falls the same way.

The clips are written for a 1.8 m skeleton (pelvis at 0.968 m). The Kobold is scaled up until its pelvis is there,
the clips are baked with its own hands and feet as targets, then body, bones and the moved distances in the keys are
scaled back to its real size. Rotations don't change with a uniform scale, so the movement stays the same.

    blender -b --factory-startup -P Tools/Nachtwesen/kobold_clips.py -- <Name>
Writes arbeit/03_animationen.blend and check pictures in vorschau/clips/.
"""
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bau  # noqa: E402
import gemein  # noqa: E402
import schleimgaenger_clips as sg  # noqa: E402  (patches bau.pose_setzen with the joint IK)

BECKEN_SKELETT = 0.968


def skalieren(obj, arm, f):
    """Scales body, bones and every baked translation by f"""
    obj.data.transform(Matrix.Scale(f, 4))
    arm.data.transform(Matrix.Scale(f, 4))
    for act in bpy.data.actions:
        kurven = list(getattr(act, "fcurves", []) or [])
        for layer in getattr(act, "layers", []):
            for strip in layer.strips:
                for cb in strip.channelbags:
                    kurven.extend(cb.fcurves)
        for fc in kurven:
            if fc.data_path.endswith(".location"):
                for kp in fc.keyframe_points:
                    kp.co.y *= f
                    kp.handle_left.y *= f
                    kp.handle_right.y *= f


def hochskalieren(obj, arm):
    """Scales the Kobold up to the skeleton's size; returns the factor"""
    k = BECKEN_SKELETT / arm.data.bones["pelvis"].head_local.z
    skalieren(obj, arm, k)
    bpy.context.view_layer.update()
    return k


def ziele_setzen(arm, name):
    """Its own hands and feet as the targets of the Schleimgaenger poses: feet where they stand, hands below the shoulders"""
    knochen = arm.data.bones
    for seite in ("l", "r"):
        s = 1.0 if seite == "l" else -1.0
        kn = knochen["foot_" + seite].head_local
        sg.FUSS_STEHEN[seite] = Vector((kn.x, kn.y, kn.z))
        sch = knochen["upperarm_" + seite].head_local
        ell = knochen["lowerarm_" + seite].head_local
        hg = knochen["hand_" + seite].head_local
        lang = (ell - sch).length + (hg - ell).length
        sg.HAND_STEHEN[seite] = Vector((sch.x + 0.05, sch.y + 0.05 * s, sch.z - 0.9 * lang))
        sg.FUSS_LIEGEN[seite] = Vector((knochen["pelvis"].head_local.x + 0.05 + (knochen["thigh_" + seite].head_local.z - kn.z),
                                        kn.y * 1.4, 0.08))
    sg.NAME = name
    # K_ rigs: hands turn with the forearm; the ones with something in the right hand close the fingers round it
    sg.HAND_FOLGT = name.startswith("K_")
    sg.GRIFF.clear()
    if name.startswith(("K_Hortwaechter", "K_Sklaventreiber")):
        sg.GRIFF["r"] = dict(seite=55.0)


def main():
    name = sys.argv[sys.argv.index("--") + 1]
    obj, arm = gemein.oeffnen(name, "02_skelett.blend")
    k = hochskalieren(obj, arm)
    ziele_setzen(arm, name)
    gemein.log(name, "Faktor %.3f, Fuesse %s, Haende %s" % (k, [tuple(round(c, 2) for c in v) for v in sg.FUSS_STEHEN.values()],
                                                          [tuple(round(c, 2) for c in v) for v in sg.HAND_STEHEN.values()]))
    for aktion, (dauer, pose, schleife, _) in sg.AKTIONEN.items():
        bau.backen(name, arm, aktion, dauer, pose, schleife)
    bau.ruhe(arm)
    skalieren(obj, arm, 1.0 / k)
    bpy.context.view_layer.update()
    gemein.speichern(name, "03_animationen.blend")
    hoehe = max(v.co.z for v in obj.data.vertices)
    for aktion, (_, _, _, bilder) in sg.AKTIONEN.items():
        if aktion in ("Aufstehen", "Gehen", "Schlag", "Tod"):
            bau.clip_pruefen(name, arm, aktion, bilder[1:4], (0.15 * hoehe, 0.0, 0.5 * hoehe), 1.5 * hoehe, ("seite",), 260)
    gemein.log(name, "Clips fertig, Gehen %.1f cm/s bei eigenem Tempo" % (100.0 * 2 * sg.SCHRITT / sg.GEHEN_DAUER / k))


if __name__ == "__main__":
    main()
