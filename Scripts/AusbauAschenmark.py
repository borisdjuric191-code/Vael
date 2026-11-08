# Fills the open Aschenmark of L_Vael between its places (goal of 8 November 2026) and gives the blockouts real models.
#
# - Imports the buildings of Tools/Welt/aschenmark_bauten.py (tents, preacher tent, banner, slag gate, pass gate) to
#   /Game/Vael/Maps/Aschenmark/Bauten with the materials of the Glutkammer kit.
# - Scatter in clusters (Diablo style: props grouped, open ground between): dead trees, fallen wood, rocks, dry weeds, by zone;
#   roads, the 40 m clearings, camps and places stay free.
# - Harvestable plants, fungi and stones of the Aschenmark (DA_Harvest_*) by zone.
# - Blockouts hidden (not deleted) and replaced: tents of the Passwache and the Predigerlager, the buried Schlackentor,
#   obsidian cones and harpy needles as pack rocks; the white engine shape near the pond is hidden.
# - The north end of the Aschenpass: a closed palisade gate with moss creeping over from the Wurzelforst (the border wall stays).
#
# Everything it adds carries the tag "VaelAusbau" in the folder "Ausbau/..."; a rerun removes those first. Nothing else is deleted.
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/AusbauAschenmark.py" -AllowCommandletRendering -unattended -nosplash

import array
import json
import math
import os
import random

import unreal

LEVEL = "/Game/Vael/Maps/L_Vael"
DATA = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", "Welt")
SOURCE = "C:/Projekte/Vael_Quellen/Aschenmark/export"
BUILD_FOLDER = "/Game/Vael/Maps/Aschenmark/Bauten"
KIT = "/Game/Vael/Refugium/Glutkammer/Probe"
SLOTS = {"Holz": "MI_GK_Holz", "Leder": "M_GK_Leder", "Metall": "M_GK_Metall", "Faden": "M_GK_Faden", "Glut": "M_GK_Glut", "Moos": "M_GK_Moos"}
BUILDINGS = ["SM_AM_Zelt", "SM_AM_Predigerzelt", "SM_AM_Banner", "SM_AM_Schlackentor", "SM_AM_Passtor"]
PACK_FOLDERS = ["/Game/Megascans", "/Game/Meshes", "/Game/Rock_Collection_04", "/Game/MS"]
TAG = "VaelAusbau"
SIZE_X, SIZE_Y = 2017, 2521
M = 100.0

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assets = unreal.EditorAssetLibrary
rng = random.Random(1108)
counts = {}


def log(message):
    unreal.log_warning("[AusbauAschenmark] " + message)


# ---------------------------------------------------------------- ground and places

heights = array.array("H")
with open(os.path.join(DATA, "AM_Hoehen.r16"), "rb") as f:
    heights.frombytes(f.read())
with open(os.path.join(DATA, "AM_Orte.json"), encoding="utf-8") as f:
    ORTE = json.load(f)


def ground(x, y):
    x = min(max(x, 0.0), SIZE_X - 1.001)
    y = min(max(y, 0.0), SIZE_Y - 1.001)
    ix, iy = int(x), int(y)
    tx, ty = x - ix, y - iy

    def h(i, j):
        return (heights[j * SIZE_X + i] - 32768) / 128.0
    return (h(ix, iy) * (1 - tx) + h(ix + 1, iy) * tx) * (1 - ty) + (h(ix, iy + 1) * (1 - tx) + h(ix + 1, iy + 1) * tx) * ty


def slope(x, y):
    return max(abs(ground(x + 2, y) - ground(x - 2, y)), abs(ground(x, y + 2) - ground(x, y - 2))) / 4.0


ROADS = [ORTE["main_road"]] + list(ORTE["side_roads"].values())


def road_distance(x, y):
    best = 1e9
    for line in ROADS:
        for a, b in zip(line, line[1:]):
            ax, ay, bx, by = a["x"], a["y"], b["x"], b["y"]
            dx, dy = bx - ax, by - ay
            t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / max(dx * dx + dy * dy, 1e-6)))
            best = min(best, math.hypot(x - ax - t * dx, y - ay - t * dy))
    return best


