# Dresses the blockout level L_Aschenmark with the meshes of the downloaded packs (first pass, to be reworked by hand).
#
# - Only adds: every new actor carries the tag "VaelKulisse" and sits in an Outliner folder ending in "Kulisse".
#   Deleting those folders in the editor undoes the dressing.
# - Placeholders that get a real mesh (rocks, dead trees, crates) are not deleted. They are made invisible and keep
#   their collision, so the game plays exactly as before. They carry the tag "VaelPlatzhalterVersteckt".
# - Ash ground, paths, house floors and the crater get world-aligned materials (M_Vael_Boden and its instances).
# - Every place is dressed once: a place whose Kulisse folder exists is skipped, so the script can grow and run again.
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


def place(name, tile, folder, label, scale=1.0, yaw=0.0, pitch=0.0, roll=0.0, z=0.0, sink=0.0, stretch=None, grounded=True):
    """Puts a pack mesh on the ground at a tile position. Purely visual: it never blocks movement."""
    asset = mesh(name)
    if asset is None:
        return None

    box = asset.get_bounding_box()
    scale_vector = unreal.Vector(scale, scale, scale) if stretch is None else unreal.Vector(scale * stretch[0], scale * stretch[1], scale * stretch[2])
    location = world(tile[0], tile[1], z - (box.min.z * scale_vector.z if grounded else 0.0) - sink)

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


# ---------------------------------------------------------------- Ground materials

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


def surface(name, suffix):
    return unreal.load_asset("/Game/Megascans/Surfaces/{0}/T_{0}_{1}".format(name, suffix))


