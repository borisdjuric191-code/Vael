# Baut den Eisenschlund (Region IV, Rippen der Urdrachin) grob in die offene Welt L_Vael, nach dem Zonenprofil
# Docs/Karte/eisenschlund_zonen.svg (Boris 13.11.2026: "sieht ganz gut aus"; erst die grobe Karte, dann Schluchten mit
# Höhlenstimmung, große Höhlen und Minen später als eigene Level).
#
# Aus Scripts/Data/Welt/ES_*_Hoehen.r16, ES_Orte.json und T_ES_Wege.png (Tools/Welt/eisenschlund_welt.py):
# - Zwei Landschaften "Eisenschlund Gelaende West/Ost" (zwischen Aschenmark, Wurzelforst und Sturmzinnen), Boden M_ES_Boden
#   (Staub und Rost, dunkler Fels an den Rippenwänden, Schlacke an der Wasserlinie, die Wege).
# - Vier Lager, die Grubenstadt der Gilde, Schmelzhütten, Förderturm, Stollenportale (Kernerz, geflutet, Knochenhalle,
#   Rostschwamm), Funkenerz, Rippenbrücke, Suhlen, Grubenlaternen, Nackenwirbel, Wachturm, Tiefster Schacht (Nid, versiegelt),
#   der glühende Schlackenfluss.
# - Der verschüttete Schlackentor-Stollen: ein Durchgang (AVaelPassage) vom Schlackentor der Aschenmark durch den Berg
#   in den Eisenschlund und zurück, bis es echte Tunnel gibt.
# - Region "Eisenschlund" (Wetter nach Verderbnis: rein ruhig, verdorben Hitze, Regen, Sturm), Küstenwände, Route ab 300.
# Alles trägt den Tag "VaelEisenschlund" (Ordner "Eisenschlund/..."); ein neuer Lauf entfernt erst das.
#
# Mit geschlossenem Editor, nach Tools/Welt/eisenschlund_welt.py:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildEisenschlund.py" -AllowCommandletRendering -unattended -nosplash

import json
import math
import os
import random

import unreal

LEVEL = "/Game/Vael/Maps/L_Vael"
DATA = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", "Welt")
FOLDER = "Eisenschlund"
TAG = "VaelEisenschlund"
ASSETS = "/Game/Vael/Maps/Eisenschlund"
BLOCKOUT = "/Game/Vael/Maps/Blockout"
M = 100.0

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
CYLINDER = unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")
CONE = unreal.load_asset("/Engine/BasicShapes/Cone.Cone")
PLANE = unreal.load_asset("/Engine/BasicShapes/Plane.Plane")
SHAPE_MATERIAL = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")
TEXTURES = {
    "grass": "/Game/Fab/Megascans/Surfaces/Uncut_Grass_oeeb70/Low/oeeb70_tier_3/Textures/T_oeeb70_1K_B",
    "rock": "/Game/Rock_Collection_04/Meshes/Rock_01/Textures/T_Rock_01_A",
    "sand": "/Game/MS/Surfaces/Dry_Sandy_Mud_wdunfjf/T_Dry_Sandy_Mud_wdunfjf_D",
}

editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assets = unreal.EditorAssetLibrary
library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
rng = random.Random(1311)
counts = {}

with open(os.path.join(DATA, "ES_Orte.json"), encoding="utf-8") as f:
    D = json.load(f)
GEBIET = D["gebiet"]
X0, Y0 = GEBIET["ursprung_m"]
NX, NY = GEBIET["punkte"]
STEP = GEBIET["schritt_m"]


def log(message):
    unreal.log_warning("[BuildEisenschlund] " + message)


def srgb(hex_code):
    def lin(c):
        value = int(c, 16) / 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4
    return unreal.LinearColor(lin(hex_code[0:2]), lin(hex_code[2:4]), lin(hex_code[4:6]), 1.0)


