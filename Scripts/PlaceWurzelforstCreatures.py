# Puts the creature spawners of the Wurzelforst into L_Wurzelforst_Blockout, at the places of the kit's wf_kreaturen.py:
# packs of three Rankenluchs in ambush in the thickets beside the trails, pairs of Spannhornkaefer in the Hain and on the
# Kammweg, single Schleieraffen in the forest, Tauraupen (the light form) in the Hain and Forstsaum and Draculaupen
# near the Nid spring in the Pfuhlmoor. Each group is one AVaelCreatureSpawner (the creatures appear when the
# level starts, on the ground found there).
# Everything it adds is tagged WF_Kreaturen (Outliner folder Wurzelforst/Kreaturen); rerunning replaces only that.
#
# Run with the editor closed, after the kit's wf_kreaturen.py:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PlaceWurzelforstCreatures.py" -EnablePlugins=PythonScriptPlugin

import json
import os

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
KIT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Blockout", "Vael_Wurzelforst_Blockout")
TAG = "WF_Kreaturen"
FOLDER = "Wurzelforst/Kreaturen"
ARTEN = {
    "Rankenluchs": unreal.VaelRankenluchsData,
    "Spannhornkaefer": unreal.VaelHornBeetleData,
    "Schleieraffe": unreal.VaelVeilMonkeyData,
    "Tauraupe": unreal.VaelTauraupeData,
    "Draculaupe": unreal.VaelDraculaupeData,
}

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(message):
    unreal.log_warning("[PlaceWurzelforstCreatures] " + message)


def main():
    data = json.load(open(os.path.join(KIT, "WF_Kreaturen.json"), encoding="utf-8"))
    world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    removed = 0
    for actor in actors.get_all_level_actors():
        if unreal.Name(TAG) in actor.tags:
            actors.destroy_actor(actor)
            removed += 1

    counts = {}
    for i, gruppe in enumerate(data["gruppen"]):
        art = gruppe["art"]
        x, y, z = gruppe["ort"]
        spawner = actors.spawn_actor_from_class(unreal.VaelCreatureSpawner, unreal.Vector(x, y, z + 100.0), unreal.Rotator(0.0, 0.0, 0.0))
        spawner.set_editor_property("creature_kind", ARTEN[art].static_class())
        spawner.set_editor_property("count", int(gruppe["anzahl"]))
        spawner.set_editor_property("spread", float(gruppe["streuung_cm"]))
        spawner.set_actor_label("{}_{:02d}".format(art, i + 1))
        spawner.set_folder_path(FOLDER)
        spawner.set_editor_property("tags", [unreal.Name(TAG)])
        counts[art] = counts.get(art, 0) + int(gruppe["anzahl"])

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log("spawners placed: {} groups ({}); {} old ones removed".format(
        len(data["gruppen"]), ", ".join("{} {}".format(v, k) for k, v in sorted(counts.items())), removed))


main()
