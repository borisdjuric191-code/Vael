# Fills the village well of L_Aschenmark with water of the water kit (MI_Water_Well, see CreateWaterKit.py).
#
# - Only adds: the water surface and a dark shaft floor below it, tag "VaelKulisse", Outliner folder "Dorf/Kulisse/Brunnenwasser".
#   Deleting that folder in the editor undoes it; the script skips the well while the folder exists.
# - The grey blockout cylinder inside the well is made invisible and keeps its collision (tag "VaelPlatzhalterVersteckt").
# - The well's shaft was measured from SM_WellBase: inner radius about 54 cm, rim at 89 cm above its origin.
#
# Run with the editor closed, after CreateWaterKit.py:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/AddWellWater.py" -EnablePlugins=PythonScriptPlugin

import os

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Aschenmark"
FOLDER = "Dorf/Kulisse/Brunnenwasser"
WELL_FOLDER = "Dorf/Kulisse/Brunnen"
WATER_MATERIAL = "/Game/VFX/Water/Materials/MI_Water_Well"
FLOOR_MATERIAL = "/Game/Vael/Maps/Kulisse/M_Vael_Wasser"
PLANE = "/Engine/BasicShapes/Plane.Plane"

DRESSING_TAG = "VaelKulisse"
HIDDEN_TAG = "VaelPlatzhalterVersteckt"

# Heights above the well's origin in cm: the water lies a hand's breadth below the rim, the dark floor deeper below it
WATER_HEIGHT = 62.0
FLOOR_HEIGHT = 24.0

# Side of the square planes in cm: wider than the shaft (54 cm radius) so their edges vanish inside the stone ring (outer radius about 100 cm)
PLANE_SIZE = 125.0

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def add_plane(location, yaw, material, label):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, location, unreal.Rotator(0.0, 0.0, yaw))
    actor.static_mesh_component.set_static_mesh(unreal.load_asset(PLANE))
    actor.static_mesh_component.set_material(0, material)
    actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    actor.static_mesh_component.set_editor_property("cast_shadow", False)
    actor.set_actor_scale3d(unreal.Vector(PLANE_SIZE / 100.0, PLANE_SIZE / 100.0, 1.0))
    actor.set_folder_path(FOLDER)
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(DRESSING_TAG)])
    return actor


def main():
    world_object = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    if world_object is None:
        unreal.log_error("Vael: could not load {}".format(LEVEL_PATH))
        return

    level_actors = list(actors.get_all_level_actors())
    lines = []

    if any(str(actor.get_folder_path()) == FOLDER for actor in level_actors):
        lines.append("skipped: the well already has its water")
    else:
        base = next((actor for actor in level_actors if str(actor.get_folder_path()) == WELL_FOLDER and actor.get_actor_label() == "WellBase"), None)
        water = unreal.load_asset(WATER_MATERIAL)
        floor = unreal.load_asset(FLOOR_MATERIAL)

        if base is None or water is None or floor is None:
            lines.append("missing: well base {}, water material {}, floor material {}".format(base is not None, water is not None, floor is not None))
        else:
            origin = base.get_actor_location()
            yaw = base.get_actor_rotation().yaw
            add_plane(unreal.Vector(origin.x, origin.y, origin.z + FLOOR_HEIGHT), yaw, floor, "Brunnenschacht")
            add_plane(unreal.Vector(origin.x, origin.y, origin.z + WATER_HEIGHT), yaw, water, "Brunnenwasser")
            lines.append("water at {:.0f} {:.0f} {:.0f}".format(origin.x, origin.y, origin.z + WATER_HEIGHT))

            for actor in level_actors:
                if isinstance(actor, unreal.StaticMeshActor) and actor.get_actor_label() == "Brunnen" and str(actor.get_folder_path()) == "Dorf":
                    actor.static_mesh_component.set_editor_property("visible", False)
                    actor.set_actor_hidden_in_game(True)
                    actor.set_editor_property("tags", list(actor.get_editor_property("tags")) + [unreal.Name(HIDDEN_TAG)])
                    lines.append("blockout well hidden")

    saved = len(lines) > 1 and unreal.EditorLoadingAndSavingUtils.save_map(world_object, LEVEL_PATH)
    lines.insert(0, "saved: {}".format(saved))
    with open(os.path.join(unreal.Paths.project_saved_dir(), "WellWaterLog.txt"), "w", encoding="utf-8") as file:
        file.write("\n".join(lines))


main()
