# Baut die Sturmzinnen (Region III, Kopf der Urdrachin) grob in die offene Welt L_Vael, nach dem Zonenprofil
# Docs/Karte/sturmzinnen_zonen.svg (Boris 13.11.2026: "Passt, bau los"; erst grob, die Details kommen später).
#
# Aus Scripts/Data/Welt/SZ_Hoehen.r16, SZ_Orte.json und T_SZ_Wege.png (Tools/Welt/sturmzinnen_welt.py):
# - Landschaft "Sturmzinnen Gelaende" östlich an den Kiefer des Wurzelforsts; Boden M_SZ_Boden: kaltes Gras, Fels an
#   Hängen, Schnee über der Schneegrenze (Parameter "Schneegrenze", später je nach Verderbnis tiefer), die Wege.
# - Kratersee im Auge, ein Meer für alle Karten, unsichtbare Küstenwände.
# - Seilbrücke über die Lücke (Schwanzspitze -> Kiefer).
# - Vier Lager (sicherer Ort, Feuer, Zelte), die Orte als Platzhalter (Akademie, Wetterwarte, Seherturm, Zahnreihe ...),
#   Elementquellen (Feuer an den Lagern, Wasser am See und an der Küste, Fels für Erde), die Nid-Quelle als Platzhalter.
# - Region "Sturmzinnen" mit Wetter nach Verderbnis (Boris 13.11.2026): rein meist klar, Wind, leichter Schnee; je verdorbener,
#   desto mehr Sturm, Schnee und Blizzard. Darin die Region "Hörner" (Vorrang, liest die Verderbnis der Sturmzinnen): dort
#   immer eine Stufe kälter. Die Schneegrenze des Bodens sinkt mit Verderbnis und Schneefall (MPC_SZ_Wetter, "SchneeAbsenkung").
# - Routenpunkte ab 200 für den Lauftest (-VaelStreamTest).
# Alles trägt den Tag "VaelSturmzinnen" (Ordner "Sturmzinnen/..."); ein neuer Lauf entfernt erst das.
#
# Mit geschlossenem Editor, nach Tools/Welt/sturmzinnen_welt.py:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildSturmzinnen.py" -AllowCommandletRendering -unattended -nosplash

import json
import math
import os
import random

import unreal

LEVEL = "/Game/Vael/Maps/L_Vael"
DATA = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", "Welt")
FOLDER = "Sturmzinnen"
TAG = "VaelSturmzinnen"
ASSETS = "/Game/Vael/Maps/Sturmzinnen"
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

with open(os.path.join(DATA, "SZ_Orte.json"), encoding="utf-8") as f:
    D = json.load(f)
LAND = D["landschaft"]
X0, Y0 = LAND["ursprung_m"]
NX, NY = LAND["punkte"]
STEP = LAND["schritt_m"]
NORTH, EAST = X0 + (NX - 1) * STEP, Y0 + (NY - 1) * STEP