# Kept free: clearings 28 m, camps 40 m, places 45 m (the ported places bring their own dressing)
RESERVED = [(c["x"], c["y"], 28.0) for c in ORTE["clearings"]]
RESERVED += [(p["x"], p["y"], 40.0) for p in ORTE["camps"].values()]
RESERVED += [(p["x"], p["y"], 45.0) for p in ORTE["places"].values()]
RESERVED += [(ORTE["places"]["Krater der Glutkönigin"]["x"], ORTE["places"]["Krater der Glutkönigin"]["y"], 95.0)]
ZONES = {name: (p["x"], p["y"]) for name, p in ORTE["zones"].items()}


def zone_of(x, y):
    return min(ZONES, key=lambda n: math.hypot(x - ZONES[n][0], y - ZONES[n][1]))


def free(x, y, road=7.0):
    if not (25 < x < SIZE_X - 60 and 25 < y < SIZE_Y - 60):
        return False
    if ground(x, y) < 2.0 or slope(x, y) > 0.9:
        return False
    if road_distance(x, y) < road:
        return False
    return not any(math.hypot(x - cx, y - cy) < r for cx, cy, r in RESERVED)


# ---------------------------------------------------------------- meshes

registry = unreal.AssetRegistryHelpers.get_asset_registry()
MESHES = {}
for folder in PACK_FOLDERS:
    for data in registry.get_assets_by_path(folder, recursive=True):
        if str(data.asset_class_path.asset_name) == "StaticMesh":
            MESHES.setdefault(str(data.asset_name), str(data.package_name))
loaded = {}


def mesh(name):
    if name not in loaded:
        path = MESHES.get(name)
        loaded[name] = unreal.load_asset(path) if path else None
        if loaded[name] is None:
            log("mesh missing: " + name)
    return loaded[name]


def place(asset, x, y, folder, label, scale=1.0, yaw=None, sink=0.0, roll=0.0, collision=False, size=None):
    """A mesh standing on the ground at world metres; size (m) scales it to that height instead of scale"""
    if isinstance(asset, str):
        asset = mesh(asset)
    if asset is None:
        return None
    box = asset.get_bounding_box()
    if size is not None:
        scale = size * M / max(box.max.z - box.min.z, 1.0)
    z = ground(x, y) * M - box.min.z * scale - sink * M
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x * M, y * M, z), unreal.Rotator(roll, 0.0, rng.uniform(0, 360) if yaw is None else yaw))
    actor.static_mesh_component.set_static_mesh(asset)
    if not collision:
        actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    actor.set_folder_path("Ausbau/" + folder)
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(TAG)])
    counts[folder] = counts.get(folder, 0) + 1
    return actor


def import_buildings():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    materials = {slot: unreal.load_asset("{}/{}".format(KIT, name)) for slot, name in SLOTS.items()}
    for name in BUILDINGS:
        fbx = os.path.join(SOURCE, name + ".fbx")
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_as_skeletal", False)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
        ui.static_mesh_import_data.set_editor_property("auto_generate_collision", True)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", fbx)
        task.set_editor_property("destination_path", BUILD_FOLDER)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", ui)
        tools.import_asset_tasks([task])
        built = unreal.load_asset("{}/{}".format(BUILD_FOLDER, name))
        for index, slot in enumerate(built.get_editor_property("static_materials")):
            slot_name = str(slot.get_editor_property("material_slot_name")).split(".")[0]
            if materials.get(slot_name) is not None:
                built.set_material(index, materials[slot_name])
        assets.save_loaded_asset(built, False)
        box = built.get_bounding_box()
        loaded[name] = built
        log("{}: {:.0f} x {:.0f} x {:.0f} cm".format(name, box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z))


# ---------------------------------------------------------------- scatter

TREES = ["SM_DeadBroomBush_V1", "SM_DeadBroomBush_V2", "SM_DeadBroomBush_V3", "SM_DeadBroomBush_V4"]
WOOD = [("SM_FallenFirTree", 0.8), ("SM_FallenPineTree", 1.1), ("SM_FallenPineAssembly", 1.2), ("SM_BrokenPineAssembly", 1.0), ("SM_OldStump", 1.0),
        ("SM_BrokenTreeStump", 1.3), ("SM_WoodBranchB", 1.3), ("SM_WoodBranchC", 1.4), ("SM_ForestRootsA", 1.0), ("SM_WoodStump", 1.4)]
