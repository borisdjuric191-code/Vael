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
         "SM_HK_Schwamm", "SM_HK_Rippe", "SM_HK_Kerzen", "SM_HK_Schleimzapfen_A", "SM_HK_Schleimzapfen_B",
         "SM_HK_Knochensaeule", "SM_HK_Schaedelbogen", "SM_HK_Schaedelbogen_Breit"]
ZAPFEN = []         # Schleimzapfen (main)
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
                      "grundlicht": 1.4, "erzfarbe": (1.0, 0.35, 0.6), "nebeldichte": 0.01, "wandhoehe": 0.55, "erzdichte": 0.3,
                      "licht": "laterne", "schutt": "kiesel", "schleim": ["Bernstein"], "schleimzapfen": 0.3},
    # Schwammwucherungen, Giftlachen, kränkliches Grün
    "Rostschwamm": {"grundfarbe": (0.62, 0.82, 0.45), "fels": (0.45, 0.3, 0.2), "boden": (0.3, 0.22, 0.15), "laterne": (0.6, 1.0, 0.3), "nebel": (0.04, 0.07, 0.02),
                    "grundlicht": 0.6, "natur": True, "wucherung": "schwamm", "schleim": ["Gift"], "erzfarbe": (0.4, 1.0, 0.3), "nebeldichte": 0.03, "schleimzapfen": 0.35,
                    "licht": "sporen", "schutt": "kiesel"},
    # Ringweg um den Abgrund, Rippen der Urdrachin, Rippenspat
    "TiefsterSchacht": {"grundfarbe": (0.68, 0.6, 0.9), "fels": (0.38, 0.36, 0.4), "boden": (0.28, 0.27, 0.3), "laterne": (0.8, 0.7, 1.0), "nebel": (0.04, 0.03, 0.06),
                        "grundlicht": 0.8, "natur": True, "erzfarbe": (1.0, 0.9, 0.7), "kristalle": [(0.8, 0.7, 1.0)], "abgrund": True,
                        "nebeldichte": 0.015, "licht": "kristall", "schutt": "kiesel", "rippen_auto": True},
    # der Höhlenverbund: das, was alle Höhlen gemeinsam haben (der rote Faden, Boris 2026-11-22): rostbrauner Eisenfels, warmes
    # Grubenlicht der Gilde, dieselbe Grubenluft
    "Eisenschlund": {"grundfarbe": (0.8, 0.72, 0.62), "fels": (0.42, 0.37, 0.33), "boden": (0.32, 0.28, 0.25), "laterne": (1.0, 0.62, 0.32),
                     "nebel": (0.05, 0.04, 0.035), "grundlicht": 0.9, "nebeldichte": 0.012, "volumennebel": False, "licht": "laterne",
                     "schutt": "kiesel"},
    # Gildenstollen zwischen den Höhlen: krumm durch den Fels getrieben, Grubenlaternen
    "Verbindung": {"grundfarbe": (0.9, 0.72, 0.55), "fels": (0.44, 0.39, 0.34), "boden": (0.33, 0.29, 0.26), "laterne": (1.0, 0.62, 0.32),
                   "nebel": (0.09, 0.07, 0.055), "grundlicht": 1.0, "natur": True, "licht": "laterne", "schutt": "kiesel", "licht_abstand": 5.0,
                   "tropfsteine": 0.2},
    # der enge Spalt zum Kobold-Stollen: kein Licht, nur Fels (schwer zu finden)
    "Spalt": {"grundfarbe": (0.6, 0.6, 0.65), "fels": (0.32, 0.3, 0.32), "boden": (0.24, 0.22, 0.23), "laterne": (0.45, 1.0, 0.35),
              "nebel": (0.03, 0.03, 0.03), "grundlicht": 0.5, "natur": True, "licht": "keins", "schutt": "kiesel", "tropfsteine": 0.5},
    "Gebeinkammer": {"grundfarbe": (0.85, 0.74, 0.6), "fels": (0.6, 0.55, 0.47), "boden": (0.42, 0.38, 0.33), "laterne": (1.0, 0.7, 0.4), "nebel": (0.05, 0.045, 0.04),
                     "grundlicht": 1.1, "schaedelwand": 0.85, "nebeldichte": 0.015, "wandhoehe": 0.7, "licht": "kerzen", "schutt": "knochen",
                     "tropfsteine": 0.3, "schleimzapfen": 0.7},
}

# Die Erze des Eisenschlunds (Docs/Eisenschlund_Erze_Pflanzen_Wesen.md): Seltenheit 0 häufig bis 5 legendär
ERZ_SELTENHEIT = {"Rosteisen": 0, "Schwefelkies": 0, "Funkenerz": 1, "Gruenspan": 2, "Salzspat": 2, "Drahtsilber": 3, "Blutzinnober": 3,
                  "Kobaltbluete": 3, "Siechglas": 4, "Rippenspat": 4, "Kernerz": 5}
ERZ_GEWICHT = [6.0, 4.0, 2.5, 1.4, 0.8, 0.25]       # wie oft ein Erz als Wandschmuck vorkommt, nach Seltenheit
# welche Erze in welcher Höhle wachsen (nach "Wo es wächst" und der Art der Höhle)
HOEHLEN_ERZE = {
    "Gildenmine": ["Rosteisen", "Schwefelkies", "Funkenerz"],
    "Olmgrotte": ["Salzspat", "Gruenspan", "Drahtsilber"],
    "Gebeinkammer": ["Rosteisen", "Siechglas", "Rippenspat"],
    "FunkenerzAdern": ["Funkenerz", "Rosteisen", "Schwefelkies", "Blutzinnober"],
    "Ameisenbau": ["Rosteisen", "Schwefelkies", "Gruenspan", "Blutzinnober", "Salzspat"],
    # der Hort der Kobolde (Boris 2026-11-22): nur Erze, die sonst selten sind; das Seltenste ganz hinten
    "KoboldStollen": ["Drahtsilber", "Blutzinnober", "Kobaltbluete", "Siechglas", "Kernerz"],
    "Rostschwamm": ["Rosteisen", "Gruenspan", "Siechglas"],
    "TiefsterSchacht": ["Salzspat", "Drahtsilber", "Rippenspat", "Kernerz"],
}
ERZ_TEST = os.environ.get("VAEL_HOEHLE_ERZ_TEST")     # zum Ausprobieren: jedes Erz durch dieses ersetzen (z. B. Russquarz)
ERZ_QUELLE = "C:/Projekte/Vael_Quellen/Hoehlen/export/erze"
ERZ_ZIEL = "/Game/Vael/Maps/Hoehlen/Erze"

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
erng = random.Random(4242)    # eigener Zufall der Erze: der Rest der Höhle bleibt, wie er war
counts = {}

D = json.load(open(os.path.join(DATA, NAME + ".json"), encoding="utf-8"))
GRID = D["grundriss"]
for _ch in os.environ.get("VAEL_HOEHLE_TEST_OHNE", ""):      # nur zum Eingrenzen von Fehlern: diese Zeichen werden Boden
    GRID = [row.replace(_ch, ".") for row in GRID]
H, W = len(GRID), max(len(r) for r in GRID)
S = D.get("zelle_m", 4.0) * M
# Höhlenverbund (Tools/Welt/hoehlen_verbund.py, Boris 2026-11-22: alle Höhlen in einer Map): jede Zelle gehört zu einer Region mit
# eigener Höhlenart; was für die ganze Map gilt (Grundlicht, Nebel, Belichtung), nimmt die Grundart "Eisenschlund"
BASIS_ART = D.get("art", "Gildenmine")
REGIONEN = D.get("regionen")
REGION_INFO = D.get("region_info", {})
_JETZT = [BASIS_ART]


def hier(r, c):
    """Die Region der Zelle wird die aktuelle: STYLE, natur(), fels() und boden() richten sich nach ihr"""
    art = BASIS_ART
    if REGIONEN and 0 <= r < len(REGIONEN) and 0 <= c < len(REGIONEN[r]):
        art = REGION_INFO.get(REGIONEN[r][c], {}).get("art", BASIS_ART)
    _JETZT[0] = art if art in ART else BASIS_ART
    return _JETZT[0]


def basis():
    _JETZT[0] = BASIS_ART


class _Stil:
    """Die Werte der Höhlenart der aktuellen Region (Einzelhöhle: immer dieselbe)"""
    def get(self, key, default=None):
        return ART[_JETZT[0]].get(key, default)

    def __getitem__(self, key):
        return ART[_JETZT[0]].get(key, ART[BASIS_ART].get(key))


STYLE = _Stil()
ARTEN = sorted({BASIS_ART} | {v.get("art") for v in REGION_INFO.values() if v.get("art") in ART})


def natur():
    return STYLE.get("natur", False) and not os.environ.get("VAEL_HOEHLE_TEST_GERADE")


def jede_art(key):
    """Hat irgendeine Region der Map diesen Wert (z. B. abgrund)"""
    return any(ART[a].get(key) for a in ARTEN)


ROCKS, FLOORS, KRISTALLE, ERZMATS = {}, {}, {}, {}


def fels():
    return ROCKS.get(_JETZT[0]) or ROCKS.get(BASIS_ART)


def boden():
    return FLOORS.get(_JETZT[0]) or FLOORS.get(BASIS_ART)


