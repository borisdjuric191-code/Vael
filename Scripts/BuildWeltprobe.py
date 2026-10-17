# Builds the open world tech test L_Weltprobe: the way from the Aschenpass into the Forstsaum, about 1 km long.
#
# - A World Partition level: the land streams in and out around the players, no loading screens.
# - Only creates new assets (the level, its actor files and one ground material instance); refuses if the level exists.
# - Blockout quality: pack rocks for the cliffs, shapes for the trees of the Forstsaum. The point is the technique:
#   streaming, two regions in one level, spawners that come back without doubles, a pass as chokepoint.
# - Route points tagged "VaelRoute" lead the stream test (-VaelStreamTest) from the camp to the far end of the forest.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="Scripts/BuildWeltprobe.py" -EnablePlugins=PythonScriptPlugin
#
# Layout in metres: x runs from the camp (x 0) through the Aschenpass (narrowest at x 440-500) into the Forstsaum (to x 1000).
# The way is 300 m wide, cliffs close it at the sides. Clearings of about 40 m stay free for fights.

import math
import random

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Weltprobe"
DRESSING_FOLDER = "/Game/Vael/Maps/Kulisse"
BLOCKOUT_FOLDER = "/Game/Vael/Maps/Blockout"
M = 100.0  # one metre in cm

LENGTH = 1000.0      # m, from the camp to the end of the forest
HALF_WIDTH = 150.0   # m, half the width of the whole stretch
GROUND_TILE = 50.0   # m, side of one ground tile

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")
CONE = unreal.load_asset("/Engine/BasicShapes/Cone.Cone")
SHAPE_MATERIAL = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(1017)


# ---------------------------------------------------------------- Helpers

def v(x, y, z=0.0):
    """World location from metres"""
    return unreal.Vector(x * M, y * M, z * M)


def srgb(hex_code):
    def to_linear(channel):
        value = int(channel, 16) / 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4
    return unreal.LinearColor(to_linear(hex_code[0:2]), to_linear(hex_code[2:4]), to_linear(hex_code[4:6]), 1.0)


def blockout_material(name, hex_code):
    """Placeholder material in one color, as in BuildAschenmark.py; an existing one is reused"""
    path = "{}/{}".format(BLOCKOUT_FOLDER, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, BLOCKOUT_FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, SHAPE_MATERIAL)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance, "Color", srgb(hex_code))
    unreal.EditorAssetLibrary.save_loaded_asset(instance, False)
    return instance


def forest_ground_material():
    """Forest floor: the path material of the Aschenmark with a mossy green tint, as a new instance"""
    name = "MI_Weltprobe_Waldboden"
    path = "{}/{}".format(DRESSING_FOLDER, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)

    library = unreal.MaterialEditingLibrary
    source = unreal.load_asset("{}/MI_Aschenmark_Weg".format(DRESSING_FOLDER))
    instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DRESSING_FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(instance, source.get_editor_property("parent"))
    for key in ("Albedo", "Normal", "Roughness", "Albedo2"):
        library.set_material_instance_texture_parameter_value(instance, key, library.get_material_instance_texture_parameter_value(source, key))
    library.set_material_instance_vector_parameter_value(instance, "Tint", unreal.LinearColor(0.30, 0.34, 0.22, 1.0))
    library.set_material_instance_vector_parameter_value(instance, "Tint2", unreal.LinearColor(0.17, 0.20, 0.12, 1.0))
    library.set_material_instance_scalar_parameter_value(instance, "TileSize", 380.0)
    library.set_material_instance_scalar_parameter_value(instance, "MacroSize", 4200.0)
    library.set_material_instance_scalar_parameter_value(instance, "Desaturation", 0.25)
    library.update_material_instance(instance)
    unreal.EditorAssetLibrary.save_loaded_asset(instance, False)
    return instance


def spawn(actor_class, location, folder, label, yaw=0.0, streamed=True):
    actor = actors.spawn_actor_from_class(actor_class, location, unreal.Rotator(0.0, 0.0, yaw))
    actor.set_folder_path(folder)
    actor.set_actor_label(label)
    if not streamed:
        # Lights, sky and the like stay loaded everywhere
        actor.set_editor_property("is_spatially_loaded", False)
    return actor


