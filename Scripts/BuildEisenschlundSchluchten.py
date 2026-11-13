# Gibt den Schluchten des Eisenschlunds Höhlenstimmung (Boris 13.11.2026: "Schluchten, bei denen es aussieht, als wäre man
# schon tiefer", Diablo-Höhlen nur als Inspiration: hohe Steinwände, Stalaktiten und Stalagmiten, Licht).
#
# - Importiert die Felsteile aus Tools/Welt/eisenschlund_fels.py (Vael_Quellen/Eisenschlund/export) nach
#   /Game/Vael/Maps/Eisenschlund/Fels und baut M_ES_Fels: Fels nach Weltposition (keine UVs nötig), kühles Blaugrau,
#   unten dunkler und feucht.
# - Entlang jeder Schlucht (ES_Orte.json "schluchten"): Felswände auf beiden Seiten, die den Hang zur Wand machen, alle
#   ~170 m ein Felsbogen über die Schlucht mit Stalaktiten darunter, Stalagmiten-Gruppen am Rand des Bodens, Tropfsteine an
#   den Wänden.
# - Licht: Lichtkegel von oben (Spotlichter durch Staub), warme Grubenlaternen am Weg, und in jeder Schlucht eine dunklere,
#   kühlere Stimmung (Post Process) mit Staub in der Luft (Local Fog).
# Alles trägt den Tag "VaelSchluchten" (Ordner "Eisenschlund/Schluchten/..."); ein neuer Lauf ersetzt nur das.
#
# Mit geschlossenem Editor, nach BuildEisenschlund.py:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildEisenschlundSchluchten.py" -AllowCommandletRendering -unattended -nosplash

import array
import json
import math
import os
import random

import unreal

LEVEL = "/Game/Vael/Maps/L_Vael"
DATA = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", "Welt")
SOURCE = "C:/Projekte/Vael_Quellen/Eisenschlund/export"
ASSETS = "/Game/Vael/Maps/Eisenschlund/Fels"
TAG = "VaelSchluchten"
FOLDER = "Eisenschlund/Schluchten"
PIECES = ["SM_ES_Wand_A", "SM_ES_Wand_B", "SM_ES_Wand_C", "SM_ES_Stalagmit_A", "SM_ES_Stalagmit_B", "SM_ES_Stalagmit_C",
          "SM_ES_Stalaktit_A", "SM_ES_Stalaktit_B", "SM_ES_Bogen"]
ROCK_TEXTURE = "/Game/Rock_Collection_04/Meshes/Rock_01/Textures/T_Rock_01_A"
M = 100.0
STEP_WALL = 20.0          # m zwischen zwei Wandstücken
STEP_ARCH = 170.0         # m zwischen zwei Felsbögen
STEP_SHAFT = 110.0        # m zwischen zwei Lichtkegeln
STEP_LANTERN = 38.0       # m zwischen zwei Laternen

editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assets = unreal.EditorAssetLibrary
library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
rng = random.Random(1313)
counts = {}

D = json.load(open(os.path.join(DATA, "ES_Orte.json"), encoding="utf-8"))
heights = {}


def log(message):
    unreal.log_warning("[BuildEisenschlundSchluchten] " + message)


def ground_at(x, y):
    for name, land in D["landschaften"].items():
        x0, y0 = land["ursprung_m"]
        nx, ny = land["punkte"]
        step = land["schritt_m"]
        fx, fy = (x - x0) / step, (y - y0) / step
        if 0 <= fx <= nx - 1.001 and 0 <= fy <= ny - 1.001:
            ix, iy = int(fx), int(fy)
            tx, ty = fx - ix, fy - iy
            h = heights[name]

            def v(i, j):
                return (h[j * nx + i] - 32768) / 128.0
            return (v(ix, iy) * (1 - tx) + v(ix + 1, iy) * tx) * (1 - ty) + (v(ix, iy + 1) * (1 - tx) + v(ix + 1, iy + 1) * tx) * ty
    return 0.0


# ---------------------------------------------------------------- assets

