# Places the creature groups of Tools/Welt/aschenmark_gegner.py (Scripts/Data/Welt/AM_Gegner.json) in L_Vael as near-player
# spawners: a group wakes when a player comes within 65 m, goes away unseen beyond 130 m and comes back 5 minutes after
# it was defeated; stronger farther from the camp, now and then with an elite. Idempotent (tag VaelGegner).
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PlaceGegner.py" -EnablePlugins=PythonScriptPlugin -unattended

import json
import os

import unreal

DATA = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", "Welt")
LEVEL = "/Game/Vael/Maps/L_Vael"
TAG = "VaelGegner"
M = 100.0
ZONES = {"K": "Kesselgrund", "S": "Schlackenhaenge", "G": "Glutfelder", "P": "Aschenpass"}


def log(message):
    unreal.log_warning("[PlaceGegner] " + message)


def main():
    groups = json.load(open(os.path.join(DATA, "AM_Gegner.json"), encoding="utf-8"))["gruppen"]
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL):
        log("could not load " + LEVEL)
        return
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    old = [a for a in actors.get_all_level_actors() if unreal.Name(TAG) in a.get_editor_property("tags")]
    for actor in old:
        actors.destroy_actor(actor)
    placed = 0
    for g in groups:
        kind = getattr(unreal, g["art"], None)
        if kind is None:
            log("unknown kind " + g["art"])
            continue
        spawner = actors.spawn_actor_from_class(unreal.VaelCreatureSpawner, unreal.Vector(g["x"] * M, g["y"] * M, g["z"] * M + 100.0), unreal.Rotator(0, 0, 0))
        spawner.set_editor_property("creature_kind", kind.static_class())
        spawner.set_editor_property("count", g["anzahl"])
        spawner.set_editor_property("spread", 260.0)
        spawner.set_editor_property("near_players_only", True)
        spawner.set_editor_property("strength", g["staerke"])
        spawner.set_editor_property("elite_chance", g["elite"])
        spawner.set_folder_path("Gegner/Gruppen/" + ZONES[g["zone"]])
        spawner.set_actor_label(g["art"].replace("Vael", "").replace("Data", ""))
        spawner.set_editor_property("tags", [unreal.Name(TAG)])
        placed += 1
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
    log("{} old removed, {} groups placed".format(len(old), placed))


main()
