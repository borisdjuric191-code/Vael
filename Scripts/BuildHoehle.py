# Baut eine Höhle des Eisenschlunds als eigenes Innenlevel aus einem Grundriss (Boris 15.11.2026: viele Höhlen, verschiedene;
# erste ist die Gildenmine). Grundrisse liegen in Scripts/Data/Hoehlen/<Name>.json: ein Raster aus Zeichen, 4 m je Zelle, oben
# ist Norden (+X). Teile aus Tools/Welt/hoehlen_baukasten.py (Vael_Quellen/Hoehlen/export).
#
# - Boden je Bodenzelle; an jeder Kante zum Fels eine Felswand, auf der Kameraseite (Süden, Westen) niedrig, damit sie die Figur
#   nicht verdeckt; Felspfeiler an den Ecken
# - je nach Zeichen: Gleis, Türstöcke im engen Stollen, Loren, Kisten, Balken, Laternen, Lichtschacht, Erzstellen (Platzhalter),
#   Plätze für Gegner (Zielpunkte, später), eingestürztes Geröll
# - Innenraum (AVaelIndoorLevel: keine Sonne, kein Wetter), Spielerstart, Ausgang (AVaelCaveEntrance) zurück in die Welt
# - in L_Vael: der Eingang an der Stelle aus "welt_eingang_m" (Tag VaelHoehle_<Name>), der in diese Höhle führt
#
# Mit geschlossenem Editor:
#   set VAEL_HOEHLE=Gildenmine
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildHoehle.py" -AllowCommandletRendering -unattended -nosplash
# VAEL_HOEHLE_OHNE_WELT=1 baut nur die Höhle (L_Vael bleibt unberührt).

import json
import math
import os
import random

import unreal

NAME = os.environ.get("VAEL_HOEHLE", "Gildenmine")
DATA = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", "Hoehlen")
SOURCE = "C:/Projekte/Vael_Quellen/Hoehlen/export"
PARTS = "/Game/Vael/Maps/Hoehlen/Teile"
LEVEL = "/Game/Vael/Maps/Hoehlen/L_" + NAME
WORLD = "/Game/Vael/Maps/L_Vael"
KIT = "/Game/Vael/Refugium/Glutkammer/Probe"
M = 100.0
CAMERA = (-0.7071, -0.7071)       # von hier schaut die Spielkamera (Süd-West); Wände dorthin bleiben niedrig

ROCK_PIECES = ["SM_HK_Wand_A", "SM_HK_Wand_B", "SM_HK_Wand_C", "SM_HK_Wand_Niedrig_A", "SM_HK_Wand_Niedrig_B",
               "SM_HK_Ecke_A", "SM_HK_Ecke_B", "SM_HK_Ecke_Niedrig", "SM_HK_Boden_A", "SM_HK_Boden_B",
               "SM_HK_Tropfstein_A", "SM_HK_Tropfstein_B", "SM_HK_Tropfstein_C"]
PROPS = ["SM_HK_Verbau", "SM_HK_Verbau_8", "SM_HK_Verbau_12", "SM_HK_Schiene", "SM_HK_Lore", "SM_HK_Kiste", "SM_HK_Balkenstapel", "SM_HK_Laterne",
         "SM_HK_Salzspat", "SM_HK_Salzspat_Klein", "SM_HK_Schaedelwand_A", "SM_HK_Schaedelwand_B", "SM_HK_Knochenhaufen", "SM_HK_Schleim",
         "SM_HK_Schwamm", "SM_HK_Rippe", "SM_HK_Kerzen"]
KIT_SLOTS = {"Holz": "MI_GK_Holz", "Leder": "M_GK_Leder", "Metall": "M_GK_Metall", "Faden": "M_GK_Faden", "Glut": "M_GK_Glut",
             "Moos": "M_GK_Moos", "Messing": "/Game/Vael/Refugium/Glutkammer/Werkstatt/M_IW_Messing"}

# Wie die Höhlenarten aussehen (Farben des Felses, Licht); weitere Arten kommen dazu
ART = {
    "Gildenmine": {"grundfarbe": (0.9, 0.72, 0.55), "fels": (0.5, 0.45, 0.4), "boden": (0.36, 0.32, 0.29), "laterne": (1.0, 0.62, 0.32), "nebel": (0.09, 0.07, 0.055),
                   "grundlicht": 1.3, "erz": "rost", "licht": "laterne", "schutt": "kiesel"},
    # natürliche Höhle: unregelmäßige Wände, Tropfsteine, Wasser, kaltes Salzspat-Licht
    "Olmgrotte": {"grundfarbe": (0.55, 0.75, 0.95), "fels": (0.36, 0.4, 0.46), "boden": (0.27, 0.3, 0.33), "laterne": (0.55, 0.8, 1.0), "nebel": (0.03, 0.06, 0.08),
                  "grundlicht": 0.9, "erz": "salzspat", "natur": True, "kristall": (0.45, 0.8, 1.0),
                  "nebeldichte": 0.004, "volumennebel": False, "licht": "kristall", "schutt": "kiesel"},
    # Katakomben der Gilde: Kalkfels, Schädelreihen an den Wänden, Grabschleim in vielen Farben
    # schmale Spalten im schwarzen Fels, glühende Adern, Feuerquellen
    "FunkenerzAdern": {"grundfarbe": (1.0, 0.55, 0.3), "fels": (0.2, 0.18, 0.18), "boden": (0.16, 0.14, 0.13), "laterne": (1.0, 0.5, 0.2), "nebel": (0.08, 0.03, 0.01),
                       "grundlicht": 0.35, "natur": True, "schleim": ["Glut"], "erzfarbe": (1.0, 0.45, 0.1), "nebeldichte": 0.02,
                       "licht": "glut", "schutt": "glut"},
    # runde, gefressene Kammern, Kristallreste in vielen Farben, Brut
    "Ameisenbau": {"grundfarbe": (0.9, 0.76, 0.52), "fels": (0.55, 0.45, 0.32), "boden": (0.4, 0.33, 0.24), "laterne": (1.0, 0.7, 0.4), "nebel": (0.05, 0.04, 0.03),
                   "grundlicht": 0.8, "natur": True, "kristalle": [(1.0, 0.3, 0.6), (0.2, 0.9, 0.7), (0.4, 1.0, 0.2), (1.0, 0.7, 0.2), (0.5, 0.4, 1.0)],
                   "erzfarbe": (0.85, 0.5, 0.2), "nebeldichte": 0.015, "licht": "kristall", "schutt": "kristall"},
    # Irrgarten der Kobolde: dunkler Fels, falsches grünes Licht, der Hort voller Kobaltblüte
    "KoboldStollen": {"grundfarbe": (0.55, 0.78, 0.55), "fels": (0.36, 0.33, 0.36), "boden": (0.26, 0.24, 0.25), "laterne": (0.45, 1.0, 0.35), "nebel": (0.03, 0.05, 0.03),
                      "grundlicht": 1.4, "erzfarbe": (1.0, 0.35, 0.6), "nebeldichte": 0.01, "wandhoehe": 0.45,
                      "licht": "laterne", "schutt": "kiesel"},
    # Schwammwucherungen, Giftlachen, kränkliches Grün
    "Rostschwamm": {"grundfarbe": (0.62, 0.82, 0.45), "fels": (0.45, 0.3, 0.2), "boden": (0.3, 0.22, 0.15), "laterne": (0.6, 1.0, 0.3), "nebel": (0.04, 0.07, 0.02),
                    "grundlicht": 0.6, "natur": True, "wucherung": "schwamm", "schleim": ["Gift"], "erzfarbe": (0.4, 1.0, 0.3), "nebeldichte": 0.03,
                    "licht": "sporen", "schutt": "kiesel"},
    # Ringweg um den Abgrund, Rippen der Urdrachin, Rippenspat
    "TiefsterSchacht": {"grundfarbe": (0.68, 0.6, 0.9), "fels": (0.38, 0.36, 0.4), "boden": (0.28, 0.27, 0.3), "laterne": (0.8, 0.7, 1.0), "nebel": (0.04, 0.03, 0.06),
                        "grundlicht": 0.8, "natur": True, "erzfarbe": (1.0, 0.9, 0.7), "kristalle": [(0.8, 0.7, 1.0)], "abgrund": True,
                        "nebeldichte": 0.015, "licht": "kristall", "schutt": "kiesel", "rippen_auto": True},
    "Gebeinkammer": {"grundfarbe": (0.85, 0.74, 0.6), "fels": (0.6, 0.55, 0.47), "boden": (0.42, 0.38, 0.33), "laterne": (1.0, 0.7, 0.4), "nebel": (0.05, 0.045, 0.04),
                     "grundlicht": 1.1, "schaedelwand": 0.85, "nebeldichte": 0.015, "wandhoehe": 0.7, "licht": "kerzen", "schutt": "knochen"},
}

editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
PLANE = unreal.load_asset("/Engine/BasicShapes/Plane.Plane")
CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
BOULDERS = [unreal.load_asset("/Game/Vael/Maps/Eisenschlund/Fels/SM_ES_Brocken_" + k) for k in "ABC"]
NEST = unreal.load_asset("/Game/Vael/Refugium/Glutkammer/Probe/SM_GK_Eisack")
SPONGE = []
MITES = []          # eigene Tropfsteine des Baukastens (main); die Stalagmiten der Schluchten ließen im Innenlevel nichts mehr laden
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.MaterialEditingLibrary
rng = random.Random(1515)
counts = {}

D = json.load(open(os.path.join(DATA, NAME + ".json"), encoding="utf-8"))
GRID = D["grundriss"]
for _ch in os.environ.get("VAEL_HOEHLE_TEST_OHNE", ""):      # nur zum Eingrenzen von Fehlern: diese Zeichen werden Boden
    GRID = [row.replace(_ch, ".") for row in GRID]
H, W = len(GRID), max(len(r) for r in GRID)
S = D.get("zelle_m", 4.0) * M
STYLE = ART[D.get("art", "Gildenmine")]
NATUR = STYLE.get("natur", False) and not os.environ.get("VAEL_HOEHLE_TEST_GERADE")


def log(message):
    unreal.log_warning("[BuildHoehle] " + message)


def cell(r, c):
    if r < 0 or r >= H or c < 0 or c >= len(GRID[r]):
        return "#"
    return GRID[r][c]


def is_floor(r, c):
    return cell(r, c) != "#"


def centre(r, c):
    return ((H - 1 - r) * S, c * S)


DIRS = {"N": (-1, 0, (1, 0)), "S": (1, 0, (-1, 0)), "W": (0, -1, (0, -1)), "E": (0, 1, (0, 1))}


# ---------------------------------------------------------------- assets

def import_piece(name, collision_complex=False):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
    ui.static_mesh_import_data.set_editor_property("auto_generate_collision", True)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(SOURCE, name + ".fbx"))
    task.set_editor_property("destination_path", PARTS)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset("{}/{}".format(PARTS, name))
    if mesh is None:
        raise RuntimeError(name + " wurde nicht importiert")
    if collision_complex:
        body = mesh.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    return mesh


EXTRA_SLOTS = {}        # Kristall, Fels, Knochen, Dunkel: im Lauf angelegt (main)
# Grabschleim in seinen Farben (Lore-Bibel: grün ist der gewöhnliche, violett zieht zum Nid)
SCHLEIM = {"Gruen": (0.35, 1.0, 0.2), "Violett": (0.6, 0.2, 1.0), "Rot": (1.0, 0.12, 0.08), "Bernstein": (1.0, 0.6, 0.1), "Tuerkis": (0.1, 0.9, 0.8)}
# Lachen anderer Höhlen (Glut der Funkenerz-Adern, Gift des Rostschwamms): nur dort, nicht im Grabschleim der Gebeinkammer
LACHEN = {"Glut": (1.0, 0.35, 0.05), "Gift": (0.45, 0.9, 0.1)}
CRYSTAL_MATS = []
ORE_MAT = []
SLIME_MATS = {}


def kit_materials(mesh):
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.get_editor_property("material_slot_name")).split(".")[0]
        if slot_name in EXTRA_SLOTS:
            mesh.set_material(index, EXTRA_SLOTS[slot_name])
            continue
        path = KIT_SLOTS.get(slot_name)
        if path is None:
            continue
        material = unreal.load_asset(path if path.startswith("/") else "{}/{}".format(KIT, path))
        if material is not None:
            mesh.set_material(index, material)
    assets.save_loaded_asset(mesh, False)