def import_pieces():
    meshes = {}
    for name in PIECES:
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
        task.set_editor_property("destination_path", ASSETS)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", ui)
        tools.import_asset_tasks([task])
        mesh = unreal.load_asset("{}/{}".format(ASSETS, name))
        # Wände und Bögen mit ihrer echten Form als Kollision: eine Hülle füllte den Raum unter dem Bogen und klemmte Spieler ein
        if mesh is not None and ("Wand" in name or "Bogen" in name):
            body = mesh.get_editor_property("body_setup")
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            assets.save_loaded_asset(mesh, False)
        meshes[name] = mesh
    return meshes


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

    def mask(self, source, column, r=False, g=False, b=False):
        e = self.node(unreal.MaterialExpressionComponentMask, column, r=r, g=g, b=b, a=False)
        self.link(source, e)
        return e

    def append(self, a, b, column):
        return self.op(unreal.MaterialExpressionAppendVector, column, a, b)

    def sample(self, texture, uv, column):
        e = self.node(unreal.MaterialExpressionTextureSample, column, texture=texture)
        self.link(uv, e, "UVs")
        return self.mask(e, column - 1, r=True, g=True, b=True)

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


def rock_material():
    """Fels nach Weltposition, dreifach projiziert; kühles Blaugrau, zum Boden hin dunkler und feucht"""
    path = ASSETS + "/M_ES_Fels"
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
    else:
        material = tools.create_asset("M_ES_Fels", ASSETS, unreal.Material, unreal.MaterialFactoryNew())
    tex = unreal.load_asset(ROCK_TEXTURE)
    g = Graph(material)
    pos = g.op(unreal.MaterialExpressionDivide, 12, g.node(unreal.MaterialExpressionWorldPosition, 13), g.const(700.0, 13))
    x, y, z = g.mask(pos, 11, r=True), g.mask(pos, 11, g=True), g.mask(pos, 11, b=True)
    normal = g.node(unreal.MaterialExpressionAbs, 11)
    g.link(g.node(unreal.MaterialExpressionVertexNormalWS, 12), normal)
    nx, nz = g.mask(normal, 10, r=True), g.mask(normal, 10, b=True)
    side_a = g.sample(tex, g.append(y, z, 10), 9)
    side_b = g.sample(tex, g.append(x, z, 10), 9)
    top = g.sample(tex, g.append(x, y, 10), 9)
    sides = g.lerp(side_b, side_a, g.saturate(g.op(unreal.MaterialExpressionMultiply, 8, nx, g.const(1.4, 9)), 7), 6)
    rock = g.lerp(sides, top, g.saturate(g.op(unreal.MaterialExpressionMultiply, 7, g.op(unreal.MaterialExpressionSubtract, 8, nz, g.const(0.6, 9)),
                                               g.const(3.0, 8)), 6), 5)
    tint = g.mask(g.node(unreal.MaterialExpressionVectorParameter, 6, parameter_name="FelsFarbe", default_value=unreal.LinearColor(0.42, 0.46, 0.55, 1.0)),
                  5, r=True, g=True, b=True)
    rock = g.op(unreal.MaterialExpressionMultiply, 4, rock, tint)
    # Unten dunkler: Höhe über dem Schluchtboden (Parameter), zum Boden hin fast schwarz und nass
    height_m = g.op(unreal.MaterialExpressionDivide, 6, g.mask(g.node(unreal.MaterialExpressionWorldPosition, 8), 7, b=True), g.const(100.0, 7))
    floor = g.node(unreal.MaterialExpressionScalarParameter, 7, parameter_name="Bodenhoehe", default_value=15.0)
    up = g.saturate(g.op(unreal.MaterialExpressionDivide, 5, g.op(unreal.MaterialExpressionSubtract, 6, height_m, floor), g.const(45.0, 6)), 4)
    dark = g.op(unreal.MaterialExpressionMultiply, 3, rock, g.lerp(g.const(0.35, 4), g.const(1.0, 4), up, 3))
    library.connect_material_property(dark, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(g.lerp(g.const(0.45, 3), g.const(0.85, 3), up, 2), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


# ---------------------------------------------------------------- placing

def spawn(cls, location, folder, label, rotation=None):
    actor = editor.spawn_actor_from_class(cls, location, rotation or unreal.Rotator(0.0, 0.0, 0.0))
    actor.set_folder_path("{}/{}".format(FOLDER, folder))
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(TAG)])
    key = folder.split("/")[-1]
    counts[key] = counts.get(key, 0) + 1
    return actor


