# Dresses the blockout level L_Aschenmark with the meshes of the downloaded packs (first pass, to be reworked by hand).
#
# - Only adds: every new actor carries the tag "VaelKulisse" and sits in an Outliner folder ending in "Kulisse".
#   Deleting those folders in the editor undoes the dressing.
# - Placeholders that get a real mesh (rocks, dead trees, crates) are not deleted. They are made invisible and keep
#   their collision, so the game plays exactly as before. They carry the tag "VaelPlatzhalterVersteckt".
# - The ash ground gets the new material MI_Aschenmark_Boden; nothing else of the existing level is changed.
# - Runs once: if the level already holds actors tagged "VaelKulisse", nothing happens.
# - Writes a summary to Saved/DressLog.txt. Mesh sizes come from Scripts/MeasureMeshes.py (Saved/MeshSizes.txt).
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="Scripts/DressAschenmark.py" -EnablePlugins=PythonScriptPlugin

import math
import os
import random

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Aschenmark"
DRESSING_FOLDER = "/Game/Vael/Maps/Kulisse"
PACK_FOLDERS = ["/Game/Megascans", "/Game/Meshes", "/Game/Rock_Collection_04"]
TILE = 140.0
MAP_TILES = 64

DRESSING_TAG = "VaelKulisse"
HIDDEN_TAG = "VaelPlatzhalterVersteckt"

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(417)

meshes = {}
counts = {}
notes = []


# ---------------------------------------------------------------- Helpers

def world(x, y, z=0.0):
    """World location of a prototype tile position, as in BuildAschenmark.py"""
    return unreal.Vector((MAP_TILES / 2 - x) * TILE, (MAP_TILES / 2 - y) * TILE, z)


def world_yaw(tile_angle):
    """Yaw in the world of a direction given as an angle in tile space; both tile axes are mirrored"""
    return tile_angle + 180.0


def ring(center, angle, radius):
    """Tile position on a circle around a tile position, angle in degrees in tile space"""
    return (center[0] + math.cos(math.radians(angle)) * radius, center[1] + math.sin(math.radians(angle)) * radius)


def load_meshes():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for folder in PACK_FOLDERS:
        for data in registry.get_assets_by_path(folder, recursive=True):
            if str(data.asset_class_path.asset_name) == "StaticMesh":
                meshes[str(data.asset_name)] = str(data.package_name)


def mesh(name):
    path = meshes.get(name)
    if path is None:
        if name not in notes:
            notes.append(name)
        return None
    return unreal.load_asset(path)


def place(name, tile, folder, label, scale=1.0, yaw=0.0, pitch=0.0, roll=0.0, z=0.0, sink=0.0, stretch=None):
    """Puts a pack mesh on the ground at a tile position. Purely visual: it never blocks movement."""
    asset = mesh(name)
    if asset is None:
        return None

    box = asset.get_bounding_box()
    scale_vector = unreal.Vector(scale, scale, scale) if stretch is None else unreal.Vector(scale * stretch[0], scale * stretch[1], scale * stretch[2])
    location = world(tile[0], tile[1], z - box.min.z * scale_vector.z - sink)

    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, location, unreal.Rotator(roll, pitch, yaw))
    actor.static_mesh_component.set_static_mesh(asset)
    actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    actor.set_actor_scale3d(scale_vector)
    actor.set_folder_path(folder)
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(DRESSING_TAG)])

    counts[folder] = counts.get(folder, 0) + 1
    return actor


def blocker(tile, length, thickness, height, yaw, folder, label):
    """Invisible wall, so that dressing which should stop the player does so without relying on the pack's collision"""
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, world(tile[0], tile[1], height * 0.5), unreal.Rotator(0.0, 0.0, yaw))
    actor.static_mesh_component.set_static_mesh(CUBE)
    actor.static_mesh_component.set_editor_property("visible", False)
    actor.set_actor_hidden_in_game(True)
    actor.set_actor_scale3d(unreal.Vector(length / 100.0, thickness / 100.0, height / 100.0))
    actor.set_folder_path(folder)
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(DRESSING_TAG)])
    return actor


def hide_placeholder(actor):
    """The blockout shape stays as collision, only its look goes"""
    actor.static_mesh_component.set_editor_property("visible", False)
    actor.set_actor_hidden_in_game(True)
    actor.set_editor_property("tags", list(actor.get_editor_property("tags")) + [unreal.Name(HIDDEN_TAG)])