def shape_material(name, hex_code):
    path = "{}/{}".format(BLOCKOUT, name)
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    instance = tools.create_asset(name, BLOCKOUT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(instance, SHAPE_MATERIAL)
    library.set_material_instance_vector_parameter_value(instance, "Color", srgb(hex_code))
    assets.save_loaded_asset(instance, False)
    return instance


# ---------------------------------------------------------------- ground material

class Graph:
    def __init__(self, material):
        self.material = material
        self.rows = {}

    def node(self, cls, column, **props):
        row = self.rows.get(column, 0)
        self.rows[column] = row + 1
        e = library.create_material_expression(self.material, cls, -300 * column, 140 * row)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, a, b, inp="", out=""):
        if not library.connect_material_expressions(a, out, b, inp):
            log("link failed: {} -> {}.{}".format(a.get_name(), b.get_name(), inp))

    def op(self, cls, column, a, b):
        e = self.node(cls, column)
        self.link(a, e, "A")
        self.link(b, e, "B")
        return e

    def const(self, value, column):
        return self.node(unreal.MaterialExpressionConstant, column, r=value)

    def scalar(self, name, value, column):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=value)

    def color(self, name, rgb, column):
        p = self.node(unreal.MaterialExpressionVectorParameter, column, parameter_name=name, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return self.mask(p, column - 1, r=True, g=True, b=True)

    def mask(self, source, column, r=False, g=False, b=False, a=False):
        e = self.node(unreal.MaterialExpressionComponentMask, column, r=r, g=g, b=b, a=a)
        self.link(source, e)
        return e

    def sample(self, texture, uv, column, masks=False):
        e = self.node(unreal.MaterialExpressionTextureSample, column, texture=texture)
        if masks:
            e.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
        self.link(uv, e, "UVs")
        return e

    def lerp(self, a, b, alpha, column):
        e = self.node(unreal.MaterialExpressionLinearInterpolate, column)
        self.link(a, e, "A")
        self.link(b, e, "B")
        self.link(alpha, e, "Alpha")
        return e

    def saturate(self, source, column):
        e = self.node(unreal.MaterialExpressionSaturate, column)
        self.link(source, e)
        return e


def import_paths():
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(DATA, "T_ES_Wege.png"))
    task.set_editor_property("destination_path", ASSETS)
    task.set_editor_property("destination_name", "T_ES_Wege")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(ASSETS + "/T_ES_Wege")
    texture.set_editor_property("srgb", False)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("never_stream", True)
    assets.save_loaded_asset(texture, False)
    return texture