def build_ground_graph(material):
    """Ground laid by world position, so it never stretches. Two surfaces are mixed by a large soft mask and every
    surface is sampled at two sizes, which hides the repeating pattern."""
    library = unreal.MaterialEditingLibrary
    library.delete_all_material_expressions(material)

    def node(expression_class, x, y):
        return library.create_material_expression(material, expression_class, x, y)

    def scalar(name, value, x, y):
        parameter = node(unreal.MaterialExpressionScalarParameter, x, y)
        parameter.set_editor_property("parameter_name", name)
        parameter.set_editor_property("default_value", value)
        return parameter

    def tint(name, color, x, y):
        parameter = node(unreal.MaterialExpressionVectorParameter, x, y)
        parameter.set_editor_property("parameter_name", name)
        parameter.set_editor_property("default_value", color)
        return parameter

    position = node(unreal.MaterialExpressionWorldPosition, -2200, 0)
    plane = node(unreal.MaterialExpressionComponentMask, -2000, 0)
    plane.set_editor_property("r", True)
    plane.set_editor_property("g", True)
    plane.set_editor_property("b", False)
    plane.set_editor_property("a", False)
    library.connect_material_expressions(position, "", plane, "")

    tile_size = scalar("TileSize", 450.0, -2000, 200)
    macro_size = scalar("MacroSize", 5200.0, -2000, 320)

    def coordinates(size, factor, x, y):
        divide = node(unreal.MaterialExpressionDivide, x, y)
        library.connect_material_expressions(plane, "", divide, "A")
        if factor == 1.0:
            library.connect_material_expressions(size, "", divide, "B")
        else:
            scaled = node(unreal.MaterialExpressionMultiply, x - 180, y + 60)
            scaled.set_editor_property("const_b", factor)
            library.connect_material_expressions(size, "", scaled, "A")
            library.connect_material_expressions(scaled, "", divide, "B")
        return divide

    near, far, other = coordinates(tile_size, 1.0, -1600, 0), coordinates(tile_size, 3.37, -1600, 160), coordinates(tile_size, 1.43, -1600, 320)
    macro, wide = coordinates(macro_size, 1.0, -1600, 480), coordinates(macro_size, 2.9, -1600, 640)

    def sample(name, texture, uvs, x, y, parameter=True):
        expression = node(unreal.MaterialExpressionTextureSampleParameter2D if parameter else unreal.MaterialExpressionTextureSample, x, y)
        if parameter:
            expression.set_editor_property("parameter_name", name)
        expression.set_editor_property("texture", texture)
        expression.set_editor_property("sampler_type", texture_sampler(texture))
        library.connect_material_expressions(uvs, "", expression, "UVs")
        return expression

    mud_d, mud_n, mud_r, forest_d = surface("HeavyMud", "D"), surface("HeavyMud", "N"), surface("HeavyMud", "R"), surface("ForestGround", "D")

    albedo_near = sample("Albedo", mud_d, near, -1250, -200)
    albedo_far = sample("", mud_d, far, -1250, 60, parameter=False)
    albedo_other = sample("Albedo2", forest_d, other, -1250, 320)
    normal = sample("Normal", mud_n, near, -1250, 900)
    roughness = sample("Roughness", mud_r, near, -1250, 1160)
    mask = sample("", mud_r, macro, -1250, 580, parameter=False)
    shade = sample("", mud_r, wide, -1250, 740, parameter=False)

    # The far sample must follow the texture the instance picks for "Albedo"; a plain sample can't, so it stays the
    # default surface and only adds large soft variation
    first = node(unreal.MaterialExpressionLinearInterpolate, -900, -100)
    first.set_editor_property("const_alpha", 0.45)
    library.connect_material_expressions(albedo_near, "RGB", first, "A")
    library.connect_material_expressions(albedo_far, "RGB", first, "B")

    first_tinted = node(unreal.MaterialExpressionMultiply, -700, -100)
    library.connect_material_expressions(first, "", first_tinted, "A")
    library.connect_material_expressions(tint("Tint", unreal.LinearColor(0.62, 0.58, 0.56, 1.0), -900, -320), "", first_tinted, "B")

    second_tinted = node(unreal.MaterialExpressionMultiply, -700, 300)
    library.connect_material_expressions(albedo_other, "RGB", second_tinted, "A")
    library.connect_material_expressions(tint("Tint2", unreal.LinearColor(0.40, 0.38, 0.36, 1.0), -900, 460), "", second_tinted, "B")

    # Soft mask with a firm edge: (mask - bias) * contrast, clamped
    bias = node(unreal.MaterialExpressionSubtract, -900, 620)
    library.connect_material_expressions(mask, "R", bias, "A")
    library.connect_material_expressions(scalar("MaskBias", 0.45, -1100, 700), "", bias, "B")
    contrast = node(unreal.MaterialExpressionMultiply, -740, 620)
    library.connect_material_expressions(bias, "", contrast, "A")
    library.connect_material_expressions(scalar("MaskContrast", 5.0, -900, 760), "", contrast, "B")
    clamped = node(unreal.MaterialExpressionSaturate, -580, 620)
    library.connect_material_expressions(contrast, "", clamped, "")

    mixed = node(unreal.MaterialExpressionLinearInterpolate, -420, 100)
    library.connect_material_expressions(first_tinted, "", mixed, "A")
    library.connect_material_expressions(second_tinted, "", mixed, "B")
    library.connect_material_expressions(clamped, "", mixed, "Alpha")

    # Slow change of brightness across the map
    brightness = node(unreal.MaterialExpressionLinearInterpolate, -420, 760)
    brightness.set_editor_property("const_a", 0.78)
    brightness.set_editor_property("const_b", 1.12)
    library.connect_material_expressions(shade, "R", brightness, "Alpha")
    shaded = node(unreal.MaterialExpressionMultiply, -240, 200)
    library.connect_material_expressions(mixed, "", shaded, "A")
    library.connect_material_expressions(brightness, "", shaded, "B")

    grey = node(unreal.MaterialExpressionDesaturation, -80, 200)
    library.connect_material_expressions(shaded, "", grey, "")
    library.connect_material_expressions(scalar("Desaturation", 0.35, -240, 380), "", grey, "Fraction")

    # Ash and dry earth don't shine: the roughness never drops below RoughnessMin and the highlight is turned down
    rough = node(unreal.MaterialExpressionLinearInterpolate, -420, 1160)
    rough.set_editor_property("const_b", 1.0)
    library.connect_material_expressions(scalar("RoughnessMin", 0.82, -640, 1300), "", rough, "A")
    library.connect_material_expressions(roughness, "R", rough, "Alpha")

    library.connect_material_property(grey, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(scalar("Specular", 0.2, -240, 1300), "", unreal.MaterialProperty.MP_SPECULAR)
    library.recompile_material(material)


def make_ground_materials():
    """Material M_Vael_Boden and its instances for the ash ground, the paths and the crater. Returns them by name."""
    for name, suffix in (("HeavyMud", "D"), ("HeavyMud", "N"), ("HeavyMud", "R"), ("ForestGround", "D"), ("ForestGround", "N"), ("ForestGround", "R")):
        if surface(name, suffix) is None:
            notes.append("ground texture T_{}_{}".format(name, suffix))
            return {}

    library = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material_path = "{}/M_Vael_Boden".format(DRESSING_FOLDER)

    if unreal.EditorAssetLibrary.does_asset_exist(material_path):
        material = unreal.load_asset(material_path)
    else:
        material = tools.create_asset("M_Vael_Boden", DRESSING_FOLDER, unreal.Material, unreal.MaterialFactoryNew())

    # Rebuild the graph when it is older than this script (RoughnessMin is its newest parameter)
    rebuilt = "RoughnessMin" not in [str(name) for name in library.get_scalar_parameter_names(material)]
    if rebuilt:
        build_ground_graph(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material, False)

    # Dark ash ground, lighter and warmer paths so they stay readable from above, burnt crater
    mud = {"Albedo": surface("HeavyMud", "D"), "Normal": surface("HeavyMud", "N"), "Roughness": surface("HeavyMud", "R"), "Albedo2": surface("ForestGround", "D")}
    path_ground = {"Albedo": surface("ForestGround", "D"), "Normal": surface("ForestGround", "N"), "Roughness": surface("ForestGround", "R"), "Albedo2": surface("HeavyMud", "D")}
    looks = {
        "MI_Aschenmark_Boden": {"textures": mud,
                                "colors": {"Tint": unreal.LinearColor(0.34, 0.31, 0.30, 1.0), "Tint2": unreal.LinearColor(0.20, 0.19, 0.185, 1.0)},
                                "scalars": {"TileSize": 450.0, "MacroSize": 5200.0, "Desaturation": 0.55}},
        "MI_Aschenmark_Weg": {"textures": path_ground,
                              "colors": {"Tint": unreal.LinearColor(0.62, 0.52, 0.42, 1.0), "Tint2": unreal.LinearColor(0.50, 0.43, 0.36, 1.0)},
                              "scalars": {"TileSize": 330.0, "MacroSize": 2600.0, "Desaturation": 0.35}},
        "MI_Aschenmark_Mark": {"textures": mud,
                               "colors": {"Tint": unreal.LinearColor(0.20, 0.035, 0.04, 1.0), "Tint2": unreal.LinearColor(0.07, 0.012, 0.016, 1.0)},
                               "scalars": {"Desaturation": 0.0, "MacroSize": 1400.0}},
        "MI_Aschenmark_Krater": {"textures": mud,
                                 "colors": {"Tint": unreal.LinearColor(0.13, 0.10, 0.095, 1.0), "Tint2": unreal.LinearColor(0.06, 0.055, 0.055, 1.0)},
                                 "scalars": {"Desaturation": 0.6}},
    }

    instances = {}
    for name, look in looks.items():
        path = "{}/{}".format(DRESSING_FOLDER, name)
        exists = unreal.EditorAssetLibrary.does_asset_exist(path)
        if exists:
            instance = unreal.load_asset(path)
        else:
            instance = tools.create_asset(name, DRESSING_FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            library.set_material_instance_parent(instance, material)
        instances[name] = instance

        # An existing instance keeps what was tuned in the editor, unless the graph is new and its values mean something else
        if exists and not rebuilt:
            continue

        for key, texture in look.get("textures", {}).items():
            library.set_material_instance_texture_parameter_value(instance, key, texture)
        for key, color in look.get("colors", {}).items():
            library.set_material_instance_vector_parameter_value(instance, key, color)
        for key, value in look.get("scalars", {}).items():
            library.set_material_instance_scalar_parameter_value(instance, key, value)
        library.update_material_instance(instance)
        unreal.EditorAssetLibrary.save_loaded_asset(instance, False)

    return instances


def make_water_material():
    """Still, dark water for the deep middle of the pond"""
    path = "{}/M_Vael_Wasser".format(DRESSING_FOLDER)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)

    library = unreal.MaterialEditingLibrary
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_Vael_Wasser", DRESSING_FOLDER, unreal.Material, unreal.MaterialFactoryNew())

    color = library.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -400, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.012, 0.022, 0.026, 1.0))
    library.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    for index, (name, value, target) in enumerate((("Roughness", 0.07, unreal.MaterialProperty.MP_ROUGHNESS), ("Specular", 0.6, unreal.MaterialProperty.MP_SPECULAR))):
        parameter = library.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -400, 200 + index * 120)
        parameter.set_editor_property("parameter_name", name)
        parameter.set_editor_property("default_value", value)
        library.connect_material_property(parameter, "", target)

    library.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    return material