def tile_of(actor):
    location = actor.get_actor_location()
    return (MAP_TILES / 2 - location.x / TILE, MAP_TILES / 2 - location.y / TILE)


# ---------------------------------------------------------------- Ground material

def texture_sampler(texture):
    compression = texture.get_editor_property("compression_settings")
    if compression == unreal.TextureCompressionSettings.TC_NORMALMAP:
        return unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    if compression == unreal.TextureCompressionSettings.TC_MASKS:
        return unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    if compression == unreal.TextureCompressionSettings.TC_GRAYSCALE:
        return unreal.MaterialSamplerType.SAMPLERTYPE_GRAYSCALE if texture.get_editor_property("srgb") else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE
    if compression == unreal.TextureCompressionSettings.TC_ALPHA:
        return unreal.MaterialSamplerType.SAMPLERTYPE_ALPHA
    return unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if texture.get_editor_property("srgb") else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR


def make_ground_material():
    """A simple material that lays ground textures by world position, so it tiles evenly on shapes of any size"""
    instance_path = "{}/MI_Aschenmark_Boden".format(DRESSING_FOLDER)
    if unreal.EditorAssetLibrary.does_asset_exist(instance_path):
        return unreal.load_asset(instance_path)

    textures = {}
    for key, suffix in (("Albedo", "D"), ("Normal", "N"), ("Roughness", "R")):
        textures[key] = unreal.load_asset("/Game/Megascans/Surfaces/HeavyMud/T_HeavyMud_{}".format(suffix))
        if textures[key] is None:
            notes.append("ground texture T_HeavyMud_{}".format(suffix))
            return None

    library = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material_path = "{}/M_Vael_Boden".format(DRESSING_FOLDER)

    if unreal.EditorAssetLibrary.does_asset_exist(material_path):
        material = unreal.load_asset(material_path)
    else:
        material = tools.create_asset("M_Vael_Boden", DRESSING_FOLDER, unreal.Material, unreal.MaterialFactoryNew())

        position = library.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1300, 0)
        mask = library.create_material_expression(material, unreal.MaterialExpressionComponentMask, -1100, 0)
        mask.set_editor_property("r", True)
        mask.set_editor_property("g", True)
        mask.set_editor_property("b", False)
        mask.set_editor_property("a", False)
        library.connect_material_expressions(position, "", mask, "")

        tile_size = library.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1100, 150)
        tile_size.set_editor_property("parameter_name", "TileSize")
        tile_size.set_editor_property("default_value", 450.0)

        divide = library.create_material_expression(material, unreal.MaterialExpressionDivide, -900, 50)
        library.connect_material_expressions(mask, "", divide, "A")
        library.connect_material_expressions(tile_size, "", divide, "B")

        samples = {}
        for index, key in enumerate(("Albedo", "Normal", "Roughness")):
            sample = library.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -650, index * 280)
            sample.set_editor_property("parameter_name", key)
            sample.set_editor_property("texture", textures[key])
            sample.set_editor_property("sampler_type", texture_sampler(textures[key]))
            library.connect_material_expressions(divide, "", sample, "UVs")
            samples[key] = sample

        tint = library.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -650, -220)
        tint.set_editor_property("parameter_name", "Tint")
        tint.set_editor_property("default_value", unreal.LinearColor(0.62, 0.58, 0.56, 1.0))

        multiply = library.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, -80)
        library.connect_material_expressions(samples["Albedo"], "RGB", multiply, "A")
        library.connect_material_expressions(tint, "", multiply, "B")

        library.connect_material_property(multiply, "", unreal.MaterialProperty.MP_BASE_COLOR)
        library.connect_material_property(samples["Normal"], "RGB", unreal.MaterialProperty.MP_NORMAL)
        library.connect_material_property(samples["Roughness"], "R", unreal.MaterialProperty.MP_ROUGHNESS)

        library.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material, False)

    instance = tools.create_asset("MI_Aschenmark_Boden", DRESSING_FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(instance, material)
    unreal.EditorAssetLibrary.save_loaded_asset(instance, False)
    return instance


# ---------------------------------------------------------------- Places

CAMP = (10.5, 52.5)
CAMP_RADIUS = 6.2
ROADS = [((10, 52), (30, 41)), ((30, 40), (51, 41)), ((30, 40), (29, 20)), ((29, 20), (45, 17)), ((45, 17), (47, 16))]
HOUSES = [(22, 33, 27, 37), (33, 33, 37, 37), (16, 40, 20, 44), (34, 43, 40, 49)]


def road_distance(x, y):
    best = 1000.0
    for (ax, ay), (bx, by) in ROADS:
        dx, dy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / max(dx * dx + dy * dy, 1e-6)))
        best = min(best, math.hypot(x - (ax + dx * t), y - (ay + dy * t)))
    return best