def cave_rock_material():
    """M_HK_Fels: Fels nach Weltposition, dreifach projiziert, zwei Größen übereinander (die Wände sind nah an der Kamera, der
    Schluchtfels M_ES_Fels war dafür zu grob); zum Boden hin etwas dunkler. Parameter FelsFarbe"""
    path = PARTS + "/M_HK_Fels"
    if assets.does_asset_exist(path) and not os.environ.get("VAEL_HOEHLE_MATERIAL_NEU"):
        # bleibt stehen: neu gebaut müsste das Spiel seine Shader beim nächsten Start erst übersetzen, bis dahin fehlen Boden und Wände
        return unreal.load_asset(path)
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
    else:
        material = tools.create_asset("M_HK_Fels", PARTS, unreal.Material, unreal.MaterialFactoryNew())
    tex = unreal.load_asset("/Game/Rock_Collection_04/Meshes/Rock_01/Textures/T_Rock_01_A")
    rows = {}

    def node(cls, col, **props):
        rows[col] = rows.get(col, 0) + 1
        e = library.create_material_expression(material, cls, -280 * col, 130 * rows[col])
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(a, b, inp=""):
        library.connect_material_expressions(a, "", b, inp)

    def op(cls, col, a, b):
        e = node(cls, col)
        link(a, e, "A")
        link(b, e, "B")
        return e

    def const(v, col):
        return node(unreal.MaterialExpressionConstant, col, r=v)

    def mask(src, col, **ch):
        e = node(unreal.MaterialExpressionComponentMask, col, r=ch.get("r", False), g=ch.get("g", False), b=ch.get("b", False), a=False)
        link(src, e)
        return e

    def lerp(a, b, t, col):
        e = node(unreal.MaterialExpressionLinearInterpolate, col)
        link(a, e, "A")
        link(b, e, "B")
        link(t, e, "Alpha")
        return e

    world = node(unreal.MaterialExpressionWorldPosition, 16)
    normal = node(unreal.MaterialExpressionAbs, 14)
    link(node(unreal.MaterialExpressionVertexNormalWS, 15), normal)
    nx, nz = mask(normal, 13, r=True), mask(normal, 13, b=True)

    def triplanar(size, col):
        """Seiten (nach der Normalen gemischt) und Draufsicht einer Felsgröße"""
        pos = op(unreal.MaterialExpressionDivide, col + 3, world, const(size, col + 4))
        x, y, z = mask(pos, col + 2, r=True), mask(pos, col + 2, g=True), mask(pos, col + 2, b=True)

        def sample(u, v):
            uv = op(unreal.MaterialExpressionAppendVector, col + 1, u, v)
            t = node(unreal.MaterialExpressionTextureSample, col, texture=tex)
            link(uv, t, "UVs")
            return mask(t, col - 1, r=True, g=True, b=True)
        towards_x = node(unreal.MaterialExpressionSaturate, col - 2)
        link(op(unreal.MaterialExpressionMultiply, col - 1, nx, const(1.4, col)), towards_x)
        return lerp(sample(x, z), sample(y, z), towards_x, col - 3), sample(x, y)

    side_small, top_small = triplanar(240.0, 10)
    side_big, top_big = triplanar(900.0, 6)
    up = node(unreal.MaterialExpressionSaturate, 4)
    link(op(unreal.MaterialExpressionMultiply, 5, op(unreal.MaterialExpressionSubtract, 6, nz, const(0.6, 7)), const(3.0, 6)), up)
    small = lerp(side_small, top_small, up, 3)
    big = lerp(side_big, top_big, up, 3)
    rock = op(unreal.MaterialExpressionMultiply, 2, op(unreal.MaterialExpressionMultiply, 3, small, big), const(1.8, 3))
    tint = mask(node(unreal.MaterialExpressionVectorParameter, 3, parameter_name="FelsFarbe", default_value=unreal.LinearColor(0.5, 0.45, 0.4, 1.0)), 2,
                r=True, g=True, b=True)
    # unten dunkler: am Boden halb so hell, ab 5 m Höhe voll
    sat = node(unreal.MaterialExpressionSaturate, 2)
    link(op(unreal.MaterialExpressionDivide, 3, mask(world, 4, b=True), const(500.0, 4)), sat)
    shade = lerp(const(0.5, 2), const(1.0, 2), sat, 1)
    colour = op(unreal.MaterialExpressionMultiply, 1, op(unreal.MaterialExpressionMultiply, 2, rock, tint), shade)
    library.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(const(0.88, 1), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def simple_material(name, base, rough, emissive=None, specular=0.5):
    """Ein schlichtes Material: Farbe, Rauheit, auf Wunsch Leuchten (Wasser, Salzspat)"""
    path = "{}/{}".format(PARTS, name)
    if assets.does_asset_exist(path) and not os.environ.get("VAEL_HOEHLE_MATERIAL_NEU"):
        return unreal.load_asset(path)
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
    else:
        material = tools.create_asset(name, PARTS, unreal.Material, unreal.MaterialFactoryNew())

    def vec(c, row):
        e = library.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, row)
        e.set_editor_property("constant", unreal.LinearColor(c[0], c[1], c[2], 1.0))
        return e

    def scalar(v, row):
        e = library.create_material_expression(material, unreal.MaterialExpressionConstant, -400, row)
        e.set_editor_property("r", v)
        return e
    library.connect_material_property(vec(base, 0), "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(scalar(rough, 150), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(scalar(specular, 300), "", unreal.MaterialProperty.MP_SPECULAR)
    if emissive is not None:
        library.connect_material_property(vec(emissive, 450), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def rock_instance(name, colour):
    """Höhlenfels (M_HK_Fels) in der Farbe der Höhlenart"""
    path = "{}/{}".format(PARTS, name)
    instance = unreal.load_asset(path) if assets.does_asset_exist(path) else \
        tools.create_asset(name, PARTS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(instance, unreal.load_asset(PARTS + "/M_HK_Fels"))
    library.set_material_instance_vector_parameter_value(instance, "FelsFarbe", unreal.LinearColor(colour[0], colour[1], colour[2], 1.0))
    assets.save_loaded_asset(instance, False)
    return instance


# ---------------------------------------------------------------- placing

def spawn(cls, location, folder, label, rotation=None):
    actor = editor.spawn_actor_from_class(cls, location, rotation or unreal.Rotator(0.0, 0.0, 0.0))
    actor.set_folder_path(folder)
    actor.set_actor_label(label)
    key = folder.split("/")[0]
    counts[key] = counts.get(key, 0) + 1
    return actor


def mesh_actor(mesh, x, y, z, folder, label, yaw=0.0, scale=(1.0, 1.0, 1.0), material=None, collision=True, pitch=0.0, roll=0.0):
    actor = spawn(unreal.StaticMeshActor, unreal.Vector(x, y, z), folder, label, unreal.Rotator(roll, pitch, yaw))
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    if material is not None:
        for index in range(component.get_num_materials()):
            component.set_material(index, material)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    if not collision:
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return actor


SKULL_TURN = float(os.environ.get("VAEL_HOEHLE_SCHAEDEL_DREHUNG", "90"))   # die Schädelwand kam um 90° verdreht an
LIGHTS = []        # wo schon Licht ist (x, y), für die Lichtabdeckung


def point_light(x, y, z, folder, label, colour, intensity, radius, shadows=False):
    if intensity >= 25.0:
        LIGHTS.append((x, y))
    light = spawn(unreal.PointLight, unreal.Vector(x, y, z), folder, label)
    c = light.light_component
    c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    c.set_editor_property("intensity", intensity)
    c.set_light_color(unreal.LinearColor(colour[0], colour[1], colour[2], 1.0))
    c.set_editor_property("attenuation_radius", radius)
    c.set_editor_property("cast_shadows", shadows)
    # viele Lampen zusammen ließen den Volumennebel wie eine Wand leuchten (Olmgrotte): nur ein Hauch Streuung
    c.set_editor_property("volumetric_scattering_intensity", 0.15)
    light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    return light


def spot_down(x, y, height, folder, label, colour, intensity, cone, beam=None):
    light = spawn(unreal.SpotLight, unreal.Vector(x, y, height), folder, label, unreal.Rotator(0.0, -90.0, 0.0))
    c = light.light_component
    c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    c.set_editor_property("intensity", intensity)
    c.set_light_color(unreal.LinearColor(colour[0], colour[1], colour[2], 1.0))
    c.set_editor_property("inner_cone_angle", cone * 0.55)
    c.set_editor_property("outer_cone_angle", cone)
    c.set_editor_property("attenuation_radius", height * 1.8)
    c.set_editor_property("volumetric_scattering_intensity", 5.0)
    light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    if beam is not None:
        radius = height * math.tan(math.radians(cone))
        mesh_actor(beam, x, y, 0.0, folder, label + " Strahl", yaw=rng.uniform(0, 360), scale=(radius / 100.0, radius / 100.0, 6.0), collision=False)
    return light


def rail_dir(r, c):
    """Richtung des Gleises in einer Zelle: 'X' (Nord-Süd), 'Y' (Ost-West) oder beides"""
    rail = lambda a, b: cell(a, b) in "=W"
    along_x = rail(r - 1, c) or rail(r + 1, c)
    along_y = rail(r, c - 1) or rail(r, c + 1)
    return ("X" if along_x else "") + ("Y" if along_y else "") or "X"


def span_across(r, c, axis):
    """Zellen des Stollens quer zum Gleis (bis zum Fels, höchstens 4) und deren Mitte"""
    dr, dc = (0, 1) if axis == "X" else (1, 0)
    lo = 0
    while lo < 4 and is_floor(r - dr * (lo + 1), c - dc * (lo + 1)):
        lo += 1
    hi = 0
    while hi < 4 and is_floor(r + dr * (hi + 1), c + dc * (hi + 1)):
        hi += 1
    return lo + hi + 1, (hi - lo) * 0.5


def light_source(meshes, r, c, x, y):
    """Eine Lichtquelle, wie sie in die Höhle gehört, an einer dunklen Stelle (Lichtabdeckung)"""
    kind = STYLE.get("licht", "laterne")
    wall = None
    for key, (dr, dc, (dx, dy)) in DIRS.items():
        if not is_floor(r + dr, c + dc):
            wall = (dx, dy)
            break
    if kind == "laterne":
        dx, dy = wall or (1, 0)
        px, py = x + dx * (S / 2 - 70.0), y + dy * (S / 2 - 70.0)
        yaw = math.degrees(math.atan2(-dy, -dx))
        mesh_actor(meshes["SM_HK_Laterne"], px, py, 0.0, "Licht", "Laterne", yaw=yaw, collision=False)
        point_light(px + math.cos(math.radians(yaw)) * 45.0, py + math.sin(math.radians(yaw)) * 45.0, 210.0, "Licht", "Laternenschein",
                    STYLE["laterne"], 120.0, 1700.0, shadows=True)
    elif kind == "kerzen":
        dx, dy = wall or (0, 0)
        px, py = x + dx * (S / 2 - 90.0), y + dy * (S / 2 - 90.0)
        mesh_actor(meshes["SM_HK_Kerzen"], px, py, 0.0, "Licht", "Kerzen", yaw=rng.uniform(0, 360), scale=(1.3, 1.3, 1.3), collision=False)
        point_light(px, py, 90.0, "Licht", "Kerzenschein", (1.0, 0.68, 0.35), 70.0, 1400.0, shadows=True)
    elif kind == "kristall":
        colour, mat = STYLE.get("kristall", (0.45, 0.8, 1.0)), None
        if CRYSTAL_MATS:
            colour, mat = CRYSTAL_MATS[(r * 5 + c * 11) % len(CRYSTAL_MATS)]
        dx, dy = wall or (0, 0)
        crystal = mesh_actor(meshes["SM_HK_Salzspat"], x + dx * 120.0, y + dy * 120.0, 0.0, "Licht", "Kristall", yaw=rng.uniform(0, 360),
                             scale=(0.8, 0.8, 0.8))
        if mat is not None:
            crystal.static_mesh_component.set_material(0, mat)
        point_light(x + dx * 120.0, y + dy * 120.0, 150.0, "Licht", "Kristall-Licht", colour, 90.0, 1600.0, shadows=True)
    elif kind == "glut":
        if "Glut" in SLIME_MATS:
            mesh_actor(meshes["SM_HK_Schleim"], x, y, 2.0, "Licht", "Glutader", yaw=rng.uniform(0, 360), scale=(0.6, 0.6, 1.0),
                       material=SLIME_MATS["Glut"], collision=False)
        point_light(x, y, 120.0, "Licht", "Glutschein", (1.0, 0.45, 0.12), 80.0, 1500.0, shadows=True)
    elif kind == "sporen":
        for k in range(2):
            s_ = rng.uniform(0.6, 1.1)
            mesh_actor(meshes["SM_HK_Schwamm"], x + rng.uniform(-100, 100), y + rng.uniform(-100, 100), -5.0, "Licht", "Rostschwamm",
                       yaw=rng.uniform(0, 360), scale=(s_, s_, s_), material=SPONGE[0] if SPONGE else None)
        point_light(x, y, 110.0, "Licht", "Sporenlicht", (0.55, 1.0, 0.3), 40.0, 1400.0, shadows=True)


def build_cave(meshes, rock, floor_mat, ore_mat, water_mat=None, crystal_mat=None):
    if assets.does_asset_exist(LEVEL):
        assets.delete_asset(LEVEL)
    assets.make_directory("/Game/Vael/Maps/Hoehlen")
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    beam = unreal.load_asset(KIT + "/SM_GK_Lichtstrahl")
    boulders = [unreal.load_asset("/Game/Vael/Maps/Eisenschlund/Fels/SM_ES_Brocken_" + k) for k in "ABC"]
    walls = [meshes["SM_HK_Wand_A"], meshes["SM_HK_Wand_B"], meshes["SM_HK_Wand_C"]]
    lows = [meshes["SM_HK_Wand_Niedrig_A"], meshes["SM_HK_Wand_Niedrig_B"]]

    indoor = spawn(unreal.VaelIndoorLevel, unreal.Vector(0, 0, 500), "Spiel", "Innenraum")
    indoor.set_editor_property("place_name", unreal.Text(D.get("titel", NAME)))
    # ein Felssockel unter der ganzen Höhle: nichts fällt durch eine Lücke zwischen den Platten ins Leere
    mesh_actor(unreal.load_asset("/Engine/BasicShapes/Cube.Cube"), (H - 1) * S / 2, (W - 1) * S / 2, -4000.0 if STYLE.get("abgrund") else -370.0, "Boden", "Felssockel",
               scale=((H + 2) * S / 100.0, (W + 2) * S / 100.0, 1.0), material=floor_mat)

    for r in range(H):
        for c in range(W):
            ch = cell(r, c)
            if ch == "#":
                continue
            x, y = centre(r, c)
            if ch == "X":
                # Abgrund: kein Boden, eine unsichtbare Sperre am Rand
                block = mesh_actor(CUBE, x, y, 100.0, "Abgrund", "Abgrund (Sperre)", scale=(S / 100.0, S / 100.0, 6.0))
                block.set_actor_hidden_in_game(True)
                continue
            depth = {"~": -45.0, "O": -300.0}.get(ch, 0.0)
            mesh_actor(rng.choice([meshes["SM_HK_Boden_A"], meshes["SM_HK_Boden_B"]]), x, y, depth, "Boden", "Boden",
                       yaw=rng.choice((0, 90, 180, 270)), material=floor_mat)
            if ch in "~O" and water_mat is not None:
                # Wasserspiegel über der Zelle; knietief zum Durchwaten, tief nur zum Ansehen
                mesh_actor(PLANE, x, y, 10.0, "Wasser", "Wasser", scale=(S / 100.0 + 0.04, S / 100.0 + 0.04, 1.0), material=water_mat, collision=False)
                if ch == "O":
                    block = mesh_actor(CUBE, x, y, 100.0, "Wasser", "Tiefes Wasser (Sperre)", scale=(S / 100.0, S / 100.0, 6.0))
                    block.set_actor_hidden_in_game(True)
                elif r % 4 == 0 and c % 4 == 0:     # einzelne Quellen; jede zweite Zelle ergab eine Fläche aus blauen Kreisen
                    pool = spawn(unreal.VaelGroundArea, unreal.Vector(x, y, 20.0), "Wasser", "Wasserquelle")
                    pool.set_editor_property("element", unreal.VaelElement.WATER)
                    pool.set_editor_property("radius", 200.0)
                    pool.set_editor_property("extinguishable", False)
                    # die Platzhalter-Scheibe ausblenden: das Wasser sieht man ja (die blauen Kreise störten)
                    for disc_ in pool.get_components_by_class(unreal.StaticMeshComponent):
                        disc_.set_editor_property("visible", False)
            # Wände zu jedem Felsnachbarn
            for key, (dr, dc, (dx, dy)) in DIRS.items():
                if is_floor(r + dr, c + dc):
                    continue
                low = dx * CAMERA[0] + dy * CAMERA[1] > 0.3
                wild = 14.0 if NATUR else 4.0
                yaw = math.degrees(math.atan2(-dx, dy)) + rng.uniform(-wild, wild)
                shift = rng.uniform(-60.0, 60.0) if NATUR else 0.0
                mesh_actor(rng.choice(lows if low else walls), x + dx * (S / 2 + shift), y + dy * (S / 2 + shift), 0.0, "Waende",
                           "Felswand niedrig" if low else "Felswand", yaw=yaw,
                           scale=(rng.uniform(1.0, 1.25) if NATUR else 1.0, rng.uniform(0.85, 1.4 if NATUR else 1.15),
                                  rng.uniform(0.8, 1.35 if NATUR else 1.15) * STYLE.get("wandhoehe", 1.0) if not low else 1.0), material=rock)
                if not low and rng.random() < STYLE.get("schaedelwand", 0.0):
                    # Schädelreihen der Gilde vor dem Fels, mit dem Gesicht in den Gang
                    # vor die Felsfront (die ragt bis ~1,5 m in die Zelle), sonst steckten die Schädel im Fels
                    mesh_actor(rng.choice([meshes["SM_HK_Schaedelwand_A"], meshes["SM_HK_Schaedelwand_B"]]), x + dx * (S / 2 - 170.0),
                               y + dy * (S / 2 - 170.0), 0.0, "Gebeine", "Schaedelwand", yaw=math.degrees(math.atan2(-dx, dy)) + SKULL_TURN,
                               scale=(0.95, 1.0, rng.uniform(1.05, 1.25)))
                if NATUR and rng.random() < 0.45:
                    # Brocken am Fuß der Wand, gegen die geraden Linien des Rasters
                    sz = rng.uniform(0.9, 2.2)
                    t = rng.uniform(-0.4, 0.4) * S
                    mesh_actor(rng.choice(BOULDERS), x + dx * (S / 2 - 60.0) + dy * t, y + dy * (S / 2 - 60.0) - dx * t, -sz * 20.0, "Waende",
                               "Brocken am Wandfuss", yaw=rng.uniform(0, 360), scale=(sz, sz, sz * 0.8), material=rock)

    # Eckpfeiler, wo der Fels um eine Ecke geht
    for i in range(H + 1):
        for j in range(W + 1):
            around = [(i - 1, j - 1), (i - 1, j), (i, j - 1), (i, j)]
            floors = [p for p in around if is_floor(*p)]
            if not floors or len(floors) == 4:
                continue
            if len(floors) == 2 and (floors[0][0] == floors[1][0] or floors[0][1] == floors[1][1]):
                continue                                                   # gerade Wand, kein Eck
            rocks = [p for p in around if not is_floor(*p)]
            low = True
            for fr, fc in floors:
                for rr, rc in rocks:
                    if abs(fr - rr) + abs(fc - rc) == 1:
                        dx, dy = (-(rr - fr), rc - fc)
                        low = low and dx * CAMERA[0] + dy * CAMERA[1] > 0.3
            vx, vy = (H - 1 - i) * S + S / 2, j * S - S / 2
            rx = sum(centre(*p)[0] for p in rocks) / len(rocks) - vx
            ry = sum(centre(*p)[1] for p in rocks) / len(rocks) - vy
            length = math.hypot(rx, ry) or 1.0
            px, py = vx + rx / length * 80.0, vy + ry / length * 80.0
            piece = meshes["SM_HK_Ecke_Niedrig"] if low else rng.choice([meshes["SM_HK_Ecke_A"], meshes["SM_HK_Ecke_B"]])
            s = rng.uniform(0.85, 1.1)
            mesh_actor(piece, px, py, -20.0, "Waende", "Eckpfeiler", yaw=rng.uniform(0, 360),
                       scale=(s, s, rng.uniform(0.9, 1.1) * (1.0 if low else STYLE.get("wandhoehe", 1.0))), material=rock)

    # Zeichen des Grundrisses
    start = None
    for r in range(H):
        for c in range(W):
            ch = cell(r, c)
            x, y = centre(r, c)
            if ch in "=W":
                for axis in rail_dir(r, c):
                    mesh_actor(meshes["SM_HK_Schiene"], x, y, 0.0, "Gleis", "Gleis", yaw=0.0 if axis == "X" else 90.0, collision=False)
                    width, offset = span_across(r, c, axis)
                    step = r if axis == "X" else c
                    if width <= 3 and step % 3 == 0 and len(rail_dir(r, c)) == 1:
                        frame = {1: "SM_HK_Verbau", 2: "SM_HK_Verbau_8", 3: "SM_HK_Verbau_12"}[width]
                        fx, fy = (x, y + offset * S) if axis == "X" else (x - offset * S, y)
                        mesh_actor(meshes[frame], fx, fy, 0.0, "Verbau", "Tuerstock", yaw=0.0 if axis == "X" else 90.0)
                if ch == "W":
                    axis = rail_dir(r, c)[0]
                    mesh_actor(meshes["SM_HK_Lore"], x, y, 14.0, "Requisiten", "Lore", yaw=(0.0 if axis == "X" else 90.0) + rng.uniform(-3, 3))
            elif ch == "K":
                for k in range(rng.randint(2, 3)):
                    s = rng.uniform(0.8, 1.1)
                    mesh_actor(meshes["SM_HK_Kiste"], x + rng.uniform(-110, 110), y + rng.uniform(-110, 110), (100.0 * s if k == 2 else 0.0),
                               "Requisiten", "Kiste", yaw=rng.uniform(0, 90), scale=(s, s, s))
            elif ch == "B" and not STYLE.get("schaedelwand"):
                mesh_actor(meshes["SM_HK_Balkenstapel"], x, y, 0.0, "Requisiten", "Balkenstapel", yaw=rng.choice((0, 90)) + rng.uniform(-8, 8))
            elif ch == "L":
                # an die nächste Felswand, die Laterne hängt zur Mitte hin
                yaw, px, py = 0.0, x, y
                for key, (dr, dc, (dx, dy)) in DIRS.items():
                    if not is_floor(r + dr, c + dc):
                        px, py = x + dx * (S / 2 - 70.0), y + dy * (S / 2 - 70.0)
                        yaw = math.degrees(math.atan2(-dy, -dx))
                        break
                mesh_actor(meshes["SM_HK_Laterne"], px, py, 0.0, "Licht", "Grubenlaterne", yaw=yaw, collision=False)
                hx = px + math.cos(math.radians(yaw)) * 45.0
                hy = py + math.sin(math.radians(yaw)) * 45.0
                point_light(hx, hy, 210.0, "Licht", "Laternenschein", STYLE["laterne"], 140.0, 1900.0, shadows=True)   # Schatten zeigen das Relief der Wände
            elif ch == "S":
                spot_down(x, y, 1600.0, "Licht", "Wetterschacht", (0.85, 0.9, 1.0), 600.0, 8.0, beam)
            elif ch == "C" and crystal_mat is not None:
                yaw = rng.uniform(0, 360)
                s_ = rng.uniform(0.85, 1.25)
                colour, mat = STYLE.get("kristall", (0.45, 0.8, 1.0)), None
                if CRYSTAL_MATS:
                    colour, mat = CRYSTAL_MATS[(r * 5 + c * 11) % len(CRYSTAL_MATS)]
                crystal = mesh_actor(meshes["SM_HK_Salzspat"], x + rng.uniform(-80, 80), y + rng.uniform(-80, 80), 0.0, "Licht", "Kristall",
                                     yaw=yaw, scale=(s_, s_, s_))
                if mat is not None:
                    crystal.static_mesh_component.set_material(0, mat)
                point_light(x, y, 160.0, "Licht", "Kristall-Licht", colour, 110.0, 1700.0, shadows=True)
            elif ch == "F":
                fire = spawn(unreal.VaelFireSpot, unreal.Vector(x, y, 10.0), "Feuer", "Feuerquelle")
                if "Glut" in SLIME_MATS:
                    mesh_actor(meshes["SM_HK_Schleim"], x, y, 2.0, "Feuer", "Glutspalt", yaw=rng.uniform(0, 360), scale=(0.7, 0.7, 1.0),
                               material=SLIME_MATS["Glut"], collision=False)
                point_light(x, y, 150.0, "Feuer", "Feuerschein", (1.0, 0.5, 0.15), 160.0, 1800.0, shadows=True)
            elif ch == "N":
                for k in range(rng.randint(2, 4)):
                    s_ = rng.uniform(0.6, 0.95)
                    mesh_actor(NEST, x + rng.uniform(-120, 120), y + rng.uniform(-120, 120), 0.0, "Brut", "Brut der Schuerfameisen",
                               yaw=rng.uniform(0, 360), scale=(s_, s_, s_))
                point_light(x, y, 90.0, "Brut", "Brutschein", (1.0, 0.65, 0.3), 25.0, 700.0)
            elif ch == "R":
                cx = sum(centre(rr, cc)[0] for rr in range(H) for cc in range(W) if cell(rr, cc) == "X")
                cy = sum(centre(rr, cc)[1] for rr in range(H) for cc in range(W) if cell(rr, cc) == "X")
                n_ = sum(1 for rr in range(H) for cc in range(W) if cell(rr, cc) == "X") or 1
                yaw = math.degrees(math.atan2(cy / n_ - y, cx / n_ - x))
                # der Bogen steht mit einem Fuß hier und spannt sich über den Abgrund
                mesh_actor(meshes["SM_HK_Rippe"], x + math.cos(math.radians(yaw)) * 700.0, y + math.sin(math.radians(yaw)) * 700.0, -150.0,
                           "Rippen", "Rippe der Urdrachin", yaw=yaw, scale=(1.0, 1.0, rng.uniform(1.0, 1.4)), collision=False)
            elif ch == "B":
                s_ = rng.uniform(0.9, 1.4)
                mesh_actor(meshes["SM_HK_Knochenhaufen"], x + rng.uniform(-60, 60), y + rng.uniform(-60, 60), 0.0, "Gebeine", "Knochenhaufen",
                           yaw=rng.uniform(0, 360), scale=(s_, s_, s_))
            elif ch == "P" and SLIME_MATS:
                farbe = sorted(SLIME_MATS)[(r * 7 + c * 3) % len(SLIME_MATS)]
                s_ = rng.uniform(0.8, 1.3)
                mesh_actor(meshes["SM_HK_Schleim"], x, y, 2.0, "Schleim", "Lache " + farbe, yaw=rng.uniform(0, 360), scale=(s_, s_, 1.0),
                           material=SLIME_MATS[farbe], collision=False)
                col = SCHLEIM.get(farbe) or LACHEN[farbe]
                point_light(x, y, 120.0, "Schleim", "Schleimschein " + farbe, col, 70.0 if farbe in SCHLEIM else 30.0, 1100.0)
            elif ch == "M" and STYLE.get("wucherung") == "schwamm":
                for k in range(rng.randint(2, 4)):
                    s_ = rng.uniform(0.8, 2.0)
                    mesh_actor(meshes["SM_HK_Schwamm"], x + rng.uniform(-140, 140), y + rng.uniform(-140, 140), -5.0, "Schwamm", "Rostschwamm",
                               yaw=rng.uniform(0, 360), scale=(s_, s_, s_ * rng.uniform(0.8, 1.3)), material=SPONGE[0])
                if rng.random() < 0.3:
                    point_light(x, y, 80.0, "Schwamm", "Sporenlicht", (0.55, 1.0, 0.3), 6.0, 600.0)
            elif ch == "M":
                for k in range(rng.randint(3, 6)):
                    h_ = rng.uniform(1.2, 4.5)
                    mesh_actor(rng.choice(MITES), x + rng.uniform(-150, 150), y + rng.uniform(-150, 150), -20.0, "Tropfsteine", "Tropfstein",
                               yaw=rng.uniform(0, 360), scale=(h_ * 0.55, h_ * 0.55, h_), material=rock, pitch=rng.uniform(-6, 6))
            elif ch == "Z" and (STYLE.get("erz") == "salzspat" or ORE_MAT):
                for k in range(rng.randint(2, 3)):
                    s_ = rng.uniform(0.7, 1.1)
                    ore = mesh_actor(meshes["SM_HK_Salzspat_Klein"], x + rng.uniform(-90, 90), y + rng.uniform(-90, 90), 0.0, "Erz", "Erzstelle",
                                     yaw=rng.uniform(0, 360), scale=(s_, s_, s_))
                    if ORE_MAT:
                        ore.static_mesh_component.set_material(0, ORE_MAT[0])
                if ORE_MAT:
                    point_light(x, y, 90.0, "Erz", "Erzglanz", STYLE["erzfarbe"], 25.0, 600.0)
                marker = spawn(unreal.TargetPoint, unreal.Vector(x, y, 50.0), "Erz", "Erzstelle (Platzhalter)")
                marker.set_editor_property("tags", [unreal.Name("VaelErzstelle")])
            elif ch == "Z":
                for k in range(rng.randint(2, 4)):
                    s = rng.uniform(0.35, 0.6)
                    mesh_actor(meshes["SM_HK_Ecke_Niedrig"], x + rng.uniform(-80, 80), y + rng.uniform(-80, 80), -10.0, "Erz", "Erzstelle",
                               yaw=rng.uniform(0, 360), scale=(s, s, s * rng.uniform(0.6, 1.0)), material=ore_mat)
                point_light(x, y, 120.0, "Erz", "Erzglanz", (1.0, 0.45, 0.2), 6.0, 450.0)
                marker = spawn(unreal.TargetPoint, unreal.Vector(x, y, 50.0), "Erz", "Erzstelle (Platzhalter)")
                marker.set_editor_property("tags", [unreal.Name("VaelErzstelle")])
            elif ch == "G":
                marker = spawn(unreal.TargetPoint, unreal.Vector(x, y, 50.0), "Gegner", "Platz fuer Gegner")
                marker.set_editor_property("tags", [unreal.Name("VaelHoehleGegner")])
            elif ch == "T":
                for k in range(rng.randint(5, 8)):
                    s = rng.uniform(1.2, 3.2)
                    mesh_actor(rng.choice(boulders), x + rng.uniform(-170, 170), y + rng.uniform(-170, 170), -s * 15.0, "Geroell", "Brocken",
                               yaw=rng.uniform(0, 360), scale=(s, s * rng.uniform(0.8, 1.1), s * rng.uniform(0.7, 1.0)), material=rock,
                               pitch=rng.uniform(-10, 10), roll=rng.uniform(-10, 10))
            elif ch == "E":
                exit_ = spawn(unreal.VaelCaveEntrance, unreal.Vector(x, y, 120.0), "Spiel", "Ausgang")
                exit_.set_editor_property("target_level", unreal.Name("L_Vael"))
                ex, ey = D["welt_eingang_m"][:2]
                exit_.set_editor_property("return_place", unreal.Vector2D((ex + 7.0) * M, ey * M))
                exit_.set_editor_property("message", unreal.Text("Eisenschlund"))
                spot_down(x, y, 1400.0, "Licht", "Tageslicht am Ausgang", (0.8, 0.88, 1.0), 900.0, 12.0, beam)
                start = (x + 3 * S, y)
    # --- Schacht: Rippen rundum am Rand des Abgrunds, nicht nur an den gezeichneten Stellen
    if STYLE.get("rippen_auto"):
        xs = [centre(rr, cc) for rr in range(H) for cc in range(W) if cell(rr, cc) == "X"]
        cx_, cy_ = sum(p[0] for p in xs) / len(xs), sum(p[1] for p in xs) / len(xs)
        placed = []
        for r in range(H):
            for c in range(W):
                if cell(r, c) != "." or not any(cell(r + dr, c + dc) == "X" for dr, dc, _ in DIRS.values()):
                    continue
                x, y = centre(r, c)
                if any(math.hypot(x - px, y - py) < 3.2 * S for px, py in placed):
                    continue
                placed.append((x, y))
                yaw = math.degrees(math.atan2(cy_ - y, cx_ - x)) + rng.uniform(-12, 12)
                mesh_actor(meshes["SM_HK_Rippe"], x + math.cos(math.radians(yaw)) * 650.0, y + math.sin(math.radians(yaw)) * 650.0, -250.0,
                           "Rippen", "Rippe der Urdrachin", yaw=yaw, scale=(rng.uniform(0.9, 1.15), 1.0, rng.uniform(1.0, 1.5)), collision=False)

    # --- Lichtabdeckung: keine Stelle weiter als ~16 m vom nächsten Licht; dort eine Lichtquelle, wie sie in diese Höhle gehört
    cover = STYLE.get("licht_abstand", 4.0) * S
    for r in range(H):
        for c in range(W):
            if cell(r, c) not in ".~":
                continue
            x, y = centre(r, c)
            if any(math.hypot(x - lx, y - ly) < cover for lx, ly in LIGHTS):
                continue
            light_source(meshes, r, c, x, y)

    # --- Bodenschutt: Kiesel und Brocken, je Höhle Knochen, Kristallsplitter oder Glutbrocken
    for r in range(H):
        for c in range(W):
            if cell(r, c) not in "." or rng.random() > 0.45:
                continue
            x, y = centre(r, c)
            for k in range(rng.randint(1, 3)):
                px, py = x + rng.uniform(-180, 180), y + rng.uniform(-180, 180)
                kind = STYLE.get("schutt", "kiesel")
                if kind == "knochen" and rng.random() < 0.5:
                    s_ = rng.uniform(0.3, 0.5)
                    mesh_actor(meshes["SM_HK_Knochenhaufen"], px, py, -2.0, "Schutt", "Gebeine", yaw=rng.uniform(0, 360), scale=(s_, s_, s_),
                               collision=False)
                elif kind == "kristall" and rng.random() < 0.35 and CRYSTAL_MATS:
                    s_ = rng.uniform(0.25, 0.45)
                    shard = mesh_actor(meshes["SM_HK_Salzspat_Klein"], px, py, -2.0, "Schutt", "Kristallsplitter", yaw=rng.uniform(0, 360),
                                       scale=(s_, s_, s_), collision=False)
                    shard.static_mesh_component.set_material(0, CRYSTAL_MATS[rng.randrange(len(CRYSTAL_MATS))][1])
                elif kind == "glut" and rng.random() < 0.3 and ORE_MAT:
                    s_ = rng.uniform(0.12, 0.25)
                    ember = mesh_actor(rng.choice(BOULDERS), px, py, -3.0, "Schutt", "Glutbrocken", yaw=rng.uniform(0, 360), scale=(s_, s_, s_ * 0.7),
                                       material=ORE_MAT[0], collision=False)
                else:
                    s_ = rng.uniform(0.12, 0.4)
                    mesh_actor(rng.choice(BOULDERS), px, py, -s_ * 12.0, "Schutt", "Kiesel", yaw=rng.uniform(0, 360), scale=(s_, s_ * rng.uniform(0.7, 1.1), s_ * 0.7),
                               material=rock, collision=False)

    if start is not None:
        spawn(unreal.PlayerStart, unreal.Vector(start[0], start[1], 120.0), "Spiel", "Spielerstart")

    if STYLE.get("abgrund"):
        xs = [centre(r, c) for r in range(H) for c in range(W) if cell(r, c) == "X"]
        if xs:
            ax_, ay_ = sum(p[0] for p in xs) / len(xs), sum(p[1] for p in xs) / len(xs)
            point_light(ax_, ay_, -2500.0, "Abgrund", "Leuchten tief unten", (0.6, 0.25, 1.0), 4000.0, 6000.0)
    # Grundlicht: ein schwaches, kühles Licht schräg von oben (die Höhle ist für die Kamera oben offen), das die Formen zeigt;
    # die Laternen und Schächte bleiben das Helle
    moon = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 3000.0), "Licht", "Grundlicht von oben", unreal.Rotator(0.0, -62.0, 35.0))
    mc = moon.light_component
    mc.set_editor_property("intensity", STYLE.get("grundlicht", 0.35) * 1.6)
    g = STYLE.get("grundfarbe", (0.62, 0.7, 0.85))           # jede Höhle in ihrer Farbe (grau war es ohne Stimmung)
    mc.set_light_color(unreal.LinearColor(g[0], g[1], g[2], 1.0))
    mc.set_editor_property("cast_shadows", True)
    moon.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    # Licht und Luft: kein Himmel; ein schwaches Grundlicht im Nebel, damit das Dunkel nicht schwarz ist
    fog = spawn(unreal.ExponentialHeightFog, unreal.Vector(H * S / 2, W * S / 2, 0.0), "Licht", "Grubenluft")
    f = fog.component
    f.set_editor_property("fog_density", STYLE.get("nebeldichte", 0.03))
    f.set_editor_property("fog_height_falloff", 0.35)
    n = STYLE["nebel"]
    f.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(n[0], n[1], n[2], 1.0))
    f.set_volumetric_fog(STYLE.get("volumennebel", True))
    post = spawn(unreal.PostProcessVolume, unreal.Vector(H * S / 2, W * S / 2, 0.0), "Licht", "Nachbearbeitung")
    post.set_editor_property("unbound", True)
    settings = post.get_editor_property("settings")
    for key, value in (("auto_exposure_min_brightness", 0.0), ("auto_exposure_max_brightness", 0.0), ("auto_exposure_bias", 1.9),
                       ("bloom_intensity", 0.9), ("vignette_intensity", 0.55)):
        settings.set_editor_property("override_" + key, True)
        settings.set_editor_property(key, value)
    post.set_editor_property("settings", settings)

    if unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL):
        log("gespeichert: " + LEVEL)
    else:
        unreal.log_error("[BuildHoehle] " + LEVEL + " konnte nicht gespeichert werden")