def piece(mesh, material, x, y, z, yaw, scale, folder, label, pitch=0.0, roll=0.0):
    actor = spawn(unreal.StaticMeshActor, unreal.Vector(x * M, y * M, z * M), folder, label, unreal.Rotator(roll, pitch, yaw))
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.static_mesh_component.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    # Nicht im Lumen-Szenenspeicher (Surface Cache): Hunderte großer Felsteile sprengten ihn, die Schlucht wurde schwarz.
    # Sie werden weiter beleuchtet und werfen Schatten, tragen nur selbst kein indirektes Licht bei.
    actor.static_mesh_component.set_editor_property("affect_dynamic_indirect_lighting", False)
    actor.static_mesh_component.set_editor_property("affect_distance_field_lighting", False)
    return actor


def along(line, step):
    """Punkte entlang einer Linie in gleichen Abständen, mit Richtung (dx, dy normiert)"""
    out = []
    rest = 0.0
    for a, b in zip(line, line[1:]):
        length = math.hypot(b["x"] - a["x"], b["y"] - a["y"])
        if length < 1e-3:
            continue
        dx, dy = (b["x"] - a["x"]) / length, (b["y"] - a["y"]) / length
        t = rest
        while t < length:
            out.append((a["x"] + dx * t, a["y"] + dy * t, dx, dy))
            t += step
        rest = t - length
    return out


FREI = [(p["x"], p["y"], 26.0) for p in D["lager"].values()] + [(p["x"], p["y"], 24.0) for p in D["kampf"]]


def frei(x, y):
    return all(math.hypot(x - fx, y - fy) > r for fx, fy, r in FREI)


