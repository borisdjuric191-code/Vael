# Places the plants, fungi and stones of the Aschenmark in L_Aschenmark, each at the places the Kompendium names.
#
# - Only adds: every new actor is an AVaelHarvestable with the tag "VaelNatur" in the Outliner folder "Natur/<kind>".
#   Deleting the folder "Natur" in the editor undoes it.
# - Runs once: if the folder "Natur" exists, nothing is changed. Single spots can then be moved by hand.
#
# Run with the editor closed, after Scripts/CreateHarvestableAssets.py:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PlaceNature.py" -EnablePlugins=PythonScriptPlugin

import random

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Aschenmark"
DATA_FOLDER = "/Game/Vael/Nature"
FOLDER = "Natur"
TAG = "VaelNatur"
TILE = 140.0
MAP_TILES = 64

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(733)

# Kind, Outliner name, tile positions (as in the prototype map, see BuildAschenmark.py)
SPOTS = [
    # Along the road from the camp to the village and at the camp
    ("DA_Harvest_Laternenglocke", "Laternenglocke", [(13.2, 55.4), (15.6, 47.6), (20.4, 46.6), (24.6, 43.4)]),
    # Burnt fields around the chest and the crater mouth
    ("DA_Harvest_Glutdistel", "Glutdistel", [(33.4, 22.6), (37.8, 19.8), (26.2, 17.4), (41.6, 14.2), (44.2, 6.4)]),
    # Shore of the pond
    ("DA_Harvest_Traenenkelch", "Traenenkelch", [(19.4, 28.2), (13.4, 34.4), (8.2, 32.2)]),
    # Inside the crater, along its rim
    ("DA_Harvest_Aschblase", "Aschblase", [(47.4, 12.6), (56.4, 13.4), (53.2, 5.2)]),
    # Village ruins and dead wood
    ("DA_Harvest_Russmoos", "Russmoos", [(25.2, 38.6), (21.2, 44.4), (32.2, 37.6), (41.8, 47.2)]),
    # Around the Mark source
    ("DA_Harvest_Fleischkelch", "Fleischkelch", [(42.8, 20.6), (48.4, 24.6)]),
    # Graveyard south of the chapel
    ("DA_Harvest_Pfeifenmorchel", "Pfeifenmorchel", [(34.4, 52.4), (40.0, 52.2), (36.6, 54.6)]),
    # Open ash fields
    ("DA_Harvest_Aschenstaeubling", "Aschenstaeubling", [(36.2, 26.4), (23.8, 22.2), (40.4, 30.4), (25.4, 27.6)]),
    # Between the fields and the Mark source, pointing to it
    ("DA_Harvest_Adernflechte", "Adernflechte", [(38.6, 23.4), (40.8, 22.2), (42.6, 25.6)]),
    # Village
    ("DA_Harvest_Schlacke", "Schlacke", [(31.4, 44.2), (27.2, 38.4), (22.4, 31.4)]),
    # Crater and fields
    ("DA_Harvest_Glutstein", "Glutstein", [(48.4, 16.4), (57.4, 9.6), (34.2, 13.2)]),
    # Crater rim, inside
    ("DA_Harvest_Russquarz", "Russquarz", [(45.6, 9.6), (51.6, 4.6), (58.2, 15.4)]),
    # At the Mark source
    ("DA_Harvest_Markdruse", "Markdruse", [(47.6, 20.8), (43.8, 24.2)]),
]


def world(x, y, z=0.0):
    """World location of a prototype tile position"""
    return unreal.Vector((MAP_TILES / 2 - x) * TILE, (MAP_TILES / 2 - y) * TILE, z)


def place():
    world_object = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    if world_object is None:
        unreal.log_error("Vael: could not load {}".format(LEVEL_PATH))
        return

    folders = set(str(actor.get_folder_path()) for actor in actors.get_all_level_actors())
    if any(folder == FOLDER or folder.startswith(FOLDER + "/") for folder in folders):
        unreal.log_warning("Vael: {} already has the folder '{}', nothing was changed".format(LEVEL_PATH, FOLDER))
        return

    placed = 0
    for asset, label, spots in SPOTS:
        data = unreal.load_asset("{}/{}".format(DATA_FOLDER, asset))
        if data is None:
            unreal.log_error("Vael: {} is missing, run CreateHarvestableAssets.py first".format(asset))
            continue

        for x, y in spots:
            actor = actors.spawn_actor_from_class(unreal.VaelHarvestable, world(x, y), unreal.Rotator(0.0, 0.0, rng.uniform(0.0, 360.0)))
            actor.set_editor_property("data", data)
            actor.set_folder_path("{}/{}".format(FOLDER, label))
            actor.set_actor_label(label)
            actor.set_editor_property("tags", [unreal.Name(TAG)])
            placed += 1

    if unreal.EditorLoadingAndSavingUtils.save_map(world_object, LEVEL_PATH):
        unreal.log("Vael: placed {} plants, fungi and stones in {}".format(placed, LEVEL_PATH))
    else:
        unreal.log_error("Vael: {} could not be saved".format(LEVEL_PATH))


place()