def dress_camp():
    folder = "Lager/Kulisse"
    gate_angle = math.degrees(math.atan2(41 - 52, 30 - 10))
    gate_half = 20.0

    # Palisade: a ring of posts, open towards the road
    post = mesh("SM_WoodenPost")
    if post is not None:
        width = (post.get_bounding_box().max.x - post.get_bounding_box().min.x) * 2.2
        step = math.degrees(width * 0.9 / (CAMP_RADIUS * TILE))
        angle = gate_angle + gate_half
        index = 0
        while angle < gate_angle + 360.0 - gate_half:
            height = 2.1 * rng.uniform(0.9, 1.12)
            place("SM_WoodenPost", ring(CAMP, angle, CAMP_RADIUS + rng.uniform(-0.04, 0.04)), folder + "/Palisade", "Pfahl {}".format(index),
                  scale=height, stretch=(1.05, 1.05, 1.0), yaw=rng.uniform(0.0, 360.0), pitch=rng.uniform(-3.0, 3.0), roll=rng.uniform(-3.0, 3.0), sink=18.0)
            angle += step * rng.uniform(0.94, 1.06)
            index += 1

        # Two heavier posts frame the gate, sharpened stakes guard it from outside
        for side in (-1.0, 1.0):
            place("SM_WoodenPost", ring(CAMP, gate_angle + side * gate_half, CAMP_RADIUS), folder + "/Palisade", "Torpfosten", scale=2.9, yaw=rng.uniform(0.0, 360.0), sink=25.0)
            for stake in range(3):
                spot = ring(CAMP, gate_angle + side * (gate_half + 7.0 + stake * 6.0), CAMP_RADIUS + 0.9)
                place("SM_WoodenPost", spot, folder + "/Palisade", "Abwehrpfahl", scale=1.25, yaw=world_yaw(gate_angle + side * (gate_half + 7.0 + stake * 6.0)), pitch=-48.0, sink=30.0)

        # The posts are only looks; these invisible walls keep the player and the creatures out
        segments = 26
        span = (360.0 - 2.0 * gate_half) / segments
        chord = 2.0 * CAMP_RADIUS * TILE * math.sin(math.radians(span * 0.5)) + 12.0
        for segment in range(segments):
            middle = gate_angle + gate_half + span * (segment + 0.5)
            blocker(ring(CAMP, middle, CAMP_RADIUS), chord, 45.0, 300.0, world_yaw(middle) + 90.0, folder + "/Palisade/Kollision", "Palisadenwand {}".format(segment))

    # Fire place: a ring of stones, firewood in the middle, seats around it
    for stone in range(14):
        place("SM_MossyStonesPack_0{}".format(stone % 4), ring(CAMP, stone * 360.0 / 14 + rng.uniform(-6, 6), 0.86), folder + "/Feuerstelle", "Feuerstein",
              scale=rng.uniform(1.5, 2.1), yaw=rng.uniform(0.0, 360.0), sink=3.0)
    for log in range(5):
        place("SM_WoodBranchA", ring(CAMP, log * 72.0 + 10.0, 0.18), folder + "/Feuerstelle", "Brennholz", scale=0.6, yaw=world_yaw(log * 72.0 + 10.0), pitch=24.0, z=6.0)
    place("SM_FallenTreeAssembly", ring(CAMP, 205.0, 1.75), folder + "/Feuerstelle", "Sitzstamm 1", scale=1.15, yaw=world_yaw(205.0) + 90.0)
    place("SM_FallenTreeAssembly", ring(CAMP, 95.0, 1.8), folder + "/Feuerstelle", "Sitzstamm 2", scale=1.05, yaw=world_yaw(95.0) + 80.0)
    place("SM_OldWoodenBench", ring(CAMP, 150.0, 1.7), folder + "/Feuerstelle", "Bank am Feuer", yaw=world_yaw(150.0))
    place("SM_OldMetalPotB", ring(CAMP, 260.0, 1.15), folder + "/Feuerstelle", "Kessel", yaw=30.0)
    place("SM_WoodenBowls_00", ring(CAMP, 250.0, 1.4), folder + "/Feuerstelle", "Schale", yaw=10.0)

    # Edda's corner: a table with her things, away from the road
    place("SM_OldWoodenTable", (13.2, 52.0), folder + "/Edda", "Tisch", yaw=35.0)
    place("SM_OldWoodenBench", (13.6, 52.7), folder + "/Edda", "Bank", yaw=125.0)
    place("SM_ManMade_Lantern_02", (13.3, 52.0), folder + "/Edda", "Laterne auf dem Tisch", z=83.0, yaw=60.0)
    place("SM_OldWoodenCarafe", (12.95, 51.85), folder + "/Edda", "Karaffe", z=83.0)
    place("SM_WoodenBowls_03", (13.45, 52.2), folder + "/Edda", "Schale", z=83.0)
    place("SM_JuteJug", (12.6, 52.6), folder + "/Edda", "Krug", yaw=70.0)

    # Supplies where the crates of the blockout stand
    place("SM_WoodenAmmoCrate", (8.5, 55.5), folder + "/Vorraete", "Lange Kiste", yaw=25.0)
    place("SM_WoodenBox", (8.4, 55.45), folder + "/Vorraete", "Kiste obenauf", z=59.0, yaw=40.0)
    place("SM_WoodenBarrelB", (7.7, 56.3), folder + "/Vorraete", "Fass 1", yaw=15.0)
    place("SM_WoodenBarrelC", (8.5, 56.75), folder + "/Vorraete", "Fass 2", yaw=80.0)
    place("SM_WoodenBarrelA", (9.2, 56.3), folder + "/Vorraete", "Faesschen", yaw=130.0)
    place("SM_WoodenBox", (14.5, 49.5), folder + "/Vorraete", "Kiste am Weg", scale=1.5, yaw=15.0)
    place("SM_WoodenBox", (14.45, 49.5), folder + "/Vorraete", "Kiste am Weg oben", z=67.0, scale=1.2, yaw=50.0)
    place("SM_WoodenBarrelC", (15.1, 50.25), folder + "/Vorraete", "Fass am Weg", yaw=200.0)
    place("SM_ManMade_Lantern_02", (15.1, 50.25), folder + "/Vorraete", "Laterne auf dem Fass", z=97.0)

    # By the first tent: water, chest and tools
    place("SM_BarrelOnStand", (6.4, 50.9), folder + "/Zelt 1", "Fass auf Bock", yaw=250.0)
    place("SM_OldWoodenChest", (8.7, 48.5), folder + "/Zelt 1", "Truhe", yaw=140.0)
    place("SM_OldWoodenBucketA", (8.95, 50.5), folder + "/Zelt 1", "Eimer", yaw=20.0)
    place("SM_OldWoodenBucketB", (6.9, 51.6), folder + "/Zelt 1", "Eimer flach", yaw=95.0)
    place("SM_OldSaddle", (9.0, 48.1), folder + "/Zelt 1", "Sattel", yaw=60.0)

    # Wood yard by the second tent
    place("SM_TreeStumpB", (11.2, 56.9), folder + "/Holzplatz", "Hackklotz", scale=0.7, yaw=40.0)
    place("SM_Axe", (11.2, 56.9), folder + "/Holzplatz", "Axt", z=76.0, yaw=115.0)
    for piece in range(7):
        place("SM_WoodBranchA", (10.0 + rng.uniform(-0.25, 0.25), 57.3 + rng.uniform(-0.2, 0.2)), folder + "/Holzplatz", "Holzscheit", scale=0.7, yaw=20.0 + rng.uniform(-12, 12), z=piece * 9.0)
    place("SM_WoodenWheelbarrow", (12.4, 57.2), folder + "/Holzplatz", "Schubkarre", yaw=310.0)
    place("SM_WoodenWheelB", (14.9, 56.9), folder + "/Holzplatz", "Wagenrad", yaw=15.0)

    # Western side: trough and hitching post, a ladder against the palisade
    place("SM_HorseHitchingPost", (5.7, 53.4), folder + "/Traenke", "Anbindebalken", yaw=world_yaw(180.0) + 90.0)
    place("SM_OldWoodenTrough", (5.3, 52.0), folder + "/Traenke", "Trog", yaw=world_yaw(180.0))
    ladder_angle = 120.0
    place("SM_Ladder", ring(CAMP, ladder_angle, CAMP_RADIUS - 0.42), folder + "/Traenke", "Leiter", scale=1.25, yaw=world_yaw(ladder_angle) + 90.0, roll=-14.0)