def kristalle():
    return KRISTALLE.get(_JETZT[0], [])


def erzmat():
    return ERZMATS.get(_JETZT[0], [])


def lachen_hier():
    farben = list(SCHLEIM) if STYLE.get("schaedelwand") else STYLE.get("schleim", [])
    return [f for f in sorted(farben) if f in SLIME_MATS]


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
    if name in FAB_BONES:
        # aus Hunderten echter Knochen (Tools/Welt/vael_knochen_fab.py): bis 250 000 Dreiecke je Teil, das trägt nur Nanite
        nanite = mesh.get_editor_property("nanite_settings")
        nanite.set_editor_property("enabled", True)
        mesh.set_editor_property("nanite_settings", nanite)
    return mesh


FAB_BONES = {"SM_HK_Schaedelwand_A", "SM_HK_Schaedelwand_B", "SM_HK_Knochensaeule", "SM_HK_Schaedelbogen", "SM_HK_Schaedelbogen_Breit",
             "SM_HK_Knochenhaufen"}


ROCK_MATERIAL = "M_HK_FelsRelief"     # M_HK_Fels ohne Relief bleibt liegen, bis alle Höhlen neu gebaut sind
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
    path = PARTS + "/" + ROCK_MATERIAL
    if assets.does_asset_exist(path) and not os.environ.get("VAEL_HOEHLE_MATERIAL_NEU"):
        # bleibt stehen: neu gebaut müsste das Spiel seine Shader beim nächsten Start erst übersetzen, bis dahin fehlen Boden und Wände
        return unreal.load_asset(path)
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
    else:
        material = tools.create_asset(ROCK_MATERIAL, PARTS, unreal.Material, unreal.MaterialFactoryNew())
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

    # Relief (Verschönern, 15.11.): die Normal-Map des Felsens auf denselben drei Projektionen, in Weltrichtung zur
    # Flächennormalen addiert. Die Höhlenteile haben keine brauchbaren UVs, deshalb ohne Tangentenraum
    ntex = unreal.load_asset("/Game/Rock_Collection_04/Meshes/Rock_01/Textures/T_Rock_01_N")
    zero = const(0.0, 20)

    def append3(a, b, c, col):
        return op(unreal.MaterialExpressionAppendVector, col, op(unreal.MaterialExpressionAppendVector, col + 1, a, b), c)

    def relief(size, col):
        pos = op(unreal.MaterialExpressionDivide, col + 4, world, const(size, col + 5))
        x, y, z = mask(pos, col + 3, r=True), mask(pos, col + 3, g=True), mask(pos, col + 3, b=True)

        def nsample(u, v):
            uv = op(unreal.MaterialExpressionAppendVector, col + 2, u, v)
            t = node(unreal.MaterialExpressionTextureSample, col + 1, texture=ntex, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
            link(uv, t, "UVs")
            return mask(t, col, r=True), mask(t, col, g=True)
        ar, ag = nsample(x, z)          # Wände mit Blick nach Y
        br, bg = nsample(y, z)          # Wände mit Blick nach X
        cr, cg = nsample(x, y)          # Draufsicht
        towards_x = node(unreal.MaterialExpressionSaturate, col - 1)
        link(op(unreal.MaterialExpressionMultiply, col, nx, const(1.4, col + 1)), towards_x)
        side = lerp(append3(ar, zero, ag, col - 1), append3(zero, br, bg, col - 1), towards_x, col - 2)
        return lerp(side, append3(cr, cg, zero, col - 1), up, col - 3)
    detail = op(unreal.MaterialExpressionAdd, 21, relief(170.0, 30), op(unreal.MaterialExpressionMultiply, 22, relief(650.0, 40), const(0.6, 23)))
    bent = op(unreal.MaterialExpressionAdd, 20, node(unreal.MaterialExpressionVertexNormalWS, 21),
              op(unreal.MaterialExpressionMultiply, 21, detail, const(1.1, 22)))
    nrm = node(unreal.MaterialExpressionNormalize, 19)
    link(bent, nrm)
    material.set_editor_property("tangent_space_normal", False)
    library.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)
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


# Wie sich jede Lache bewegt (Boris 13.11.: jede Schleimart auch anders in der Bewegung, Rot zackig und angriffslustig):
# Tempo, Größe der Flecken in cm, zackig (Turbulenz), Glanz von bis
FLUSS = {"Gruen": (0.12, 140.0, False, 0.15, 2.4), "Violett": (0.05, 220.0, False, 0.1, 2.6), "Rot": (0.6, 90.0, True, 0.05, 3.0),
         "Bernstein": (0.08, 180.0, False, 0.2, 2.2), "Tuerkis": (0.2, 120.0, False, 0.15, 2.4), "Glut": (0.1, 160.0, True, 0.1, 2.6),
         "Gift": (0.15, 110.0, False, 0.12, 2.2)}