def log(message):
    unreal.log_warning("[BuildSturmzinnen] " + message)


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
    task.set_editor_property("filename", os.path.join(DATA, "T_SZ_Wege.png"))
    task.set_editor_property("destination_path", ASSETS)
    task.set_editor_property("destination_name", "T_SZ_Wege")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(ASSETS + "/T_SZ_Wege")
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
    path = ASSETS + "/M_SZ_Boden"
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
    else:
        material = tools.create_asset("M_SZ_Boden", ASSETS, unreal.Material, unreal.MaterialFactoryNew())
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
                 g.color("GrasFarbe", (0.42, 0.44, 0.46), 10))
    rock = g.op(unreal.MaterialExpressionMultiply, 9, g.mask(g.sample(tex["rock"], uv(900.0), 10), 9, r=True, g=True, b=True),
                g.color("FelsFarbe", (0.62, 0.66, 0.72), 10))
    shore = g.op(unreal.MaterialExpressionMultiply, 9, g.mask(g.sample(tex["sand"], uv(500.0), 10), 9, r=True, g=True, b=True),
                 g.color("UferFarbe", (0.5, 0.52, 0.55), 10))
    # Schnee: hell, leicht bläulich, mit großen weichen Flecken (Rauschen liest die Weltposition selbst)
    patches = g.node(unreal.MaterialExpressionNoise, 10, scale=0.0016, levels=3, output_min=0.0, output_max=1.0, quality=1)
    snow = g.lerp(g.color("SchneeSchatten", (0.72, 0.78, 0.88), 9), g.color("SchneeFarbe", (0.93, 0.95, 1.0), 9), patches, 8)

    # Fels an Hängen, Ufer an der Wasserlinie, Schnee über der Schneegrenze (je flacher, desto mehr bleibt liegen)
    steep = g.saturate(g.op(unreal.MaterialExpressionMultiply, 9, g.op(unreal.MaterialExpressionSubtract, 10, g.const(0.86, 11), normal_z), g.const(6.0, 10)), 8)
    wet = g.saturate(g.op(unreal.MaterialExpressionMultiply, 9, g.op(unreal.MaterialExpressionSubtract, 10, g.const(3.0, 11), height_m), g.const(0.4, 10)), 8)
    drop = g.node(unreal.MaterialExpressionCollectionParameter, 12, collection=snow_collection(), parameter_name="SchneeAbsenkung")
    line = g.op(unreal.MaterialExpressionSubtract, 10, g.scalar("Schneegrenze", 85.0, 11), drop)
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
                       g.color("WegFarbe", (0.42, 0.42, 0.44), 6))
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
    # Akademie am Auge: Türme und Häuser auf dem Kraterrand, eine Mauer zum See
    eye = D["auge"]
    for i in range(10):
        a = math.radians(150 + i * 22)
        r = eye["radius"] + rng.uniform(42, 60)
        p = {"x": eye["x"] + math.cos(a) * r, "y": eye["y"] + math.sin(a) * r, "z": 0.0}
        p["z"] = ground_at(p["x"], p["y"])
        if i % 3 == 0:
            shape(CYLINDER, mat["academy"], p, (7.0, 7.0, rng.uniform(16, 26)), "Orte/Akademie", "Turm {}".format(i + 1), up=-0.5)
            shape(CONE, mat["roof"], p, (8.0, 8.0, 6.0), "Orte/Akademie", "Turmdach {}".format(i + 1), up=-0.5 + 22.0, collision=False)
        else:
            shape(CUBE, mat["academy"], p, (rng.uniform(10, 16), rng.uniform(8, 12), rng.uniform(7, 11)), "Orte/Akademie", "Haus {}".format(i + 1),
                  yaw=math.degrees(a), up=-0.5)
    obs = near(D["lager"]["Akademie am Auge"], 30, 40)
    obs["z"] = ground_at(obs["x"], obs["y"])
    shape(CYLINDER, mat["academy"], obs, (12.0, 12.0, 30.0), "Orte/Akademie", "Sternwarte", up=-0.5)
    # Wetterwarte: Turm mit Blitzableiter
    p = o["Wetterwarte"]
    shape(CYLINDER, mat["stone"], p, (6.0, 6.0, 18.0), "Orte/Wetterwarte", "Turm", up=-0.5)
    shape(CYLINDER, mat["metal"], p, (0.3, 0.3, 10.0), "Orte/Wetterwarte", "Blitzableiter", up=17.5, collision=False)
    # Seherturm des Ordens
    p = o["Seherturm des Ordens"]
    shape(CYLINDER, mat["order"], p, (5.0, 5.0, 28.0), "Orte/Seherturm", "Seherturm", up=-0.5)
    shape(CONE, mat["order"], p, (6.0, 6.0, 7.0), "Orte/Seherturm", "Spitze", up=27.5, collision=False)
    # Archiv im Fels
    p = o["Archiv im Fels"]
    shape(CUBE, mat["stone"], p, (14.0, 10.0, 8.0), "Orte/Archiv", "Archiv", up=-0.5)
    # Zahnreihe: Felszähne entlang der Südküste
    zahn = o["Zahnreihe"]
    for i in range(9):
        q = {"x": zahn["x"] - 30 + rng.uniform(-15, 15), "y": zahn["y"] - 400 + i * 100 + rng.uniform(-20, 20)}
        q["z"] = ground_at(q["x"], q["y"])
        h = rng.uniform(9, 16)
        shape(CONE, mat["tooth"], q, (h * 0.45, h * 0.45, h), "Orte/Zahnreihe", "Zahn {}".format(i + 1), yaw=rng.uniform(0, 360), up=-1.5,
              pitch=rng.uniform(-12, 12))
    # Blitzfeld: dunkle Glassäulen (Blitzglas)
    p = o["Blitzfeld"]
    for i in range(12):
        q = near(p, rng.uniform(-30, 30), rng.uniform(-30, 30))
        q["z"] = ground_at(q["x"], q["y"])
        shape(CYLINDER, mat["glass"], q, (0.8, 0.8, rng.uniform(1.5, 4.0)), "Orte/Blitzfeld", "Blitzglas {}".format(i + 1), up=-0.3, pitch=rng.uniform(-15, 15))
    # Sturmglocken (Pflanze, Platzhalter)
    p = o["Sturmglocken"]
    for i in range(7):
        q = near(p, rng.uniform(-25, 25), rng.uniform(-25, 25))
        q["z"] = ground_at(q["x"], q["y"])
        shape(CONE, mat["bell"], q, (1.0, 1.0, 1.4), "Orte/Sturmglocken", "Sturmglocke {}".format(i + 1), up=0.6, pitch=180.0, collision=False)
    # Frostspat-Bruch
    p = o["Frostspat-Bruch"]
    for i in range(8):
        q = near(p, rng.uniform(-25, 25), rng.uniform(-25, 25))
        q["z"] = ground_at(q["x"], q["y"])
        shape(CUBE, mat["ice"], q, (rng.uniform(1.5, 3.5), rng.uniform(1.5, 3.5), rng.uniform(2, 5)), "Orte/Frostspat", "Frostspat {}".format(i + 1),
              yaw=rng.uniform(0, 360), up=-0.5, pitch=rng.uniform(-20, 20))
    # Doppeluhu-Horste: Nester auf Felsen
    p = o["Doppeluhu-Horste"]
    for i in range(4):
        q = near(p, rng.uniform(-40, 40), rng.uniform(-40, 40))
        q["z"] = ground_at(q["x"], q["y"])
        shape(CYLINDER, mat["stone"], q, (4.0, 4.0, 6.0), "Orte/Horste", "Horstfels {}".format(i + 1), up=-0.5)
        shape(CYLINDER, mat["nest"], q, (3.0, 3.0, 0.8), "Orte/Horste", "Horst {}".format(i + 1), up=5.5, collision=False)
    # Gipfel: Steinkreis der Bossarena
    p = o["Gipfel"]
    for i in range(12):
        a = math.radians(i * 30)
        q = near(p, math.cos(a) * 27, math.sin(a) * 27)
        q["z"] = ground_at(q["x"], q["y"])
        shape(CUBE, mat["stone"], q, (2.0, 1.5, rng.uniform(3, 5)), "Orte/Gipfel", "Stein {}".format(i + 1), yaw=math.degrees(a), up=-0.5)
    # Nid-Quelle (Tränenkanal), versiegelt: nur ein Platzhalter
    shape(CYLINDER, mat["nid"], D["nid_quelle"], (5.0, 5.0, 0.6), "Orte/Traenenkanal", "Nid-Quelle (versiegelt)", up=-0.2, collision=False)
    # Weg zum Nackenstieg: Wegweiser
    shape(CUBE, mat["wood"], o["Zum Nackenstieg"], (0.3, 0.3, 3.0), "Orte/Nackenstieg", "Wegweiser zum Eisenschlund")