def dress_rocks(level_actors):
    """Real rocks where the blockout has grey cubes; the cubes stay as invisible collision"""
    rocks = ["SM_Rock_0{}".format(index) for index in range(1, 8)]

    for actor in level_actors:
        if not isinstance(actor, unreal.StaticMeshActor) or actor.get_actor_label() != "Fels":
            continue

        place_folder = str(actor.get_folder_path())
        if place_folder not in ("Streuung", "Krater"):
            continue

        name = rng.choice(rocks)
        asset = mesh(name)
        if asset is None:
            continue

        box = asset.get_bounding_box()
        size = box.max - box.min
        scale = actor.get_actor_scale3d()
        width, height = max(scale.x, scale.y) * 100.0, scale.z * 100.0

        # Scatter rocks cover their cube; the rocks of the crater ring also reach its height, so the ring reads as a wall
        fit = width * 1.4 / max(size.x, size.y)
        if place_folder == "Krater":
            fit = min(max(fit, height * 1.05 / size.z), 1.6)

        place(name, tile_of(actor), place_folder + "/Kulisse", "Fels", scale=fit * rng.uniform(0.92, 1.1), yaw=rng.uniform(0.0, 360.0), sink=size.z * fit * 0.1)
        hide_placeholder(actor)


def dress_trees(level_actors):
    """Dead trunks where the blockout has dark cylinders"""
    for actor in level_actors:
        if not isinstance(actor, unreal.StaticMeshActor) or actor.get_actor_label() != "Toter Baum":
            continue

        roll = rng.random()
        if roll < 0.5:
            name, scale = "SM_TreeStumpA_00", rng.uniform(0.42, 0.56)
        elif roll < 0.8:
            name, scale = "SM_FirTreeStump", rng.uniform(1.25, 1.75)
        else:
            name, scale = "SM_BrokenPineStump", rng.uniform(1.4, 2.0)

        if place(name, tile_of(actor), "Streuung/Kulisse", "Toter Stamm", scale=scale, yaw=rng.uniform(0.0, 360.0), sink=6.0) is not None:
            hide_placeholder(actor)