def flow_material(farbe, col, strength):
    """Leuchtende Lache, die lebt: Flecken ziehen langsam über die Fläche und glühen auf (Verschönern, 15.11.; vorher flach)"""
    name = "M_HK_Fluss_" + farbe
    path = "{}/{}".format(PARTS, name)
    if assets.does_asset_exist(path) and not (os.environ.get("VAEL_HOEHLE_MATERIAL_NEU") or os.environ.get("VAEL_HOEHLE_FLUSS_NEU")):
        return unreal.load_asset(path)
    material = unreal.load_asset(path) if assets.does_asset_exist(path) else         tools.create_asset(name, PARTS, unreal.Material, unreal.MaterialFactoryNew())
    library.delete_all_material_expressions(material)
    speed, size, jagged, low, high = FLUSS.get(farbe, (0.12, 140.0, False, 0.5, 1.5))
    rows = {}

    def node(cls, col_, **props):
        rows[col_] = rows.get(col_, 0) + 1
        e = library.create_material_expression(material, cls, -260 * col_, 130 * rows[col_])
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(a, b, inp=""):
        library.connect_material_expressions(a, "", b, inp)

    def op(cls, col_, a, b):
        e = node(cls, col_)
        link(a, e, "A")
        link(b, e, "B")
        return e

    def vec(c, col_):
        return node(unreal.MaterialExpressionConstant3Vector, col_, constant=unreal.LinearColor(c[0], c[1], c[2], 1.0))

    def const(v, col_):
        return node(unreal.MaterialExpressionConstant, col_, r=v)
    world = node(unreal.MaterialExpressionWorldPosition, 8)
    time = node(unreal.MaterialExpressionTime, 8)
    drift = op(unreal.MaterialExpressionMultiply, 7, time, vec((speed * 60.0, speed * 37.0, speed * 45.0), 8))
    pos = op(unreal.MaterialExpressionAdd, 6, world, drift)
    noises = []
    for k, (scale, levels) in enumerate(((1.0 / size, 3), (2.7 / size, 2))):
        n = node(unreal.MaterialExpressionNoise, 5, scale=scale, levels=levels, turbulence=jagged, level_scale=2.0,
                 noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU, output_min=0.0, output_max=1.0,
                 quality=1)
        link(pos if k == 0 else op(unreal.MaterialExpressionMultiply, 6, pos, const(-1.0, 7)), n, "Position")
        noises.append(n)
    mix = op(unreal.MaterialExpressionMultiply, 4, op(unreal.MaterialExpressionAdd, 4, noises[0], noises[1]), const(0.5, 5))
    shine = node(unreal.MaterialExpressionLinearInterpolate, 3)
    link(const(low, 4), shine, "A")
    link(const(high, 4), shine, "B")
    link(mix, shine, "Alpha")
    emissive = op(unreal.MaterialExpressionMultiply, 2, vec((col[0] * strength, col[1] * strength, col[2] * strength), 3), shine)
    library.connect_material_property(vec((col[0] * 0.04, col[1] * 0.04, col[2] * 0.04), 1), "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(const(0.06, 1), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(const(0.7, 1), "", unreal.MaterialProperty.MP_SPECULAR)
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def rock_instance(name, colour):
    """Höhlenfels (M_HK_Fels) in der Farbe der Höhlenart"""
    path = "{}/{}".format(PARTS, name)
    instance = unreal.load_asset(path) if assets.does_asset_exist(path) else \
        tools.create_asset(name, PARTS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(instance, unreal.load_asset(PARTS + "/" + ROCK_MATERIAL))
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
        # das Profil, nicht nur den Schalter: beim Laden der Map setzte das Profil "BlockAll" die Kollision wieder an, und Laternen,
        # Kiesel, Knochen und Wanderze versperrten im Spiel die engen Gänge (Lauftest 22.11.)
        component.set_collision_profile_name("NoCollision")
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    # Decken über dem Weg (Felsbrücken und was daran hängt, Schädelbögen): das Spiel blendet sie aus, wenn sie die Figur verdecken
    # (AVaelIndoorLevel); die Felsdecke über dem massiven Fels verdeckt niemanden
    if folder == "Schutt":
        # Kiesel und Splitter am Boden: kein Schatten (von oben kaum zu sehen, aber jedes Licht mit Schatten rechnete Tausende davon),
        # und nur in der Nähe der Kamera
        component.set_editor_property("cast_shadow", False)
        component.set_editor_property("ld_max_draw_distance", 4000.0)
    if (folder == "Decke" and label != "Felsdecke") or label.startswith("Schaedelbogen"):
        actor.set_editor_property("tags", [unreal.Name("VaelDecke")])
    return actor


# Blender -Y (die raue Vorderseite) kommt in Unreal als +Y an: ohne die halbe Drehung zeigte die glatte Rückseite in den Gang
# (Boris 15.11.: "viele Wände, die zu glatt sind")
WALL_TURN = float(os.environ.get("VAEL_HOEHLE_WAND_DREHUNG", "180"))
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
    if kind == "laterne" and wall is None:
        # kein Fels direkt daneben: nur das Licht, ohne Gestell (vorher stand die Laterne mitten im Gang, Lauftest 22.11.)
        point_light(x, y, 260.0, "Licht", "Laternenschein", STYLE["laterne"], 100.0, 1700.0, shadows=True)
    elif kind == "laterne":
        dx, dy = wall
        # in einspurigen Gängen ganz an die Wand (70 cm davor stand sie im Kobold-Labyrinth im Weg, Lauftest 22.11.)
        einspurig = sum(is_floor(r + a, c + b) for a, b, _ in DIRS.values()) <= 2
        abstand = 15.0 if einspurig else 70.0
        px, py = x + dx * (S / 2 - abstand), y + dy * (S / 2 - abstand)
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
        if kristalle():
            colour, mat = kristalle()[(r * 5 + c * 11) % len(kristalle())]
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


def erz_name(name):
    return ERZ_TEST or name


def erz_material(name):
    """Die Material-Instanz des aufbereiteten Tripo-Erzes (Kreaturenfabrik, Requisite)"""
    for region in ("Eisenschlund", "Aschenmark"):
        path = "/Game/Vael/Requisiten/{}/{}/MI_{}".format(region, name, name)
        if assets.does_asset_exist(path):
            return unreal.load_asset(path)
    return None


ERZ_STUECKE = {}


def erz_stuecke(name):
    """Die drei Wandstücke eines Erzes (Tools/Welt/hoehlen_erzwand.py), beim ersten Mal importiert; leer, wenn es sie nicht gibt"""
    name = erz_name(name)
    if name in ERZ_STUECKE:
        return ERZ_STUECKE[name]
    material = erz_material(name)
    result = []
    for label in "ABC":
        piece = "SM_HK_ErzWand_{}_{}".format(name, label)
        path = "{}/{}".format(ERZ_ZIEL, piece)
        fbx = os.path.join(ERZ_QUELLE, piece + ".fbx")
        if os.path.exists(fbx) and (not assets.does_asset_exist(path) or os.environ.get("VAEL_HOEHLE_ERZ_NEU")):
            ui = unreal.FbxImportUI()
            ui.set_editor_property("import_mesh", True)
            ui.set_editor_property("import_as_skeletal", False)
            ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
            ui.set_editor_property("import_materials", False)
            ui.set_editor_property("import_textures", False)
            ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
            ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", fbx)
            task.set_editor_property("destination_path", ERZ_ZIEL)
            task.set_editor_property("destination_name", piece)
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("automated", True)
            task.set_editor_property("save", True)
            task.set_editor_property("options", ui)
            tools.import_asset_tasks([task])
        mesh = unreal.load_asset(path) if assets.does_asset_exist(path) else None
        if mesh is not None:
            if material is not None:
                for i in range(len(mesh.get_editor_property("static_materials"))):
                    mesh.set_material(i, material)
            # Schmuck: keine Kollision (man läuft nicht dagegen, und die Spuren der Erzsuche treffen weiter den Fels)
            body = mesh.get_editor_property("body_setup")
            if body is not None:
                body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
            assets.save_loaded_asset(mesh, False)
            result.append(mesh)
    ERZ_STUECKE[name] = result
    if not result:
        log("keine Wandstücke für " + name)
    return result


def trace(world, start, end):
    hit = unreal.SystemLibrary.line_trace_single(world, start, end, unreal.TraceTypeQuery.ECC_VISIBILITY, True, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    values = hit.to_tuple()
    if not values[0]:
        return None
    # nur der Fels zählt (Wände, Ecken, Brocken), nicht Schädelreihen, Verbau oder Kisten
    actor = values[9] if len(values) > 9 else None
    mesh = actor.static_mesh_component.static_mesh if isinstance(actor, unreal.StaticMeshActor) else None
    name = mesh.get_name() if mesh is not None else ""
    if not (name.startswith("SM_HK_Wand") or name.startswith("SM_HK_Ecke") or name.startswith("SM_ES_Brocken")):
        return None
    normal = values[7]
    ray = end - start
    if normal.x * ray.x + normal.y * ray.y + normal.z * ray.z > 0.0:
        # von innen getroffen: der Strahl begann im Fels (die Felsfront ragt weit in die Zelle); so ein Treffer liegt im Fels
        return None
    return values[4], normal


def erz_auf(mesh, point, normal, size, label, sink=0.18):
    """Ein Wandstück so setzen, dass seine +Z-Achse der Flächennormale folgt und die Schnittfläche im Fels steckt"""
    n = unreal.Vector(normal.x, normal.y, normal.z)
    n.normalize()
    side = unreal.Vector(erng.uniform(-1, 1), erng.uniform(-1, 1), erng.uniform(-1, 1))
    side = side - n * side.dot(n)
    if side.length() < 0.01:
        side = unreal.Vector(0, 0, 1) - n * n.z
    rot = unreal.MathLibrary.make_rot_from_zx(n, side)
    at = point - n * (size * 100.0 * sink)
    actor = mesh_actor(mesh, at.x, at.y, at.z, "Erze/Wand", label, collision=False, scale=(size, size, size))
    actor.set_actor_rotation(rot, False)
    return actor


def erz_wahl(erze):
    gewichte = [ERZ_GEWICHT[ERZ_SELTENHEIT[e]] for e in erze]
    return erng.choices(erze, weights=gewichte)[0]


def wand_erze(world):
    """Erze als Teil der Wandoptik, nicht erntebar (Boris 15.11.): in die Wände (Strahl von der Zellmitte gegen den Fels) und oben
    auf den Fels (die Decke der Höhle sieht die Kamera als Felskrone)"""
    n = 0
    for r in range(H):
        for c in range(W):
            if not is_floor(r, c) or cell(r, c) in "XO":
                continue
            erze = [e for e in HOEHLEN_ERZE.get(hier(r, c), []) if erz_stuecke(e)]
            if not erze:
                continue
            dichte = STYLE.get("erzdichte", 0.5)          # 0,32 war Boris zu spärlich
            x, y = centre(r, c)
            for key, (dr, dc, (dx, dy)) in DIRS.items():
                if is_floor(r + dr, c + dc) or erng.random() > dichte:
                    continue
                low = dx * CAMERA[0] + dy * CAMERA[1] > 0.3
                top = 180.0 if low else 520.0 * STYLE.get("wandhoehe", 1.0)
                e = erz_wahl(erze)
                back = 1.0 if is_floor(r - dr, c - dc) else 0.0      # von der Zelle davor aus, wenn es Boden ist
                for k in range(erng.randint(1, 3)):
                    t = erng.uniform(-0.35, 0.35) * S
                    z = erng.uniform(60.0, top)
                    start = unreal.Vector(x + dy * t - dx * S * back, y - dx * t - dy * S * back, z)
                    end = unreal.Vector(x + dy * t + dx * S, y - dx * t + dy * S, z)
                    hit = trace(world, start, end)
                    if hit is None:
                        continue
                    mesh = erng.choice(ERZ_STUECKE[erz_name(e)])
                    size = erng.uniform(1.0, 2.2) * (0.75 if ERZ_SELTENHEIT[e] >= 3 else 1.0)
                    erz_auf(mesh, hit[0], hit[1], size, e)
                    n += 1
    # oben auf dem Fels: Krusten und Spitzen, von oben gesehen (gleich hinter der Felsfront, dort steht die Wand)
    for r in range(H):
        for c in range(W):
            if not is_floor(r, c) or cell(r, c) in "XO":
                continue
            erze = [e for e in HOEHLEN_ERZE.get(hier(r, c), []) if erz_stuecke(e)]
            if not erze:
                continue
            dichte = STYLE.get("erzdichte", 0.5)
            x, y = centre(r, c)
            for key, (dr, dc, (dx, dy)) in DIRS.items():
                if is_floor(r + dr, c + dc) or erng.random() > dichte * 0.5:
                    continue
                t = erng.uniform(-0.4, 0.4) * S
                into = S / 2 + erng.uniform(-40.0, 90.0)
                px, py = x + dx * into + dy * t, y + dy * into - dx * t
                hit = trace(world, unreal.Vector(px, py, 2500.0), unreal.Vector(px, py, -50.0))
                if hit is None or hit[0].z < 150.0:
                    continue
                e = erz_wahl(erze)
                erz_auf(erng.choice(ERZ_STUECKE[erz_name(e)]), hit[0], hit[1], erng.uniform(1.2, 2.6), e + " (Felskrone)")
                n += 1
    return n


def tropfsteine(world):
    """Stalagmiten am Fuß der Wände und Stalaktiten unter den Überhängen (Boris 15.11.): Stalaktiten hängen dort, wo ein Strahl vom
    Boden nach oben den Fels trifft; natürliche Höhlen dicht, Stollen und Katakomben spärlich"""
    n_mit, n_tit, n_zap = 0, 0, [0]
    for r in range(H):
        for c in range(W):
            if not is_floor(r, c) or cell(r, c) in "XO~E":
                continue
            hier(r, c)
            dichte = STYLE.get("tropfsteine", 0.4 if natur() else 0.12)
            x, y = centre(r, c)
            for key, (dr, dc, (dx, dy)) in DIRS.items():
                if is_floor(r + dr, c + dc):
                    continue
                low = dx * CAMERA[0] + dy * CAMERA[1] > 0.3
                narrow = not is_floor(r - dr, c - dc) or sum(is_floor(r + a, c + b) for a, b, _ in DIRS.values()) <= 2
                # Stalagmiten: kleine Gruppen am Wandfuß, aus dem breiten Fuß heraus (nicht in engen Gängen, da versperrten sie den Weg)
                if not narrow and erng.random() < dichte:
                    t = erng.uniform(-0.4, 0.4) * S
                    base = S / 2 - erng.uniform(90.0, 170.0)
                    for k in range(erng.randint(1, 4)):
                        h_ = erng.uniform(0.8, 3.2) * (0.6 if low else 1.0)
                        px = x + dx * (base - erng.uniform(0, 60)) + dy * (t + erng.uniform(-70, 70))
                        py = y + dy * (base - erng.uniform(0, 60)) - dx * (t + erng.uniform(-70, 70))
                        mesh_actor(erng.choice(MITES), px, py, -25.0, "Tropfsteine", "Stalagmit", yaw=erng.uniform(0, 360),
                                   scale=(h_ * 0.42, h_ * 0.42, h_), material=fels(), pitch=erng.uniform(-6, 6), roll=erng.uniform(-6, 6),
                                   collision=False)      # Schmuck: ein Stalagmit darf nie einen Gang versperren (Lauftest 22.11.)
                        n_mit += 1
                # Stalaktiten unter dem Überhang: von unten nach oben gegen den Fels
                if not low and erng.random() < min(0.9, dichte * 2.0):
                    for k in range(erng.randint(3, 7)):
                        t = erng.uniform(-0.45, 0.45) * S
                        into = S / 2 - erng.uniform(10.0, 140.0)
                        px, py = x + dx * into + dy * t, y + dy * into - dx * t
                        hit = trace(world, unreal.Vector(px, py, 120.0), unreal.Vector(px, py, 1800.0))
                        if hit is None or hit[0].z < 260.0:
                            continue
                        h_ = erng.uniform(0.7, 2.6)
                        if lachen_hier() and ZAPFEN and erng.random() < STYLE.get("schleimzapfen", 0.0):
                            # Schleimzapfen: leuchtende Säure-Bakterien, die mit ihrem Licht anlocken (Lore, Boris 13.11.)
                            farbe = erng.choice(lachen_hier())
                            z_ = mesh_actor(erng.choice(ZAPFEN), px, py, hit[0].z + 25.0, "Tropfsteine", "Schleimzapfen " + farbe,
                                            yaw=erng.uniform(0, 360), scale=(h_ * 0.4, h_ * 0.4, h_), collision=False)
                            z_.static_mesh_component.set_material_by_name("Fels", fels())
                            z_.static_mesh_component.set_material_by_name("Schleim", SLIME_MATS[farbe])
                            if erng.random() < 0.35:
                                col = SCHLEIM.get(farbe) or LACHEN[farbe]
                                point_light(px, py, hit[0].z - h_ * 100.0 - 40.0, "Tropfsteine", "Zapfenschein " + farbe, col, 6.0, 500.0)
                            n_zap[0] += 1
                            continue
                        mesh_actor(erng.choice(MITES), px, py, hit[0].z + 30.0, "Tropfsteine", "Stalaktit", yaw=erng.uniform(0, 360),
                                   scale=(h_ * 0.38, h_ * 0.38, h_), material=fels(), roll=180.0 + erng.uniform(-5, 5),
                                   collision=False)
                        n_tit += 1
    log("Tropfsteine: {} Stalagmiten, {} Stalaktiten, {} Schleimzapfen".format(n_mit, n_tit, n_zap[0]))
    return n_mit + n_tit


def deckenboegen():
    """Sichtbare Decken (Boris 2026-11-22: wo die tropfenden Bakterien an der Decke hängen, muss man die Decke auch sehen; im
    Kobold-Stollen sah man keine): in natürlichen Gängen spannt sich ab und zu eine Felsbrücke von Wand zu Wand, darunter hängen
    Tropfsteine und, wo es die Höhle hat, leuchtende Schleimzapfen. Nicht in der Mine (Türstöcke) und der Katakombe (Schädelbögen)"""
    brocken = BOULDERS[1]
    box = brocken.get_bounding_box()
    groesse = box.max - box.min
    n = 0
    for r in range(H):
        for c in range(W):
            if cell(r, c) not in ".G":
                continue
            hier(r, c)
            if not (natur() or _JETZT[0] == "KoboldStollen") or STYLE.get("schaedelwand") or (r * 7 + c * 13) % 7:
                continue
            quer_x = not is_floor(r - 1, c) and not is_floor(r + 1, c) and is_floor(r, c - 1) and is_floor(r, c + 1)   # Gang entlang Y
            quer_y = not is_floor(r, c - 1) and not is_floor(r, c + 1) and is_floor(r - 1, c) and is_floor(r + 1, c)   # Gang entlang X
            if not (quer_x or quer_y):
                continue
            x, y = centre(r, c)
            hoch = 520.0 * STYLE.get("wandhoehe", 1.0)
            # über den Wandkronen, von Krone zu Krone: tiefer sah die Brücke von oben wie ein Steg aus, auf dem die Figur steht
            bz = max(420.0, hoch + 60.0)
            dicke = erng.uniform(70.0, 100.0)
            mesh_actor(brocken, x, y, bz, "Decke", "Felsbruecke", yaw=(0.0 if quer_x else 90.0) + erng.uniform(-8, 8),
                       scale=(S * 1.7 / groesse.x, erng.uniform(90.0, 130.0) / groesse.y, dicke / groesse.z), material=fels(), collision=False,
                       roll=erng.uniform(-4, 4))
            unten = bz - dicke * 0.45
            zapfen = lachen_hier() and ZAPFEN and STYLE.get("schleimzapfen", 0.0) > 0.0
            for k in range(erng.randint(2, 4)):
                t = erng.uniform(-0.35, 0.35) * S
                px, py = (x + t, y + erng.uniform(-40, 40)) if quer_x else (x + erng.uniform(-40, 40), y + t)
                h_ = erng.uniform(0.6, 1.6)
                if zapfen and erng.random() < 0.6:
                    farbe = erng.choice(lachen_hier())
                    z_ = mesh_actor(erng.choice(ZAPFEN), px, py, unten + 25.0, "Decke", "Schleimzapfen " + farbe, yaw=erng.uniform(0, 360),
                                    scale=(h_ * 0.4, h_ * 0.4, h_), collision=False)
                    z_.static_mesh_component.set_material_by_name("Fels", fels())
                    z_.static_mesh_component.set_material_by_name("Schleim", SLIME_MATS[farbe])
                    if k == 0:
                        col = SCHLEIM.get(farbe) or LACHEN[farbe]
                        point_light(px, py, unten - h_ * 100.0 - 40.0, "Decke", "Zapfenschein " + farbe, col, 6.0, 500.0)
                else:
                    mesh_actor(erng.choice(MITES), px, py, unten + 30.0, "Decke", "Stalaktit", yaw=erng.uniform(0, 360),
                               scale=(h_ * 0.38, h_ * 0.38, h_), material=fels(), roll=180.0 + erng.uniform(-5, 5), collision=False)
            n += 1
    log("Deckenbögen: {}".format(n))
    return n


def felsdecke():
    """Die Decke der Höhle (Boris 2026-11-22: die Decken müssen ersichtlich sein, im Kobold-Stollen sah man keine): über dem massiven
    Fels (Zellen, deren acht Nachbarn auch Fels sind) liegt eine dunkle Felsplatte über den Wandkronen. Von oben liest sich der Fels so
    als geschlossene Gesteinsmasse und die Gänge als Stollen darunter; an den Gängen selbst bleiben die Wandkronen sichtbar. Eine Platte
    je Lauf in einer Zeile, ohne Kollision und ohne Schatten (die Lichter hängen tiefer)"""
    dunkel = EXTRA_SLOTS.get("Dunkel")
    n = 0
    for r in range(H):
        c = 0
        while c < W:
            if not all(not is_floor(r + dr, c + dc) for dr in (-1, 0, 1) for dc in (-1, 0, 1)):
                c += 1
                continue
            c0 = c
            while c < W and all(not is_floor(r + dr, c + dc) for dr in (-1, 0, 1) for dc in (-1, 0, 1)):
                c += 1
            x, y0 = centre(r, c0)
            _, y1 = centre(r, c - 1)
            platte = mesh_actor(CUBE, x, (y0 + y1) / 2, 640.0 + erng.uniform(-30.0, 30.0), "Decke", "Felsdecke",
                                scale=(S / 100.0 + 0.02, (y1 - y0 + S) / 100.0 + 0.02, 0.6), material=dunkel, collision=False)
            platte.static_mesh_component.set_editor_property("cast_shadow", False)
            n += 1
    log("Felsdecke: {} Platten".format(n))
    return n


def katakombe(meshes):
    """Die Gebeinkammer als Katakombe (Boris 13.11.: wie die Pariser Katakomben, eine echte Knochensammlung): Schädelbögen quer
    über die engen Gänge, Knochensäulen in den Hallen"""
    if not jede_art("schaedelwand"):
        return 0
    n = 0
    for r in range(H):
        for c in range(W):
            if cell(r, c) != ".":
                continue
            hier(r, c)
            if not STYLE.get("schaedelwand"):
                continue
            x, y = centre(r, c)
            rock_ns = not is_floor(r - 1, c) and not is_floor(r + 1, c)
            rock_we = not is_floor(r, c - 1) and not is_floor(r, c + 1)
            if (rock_ns and is_floor(r, c - 1) and is_floor(r, c + 1)) or (rock_we and is_floor(r - 1, c) and is_floor(r + 1, c)):
                # Gang: ab und zu ein Bogen quer darüber (Säulen bei Y = ±1,8 m im Teil: entlang Y stehen die Säulen)
                if (r * 5 + c * 3) % 4 == 0:
                    yaw = 0.0 if rock_we else 90.0
                    mesh_actor(meshes["SM_HK_Schaedelbogen"], x, y, 0.0, "Gebeine", "Schaedelbogen", yaw=yaw, scale=(1.0, 0.95, 1.0))
                    n += 1
                continue
            # zweispuriger Gang: ein breiter Bogen über beide Zellen, jede dritte Reihe
            if is_floor(r, c + 1) and not is_floor(r, c - 1) and not is_floor(r, c + 2) and is_floor(r - 1, c) and is_floor(r + 1, c) and r % 3 == 0:
                mesh_actor(meshes["SM_HK_Schaedelbogen_Breit"], x, y + S / 2, 0.0, "Gebeine", "Schaedelbogen breit", yaw=0.0)
                n += 1
                continue
            if is_floor(r + 1, c) and not is_floor(r - 1, c) and not is_floor(r + 2, c) and is_floor(r, c - 1) and is_floor(r, c + 1) and c % 3 == 0:
                mesh_actor(meshes["SM_HK_Schaedelbogen_Breit"], x - S / 2, y, 0.0, "Gebeine", "Schaedelbogen breit", yaw=90.0)
                n += 1
                continue
            if all(is_floor(r + dr, c + dc) for dr in (-1, 0, 1) for dc in (-1, 0, 1)) and erng.random() < 0.14:
                s_ = erng.uniform(0.9, 1.15)
                mesh_actor(meshes["SM_HK_Knochensaeule"], x + erng.uniform(-60, 60), y + erng.uniform(-60, 60), -10.0, "Gebeine",
                           "Knochensaeule", yaw=erng.uniform(0, 360), scale=(s_, s_, s_ * erng.uniform(0.95, 1.2)))
                n += 1
    log("Katakombe: {} Bögen und Säulen".format(n))
    return n


def ernte_plan():
    """Welche Erzstelle (Z) welches erntebare Erz trägt: je weiter vom Ausgang, desto seltener (Boris 15.11.: so verteilen, dass es
    Sinn macht). Legendäres nur an der tiefsten Stelle, sehr Seltenes nur in der hinteren Hälfte"""
    exits = [(r, c) for r in range(H) for c in range(W) if cell(r, c) == "E"]
    dist = {p: 0 for p in exits}
    queue = list(exits)
    while queue:
        r, c = queue.pop(0)
        for dr, dc, _ in DIRS.values():
            q = (r + dr, c + dc)
            if q not in dist and is_floor(*q) and cell(*q) not in "XO":
                dist[q] = dist[(r, c)] + 1
                queue.append(q)
    plan = {}
    for art in ARTEN:
        erze = sorted(HOEHLEN_ERZE.get(art, []), key=lambda e: ERZ_SELTENHEIT[e])
        spots = sorted([(r, c) for r in range(H) for c in range(W) if cell(r, c) == "Z" and hier(r, c) == art], key=lambda p: dist.get(p, 0))
        if erze:
            plan_region(plan, erze, spots)
    return plan


def plan_region(plan, erze, spots):
    for i, p in enumerate(spots):
        q = i / max(1, len(spots) - 1)
        if q >= 0.999 and ERZ_SELTENHEIT[erze[-1]] >= 4:
            plan[p] = erze[-1]                                   # das Seltenste an der tiefsten Stelle
            continue
        wahl = [e for e in erze if ERZ_SELTENHEIT[e] < 5 and (ERZ_SELTENHEIT[e] < 4 or q > 0.5)]
        gewichte = [ERZ_GEWICHT[ERZ_SELTENHEIT[e]] * (1.0 + q * 2.0 * ERZ_SELTENHEIT[e]) for e in wahl]
        plan[p] = erng.choices(wahl, weights=gewichte)[0]


ERNTE = {}


def ernte_erz(name, x, y, plan_erz):
    """Ein erntebares Erz (VaelHarvestable mit DA_Harvest_<Erz>); False, wenn es die Daten noch nicht gibt"""
    data = unreal.load_asset("/Game/Vael/Nature/DA_Harvest_" + erz_name(plan_erz))
    if data is None or data.get_editor_property("mesh") is None:      # ohne Modell (noch nicht importiert): Platzhalter
        return False
    actor = spawn(unreal.VaelHarvestable, unreal.Vector(x, y, 0.0), "Erze/Ernte", "{} (erntebar)".format(plan_erz),
                  unreal.Rotator(0.0, 0.0, erng.uniform(0, 360)))
    actor.set_editor_property("data", data)
    actor.set_editor_property("tags", [unreal.Name("VaelErzstelle"), unreal.Name("VaelNatur")])
    # Aura (Lore-Bibel): ein weicher Schein in der Farbe des Erzes; je seltener, desto heller
    g = data.get_editor_property("glow_color")
    rar = ERZ_SELTENHEIT.get(plan_erz, 0)
    point_light(x, y, 110.0, "Erze/Ernte", plan_erz + " (Schein)", (g.r, g.g, g.b), 6.0 + rar * 5.0, 500.0 + rar * 80.0)
    return True


def ankunft(r, c, schritte=3):
    """Wo man vom Ausgang (r, c) aus ankommt: ein paar Zellen tiefer in der Höhle"""
    weit = {(r, c): 0}
    queue = [(r, c)]
    best = (r, c)
    while queue:
        p = queue.pop(0)
        if weit[p] >= schritte:
            best = p
            break
        for dr, dc, _ in DIRS.values():
            q = (p[0] + dr, p[1] + dc)
            if q not in weit and cell(*q) in ".GL":
                weit[q] = weit[p] + 1
                queue.append(q)
                best = q
    return centre(*best)


def wasser_material():
    """Höhlenwasser (Boris 2026-11-22: richtiges Wasser statt Vierecke): am Ufer etwas heller, zur Mitte tief und dunkel (Vertexfarbe
    Rot aus Tools/Welt/hoehlen_wasser.py), matt und dunkel. Eine Wellen-Normalmap der Engine (water_n) und Rauschen im Glanz zeigten
    von oben Kacheln, glatt spiegelte es die Kristalle weiß.
    VAEL_HOEHLE_WASSER_NEU=1 baut das Material im bestehenden Asset neu (die Map behält ihren Verweis)"""
    path = PARTS + "/M_HK_Hoehlenwasser"
    neu = os.environ.get("VAEL_HOEHLE_MATERIAL_NEU") or os.environ.get("VAEL_HOEHLE_WASSER_NEU")
    if assets.does_asset_exist(path):
        mat = unreal.load_asset(path)
        if not neu:
            return mat
        library.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset("M_HK_Hoehlenwasser", PARTS, unreal.Material, unreal.MaterialFactoryNew())
    E = unreal.MaterialProperty

    def node(cls, x, y, **props):
        n = library.create_material_expression(mat, cls, x, y)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n
    vc = node(unreal.MaterialExpressionVertexColor, -900, 0)
    flach = node(unreal.MaterialExpressionVectorParameter, -900, -200, parameter_name="Flach", default_value=unreal.LinearColor(0.022, 0.04, 0.042, 1.0))
    tief = node(unreal.MaterialExpressionVectorParameter, -900, -60, parameter_name="Tief", default_value=unreal.LinearColor(0.002, 0.006, 0.009, 1.0))
    farbe = node(unreal.MaterialExpressionLinearInterpolate, -500, -120)
    library.connect_material_expressions(flach, "", farbe, "A")
    library.connect_material_expressions(tief, "", farbe, "B")
    library.connect_material_expressions(vc, "R", farbe, "Alpha")
    library.connect_material_property(farbe, "", E.MP_BASE_COLOR)
    # matt: jede Struktur im Glanz (Wellen-Normalmap, Rauschen) zeigte von oben ein Kachelmuster und spiegelte die hellen Kristalle weiß
    rauh = node(unreal.MaterialExpressionScalarParameter, -500, 120, parameter_name="Rauheit", default_value=0.55)
    library.connect_material_property(rauh, "", E.MP_ROUGHNESS)
    spec = node(unreal.MaterialExpressionScalarParameter, -500, 200, parameter_name="Glanz", default_value=0.25)
    library.connect_material_property(spec, "", E.MP_SPECULAR)
    library.recompile_material(mat)
    assets.save_loaded_asset(mat, False)
    return mat


def wasser_flaeche():
    """Die weiche Wasserfläche dieser Höhle aus Blender (Tools/Welt/hoehlen_wasser.py), oder None (dann die alten Zellen)"""
    name = "SM_HK_Wasser_" + NAME
    if not os.path.exists(os.path.join(SOURCE, name + ".fbx")):
        return None
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
    ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    ui.static_mesh_import_data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(SOURCE, name + ".fbx"))
    task.set_editor_property("destination_path", PARTS)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])
    return unreal.load_asset("{}/{}".format(PARTS, name))


