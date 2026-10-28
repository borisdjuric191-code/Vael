# Diablo-like encounters for L_Wurzelforst_Blockout (Boris 2026-10-28): packs keep appearing ahead of the group while
# it walks, instead of fixed groups on the map.
#
# - Data asset DA_Begegnungen_Wurzelforst (UVaelEncounterTable): which packs come and how often. Rankenluchs packs of
#   three (more at night), Spannhornkaefer in twos and threes, Schleieraffen alone or in pairs (mostly at dusk and
#   night). Only on dry ground (the lakes and the moor stand at 0-6 cm). New kinds (Distelspinne, Pfuhlnatter,
#   Peitschenunke) are added here once they exist.
# - One AVaelEncounterDirector in the level that uses it.
# - The fixed spawners of these common kinds are taken out by Scripts/PlaceWurzelforstCreatures.py, which now only places
#   the special ones (Tauraupen, Draculaupen at the Nid spring); run it after this one.
# Everything it adds to the level is tagged WF_Begegnungen; rerunning replaces only that. Values can be tuned in the
# data asset in the editor.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PlaceWurzelforstEncounters.py" -EnablePlugins=PythonScriptPlugin

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
TABLE_FOLDER = "/Game/Vael/Kreaturen/Begegnungen"
TABLE_NAME = "DA_Begegnungen_Wurzelforst"
TAG = "WF_Begegnungen"
FOLDER = "Wurzelforst/Kreaturen"
DRY = 60.0  # cm: lowest ground for a pack, above lake and moor water

# kind, count range, spread in cm, weight, night weight
PACKS = [
    (unreal.VaelRankenluchsData, (3, 3), 300.0, 4.0, 1.5),
    (unreal.VaelHornBeetleData, (2, 3), 320.0, 3.0, 0.8),
    (unreal.VaelVeilMonkeyData, (1, 2), 380.0, 1.5, 2.5),
]

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assets = unreal.EditorAssetLibrary


def log(message):
    unreal.log_warning("[PlaceWurzelforstEncounters] " + message)


def build_table():
    path = "{}/{}".format(TABLE_FOLDER, TABLE_NAME)
    if assets.does_asset_exist(path):
        table = unreal.load_asset(path)
    else:
        table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(TABLE_NAME, TABLE_FOLDER, unreal.VaelEncounterTable, None)
    packs = []
    for kind, count, spread, weight, night in PACKS:
        pack = unreal.VaelEncounterPack()
        pack.set_editor_property("creature_kind", kind.static_class())
        pack.set_editor_property("count", unreal.IntPoint(count[0], count[1]))
        pack.set_editor_property("spread", spread)
        pack.set_editor_property("weight", weight)
        pack.set_editor_property("night_weight", night)
        pack.set_editor_property("ground_height", unreal.Vector2D(DRY, 1.0e6))
        packs.append(pack)
    table.set_editor_property("packs", packs)
    table.set_editor_property("min_ground_height", DRY)
    assets.save_loaded_asset(table, False)
    return table


def main():
    table = build_table()
    world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    removed = 0
    for actor in actors.get_all_level_actors():
        if unreal.Name(TAG) in actor.tags:
            actors.destroy_actor(actor)
            removed += 1
    director = actors.spawn_actor_from_class(unreal.VaelEncounterDirector, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
    director.set_editor_property("table", table)
    director.set_actor_label("Begegnungen_Wurzelforst")
    director.set_folder_path(FOLDER)
    director.set_editor_property("tags", [unreal.Name(TAG)])
    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log("encounter director placed with {} packs ({} old ones removed)".format(len(table.get_editor_property("packs")), removed))


main()
