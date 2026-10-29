# Puts the Pfuhlnattern into L_Wurzelforst_Blockout (Boris 2026-10-29): one AVaelPfuhlnatterPopulation for the
# Pfuhlmoor with its nests on the banks. The population starts with a few snakes (mostly young, some grown elites, few
# old giants), and the grown ones lay broods now and then: the fewer the players kill, the more there are.
#
# Nests: bank points of the moor (Blockout/Vael_Wurzelforst_Blockout/WF_Moor.json, "ufer", written by wf_moor.py),
# picked far apart. Everything it adds is tagged WF_Pfuhlnatter (Outliner folder Wurzelforst/Kreaturen); rerunning
# replaces only that.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PlacePfuhlnattern.py" -EnablePlugins=PythonScriptPlugin

import json
import os
import random

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MOOR = os.path.join(ROOT, "Blockout", "Vael_Wurzelforst_Blockout", "WF_Moor.json")
TAG = "WF_Pfuhlnatter"
FOLDER = "Wurzelforst/Kreaturen"
NESTS = 9
NEST_SPACING = 3500.0  # cm between two nests
SEED = 29

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(message):
    unreal.log_warning("[PlacePfuhlnattern] " + message)


def pick_nests(banks):
    rng = random.Random(SEED)
    rng.shuffle(banks)
    nests = []
    for x, y, z in banks:
        if all((x - a) ** 2 + (y - b) ** 2 > NEST_SPACING ** 2 for a, b, _ in nests):
            nests.append((x, y, z))
        if len(nests) >= NESTS:
            break
    return nests


def main():
    moor = json.load(open(MOOR, encoding="utf-8"))
    banks = [(p[0], p[1], p[2]) for p in moor["ufer"]]
    nests = pick_nests(banks)
    world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    removed = 0
    for actor in actors.get_all_level_actors():
        if unreal.Name(TAG) in actor.tags:
            actors.destroy_actor(actor)
            removed += 1

    middle = unreal.Vector(sum(n[0] for n in nests) / len(nests), sum(n[1] for n in nests) / len(nests), 0.0)
    population = actors.spawn_actor_from_class(unreal.VaelPfuhlnatterPopulation, middle, unreal.Rotator(0.0, 0.0, 0.0))
    population.set_editor_property("nests", [unreal.Vector(x, y, z) for x, y, z in nests])
    population.set_actor_label("Pfuhlnattern_Pfuhlmoor")
    population.set_folder_path(FOLDER)
    population.set_editor_property("tags", [unreal.Name(TAG)])

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log("population with {} nests placed ({} old removed); nests: {}".format(
        len(nests), removed, ", ".join("{:.0f} {:.0f} m".format(x / 100.0, y / 100.0) for x, y, _ in nests)))


main()