def build_cave(meshes, rock, floor_mat, ore_mat, water_mat=None, crystal_mat=None):
    if assets.does_asset_exist(LEVEL):
        assets.delete_asset(LEVEL)
    assets.make_directory("/Game/Vael/Maps/Hoehlen")
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    beam = unreal.load_asset(KIT + "/SM_GK_Lichtstrahl")
    boulders = [unreal.load_asset("/Game/Vael/Maps/Eisenschlund/Fels/SM_ES_Brocken_" + k) for k in "ABC"]
    walls = [meshes["SM_HK_Wand_A"], meshes["SM_HK_Wand_B"], meshes["SM_HK_Wand_C"]]
    lows = [meshes["SM_HK_Wand_Niedrig_A"], meshes["SM_HK_Wand_Niedrig_B"]]

    ERNTE.clear()
    ERNTE.update(ernte_plan())
    log("Erzstellen: " + ", ".join("{}".format(e) for e in sorted(ERNTE.values())))
    indoor = spawn(unreal.VaelIndoorLevel, unreal.Vector(0, 0, 500), "Spiel", "Innenraum")
    indoor.set_editor_property("place_name", unreal.Text(D.get("titel", NAME)))
    if REGIONEN:
        # die Regionen des Verbunds als Regionen des Spiels: ihr Name steht oben rechts, kein Wetter unter Tage (Boris 2026-11-22);
        # der Tag "L_<Höhle>" sagt dem Spiel, welche Höhle es ist (Bewohner, Seuchenstämme: AVaelIndoorLevel::PlaceKeyAt)
        for buchstabe, info in sorted(REGION_INFO.items()):
            if not info.get("box"):
                continue
            r0, c0, r1, c1 = info["box"]
            x0, x1 = (H - 1 - r1) * S - S / 2, (H - 1 - r0) * S + S / 2
            y0, y1 = c0 * S - S / 2, c1 * S + S / 2
            region = spawn(unreal.VaelRegion, unreal.Vector((x0 + x1) / 2, (y0 + y1) / 2, 0.0), "Spiel", "Region " + info["titel"])
            region.set_editor_property("region_name", unreal.Text(info["titel"]))
            region.set_editor_property("start_weather", unreal.VaelWeather.CLEAR)
            region.set_editor_property("available_weathers", [unreal.VaelWeather.CLEAR])
            region.set_editor_property("changing_weather", False)
            region.set_editor_property("tags", [unreal.Name("L_" + info["name"])])
            region.get_component_by_class(unreal.BoxComponent).set_box_extent(unreal.Vector((x1 - x0) / 2, (y1 - y0) / 2, 5000.0))
    # ein Felssockel unter der ganzen Höhle: nichts fällt durch eine Lücke zwischen den Platten ins Leere
    # im Verbund nur unter der Region mit dem Abgrund tief, sonst knapp unter dem Boden (40 m tief fiel man durch jede Lücke)
    tief = [info["box"] for info in REGION_INFO.values() if info.get("box") and ART.get(info.get("art"), {}).get("abgrund")]
    cube_ = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")

    def sockel(r0, c0, r1, c1, z):
        if r1 < r0 or c1 < c0:
            return
        x0, x1 = (H - 1 - r1) * S - S, (H - 1 - r0) * S + S
        y0, y1 = c0 * S - S, c1 * S + S
        mesh_actor(cube_, (x0 + x1) / 2, (y0 + y1) / 2, z, "Boden", "Felssockel", scale=((x1 - x0) / 100.0, (y1 - y0) / 100.0, 1.0), material=floor_mat)
    if REGIONEN and len(tief) == 1:
        tr0, tc0, tr1, tc1 = tief[0]
        sockel(tr0, tc0, tr1, tc1, -4000.0)
        # der Rest in vier Streifen um die tiefe Region herum
        sockel(0, 0, tr0 - 2, W - 1, -370.0)
        sockel(tr1 + 2, 0, H - 1, W - 1, -370.0)
        sockel(tr0 - 1, 0, tr1 + 1, tc0 - 2, -370.0)
        sockel(tr0 - 1, tc1 + 2, tr1 + 1, W - 1, -370.0)
    else:
        mesh_actor(cube_, (H - 1) * S / 2, (W - 1) * S / 2, -4000.0 if jede_art("abgrund") else -370.0, "Boden", "Felssockel",
                   scale=((H + 2) * S / 100.0, (W + 2) * S / 100.0, 1.0), material=floor_mat)

    WASSER[0] = wasser_flaeche()
    if WASSER[0] is not None:
        # eine Fläche mit runden Ufern für alles Wasser der Höhle
        mesh_actor(WASSER[0], 0.0, 0.0, 10.0, "Wasser", "Wasser", material=wasser_material(), collision=False)
    for r in range(H):
        for c in range(W):
            ch = cell(r, c)
            if ch == "#":
                continue
            hier(r, c)
            x, y = centre(r, c)
            if ch == "X":
                # Abgrund: kein Boden, eine unsichtbare Sperre am Rand
                block = mesh_actor(CUBE, x, y, 100.0, "Abgrund", "Abgrund (Sperre)", scale=(S / 100.0, S / 100.0, 6.0))
                block.set_actor_hidden_in_game(True)
                continue
            depth = {"~": -45.0, "O": -300.0}.get(ch, 0.0)
            mesh_actor(rng.choice([meshes["SM_HK_Boden_A"], meshes["SM_HK_Boden_B"]]), x, y, depth, "Boden", "Boden",
                       yaw=rng.choice((0, 90, 180, 270)), material=boden())
            if ch in "~O" and water_mat is not None:
                # Wasserspiegel über der Zelle (ohne weiche Fläche aus Blender); knietief zum Durchwaten, tief nur zum Ansehen
                if WASSER[0] is None:
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
                wild = 14.0 if natur() else 4.0
                yaw = math.degrees(math.atan2(-dx, dy)) + rng.uniform(-wild, wild)
                shift = rng.uniform(-60.0, 60.0) if natur() else 0.0
                # enger Gang (Fels auf beiden Seiten der Zelle): die Wand mit dem breiten Fuß zurücksetzen und flacher machen,
                # sonst wuchsen die Füße beider Seiten zusammen und der Gang war zu
                # auch an Knicken eines einspurigen Gangs (höchstens zwei Bodennachbarn): dort ragte die Außenwand mit voller Tiefe in
                # die Zelle und schnürte den Weg ab (Lauftest 22.11., der Spalt zum Kobold-Stollen)
                narrow = not is_floor(r - dr, c - dc) or sum(is_floor(r + a, c + b) for a, b, _ in DIRS.values()) <= 2
                depth = rng.uniform(0.85, 1.4 if natur() else 1.15)
                if narrow:
                    shift, depth = shift * 0.3 + 70.0, 0.6
                mesh_actor(rng.choice(lows if low else walls), x + dx * (S / 2 + shift), y + dy * (S / 2 + shift), 0.0, "Waende",
                           "Felswand niedrig" if low else "Felswand", yaw=WALL_TURN + (yaw if not narrow else math.degrees(math.atan2(-dx, dy)) + rng.uniform(-4, 4)),
                           scale=(rng.uniform(1.0, 1.25) if natur() else 1.0, depth,
                                  rng.uniform(0.8, 1.35 if natur() else 1.15) * STYLE.get("wandhoehe", 1.0) if not low else 1.0), material=fels())
                # nicht in engen Gängen: die Schädelwand steht 30 cm vor der Zellmitte, beidseits ließ sie kaum einen Spalt (Lauftest 22.11.)
                # (der Zufall wird gezogen wie zuvor, sonst verrutscht jede Wand der Map)
                if not low and rng.random() < STYLE.get("schaedelwand", 0.0) and not narrow:
                    # Schädelreihen der Gilde vor dem Fels, mit dem Gesicht in den Gang
                    # vor die Felsfront (die ragt bis ~1,5 m in die Zelle), sonst steckten die Schädel im Fels
                    mesh_actor(rng.choice([meshes["SM_HK_Schaedelwand_A"], meshes["SM_HK_Schaedelwand_B"]]), x + dx * (S / 2 - 170.0),
                               y + dy * (S / 2 - 170.0), 0.0, "Gebeine", "Schaedelwand", yaw=math.degrees(math.atan2(-dx, dy)) + SKULL_TURN,
                               scale=(0.95, 1.0, rng.uniform(1.05, 1.25)))
                if natur() and rng.random() < 0.45 and not narrow:
                    # Brocken am Fuß der Wand, gegen die geraden Linien des Rasters
                    sz = rng.uniform(0.9, 2.2)
                    t = rng.uniform(-0.4, 0.4) * S
                    mesh_actor(rng.choice(BOULDERS), x + dx * (S / 2 - 60.0) + dy * t, y + dy * (S / 2 - 60.0) - dx * t, -sz * 20.0, "Waende",
                               "Brocken am Wandfuss", yaw=rng.uniform(0, 360), scale=(sz, sz, sz * 0.8), material=fels())

    # Eckpfeiler, wo der Fels um eine Ecke geht
    for i in range(H + 1):
        for j in range(W + 1):
            around = [(i - 1, j - 1), (i - 1, j), (i, j - 1), (i, j)]
            floors = [p for p in around if is_floor(*p)]
            if not floors or len(floors) == 4:
                continue
            hier(*floors[0])
            if len(floors) == 2 and (floors[0][0] == floors[1][0] or floors[0][1] == floors[1][1]):
                continue                                                   # gerade Wand, kein Eck
            # nicht an einspurigen Stellen (höchstens zwei Bodennachbarn): dort verstellte der Eckfels den Knick (Lauftest 22.11., Spalt)
            if any(sum(is_floor(fr + a, fc + b) for a, b, _ in DIRS.values()) <= 2 for fr, fc in floors):
                continue
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
                       scale=(s, s, rng.uniform(0.9, 1.1) * (1.0 if low else STYLE.get("wandhoehe", 1.0))), material=fels())

    # Zeichen des Grundrisses
    start = None
    for r in range(H):
        for c in range(W):
            ch = cell(r, c)
            x, y = centre(r, c)
            hier(r, c)
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
                # in einspurigen Gängen ganz an die Wand (sonst stand sie im Kobold-Labyrinth im Weg, Lauftest 22.11.)
                abstand = 15.0 if sum(is_floor(r + a, c + b) for a, b, _ in DIRS.values()) <= 2 else 70.0
                an_wand = False
                for key, (dr, dc, (dx, dy)) in DIRS.items():
                    if not is_floor(r + dr, c + dc):
                        px, py = x + dx * (S / 2 - abstand), y + dy * (S / 2 - abstand)
                        yaw = math.degrees(math.atan2(-dy, -dx))
                        an_wand = True
                        break
                if an_wand:      # ohne Wand daneben nur das Licht (das Gestell stand sonst mitten im Weg)
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
                if kristalle():
                    colour, mat = kristalle()[(r * 5 + c * 11) % len(kristalle())]
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
            elif ch == "P" and (lachen_hier() or SLIME_MATS):
                farben = lachen_hier() or sorted(SLIME_MATS)
                farbe = farben[(r * 7 + c * 3) % len(farben)]
                s_ = rng.uniform(0.8, 1.3)
                mesh_actor(meshes["SM_HK_Schleim"], x, y, 2.0, "Schleim", "Lache " + farbe, yaw=rng.uniform(0, 360), scale=(s_, s_, 1.0),
                           material=SLIME_MATS[farbe], collision=False)
                col = SCHLEIM.get(farbe) or LACHEN[farbe]
                # höher und schwächer: 70 cd auf 1,2 m strahlten die Lache selbst weiß an
                point_light(x, y, 260.0, "Schleim", "Schleimschein " + farbe, col, 18.0 if farbe in SCHLEIM else 9.0, 1100.0)
                LIGHTS.append((x, y))           # die Lache leuchtet selbst: die Stelle gilt als hell
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
                               yaw=rng.uniform(0, 360), scale=(h_ * 0.55, h_ * 0.55, h_), material=fels(), pitch=rng.uniform(-6, 6),
                               collision=False)
            elif ch == "Z" and (r, c) in ERNTE and ernte_erz(NAME, x + erng.uniform(-60, 60), y + erng.uniform(-60, 60), ERNTE[(r, c)]):
                # ein paar Spitzen desselben Erzes am Boden drumherum, damit die Stelle wie ein Vorkommen aussieht
                for k in range(erng.randint(1, 3)):
                    stuecke = erz_stuecke(ERNTE[(r, c)])
                    if stuecke:
                        a = erng.uniform(0, 2 * math.pi)
                        erz_auf(stuecke[0], unreal.Vector(x + math.cos(a) * 130.0, y + math.sin(a) * 130.0, 0.0), unreal.Vector(0, 0, 1),
                                erng.uniform(0.4, 0.7), ERNTE[(r, c)] + " (Schmuck)")
                counts["Ernte"] = counts.get("Ernte", 0) + 1
            elif ch == "Z" and (STYLE.get("erz") == "salzspat" or erzmat()):
                for k in range(rng.randint(2, 3)):
                    s_ = rng.uniform(0.7, 1.1)
                    ore = mesh_actor(meshes["SM_HK_Salzspat_Klein"], x + rng.uniform(-90, 90), y + rng.uniform(-90, 90), 0.0, "Erz", "Erzstelle",
                                     yaw=rng.uniform(0, 360), scale=(s_, s_, s_))
                    if erzmat():
                        ore.static_mesh_component.set_material(0, erzmat()[0])
                if erzmat():
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
                               yaw=rng.uniform(0, 360), scale=(s, s * rng.uniform(0.8, 1.1), s * rng.uniform(0.7, 1.0)), material=fels(),
                               pitch=rng.uniform(-10, 10), roll=rng.uniform(-10, 10))
            elif ch == "Q":
                # der Schatz am Ende des Kobold-Horts (Boris 2026-11-22): eine Truhe mit seltener Beute, im Schein der Kobaltblüte
                chest = spawn(unreal.VaelChest, unreal.Vector(x, y, 40.0), "Requisiten", "Schatz des Horts", unreal.Rotator(0.0, 0.0, rng.uniform(0, 360)))
                chest.set_editor_property("rolled_items", 4)
                chest.set_editor_property("min_rarity", unreal.VaelRarity.RARE)
                for k in range(6):
                    a = k / 6.0 * 2 * math.pi
                    s_ = rng.uniform(0.7, 1.0)
                    mesh_actor(meshes["SM_HK_Kiste"], x + math.cos(a) * 230.0, y + math.sin(a) * 230.0, 0.0, "Requisiten", "Hortkiste",
                               yaw=rng.uniform(0, 90), scale=(s_, s_, s_))
                point_light(x, y, 260.0, "Licht", "Hortschein", (1.0, 0.35, 0.6), 60.0, 1400.0, shadows=True)
            elif ch == "E":
                # der Auslöser an den äußeren Rand der Zelle, zur Felsseite: im Verbund führt manchmal ein Gang neben dem Ausgang vorbei,
                # und wer dort lief, streifte den Mund und stand wieder in der Welt (Lauftest 2026-11-22)
                ox, oy = 0.0, 0.0
                for key, (dr, dc, (dx, dy)) in DIRS.items():
                    if not is_floor(r + dr, c + dc):
                        ox, oy = dx * 170.0, dy * 170.0
                        break
                exit_ = spawn(unreal.VaelCaveEntrance, unreal.Vector(x + ox, y + oy, 120.0), "Spiel", "Ausgang")
                exit_.set_editor_property("target_level", unreal.Name("L_Vael"))
                ein = (REGION_INFO.get(REGIONEN[r][c], {}).get("eingang_m") if REGIONEN else None) or D["welt_eingang_m"]
                ex, ey = ein[:2]
                exit_.set_editor_property("return_place", unreal.Vector2D((ex + 7.0) * M, ey * M))
                exit_.set_editor_property("message", unreal.Text("Eisenschlund"))
                spot_down(x, y, 1400.0, "Licht", "Tageslicht am Ausgang", (0.8, 0.88, 1.0), 900.0, 12.0, beam)
                if start is None or (REGIONEN and REGIONEN[r][c] == "g"):
                    start = ankunft(r, c)
    # --- Schacht: Rippen rundum am Rand des Abgrunds, nicht nur an den gezeichneten Stellen
    if jede_art("rippen_auto"):
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
            hier(r, c)
            if any(math.hypot(x - lx, y - ly) < STYLE.get("licht_abstand", 4.0) * S for lx, ly in LIGHTS):
                continue
            light_source(meshes, r, c, x, y)

    # --- Bodenschutt: Kiesel und Brocken, je Höhle Knochen, Kristallsplitter oder Glutbrocken
    for r in range(H):
        for c in range(W):
            if cell(r, c) not in "." or rng.random() > 0.45:
                continue
            x, y = centre(r, c)
            hier(r, c)
            for k in range(rng.randint(1, 3)):
                px, py = x + rng.uniform(-180, 180), y + rng.uniform(-180, 180)
                kind = STYLE.get("schutt", "kiesel")
                if kind == "knochen" and rng.random() < 0.5:
                    s_ = rng.uniform(0.3, 0.5)
                    mesh_actor(meshes["SM_HK_Knochenhaufen"], px, py, -2.0, "Schutt", "Gebeine", yaw=rng.uniform(0, 360), scale=(s_, s_, s_),
                               collision=False)
                elif kind == "kristall" and rng.random() < 0.35 and kristalle():
                    s_ = rng.uniform(0.25, 0.45)
                    shard = mesh_actor(meshes["SM_HK_Salzspat_Klein"], px, py, -2.0, "Schutt", "Kristallsplitter", yaw=rng.uniform(0, 360),
                                       scale=(s_, s_, s_), collision=False)
                    shard.static_mesh_component.set_material(0, kristalle()[rng.randrange(len(kristalle()))][1])
                elif kind == "glut" and rng.random() < 0.3 and erzmat():
                    s_ = rng.uniform(0.12, 0.25)
                    ember = mesh_actor(rng.choice(BOULDERS), px, py, -3.0, "Schutt", "Glutbrocken", yaw=rng.uniform(0, 360), scale=(s_, s_, s_ * 0.7),
                                       material=erzmat()[0], collision=False)
                else:
                    s_ = rng.uniform(0.12, 0.4)
                    mesh_actor(rng.choice(BOULDERS), px, py, -s_ * 12.0, "Schutt", "Kiesel", yaw=rng.uniform(0, 360), scale=(s_, s_ * rng.uniform(0.7, 1.1), s_ * 0.7),
                               material=fels(), collision=False)

    if start is not None:
        spawn(unreal.PlayerStart, unreal.Vector(start[0], start[1], 120.0), "Spiel", "Spielerstart")

    counts["Wanderz"] = wand_erze(world)
    counts["Tropfsteine2"] = tropfsteine(world)
    counts["Katakombe"] = katakombe(meshes)
    counts["Decke"] = deckenboegen() + felsdecke()

    basis()
    if jede_art("abgrund"):
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