def snow_collection():
    """Parametersammlung, deren "SchneeAbsenkung" (m) die Region im Spiel setzt"""
    path = ASSETS + "/MPC_SZ_Wetter"
    if assets.does_asset_exist(path):
        mpc = unreal.load_asset(path)
    else:
        mpc = tools.create_asset("MPC_SZ_Wetter", ASSETS, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    param = unreal.CollectionScalarParameter()
    param.set_editor_property("parameter_name", "SchneeAbsenkung")
    param.set_editor_property("default_value", 0.0)
    mpc.set_editor_property("scalar_parameters", [param])
    assets.save_loaded_asset(mpc, False)
    return mpc


def ground_material():
    path = ASSETS + "/M_ES_Boden"
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
    else:
        material = tools.create_asset("M_ES_Boden", ASSETS, unreal.Material, unreal.MaterialFactoryNew())
    tex = {k: unreal.load_asset(v) for k, v in TEXTURES.items()}
    paths = import_paths()
    g = Graph(material)
    position = g.node(unreal.MaterialExpressionWorldPosition, 14)
    plane = g.mask(position, 13, r=True, g=True)
    height_m = g.op(unreal.MaterialExpressionDivide, 12, g.mask(position, 13, b=True), g.const(100.0, 13))
    normal_z = g.mask(g.node(unreal.MaterialExpressionVertexNormalWS, 13), 12, b=True)

    def uv(size):
        return g.op(unreal.MaterialExpressionDivide, 11, plane, g.const(size, 12))
    grass = g.op(unreal.MaterialExpressionMultiply, 9, g.mask(g.sample(tex["grass"], uv(1100.0), 10), 9, r=True, g=True, b=True),
                 g.color("GrasFarbe", (0.42, 0.34, 0.27), 10))
    rock = g.op(unreal.MaterialExpressionMultiply, 9, g.mask(g.sample(tex["rock"], uv(900.0), 10), 9, r=True, g=True, b=True),
                g.color("FelsFarbe", (0.5, 0.45, 0.42), 10))
    shore = g.op(unreal.MaterialExpressionMultiply, 9, g.mask(g.sample(tex["sand"], uv(500.0), 10), 9, r=True, g=True, b=True),
                 g.color("UferFarbe", (0.25, 0.23, 0.22), 10))
    # Schnee: hell, leicht bläulich, mit großen weichen Flecken (Rauschen liest die Weltposition selbst)
    patches = g.node(unreal.MaterialExpressionNoise, 10, scale=0.0016, levels=3, output_min=0.0, output_max=1.0, quality=1)
    snow = g.lerp(g.color("SchneeSchatten", (0.72, 0.78, 0.88), 9), g.color("SchneeFarbe", (0.93, 0.95, 1.0), 9), patches, 8)

    # Fels an Hängen, Ufer an der Wasserlinie, Schnee über der Schneegrenze (je flacher, desto mehr bleibt liegen)
    steep = g.saturate(g.op(unreal.MaterialExpressionMultiply, 9, g.op(unreal.MaterialExpressionSubtract, 10, g.const(0.86, 11), normal_z), g.const(6.0, 10)), 8)
    wet = g.saturate(g.op(unreal.MaterialExpressionMultiply, 9, g.op(unreal.MaterialExpressionSubtract, 10, g.const(3.0, 11), height_m), g.const(0.4, 10)), 8)
    line = g.scalar("Schneegrenze", 5000.0, 11)
    above = g.op(unreal.MaterialExpressionDivide, 9, g.op(unreal.MaterialExpressionSubtract, 10, height_m, line), g.const(30.0, 10))
    snow_amount = g.saturate(g.op(unreal.MaterialExpressionAdd, 8, above, g.op(unreal.MaterialExpressionMultiply, 9,
                                  g.op(unreal.MaterialExpressionSubtract, 10, patches, g.const(0.5, 11)), g.const(0.8, 10))), 7)
    snow_amount = g.op(unreal.MaterialExpressionMultiply, 6, snow_amount, g.saturate(g.op(unreal.MaterialExpressionMultiply, 7,
                       g.op(unreal.MaterialExpressionSubtract, 8, normal_z, g.const(0.45, 9)), g.const(4.0, 8)), 6))

    ground = g.lerp(grass, rock, steep, 6)
    ground = g.lerp(ground, shore, wet, 5)
    ground = g.lerp(ground, snow, snow_amount, 4)
    # Wege: festgetretener Kies, im Schnee bleibt eine helle Spur
    path_uv = g.op(unreal.MaterialExpressionDivide, 11, g.op(unreal.MaterialExpressionSubtract, 12, plane,
                   g.node(unreal.MaterialExpressionConstant2Vector, 13, r=X0 * M, g=Y0 * M)),
                   g.node(unreal.MaterialExpressionConstant2Vector, 12, r=NX * STEP * M, g=NY * STEP * M))
    on_path = g.saturate(g.op(unreal.MaterialExpressionMultiply, 5, g.op(unreal.MaterialExpressionSubtract, 6,
                         g.mask(g.sample(paths, path_uv, 8, masks=True), 7, r=True), g.const(0.45, 7)), g.const(4.0, 6)), 4)
    path_colour = g.op(unreal.MaterialExpressionMultiply, 4, g.mask(g.sample(tex["sand"], uv(300.0), 6), 5, r=True, g=True, b=True),
                       g.color("WegFarbe", (0.48, 0.42, 0.36), 6))
    ground = g.lerp(ground, path_colour, on_path, 3)
    library.connect_material_property(ground, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = g.lerp(g.const(0.9, 4), g.const(0.55, 4), snow_amount, 3)
    library.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


# ---------------------------------------------------------------- placing

def at(p, up=0.0):
    return unreal.Vector(p["x"] * M, p["y"] * M, (p["z"] + up) * M)


def spawn(cls, location, folder, label, yaw=0.0, streamed=True):
    actor = editor.spawn_actor_from_class(cls, location, unreal.Rotator(0.0, 0.0, yaw))
    actor.set_folder_path("{}/{}".format(FOLDER, folder))
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(TAG)])
    if not streamed:
        actor.set_editor_property("is_spatially_loaded", False)
    counts[folder.split("/")[0]] = counts.get(folder.split("/")[0], 0) + 1
    return actor


def shape(mesh, material, p, size, folder, label, yaw=0.0, up=0.0, collision=True, pitch=0.0):
    """Grundform, Größe in Metern, steht auf dem Punkt"""
    actor = spawn(unreal.StaticMeshActor, unreal.Vector(p["x"] * M, p["y"] * M, (p["z"] + up + size[2] * 0.5) * M), folder, label, yaw)
    actor.set_actor_rotation(unreal.Rotator(0.0, pitch, yaw), False)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.static_mesh_component.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(*size))
    if not collision:
        actor.static_mesh_component.set_collision_profile_name("NoCollision")
    return actor


