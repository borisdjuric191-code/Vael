# Builds the open world of Vael, L_Vael (World Partition), with region I, the Aschenmark, as approved by Boris on 2 November 2026
# (zone profile Docs/Karte/aschenmark_zonen.svg). Step 1: the land and the bones of the region.
#
# - Landscape from Scripts/Data/Welt/AM_Hoehen.r16 (Tools/Welt/aschenmark_welt.py), 2016 m north x 2520 m east, 1 m per quad,
#   streamed in cells of 2 components; ground material M_AM_Boden: ash, road and clearing masks, a tint per zone, dark rock on
#   steep slopes, embers glowing in the ground of the Glutfelder.
# - Light, sky and haze; the region Aschenmark over the whole land; walls along the coast.
# - The three camps (safe place, fire, tents), the places of the zones as blockout, 13 clearings with their creatures, the queen
#   in her crater, and route points along the road of the campaign for the stream test (-VaelStreamTest).
# - World: X runs north, Y east, the landscape starts at 0/0. Heights come from AM_Orte.json.
#
# Only creates new assets. Refuses if the level exists, unless VAEL_WELT_NEU=1 is set: then it first deletes the level this
# script made (only L_Vael and its actor files), nothing else.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildVaelWelt.py"

import json
import math
import os
import random

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Vael"
WORLD_FOLDER = "/Game/Vael/Welt"
DRESSING_FOLDER = "/Game/Vael/Maps/Kulisse"
BLOCKOUT_FOLDER = "/Game/Vael/Maps/Blockout"
DATA = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", "Welt")
M = 100.0
SIZE_X, SIZE_Y = 2017, 2521
KIT_VERSION = 1.1

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")
CONE = unreal.load_asset("/Engine/BasicShapes/Cone.Cone")
SHAPE_MATERIAL = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")

assets = unreal.EditorAssetLibrary
library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(1102)


def log(message):
    unreal.log_warning("[BuildVaelWelt] " + message)


def v(x, y, z=0.0):
    return unreal.Vector(x * M, y * M, z * M)


def at(p, lift=0.0):
    return v(p["x"], p["y"], p["z"] + lift)


def srgb(hex_code):
    def lin(c):
        value = int(c, 16) / 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4
    return unreal.LinearColor(lin(hex_code[0:2]), lin(hex_code[2:4]), lin(hex_code[4:6]), 1.0)