def dress_crates(level_actors):
    for actor in level_actors:
        if isinstance(actor, unreal.StaticMeshActor) and str(actor.get_folder_path()) == "Lager" and actor.get_actor_label() in ("Kiste 1", "Kiste 2"):
            hide_placeholder(actor)


def dress_graveyard(taken):
    """A few graves south of the chapel"""
    folder = "Dorf/Kapelle/Kulisse"
    graves = [("SM_GraveA_01", (35.4, 51.2), 96.0), ("SM_GraveA_01", (37.0, 51.35), 84.0), ("SM_GraveB", (38.7, 51.3), 92.0),
              ("SM_GraveA_00", (36.2, 53.0), 88.0), ("SM_GraveA_01", (38.2, 53.1), 99.0)]
    for name, spot, yaw in graves:
        if any(math.hypot(spot[0] - x, spot[1] - y) < 1.1 for x, y in taken):
            continue
        place(name, spot, folder, "Grab", yaw=yaw + rng.uniform(-5, 5), roll=rng.uniform(-4, 4), sink=4.0)
        taken.append(spot)


def dress_fields(taken):
    """Fallen wood and dry weeds between the places, away from the roads"""
    reserved = [(13, 28, 6.3), (45.5, 22.5, 3.2), (52, 11, 9.6), (10.5, 52.5, 7.6), (53, 41, 5.2), (30, 13, 2.0), (31.5, 15.5, 1.5), (29.5, 39.5, 1.6)]

    def free(x, y, road, others):
        if not (3.5 < x < 60.5 and 3.5 < y < 60.5) or road_distance(x, y) < road:
            return False
        if any(math.hypot(x - cx, y - cy) < r for cx, cy, r in reserved):
            return False
        if any(x0 - 0.6 < x < x1 + 1.6 and y0 - 0.6 < y < y1 + 1.6 for x0, y0, x1, y1 in HOUSES):
            return False
        return not any(math.hypot(x - ox, y - oy) < others for ox, oy in taken)

    wood = [("SM_FallenFirTree", 0.8), ("SM_FallenPineTree", 1.1), ("SM_FallenPineAssembly", 1.2), ("SM_BrokenPineAssembly", 1.0), ("SM_OldStump", 1.0),
            ("SM_BrokenTreeStump", 1.3), ("SM_WoodBranchB", 1.3), ("SM_WoodBranchC", 1.4), ("SM_Branch_00", 1.0), ("SM_ForestRootsA", 1.0), ("SM_WoodStump", 1.4)]
    placed, tries = 0, 0
    while placed < 38 and tries < 4000:
        tries += 1
        x, y = rng.uniform(4, 60), rng.uniform(4, 60)
        if not free(x, y, 3.2, 2.2):
            continue
        name, scale = rng.choice(wood)
        if place(name, (x, y), "Streuung/Kulisse/Totholz", "Totholz", scale=scale * rng.uniform(0.85, 1.15), yaw=rng.uniform(0.0, 360.0), sink=5.0) is not None:
            taken.append((x, y))
            placed += 1

    weeds = ["SM_DryPlant00_V1", "SM_DryPlant00_V2", "SM_DryPlant00_V3", "SM_DryPlant00_V4", "SM_DryPlantSet01_V01", "SM_DryPlantSet01_V02", "SM_DryPlantSet01_V06",
             "SM_DryPlantSet01_V09", "SM_DryPlantSet01_V13", "SM_DryPlantSet_V1", "SM_DryPlantSet_V2", "SM_DryPlantSet_V3", "SM_DryPlantSet_V6", "SM_DryPlantSet_V9",
             "SM_DeadBroomBush_V1", "SM_DeadBroomBush_V2", "SM_DeadBroomBush_V3", "SM_DeadBroomBush_V4", "SM_ThatchingGrass00_V10", "SM_ThatchingGrass00_V2", "SM_ThatchingGrass00_V6"]
    clusters, tries = 0, 0
    while clusters < 95 and tries < 6000:
        tries += 1
        x, y = rng.uniform(4, 60), rng.uniform(4, 60)
        if not free(x, y, 1.75, 0.9):
            continue
        for plant in range(rng.randint(2, 4)):
            spot = (x + rng.uniform(-0.55, 0.55), y + rng.uniform(-0.55, 0.55))
            place(rng.choice(weeds), spot, "Streuung/Kulisse/Gestruepp", "Gestruepp", scale=rng.uniform(0.9, 1.5), yaw=rng.uniform(0.0, 360.0), sink=2.0)
        clusters += 1