WASSER = [None]
ALLE_HOEHLEN = ["Gildenmine", "Olmgrotte", "Gebeinkammer", "FunkenerzAdern", "Ameisenbau", "KoboldStollen", "Rostschwamm", "TiefsterSchacht"]


def eingaenge():
    """Die Eingänge in der Welt: (Name, Titel, [x, y, Boden] in m, Portal bauen, Ziel-Level, Ankunft in cm oder None).
    Im Verbund führt jeder alte Eingang in seine Region der einen Map; der Kobold-Stollen hat keinen (ein Geheimnis)"""
    if not REGIONEN:
        return [(NAME, D.get("titel", NAME), D["welt_eingang_m"], D.get("portal_bauen"), "L_" + NAME, None)]
    liste = []
    for buchstabe, info in sorted(REGION_INFO.items()):
        if not info.get("eingang_m"):
            continue
        ausgaenge = [(r, c) for r in range(H) for c in range(W) if cell(r, c) == "E" and REGIONEN[r][c] == buchstabe]
        ziel = ankunft(*ausgaenge[0]) if ausgaenge else None
        liste.append((info["name"], info["titel"], info["eingang_m"], info.get("portal_bauen"), "L_" + NAME, ziel))
    return liste


def build_world_entrance():
    """In L_Vael: die Eingänge, die in diese Höhle führen (ersetzt die vom letzten Lauf; im Verbund auch die der alten Einzelhöhlen)"""
    if not levels.load_level(WORLD):
        log("konnte " + WORLD + " nicht öffnen")
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    try:
        descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
        unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
    except Exception as error:  # noqa: BLE001
        log("konnte nicht alle Actors laden: {}".format(error))
    alte = {unreal.Name("VaelHoehle_" + n) for n in ([NAME] + (ALLE_HOEHLEN if REGIONEN else []))}
    for actor in editor.get_all_level_actors():
        if any(t in alte for t in actor.get_editor_property("tags")):
            editor.destroy_actor(actor)
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    stone = unreal.load_asset("/Game/Vael/Maps/Blockout/MI_Blockout_Stone")
    dark = unreal.load_asset("/Game/Vael/Maps/Blockout/MI_Blockout_Dunkel")
    for name, titel, (ex, ey, ground), portal, ziel_level, ziel in eingaenge():
        tag = unreal.Name("VaelHoehle_" + name)
        z = ground * M        # die Bodenhöhe aus den Ortsdaten (Abtasten von oben traf den Felssturz über dem Portal)
        mouth = editor.spawn_actor_from_class(unreal.VaelCaveEntrance, unreal.Vector(ex * M - 80.0, ey * M, z + 120.0), unreal.Rotator(0, 0, 0))
        mouth.set_actor_label("Eingang " + titel)
        mouth.set_folder_path("Eisenschlund/Hoehlen/" + name)
        mouth.set_editor_property("tags", [tag])
        mouth.set_editor_property("target_level", unreal.Name(ziel_level))
        mouth.set_editor_property("message", unreal.Text(titel))
        if ziel is not None:
            mouth.set_editor_property("return_place", unreal.Vector2D(ziel[0], ziel[1]))
        if portal:
            # an diesem Ort stand noch kein Stollenportal (BuildEisenschlund.py baut nur einige): zwei Felspfeiler, ein Sturz, Dunkel dahinter
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
                part.set_folder_path("Eisenschlund/Hoehlen/" + name)
                part.set_editor_property("tags", [tag])
        log("Eingang {} in L_Vael bei {:.0f} / {:.0f} m -> {} {}".format(titel, ex, ey, ziel_level, ziel))
    unreal.EditorLoadingAndSavingUtils.save_map(world, WORLD)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)