def near(p, dx, dy, dz=0.0):
    return {"x": p["x"] + dx, "y": p["y"] + dy, "z": p["z"] + dz}


def pack_rocks():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = []
    for a in registry.get_assets_by_path("/Game/Rock_Collection_04", recursive=True):
        if str(a.asset_class_path.asset_name) == "StaticMesh" and "Rock" in str(a.asset_name):
            found.append(a.get_asset())
    return found[:8]


def camp(p, name, mat):
    folder = "Lager/" + name
    zone = spawn(unreal.VaelSafeZone, at(p), folder, "Sicherer Ort")
    zone.set_editor_property("radius", 1600.0)
    spawn(unreal.VaelFireLight, at(p, 1.0), folder, "Lagerfeuer-Licht")
    spawn(unreal.VaelFireSpot, at(p, 0.1), folder, "Lagerfeuer")
    shape(CYLINDER, mat["stone"], p, (1.6, 1.6, 0.4), folder, "Feuerstelle", up=-0.2, collision=False)
    for index in range(4):
        angle = index * 90.0 + rng.uniform(-20.0, 20.0) + 30.0
        d = rng.uniform(7.0, 10.0)
        shape(CONE, mat["tent"], near(p, math.cos(math.radians(angle)) * d, math.sin(math.radians(angle)) * d), (4.0, 4.0, 3.0),
              folder, "Zelt {}".format(index + 1), yaw=angle, up=-0.3)


