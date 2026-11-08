# Fills the Aschenmark in L_Vael with the plan of Tools/Welt/aschenmark_fuellung.py (Scripts/Data/Welt/AM_Fuellung.json):
# - the scenery as one actor per 128 m cell with a hierarchical instanced mesh per piece (C++ VaelWorldBuilder.spawn_instance_cell)
# - finds with a chest or something to harvest
# - smoke columns over slag and glowing fields, thin smoke out of the glowing cracks, fog volumes in hollows
# Idempotent: everything it placed carries the tag VaelFuellung and goes first on the next run.
#
# The plan gives the ground point under each piece; here the pivot of the mesh is evened out from its bounds, so the
# bottom of every piece sits on that point (pack meshes have their pivot in the middle, own pieces at the bottom).
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/FillAschenmark.py" -EnablePlugins=PythonScriptPlugin -unattended

import json
import math
import os

import unreal

DATA = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", "Welt")
LEVEL = "/Game/Vael/Maps/L_Vael"
TAG = "VaelFuellung"
M = 100.0
CELL = 128.0
SMALL_CULL = {"Kiesel": 9000.0, "Ast": 10000.0, "Kraut": 8000.0}
SMOKE = {"Rauchsaeule": "/Game/NiagaraExamples/FX_Smoke/NS_Chimney_Smoke", "Glutrauch": "/Game/Fire_Solution_FX/Smokes_Light/NS_Smoke_Whispy"}

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(message):
    unreal.log_warning("[FillAschenmark] " + message)


def tag(actor, folder, label):
    actor.set_folder_path(folder)
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(TAG)])


def main():
    plan = json.load(open(os.path.join(DATA, "AM_Fuellung.json"), encoding="utf-8"))
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL):
        log("could not load " + LEVEL)
        return
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    old = [a for a in actors.get_all_level_actors() if unreal.Name(TAG) in a.get_editor_property("tags")]
    for actor in old:
        actors.destroy_actor(actor)
    log("{} actors of the last run removed".format(len(old)))

    # Meshes, their materials and where their bottom middle lies in their own frame
    meshes = {}
    for name, info in plan["meshes"].items():
        mesh = unreal.load_asset(info["mesh"])
        if mesh is None:
            log("missing mesh " + info["mesh"])
            continue
        box = mesh.get_bounding_box()
        foot = unreal.Vector((box.min.x + box.max.x) * 0.5, (box.min.y + box.max.y) * 0.5, box.min.z)
        materials = [unreal.load_asset(m) for m in info["materials"]]
        meshes[name] = (mesh, materials, foot, info["kollision"], info["art"])

    cells = {}
    for name, x, y, z, yaw, nx, ny, nz, scale in plan["teile"]:
        if name not in meshes:
            continue
        mesh, materials, foot, collision, art = meshes[name]
        forward = unreal.Vector(math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0.0)
        if nz < 0.9999:
            rotation = unreal.MathLibrary.make_rot_from_zx(unreal.Vector(nx, ny, nz), forward)
        else:
            rotation = unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw)
        offset = unreal.MathLibrary.greater_greater_vector_rotator(unreal.Vector(foot.x * scale, foot.y * scale, foot.z * scale), rotation)
        location = unreal.Vector(x * M, y * M, z * M) - offset
        transform = unreal.Transform(location, rotation, unreal.Vector(scale, scale, scale))
        key = (int(x // CELL), int(y // CELL))
        cells.setdefault(key, {}).setdefault(name, []).append(transform)

    count = 0
    for (cx, cy), by_mesh in cells.items():
        batches = []
        for name, transforms in by_mesh.items():
            mesh, materials, _, collision, art = meshes[name]
            batch = unreal.VaelInstanceBatch()
            batch.set_editor_property("mesh", mesh)
            batch.set_editor_property("materials", materials)
            batch.set_editor_property("transforms", transforms)
            batch.set_editor_property("collision", collision)
            batch.set_editor_property("cull_distance", SMALL_CULL.get(art, 0.0))
            batches.append(batch)
            count += len(transforms)
        origin = unreal.Vector((cx + 0.5) * CELL * M, (cy + 0.5) * CELL * M, 0.0)
        unreal.VaelWorldBuilder.spawn_instance_cell(world, origin, batches, unreal.Name(TAG), "Fuellung/Zellen", "Zelle {}_{}".format(cx, cy))
    log("{} pieces in {} cells".format(count, len(cells)))

    # Finds: a chest beside some, something to harvest beside others
    chests = harvest = 0
    for find in plan["fundorte"]:
        a = math.radians(find["yaw"])
        spot = unreal.Vector((find["x"] + math.cos(a) * 2.5) * M, (find["y"] + math.sin(a) * 2.5) * M, find["z"] * M)
        if find["truhe"]:
            chest = actors.spawn_actor_from_class(unreal.VaelChest, spot, unreal.Rotator(0, 0, find["yaw"]))
            chest.set_editor_property("rolled_items", 1)
            tag(chest, "Fuellung/Fundorte/Truhen", "Truhe " + find["art"])
            chests += 1
        if find["ernte"]:
            data = unreal.load_asset("/Game/Vael/Nature/DA_Harvest_" + find["ernte"])
            if data is not None:
                plant = actors.spawn_actor_from_class(unreal.VaelHarvestable, spot + unreal.Vector(-150, 120, 0), unreal.Rotator(0, 0, find["yaw"]))
                plant.set_editor_property("data", data)
                tag(plant, "Fuellung/Fundorte/Ernte", find["ernte"])
                harvest += 1
    log("{} finds: {} chests, {} harvestables".format(len(plan["fundorte"]), chests, harvest))

    # Smoke
    systems = {k: unreal.load_asset(v) for k, v in SMOKE.items()}
    for smoke in plan["rauch"]:
        system = systems.get(smoke["art"])
        if system is None:
            continue
        actor = actors.spawn_actor_from_class(unreal.NiagaraActor, unreal.Vector(smoke["x"] * M, smoke["y"] * M, smoke["z"] * M), unreal.Rotator(0, 0, 0))
        actor.niagara_component.set_asset(system)
        if smoke["art"] == "Rauchsaeule":
            actor.set_actor_scale3d(unreal.Vector(2.5, 2.5, 2.5))
        tag(actor, "Fuellung/Rauch", smoke["art"])
    log("{} smoke sources".format(len(plan["rauch"])))

    # Fog in hollows: local fog volumes, flat and wide
    for fog in plan["nebel"]:
        actor = actors.spawn_actor_from_class(unreal.LocalFogVolume, unreal.Vector(fog["x"] * M, fog["y"] * M, fog["z"] * M + 150.0), unreal.Rotator(0, 0, 0))
        size = fog["groesse"] * M / 500.0
        actor.set_actor_scale3d(unreal.Vector(size, size, size * 0.25))
        fog_component = actor.get_component_by_class(unreal.LocalFogVolumeComponent)
        if fog_component is not None:
            fog_component.set_editor_property("radial_fog_extinction", 0.12)
            fog_component.set_editor_property("height_fog_extinction", 0.12)
        tag(actor, "Fuellung/Nebel", "Nebel")
    log("{} fog volumes".format(len(plan["nebel"])))

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
    log("saved")


main()