def lay_ground(level_actors, instances):
    """Ash ground, paths, house floors and the crater get the world-aligned materials"""
    def is_flat(actor, label, folder_start):
        return isinstance(actor, unreal.StaticMeshActor) and actor.get_actor_label().startswith(label) and str(actor.get_folder_path()).startswith(folder_start)

    for actor in level_actors:
        look = None
        if is_flat(actor, "Ascheboden", "Boden"):
            look = "MI_Aschenmark_Boden"
        elif is_flat(actor, "Weg ", "Wege") or is_flat(actor, "Boden", "Dorf"):
            look = "MI_Aschenmark_Weg"
        elif is_flat(actor, "Verbrannter Boden", "Krater"):
            look = "MI_Aschenmark_Krater"
        elif is_flat(actor, "Tiefes Wasser", "Teich") and "Wasser" in instances:
            actor.static_mesh_component.set_material(0, instances["Wasser"])

        if look is not None and look in instances:
            actor.static_mesh_component.set_material(0, instances[look])
            counts["Boden: " + look] = counts.get("Boden: " + look, 0) + 1


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
        place("SM_WoodBranchA", (10.0 + rng.uniform(-0.25, 0.25), 57.3 + rng.uniform(-0.2, 0.2)), folder + "/Holzplatz", "Holzscheit", scale=0.7, yaw=20.0 + rng.uniform(-12, 12), z=(piece // 3) * 14.0)
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
        if not isinstance(actor, unreal.StaticMeshActor) or actor.get_actor_label() != "Fels" or unreal.Name(HIDDEN_TAG) in actor.get_editor_property("tags"):
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
        if not isinstance(actor, unreal.StaticMeshActor) or actor.get_actor_label() != "Toter Baum" or str(actor.get_folder_path()) != "Streuung" or unreal.Name(HIDDEN_TAG) in actor.get_editor_property("tags"):
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


def dress_village(level_actors):
    """Dry-stone ruins where the blockout has wall cubes, rubble and fallen beams inside, the well and a few fences"""
    local = random.Random(733)
    houses = {"Dorf/Haus 1": (22, 33, 27, 37), "Dorf/Haus 2": (33, 33, 37, 37), "Dorf/Haus 3": (16, 40, 20, 44), "Dorf/Kapelle": (34, 43, 40, 49)}

    for actor in level_actors:
        if not isinstance(actor, unreal.StaticMeshActor) or actor.get_actor_label() != "Mauer":
            continue

        house = str(actor.get_folder_path())
        if house not in houses or unreal.Name(HIDDEN_TAG) in actor.get_editor_property("tags"):
            continue

        x0, y0, x1, y1 = houses[house]
        tile = tile_of(actor)
        along_x = int(tile[1]) in (y0, y1)
        chapel = house == "Dorf/Kapelle"
        height = actor.get_actor_scale3d().z * 100.0

        # Rows of the same wall piece, each row a little off, so the stones keep one size whatever the wall's height
        size = 1.12 if chapel else 0.88
        row_height = 93.0 * size * 0.86
        rows = max(1, int(round(height / row_height)))
        if rows > 1 and local.random() < 0.3:
            rows -= 1
        for row in range(rows):
            yaw = (0.0 if along_x else 90.0) + local.choice((0.0, 180.0)) + local.uniform(-3.0, 3.0)
            spot = (tile[0] + local.uniform(-0.05, 0.05), tile[1] + local.uniform(-0.05, 0.05))
            place("SM_MossyStoneWallC_02", spot, house + "/Kulisse", "Bruchsteinmauer", scale=size, stretch=(1.0, 1.45, 1.0), yaw=yaw, z=row * row_height, sink=5.0 if row == 0 else 0.0)
        hide_placeholder(actor)

    # Inside the houses: fallen beams, rubble and what was left behind
    left_behind = [("SM_WoodenBarrelB", 1.0), ("SM_WoodenWheelA", 1.0), ("SM_OldWoodenChest", 1.0), ("SM_WoodenBox", 1.3), ("SM_OldWoodenBucketB", 1.0), ("SM_MedievalButterChurn", 1.0), ("SM_OldWoodenTrough", 0.8)]
    for house, (x0, y0, x1, y1) in houses.items():
        folder = house + "/Kulisse/Innen"
        for beam in range(3 if house == "Dorf/Kapelle" else 2):
            spot = (local.uniform(x0 + 1.4, x1 - 0.4), local.uniform(y0 + 1.4, y1 - 0.4))
            place(local.choice(("SM_WornWoodenBeamA_00", "SM_WornWoodenBeamA_01", "SM_WornWoodenBeamB")), spot, folder, "Gefallener Balken", scale=1.2, yaw=local.uniform(0.0, 360.0), pitch=90.0 + local.uniform(-4.0, 4.0), z=16.0, grounded=False)
        for rubble in range(9):
            edge = local.choice(("N", "S", "W", "E"))
            x = local.uniform(x0 + 1.1, x1 - 0.1) if edge in ("N", "S") else (x0 + 1.25 if edge == "W" else x1 - 0.25)
            y = local.uniform(y0 + 1.1, y1 - 0.1) if edge in ("W", "E") else (y0 + 1.25 if edge == "N" else y1 - 0.25)
            place("SM_MossyStonesPack_0{}".format(local.randint(0, 3)), (x, y), folder, "Schutt", scale=local.uniform(2.2, 3.6), yaw=local.uniform(0.0, 360.0), sink=4.0)
        if house != "Dorf/Kapelle":
            for thing in range(2):
                name, scale = local.choice(left_behind)
                place(name, (local.uniform(x0 + 1.5, x1 - 0.5), local.uniform(y0 + 1.5, y1 - 0.5)), folder, "Hausrat", scale=scale, yaw=local.uniform(0.0, 360.0))

    # Chapel: a stone dais at the east wall with candles
    place("SM_JapaneseShrineStoneFloorC_00", (39.1, 46.5), "Dorf/Kapelle/Kulisse/Innen", "Altarstufe", yaw=0.0)
    place("SM_GraveB", (39.3, 46.5), "Dorf/Kapelle/Kulisse/Innen", "Altarstein", z=10.0, yaw=0.0)
    for candle in range(7):
        place("SM_Candles_0{}".format(candle % 3 + 1), (39.0 + local.uniform(-0.45, 0.45), 46.5 + local.uniform(-0.75, 0.75)), "Dorf/Kapelle/Kulisse/Innen", "Kerzen", scale=2.4, z=12.0, yaw=local.uniform(0.0, 360.0))

    # The well in the square; its parts share one origin, so none of them is dropped to the ground
    for actor in level_actors:
        if isinstance(actor, unreal.StaticMeshActor) and actor.get_actor_label() == "Brunnen" and str(actor.get_folder_path()) == "Dorf":
            for part in ("SM_WellBase", "SM_MainWell", "SM_WellDetailPieces", "SM_WellShingles"):
                place(part, tile_of(actor), "Dorf/Kulisse/Brunnen", part.replace("SM_", ""), yaw=35.0, grounded=False)
            hide_placeholder(actor)

    # A broken garden fence west of the third house, a hitching post by the square
    for index, name in enumerate(("SM_Fenc01_P1", "SM_Fence01_Dmg", "SM_Fence02_P2")):
        place(name, (14.9, 40.9 + index * 1.95), "Dorf/Kulisse/Zaun", "Gartenzaun", yaw=90.0 + local.uniform(-4.0, 4.0), roll=local.uniform(-5.0, 5.0))
    place("SM_Fence01_End", (15.6, 39.9), "Dorf/Kulisse/Zaun", "Zaunende", yaw=local.uniform(-6.0, 6.0))
    place("SM_HorseHitchingPost", (27.4, 38.4), "Dorf/Kulisse", "Anbindebalken", yaw=12.0)
    place("SM_WoodenWheelbarrow", (31.3, 38.2), "Dorf/Kulisse", "Schubkarre", yaw=200.0, roll=8.0)


def dress_pond():
    """Shore of the pond: mossy banks on the far side, stones and dry reeds all around, candles at the shrine"""
    local = random.Random(288)
    pond = (13.0, 28.0)
    folder = "Teich/Kulisse"

    # Banks on the west and north shore, where nobody arrives from
    banks = ["SM_MossyEmbankmentA_00", "SM_MossyEmbankmentA_01", "SM_MossyEmbankmentB", "SM_MossyEmbankmentC", "SM_MossyEmbankmentE"]
    angle = 150.0
    while angle < 300.0:
        place(local.choice(banks), ring(pond, angle, 6.05 + local.uniform(-0.15, 0.2)), folder + "/Ufer", "Uferwall", scale=local.uniform(1.0, 1.25), yaw=world_yaw(angle) + 90.0 + local.uniform(-12.0, 12.0), sink=10.0)
        angle += local.uniform(24.0, 32.0)

    stones = ["SM_MossyRocksA", "SM_MossyRocksB", "SM_MossyRock", "SM_IcelandicMossyRock", "SM_MossyStonesPack_01", "SM_HovsBeachRock"]
    for stone in range(22):
        name = local.choice(stones)
        scale = local.uniform(2.5, 4.0) if "StonesPack" in name else local.uniform(0.6, 1.1)
        place(name, ring(pond, local.uniform(0.0, 360.0), local.uniform(4.9, 6.1)), folder + "/Steine", "Uferstein", scale=scale, yaw=local.uniform(0.0, 360.0), sink=6.0)

    # Reeds: tall dry stalks, in clumps, some of them standing in the shallow water
    reeds = ["SM_DryPlantSet01_V09", "SM_DryPlantSet01_V10", "SM_DryPlantSet01_V11", "SM_DryPlantSet01_V01", "SM_ThatchingGrass00_V10", "SM_ThatchingGrass00_V2", "SM_DryPlant00_V1"]
    for clump in range(34):
        angle, radius = local.uniform(0.0, 360.0), local.uniform(4.2, 5.9)
        center = ring(pond, angle, radius)
        for stalk in range(local.randint(3, 6)):
            place(local.choice(reeds), (center[0] + local.uniform(-0.35, 0.35), center[1] + local.uniform(-0.35, 0.35)), folder + "/Schilf", "Schilf", scale=local.uniform(1.1, 1.7), yaw=local.uniform(0.0, 360.0), z=3.0, sink=3.0)

    # Dead wood in the water
    place("SM_MossyLog", ring(pond, 35.0, 4.4), folder + "/Totholz", "Stamm im Wasser", yaw=local.uniform(0.0, 360.0), sink=45.0)
    place("SM_MossyLog", ring(pond, 215.0, 4.7), folder + "/Totholz", "Stamm im Wasser", scale=0.85, yaw=local.uniform(0.0, 360.0), sink=40.0)
    place("SM_FallenFirTree", ring(pond, 110.0, 5.0), folder + "/Totholz", "Gestuerzte Tanne", yaw=world_yaw(110.0) + 20.0, sink=8.0)

    # Shrine: stone slabs around it, candles at its foot and on top
    shrine = (7.0, 23.0)
    for index, (dx, dy) in enumerate(((-0.95, -0.95), (0.95, -0.95), (-0.95, 0.95), (0.95, 0.95))):
        place("SM_JapaneseShrineStoneFloorA" if index % 2 == 0 else "SM_JapaneseShrineStoneFloorC_00", (shrine[0] + dx, shrine[1] + dy), folder + "/Schrein", "Steinplatte", yaw=local.choice((0.0, 90.0, 180.0, 270.0)) + local.uniform(-3.0, 3.0), sink=8.0)
    for candle in range(6):
        place("SM_Candles_0{}".format(candle % 3 + 1), (shrine[0] + local.uniform(-0.6, 0.6), shrine[1] + local.uniform(-0.6, 0.6)), folder + "/Schrein", "Kerzen auf dem Schrein", scale=2.4, z=120.0, yaw=local.uniform(0.0, 360.0))
    for candle in range(8):
        place("SM_Candles_0{}".format(candle % 3 + 1), ring(shrine, local.uniform(0.0, 360.0), local.uniform(1.0, 1.35)), folder + "/Schrein", "Kerzen am Schrein", scale=2.4, z=14.0, yaw=local.uniform(0.0, 360.0))


def dress_order():
    """Order of the Scar: a ring of standing stones around the fire bowl, candles at their feet. The tents stay blockout."""
    local = random.Random(905)
    bowl = (53.5, 41.5)
    folder = "Orden der Narbe/Kulisse"

    for stone in range(7):
        angle = stone * 360.0 / 7 + 12.0
        spot = ring(bowl, angle, 2.25)
        place("SM_Rock_03", spot, folder + "/Steinkreis", "Stehender Stein", scale=local.uniform(0.95, 1.2), stretch=(0.36, 0.38, 1.15),
              yaw=local.uniform(0.0, 360.0), pitch=local.uniform(-6.0, 6.0), roll=local.uniform(-6.0, 6.0), sink=25.0)
        for candle in range(3):
            place("SM_Candles_0{}".format(local.randint(1, 3)), ring(spot, local.uniform(0.0, 360.0), local.uniform(0.38, 0.55)), folder + "/Steinkreis", "Kerzen", scale=2.4, yaw=local.uniform(0.0, 360.0))

    # Offerings between the stones
    place("SM_PileOfChains", ring(bowl, 70.0, 1.3), folder, "Ketten", scale=1.4, yaw=40.0)
    place("SM_PileOfChains", ring(bowl, 250.0, 1.5), folder, "Ketten", scale=1.2, yaw=160.0)
    place("SM_Sword", ring(bowl, 160.0, 1.2), folder, "Schwert", yaw=75.0, z=2.0)
    place("SM_OldMetalPotA", ring(bowl, 320.0, 1.25), folder, "Topf", yaw=10.0)
    for coal in range(10):
        place(local.choice(("SM_CoalA", "SM_CoalB")), ring(bowl, local.uniform(0.0, 360.0), local.uniform(0.7, 1.0)), folder, "Kohle", scale=local.uniform(4.0, 7.0), yaw=local.uniform(0.0, 360.0))


def dress_dead_trees(taken):
    """Bare dead trees between the places: the dead broom bush of the plant pack, grown to the size of a tree"""
    local = random.Random(551)
    reserved = [(13, 28, 6.8), (45.5, 22.5, 3.4), (52, 11, 9.8), (10.5, 52.5, 7.6), (53.5, 41.5, 5.6), (30, 13, 2.2), (31.5, 15.5, 1.8), (29.5, 39.5, 2.4)]
    trees = ["SM_DeadBroomBush_V1", "SM_DeadBroomBush_V2", "SM_DeadBroomBush_V3", "SM_DeadBroomBush_V4"]

    placed, tries = 0, 0
    while placed < 48 and tries < 5000:
        tries += 1
        x, y = local.uniform(4, 60), local.uniform(4, 60)
        if road_distance(x, y) < 2.9 or any(math.hypot(x - cx, y - cy) < r for cx, cy, r in reserved):
            continue
        if any(x0 - 1.2 < x < x1 + 2.2 and y0 - 1.2 < y < y1 + 2.2 for x0, y0, x1, y1 in HOUSES):
            continue
        if any(math.hypot(x - ox, y - oy) < 1.8 for ox, oy in taken):
            continue
        place(local.choice(trees), (x, y), "Streuung/Kulisse/Tote Baeume", "Toter Baum", scale=local.uniform(3.6, 5.6), yaw=local.uniform(0.0, 360.0), roll=local.uniform(-5.0, 5.0), sink=10.0)
        taken.append((x, y))
        placed += 1


def dress_light(level_actors):
    """Dusk: a low, warm sun, thicker ash haze, a slightly muted picture and flickering fire light at the fires"""
    folder = "Licht/Kulisse"

    # Sun and haze are only moved while they still hold the values of BuildAschenmark.py; tuned ones stay
    for actor in level_actors:
        if isinstance(actor, unreal.DirectionalLight) and actor.get_actor_label() == "Sonne":
            if abs(actor.light_component.get_editor_property("intensity") - 4.0) < 0.01:
                actor.set_actor_rotation(unreal.Rotator(0.0, -24.0, -30.0), False)
                actor.light_component.set_editor_property("intensity", 3.2)
                actor.light_component.set_light_color(unreal.LinearColor(1.0, 0.66, 0.46, 1.0))
        elif isinstance(actor, unreal.ExponentialHeightFog) and actor.get_actor_label() == "Ascheschleier":
            if abs(actor.component.get_editor_property("fog_density") - 0.03) < 0.001:
                actor.component.set_editor_property("fog_density", 0.045)
                actor.component.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.20, 0.15, 0.12, 1.0))

    volume = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0.0, 0.0, 200.0), unreal.Rotator(0.0, 0.0, 0.0))
    volume.set_editor_property("unbound", True)
    settings = volume.get_editor_property("settings")
    settings.set_editor_property("override_auto_exposure_bias", True)
    settings.set_editor_property("auto_exposure_bias", -0.4)
    settings.set_editor_property("override_color_saturation", True)
    settings.set_editor_property("color_saturation", unreal.Vector4(0.84, 0.84, 0.84, 1.0))
    settings.set_editor_property("override_vignette_intensity", True)
    settings.set_editor_property("vignette_intensity", 0.5)
    volume.set_editor_property("settings", settings)
    volume.set_folder_path(folder)
    volume.set_actor_label("Stimmung Aschenmark")
    volume.set_editor_property("tags", [unreal.Name(DRESSING_TAG)])
    counts[folder] = counts.get(folder, 0) + 1

    # Fire light, unless some was placed by hand already
    if any(isinstance(actor, unreal.VaelFireLight) for actor in level_actors):
        return
    fires = [("Feuerschein Lagerfeuer", (10.5, 52.5), 130.0, 140.0), ("Feuerschein Feuerschale", (53.5, 41.5), 150.0, 110.0), ("Kerzenschein Kapelle", (39.0, 46.5), 70.0, 25.0),
             ("Kerzenschein Schrein", (7.0, 23.0), 170.0, 25.0), ("Laternenschein Edda", (13.3, 52.0), 120.0, 20.0), ("Laternenschein Tor", (15.1, 50.25), 130.0, 20.0)]
    for label, tile, height, intensity in fires:
        light = actors.spawn_actor_from_class(unreal.VaelFireLight, world(tile[0], tile[1], height), unreal.Rotator(0.0, 0.0, 0.0))
        light.set_editor_property("base_intensity", intensity)
        light.set_folder_path(folder)
        light.set_actor_label(label)
        light.set_editor_property("tags", [unreal.Name(DRESSING_TAG)])
        counts[folder] = counts.get(folder, 0) + 1