WEEDS = ["SM_DryPlant00_V1", "SM_DryPlant00_V2", "SM_DryPlant00_V3", "SM_DryPlant00_V4", "SM_DryPlantSet01_V01", "SM_DryPlantSet01_V06", "SM_DryPlantSet01_V09",
         "SM_DryPlantSet_V1", "SM_DryPlantSet_V2", "SM_DryPlantSet_V3", "SM_DeadBroomBush_V1", "SM_DeadBroomBush_V3", "SM_ThatchingGrass00_V2", "SM_ThatchingGrass00_V6"]
ROCKS = sorted(n for n in MESHES if n.startswith("SM_Rock_0"))
MOSS = sorted(n for n in MESHES if n.startswith("SM_IcelandicMossClusters"))
# Per zone: clusters, trees, wood, rocks, weeds per cluster (ranges)
ZONE_MIX = {
    "Kesselgrund": dict(trees=(1, 3), wood=(1, 3), rocks=(0, 2), weeds=(3, 6)),
    "Schlackenhänge": dict(trees=(0, 1), wood=(0, 1), rocks=(3, 6), weeds=(1, 3)),
    "Glutfelder": dict(trees=(1, 2), wood=(0, 2), rocks=(2, 4), weeds=(1, 3)),
    "Aschenpass": dict(trees=(0, 2), wood=(0, 1), rocks=(3, 5), weeds=(2, 4)),
}


def scatter():
    centres = []
    tries = 0
    while len(centres) < 820 and tries < 40000:
        tries += 1
        x, y = rng.uniform(40, SIZE_X - 80), rng.uniform(40, SIZE_Y - 80)
        if not free(x, y, 12.0) or any(math.hypot(x - cx, y - cy) < 30.0 for cx, cy in centres):
            continue
        centres.append((x, y))
    for cx, cy in centres:
        mix = ZONE_MIX[zone_of(cx, cy)]
        zone = zone_of(cx, cy)

        def spot(spread):
            for _ in range(12):
                a, d = rng.uniform(0, 2 * math.pi), rng.uniform(0, spread)
                x, y = cx + math.cos(a) * d, cy + math.sin(a) * d
                if free(x, y):
                    return x, y
            return None

        for _ in range(rng.randint(*mix["trees"])):
            s = spot(12.0)
            if s:
                place(rng.choice(TREES), s[0], s[1], "Streuung/{}/Tote Baeume".format(zone), "Toter Baum", scale=rng.uniform(3.8, 6.0), sink=0.1, roll=rng.uniform(-6, 6))
        for _ in range(rng.randint(*mix["wood"])):
            s = spot(14.0)
            if s:
                name, scale = rng.choice(WOOD)
                place(name, s[0], s[1], "Streuung/{}/Totholz".format(zone), "Totholz", scale=scale * rng.uniform(0.85, 1.2), sink=0.05)
        for _ in range(rng.randint(*mix["rocks"])):
            s = spot(13.0)
            if s and ROCKS:
                place(rng.choice(ROCKS), s[0], s[1], "Streuung/{}/Felsen".format(zone), "Fels", scale=rng.uniform(0.9, 2.4), sink=0.3)
        for _ in range(rng.randint(*mix["weeds"])):
            s = spot(16.0)
            if s:
                for k in range(rng.randint(2, 4)):
                    place(rng.choice(WEEDS), s[0] + rng.uniform(-0.8, 0.8), s[1] + rng.uniform(-0.8, 0.8), "Streuung/{}/Gestruepp".format(zone), "Gestruepp",
                          scale=rng.uniform(0.9, 1.5), sink=0.02)
    log("{} clusters".format(len(centres)))


HARVEST = {
    "Kesselgrund": ["Laternenglocke", "Traenenkelch", "Russmoos", "Aschenstaeubling", "Pfeifenmorchel", "Schlacke"],
    "Schlackenhänge": ["Schlacke", "Glutstein", "Russquarz", "Markdruse", "Aschblase"],
    "Glutfelder": ["Glutdistel", "Glutstein", "Fleischkelch", "Aschblase", "Adernflechte"],
    "Aschenpass": ["Russquarz", "Pfeifenmorchel", "Adernflechte", "Laternenglocke"],
}