def places(mat):
    o = D["orte"]
    # Grubenstadt der Gilde: Häuser, Türme und Hallen im Kessel, die vielfältigste Stadt der Welt
    g = D["lager"]["Grubenstadt der Gilde"]
    for i in range(16):
        a = math.radians(i * 22.5 + rng.uniform(-6, 6))
        r = rng.uniform(32, 62)
        p = {"x": g["x"] + math.cos(a) * r, "y": g["y"] + math.sin(a) * r}
        p["z"] = ground_at(p["x"], p["y"])
        if i % 4 == 0:
            shape(CYLINDER, mat["stone"], p, (6.0, 6.0, rng.uniform(14, 22)), "Orte/Grubenstadt", "Turm {}".format(i + 1), up=-0.5)
            shape(CONE, mat["roof"], p, (7.0, 7.0, 5.0), "Orte/Grubenstadt", "Turmdach {}".format(i + 1), up=19.0, collision=False)
        else:
            shape(CUBE, mat["guild"], p, (rng.uniform(9, 15), rng.uniform(7, 11), rng.uniform(6, 10)), "Orte/Grubenstadt", "Haus {}".format(i + 1),
                  yaw=math.degrees(a), up=-0.5)
    # Schmelzhütten: Hallen mit Schloten, Glut davor
    p = o["Schmelzhütten"]
    for i in range(3):
        q = near(p, rng.uniform(-30, 30), rng.uniform(-30, 30))
        q["z"] = ground_at(q["x"], q["y"])
        shape(CUBE, mat["slag"], q, (14.0, 10.0, 8.0), "Orte/Schmelzhuetten", "Halle {}".format(i + 1), yaw=rng.uniform(0, 360), up=-0.5)
        shape(CYLINDER, mat["stone"], near(q, 4, 3, 0), (2.5, 2.5, 18.0), "Orte/Schmelzhuetten", "Schlot {}".format(i + 1), up=-0.5)
    # Förderturm über dem Hauptschacht
    p = o["Förderturm"]
    for dx, dy in ((-4, -4), (4, -4), (-4, 4), (4, 4)):
        shape(CUBE, mat["wood"], near(p, dx, dy), (0.8, 0.8, 26.0), "Orte/Foerderturm", "Pfeiler", up=-0.5)
    shape(CUBE, mat["wood"], p, (10.0, 10.0, 1.0), "Orte/Foerderturm", "Plattform", up=25.0)
    shape(CYLINDER, mat["metal"], p, (6.0, 6.0, 0.6), "Orte/Foerderturm", "Seilrad", up=27.0, pitch=90.0, collision=False)
    shape(CYLINDER, mat["dark"], p, (7.0, 7.0, 0.2), "Orte/Foerderturm", "Schachtöffnung", up=0.05, collision=False)
    # Stolleneingänge: Felsportale (die großen Minen werden später eigene Höhlenlevel)
    for name in ("Kernerz-Stollen", "Geflutete Stollen", "Knochenhalle", "Rostschwamm-Höhle"):
        portal(mat, o[name], name)
    # Funkenerz-Adern: glimmende Brocken
    p = o["Funkenerz-Adern"]
    for i in range(10):
        q = near(p, rng.uniform(-35, 35), rng.uniform(-35, 35))
        q["z"] = ground_at(q["x"], q["y"])
        shape(CUBE, mat["ember"], q, (rng.uniform(0.8, 2.0), rng.uniform(0.8, 2.0), rng.uniform(0.8, 1.8)), "Orte/Funkenerz", "Funkenerz {}".format(i + 1),
              yaw=rng.uniform(0, 360), up=-0.4, pitch=rng.uniform(-25, 25))
    # Windschlucht: nur eine Markierung (die Schlucht ist das Gelände)
    shape(CYLINDER, mat["wood"], o["Windschlucht"], (0.3, 0.3, 4.0), "Orte/Windschlucht", "Wimpelstange", up=-0.3)
    # Rippenbrücke: ein Felsbogen über den Weg
    arch(mat, o["Rippenbrücke"], "Rippenbrücke", 18.0, 14.0)
    # Salvenschwein-Suhlen: Schlammflächen
    p = o["Salvenschwein-Suhlen"]
    for i in range(5):
        q = near(p, rng.uniform(-30, 30), rng.uniform(-30, 30))
        q["z"] = ground_at(q["x"], q["y"])
        shape(CYLINDER, mat["mud"], q, (rng.uniform(5, 9), rng.uniform(4, 7), 0.15), "Orte/Suhlen", "Suhle {}".format(i + 1), yaw=rng.uniform(0, 360),
              up=0.0, collision=False)
    # Grubenlaternen (Pflanze, Platzhalter): leuchtende Kolben
    p = o["Grubenlaternen"]
    for i in range(8):
        q = near(p, rng.uniform(-25, 25), rng.uniform(-25, 25))
        q["z"] = ground_at(q["x"], q["y"])
        shape(CYLINDER, mat["lantern"], q, (0.4, 0.4, 1.2), "Orte/Grubenlaternen", "Grubenlaterne {}".format(i + 1), up=0.0, collision=False)
    # Nackenwirbel: große runde Knochenfelsen
    p = o["Nackenwirbel"]
    for i in range(5):
        q = near(p, i * 22.0 - 44.0, rng.uniform(-10, 10))
        q["z"] = ground_at(q["x"], q["y"])
        shape(CYLINDER, mat["bone"], q, (12.0, 12.0, 5.0), "Orte/Nackenwirbel", "Wirbel {}".format(i + 1), yaw=rng.uniform(0, 360), up=-1.0)
    # Wachturm der Gilde
    p = o["Wachturm der Gilde"]
    shape(CUBE, mat["guild"], p, (6.0, 6.0, 16.0), "Orte/Wachturm", "Wachturm", up=-0.5)
    # Tiefster Schacht: versiegelte Nid-Quelle und die Sondierung zum Herzen
    shape(CYLINDER, mat["nid"], D["nid_quelle"], (5.0, 5.0, 0.6), "Orte/TiefsterSchacht", "Nid-Quelle (versiegelt)", up=-0.2, collision=False)
    shape(CYLINDER, mat["metal"], near(D["nid_quelle"], 8, 0), (1.2, 1.2, 9.0), "Orte/TiefsterSchacht", "Sondierbohrer", up=-0.5)
    # Der Schlackenfluss glüht in seiner Rinne
    line = D["schlackenfluss"]
    for a, b in zip(line, line[1:]):
        length = math.hypot(b["x"] - a["x"], b["y"] - a["y"])
        yaw = math.degrees(math.atan2(b["y"] - a["y"], b["x"] - a["x"]))
        mid = {"x": (a["x"] + b["x"]) * 0.5, "y": (a["y"] + b["y"]) * 0.5}
        mid["z"] = min(ground_at(mid["x"], mid["y"]), a["z"], b["z"])
        shape(CUBE, mat["ember"], mid, (length + 4.0, 7.0, 0.3), "Orte/Schlackenfluss", "Glutstrom", yaw=yaw, up=0.2, collision=False)
        spawn(unreal.VaelFireLight, at(mid, 2.0), "Orte/Schlackenfluss", "Glutschein")