def blockout_material(name, hex_code):
    path = "{}/{}".format(BLOCKOUT_FOLDER, name)
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    instance = tools.create_asset(name, BLOCKOUT_FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(instance, SHAPE_MATERIAL)
    library.set_material_instance_vector_parameter_value(instance, "Color", srgb(hex_code))
    assets.save_loaded_asset(instance, False)
    return instance


# ---------------------------------------------------------------- Ground material

class Graph:
    def __init__(self, material):
        self.material = material
        self.row = 0

    def node(self, cls, **props):
        self.row += 1
        e = library.create_material_expression(self.material, cls, -900 + 300 * (self.row % 3), 120 * self.row)
        for k, val in props.items():
            e.set_editor_property(k, val)
        return e

    def link(self, a, b, b_input="", a_output=""):
        if not library.connect_material_expressions(a, a_output, b, b_input):
            log("link failed {} -> {}".format(a.get_name(), b_input))

    def const(self, value):
        return self.node(unreal.MaterialExpressionConstant, r=value)

    def mask(self, source, channels, output=""):
        e = self.node(unreal.MaterialExpressionComponentMask, r="R" in channels, g="G" in channels, b="B" in channels, a="A" in channels)
        self.link(source, e, "", output)
        return e

    def sample(self, texture, uv, sampler=None):
        e = self.node(unreal.MaterialExpressionTextureSample, texture=texture)
        if sampler is not None:
            e.set_editor_property("sampler_type", sampler)
        self.link(uv, e, "UVs")
        return e

    def custom(self, code, output_type, inputs):
        e = self.node(unreal.MaterialExpressionCustom, code=code, output_type=output_type)
        entries = []
        for name in inputs:
            entry = unreal.CustomInput()
            entry.set_editor_property("input_name", name)
            entries.append(entry)
        e.set_editor_property("inputs", entries)
        for name, source in inputs.items():
            self.link(source, e, name)
        return e


GROUND_COLOR = """float3 Kessel = float3(1.0, 0.86, 0.72), Schlacke = float3(0.5, 0.52, 0.58), Glut = float3(0.95, 0.6, 0.48), Pass = float3(0.78, 0.77, 0.75);
float3 Tint = Zone < 0.333 ? lerp(Kessel, Schlacke, Zone * 3.0) : (Zone < 0.667 ? lerp(Schlacke, Glut, Zone * 3.0 - 1.0) : lerp(Glut, Pass, Zone * 3.0 - 2.0));
float Cliff = saturate((0.82 - Up) * 4.0);
float3 Ground = lerp(Ash * Tint, Road * float3(0.95, 0.9, 0.82), Way);
Ground = lerp(Ground, Ground * 1.12, Clear * (1.0 - Way));
return lerp(Ground, Ash * float3(0.32, 0.3, 0.3), Cliff);"""
GROUND_GLOW = """float GlutShare = saturate(1.0 - abs(Zone - 0.667) * 6.0) * (1.0 - Way) * (1.0 - Clear * 0.6);
return float3(1.0, 0.33, 0.07) * pow(saturate(Noise), 9.0) * 4.0 * GlutShare;"""


def ground_material():
    """M_AM_Boden: the ash and road textures of the Aschenmark (from its dressing materials), laid out by T_AM_Wege"""
    path = WORLD_FOLDER + "/M_AM_Boden"
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        if library.get_material_default_scalar_parameter_value(material, "KitVersion") >= KIT_VERSION - 0.001:
            return material
        library.delete_all_material_expressions(material)
        for expression in list(unreal.ObjectIterator(unreal.MaterialExpression)):
            if expression.get_outer() == material:
                library.delete_material_expression(material, expression)
    else:
        material = tools.create_asset("M_AM_Boden", WORLD_FOLDER, unreal.Material, unreal.MaterialFactoryNew())

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(DATA, "T_AM_Wege.png"))
    task.set_editor_property("destination_path", WORLD_FOLDER)
    task.set_editor_property("destination_name", "T_AM_Wege")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    masks = unreal.load_asset(WORLD_FOLDER + "/T_AM_Wege")
    masks.set_editor_property("srgb", False)
    masks.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    masks.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    masks.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    masks.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    assets.save_loaded_asset(masks, False)

    ash_source = unreal.load_asset(DRESSING_FOLDER + "/MI_Aschenmark_Boden")
    road_source = unreal.load_asset(DRESSING_FOLDER + "/MI_Aschenmark_Weg")
    noise = unreal.load_asset("/Game/Vael/Effects/Feuer/T_Feuer_Rauschen")

    def tex(source, key):
        return library.get_material_instance_texture_parameter_value(source, key) if source is not None else None

    g = Graph(material)
    g.node(unreal.MaterialExpressionScalarParameter, parameter_name="KitVersion", default_value=KIT_VERSION)
    world = g.node(unreal.MaterialExpressionWorldPosition)
    xy = g.mask(world, "RG")
    tile = g.node(unreal.MaterialExpressionDivide)
    g.link(xy, tile, "A")
    g.link(g.node(unreal.MaterialExpressionScalarParameter, parameter_name="TileSize", default_value=420.0), tile, "B")

    # Masks over the whole land: u runs east (world Y), v north (world X)
    # (with nodes: the world position is a large-world value a custom node can't take)
    mask_u, mask_v = g.node(unreal.MaterialExpressionDivide), g.node(unreal.MaterialExpressionDivide)
    g.link(g.mask(world, "G"), mask_u, "A")
    g.link(g.const((SIZE_Y - 1) * M), mask_u, "B")
    g.link(g.mask(world, "R"), mask_v, "A")
    g.link(g.const((SIZE_X - 1) * M), mask_v, "B")
    mask_uv = g.node(unreal.MaterialExpressionAppendVector)
    g.link(mask_u, mask_uv, "A")
    g.link(mask_v, mask_uv, "B")
    masks_sample = g.sample(masks, mask_uv, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    way, clear, zone = g.mask(masks_sample, "R"), g.mask(masks_sample, "G"), g.mask(masks_sample, "B")

    ash = g.sample(tex(ash_source, "Albedo"), tile)
    road = g.sample(tex(road_source, "Albedo") or tex(ash_source, "Albedo"), tile)
    up = g.mask(g.node(unreal.MaterialExpressionVertexNormalWS), "B")
    color = g.custom(GROUND_COLOR, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                     {"Ash": g.mask(ash, "RGB"), "Road": g.mask(road, "RGB"), "Way": way, "Clear": clear, "Zone": zone, "Up": up})
    library.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    normal_tex = tex(ash_source, "Normal")
    if normal_tex is not None:
        normal = g.sample(normal_tex, tile, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        library.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(g.const(0.85), "", unreal.MaterialProperty.MP_ROUGHNESS)

    if noise is not None:
        noise_uv = g.node(unreal.MaterialExpressionDivide)
        g.link(xy, noise_uv, "A")
        g.link(g.const(1800.0), noise_uv, "B")
        embers = g.mask(g.sample(noise, noise_uv, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS), "R")
        glow = g.custom(GROUND_GLOW, unreal.CustomMaterialOutputType.CMOT_FLOAT3, {"Noise": embers, "Zone": zone, "Way": way, "Clear": clear})
        library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


# ---------------------------------------------------------------- Actors

def spawn(actor_class, location, folder, label, yaw=0.0, streamed=True):
    actor = actors.spawn_actor_from_class(actor_class, location, unreal.Rotator(0.0, 0.0, yaw))
    actor.set_folder_path(folder)
    actor.set_actor_label(label)
    if not streamed:
        actor.set_editor_property("is_spatially_loaded", False)
    return actor


def shape(mesh, material, location, size, folder, label, yaw=0.0, collision=True):
    """Engine shape, size in metres, standing on location"""
    actor = spawn(unreal.StaticMeshActor, location + unreal.Vector(0.0, 0.0, size[2] * 0.5 * M), folder, label, yaw)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.static_mesh_component.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(*size))
    if not collision:
        actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return actor


def wall(a, b, folder, label):
    """Invisible wall between two points, 14 m high, from their ground up"""
    length = math.hypot(b["x"] - a["x"], b["y"] - a["y"]) / 1.0
    yaw = math.degrees(math.atan2(b["y"] - a["y"], b["x"] - a["x"]))
    z = min(a["z"], b["z"]) - 4.0
    actor = spawn(unreal.StaticMeshActor, v((a["x"] + b["x"]) * 0.5, (a["y"] + b["y"]) * 0.5, z + 9.0), folder, label, yaw)
    actor.static_mesh_component.set_static_mesh(CUBE)
    actor.static_mesh_component.set_editor_property("visible", False)
    actor.set_actor_hidden_in_game(True)
    actor.set_actor_scale3d(unreal.Vector(length + 2.0, 3.0, 18.0))
    return actor


def spawner(kind, p, count, folder, label, spread=180.0):
    actor = spawn(unreal.VaelCreatureSpawner, at(p, 1.0), folder, label)
    actor.set_editor_property("creature_kind", kind.static_class())
    actor.set_editor_property("count", count)
    actor.set_editor_property("spread", spread)
    return actor


def camp(p, name, mat, start=False):
    folder = "Lager/" + name
    if start:
        spawn(unreal.PlayerStart, at(p, 1.0), folder, "Spielerstart", yaw=45.0)
    zone = spawn(unreal.VaelSafeZone, at(p), folder, "Sicherer Ort")
    zone.set_editor_property("radius", 1600.0)
    spawn(unreal.VaelFireLight, at(p, 1.0), folder, "Lagerfeuer-Licht")
    shape(CYLINDER, mat["stone"], at(p, -0.2), (1.6, 1.6, 0.4), folder, "Feuerstelle", collision=False)
    for index in range(4):
        angle = index * 90.0 + rng.uniform(-20.0, 20.0) + 30.0
        d = rng.uniform(7.0, 10.0)
        tent = {"x": p["x"] + math.cos(math.radians(angle)) * d, "y": p["y"] + math.sin(math.radians(angle)) * d, "z": p["z"]}
        shape(CONE, mat["tent"], at(tent, -0.3), (4.0, 4.0, 3.0), folder, "Zelt {}".format(index + 1), yaw=angle)


def build():
    if assets.does_asset_exist(LEVEL_PATH):
        if os.environ.get("VAEL_WELT_NEU") != "1":
            log("{} exists already, nothing was changed (VAEL_WELT_NEU=1 builds it anew)".format(LEVEL_PATH))
            return
        assets.delete_asset(LEVEL_PATH)
        external = "/Game/__ExternalActors__/Vael/Maps/L_Vael"
        if assets.does_directory_exist(external):
            assets.delete_directory(external)
        log("deleted the old " + LEVEL_PATH)

    with open(os.path.join(DATA, "AM_Orte.json"), encoding="utf-8") as f:
        data = json.load(f)

    assets.make_directory(WORLD_FOLDER)
    ground = ground_material()

    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_editor.new_level(LEVEL_PATH, True):
        log("could not create the open world level")
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

    land = unreal.VaelWorldBuilder.create_landscape_from_raw(world, os.path.join(DATA, "AM_Hoehen.r16"), SIZE_X, SIZE_Y,
                                                             unreal.Vector(0.0, 0.0, 0.0), unreal.Vector(100.0, 100.0, 100.0), ground, 2)
    if land is None:
        log("the landscape could not be made")
        return
    land.set_actor_label("Aschenmark Gelaende")

    mat = {
        "tent": blockout_material("MI_Blockout_Tent", "6b5a44"),
        "stone": blockout_material("MI_Blockout_Stone", "5a554f"),
        "ruin": blockout_material("MI_Blockout_Ruin", "6e675d"),
        "guild": blockout_material("MI_Blockout_Guild", "7a5a3a"),
        "order": blockout_material("MI_Blockout_Order", "4d4651"),
        "slag": blockout_material("MI_Blockout_Slag", "2e2c2f"),
        "water": blockout_material("MI_Blockout_Water", "2c4a5c"),
    }

    # Light, sky and haze, loaded everywhere; dusk but bright enough for the start
    centre = v(SIZE_X * 0.5, SIZE_Y * 0.5, 60.0)
    sun = spawn(unreal.DirectionalLight, centre, "Licht", "Sonne", streamed=False)
    sun.set_actor_rotation(unreal.Rotator(0.0, -38.0, -40.0), False)
    sun.light_component.set_editor_property("intensity", 4.0)
    sun.light_component.set_light_color(unreal.LinearColor(1.0, 0.82, 0.68, 1.0))
    sky = spawn(unreal.SkyLight, centre, "Licht", "Himmelslicht", streamed=False)
    sky.light_component.set_editor_property("real_time_capture", True)
    spawn(unreal.SkyAtmosphere, centre, "Licht", "Atmosphaere", streamed=False)
    fog = spawn(unreal.ExponentialHeightFog, centre, "Licht", "Dunst", streamed=False)
    fog.component.set_editor_property("fog_density", 0.02)
    fog.component.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.25, 0.2, 0.17, 1.0))

    # The region over the whole land: weather, corruption and the name in the HUD
    region = spawn(unreal.VaelRegion, centre, "Spiel", "Region Aschenmark", streamed=False)
    region.set_editor_property("region_name", unreal.Text("Aschenmark"))
    region.set_editor_property("covers_whole_level", True)
    region.set_editor_property("offering_crystals", 5)

    # Sea: a water-coloured plane below the coast, walls along the coast keep everyone on land
    shape(CUBE, mat["water"], v(SIZE_X * 0.5, SIZE_Y * 0.5, -1.2), (SIZE_X + 400.0, SIZE_Y + 400.0, 0.2), "Meer", "Meer", collision=False)
    for line in data["coast"]:
        for a, b in zip(line, line[1:]):
            wall(a, b, "Grenzen/Kueste", "Kuestenwand")

    camps = data["camps"]
    camp(camps["Lager der Überlebenden"], "Lager der Ueberlebenden", mat, start=True)
    camp(camps["Gildenkontor"], "Gildenkontor", mat)
    camp(camps["Passwache"], "Passwache", mat)

    # Places of the zones as blockout, to be dressed later
    places = data["places"]
    for index in range(7):
        p = places["Ruinen von Kesselgrund"]
        q = {"x": p["x"] + rng.uniform(-30, 30), "y": p["y"] + rng.uniform(-30, 30), "z": p["z"]}
        shape(CUBE, mat["ruin"], at(q, -0.5), (rng.uniform(5, 9), rng.uniform(5, 9), rng.uniform(2, 5)), "Orte/Kesselgrund/Ruinen", "Ruine {}".format(index + 1), yaw=rng.uniform(0, 90))
    shape(CYLINDER, mat["stone"], at(places["Ruinen von Kesselgrund"], -0.2), (3.0, 3.0, 1.2), "Orte/Kesselgrund/Ruinen", "Brunnen")
    shape(CYLINDER, mat["water"], at(places["Teich"], -0.6), (28.0, 22.0, 0.7), "Orte/Kesselgrund/Teich", "Teich", collision=False)
    p = places["Kathedrale des Ordens"]
    shape(CUBE, mat["order"], at(p, -0.5), (34.0, 14.0, 12.0), "Orte/Kesselgrund/Kathedrale", "Kirchenschiff", yaw=40.0)
    shape(CUBE, mat["order"], at({"x": p["x"] + 14, "y": p["y"] + 12, "z": p["z"]}, -0.5), (8.0, 8.0, 24.0), "Orte/Kesselgrund/Kathedrale", "Turm", yaw=40.0)
    spawn(unreal.VaelMarkSource, at(places["Nid-Quelle"], 1.0), "Orte/Kesselgrund/Kathedrale", "Nid-Quelle")

    p = places["Gildenspeicher"]
    shape(CUBE, mat["guild"], at(p, -0.5), (24.0, 14.0, 9.0), "Orte/Schlackenhaenge", "Gildenspeicher", yaw=10.0)
    for index in range(6):
        q = places["Obsidianbruch"]
        q = {"x": q["x"] + rng.uniform(-25, 25), "y": q["y"] + rng.uniform(-25, 25), "z": q["z"]}
        shape(CONE, mat["slag"], at(q, -1.0), (rng.uniform(3, 6), rng.uniform(3, 6), rng.uniform(4, 9)), "Orte/Schlackenhaenge/Obsidianbruch", "Obsidian {}".format(index + 1), yaw=rng.uniform(0, 360))
    shape(CUBE, mat["slag"], at(places["Schlackentor"], -1.0), (6.0, 26.0, 18.0), "Orte/Schlackenhaenge", "Schlackentor (verschuettet)", yaw=0.0)

    for index in range(5):
        q = places["Harpyienfelsen"]
        q = {"x": q["x"] + rng.uniform(-20, 20), "y": q["y"] + rng.uniform(-20, 20), "z": q["z"]}
        shape(CYLINDER, mat["stone"], at(q, -1.0), (rng.uniform(3, 5), rng.uniform(3, 5), rng.uniform(10, 18)), "Orte/Glutfelder/Harpyienfelsen", "Felsnadel {}".format(index + 1))
    shape(CUBE, mat["guild"], at({"x": places["Krater der Glutkönigin"]["x"] + 30, "y": places["Krater der Glutkönigin"]["y"] - 20, "z": places["Krater der Glutkönigin"]["z"]}, -0.5),
          (4.0, 4.0, 6.0), "Orte/Glutfelder/Krater", "Sondierschacht der Gilde")
    for index in range(4):
        q = places["Predigerlager"]
        q = {"x": q["x"] + rng.uniform(-12, 12), "y": q["y"] + rng.uniform(-12, 12), "z": q["z"]}
        shape(CONE, mat["order"], at(q, -0.3), (3.5, 3.5, 3.0), "Orte/Aschenpass/Predigerlager", "Zelt {}".format(index + 1))

    # Creatures on the clearings, by zone (my distribution; Boris checks it in play)
    c = data["clearings"]
    kinds = [
        (unreal.VaelVeilMonkeyData, 2, "Kesselgrund"), (unreal.VaelSickleDollData, 2, "Kesselgrund"), (unreal.VaelEmberCrawlerData, 3, "Kesselgrund"),
        (unreal.VaelEmberCrawlerData, 4, "Glutfelder"), (unreal.VaelEmberCrawlerData, 5, "Glutfelder"), (unreal.VaelAshHarpyData, 3, "Glutfelder"), (unreal.VaelHarpyElderData, 1, "Glutfelder"),
        (unreal.VaelAshRollerData, 2, "Schlackenhaenge"), (unreal.VaelAshRollerData, 3, "Schlackenhaenge"), (unreal.VaelEmberCrawlerData, 4, "Schlackenhaenge"), (unreal.VaelAshHarpyData, 3, "Schlackenhaenge"),
        (unreal.VaelPreacherData, 2, "Aschenpass"), (unreal.VaelAshHarpyData, 4, "Aschenpass"),
    ]
    for index, (kind, count, zone_name) in enumerate(kinds):
        spawner(kind, c[index], count, "Gegner/" + zone_name, "Lichtung {}".format(index + 1))
    spawner(unreal.VaelEmberQueenData, places["Krater der Glutkönigin"], 1, "Gegner/Glutfelder", "Glutkoenigin", spread=0.0)

    # Route along the road of the campaign for the stream test
    for number, p in enumerate(data["main_road"][::3], start=1):
        point = spawn(unreal.TargetPoint, at(p, 1.0), "Spiel/Route", "Route {}".format(number), streamed=False)
        point.set_editor_property("tags", [unreal.Name("VaelRoute"), unreal.Name(str(number))])

    if not unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH):
        log("could not save the level")
        return
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("built {}".format(LEVEL_PATH))


build()
