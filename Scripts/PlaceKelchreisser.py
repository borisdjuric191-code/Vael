# Puts the Kelchreisser into L_Wurzelforst_Blockout (Boris 2026-10-29): every giant tree has its own, the elite of its
# territory, where the Sleeper's nerve roots run deep to the Nid. One AVaelCreatureSpawner of one Kelchreisser beside
# each trunk (it roams round the tree from there, AVaelKelchreisser).
#
# Trees: the level's own "Riesenbaum_NN_Stamm" actors (the blockout JSON is in other coordinates). The spawner stands
# just outside the buttress roots on the side facing the forest's middle, a little above the trunk's foot, so the
# creature finds the ground there when the level starts. Everything it adds is tagged WF_Kelchreisser (Outliner folder
# Wurzelforst/Kreaturen); rerunning replaces only that.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PlaceKelchreisser.py" -EnablePlugins=PythonScriptPlugin

import math

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
TAG = "WF_Kelchreisser"
FOLDER = "Wurzelforst/Kreaturen"
BESIDE_ROOTS = 700.0  # cm out from the trunk's bark, past the buttress roots
ABOVE_FOOT = 400.0    # cm above the trunk's foot (the spawner looks for ground from 5 m above to 20 m below itself)

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(message):
    unreal.log_warning("[PlaceKelchreisser] " + message)


def main():
    world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    removed = 0
    trunks = []
    for actor in actors.get_all_level_actors():
        if unreal.Name(TAG) in actor.tags:
            actors.destroy_actor(actor)
            removed += 1
            continue
        label = actor.get_actor_label()
        if label.startswith("Riesenbaum_") and label.endswith("_Stamm"):
            origin, extent = actor.get_actor_bounds(False)
            trunks.append((label, origin, extent))
    trunks.sort(key=lambda t: t[0])
    if not trunks:
        log("no Riesenbaum_NN_Stamm actors in the level")
        return

    middle_x = sum(t[1].x for t in trunks) / len(trunks)
    middle_y = sum(t[1].y for t in trunks) / len(trunks)
    for label, origin, extent in trunks:
        towards = math.atan2(middle_y - origin.y, middle_x - origin.x)
        out = max(extent.x, extent.y) + BESIDE_ROOTS
        spot = unreal.Vector(origin.x + math.cos(towards) * out, origin.y + math.sin(towards) * out,
                             origin.z - extent.z + ABOVE_FOOT)
        spawner = actors.spawn_actor_from_class(unreal.VaelCreatureSpawner, spot, unreal.Rotator(0.0, 0.0, math.degrees(towards)))
        spawner.set_editor_property("creature_kind", unreal.VaelKelchreisserData.static_class())
        spawner.set_editor_property("count", 1)
        spawner.set_editor_property("spread", 0.0)
        spawner.set_actor_label("Kelchreisser_" + label.split("_")[1])
        spawner.set_folder_path(FOLDER)
        spawner.set_editor_property("tags", [unreal.Name(TAG)])

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log("{} Kelchreisser placed, one at each giant tree ({} old spawners removed)".format(len(trunks), removed))


main()
