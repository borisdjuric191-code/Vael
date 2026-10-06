# Builds the level L_Aschenmark as a blockout of the browser prototype map.
#
# - Creates the new level /Game/Vael/Maps/L_Aschenmark and placeholder materials under /Game/Vael/Maps/Blockout.
# - Never overwrites: if the level exists already, nothing happens. Delete it in the editor first to build it anew.
# - Everything is sorted into folders of the World Outliner (Lager, Dorf, Teich ...), so it can be replaced piece by piece.
#
# Run with the editor closed, after Scripts/CreateStoryAssets.py and Scripts/CreateItemAssets.py:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="Scripts/BuildAschenmark.py" -EnablePlugins=PythonScriptPlugin
#
# The prototype map is 64 x 64 tiles, one tile is 140 cm. Tile x and y are mirrored, so with the camera turned 45 degrees
# the camp lies at the bottom left of the screen and the crater at the top right, as in the prototype.

import math
import random

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Aschenmark"
MATERIAL_FOLDER = "/Game/Vael/Maps/Blockout"
TILE = 140.0
MAP_TILES = 64

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")
CONE = unreal.load_asset("/Engine/BasicShapes/Cone.Cone")
SPHERE = unreal.load_asset("/Engine/BasicShapes/Sphere.Sphere")
SHAPE_MATERIAL = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")

Element = unreal.VaelElement

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(91)


# ---------------------------------------------------------------- Helpers

def world(x, y, z=0.0):
    """World location of a prototype tile position"""
    return unreal.Vector((MAP_TILES / 2 - x) * TILE, (MAP_TILES / 2 - y) * TILE, z)


def srgb(hex_code):
    def to_linear(channel):
        value = int(channel, 16) / 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4
    return unreal.LinearColor(to_linear(hex_code[0:2]), to_linear(hex_code[2:4]), to_linear(hex_code[4:6]), 1.0)


def make_material(name, hex_code):
    """Placeholder material instance in one color; an existing one is reused"""
    path = "{}/{}".format(MATERIAL_FOLDER, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)

    factory = unreal.MaterialInstanceConstantFactoryNew()
    instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MATERIAL_FOLDER, unreal.MaterialInstanceConstant, factory)
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, SHAPE_MATERIAL)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance, "Color", srgb(hex_code))
    unreal.EditorAssetLibrary.save_loaded_asset(instance, False)
    return instance


def spawn(actor_class, location, folder, label, yaw=0.0):
    actor = actors.spawn_actor_from_class(actor_class, location, unreal.Rotator(0.0, 0.0, yaw))
    actor.set_folder_path(folder)
    actor.set_actor_label(label)
    return actor


def shape(mesh, material, x, y, size_x, size_y, height, folder, label, base=0.0, yaw=0.0, collision=True):
    """A static mesh of the engine shapes, sizes in tiles, height in cm, standing on base"""
    actor = spawn(unreal.StaticMeshActor, world(x, y, base + height * 0.5), folder, label, yaw)
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    component.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(size_x * TILE / 100.0, size_y * TILE / 100.0, height / 100.0))
    if not collision:
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return actor


def flat(material, x, y, size_x, size_y, folder, label, yaw=0.0, round_shape=False, lift=1.0):
    """A patch lying on the ground, for roads, dirt and scorched earth"""
    return shape(CYLINDER if round_shape else CUBE, material, x, y, size_x, size_y, 2.0, folder, label, base=lift - 2.0, yaw=yaw, collision=False)


def spawner(kind_class, x, y, count, folder, label, spread=150.0):
    actor = spawn(unreal.VaelCreatureSpawner, world(x, y, 100.0), folder, label)
    actor.set_editor_property("creature_kind", kind_class.static_class())
    actor.set_editor_property("count", count)
    actor.set_editor_property("spread", spread)
    return actor


def ground_area(element, x, y, radius_tiles, folder, label, extinguishable=True):
    actor = spawn(unreal.VaelGroundArea, world(x, y, 2.0), folder, label)
    actor.set_editor_property("element", element)
    actor.set_editor_property("radius", radius_tiles * TILE)
    actor.set_editor_property("extinguishable", extinguishable)
    return actor


# ---------------------------------------------------------------- Level