def portal(mat, p, name):
    """Felsportal eines Stolleneingangs: zwei Pfeiler und ein Sturz, dahinter Dunkel"""
    for side in (-3.5, 3.5):
        q = near(p, 0, side)
        q["z"] = ground_at(q["x"], q["y"])
        shape(CUBE, mat["stone"], q, (2.0, 1.6, 7.0), "Orte/Stollen/" + name, "Pfeiler", up=-0.5)
    shape(CUBE, mat["stone"], p, (2.4, 9.0, 1.6), "Orte/Stollen/" + name, "Sturz", up=6.5)
    shape(CUBE, mat["dark"], near(p, -1.5, 0), (0.4, 5.6, 6.2), "Orte/Stollen/" + name, "Dunkel", up=0.0, collision=False)


def arch(mat, p, name, width, height):
    """Ein Felsbogen über dem Weg, unter dem man durchgeht"""
    for side in (-width * 0.5, width * 0.5):
        q = near(p, 0, side)
        q["z"] = ground_at(q["x"], q["y"])
        shape(CUBE, mat["bone"], q, (6.0, 4.0, height), "Orte/" + name, "Bogenfuß", up=-1.0)
    shape(CUBE, mat["bone"], p, (6.0, width + 4.0, 3.5), "Orte/" + name, "Bogen", up=height - 2.0)


def passages(mat):
    """Der verschüttete Schlackentor-Stollen: von der Aschenmark durch den Berg in den Eisenschlund und zurück"""
    st = D["stollen"]
    am = st["aschenmark"]
    am_z = am_ground(am["x"], am["y"])
    es = st["eisenschlund"]
    a = spawn(unreal.VaelPassage, unreal.Vector(am["x"] * M, am["y"] * M, (am_z + 1.5) * M), "Stollen", "Schlackentor-Stollen (Aschenmark)", streamed=False)
    a.set_editor_property("destination", unreal.Vector2D((es["x"] + 2.0) * M, (es["y"] + 9.0) * M))
    a.set_editor_property("message", unreal.Text("Durch den alten Stollen in den Eisenschlund"))
    b = spawn(unreal.VaelPassage, at(es, 1.5), "Stollen", "Schlackentor-Stollen (Eisenschlund)", streamed=False)
    b.set_editor_property("destination", unreal.Vector2D(am["x"] * M, (am["y"] - 9.0) * M))
    b.set_editor_property("message", unreal.Text("Durch den alten Stollen zurück in die Aschenmark"))
    portal(mat, es, "Schlackentor-Ausgang")


def am_ground(x, y):
    import array
    raw = array.array("H")
    raw.frombytes(open(os.path.join(DATA, "AM_Hoehen.r16"), "rb").read())
    return (raw[int(round(y)) * 2017 + int(round(x))] - 32768) / 128.0


heights = {}


def ground_at(x, y):
    """Bodenhöhe aus dem ganzen Gebiet (beide Landschaften), bilinear"""
    g = D["gebiet"]
    gx0, gy0 = g["ursprung_m"]
    ni, nj = g["punkte"]
    for name, land in D["landschaften"].items():
        x0, y0 = land["ursprung_m"]
        nx, ny = land["punkte"]
        fx, fy = (x - x0) / STEP, (y - y0) / STEP
        if 0 <= fx <= nx - 1.001 and 0 <= fy <= ny - 1.001:
            ix, iy = int(fx), int(fy)
            tx, ty = fx - ix, fy - iy
            h = heights[name]

            def v(i, j):
                return (h[j * nx + i] - 32768) / 128.0
            return (v(ix, iy) * (1 - tx) + v(ix + 1, iy) * tx) * (1 - ty) + (v(ix, iy + 1) * (1 - tx) + v(ix + 1, iy + 1) * tx) * ty
    return 0.0


def walls():
    n = 0
    for line in D["kueste"]:
        for a, b in zip(line, line[1:]):
            length = math.hypot(b["x"] - a["x"], b["y"] - a["y"])
            yaw = math.degrees(math.atan2(b["y"] - a["y"], b["x"] - a["x"]))
            z = min(a["z"], b["z"]) - 4.0
            wall = spawn(unreal.StaticMeshActor, unreal.Vector((a["x"] + b["x"]) * 50.0, (a["y"] + b["y"]) * 50.0, (z + 9.0) * M),
                         "Grenzen/Kueste", "Kuestenwand", yaw)
            wall.static_mesh_component.set_static_mesh(CUBE)
            wall.static_mesh_component.set_editor_property("visible", False)
            wall.set_actor_hidden_in_game(True)
            wall.set_actor_scale3d(unreal.Vector(length + 2.0, 3.0, 18.0))
            n += 1
    return n