def fix_floating(level_actors):
    """Corrections to earlier passes: beams that hung in the air (their pivot is in the middle) and loose firewood"""
    stacked = 0
    for actor in level_actors:
        if unreal.Name(DRESSING_TAG) not in actor.get_editor_property("tags"):
            continue

        label = actor.get_actor_label()
        location = actor.get_actor_location()
        if label.startswith("Gefallener Balken") and location.z > 40.0:
            actor.set_actor_location(unreal.Vector(location.x, location.y, 16.0), False, False)
            counts["Korrektur: Balken"] = counts.get("Korrektur: Balken", 0) + 1
        elif label.startswith("Holzscheit") and str(actor.get_folder_path()) == "Lager/Kulisse/Holzplatz":
            actor.set_actor_location(unreal.Vector(location.x, location.y, location.z - max(0.0, location.z - 3.0) + (stacked // 3) * 14.0), False, False)
            stacked += 1


def dress_mark_source(instances):
    """Mark source: dark red ground, a crown of stone thorns leaning away from it, roots and dead trees around"""
    local = random.Random(166)
    source = (45.5, 22.5)
    folder = "Mark-Quelle/Kulisse"

    if "MI_Aschenmark_Mark" in instances:
        stain = actors.spawn_actor_from_class(unreal.StaticMeshActor, world(source[0], source[1], 1.6), unreal.Rotator(0.0, 0.0, 0.0))
        stain.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder"))
        stain.static_mesh_component.set_material(0, instances["MI_Aschenmark_Mark"])
        stain.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        stain.static_mesh_component.set_cast_shadow(False)
        stain.set_actor_scale3d(unreal.Vector(3.1 * 2.0 * TILE / 100.0, 3.1 * 2.0 * TILE / 100.0, 0.012))
        stain.set_folder_path(folder)
        stain.set_actor_label("Markgetraenkter Boden")
        stain.set_editor_property("tags", [unreal.Name(DRESSING_TAG)])
        counts[folder] = counts.get(folder, 0) + 1

    for thorn in range(10):
        angle = thorn * 36.0 + local.uniform(-9.0, 9.0)
        place(local.choice(("SM_Rock_03", "SM_Rock_07", "SM_Rock_06")), ring(source, angle, local.uniform(2.3, 2.9)), folder + "/Dornen", "Steindorn",
              scale=local.uniform(0.7, 1.15), stretch=(0.3, 0.34, 1.25), yaw=world_yaw(angle), pitch=-local.uniform(16.0, 30.0), sink=30.0)
    for root in range(7):
        angle = root * 360.0 / 7 + local.uniform(-12.0, 12.0)
        place(local.choice(("SM_ForestRootsA", "SM_ForestRootsB")), ring(source, angle, local.uniform(1.5, 2.0)), folder, "Wurzel", scale=local.uniform(0.9, 1.3), yaw=world_yaw(angle), sink=3.0)
    for tree in range(6):
        place(local.choice(("SM_DeadBroomBush_V1", "SM_DeadBroomBush_V2", "SM_DeadBroomBush_V3")), ring(source, tree * 60.0 + local.uniform(-15.0, 15.0), local.uniform(3.4, 4.1)), folder, "Verdorrter Baum",
              scale=local.uniform(3.2, 4.8), yaw=local.uniform(0.0, 360.0), roll=local.uniform(-12.0, 12.0), sink=10.0)


def dress_crater():
    """Crater of the ember queen: charred wood and coal along the inside of the rock ring, the middle stays free for the fight"""
    local = random.Random(377)
    crater = (52.0, 11.0)
    folder = "Krater/Kulisse/Verbranntes"
    gap = 135.0

    def inside(spot):
        return 3.0 < spot[0] < 61.0 and 3.0 < spot[1] < 61.0

    for coal in range(46):
        angle = local.uniform(0.0, 360.0)
        spot = ring(crater, angle, local.uniform(5.6, 7.9))
        if inside(spot):
            place(local.choice(("SM_CoalA", "SM_CoalB")), spot, folder, "Kohlebrocken", scale=local.uniform(6.0, 14.0), yaw=local.uniform(0.0, 360.0), sink=4.0)
    for stump in range(12):
        angle = local.uniform(0.0, 360.0)
        spot = ring(crater, angle, local.uniform(6.6, 7.8))
        if inside(spot) and abs((angle - gap + 180.0) % 360.0 - 180.0) > 28.0:
            place(local.choice(("SM_BrokenPineStump", "SM_WoodStump", "SM_BrokenTreeStump", "SM_FallenPineTree")), spot, folder, "Verkohlter Stumpf", scale=local.uniform(0.9, 1.4), yaw=local.uniform(0.0, 360.0), sink=6.0)
    for bush in range(14):
        spot = ring(crater, local.uniform(0.0, 360.0), local.uniform(6.2, 7.9))
        if inside(spot):
            place(local.choice(("SM_DeadBroomBush_V3", "SM_DeadBroomBush_V4", "SM_DeadBranches_02")), spot, folder, "Verbranntes Geaest", scale=local.uniform(1.4, 2.6), yaw=local.uniform(0.0, 360.0), sink=4.0)

    # Embers glowing along the edge
    for ember in range(4):
        spot = ring(crater, 45.0 + ember * 90.0, 6.4)
        if not inside(spot):
            continue
        light = actors.spawn_actor_from_class(unreal.VaelFireLight, world(spot[0], spot[1], 60.0), unreal.Rotator(0.0, 0.0, 0.0))
        light.set_editor_property("base_intensity", 45.0)
        light.set_folder_path("Krater/Kulisse/Glut")
        light.set_actor_label("Glutschein")
        light.set_editor_property("tags", [unreal.Name(DRESSING_TAG)])
        counts["Krater/Kulisse/Glut"] = counts.get("Krater/Kulisse/Glut", 0) + 1


def dress_border(level_actors):
    """A wall of big rocks instead of the four grey blocks around the map; the blocks stay as invisible collision"""
    local = random.Random(640)
    rocks = ["SM_Rock_0{}".format(index) for index in range(1, 8)]
    folder = "Rand/Kulisse"

    position = 0.5
    while position < MAP_TILES:
        for side in range(4):
            offset = 1.0 + local.uniform(-0.45, 0.35)
            spot = {0: (position, offset), 1: (position, MAP_TILES - offset), 2: (offset, position), 3: (MAP_TILES - offset, position)}[side]
            place(local.choice(rocks), spot, folder, "Randfels", scale=local.uniform(1.5, 2.3), yaw=local.uniform(0.0, 360.0), sink=45.0)
        position += local.uniform(2.2, 3.0)

    for actor in level_actors:
        if isinstance(actor, unreal.StaticMeshActor) and str(actor.get_folder_path()) == "Rand" and actor.get_actor_label().startswith("Rand "):
            hide_placeholder(actor)

KONTOR = (26.0, 57.0)
KONTOR_RADIUS = 4.2


def dress_kontor(level_actors):
    """Gildenkontor (step 11b package 2): a fortified trading post at the road out, south of the village, with Maren Holt"""
    folder = "Kontor/Kulisse"
    road_start, road_end = (30.0, 41.5), (KONTOR[0] + 0.6, KONTOR[1] - KONTOR_RADIUS)

    def near_road(tile, margin):
        dx, dy = road_end[0] - road_start[0], road_end[1] - road_start[1]
        t = max(0.0, min(1.0, ((tile[0] - road_start[0]) * dx + (tile[1] - road_start[1]) * dy) / (dx * dx + dy * dy)))
        return math.hypot(tile[0] - (road_start[0] + dx * t), tile[1] - (road_start[1] + dy * t)) < margin

    # Scattered rocks, trees and dead wood in the way are hidden, not deleted; blockout shapes there lose their collision too
    for actor in level_actors:
        if not isinstance(actor, unreal.StaticMeshActor) or not str(actor.get_folder_path()).startswith("Streuung"):
            continue
        tile = tile_of(actor)
        if math.hypot(tile[0] - KONTOR[0], tile[1] - KONTOR[1]) > KONTOR_RADIUS + 1.5 and not near_road(tile, 2.0):
            continue
        if unreal.Name(HIDDEN_TAG) not in actor.get_editor_property("tags"):
            hide_placeholder(actor)
        actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        counts["Kontor: versteckt"] = counts.get("Kontor: versteckt", 0) + 1

    materials = {}
    for actor in level_actors:
        if isinstance(actor, unreal.StaticMeshActor):
            label = actor.get_actor_label()
            for key, wanted in (("road", "Weg 1"), ("floor", "Boden"), ("wall", "Mauer")):
                if key not in materials and label == wanted:
                    materials[key] = actor.static_mesh_component.get_material(0)

    def cube(tile, size_x, size_y, height, label, base=0.0, yaw=0.0, material="wall", collision=True):
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, world(tile[0], tile[1], base + height * 0.5), unreal.Rotator(0.0, 0.0, yaw))
        actor.static_mesh_component.set_static_mesh(CUBE)
        if materials.get(material) is not None:
            actor.static_mesh_component.set_material(0, materials[material])
        if not collision:
            actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        actor.set_actor_scale3d(unreal.Vector(size_x * TILE / 100.0, size_y * TILE / 100.0, height / 100.0))
        actor.set_folder_path(folder)
        actor.set_actor_label(label)
        actor.set_editor_property("tags", [unreal.Name(DRESSING_TAG)])
        counts[folder] = counts.get(folder, 0) + 1
        return actor

    # Road from the village well down to the gate, and trodden ground inside
    length = math.hypot(road_end[0] - road_start[0], road_end[1] - road_start[1])
    yaw = math.degrees(math.atan2(-(road_end[1] - road_start[1]), -(road_end[0] - road_start[0])))
    cube(((road_start[0] + road_end[0]) * 0.5, (road_start[1] + road_end[1]) * 0.5), length + 1.0, 2.6, 2.0, "Weg zum Kontor", base=-0.5, yaw=yaw, material="road", collision=False)
    cube(KONTOR, KONTOR_RADIUS * 1.8, KONTOR_RADIUS * 1.8, 2.0, "Boden", base=-0.8, material="floor", collision=False)

    # Palisade on a square, the gate towards the road in the north
    gate_angle = math.degrees(math.atan2(road_start[1] - KONTOR[1], road_start[0] - KONTOR[0]))
    gate_half = 16.0
    if mesh("SM_WoodenPost") is not None:
        angle = gate_angle + gate_half
        index = 0
        while angle < gate_angle + 360.0 - gate_half:
            place("SM_WoodenPost", ring(KONTOR, angle, KONTOR_RADIUS + rng.uniform(-0.04, 0.04)), folder + "/Palisade", "Pfahl {}".format(index),
                  scale=2.4 * rng.uniform(0.95, 1.08), stretch=(1.05, 1.05, 1.0), yaw=rng.uniform(0.0, 360.0), sink=18.0)
            angle += math.degrees(0.42 / KONTOR_RADIUS)
            index += 1
        for side in (-1.0, 1.0):
            place("SM_WoodenPost", ring(KONTOR, gate_angle + side * gate_half, KONTOR_RADIUS), folder + "/Palisade", "Torpfosten", scale=3.2, sink=25.0)
    segments = 22
    span = (360.0 - 2.0 * gate_half) / segments
    chord = 2.0 * KONTOR_RADIUS * TILE * math.sin(math.radians(span * 0.5)) + 12.0
    for segment in range(segments):
        middle = gate_angle + gate_half + span * (segment + 0.5)
        blocker(ring(KONTOR, middle, KONTOR_RADIUS), chord, 45.0, 320.0, world_yaw(middle) + 90.0, folder + "/Palisade/Kollision", "Palisadenwand {}".format(segment))

    # Store house at the back (placeholder walls until a guild building is chosen), a counter in front of it
    back = ring(KONTOR, gate_angle + 180.0, 2.2)
    cube(back, 3.6, 2.2, 260.0, "Lagerhaus", yaw=world_yaw(gate_angle) + 90.0)
    cube(back, 4.0, 2.6, 30.0, "Lagerhaus Dach", base=260.0, yaw=world_yaw(gate_angle) + 90.0, material="road", collision=False)
    counter = ring(KONTOR, gate_angle + 180.0, 0.6)
    place("SM_OldWoodenTable", counter, folder, "Theke", scale=1.2, yaw=world_yaw(gate_angle) + 90.0)
    place("SM_ManMade_Lantern_02", counter, folder, "Laterne auf der Theke", z=92.0, yaw=30.0)
    place("SM_WoodenBox", ring(KONTOR, gate_angle + 130.0, 2.6), folder, "Kiste", scale=1.4, yaw=20.0)
    place("SM_WoodenBox", ring(KONTOR, gate_angle + 130.0, 2.6), folder, "Kiste oben", z=70.0, scale=1.1, yaw=55.0)
    place("SM_WoodenAmmoCrate", ring(KONTOR, gate_angle + 115.0, 3.2), folder, "Lange Kiste", yaw=world_yaw(gate_angle + 115.0))
    place("SM_WoodenBarrelB", ring(KONTOR, gate_angle - 125.0, 2.8), folder, "Fass 1", yaw=10.0)
    place("SM_WoodenBarrelC", ring(KONTOR, gate_angle - 140.0, 2.9), folder, "Fass 2", yaw=80.0)
    place("SM_WoodenWheelbarrow", ring(KONTOR, gate_angle - 90.0, 3.0), folder, "Schubkarre", yaw=world_yaw(gate_angle))
    place("SM_HorseHitchingPost", ring(KONTOR, gate_angle + 60.0, 3.3), folder, "Anbindebalken", yaw=world_yaw(gate_angle + 60.0) + 90.0)

    light = actors.spawn_actor_from_class(unreal.VaelFireLight, world(counter[0], counter[1], 140.0), unreal.Rotator(0.0, 0.0, 0.0))
    light.set_folder_path(folder)
    light.set_actor_label("Licht der Theke")

    # Maren behind the counter, facing the gate
    behind = ring(KONTOR, gate_angle + 180.0, 1.15)
    maren = actors.spawn_actor_from_class(unreal.VaelNpc, world(behind[0], behind[1], 92.0), unreal.Rotator(0.0, 0.0, world_yaw(gate_angle)))
    maren.set_editor_property("dialogue", unreal.load_asset("/Game/Vael/Story/DA_Dialogue_Maren"))
    maren.set_editor_property("robe_color", unreal.LinearColor(0.07, 0.1, 0.16, 1.0))
    maren.set_folder_path("Kontor")
    maren.set_actor_label("Maren Holt")

    marker = actors.spawn_actor_from_class(unreal.VaelQuestMarker, world(KONTOR[0], KONTOR[1], 50.0), unreal.Rotator(0.0, 0.0, 0.0))
    marker.set_editor_property("marker_id", unreal.Name("Kontor"))
    marker.set_editor_property("radius", 650.0)
    marker.set_folder_path("Quests")
    marker.set_actor_label("Questziel Kontor")


def place_scrolls():
    """Scrolls of the sealed formulas Act I gained with the spell list (step 10b); the places match the hints of the formulas"""
    element = unreal.VaelElement
    scrolls = [((46.0, 17.2), [element.FIRE, element.FIRE, element.EARTH], "Schriftrolle am Kratereingang"),
               ((52.2, 41.5), [element.FIRE, element.FIRE, element.AIR], "Schriftrolle im Steinkreis"),
               ((7.6, 28.3), [element.AIR, element.AIR, element.WATER], "Schriftrolle am Westufer"),
               ((36.9, 52.1), [element.FIRE, element.EARTH, element.AIR], "Schriftrolle bei den Graebern")]
    for tile, elements, label in scrolls:
        scroll = actors.spawn_actor_from_class(unreal.VaelFormulaScroll, world(tile[0], tile[1], 40.0), unreal.Rotator(0.0, 0.0, 0.0))
        scroll.set_editor_property("elements", elements)
        scroll.set_editor_property("label", unreal.Text(label))
        scroll.set_folder_path("Schriftrollen")
        scroll.set_actor_label(label)
        counts["Schriftrollen"] = counts.get("Schriftrollen", 0) + 1

# ---------------------------------------------------------------- Run

def dress():
    world_object = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    if world_object is None:
        unreal.log_error("Vael: could not load {}".format(LEVEL_PATH))
        return

    level_actors = list(actors.get_all_level_actors())
    folders = set(str(actor.get_folder_path()) for actor in level_actors)

    def done(folder):
        """A place is dressed once; whoever wants it anew deletes its Kulisse folder in the editor first"""
        return any(existing == folder or existing.startswith(folder + "/") for existing in folders)

    load_meshes()

    # Tiles the blockout already uses for rocks and trees, so new things keep their distance
    taken = [tile_of(actor) for actor in level_actors if isinstance(actor, unreal.StaticMeshActor) and actor.get_actor_label() in ("Fels", "Toter Baum")]

    instances = {}
    try:
        instances = make_ground_materials()
        instances["Wasser"] = make_water_material()
        lay_ground(level_actors, instances)
    except Exception as error:
        notes.append("ground materials failed: {}".format(error))

    if not done("Lager/Kulisse"):
        dress_camp()
        dress_crates(level_actors)
    dress_rocks(level_actors)
    dress_trees(level_actors)
    if not done("Dorf/Kapelle/Kulisse"):
        dress_graveyard(taken)
    if not done("Streuung/Kulisse/Totholz"):
        dress_fields(taken)
    if not done("Dorf/Kulisse"):
        dress_village(level_actors)
    if not done("Teich/Kulisse"):
        dress_pond()
    if not done("Orden der Narbe/Kulisse"):
        dress_order()
    if not done("Streuung/Kulisse/Tote Baeume"):
        dress_dead_trees(taken)
    if not done("Licht/Kulisse"):
        dress_light(level_actors)
    fix_floating(level_actors)
    if not done("Mark-Quelle/Kulisse"):
        dress_mark_source(instances)
    if not done("Krater/Kulisse/Verbranntes"):
        dress_crater()
    if not done("Rand/Kulisse"):
        dress_border(level_actors)
    if not done("Schriftrollen"):
        place_scrolls()
    if not done("Kontor/Kulisse"):
        dress_kontor(level_actors)

    saved = unreal.EditorLoadingAndSavingUtils.save_map(world_object, LEVEL_PATH)

    lines = ["saved: {}".format(saved)]
    lines += ["{:<40} {}".format(folder, count) for folder, count in sorted(counts.items())]
    lines += ["total new actors: {}".format(sum(count for folder, count in counts.items() if not folder.startswith("Boden: ")))]
    lines += ["missing: {}".format(", ".join(notes) if notes else "nothing")]
    with open(os.path.join(unreal.Paths.project_saved_dir(), "DressLog.txt"), "w", encoding="utf-8") as file:
        file.write("\n".join(lines))

    if saved:
        unreal.log("Vael: dressed {}, see Saved/DressLog.txt".format(LEVEL_PATH))
    else:
        unreal.log_error("Vael: could not save {}".format(LEVEL_PATH))


dress()