def build_map():
    """Die Karte der Höhle (Tools/Welt/hoehlen_karte.py) als /Game/Vael/UI/Karten/DA_Karte_<Name>: die Minikarte findet sie über
    den Namen des Levels; mit Nebel, die Höhle zeigt sich erst beim Erkunden"""
    info = D.get("karte")
    if not info:
        log("keine Karte im Grundriss (erst py Tools/Welt/hoehlen_karte.py)")
        return
    folder = "/Game/Vael/UI/Karten"
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(DATA, info["bild"]))
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("destination_name", "T_Karte_" + NAME)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    texture = unreal.load_asset("{}/T_Karte_{}".format(folder, NAME))
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("never_stream", True)
    assets.save_loaded_asset(texture, False)
    path = "{}/DA_Karte_{}".format(folder, NAME)
    world_map = unreal.load_asset(path) if assets.does_asset_exist(path) else         tools.create_asset("DA_Karte_" + NAME, folder, unreal.VaelWorldMap, unreal.DataAssetFactory())
    world_map.set_editor_property("title", unreal.Text(D.get("titel", NAME)))
    world_map.set_editor_property("picture", texture)
    world_map.set_editor_property("world_min", unreal.Vector2D(info["welt_min"][0], info["welt_min"][1]))
    world_map.set_editor_property("world_size", float(info["welt_groesse"]))
    world_map.set_editor_property("focus_min", unreal.Vector2D(info["fokus_min"][0], info["fokus_min"][1]))
    world_map.set_editor_property("focus_max", unreal.Vector2D(info["fokus_max"][0], info["fokus_max"][1]))
    labels = []
    for entry in D.get("karte_marken", []):
        label = unreal.VaelMapLabel()
        label.set_editor_property("name", unreal.Text(entry["name"]))
        label.set_editor_property("location", unreal.Vector2D(entry["welt"][0], entry["welt"][1]))
        label.set_editor_property("size", int(entry["size"]))
        labels.append(label)
    world_map.set_editor_property("labels", labels)
    world_map.set_editor_property("fog_of_war", True)
    assets.save_loaded_asset(world_map, False)
    log("Karte " + path)


