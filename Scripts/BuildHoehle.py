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
               "SM_HK_Ecke_A", "SM_HK_Ecke_B", "SM_HK_Ecke_Niedrig", "SM_HK_Boden_A", "SM_HK_Boden_B"]
PROPS = ["SM_HK_Verbau", "SM_HK_Verbau_8", "SM_HK_Verbau_12", "SM_HK_Schiene", "SM_HK_Lore", "SM_HK_Kiste", "SM_HK_Balkenstapel", "SM_HK_Laterne"]
KIT_SLOTS = {"Holz": "MI_GK_Holz", "Leder": "M_GK_Leder", "Metall": "M_GK_Metall", "Faden": "M_GK_Faden", "Glut": "M_GK_Glut",
             "Moos": "M_GK_Moos", "Messing": "/Game/Vael/Refugium/Glutkammer/Werkstatt/M_IW_Messing"}

# Wie die Höhlenarten aussehen (Farben des Felses, Licht); weitere Arten kommen dazu
ART = {
    "Gildenmine": {"fels": (0.5, 0.45, 0.4), "boden": (0.36, 0.32, 0.29), "laterne": (1.0, 0.62, 0.32), "nebel": (0.09, 0.07, 0.055),
                   "grundlicht": 1.3},
}

editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.MaterialEditingLibrary
rng = random.Random(1515)
counts = {}

D = json.load(open(os.path.join(DATA, NAME + ".json"), encoding="utf-8"))
GRID = D["grundriss"]
H, W = len(GRID), max(len(r) for r in GRID)
S = D.get("zelle_m", 4.0) * M
STYLE = ART[D.get("art", "Gildenmine")]


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


def kit_materials(mesh):
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.get_editor_property("material_slot_name")).split(".")[0]
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


def point_light(x, y, z, folder, label, colour, intensity, radius, shadows=False):
    light = spawn(unreal.PointLight, unreal.Vector(x, y, z), folder, label)
    c = light.light_component
    c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    c.set_editor_property("intensity", intensity)
    c.set_light_color(unreal.LinearColor(colour[0], colour[1], colour[2], 1.0))
    c.set_editor_property("attenuation_radius", radius)
    c.set_editor_property("cast_shadows", shadows)
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