def build():
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
        unreal.log_warning("Vael: {} exists already, nothing was changed. Delete it in the editor to build it anew.".format(LEVEL_PATH))
        return

    world_object = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    if world_object is None:
        unreal.log_error("Vael: could not create a new level")
        return

    m = {
        "ash": make_material("MI_Blockout_Ash", "37302c"),
        "dirt": make_material("MI_Blockout_Dirt", "3d3128"),
        "road": make_material("MI_Blockout_Road", "4a3f35"),
        "scorch": make_material("MI_Blockout_Scorch", "2b1d18"),
        "rock": make_material("MI_Blockout_Rock", "5a524c"),
        "wall": make_material("MI_Blockout_Wall", "6b5e52"),
        "chapel": make_material("MI_Blockout_Chapel", "7a6e62"),
        "water": make_material("MI_Blockout_DeepWater", "132630"),
        "tree": make_material("MI_Blockout_DeadTree", "2a221c"),
        "tent": make_material("MI_Blockout_Tent", "6b5a44"),
        "order": make_material("MI_Blockout_Order", "5a2442"),
        "wood": make_material("MI_Blockout_Wood", "5c4632"),
        "stone": make_material("MI_Blockout_Stone", "8a8178"),
    }

    # Light and sky: dark ash light, to be tuned in the editor
    sun = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 2000), "Licht", "Sonne", yaw=-30.0)
    sun.set_actor_rotation(unreal.Rotator(0.0, -50.0, -30.0), False)
    sun.light_component.set_editor_property("intensity", 4.0)
    sun.light_component.set_light_color(unreal.LinearColor(1.0, 0.82, 0.68, 1.0))
    sky = spawn(unreal.SkyLight, unreal.Vector(0, 0, 1500), "Licht", "Himmelslicht")
    sky.light_component.set_editor_property("real_time_capture", True)

    # Movable lights need no light building; the light changes with the weather later anyway
    sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    spawn(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), "Licht", "Atmosphaere")
    fog = spawn(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0), "Licht", "Ascheschleier")
    fog.component.set_editor_property("fog_density", 0.03)
    fog.component.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.25, 0.2, 0.18, 1.0))

    # Region, start and ground
    region = spawn(unreal.VaelRegion, world(32, 32, 0), "Spiel", "Region Aschenmark")
    region.set_editor_property("region_name", unreal.Text("Aschenmark"))
    region.set_editor_property("covers_whole_level", True)
    region.set_editor_property("offering_crystals", 5)

    spawn(unreal.PlayerStart, world(11.6, 51.0, 100.0), "Spiel", "Spielerstart", yaw=225.0)

    shape(CUBE, m["ash"], 32, 32, MAP_TILES, MAP_TILES, 20.0, "Boden", "Ascheboden", base=-20.0)

    # Border of rock around the map
    for label, x, y, sx, sy in (("Rand Nord", 32, 1, MAP_TILES, 2), ("Rand Sued", 32, 63, MAP_TILES, 2), ("Rand West", 1, 32, 2, MAP_TILES), ("Rand Ost", 63, 32, 2, MAP_TILES)):
        shape(CUBE, m["rock"], x, y, sx, sy, 300.0, "Rand", label)

    # Roads between the places
    roads = [((10, 52), (30, 41)), ((30, 40), (51, 41)), ((30, 40), (29, 20)), ((29, 20), (45, 17)), ((45, 17), (47, 16))]
    for index, (a, b) in enumerate(roads):
        start, end = world(*a), world(*b)
        length = math.hypot(end.x - start.x, end.y - start.y) / TILE
        yaw = math.degrees(math.atan2(end.y - start.y, end.x - start.x))
        middle = ((a[0] + b[0]) * 0.5, (a[1] + b[1]) * 0.5)
        flat(m["road"], middle[0], middle[1], length + 1.0, 3.0, "Wege", "Weg {}".format(index + 1), yaw=yaw)

    # Camp of the survivors with Edda
    ground_area(Element.FIRE, 10.5, 52.5, 0.8, "Lager", "Lagerfeuer", extinguishable=False)
    for label, x, y, color in (("Zelt 1", 7.5, 49.5, "tent"), ("Zelt 2", 13.5, 55.5, "tent")):
        shape(CONE, m[color], x, y, 1.6, 1.6, 180.0, "Lager", label)
    for label, x, y in (("Kiste 1", 8.5, 55.5), ("Kiste 2", 14.5, 49.5)):
        shape(CUBE, m["wood"], x, y, 0.7, 0.7, 70.0, "Lager", label)
    edda = spawn(unreal.VaelNpc, world(12.6, 50.4, 92.0), "Lager", "Edda Krell", yaw=225.0)
    edda.set_editor_property("dialogue", unreal.load_asset("/Game/Vael/Story/DA_Dialogue_Edda"))
    edda.set_editor_property("robe_color", srgb("6b5a44"))

    # Pond in the west: shallow water to draw from, a deep middle nobody crosses
    ground_area(Element.WATER, 13, 28, 5.2, "Teich", "Seichtes Wasser")
    shape(CYLINDER, m["water"], 13, 28, 5.4, 5.4, 25.0, "Teich", "Tiefes Wasser")
    shape(CUBE, m["stone"], 7.0, 23.0, 1.5, 1.5, 120.0, "Teich", "Schrein")
    shape(CUBE, m["stone"], 7.0, 23.0, 0.5, 0.5, 260.0, "Teich", "Schrein Saeule", base=120.0, collision=False)

    # Village ruins: three houses and the chapel, doors to the south and the chapel door to the west
    houses = [(22, 33, 27, 37, "S", "Haus 1"), (33, 33, 37, 37, "S", "Haus 2"), (16, 40, 20, 44, "S", "Haus 3"), (34, 43, 40, 49, "W", "Kapelle")]
    for x0, y0, x1, y1, door, name in houses:
        chapel = name == "Kapelle"
        folder = "Dorf/{}".format(name)
        flat(m["dirt"], (x0 + x1 + 1) * 0.5, (y0 + y1 + 1) * 0.5, x1 - x0 + 1, y1 - y0 + 1, folder, "Boden")
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                if x not in (x0, x1) and y not in (y0, y1):
                    continue
                mid_x, mid_y = (x0 + x1) // 2, (y0 + y1) // 2
                if door == "S" and y == y1 and x in (mid_x, mid_x + 1):
                    continue
                if door == "W" and x == x0 and y in (mid_y, mid_y + 1):
                    continue
                if rng.random() < 0.18:
                    continue
                height = (260.0 + rng.random() * 160.0) if chapel else (120.0 + rng.random() * 140.0)
                shape(CUBE, m["chapel" if chapel else "wall"], x + 0.5, y + 0.5, 1.0, 1.0, height, folder, "Mauer")
    shape(CYLINDER, m["stone"], 29.5, 39.5, 1.2, 1.2, 80.0, "Dorf", "Brunnen")

    # Order of the Scar in the east
    for label, x, y in (("Zelt 1", 51.5, 37.5), ("Zelt 2", 55.5, 38.5), ("Zelt 3", 54.5, 44.5)):
        shape(CONE, m["order"], x, y, 1.8, 1.8, 200.0, "Orden der Narbe", label)
    for label, x, y in (("Banner 1", 49.5, 42.5), ("Banner 2", 57.5, 41.5)):
        shape(CUBE, m["order"], x, y, 0.15, 0.5, 320.0, "Orden der Narbe", label)
    ground_area(Element.FIRE, 53.5, 41.5, 0.6, "Orden der Narbe", "Feuerschale", extinguishable=False)

    # Mark source in the east
    spawn(unreal.VaelMarkSource, world(45.5, 22.5, 2.0), "Mark-Quelle", "Mark-Quelle")

    # Crater of the ember queen: scorched ground in a ring of rock, open towards the southwest
    flat(m["scorch"], 52, 11, 16.4, 16.4, "Krater", "Verbrannter Boden", round_shape=True)
    gap = math.atan2(1.0, -1.0)
    for step in range(56):
        angle = step / 56.0 * 2.0 * math.pi
        difference = abs((angle - gap + math.pi) % (2.0 * math.pi) - math.pi)
        if difference < 0.36:
            continue
        x, y = 52 + math.cos(angle) * 8.9, 11 + math.sin(angle) * 8.9
        if not (2 < x < 62 and 2 < y < 62):
            continue
        size = 1.0 + rng.random() * 0.4
        shape(CUBE, m["rock"], x, y, size, size, 150.0 + rng.random() * 130.0, "Krater", "Fels", yaw=rng.random() * 90.0)

    # Scattered rocks and dead trees, away from paths and places
    reserved = [(13, 28, 6.5), (45.5, 22.5, 4.5), (52, 11, 10.5), (46, 17, 3), (30, 40, 3), (10, 52, 7), (53, 41, 6.5),
                (24.5, 35, 4), (35, 35, 4), (18, 42, 4), (37, 46, 5), (7, 23, 3), (30, 13, 3)]
    def near_road(x, y):
        for (ax, ay), (bx, by) in roads:
            dx, dy = bx - ax, by - ay
            t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / max(dx * dx + dy * dy, 1e-6)))
            if math.hypot(x - (ax + dx * t), y - (ay + dy * t)) < 2.2:
                return True
        return False
    for y in range(3, 61):
        for x in range(3, 61):
            px, py = x + 0.5, y + 0.5
            if any(math.hypot(px - cx, py - cy) < r for cx, cy, r in reserved) or near_road(px, py):
                continue
            roll = rng.random()
            if roll < 0.022:
                size = 0.7 + rng.random() * 0.6
                shape(CUBE, m["rock"], px, py, size, size, 80.0 + rng.random() * 120.0, "Streuung", "Fels", yaw=rng.random() * 90.0)
            elif roll < 0.045:
                shape(CYLINDER, m["tree"], px, py, 0.25, 0.25, 220.0 + rng.random() * 160.0, "Streuung", "Toter Baum")

    # Scrolls of sealed formulas, as in the prototype
    scrolls = [((31.6, 41.6), [Element.WATER, Element.AIR], "Schriftrolle am Brunnen", "Dorf"),
               ((37.6, 46.4), [Element.EARTH, Element.EARTH, Element.EARTH], "Schriftrolle der Kapelle", "Dorf/Kapelle"),
               ((8.4, 25.6), [Element.WATER, Element.WATER, Element.WATER], "Schriftrolle am Schrein", "Teich")]
    for (x, y), elements, label, folder in scrolls:
        scroll = spawn(unreal.VaelFormulaScroll, world(x, y, 40.0), folder, label)
        scroll.set_editor_property("elements", elements)
        scroll.set_editor_property("label", unreal.Text(label))

    # Chest with the Sturmmantel among the harpies of the fields, the creatures of wind
    chest = spawn(unreal.VaelChest, world(31.5, 15.5, 0.0), "Felder", "Truhe mit Sturmmantel")
    chest.set_editor_property("fixed_items", [unreal.load_asset("/Game/Vael/Items/Gear/DA_Item_Sturmmantel")])

    # Creatures, positions of the prototype
    crawlers = [(20, 44), (24, 40), (33, 41), (36, 38), (26, 50), (18, 38), (40, 30), (34, 26), (22, 24), (38, 20), (42, 35), (16, 34), (44, 46), (28, 46)]
    for index, (x, y) in enumerate(crawlers):
        spawner(unreal.VaelEmberCrawlerData, x + 0.5, y + 0.5, 1, "Gegner/Glutkriecher", "Glutkriecher {}".format(index + 1))
    for index, (x, y) in enumerate([(22, 16), (34, 21), (38, 12), (19, 22)]):
        spawner(unreal.VaelAshHarpyData, x, y, 3, "Gegner/Aschharpyien", "Harpyien {}".format(index + 1))
    spawner(unreal.VaelHarpyElderData, 30, 13, 1, "Gegner/Aschharpyien", "Harpyien-Aelteste")
    spawner(unreal.VaelAshHarpyData, 30, 13, 3, "Gegner/Aschharpyien", "Harpyien der Aeltesten", spread=210.0)
    for index, (x, y) in enumerate([(52, 40), (56, 41), (53, 45)]):
        spawner(unreal.VaelPreacherData, x + 0.5, y + 0.5, 1, "Gegner/Prediger", "Prediger {}".format(index + 1))
    spawner(unreal.VaelEmberQueenData, 52, 10, 1, "Gegner/Glutkoenigin", "Glutkoenigin")

    if unreal.EditorLoadingAndSavingUtils.save_map(world_object, LEVEL_PATH):
        unreal.log("Vael: built {}".format(LEVEL_PATH))
    else:
        unreal.log_error("Vael: could not save {}".format(LEVEL_PATH))


build()