def shape(mesh, material, x, y, size_x, size_y, height, folder, label, base=0.0, yaw=0.0, collision=True):
    """A static mesh of the engine shapes, sizes and heights in metres, standing on base"""
    actor = spawn(unreal.StaticMeshActor, v(x, y, base + height * 0.5), folder, label, yaw)
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    component.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(size_x, size_y, height))
    if not collision:
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return actor


def pack_mesh(name):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for data in registry.get_assets_by_path("/Game/Rock_Collection_04", recursive=True):
        if str(data.asset_name) == name:
            return unreal.load_asset(str(data.package_name))
    return None


ROCKS = []


def rock(x, y, scale, folder, label, yaw=None):
    """A pack rock, purely visual; the cliffs get invisible walls for collision"""
    if not ROCKS:
        return None
    asset = rng.choice(ROCKS)
    box = asset.get_bounding_box()
    actor = spawn(unreal.StaticMeshActor, v(x, y, -box.min.z * scale / M - 0.5), folder, label, rng.uniform(0.0, 360.0) if yaw is None else yaw)
    actor.static_mesh_component.set_static_mesh(asset)
    actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale * rng.uniform(1.1, 1.6)))
    return actor


def wall(x0, y0, x1, y1, folder, label):
    """Invisible wall between two points, 12 m high"""
    length = math.hypot(x1 - x0, y1 - y0)
    yaw = math.degrees(math.atan2(y1 - y0, x1 - x0))
    actor = spawn(unreal.StaticMeshActor, v((x0 + x1) * 0.5, (y0 + y1) * 0.5, 6.0), folder, label, yaw)
    actor.static_mesh_component.set_static_mesh(CUBE)
    actor.static_mesh_component.set_editor_property("visible", False)
    actor.set_actor_hidden_in_game(True)
    actor.set_actor_scale3d(unreal.Vector(length + 2.0, 3.0, 12.0))
    return actor


def spawner(kind_class, x, y, count, folder, label, spread=150.0):
    actor = spawn(unreal.VaelCreatureSpawner, v(x, y, 1.0), folder, label)
    actor.set_editor_property("creature_kind", kind_class.static_class())
    actor.set_editor_property("count", count)
    actor.set_editor_property("spread", spread)
    return actor


def region(name, x0, x1, corruption, folder):
    actor = spawn(unreal.VaelRegion, v((x0 + x1) * 0.5, 0.0, 0.0), folder, "Region " + name, streamed=False)
    actor.set_editor_property("region_name", unreal.Text(name))
    actor.set_editor_property("start_corruption", corruption)
    box = actor.get_component_by_class(unreal.BoxComponent)
    box.set_box_extent(unreal.Vector((x1 - x0) * 0.5 * M, (HALF_WIDTH + 20.0) * M, 60.0 * M), False)
    return actor


# ---------------------------------------------------------------- Shape of the way

def corridor(x):
    """Half the free width of the way at x, in metres: wide in the ash, narrow in the pass, wide again in the forest"""
    points = [(-20.0, 60.0), (60.0, 110.0), (340.0, 120.0), (430.0, 16.0), (500.0, 14.0), (580.0, 125.0), (1020.0, 125.0)]
    for (xa, wa), (xb, wb) in zip(points, points[1:]):
        if xa <= x <= xb:
            t = (x - xa) / (xb - xa)
            t = t * t * (3.0 - 2.0 * t)
            return wa + (wb - wa) * t
    return points[-1][1]


def road_y(x):
    """The middle of the road wanders a little"""
    return 14.0 * math.sin(x / 140.0) * min(1.0, corridor(x) / 60.0)


# Clearings kept free of trees and rocks, with who lives there: (x, y, radius m)
CLEARINGS = [(0.0, 0.0, 25.0), (130.0, -30.0, 22.0), (250.0, 35.0, 22.0), (360.0, -20.0, 20.0), (620.0, 30.0, 22.0), (740.0, -40.0, 22.0), (880.0, 20.0, 22.0)]


def in_clearing(x, y, margin=0.0):
    return any(math.hypot(x - cx, y - cy) < r + margin for cx, cy, r in CLEARINGS)


# ---------------------------------------------------------------- Level

