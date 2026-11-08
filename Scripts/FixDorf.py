# The village and the old blockouts in L_Vael (goal 8.11. evening):
# - invisible walls go: the hidden blockout cubes of the houses, the crater rocks and two camp crates kept their collision
#   when the real pieces took their place; they block nothing visible any more, so they lose it (the houses can be entered
#   through their gaps; the low ruin walls collide by themselves). Coast and border walls, palisades and deep water stay.
# - the chapel was only hidden cubes: every pillar becomes a visible broken corner of masonry
# - every pack wall in the level (houses, storehouse, ruins) gets its own instance of the scenery master: its own textures
#   as the base, the moss drained, ash and soot on it (MI_AM_<mesh>, Scripts/BuildKulisse.py); the pack assets stay as they are
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/FixDorf.py" -EnablePlugins=PythonScriptPlugin -unattended

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import BuildKulisse as bk  # noqa: E402

LEVEL = "/Game/Vael/Maps/L_Vael"
TAG = "VaelDorfFix"
WALL_PREFIXES = ("SM_MossyStoneWall", "SM_StoneWall", "SM_CastleWall", "SM_Medieval", "SM_DamagedCastleStairs")
CORNER = "/Game/Megascans/3D_Assets/MedievalModularCornerWall1x2M/SM_MedievalModularCornerWall1x2M"


def log(message):
    unreal.log_warning("[FixDorf] " + message)


def no_collision(actor):
    actor.modify()
    actor.set_actor_enable_collision(False)
    actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)


def main():
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL):
        log("could not load " + LEVEL)
        return
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level_actors = actors.get_all_level_actors()
    for actor in level_actors:
        if unreal.Name(TAG) in actor.get_editor_property("tags"):
            actors.destroy_actor(actor)
    level_actors = actors.get_all_level_actors()

    master = unreal.load_asset(bk.FOLDER + "/M_AM_Kulisse")
    corner_mesh = unreal.load_asset(CORNER)
    corner_mi, _ = bk.pack_instance(master, CORNER, bk.RUIN)
    instances = {}
    freed = pillars = painted = 0
    for actor in level_actors:
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        folder = str(actor.get_folder_path())
        label = actor.get_actor_label()
        mesh = actor.static_mesh_component.static_mesh
        hidden = actor.get_editor_property("hidden") or not actor.static_mesh_component.get_editor_property("visible")
        blockout = hidden and (
            (folder.startswith("Kesselgrund (aus L_Aschenmark)/Dorf/Haus") and label == "Mauer")
            or (folder == "Kesselgrund (aus L_Aschenmark)/Dorf/Kapelle" and label == "Mauer")
            or (folder == "Kesselgrund (aus L_Aschenmark)/Krater" and label == "Fels")
            or (folder == "Kesselgrund (aus L_Aschenmark)/Lager" and label.startswith("Kiste")))
        if blockout:
            no_collision(actor)
            freed += 1
            if folder.endswith("/Kapelle") and corner_mesh is not None:
                # A broken corner of masonry where the hidden pillar stood, as high as it was
                origin, extent = actor.get_actor_bounds(False)
                box = corner_mesh.get_bounding_box()
                sx = extent.x * 2 / max(box.max.x - box.min.x, 1.0)
                sy = extent.y * 2 / max(box.max.y - box.min.y, 1.0)
                sz = extent.z * 2 / max(box.max.z - box.min.z, 1.0)
                loc = unreal.Vector(origin.x - (box.min.x + box.max.x) * 0.5 * sx, origin.y - (box.min.y + box.max.y) * 0.5 * sy,
                                    origin.z - extent.z - box.min.z * sz - 5.0)
                pillar = actors.spawn_actor_from_class(unreal.StaticMeshActor, loc, actor.get_actor_rotation())
                pillar.static_mesh_component.set_static_mesh(corner_mesh)
                pillar.set_actor_scale3d(unreal.Vector(sx, sy, sz))
                if corner_mi is not None:
                    pillar.static_mesh_component.set_material(0, corner_mi)
                pillar.set_folder_path(folder)
                pillar.set_actor_label("Kapellenpfeiler")
                pillar.set_editor_property("tags", [unreal.Name(TAG)])
                pillars += 1
            continue
        if hidden or mesh is None or not mesh.get_name().startswith(WALL_PREFIXES):
            continue
        path = mesh.get_path_name().split(".")[0]
        if path not in instances:
            instances[path] = bk.pack_instance(master, path, bk.RUIN)[0]
        if instances[path] is not None:
            actor.modify()
            for slot in range(actor.static_mesh_component.get_num_materials()):
                actor.static_mesh_component.set_material(slot, instances[path])
            painted += 1
    # The square floor plates of the ported places: hard pale edges on the ground. They go; a soft trodden floor takes their place
    floor_material = unreal.load_asset("/Game/Vael/Welt/Boden/M_AM_Lehmboden")
    floors = 0
    for actor in level_actors:
        if not isinstance(actor, unreal.StaticMeshActor) or actor.get_actor_label() != "Boden":
            continue
        mesh = actor.static_mesh_component.static_mesh
        if mesh is None or mesh.get_name() != "Cube" or actor.get_editor_property("hidden"):
            continue
        origin, extent = actor.get_actor_bounds(False)
        actor.modify()
        actor.set_actor_hidden_in_game(True)
        actor.static_mesh_component.set_editor_property("visible", False)
        if floor_material is not None:
            decal = actors.spawn_actor_from_class(unreal.DecalActor, unreal.Vector(origin.x, origin.y, origin.z), unreal.Rotator(0.0, -90.0, actor.get_actor_rotation().yaw))
            decal.decal.set_decal_material(floor_material)
            decal.decal.set_editor_property("decal_size", unreal.Vector(200.0, extent.y * 1.15, extent.x * 1.15))
            decal.set_folder_path(str(actor.get_folder_path()))
            decal.set_actor_label("Lehmboden")
            decal.set_editor_property("tags", [unreal.Name(TAG)])
        floors += 1
    log("{} floor plates turned into soft floors".format(floors))

    # The wind that lifts the ash, once for the whole world
    drift = actors.spawn_actor_from_class(unreal.VaelAshDrift, unreal.Vector(100000, 120000, 6000), unreal.Rotator(0, 0, 0))
    drift.set_folder_path("Spiel")
    drift.set_actor_label("Aschenwind")
    drift.set_editor_property("tags", [unreal.Name(TAG)])

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
    log("{} invisible walls freed, {} chapel pillars built, {} walls in their own stone look ({} wall kinds)".format(freed, pillars, painted, len(instances)))


main()