def build_world_entrance():
    """In L_Vael: der Eingang, der in diese Höhle führt (ersetzt den vom letzten Lauf)"""
    if not levels.load_level(WORLD):
        log("konnte " + WORLD + " nicht öffnen")
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    tag = unreal.Name("VaelHoehle_" + NAME)
    try:
        descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
        unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
    except Exception as error:  # noqa: BLE001
        log("konnte nicht alle Actors laden: {}".format(error))
    for actor in editor.get_all_level_actors():
        if tag in actor.get_editor_property("tags"):
            editor.destroy_actor(actor)
    ex, ey, ground = D["welt_eingang_m"]
    z = ground * M        # die Bodenhöhe aus den Ortsdaten (Abtasten von oben traf den Felssturz über dem Portal)
    mouth = editor.spawn_actor_from_class(unreal.VaelCaveEntrance, unreal.Vector(ex * M - 80.0, ey * M, z + 120.0), unreal.Rotator(0, 0, 0))
    mouth.set_actor_label("Eingang " + D.get("titel", NAME))
    mouth.set_folder_path("Eisenschlund/Hoehlen/" + NAME)
    mouth.set_editor_property("tags", [tag])
    mouth.set_editor_property("target_level", unreal.Name("L_" + NAME))
    mouth.set_editor_property("message", unreal.Text(D.get("titel", NAME)))
    if D.get("portal_bauen"):
        # an diesem Ort stand noch kein Stollenportal (BuildEisenschlund.py baut nur einige): zwei Felspfeiler, ein Sturz, Dunkel dahinter
        cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
        stone = unreal.load_asset("/Game/Vael/Maps/Blockout/MI_Blockout_Stone")
        dark = unreal.load_asset("/Game/Vael/Maps/Blockout/MI_Blockout_Dunkel")
        for label, (px, py, pz), size, mat, solid in (("Pfeiler", (ex * M, ey * M - 350.0, z + 300.0), (2.0, 1.6, 7.0), stone, True),
                                                      ("Pfeiler", (ex * M, ey * M + 350.0, z + 300.0), (2.0, 1.6, 7.0), stone, True),
                                                      ("Sturz", (ex * M, ey * M, z + 650.0), (2.4, 9.0, 1.6), stone, True),
                                                      ("Dunkel", (ex * M - 150.0, ey * M, z + 300.0), (0.4, 5.6, 6.2), dark, False)):
            part = editor.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(px, py, pz), unreal.Rotator(0, 0, 0))
            part.static_mesh_component.set_static_mesh(cube)
            part.static_mesh_component.set_material(0, mat)
            part.set_actor_scale3d(unreal.Vector(*size))
            if not solid:
                part.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            part.set_actor_label(label)
            part.set_folder_path("Eisenschlund/Hoehlen/" + NAME)
            part.set_editor_property("tags", [tag])
    unreal.EditorLoadingAndSavingUtils.save_map(world, WORLD)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("Eingang in L_Vael bei {:.0f} / {:.0f} m, Boden {:.1f} m".format(ex, ey, z / M))


