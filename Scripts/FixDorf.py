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
import random
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import BuildKulisse as bk  # noqa: E402

LEVEL = "/Game/Vael/Maps/L_Vael"
TAG = "VaelDorfFix"
WALL_PREFIXES = ("SM_MossyStoneWall", "SM_StoneWall", "SM_CastleWall", "SM_Medieval", "SM_DamagedCastleStairs")
# The chapel has its own low rubble walls (folder Kulisse) with a door in the south and a gap in the west; pillars where
# the hidden cubes stood would close them again, so they stay off
BUILD_PILLARS = False
STONE = "/Game/Megascans/3D_Assets/MossyStoneWallC/SM_MossyStoneWallC_02"


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
    # The pack rubble wall carries a collision hull that sticks out about 1.3 m past the stones: that was the invisible wall
    # in the doorways. Our own copy collides exactly as it looks; the pack asset stays as it is.
    own_wall_path = bk.FOLDER + "/SM_AM_Bruchsteinmauer"
    if not unreal.EditorAssetLibrary.does_asset_exist(own_wall_path):
        unreal.EditorAssetLibrary.duplicate_asset(STONE, own_wall_path)
    own_wall = unreal.load_asset(own_wall_path)
    own_wall.get_editor_property("body_setup").set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    unreal.EditorAssetLibrary.save_loaded_asset(own_wall, False)
    stone_mesh = unreal.load_asset(STONE)
    stone_mi, _ = bk.pack_instance(master, STONE, bk.RUIN)
    rng = random.Random(811)
    instances = {}
    freed = pillars = painted = props = swapped = 0
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
            or (folder == "Kesselgrund (aus L_Aschenmark)/Lager" and label.startswith("Kiste"))
            or (folder == "Kesselgrund (aus L_Aschenmark)/Dorf/Gildenspeicher/Kulisse" and label != "Boden"))
        if blockout:
            no_collision(actor)
            freed += 1
            # A ruin: about every fourth pillar of the chapel has fallen, so it can be walked into from several sides
            if BUILD_PILLARS and folder.endswith("/Kapelle") and stone_mesh is not None and rng.random() >= 0.28:
                # A pillar of the same rubble stone as the house walls where the hidden pillar stood: blocks stacked
                # crosswise, the top broken off at a random height
                origin, extent = actor.get_actor_bounds(False)
                box = stone_mesh.get_bounding_box()
                bx, by, bz = box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z
                height = min(extent.z * 2 * rng.uniform(0.3, 0.75), 250.0)
                course, z = 0, origin.z - extent.z - 10.0
                while z < origin.z - extent.z + height:
                    yaw = actor.get_actor_rotation().yaw + (90.0 if course % 2 else 0.0) + rng.uniform(-6, 6)
                    scale = unreal.Vector(extent.x * 2.1 / bx, extent.x * 2.1 / by, 1.0)
                    block = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(origin.x, origin.y, z - box.min.z), unreal.Rotator(0.0, 0.0, yaw))
                    block.static_mesh_component.set_static_mesh(stone_mesh)
                    block.set_actor_scale3d(scale)
                    if stone_mi is not None:
                        block.static_mesh_component.set_material(0, stone_mi)
                    block.set_folder_path(folder)
                    block.set_actor_label("Kapellenpfeiler")
                    block.set_editor_property("tags", [unreal.Name(TAG)])
                    z += bz * 0.85
                    course += 1
                pillars += 1
            continue
        if hidden or mesh is None:
            continue
        if not mesh.get_name().startswith(WALL_PREFIXES):
            # Small things of the ported places (barrels, crates, wheelbarrow, beams, rubble): the same look as the scenery,
            # their foot running out into the ash; skipped: engine shapes, our own buildings and things without pack textures
            path = mesh.get_path_name().split(".")[0]
            if not folder.startswith("Kesselgrund (aus L_Aschenmark)") or not path.startswith(("/Game/Megascans", "/Game/Meshes")):
                continue
            if path not in instances:
                instances[path] = bk.pack_instance(master, path, bk.JUNK)[0]
            if instances[path] is not None:
                actor.modify()
                for slot in range(actor.static_mesh_component.get_num_materials()):
                    actor.static_mesh_component.set_material(slot, instances[path])
                props += 1
            continue
        path = mesh.get_path_name().split(".")[0]
        if path not in instances:
            instances[path] = bk.pack_instance(master, path, bk.RUIN)[0]
        if path == STONE:
            actor.modify()
            actor.static_mesh_component.set_static_mesh(own_wall)
            swapped += 1
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
        if mesh is None or mesh.get_name() != "Cube" or not str(actor.get_folder_path()).startswith(("Kesselgrund (aus L_Aschenmark)/Dorf", "Kesselgrund (aus L_Aschenmark)/Kontor")):
            continue
        origin, extent = actor.get_actor_bounds(False)
        actor.modify()
        actor.set_actor_hidden_in_game(True)
        actor.static_mesh_component.set_editor_property("visible", False)
        # its raised edge stopped the players at the threshold; the ground below is the landscape
        actor.set_actor_enable_collision(False)
        actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
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
    log("{} rubble walls now collide as they look".format(swapped))
    log("{} invisible walls freed, {} chapel pillars built, {} walls in their own stone look, {} small things in the scenery look ({} meshes)".format(
        freed, pillars, painted, props, len(instances)))
    for actor in actors.get_all_level_actors():
        if actor.get_actor_label() == "Lehmboden":
            loc = actor.get_actor_location()
            log("floor {} at {:.1f} {:.1f}".format(actor.get_folder_path(), loc.x / 100.0, loc.y / 100.0))


main()
