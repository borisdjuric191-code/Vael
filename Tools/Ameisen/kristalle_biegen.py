"""
Gebogene Kristallstücke für den Belag der Erztochter (Boris 27.11.: dünne, gerade Erze wie das Funkenerz biegen und
formen, so dass sie überall sauber am Körper anliegen).

Nimmt jedes Stück aus ameise_kristalle.py (Modus 'export': SM_AK_<Erz>_<k>.fbx, Fuß bei 0, Achse +Z) und biegt es
gleichmäßig über seine Länge zur Seite -X hin (Bogen um eine Achse parallel zu Y). Im Spiel wird das Stück so gedreht,
dass -X zur Körperoberfläche zeigt: die Spitze legt sich an Horn oder Flügel an. Schreibt SM_AK_<Erz>_<k>_B.fbx und
SM_AK_<Erz>_B.json (Breite wie vorher, Länge = Höhe nach dem Biegen).

  blender -b -P Tools/Ameisen/kristalle_biegen.py -- <Ordner mit SM_AK_*.fbx> [Grad]
"""
import glob
import json
import math
import os
import re
import sys

import bpy

args = sys.argv[sys.argv.index("--") + 1:]
ORDNER = args[0]
GRAD = float(args[1]) if len(args) > 1 else 45.0

gruppen = {}
for pfad in sorted(glob.glob(os.path.join(ORDNER, "SM_AK_*.fbx"))):
    name = os.path.splitext(os.path.basename(pfad))[0]
    if name.endswith("_B"):
        continue
    erz = name.split("_")[2]
    breiten = {e["name"]: e["breite"] for e in json.load(open(os.path.join(ORDNER, "SM_AK_{}.json".format(erz)), encoding="utf-8"))}

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=pfad)
    obj = next(o for o in bpy.context.scene.objects if o.type == "MESH")
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    me = obj.data
    laenge = max(v.co.z for v in me.vertices)
    if laenge <= 0:
        continue
    winkel = math.radians(GRAD)
    R = laenge / winkel                     # Bogenradius: über die ganze Länge GRAD Grad
    for v in me.vertices:
        x, y, z = v.co
        z = max(0.0, z)
        a = z / R
        rr = R + x                          # Abstand zur Biegeachse (sie liegt bei x = -R)
        v.co = (-R + rr * math.cos(a), y, rr * math.sin(a))
    me.update()
    neu = name + "_B"
    obj.name = me.name = neu
    bpy.ops.export_scene.fbx(filepath=os.path.join(ORDNER, neu + ".fbx"), use_selection=True, object_types={"MESH"},
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL", bake_anim=False,
                             mesh_smooth_type="FACE", path_mode="STRIP")
    hoehe = max(v.co.z for v in me.vertices)
    gruppen.setdefault(erz, []).append({"name": neu, "breite": breiten.get(name, 0.1), "laenge": hoehe,
                                        "laenge_gerade": laenge})
    print("GEBOGEN", neu, round(laenge, 3), "->", round(hoehe, 3))

for erz, info in gruppen.items():
    with open(os.path.join(ORDNER, "SM_AK_{}_B.json".format(erz)), "w", encoding="utf-8") as f:
        json.dump(info, f, indent=1)
print("FERTIG", sum(len(i) for i in gruppen.values()), "Stuecke")