def dress(name, canyon, meshes, rock, bright):
    line = canyon["linie"]
    half = canyon["boden_halbe_breite"]
    walls = [meshes["SM_ES_Wand_A"], meshes["SM_ES_Wand_B"], meshes["SM_ES_Wand_C"]]
    mites = [meshes["SM_ES_Stalagmit_A"], meshes["SM_ES_Stalagmit_B"], meshes["SM_ES_Stalagmit_C"]]
    tites = [meshes["SM_ES_Stalaktit_A"], meshes["SM_ES_Stalaktit_B"]]
    folder = name

    # Felswände: die Front (lokal -Y) schaut in die Schlucht; der Fuß sitzt etwas unter dem Boden am Wandfuß.
    # Auf der Kameraseite nur eine niedrige Felskante (sonst verdeckt sie den Spieler), gegenüber die hohen Wände.
    cam = canyon.get("kamera_seite", [-0.7071, -0.7071])
    for x, y, dx, dy in along(line, STEP_WALL):
        for side in (-1, 1):
            nxv, nyv = -dy * side, dx * side                       # nach außen
            camera_side = nxv * cam[0] + nyv * cam[1] > 0.0
            off = half + 3.0 + rng.uniform(-1.5, 2.0)
            px, py = x + nxv * off, y + nyv * off
            floor = ground_at(x, y)
            yaw = math.degrees(math.atan2(-nyv, -nxv)) + 90.0     # lokale -Y-Achse zeigt zur Mitte
            sc = rng.uniform(0.9, 1.15)
            tall = rng.uniform(0.3, 0.42) if camera_side else rng.uniform(1.6, 2.2)
            piece(rng.choice(walls), rock, px, py, floor - 4.0, yaw + rng.uniform(-6, 6), (sc, rng.uniform(0.8, 1.2), tall),
                  folder + "/Waende", "Felskante" if camera_side else "Felswand", roll=rng.uniform(-3, 3))

    # Felsbögen quer über die Schlucht, darunter Stalaktiten
    for x, y, dx, dy in along(line, STEP_ARCH)[1:]:
        floor = ground_at(x, y)
        span = 2.0 * half + 14.0
        yaw = math.degrees(math.atan2(dy, dx)) + 90.0              # Bogen-X quer zur Schlucht
        hs = rng.uniform(1.0, 1.2)
        piece(meshes["SM_ES_Bogen"], rock, x, y, floor - 1.5, yaw, (span / 34.0, rng.uniform(1.0, 1.4), hs), folder + "/Boegen", "Felsbogen")
        top = floor - 1.5 + 20.0 * hs - 3.5
        for k in range(rng.randint(4, 7)):
            t = rng.uniform(-0.35, 0.35)
            sx, sy = x - dy * t * span, y + dx * t * span
            drop = top - 2.0 * (abs(t) * 3.0) ** 2
            length = rng.uniform(2.5, 6.0)
            piece(rng.choice(tites), rock, sx + dx * rng.uniform(-1.5, 1.5), sy + dy * rng.uniform(-1.5, 1.5), drop, rng.uniform(0, 360),
                  (length * 0.45, length * 0.45, length), folder + "/Stalaktiten", "Stalaktit")

    # Stalagmiten: Gruppen am Rand des Bodens, der Weg in der Mitte bleibt frei
    for x, y, dx, dy in along(line, 26.0):
        if rng.random() < 0.35:
            continue
        side = rng.choice((-1, 1))
        for k in range(rng.randint(2, 5)):
            off = rng.uniform(half * 0.45, half + 1.0) * side
            px, py = x - dy * off + dx * rng.uniform(-6, 6), y + dx * off + dy * rng.uniform(-6, 6)
            if not frei(px, py):
                continue
            height = rng.uniform(2.0, 9.0) if abs(off) > half * 0.7 else rng.uniform(1.2, 3.5)
            piece(rng.choice(mites), rock, px, py, ground_at(px, py) - 0.3, rng.uniform(0, 360), (height * 0.5, height * 0.5, height),
                  folder + "/Stalagmiten", "Stalagmit", pitch=rng.uniform(-6, 6), roll=rng.uniform(-6, 6))
        # Tropfsteine an der Wand, die aus der Wand ragen und herabhängen
        if rng.random() < 0.5:
            side = 1 if (-dy * cam[0] + dx * cam[1]) < 0.0 else -1     # die hohe Wand, nicht die Kante zur Kamera
            off = (half + 1.0) * side
            px, py = x - dy * off, y + dx * off
            length = rng.uniform(3.0, 7.0)
            piece(rng.choice(tites), rock, px, py, ground_at(x, y) + rng.uniform(18.0, 32.0), rng.uniform(0, 360), (length * 0.5, length * 0.5, length),
                  folder + "/Stalaktiten", "Wandzapfen")

    # Lichtkegel von oben: Spotlicht hoch über dem Boden, senkrecht nach unten, im Staub sichtbar
    for x, y, dx, dy in along(line, STEP_SHAFT):
        floor = ground_at(x, y)
        light = spawn(unreal.SpotLight, unreal.Vector((x + rng.uniform(-6, 6)) * M, (y + rng.uniform(-6, 6)) * M, (floor + 70.0) * M),
                      folder + "/Licht", "Lichtkegel", unreal.Rotator(0.0, -90.0, 0.0))
        c = light.spot_light_component
        # In Candela, maßvoll: ein zu heller Kegel lässt die automatische Belichtung alles andere schwarz machen
        c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        c.set_editor_property("intensity", 300.0 * bright)
        c.set_editor_property("light_color", unreal.Color(r=255, g=236, b=200, a=255))
        c.set_editor_property("attenuation_radius", 9000.0)
        c.set_editor_property("outer_cone_angle", 9.0)
        c.set_editor_property("inner_cone_angle", 4.0)
        c.set_editor_property("cast_shadows", False)
        c.set_editor_property("volumetric_scattering_intensity", 3.0)

    # Grubenlaternen am Weg: warmes Licht in Augenhöhe, abwechselnd links und rechts
    for index, (x, y, dx, dy) in enumerate(along(line, STEP_LANTERN)):
        side = 1 if index % 2 else -1
        off = rng.uniform(5.0, 8.0) * side
        px, py = x - dy * off, y + dx * off
        floor = ground_at(px, py)
        post = piece(unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder"), wood_material(), px, py, floor, 0.0, (0.15, 0.15, 2.4),
                     folder + "/Laternen", "Laternenpfahl")
        post.static_mesh_component.set_collision_profile_name("NoCollision")
        glow = piece(unreal.load_asset("/Engine/BasicShapes/Sphere.Sphere"), lantern_material(), px, py, floor + 2.3, 0.0, (0.3, 0.3, 0.4),
                     folder + "/Laternen", "Laterne")
        glow.static_mesh_component.set_collision_profile_name("NoCollision")
        spawn(unreal.VaelFireLight, unreal.Vector(px * M, py * M, (floor + 2.6) * M), folder + "/Laternen", "Laternenlicht")

    # Stimmung: je Abschnitt ein Kasten, der dunkler und kühler macht, mit Staub in der Luft
    for a, b in zip(line, line[1:]):
        length = math.hypot(b["x"] - a["x"], b["y"] - a["y"])
        cx, cy = (a["x"] + b["x"]) * 0.5, (a["y"] + b["y"]) * 0.5
        floor = ground_at(cx, cy)
        yaw = math.degrees(math.atan2(b["y"] - a["y"], b["x"] - a["x"]))
        volume = spawn(unreal.PostProcessVolume, unreal.Vector(cx * M, cy * M, (floor + 25.0) * M), folder + "/Stimmung", "Schluchtstimmung",
                       unreal.Rotator(0.0, 0.0, yaw))
        volume.set_actor_scale3d(unreal.Vector((length + 30.0) * M / 200.0, (2.0 * half + 10.0) * M / 200.0, 60.0 * M / 200.0))
        volume.set_editor_property("blend_radius", 1500.0)
        volume.set_editor_property("priority", 2.0)
        settings = volume.get_editor_property("settings")
        settings.set_editor_property("override_auto_exposure_bias", True)
        settings.set_editor_property("auto_exposure_bias", -0.6)
        settings.set_editor_property("override_vignette_intensity", True)
        settings.set_editor_property("vignette_intensity", 0.7)
        settings.set_editor_property("override_color_gain", True)
        settings.set_editor_property("color_gain", unreal.Vector4(0.9, 0.95, 1.05, 1.0))
        volume.set_editor_property("settings", settings)
        fog = spawn(unreal.LocalFogVolume, unreal.Vector(cx * M, cy * M, (floor + 12.0) * M), folder + "/Stimmung", "Staub",
                    unreal.Rotator(0.0, 0.0, yaw))
        fog.set_actor_scale3d(unreal.Vector((length + 30.0) / 10.0, (2.0 * half + 10.0) / 10.0, 4.0))
        component = fog.get_component_by_class(unreal.LocalFogVolumeComponent)
        if component is not None:
            component.set_editor_property("radial_fog_extinction", 0.03)
            component.set_editor_property("height_fog_extinction", 0.025)
            component.set_editor_property("fog_albedo", unreal.LinearColor(0.55, 0.5, 0.45, 1.0))