# ---------------------------------------------------------------- Run

def dress():
    world_object = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    if world_object is None:
        unreal.log_error("Vael: could not load {}".format(LEVEL_PATH))
        return

    level_actors = list(actors.get_all_level_actors())
    if any(unreal.Name(DRESSING_TAG) in actor.get_editor_property("tags") for actor in level_actors):
        unreal.log_warning("Vael: {} is dressed already, nothing was changed. Delete the folders named Kulisse in the editor to dress it anew.".format(LEVEL_PATH))
        return

    load_meshes()

    # Tiles the blockout already uses for rocks and trees, so new things keep their distance
    taken = [tile_of(actor) for actor in level_actors if isinstance(actor, unreal.StaticMeshActor) and actor.get_actor_label() in ("Fels", "Toter Baum")]

    try:
        ground = make_ground_material()
    except Exception as error:
        ground = None
        notes.append("ground material failed: {}".format(error))
    if ground is not None:
        for actor in level_actors:
            if isinstance(actor, unreal.StaticMeshActor) and actor.get_actor_label() == "Ascheboden":
                actor.static_mesh_component.set_material(0, ground)

    dress_camp()
    dress_crates(level_actors)
    dress_rocks(level_actors)
    dress_trees(level_actors)
    dress_graveyard(taken)
    dress_fields(taken)

    saved = unreal.EditorLoadingAndSavingUtils.save_map(world_object, LEVEL_PATH)

    lines = ["saved: {}".format(saved), "ground material: {}".format(ground is not None)]
    lines += ["{:<40} {}".format(folder, count) for folder, count in sorted(counts.items())]
    lines += ["total new actors: {}".format(sum(counts.values()))]
    lines += ["missing: {}".format(", ".join(notes) if notes else "nothing")]
    with open(os.path.join(unreal.Paths.project_saved_dir(), "DressLog.txt"), "w", encoding="utf-8") as file:
        file.write("\n".join(lines))

    if saved:
        unreal.log("Vael: dressed {} with {} new actors".format(LEVEL_PATH, sum(counts.values())))
    else:
        unreal.log_error("Vael: could not save {}".format(LEVEL_PATH))


dress()