def harvestables():
    placed = 0
    for zone, kinds in HARVEST.items():
        zx, zy = ZONES[zone]
        for kind in kinds:
            data = unreal.load_asset("/Game/Vael/Nature/DA_Harvest_" + kind)
            if data is None:
                log("missing DA_Harvest_" + kind)
                continue
            done, tries = 0, 0
            while done < 5 and tries < 400:
                tries += 1
                x, y = zx + rng.uniform(-300, 300), zy + rng.uniform(-300, 300)
                if zone_of(x, y) != zone or not free(x, y, 5.0):
                    continue
                actor = actors.spawn_actor_from_class(unreal.VaelHarvestable, unreal.Vector(x * M, y * M, ground(x, y) * M), unreal.Rotator(0, 0, rng.uniform(0, 360)))
                actor.set_editor_property("data", data)
                actor.set_folder_path("Ausbau/Natur/{}/{}".format(zone, kind))
                actor.set_actor_label(kind)
                actor.set_editor_property("tags", [unreal.Name(TAG), unreal.Name("VaelNatur")])
                done += 1
            placed += done
    log("{} harvestables".format(placed))


# ---------------------------------------------------------------- blockouts and new buildings

def hide(actor):
    actor.set_actor_hidden_in_game(True)
    actor.set_actor_enable_collision(False)
    if isinstance(actor, unreal.StaticMeshActor):
        actor.static_mesh_component.set_editor_property("visible", False)


def replace_blockouts(level_actors):
    hidden = 0
    teich = ORTE["places"]["Teich"]
    for actor in level_actors:
        if not isinstance(actor, unreal.StaticMeshActor) or unreal.Name(TAG) in actor.get_editor_property("tags"):
            continue
        folder = str(actor.get_folder_path())
        label = actor.get_actor_label()
        if (folder == "Lager/Passwache" and label.startswith("Zelt")) or folder.startswith("Orte/Aschenpass/Predigerlager") \
                or label.startswith("Schlackentor") or folder.startswith("Orte/Schlackenhaenge/Obsidianbruch") or folder.startswith("Orte/Glutfelder/Harpyienfelsen"):
            hide(actor)
            hidden += 1
            continue
        # The white placeholder near the pond: an engine shape without any material of its own
        l = actor.get_actor_location()
        sm = actor.static_mesh_component.static_mesh
        if sm is not None and sm.get_path_name().startswith("/Engine/BasicShapes") and math.hypot(l.x / M - teich["x"], l.y / M - teich["y"]) < 90 \
                and actor.static_mesh_component.get_material(0) is not None and "BasicShapeMaterial" in actor.static_mesh_component.get_material(0).get_name() \
                and actor.static_mesh_component.is_visible() and not actor.is_hidden_ed():
            log("white shape at the pond hidden: {} ({})".format(label, folder))
            hide(actor)
            hidden += 1
    log("{} blockouts hidden".format(hidden))

    # Passwache: four guard tents round the fire, a banner at the road
    camp = ORTE["camps"]["Passwache"]
    for k in range(4):
        a = math.radians(k * 90 + 35)
        x, y = camp["x"] + math.cos(a) * 9.0, camp["y"] + math.sin(a) * 9.0
        place("SM_AM_Zelt", x, y, "Passwache", "Zelt", size=2.3, yaw=math.degrees(a) + 90, sink=0.05, collision=True)
    place("SM_AM_Banner", camp["x"] + 13, camp["y"] - 4, "Passwache", "Banner", size=4.4, yaw=20, sink=0.2)
    for k in range(6):
        a = rng.uniform(0, 2 * math.pi)
        place(rng.choice(["SM_WoodenBarrelA", "SM_WoodenBox", "SM_OldWoodenChest", "SM_WoodenAmmoCrate"]), camp["x"] + math.cos(a) * 5.5, camp["y"] + math.sin(a) * 5.5, "Passwache", "Vorrat")

    # Predigerlager: three round tents of the Orden, a banner of the scar
    q = ORTE["places"]["Predigerlager"]
    for k in range(3):
        a = math.radians(k * 120 + 10)
        place("SM_AM_Predigerzelt", q["x"] + math.cos(a) * 9.0, q["y"] + math.sin(a) * 9.0, "Predigerlager", "Predigerzelt", size=4.6, sink=0.05, collision=True)
    place("SM_AM_Banner", q["x"], q["y"], "Predigerlager", "Banner der Narbe", size=4.4, sink=0.2)

    # The buried Schlackentor: the arch facing the slag road, slag rocks heaped round
    g = ORTE["places"]["Schlackentor"]
    place("SM_AM_Schlackentor", g["x"], g["y"], "Schlackentor", "Schlackentor (verschuettet)", size=9.0, yaw=0.0, sink=1.2, collision=True)
    for k in range(10):
        a = rng.uniform(0, 2 * math.pi)
        place(rng.choice(ROCKS), g["x"] + math.cos(a) * rng.uniform(6, 12), g["y"] + math.sin(a) * rng.uniform(6, 12), "Schlackentor", "Schlacke", scale=rng.uniform(1.5, 3.0), sink=0.6)

    # Obsidianbruch and Harpyienfelsen: pack rocks instead of cones and cylinders
    for name, count, tall in (("Obsidianbruch", 9, (1.0, 1.6)), ("Harpyienfelsen", 7, (2.5, 4.0))):
        p = ORTE["places"][name]
        for k in range(count):
            x, y = p["x"] + rng.uniform(-24, 24), p["y"] + rng.uniform(-24, 24)
            s = rng.uniform(2.0, 3.4)
            actor = place(rng.choice(ROCKS), x, y, name, "Fels", scale=s, sink=0.5, collision=True)
            if actor is not None:
                actor.set_actor_scale3d(unreal.Vector(s, s, s * rng.uniform(*tall)))