def build_cave(meshes, rock, floor_mat, ore_mat):
    if assets.does_asset_exist(LEVEL):
        assets.delete_asset(LEVEL)
    assets.make_directory("/Game/Vael/Maps/Hoehlen")
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    beam = unreal.load_asset(KIT + "/SM_GK_Lichtstrahl")
    boulders = [unreal.load_asset("/Game/Vael/Maps/Eisenschlund/Fels/SM_ES_Brocken_" + k) for k in "ABC"]
    walls = [meshes["SM_HK_Wand_A"], meshes["SM_HK_Wand_B"], meshes["SM_HK_Wand_C"]]
    lows = [meshes["SM_HK_Wand_Niedrig_A"], meshes["SM_HK_Wand_Niedrig_B"]]

    spawn(unreal.VaelIndoorLevel, unreal.Vector(0, 0, 500), "Spiel", "Innenraum")
    # ein Felssockel unter der ganzen Höhle: nichts fällt durch eine Lücke zwischen den Platten ins Leere
    mesh_actor(unreal.load_asset("/Engine/BasicShapes/Cube.Cube"), (H - 1) * S / 2, (W - 1) * S / 2, -55.0, "Boden", "Felssockel",
               scale=((H + 2) * S / 100.0, (W + 2) * S / 100.0, 1.0), material=floor_mat)

    for r in range(H):
        for c in range(W):
            ch = cell(r, c)
            if ch == "#":
                continue
            x, y = centre(r, c)
            mesh_actor(rng.choice([meshes["SM_HK_Boden_A"], meshes["SM_HK_Boden_B"]]), x, y, 0.0, "Boden", "Boden",
                       yaw=rng.choice((0, 90, 180, 270)), material=floor_mat)
            # Wände zu jedem Felsnachbarn
            for key, (dr, dc, (dx, dy)) in DIRS.items():
                if is_floor(r + dr, c + dc):
                    continue
                low = dx * CAMERA[0] + dy * CAMERA[1] > 0.3
                yaw = math.degrees(math.atan2(-dx, dy)) + rng.uniform(-4, 4)
                mesh_actor(rng.choice(lows if low else walls), x + dx * S / 2, y + dy * S / 2, 0.0, "Waende", "Felswand niedrig" if low else "Felswand",
                           yaw=yaw, scale=(1.0, rng.uniform(0.85, 1.15), rng.uniform(0.9, 1.15) if not low else 1.0), material=rock)

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
            mesh_actor(piece, px, py, -20.0, "Waende", "Eckpfeiler", yaw=rng.uniform(0, 360), scale=(s, s, rng.uniform(0.9, 1.1)), material=rock)

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
            elif ch == "B":
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
    if start is not None:
        spawn(unreal.PlayerStart, unreal.Vector(start[0], start[1], 120.0), "Spiel", "Spielerstart")

    # Grundlicht: ein schwaches, kühles Licht schräg von oben (die Höhle ist für die Kamera oben offen), das die Formen zeigt;
    # die Laternen und Schächte bleiben das Helle
    moon = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 3000.0), "Licht", "Grundlicht von oben", unreal.Rotator(0.0, -62.0, 35.0))
    mc = moon.light_component
    mc.set_editor_property("intensity", STYLE.get("grundlicht", 0.35))
    mc.set_light_color(unreal.LinearColor(0.62, 0.7, 0.85, 1.0))
    mc.set_editor_property("cast_shadows", True)
    moon.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    # Licht und Luft: kein Himmel; ein schwaches Grundlicht im Nebel, damit das Dunkel nicht schwarz ist
    fog = spawn(unreal.ExponentialHeightFog, unreal.Vector(H * S / 2, W * S / 2, 0.0), "Licht", "Grubenluft")
    f = fog.component
    f.set_editor_property("fog_density", 0.03)
    f.set_editor_property("fog_height_falloff", 0.35)
    n = STYLE["nebel"]
    f.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(n[0], n[1], n[2], 1.0))
    f.set_volumetric_fog(True)
    post = spawn(unreal.PostProcessVolume, unreal.Vector(H * S / 2, W * S / 2, 0.0), "Licht", "Nachbearbeitung")
    post.set_editor_property("unbound", True)
    settings = post.get_editor_property("settings")
    for key, value in (("auto_exposure_min_brightness", 0.0), ("auto_exposure_max_brightness", 0.0), ("auto_exposure_bias", 1.4),
                       ("bloom_intensity", 0.9), ("vignette_intensity", 0.55)):
        settings.set_editor_property("override_" + key, True)
        settings.set_editor_property(key, value)
    post.set_editor_property("settings", settings)

    if unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL):
        log("gespeichert: " + LEVEL)
    else:
        unreal.log_error("[BuildHoehle] " + LEVEL + " konnte nicht gespeichert werden")


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
    unreal.EditorLoadingAndSavingUtils.save_map(world, WORLD)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("Eingang in L_Vael bei {:.0f} / {:.0f} m, Boden {:.1f} m".format(ex, ey, z / M))


def main():
    assets.make_directory(PARTS)
    meshes = {}
    for name in ROCK_PIECES:
        meshes[name] = import_piece(name, collision_complex=("Wand" in name or "Boden" in name or "Ecke" in name))
        assets.save_loaded_asset(meshes[name], False)
    for name in PROPS:
        meshes[name] = import_piece(name)
        kit_materials(meshes[name])
    cave_rock_material()
    rock = rock_instance("MI_HK_Fels_" + D.get("art", NAME), STYLE["fels"])
    floor_mat = rock_instance("MI_HK_Boden_" + D.get("art", NAME), STYLE["boden"])
    ore_mat = rock_instance("MI_HK_Erz_Platzhalter", (0.75, 0.32, 0.16))
    build_cave(meshes, rock, floor_mat, ore_mat)
    if not os.environ.get("VAEL_HOEHLE_OHNE_WELT"):
        build_world_entrance()
    log("fertig {}: {}".format(NAME, counts))


main()