def main():
    import array
    for name in D["landschaften"]:
        raw = array.array("H")
        raw.frombytes(open(os.path.join(DATA, "ES_{}_Hoehen.r16".format(name)), "rb").read())
        heights[name] = raw

    assets.make_directory(ASSETS)
    ground = ground_material()
    mat = {
        "stone": shape_material("MI_Blockout_Stone", "5a554f"), "tent": shape_material("MI_Blockout_Tent", "6b5a44"),
        "guild": shape_material("MI_Blockout_Guild", "7a5a3a"), "slag": shape_material("MI_Blockout_Slag", "2e2c2f"),
        "roof": shape_material("MI_Blockout_Dach_Rost", "7a3f22"), "metal": shape_material("MI_Blockout_Metall", "9aa0a6"),
        "wood": shape_material("MI_Blockout_Holz", "6a4e33"), "dark": shape_material("MI_Blockout_Dunkel", "0b0a0a"),
        "ember": shape_material("MI_Blockout_Glut", "ff6a1a"), "bone": shape_material("MI_Blockout_Knochen", "cfc4ae"),
        "mud": shape_material("MI_Blockout_Schlamm", "3b2f24"), "lantern": shape_material("MI_Blockout_Grubenlaterne", "ffd27a"),
        "nid": shape_material("MI_Blockout_Nid", "a24dff"),
    }

    if not levels.load_level(LEVEL):
        log("konnte " + LEVEL + " nicht öffnen")
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    try:
        descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
        unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
    except Exception as error:  # noqa: BLE001
        log("konnte nicht alle Actors laden: {}".format(error))
    level_actors = editor.get_all_level_actors()

    def eigene_landschaft(a):
        if not isinstance(a, unreal.LandscapeProxy):
            return False
        l = a.get_actor_location()
        west = l.x < 201300.0 and l.y >= 250600.0
        ost = l.y >= 376600.0 and l.x < 290100.0
        return west or ost
    old = [a for a in level_actors if unreal.Name(TAG) in a.get_editor_property("tags") or eigene_landschaft(a)]
    for actor in old:
        editor.destroy_actor(actor)
    log("{} Actors des letzten Laufs entfernt".format(len(old)))

    for name, land in D["landschaften"].items():
        x0, y0 = land["ursprung_m"]
        nx, ny = land["punkte"]
        actor = unreal.VaelWorldBuilder.create_landscape_from_raw(world, os.path.join(DATA, "ES_{}_Hoehen.r16".format(name)), nx, ny,
                                                                  unreal.Vector(x0 * M, y0 * M, 0.0), unreal.Vector(STEP * M, STEP * M, 100.0), ground, 2)
        if actor is None:
            log("die Landschaft {} konnte nicht angelegt werden".format(name))
            return
        actor.set_actor_label("Eisenschlund Gelaende {}".format(name))
        actor.set_folder_path(FOLDER)
        actor.set_editor_property("tags", [unreal.Name(TAG)])

    for name, p in D["lager"].items():
        camp(p, name, mat)
    places(mat)
    passages(mat)

    # Elementquellen: Feuer an den Schmelzen und den Funkenerz-Adern, Wasser in den Stollen, Fels für Erde
    for i, p in enumerate(D["quellen"]["Feuer"]):
        spawn(unreal.VaelFireSpot, at(p, 0.1), "Quellen/Feuer", "Glutstelle {}".format(i + 1))
    for i, p in enumerate(D["quellen"]["Wasser"]):
        spring = spawn(unreal.VaelGroundArea, at(p, 0.1), "Quellen/Wasser", "Grubenwasser {}".format(i + 1))
        spring.set_editor_property("element", unreal.VaelElement.WATER)
        spring.set_editor_property("radius", 400.0)
        spring.set_editor_property("extinguishable", False)
    rocks = pack_rocks()
    for i, p in enumerate(D["quellen"]["Erde"]):
        for k in range(3):
            if not rocks:
                break
            q = near(p, rng.uniform(-6, 6), rng.uniform(-6, 6))
            q["z"] = ground_at(q["x"], q["y"])
            rock = spawn(unreal.StaticMeshActor, at(q, -0.4), "Quellen/Erde", "Fels {}-{}".format(i + 1, k + 1), yaw=rng.uniform(0, 360))
            rock.static_mesh_component.set_static_mesh(rng.choice(rocks))
            s = rng.uniform(1.4, 2.4)
            rock.set_actor_scale3d(unreal.Vector(s, s, s))

    # Region: rein ruhig und windig; verdorben Hitze (Glut), Sturm und Regen (Vorschlag: Staub, Glutregen, Rostregen, Beben kommen später)
    W = unreal.VaelWeather
    gx0, gy0 = D["gebiet"]["ursprung_m"]
    gni, gnj = D["gebiet"]["punkte"]
    gx1, gy1 = gx0 + (gni - 1) * STEP, gy0 + (gnj - 1) * STEP
    region = spawn(unreal.VaelRegion, unreal.Vector((gx0 + gx1) * 50.0, (2521.0 + gy1) * 50.0, 0.0), "Spiel", "Region Eisenschlund", streamed=False)
    region.set_editor_property("region_name", unreal.Text("Eisenschlund"))
    region.set_editor_property("start_weather", W.WIND)
    region.set_editor_property("available_weathers", [W.CLEAR, W.WIND, W.RAIN, W.DROUGHT, W.STORM])
    region.set_editor_property("weather_weights_pure", {W.CLEAR: 3.0, W.WIND: 2.0, W.RAIN: 1.0, W.DROUGHT: 0.5, W.STORM: 0.3})
    region.set_editor_property("weather_weights_corrupted", {W.CLEAR: 0.2, W.WIND: 1.0, W.RAIN: 1.5, W.DROUGHT: 2.0, W.STORM: 2.0})
    region.get_component_by_class(unreal.BoxComponent).set_box_extent(unreal.Vector((gx1 - gx0) * 50.0, (gy1 - 2521.0) * 50.0, 60000.0))
    # Der Kasten des Eisenschlunds reicht in die Ecke des Wurzelforsts: dort zählt der Wurzelforst
    for actor in level_actors:
        if isinstance(actor, unreal.VaelRegion) and actor.get_actor_label() == "Region Wurzelforst":
            actor.set_editor_property("priority", 1)

    n_walls = walls()

    # Lauftest: vom Gipfel der Sturmzinnen über Akademie und Archiv den Nackenweg hinab, durch die Rippen zur Grubenstadt
    # und zum Stollen (Punkte ab 300)
    route = [{"x": 4270.0, "y": 5260.0}, {"x": 3960.0, "y": 5160.0}, {"x": 3800.0, "y": 5450.0}, {"x": 3640.0, "y": 5600.0},
             {"x": 3460.0, "y": 5760.0}, {"x": 3260.0, "y": 5820.0}, {"x": 3040.0, "y": 5830.0}]
    last = None
    for name in ("Hauptweg", "Westweg"):
        punkte = next(w for w in D["wege"] if w["name"] == name)["points"]
        if name == "Westweg":
            punkte = list(reversed(punkte))
        for x, y, z in punkte:
            if last is None or math.hypot(x - last[0], y - last[1]) >= 150.0:
                route.append({"x": x, "y": y})
                last = (x, y)
    for number, p in enumerate(route, start=300):
        z = ground_at(p["x"], p["y"]) if p["x"] < 2902.0 else 40.0
        point = spawn(unreal.TargetPoint, unreal.Vector(p["x"] * M, p["y"] * M, (z + 1.0) * M), "Spiel/Route", "Route {}".format(number), streamed=False)
        point.set_editor_property("tags", [unreal.Name("VaelRoute"), unreal.Name(str(number)), unreal.Name(TAG)])

    # Ein Meer für alle Karten (wächst nur)
    for actor in level_actors:
        if actor.get_actor_label() == "Meer" and isinstance(actor, unreal.StaticMeshActor):
            l, s = actor.get_actor_location(), actor.get_actor_scale3d()
            south, north = min(l.x - s.x * 50.0, (gx0 - 600.0) * M), max(l.x + s.x * 50.0, (gx1 + 600.0) * M)
            west, east = min(l.y - s.y * 50.0, (gy0 - 600.0) * M), max(l.y + s.y * 50.0, (gy1 + 600.0) * M)
            actor.set_actor_location(unreal.Vector((south + north) * 0.5, (west + east) * 0.5, l.z), False, True)
            actor.set_actor_scale3d(unreal.Vector((north - south) / 100.0, (east - west) / 100.0, 1.0))

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("fertig: {} Küstenwände, {} Routenpunkte, gesetzt {}".format(n_walls, len(route), counts))


main()