def pass_gate():
    """The north end of the Aschenpass, where the road leaves for the Forstsaum: a closed palisade, moss growing in from the north"""
    road = ORTE["main_road"]
    end = max(road, key=lambda p: p["x"])
    gate = min((p for p in road if p["x"] < end["x"] - 30), key=lambda p: abs(p["x"] - (end["x"] - 40)))
    nxt = road[road.index(gate) + 1] if road.index(gate) + 1 < len(road) else end
    yaw = math.degrees(math.atan2(nxt["y"] - gate["y"], nxt["x"] - gate["x"]))
    place("SM_AM_Passtor", gate["x"], gate["y"], "Passtor", "Tor zum Forstsaum (verschlossen)", size=5.6, yaw=yaw, sink=0.5, collision=True)
    # Moss and green between the last stones: the first sign of the forest
    for k in range(60):
        t = rng.uniform(-10, 70)
        x = gate["x"] + math.cos(math.radians(yaw)) * t + rng.uniform(-14, 14)
        y = gate["y"] + math.sin(math.radians(yaw)) * t + rng.uniform(-14, 14)
        if ground(x, y) > 2 and MOSS:
            place(rng.choice(MOSS), x, y, "Passtor/Moos", "Moos", scale=rng.uniform(1.0, 2.2), sink=0.05)
    log("pass gate at {:.0f} {:.0f}, yaw {:.0f}".format(gate["x"], gate["y"], yaw))


# ---------------------------------------------------------------- main

def main():
    import_buildings()
    if not levels.load_level(LEVEL):
        log("could not open " + LEVEL)
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    # The whole open world in memory, so the blockouts far away are found too
    try:
        descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
        unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
        log("{} actor descs loaded".format(len(descs)))
    except Exception as error:  # noqa: BLE001
        log("could not load all actors: {}".format(error))

    level_actors = actors.get_all_level_actors()
    old = [a for a in level_actors if unreal.Name(TAG) in a.get_editor_property("tags")]
    for actor in old:
        actors.destroy_actor(actor)
    log("{} actors of the last run removed, {} in the level".format(len(old), len(level_actors) - len(old)))
    level_actors = [a for a in actors.get_all_level_actors()]

    replace_blockouts(level_actors)
    pass_gate()
    harvestables()
    scatter()
    log("placed: " + ", ".join("{} {}".format(k, v) for k, v in sorted(counts.items())))

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("done")


main()
