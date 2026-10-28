# Stops the pack trees at the Pfuhlmoor from stretching (Boris 2026-10-28: along the moor the pack trees "shrink and
# grow up again" all the time). They are the Black Alders and dead pines BuildMoorWasser.py puts on the banks, as
# instances in AVaelScatterField actors; the packs' wind (Megascans SimpleWind, stored pivots in the UVs) does not
# survive the instancing and pulls whole trees up and down. Their world position offset is switched off, so the trees
# stand still; leaves, grass and the mangroves keep their own sway.
#
# Only the scatter fields of L_Wurzelforst_Blockout with these tree meshes are touched (tag WF_BaumStill). Rerunning
# is harmless. Undo: tick "Evaluate World Position Offset" on the field's Instances component again.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/StillMoorTrees.py" -EnablePlugins=PythonScriptPlugin

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
TREE_PACKS = ("/Game/BlackAlder/", "/Game/Megaplant_Library/Tree_Dead_Pine")
TAG = "WF_BaumStill"


def log(message):
    unreal.log_warning("[StillMoorTrees] " + message)


def main():
    world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    stilled = []
    for actor in actors.get_all_level_actors():
        if not isinstance(actor, unreal.VaelScatterField):
            continue
        component = actor.get_editor_property("instances")
        mesh = component.get_editor_property("static_mesh") if component is not None else None
        if mesh is None or not mesh.get_path_name().startswith(TREE_PACKS):
            continue
        component.set_editor_property("evaluate_world_position_offset", False)
        if unreal.Name(TAG) not in actor.tags:
            actor.set_editor_property("tags", list(actor.tags) + [unreal.Name(TAG)])
        stilled.append("{} ({} trees)".format(actor.get_actor_label(), component.get_instance_count()))
    if stilled:
        unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log("{} tree fields stand still: {}".format(len(stilled), ", ".join(stilled)))


main()