def build():
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
        unreal.log_warning("Vael: {} exists already, nothing was changed. Delete it in the editor to build it anew.".format(LEVEL_PATH))
        return

    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_editor.new_level(LEVEL_PATH, True):
        unreal.log_error("Vael: could not create the open world level {}".format(LEVEL_PATH))
        return
    world_object = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

    for index in range(1, 8):
        asset = pack_mesh("SM_Rock_0{}".format(index))
        if asset is not None:
            ROCKS.append(asset)

    m = {
        "ash": unreal.load_asset("{}/MI_Aschenmark_Boden".format(DRESSING_FOLDER)) or blockout_material("MI_Blockout_Ash", "37302c"),
        "road": unreal.load_asset("{}/MI_Aschenmark_Weg".format(DRESSING_FOLDER)) or blockout_material("MI_Blockout_Road", "4a3f35"),
        "forest": forest_ground_material(),
        "trunk": blockout_material("MI_Blockout_Trunk", "3b2e24"),
        "crown": blockout_material("MI_Blockout_Crown", "2c3b22"),
        "crown_dark": blockout_material("MI_Blockout_CrownDark", "1f2a1a"),
        "tent": blockout_material("MI_Blockout_Tent", "6b5a44"),
    }

    # Light and sky, loaded everywhere
    sun = spawn(unreal.DirectionalLight, v(0, 0, 30), "Licht", "Sonne", streamed=False)
    sun.set_actor_rotation(unreal.Rotator(0.0, -50.0, -30.0), False)
    sun.light_component.set_editor_property("intensity", 4.0)
    sun.light_component.set_light_color(unreal.LinearColor(1.0, 0.85, 0.72, 1.0))
    sky = spawn(unreal.SkyLight, v(0, 0, 20), "Licht", "Himmelslicht", streamed=False)
    sky.light_component.set_editor_property("real_time_capture", True)
    spawn(unreal.SkyAtmosphere, v(0, 0, 0), "Licht", "Atmosphaere", streamed=False)
    fog = spawn(unreal.ExponentialHeightFog, v(0, 0, 0), "Licht", "Dunst", streamed=False)
    fog.component.set_editor_property("fog_density", 0.025)
    fog.component.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.24, 0.22, 0.19, 1.0))

    # Two regions in one level; the corruption of the forest edge is lower (my test values)
    region("Aschenpass", -40.0, 540.0, -1.0, "Spiel")
    region("Forstsaum", 540.0, LENGTH + 40.0, 30.0, "Spiel")

    # Camp at the start
    spawn(unreal.PlayerStart, v(0.0, 0.0, 1.0), "Spiel", "Spielerstart", yaw=0.0)
    camp = spawn(unreal.VaelSafeZone, v(-6.0, 0.0, 0.0), "Lager", "Sicherer Ort")
    camp.set_editor_property("radius", 1400.0)
    spawn(unreal.VaelFireLight, v(-6.0, 0.0, 1.0), "Lager", "Lagerfeuer-Licht")
    for index, (x, y, yaw) in enumerate([(-14.0, 6.0, 20.0), (-14.0, -6.0, -20.0), (-2.0, 9.0, 80.0)]):
        shape(CONE, m["tent"], x, y, 3.5, 3.5, 2.6, "Lager", "Zelt {}".format(index + 1), yaw=yaw)

    # Ground tiles: ash up to the pass, forest floor behind it, each tile streams on its own
    tiles = int(math.ceil((LENGTH + 80.0) / GROUND_TILE))
    rows = int(math.ceil(2.0 * HALF_WIDTH / GROUND_TILE))
    for ix in range(tiles):
        x = -40.0 + (ix + 0.5) * GROUND_TILE
        material = m["ash"] if x < 520.0 else m["forest"]
        for iy in range(rows):
            y = -HALF_WIDTH + (iy + 0.5) * GROUND_TILE
            shape(CUBE, material, x, y, GROUND_TILE, GROUND_TILE, 0.2, "Boden", "Boden {}-{}".format(ix, iy), base=-0.2)

    # Road along the middle, in pieces of 10 m
    x = -10.0
    while x < LENGTH:
        x_next = x + 10.0
        y0, y1 = road_y(x), road_y(x_next)
        yaw = math.degrees(math.atan2(y1 - y0, 10.0))
        shape(CUBE, m["road"], x + 5.0, (y0 + y1) * 0.5, 10.6, 5.0, 0.02, "Weg", "Weg {:.0f}".format(x), base=0.0, yaw=yaw, collision=False)
        x = x_next

    # Cliffs on both sides: rocks along the edge of the corridor, invisible walls behind them, rocks filling the rest
    step = 12.0
    x = -40.0
    previous = None
    while x <= LENGTH + 40.0:
        half = corridor(x)
        for side in (-1.0, 1.0):
            edge = side * half
            rock(x + rng.uniform(-3.0, 3.0), edge + side * rng.uniform(2.0, 6.0), rng.uniform(2.6, 4.2), "Felsen/Rand", "Randfels")
            # A second, higher row a little further out
            if abs(edge) + 14.0 < HALF_WIDTH:
                rock(x + rng.uniform(-5.0, 5.0), edge + side * rng.uniform(12.0, 18.0), rng.uniform(4.0, 6.5), "Felsen/Rand", "Hochfels")
        if previous is not None:
            px, ph = previous
            for side in (-1.0, 1.0):
                wall(px, side * (ph + 2.0), x, side * (half + 2.0), "Felsen/Wand", "Felswand")
        previous = (x, half)
        x += step

    # End walls at both ends
    wall(-40.0, -HALF_WIDTH, -40.0, HALF_WIDTH, "Felsen/Wand", "Ende Lager")
    wall(LENGTH + 40.0, -HALF_WIDTH, LENGTH + 40.0, HALF_WIDTH, "Felsen/Wand", "Ende Wald")

    # Scattered boulders in the ash, never on the road or in clearings
    for _ in range(70):
        x = rng.uniform(20.0, 520.0)
        half = corridor(x)
        y = rng.uniform(-half + 6.0, half - 6.0)
        if abs(y - road_y(x)) < 8.0 or in_clearing(x, y, 4.0):
            continue
        rock(x, y, rng.uniform(0.6, 1.4), "Felsen/Streuung", "Brocken")

    # Forest: trees thick at the edges, in groves between the clearings, the road and the clearings stay free
    planted = 0
    for _ in range(1400):
        x = rng.uniform(530.0, LENGTH + 30.0)
        half = corridor(x)
        y = rng.uniform(-half, half)
        edge_share = abs(y) / half
        if abs(y - road_y(x)) < 9.0 or in_clearing(x, y, 3.0):
            continue
        # Thinner towards the road, so there is room to fight between the trees
        if rng.random() > 0.25 + 0.75 * edge_share * edge_share:
            continue
        height = rng.uniform(9.0, 16.0)
        trunk = rng.uniform(0.45, 0.8)
        folder = "Wald/Baeume"
        shape(CYLINDER, m["trunk"], x, y, trunk, trunk, height * 0.55, folder, "Stamm")
        shape(CONE, m["crown"] if rng.random() < 0.6 else m["crown_dark"], x, y, height * 0.38, height * 0.38, height * 0.7, folder, "Krone", base=height * 0.3, collision=False)
        planted += 1

    # Creatures: the Aschenmark kinds stand in for the forest ones that are not designed yet
    spawner(unreal.VaelEmberCrawlerData, 130.0, -30.0, 4, "Gegner/Aschenpass", "Glutkriecher Lichtung")
    spawner(unreal.VaelAshHarpyData, 250.0, 35.0, 3, "Gegner/Aschenpass", "Harpyien Lichtung")
    spawner(unreal.VaelAshRollerData, 360.0, -20.0, 2, "Gegner/Aschenpass", "Aschenroller Lichtung")
    spawner(unreal.VaelSickleDollData, 470.0, 0.0, 2, "Gegner/Pass", "Sichelpuppen in der Enge")
    spawner(unreal.VaelVeilMonkeyData, 620.0, 30.0, 2, "Gegner/Forstsaum", "Schleieraffen Lichtung")
    spawner(unreal.VaelDraculaupeData, 740.0, -40.0, 3, "Gegner/Forstsaum", "Draculaupen Lichtung")
    spawner(unreal.VaelAshHarpyData, 880.0, 20.0, 3, "Gegner/Forstsaum", "Harpyien am Waldende")

    # Route for the stream test: along the road from the camp to the end of the forest
    number = 1
    x = 40.0
    while x <= LENGTH - 20.0:
        point = spawn(unreal.TargetPoint, v(x, road_y(x), 1.0), "Spiel/Route", "Route {}".format(number))
        point.set_editor_property("tags", [unreal.Name("VaelRoute"), unreal.Name(str(number))])
        number += 1
        x += 40.0

    if not unreal.EditorLoadingAndSavingUtils.save_map(world_object, LEVEL_PATH):
        unreal.log_error("Vael: could not save {}".format(LEVEL_PATH))
        return
    # The actors of an open world level live in their own files
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log("Vael: built {} with {} trees and {} route points".format(LEVEL_PATH, planted, number - 1))


build()
