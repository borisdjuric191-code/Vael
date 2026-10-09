# Places the quest markers of Act I in L_Aschenmark: invisible places quest steps of type Reach send the players to.
#
# - Only adds: AVaelQuestMarker actors in the Outliner folder "Quests". Deleting the folder undoes it.
# - Runs once: if the folder "Quests" exists, nothing is changed.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PlaceQuestMarkers.py" -EnablePlugins=PythonScriptPlugin

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Aschenmark"
FOLDER = "Quests"
TILE = 140.0
MAP_TILES = 64

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Marker id, tile position (as in Scripts/BuildAschenmark.py), radius in cm
MARKERS = [
    ("Dorf", (29.0, 40.0), 900.0),
    ("Brunnen", (29.5, 39.5), 260.0),
    ("Orden", (53.5, 41.0), 800.0),
    ("Krater", (52.0, 11.0), 1100.0),
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
    if FOLDER in folders:
        unreal.log_warning("Vael: {} already has the folder '{}', nothing was changed".format(LEVEL_PATH, FOLDER))
        return

    for marker_id, (x, y), radius in MARKERS:
        marker = actors.spawn_actor_from_class(unreal.VaelQuestMarker, world(x, y, 50.0))
        marker.set_editor_property("marker_id", unreal.Name(marker_id))
        marker.set_editor_property("radius", radius)
        marker.set_folder_path(FOLDER)
        marker.set_actor_label("Questziel {}".format(marker_id))

    if unreal.EditorLoadingAndSavingUtils.save_map(world_object, LEVEL_PATH):
        unreal.log("Vael: placed {} quest markers in {}".format(len(MARKERS), LEVEL_PATH))
    else:
        unreal.log_error("Vael: {} could not be saved".format(LEVEL_PATH))


place()