def main():
    if os.environ.get("VAEL_HOEHLE_NUR_WASSER"):
        # nur das Wassermaterial neu (VAEL_HOEHLE_WASSER_NEU=1 dazu), ohne die Höhle neu zu bauen
        wasser_material()
        log("Wassermaterial neu")
        return
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
    # die echten Knochen (Fab-Textur, gealtert; Scripts/ImportKnochenFab.py), sonst der schlichte Ton
    EXTRA_SLOTS["Gebein"] = unreal.load_asset("/Game/Vael/Requisiten/Knochen/MI_Gebein_Hoehle") or EXTRA_SLOTS["Knochen"]
    EXTRA_SLOTS["Dunkel"] = simple_material("M_HK_Dunkel", (0.015, 0.012, 0.01), 0.9)
    EXTRA_SLOTS["Schleim"] = simple_material("M_HK_Schleim_Gruen", (0.1, 0.3, 0.06), 0.08, emissive=(1.0, 3.0, 0.6))
    lachen = sorted({f for art in ARTEN for f in (list(SCHLEIM) if ART[art].get("schaedelwand") else ART[art].get("schleim", []))})
    for farbe in lachen:
        if farbe in SCHLEIM:
            SLIME_MATS[farbe] = flow_material(farbe, SCHLEIM[farbe], 0.3)   # 2,2 und 0,7 waren weiß überstrahlt (Belichtung der Höhlen +1,9)
        else:
            # Glut und Gift glühen gedämpft (mit 3-fachem Leuchten waren sie weiß überstrahlt)
            SLIME_MATS[farbe] = flow_material(farbe, LACHEN[farbe], 0.2)

    def glow_mat(prefix, col):
        name = "{}_{:02x}{:02x}{:02x}".format(prefix, int(col[0] * 255), int(col[1] * 255), int(col[2] * 255))
        return simple_material(name, (col[0] * 0.5 + 0.2, col[1] * 0.5 + 0.2, col[2] * 0.5 + 0.2), 0.15, emissive=(col[0] * 3.0, col[1] * 3.0, col[2] * 3.0))
    CRYSTAL_MATS[:] = [(col, glow_mat("M_HK_Kristall", col)) for col in STYLE.get("kristalle", [])]
    if STYLE.get("erzfarbe"):
        ORE_MAT[:] = [glow_mat("M_HK_Erz", STYLE["erzfarbe"])]
    # je Höhlenart der Map ihr Fels, Boden, ihre Kristalle und Erze; im Verbund zieht der Fels halb zur Grundfarbe des Eisenschlunds hin
    # (ein roter Faden durch alle Höhlen, die Regionen bleiben erkennbar)
    for art in ARTEN:
        a = ART[art]
        if REGIONEN:
            mix = lambda col, ziel: tuple(v * 0.55 + w * 0.45 for v, w in zip(col, ziel))
            ROCKS[art] = rock_instance("MI_HK_Fels_V_" + art, mix(a["fels"], ART["Eisenschlund"]["fels"]))
            FLOORS[art] = rock_instance("MI_HK_Boden_V_" + art, tuple(min(1.0, v * 1.9) for v in mix(a["boden"], ART["Eisenschlund"]["boden"])))
        else:
            ROCKS[art], FLOORS[art] = rock, floor_mat
        KRISTALLE[art] = [(col, glow_mat("M_HK_Kristall", col)) for col in a.get("kristalle", [])]
        ERZMATS[art] = [glow_mat("M_HK_Erz", a["erzfarbe"])] if a.get("erzfarbe") else []
    SPONGE[:] = [simple_material("M_HK_Rostschwamm", (0.26, 0.11, 0.04), 0.9)]
    EXTRA_SLOTS["Fels"] = rock
    for name in PROPS:
        # Bögen und Türstöcke: man geht darunter durch (mit der einfachen Hülle versperrte der Türstock den ganzen Stollen, Lauftest 22.11.)
        meshes[name] = import_piece(name, collision_complex=name.startswith("SM_HK_Schaedelbogen") or name.startswith("SM_HK_Verbau"))
        kit_materials(meshes[name])
    ZAPFEN[:] = [meshes["SM_HK_Schleimzapfen_A"], meshes["SM_HK_Schleimzapfen_B"]]
    build_cave(meshes, rock, floor_mat, ore_mat, water_mat, crystal_mat)
    build_map()
    if not os.environ.get("VAEL_HOEHLE_OHNE_WELT"):
        build_world_entrance()
    log("fertig {}: {}".format(NAME, counts))


main()