def main():
    assets.make_directory(PARTS)
    meshes = {}
    for name in ROCK_PIECES:
        meshes[name] = import_piece(name, collision_complex=("Wand" in name or "Boden" in name or "Ecke" in name))
        assets.save_loaded_asset(meshes[name], False)
    MITES[:] = [meshes["SM_HK_Tropfstein_" + k] for k in "ABC"]
    cave_rock_material()
    rock = rock_instance("MI_HK_Fels_" + D.get("art", NAME), STYLE["fels"])
    # der Höhlenfels dunkelt zum Boden hin auf die Hälfte ab: der Boden bekommt dafür eine hellere Farbe (Verschönern, 15.11.)
    floor_mat = rock_instance("MI_HK_Boden_" + D.get("art", NAME), tuple(min(1.0, v * 1.9) for v in STYLE["boden"]))
    ore_mat = rock_instance("MI_HK_Erz_Platzhalter", (0.75, 0.32, 0.16))
    water_mat = simple_material("M_HK_Grottenwasser_Matt", (0.006, 0.02, 0.025), 0.3, specular=0.4)   # glatt spiegelte es den hellen Nebel
    glow = STYLE.get("kristall", (0.45, 0.8, 1.0))
    crystal_mat = simple_material("M_HK_Salzspat", (0.7, 0.85, 0.95), 0.15, emissive=(glow[0] * 3.0, glow[1] * 3.0, glow[2] * 3.0))
    EXTRA_SLOTS["Kristall"] = crystal_mat
    EXTRA_SLOTS["Knochen"] = simple_material("M_HK_Gebein", (0.42, 0.37, 0.29), 0.8)      # M_HK_Knochen war zu weiß
    EXTRA_SLOTS["Dunkel"] = simple_material("M_HK_Dunkel", (0.015, 0.012, 0.01), 0.9)
    EXTRA_SLOTS["Schleim"] = simple_material("M_HK_Schleim_Gruen", (0.1, 0.3, 0.06), 0.08, emissive=(1.0, 3.0, 0.6))
    lachen = list(SCHLEIM) if STYLE.get("schaedelwand") else STYLE.get("schleim", [])
    for farbe in lachen:
        if farbe in SCHLEIM:
            col = SCHLEIM[farbe]
            SLIME_MATS[farbe] = simple_material("M_HK_Schleim_" + farbe, (col[0] * 0.3, col[1] * 0.3, col[2] * 0.3), 0.08,
                                                emissive=(col[0] * 3.0, col[1] * 3.0, col[2] * 3.0))
        else:
            # Glut und Gift glühen gedämpft (mit 3-fachem Leuchten waren sie weiß überstrahlt)
            col = LACHEN[farbe]
            SLIME_MATS[farbe] = simple_material("M_HK_Lache_" + farbe, (col[0] * 0.25, col[1] * 0.25, col[2] * 0.25), 0.1,
                                                emissive=(col[0] * 0.9, col[1] * 0.9, col[2] * 0.9))

    def glow_mat(prefix, col):
        name = "{}_{:02x}{:02x}{:02x}".format(prefix, int(col[0] * 255), int(col[1] * 255), int(col[2] * 255))
        return simple_material(name, (col[0] * 0.5 + 0.2, col[1] * 0.5 + 0.2, col[2] * 0.5 + 0.2), 0.15, emissive=(col[0] * 3.0, col[1] * 3.0, col[2] * 3.0))
    CRYSTAL_MATS[:] = [(col, glow_mat("M_HK_Kristall", col)) for col in STYLE.get("kristalle", [])]
    if STYLE.get("erzfarbe"):
        ORE_MAT[:] = [glow_mat("M_HK_Erz", STYLE["erzfarbe"])]
    SPONGE[:] = [simple_material("M_HK_Rostschwamm", (0.26, 0.11, 0.04), 0.9)]
    EXTRA_SLOTS["Fels"] = rock
    for name in PROPS:
        meshes[name] = import_piece(name)
        kit_materials(meshes[name])
    build_cave(meshes, rock, floor_mat, ore_mat, water_mat, crystal_mat)
    build_map()
    if not os.environ.get("VAEL_HOEHLE_OHNE_WELT"):
        build_world_entrance()
    log("fertig {}: {}".format(NAME, counts))


main()