heights = None


def ground_at(x, y):
    fx = min(max((x - X0) / STEP, 0.0), NX - 1.001)
    fy = min(max((y - Y0) / STEP, 0.0), NY - 1.001)
    ix, iy = int(fx), int(fy)
    tx, ty = fx - ix, fy - iy

    def h(i, j):
        return (heights[j * NX + i] - 32768) / 128.0
    return (h(ix, iy) * (1 - tx) + h(ix + 1, iy) * tx) * (1 - ty) + (h(ix, iy + 1) * (1 - tx) + h(ix + 1, iy + 1) * tx) * ty


def bridge(mat):
    """Seilbrücke über die Lücke, von der Schwanzspitze (Wurzelforst) zum Kiefer"""
    import array
    wf = json.load(open(os.path.join(DATA, "WF_Welt.json"), encoding="utf-8"))
    raw = array.array("H")
    raw.frombytes(open(os.path.join(DATA, "WF_Welt_Hoehen.r16"), "rb").read())
    wnx = wf["punkte"][0]

    def wf_ground(x, y):
        ix = int(round((x * M - wf["ursprung_cm"][0]) / wf["schritt_cm"]))
        iy = int(round((y * M - wf["ursprung_cm"][1]) / wf["schritt_cm"]))
        return (raw[iy * wnx + ix] - 32768) / 128.0
    a, b = D["bruecke"]["von"], D["bruecke"]["nach"]
    a = {"x": a["x"], "y": a["y"], "z": wf_ground(a["x"], a["y"]) + 0.3}
    b = {"x": b["x"], "y": b["y"], "z": wf_ground(b["x"], b["y"]) + 0.3}
    length = math.hypot(b["x"] - a["x"], b["y"] - a["y"])
    yaw = math.degrees(math.atan2(b["y"] - a["y"], b["x"] - a["x"]))
    pitch = math.degrees(math.atan2(b["z"] - a["z"], length))
    mid = {"x": (a["x"] + b["x"]) * 0.5, "y": (a["y"] + b["y"]) * 0.5, "z": (a["z"] + b["z"]) * 0.5}
    deck = shape(CUBE, mat["wood"], mid, (length + 4.0, 3.0, 0.3), "Lücke/Seilbrücke", "Brückensteg", yaw=yaw, up=-0.15, pitch=pitch)
    for side in (-1.6, 1.6):
        rope = shape(CYLINDER, mat["rope"], mid, (0.12, 0.12, length + 4.0), "Lücke/Seilbrücke", "Seil", yaw=yaw, up=1.0, collision=False)
        rope.set_actor_rotation(unreal.Rotator(0.0, 90.0 - pitch, yaw), False)
        off = unreal.Vector(-math.sin(math.radians(yaw)) * side * M, math.cos(math.radians(yaw)) * side * M, 0.0)
        rope.set_actor_location(rope.get_actor_location() + off, False, False)
        for end in (a, b):
            post = near(end, -math.sin(math.radians(yaw)) * side, math.cos(math.radians(yaw)) * side, -0.3)
            shape(CYLINDER, mat["wood"], post, (0.4, 0.4, 2.0), "Lücke/Seilbrücke", "Pfosten", up=-0.5)
    # Der Steg ist begehbar, aber nicht der Rand: links und rechts unsichtbare Geländer
    for side in (-1.7, 1.7):
        rail = shape(CUBE, mat["wood"], mid, (length + 4.0, 0.2, 1.5), "Lücke/Seilbrücke", "Geländer (unsichtbar)", yaw=yaw, up=0.0, pitch=pitch)
        off = unreal.Vector(-math.sin(math.radians(yaw)) * side * M, math.cos(math.radians(yaw)) * side * M, 0.0)
        rail.set_actor_location(rail.get_actor_location() + off, False, False)
        rail.static_mesh_component.set_editor_property("visible", False)
        rail.set_actor_hidden_in_game(True)
    log("Seilbrücke {:.0f} m lang, {:.1f} m -> {:.1f} m".format(length, a["z"], b["z"]))
    return deck


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
    global heights
    import array
    heights = array.array("H")
    heights.frombytes(open(os.path.join(DATA, "SZ_Hoehen.r16"), "rb").read())

    assets.make_directory(ASSETS)
    ground = ground_material()
    mat = {
        "stone": shape_material("MI_Blockout_Stone", "5a554f"), "tent": shape_material("MI_Blockout_Tent", "6b5a44"),
        "order": shape_material("MI_Blockout_Order", "4d4651"), "academy": shape_material("MI_Blockout_Akademie", "8a8f9a"),
        "roof": shape_material("MI_Blockout_Dach_Blau", "3d5a7a"), "metal": shape_material("MI_Blockout_Metall", "9aa0a6"),
        "tooth": shape_material("MI_Blockout_Zahn", "d8d0bf"), "glass": shape_material("MI_Blockout_Blitzglas", "1d2433"),
        "bell": shape_material("MI_Blockout_Sturmglocke", "7aa3c8"), "ice": shape_material("MI_Blockout_Frostspat", "bfe3ff"),
        "nest": shape_material("MI_Blockout_Horst", "5a4630"), "nid": shape_material("MI_Blockout_Nid", "a24dff"),
        "wood": shape_material("MI_Blockout_Holz", "6a4e33"), "rope": shape_material("MI_Blockout_Seil", "b59a6a"),
        "water": shape_material("MI_Blockout_Water", "2c4a5c"),
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
    old = [a for a in level_actors if unreal.Name(TAG) in a.get_editor_property("tags")
           or (isinstance(a, unreal.LandscapeProxy) and a.get_actor_location().y >= Y0 * M - 10.0)]
    for actor in old:
        editor.destroy_actor(actor)
    log("{} Actors des letzten Laufs entfernt".format(len(old)))

    land = unreal.VaelWorldBuilder.create_landscape_from_raw(world, os.path.join(DATA, "SZ_Hoehen.r16"), NX, NY,
                                                             unreal.Vector(X0 * M, Y0 * M, 0.0), unreal.Vector(STEP * M, STEP * M, 100.0), ground, 2)
    if land is None:
        log("die Landschaft konnte nicht angelegt werden")
        return
    land.set_actor_label("Sturmzinnen Gelaende")
    land.set_folder_path(FOLDER)
    land.set_editor_property("tags", [unreal.Name(TAG)])

    # Kratersee im Auge
    sea_mat = unreal.load_asset("/Game/VFX/Water/Materials/MI_Water_Meer") or mat["water"]
    eye = D["auge"]
    lake = shape(PLANE, sea_mat, {"x": eye["x"], "y": eye["y"], "z": eye["spiegel"]}, (2.0 * eye["radius"] + 30.0, 2.0 * eye["radius"] + 30.0, 1.0),
                 "Orte/Auge", "Kratersee", collision=False)
    lake.set_actor_location(unreal.Vector(eye["x"] * M, eye["y"] * M, eye["spiegel"] * M), False, False)
    lake.static_mesh_component.set_editor_property("cast_shadow", False)
    area = spawn(unreal.VaelGroundArea, unreal.Vector(eye["x"] * M, eye["y"] * M, eye["spiegel"] * M), "Quellen/Wasser", "Kratersee (Wasser)")
    area.set_editor_property("element", unreal.VaelElement.WATER)
    area.set_editor_property("radius", eye["radius"] * M)
    area.set_editor_property("extinguishable", False)

    for name, p in D["lager"].items():
        camp(p, name, mat)
    places(mat)
    bridge(mat)

    # Elementquellen: Wasser an der Küste, Fels für Erde (Feuer an den Lagern, Luft bringt das Wetter)
    for i, p in enumerate(D["quellen"]["Wasser"][:2]):
        spring = spawn(unreal.VaelGroundArea, at(p, 0.1), "Quellen/Wasser", "Brandungsbecken {}".format(i + 1))
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

    # Region: windig und stürmisch; Schnee und Blizzard nach Verderbnis kommen mit dem neuen Wetter
    W = unreal.VaelWeather
    region = spawn(unreal.VaelRegion, unreal.Vector((X0 + NORTH) * 50.0, (Y0 + EAST) * 50.0, 0.0), "Spiel", "Region Sturmzinnen", streamed=False)
    region.set_editor_property("region_name", unreal.Text("Sturmzinnen"))
    region.set_editor_property("start_weather", W.WIND)
    region.set_editor_property("available_weathers", [W.CLEAR, W.WIND, W.RAIN, W.SNOW, W.STORM, W.BLIZZARD])
    # Rein: meist klar, kalter Wind, mal Regen oder leichter Schnee; verdorben: Sturm mit Gewitter, Schnee, Blizzard
    region.set_editor_property("weather_weights_pure", {W.CLEAR: 3.0, W.WIND: 2.0, W.RAIN: 1.0, W.SNOW: 1.0, W.STORM: 0.3, W.BLIZZARD: 0.0})
    region.set_editor_property("weather_weights_corrupted", {W.CLEAR: 0.1, W.WIND: 0.6, W.RAIN: 0.8, W.SNOW: 1.5, W.STORM: 2.2, W.BLIZZARD: 3.0})
    region.set_editor_property("snow_line_collection", snow_collection())
    region.get_component_by_class(unreal.BoxComponent).set_box_extent(unreal.Vector((NORTH - X0) * 50.0, (EAST - Y0) * 50.0, 60000.0))

    # Die Hörner: immer eine Stufe kälter, gleiche Verderbnis wie die Sturmzinnen
    hs, hn, hw, he = 4150.0, NORTH, 4700.0, 6150.0
    horns = spawn(unreal.VaelRegion, unreal.Vector((hs + hn) * 50.0, (hw + he) * 50.0, 0.0), "Spiel", "Region Hoerner", streamed=False)
    horns.set_editor_property("region_name", unreal.Text("Hörner"))
    horns.set_editor_property("priority", 1)
    horns.set_editor_property("corruption_from", region)
    horns.set_editor_property("start_weather", W.SNOW)
    horns.set_editor_property("available_weathers", [W.CLEAR, W.WIND, W.SNOW, W.BLIZZARD])
    horns.set_editor_property("weather_weights_pure", {W.CLEAR: 1.0, W.WIND: 1.5, W.SNOW: 2.5, W.BLIZZARD: 0.2})
    horns.set_editor_property("weather_weights_corrupted", {W.CLEAR: 0.0, W.WIND: 0.3, W.SNOW: 1.2, W.BLIZZARD: 4.0})
    horns.get_component_by_class(unreal.BoxComponent).set_box_extent(unreal.Vector((hn - hs) * 50.0, (he - hw) * 50.0, 60000.0))

    n_walls = walls()

    # Lauftest: über die Brücke und den Kiefer zum Hauptweg, dann bis zum Gipfel (Punkte ab 200)
    route = [D["bruecke"]["von"], D["bruecke"]["nach"], {"x": 3810.0, "y": 3300.0}]
    last = None
    for x, y, z in next(w for w in D["wege"] if w["name"] == "Hauptweg")["points"]:
        if last is None or math.hypot(x - last[0], y - last[1]) >= 150.0:
            route.append({"x": x, "y": y})
            last = (x, y)
    for number, p in enumerate(route, start=200):
        z = ground_at(p["x"], p["y"]) if p["y"] >= Y0 else 40.0
        point = spawn(unreal.TargetPoint, unreal.Vector(p["x"] * M, p["y"] * M, (z + 1.0) * M), "Spiel/Route", "Route {}".format(number), streamed=False)
        point.set_editor_property("tags", [unreal.Name("VaelRoute"), unreal.Name(str(number)), unreal.Name(TAG)])

    # Ein Meer für alle Karten: das Meer der Aschenmark wächst bis über die Sturmzinnen (nie kleiner)
    for actor in level_actors:
        if actor.get_actor_label() == "Meer" and isinstance(actor, unreal.StaticMeshActor):
            l, s = actor.get_actor_location(), actor.get_actor_scale3d()
            south, north = min(l.x - s.x * 50.0, -60000.0), max(l.x + s.x * 50.0, (NORTH + 600.0) * M)
            west, east = min(l.y - s.y * 50.0, -60000.0), max(l.y + s.y * 50.0, (EAST + 600.0) * M)
            actor.set_actor_location(unreal.Vector((south + north) * 0.5, (west + east) * 0.5, l.z), False, True)
            actor.set_actor_scale3d(unreal.Vector((north - south) / 100.0, (east - west) / 100.0, 1.0))

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("fertig: {} Küstenwände, {} Routenpunkte, gesetzt {}".format(n_walls, len(route), counts))


main()