_wood = {}


def wood_material():
    if "m" not in _wood:
        _wood["m"] = shape_material("MI_Blockout_Holz", "6a4e33")
    return _wood["m"]


def lantern_material():
    if "l" not in _wood:
        _wood["l"] = shape_material("MI_Blockout_Grubenlaterne", "ffd27a")
    return _wood["l"]


def shape_material(name, hex_code):
    path = "/Game/Vael/Maps/Blockout/" + name
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    instance = tools.create_asset(name, "/Game/Vael/Maps/Blockout", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(instance, unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"))

    def lin(c):
        value = int(c, 16) / 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4
    library.set_material_instance_vector_parameter_value(instance, "Color", unreal.LinearColor(lin(hex_code[0:2]), lin(hex_code[2:4]), lin(hex_code[4:6]), 1.0))
    assets.save_loaded_asset(instance, False)
    return instance


def main():
    for name in D["landschaften"]:
        raw = array.array("H")
        raw.frombytes(open(os.path.join(DATA, "ES_{}_Hoehen.r16".format(name)), "rb").read())
        heights[name] = raw

    assets.make_directory(ASSETS)
    meshes = import_pieces()
    missing = [n for n, m in meshes.items() if m is None]
    if missing:
        log("Felsteile fehlen: {} (zuerst Tools/Welt/eisenschlund_fels.py in Blender)".format(missing))
        return
    rock = rock_material()

    if not levels.load_level(LEVEL):
        log("konnte " + LEVEL + " nicht öffnen")
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    try:
        descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
        unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
    except Exception as error:  # noqa: BLE001
        log("konnte nicht alle Actors laden: {}".format(error))
    old = [a for a in editor.get_all_level_actors() if unreal.Name(TAG) in a.get_editor_property("tags")]
    for actor in old:
        editor.destroy_actor(actor)
    log("{} Actors des letzten Laufs entfernt".format(len(old)))

    for name, canyon in D["schluchten"].items():
        dress(name, canyon, meshes, rock, bright=1.0)

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("fertig: gesetzt {}".format(counts))


main()
