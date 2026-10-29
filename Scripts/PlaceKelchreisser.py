# Puts the Kelchreisser into L_Wurzelforst_Blockout (Boris 2026-10-29): about a dozen in the whole forest, each the elite
# of its own territory round a giant tree, where the Sleeper's nerve roots run deep to the Nid. The territories go to the
# giant trees nearest the Nid spring, at least SPACING apart, one AVaelCreatureSpawner of one Kelchreisser each, a few
# metres beside the trunk (it roams round the tree from there, AVaelKelchreisser).
#
# Trees: the "Riesenbaum" trunks ("Rinde") of the kit's blockout (Blockout/Vael_Wurzelforst_Blockout/WF_Platzhalter.json),
# the Nid spring from its texts. Everything it adds is tagged WF_Kelchreisser (Outliner folder Wurzelforst/Kreaturen);
# rerunning replaces only that.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PlaceKelchreisser.py" -EnablePlugins=PythonScriptPlugin

import json
import math
import os

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BLOCKOUT = os.path.join(ROOT, "Blockout", "Vael_Wurzelforst_Blockout", "WF_Platzhalter.json")
TAG = "WF_Kelchreisser"
FOLDER = "Wurzelforst/Kreaturen"
TERRITORIES = 12
SPACING = 6000.0      # cm between two territories
BESIDE_TRUNK = 600.0  # cm out from the trunk's bark

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(message):
    unreal.log_warning("[PlaceKelchreisser] " + message)


def giant_trees(blockout):
    """(x, y, z, trunk radius) of every giant tree trunk"""
    trees = []
    for prim in blockout["prims"]:
        if prim.get("mat") == "Rinde" and prim["n"].startswith("Riesenbaum"):
            x, y, z = prim["l"]
            trees.append((x, y, z, 50.0 * max(prim["s"][0], prim["s"][1])))
    return trees


def nid_spring(blockout):
    for text in blockout["texts"]:
        if "Nid" in text.get("text", text.get("t", "")):
            return text["l"]
    raise RuntimeError("no Nid spring among the blockout's texts")


def main():
    blockout = json.load(open(BLOCKOUT, encoding="utf-8"))
    spring = nid_spring(blockout)
    trees = sorted(giant_trees(blockout), key=lambda t: (t[0] - spring[0]) ** 2 + (t[1] - spring[1]) ** 2)
    chosen = []
    for tree in trees:
        if all((tree[0] - c[0]) ** 2 + (tree[1] - c[1]) ** 2 > SPACING ** 2 for c in chosen):
            chosen.append(tree)
        if len(chosen) >= TERRITORIES:
            break

    world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    removed = 0
    for actor in actors.get_all_level_actors():
        if unreal.Name(TAG) in actor.tags:
            actors.destroy_actor(actor)
            removed += 1

    for i, (x, y, z, radius) in enumerate(chosen):
        # On the side of the tree facing the Nid spring
        away = math.atan2(spring[1] - y, spring[0] - x)
        spot = unreal.Vector(x + math.cos(away) * (radius + BESIDE_TRUNK), y + math.sin(away) * (radius + BESIDE_TRUNK), z + 300.0)
        spawner = actors.spawn_actor_from_class(unreal.VaelCreatureSpawner, spot, unreal.Rotator(0.0, 0.0, math.degrees(away)))
        spawner.set_editor_property("creature_kind", unreal.VaelKelchreisserData.static_class())
        spawner.set_editor_property("count", 1)
        spawner.set_editor_property("spread", 0.0)
        spawner.set_actor_label("Kelchreisser_{:02d}".format(i + 1))
        spawner.set_folder_path(FOLDER)
        spawner.set_editor_property("tags", [unreal.Name(TAG)])

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log("{} territories placed ({} old removed, {} giant trees): {}".format(
        len(chosen), removed, len(trees), ", ".join("{:.0f} {:.0f} m".format(c[0] / 100.0, c[1] / 100.0) for c in chosen)))


main()
