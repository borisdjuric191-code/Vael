# Puts Wachbeeren (Herbarium, Tripo model SM_Wachbeere) into L_Wurzelforst_Blockout: tall weeping shrubs with hanging
# eye-berries, alone or in small groups of two or three, in the thickets of the Hain and the Forstsaum.
#
# - Material M_WF_Wachbeere: the Tripo picture; every eye opens and closes in its own rhythm (UV map "Takt" from
#   Tools/Kreaturenfabrik/blender/vael_augen_vermessen.py: U = phase, V = lid zone), a closed eye is covered by a dark
#   teal lid, an open one glows light turquoise, softly by day and much stronger at night ("Nacht" from the clock).
#   The canes sway a little (World Position Offset).
# - Material M_WF_Wachbeere_Strahl: an additive light beam out of each open eye, blinking with it (same rhythm,
#   phase in custom data 0), fading towards its end and into the ground.
# - SM_WF_Lichtkegel: an open cone (tip at 0, 100 cm long along -Z, 50 cm wide), made in Blender.
# - Each shrub is one AVaelWatchBerry (C++): it sets the beams from Vael_Quellen/Wachbeere/03_analyse/augen.json,
#   sweeps them slowly and swings them over to a player nearby.
# - LODs on the mesh, textures at 2K except the normal map.
# Everything it adds is tagged WF_Wachbeere (Outliner folder Wurzelforst/Wachbeeren); rerunning replaces only that.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildWachbeeren.py" -EnablePlugins=PythonScriptPlugin

import json
import math
import os
import random
import struct
import zlib

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KIT = os.path.join(ROOT, "Blockout", "Vael_Wurzelforst_Blockout")
SOURCES = os.path.join(os.path.dirname(ROOT), "Vael_Quellen", "Wachbeere")
EYE_DATA = os.path.join(SOURCES, "03_analyse", "augen.json")
BEAM_FBX = os.path.join(SOURCES, "04_export", "SM_WF_Lichtkegel.fbx")
BERRY = "/Game/Vael/Requisiten/Wurzelforst/Wachbeere"
MATERIAL_PATH = BERRY + "/M_WF_Wachbeere"
BEAM_MATERIAL_PATH = BERRY + "/M_WF_Wachbeere_Strahl"
TAG = "WF_Wachbeere"
FOLDER = "Wurzelforst/Wachbeeren"
SEED = 53

SPOTS = 16                    # places with Wachbeeren over the forest
GROUP = (1, 3)                # shrubs at one place
GROUP_REACH = 420.0           # cm from the place to a shrub of its group
HEIGHTS = (250.0, 330.0)      # cm (Boris: big enough that the blinking eyes read from the camera)
MESH_HEIGHT = 300.0           # cm, as exported by the 'requisite' step
SINK = 8.0                    # cm the root bulb sinks into the ground
DRY_Z = 60.0                  # lowest ground in cm (sea and moor water stand at 0-6 cm)
BERRY_ZONES = ("hain", "saum")  # thickets of the Hain and the Forstsaum (Herbarium)
PIXEL = 200.0                 # 1 zone map pixel = 2 m (as in ScatterWurzelforst.py)
ZONE_COLORS = {               # zone colours of T_WF_Zonenkarte.png (as in ScatterWurzelforst.py)
    "saum": "6e6448", "moor": "3e6b55", "hain": "3f7f45", "kamm": "3b6670", "grat": "6a6d60", "asche": "7a4a2a",
    "jaw": "4a5f78", "grat_hoch": "9a9c90", "lagune": "2f6f74", "nid": "5a3a7a", "weg": "b9a77a",
}
MIN_SPACING = 3800.0          # cm between two places
GLOW_COLOUR = (0.35, 0.95, 1.0)    # light turquoise of the open eyes and their beams
LID_COLOUR = (0.025, 0.09, 0.1)    # dark teal of a closed lid
BEAM_COLOUR = (0.08, 0.75, 0.85)   # beams: deeper turquoise than the eyes, so they tint the forest instead of whitening it
BEAM_HALF_ANGLE = 6.0              # degrees
BEAM_LENGTH = 210.0                # cm (mesh scale)

# Blink rhythm: the same in both materials, so a beam goes out with its eye
BLINK_PERIOD = 5.0            # seconds for one open-and-close of an eye
OPEN_SHARE = 0.62             # share of the period an eye stays open
BLINK_FADE = 0.035            # share of the period the lid takes to close or open

library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(SEED)
notes = []
WORLD = [None]


def log(message):
    unreal.log_warning("[BuildWachbeeren] " + message)


# ---------------------------------------------------------------- Material

class Graph:
    """Places material nodes in columns and checks every link (as in BuildRiesenpilze.py)"""

    def __init__(self, material):
        self.material = material
        self.rows = {}

    def node(self, expression_class, column, **properties):
        row = self.rows.get(column, 0)
        self.rows[column] = row + 1
        expression = library.create_material_expression(self.material, expression_class, -300 * column, 140 * row)
        for key, value in properties.items():
            expression.set_editor_property(key, value)
        return expression

    def link(self, source, target, target_input="", source_output=""):
        if not library.connect_material_expressions(source, source_output, target, target_input):
            notes.append("link failed: {} -> {}.{}".format(source.get_name(), target.get_name(), target_input))

    def scalar(self, name, value, column):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=value)

    def const(self, value, column):
        return self.node(unreal.MaterialExpressionConstant, column, r=value)

    def colour(self, name, rgb, column):
        return self.mask(self.node(unreal.MaterialExpressionVectorParameter, column + 1, parameter_name=name,
                                   default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0)), column)

    def op(self, expression_class, column, a, b):
        expression = self.node(expression_class, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        return expression

    def one(self, expression_class, column, a):
        expression = self.node(expression_class, column)
        self.link(a, expression)
        return expression

    def mul(self, column, a, b):
        return self.op(unreal.MaterialExpressionMultiply, column, a, b)

    def add(self, column, a, b):
        return self.op(unreal.MaterialExpressionAdd, column, a, b)

    def sub(self, column, a, b):
        return self.op(unreal.MaterialExpressionSubtract, column, a, b)

    def div(self, column, a, b):
        return self.op(unreal.MaterialExpressionDivide, column, a, b)

    def min(self, column, a, b):
        return self.op(unreal.MaterialExpressionMin, column, a, b)

    def append(self, column, a, b):
        return self.op(unreal.MaterialExpressionAppendVector, column, a, b)

    def mask(self, source, column, r=True, g=True, b=True):
        expression = self.node(unreal.MaterialExpressionComponentMask, column, r=r, g=g, b=b, a=False)
        self.link(source, expression)
        return expression

    def lerp(self, a, b, alpha, column):
        expression = self.node(unreal.MaterialExpressionLinearInterpolate, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        self.link(alpha, expression, "Alpha")
        return expression

    def saturate(self, source, column):
        return self.one(unreal.MaterialExpressionSaturate, column, source)

    def sample(self, texture, column, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR):
        return self.node(unreal.MaterialExpressionTextureSample, column, texture=texture, sampler_type=sampler)

    def blink(self, phase, column):
        """1 while an eye is open, 0 while closed; the lid moves within BLINK_FADE of the period"""
        time = self.node(unreal.MaterialExpressionTime, column + 4)
        cycle = self.one(unreal.MaterialExpressionFrac, column + 2,
                         self.add(column + 3, self.div(column + 4, time, self.scalar("BlinzelDauer", BLINK_PERIOD, column + 5)), phase))
        fade = self.scalar("BlinzelUebergang", BLINK_FADE, column + 3)
        opening = self.saturate(self.div(column + 1, cycle, fade), column)
        closing = self.saturate(self.div(column + 1, self.sub(column + 2, self.scalar("OffenAnteil", OPEN_SHARE, column + 3), cycle), fade), column)
        return self.min(column - 1, opening, closing)


def texture(path):
    loaded = unreal.load_asset(path)
    if loaded is None:
        notes.append("missing texture " + path)
    return loaded


def fresh_material(path):
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
        return material
    folder, name = path.rsplit("/", 1)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def build_material():
    material = fresh_material(MATERIAL_PATH)
    material.set_editor_property("two_sided", True)
    g = Graph(material)

    picture = g.mask(g.sample(texture(BERRY + "/T_Wachbeere_basecolor"), 14), 13)
    darkest = g.min(11, g.min(12, g.mask(picture, 13, g=False, b=False), g.mask(picture, 13, r=False, b=False)),
                    g.mask(picture, 12, r=False, g=False))
    # Eye whites are the brightest, barely coloured texels
    white = g.saturate(g.mul(9, g.sub(10, darkest, g.scalar("WeissSchwelle", 0.5, 11)), g.const(5.0, 10)), 8)

    takt = g.node(unreal.MaterialExpressionTextureCoordinate, 14, coordinate_index=1)
    phase = g.add(12, g.mask(takt, 13, g=False, b=False), g.scalar("Versatz", 0.0, 13))
    zone = g.mask(takt, 13, r=False, b=False)
    is_open = g.blink(phase, 8)

    # A closed eye is covered by its lid: the whole lid zone turns dark teal
    lid = g.mul(6, g.saturate(g.mul(7, g.sub(8, zone, g.const(0.25, 9)), g.const(2.5, 8)), 7), g.sub(7, g.const(1.0, 8), is_open))
    base = g.lerp(picture, g.colour("LidFarbe", LID_COLOUR, 6), lid, 4)

    # An open eye glows: by day softly, at night much stronger
    night = g.scalar("Nacht", 0.0, 8)
    strength = g.lerp(g.scalar("LeuchtTag", 2.0, 7), g.scalar("LeuchtNacht", 12.0, 7), night, 6)
    glow = g.mul(3, g.mul(4, g.mul(5, g.mul(6, white, zone), is_open), strength), g.colour("LeuchtFarbe", GLOW_COLOUR, 5))

    # The canes sway a little, more the higher up (the root bulb stays still)
    local = g.node(unreal.MaterialExpressionLocalPosition, 12)
    local_z = g.mask(local, 11, r=False, g=False)
    height_share = g.div(9, local_z, g.const(MESH_HEIGHT, 10))
    sway_amount = g.mul(6, g.mul(7, height_share, height_share), g.scalar("Schwanken", 3.0, 7))
    time = g.node(unreal.MaterialExpressionTime, 10)
    sway_phase = g.add(8, g.mul(9, time, g.const(0.45, 10)), g.mul(9, g.scalar("Versatz", 0.0, 10), g.const(6.283, 10)))
    sway = g.append(5, g.append(6, g.one(unreal.MaterialExpressionSine, 7, sway_phase),
                                g.one(unreal.MaterialExpressionCosine, 7, g.mul(8, sway_phase, g.const(0.7, 9)))), g.const(0.0, 6))
    to_world = g.node(unreal.MaterialExpressionTransform, 2,
                      transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                      transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    g.link(g.mul(3, sway, sway_amount), to_world)

    normal_map = g.sample(texture(BERRY + "/T_Wachbeere_normal"), 3, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    roughness = g.mask(g.sample(texture(BERRY + "/T_Wachbeere_roughness"), 4, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR), 3, g=False, b=False)
    # Wet eyes: the eye whites shine
    shiny = g.lerp(roughness, g.const(0.15, 3), g.mul(3, white, zone), 2)
    library.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(normal_map, "RGB", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(shiny, "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(to_world, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def build_beam_material():
    material = fresh_material(BEAM_MATERIAL_PATH)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    g = Graph(material)

    phase = g.node(unreal.MaterialExpressionPerInstanceCustomData, 12, data_index=0, const_default_value=0.0)
    is_open = g.blink(phase, 8)

    # Brightest just in front of the eye, gone at the end; a short soft start so the tip never shows as a point
    local_z = g.mask(g.node(unreal.MaterialExpressionLocalPosition, 12), 11, r=False, g=False)
    along = g.saturate(g.div(9, g.mul(10, local_z, g.const(-1.0, 11)), g.const(100.0, 10)), 8)
    remaining = g.sub(7, g.const(1.0, 8), along)
    fade = g.mul(5, g.mul(6, remaining, remaining), g.saturate(g.mul(6, along, g.const(10.0, 7)), 6))
    # Soft edges: the beam is thin where we look along its surface
    facing = g.one(unreal.MaterialExpressionAbs, 8, g.op(unreal.MaterialExpressionDotProduct, 9,
                                                          g.node(unreal.MaterialExpressionVertexNormalWS, 10),
                                                          g.node(unreal.MaterialExpressionCameraVectorWS, 10)))
    edge = g.mul(6, facing, facing)
    # Fades out where the beam meets the ground, so it never ends in a hard line
    soft_ground = g.node(unreal.MaterialExpressionDepthFade, 5)
    g.link(g.scalar("BodenWeich", 40.0, 6), soft_ground, "FadeDistance")
    night = g.scalar("Nacht", 0.0, 7)
    strength = g.lerp(g.scalar("StrahlTag", 0.08, 6), g.scalar("StrahlNacht", 0.45, 6), night, 5)
    light = g.mul(2, g.mul(3, g.mul(4, g.mul(5, fade, edge), is_open), strength), g.colour("StrahlFarbe", BEAM_COLOUR, 4))
    library.connect_material_property(g.mul(1, light, soft_ground), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def import_beam_mesh():
    path = BERRY + "/SM_WF_Lichtkegel"
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", BEAM_FBX)
    task.set_editor_property("destination_path", BERRY)
    task.set_editor_property("destination_name", "SM_WF_Lichtkegel")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.load_asset(path)


def lighten(mesh):
    """Up to forty shrubs: LODs for the far ones, textures at 2K except the normal map"""
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary
    if subsystem.get_lod_count(mesh) < 3:
        options = unreal.StaticMeshReductionOptions()
        options.set_editor_property("auto_compute_lod_screen_size", False)
        settings = []
        for percent, screen in ((1.0, 1.0), (0.35, 0.35), (0.12, 0.12)):
            setting = unreal.StaticMeshReductionSettings()
            setting.set_editor_property("percent_triangles", percent)
            setting.set_editor_property("screen_size", screen)
            settings.append(setting)
        options.set_editor_property("reduction_settings", settings)
        subsystem.set_lods(mesh, options)
        assets.save_loaded_asset(mesh, False)
    for kind in ("basecolor", "roughness", "metallic"):
        picture = unreal.load_asset("{}/T_Wachbeere_{}".format(BERRY, kind))
        if picture is not None and picture.get_editor_property("max_texture_size") != 2048:
            picture.set_editor_property("max_texture_size", 2048)
            assets.save_loaded_asset(picture, False)


# ---------------------------------------------------------------- Placement

def trace(start, end):
    hit = unreal.SystemLibrary.line_trace_single(WORLD[0], start, end,
                                                 unreal.TraceTypeQuery.ECC_VISIBILITY, True, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    values = hit.to_tuple()
    if not values[0]:
        return None
    return values[4], values[7], values


def far_enough(spot, taken):
    return all((spot[0] - x) ** 2 + (spot[1] - y) ** 2 > MIN_SPACING ** 2 for x, y in taken)


def on_land(hit):
    """Only the landscape counts (as in BuildFluesterkappen.py): no rocks, trees or water"""
    values = hit[2]
    actor = values[9] if len(values) > 9 else None
    return isinstance(actor, unreal.LandscapeProxy) and hit[0].z > DRY_Z and hit[1].z >= 0.85


class ZoneMap:
    """Zones of the forest from T_WF_Zonenkarte.png, looked up in world cm (reader as in ScatterWurzelforst.py)"""

    def __init__(self, origin):
        self.origin = origin
        data = open(os.path.join(KIT, "T_WF_Zonenkarte.png"), "rb").read()
        pos, chunks = 8, []
        while pos < len(data):
            length, kind = struct.unpack(">I4s", data[pos:pos + 8])
            body = data[pos + 8:pos + 8 + length]
            if kind == b"IHDR":
                self.width, self.height, depth, color = struct.unpack(">IIBB", body[:10])
            elif kind == b"IDAT":
                chunks.append(body)
            elif kind == b"IEND":
                break
            pos += 12 + length
        self.channels = {0: 1, 2: 3, 4: 2, 6: 4}[color]
        bpp = max(1, self.channels * depth // 8)
        stride = self.width * self.channels * depth // 8
        raw = zlib.decompress(b"".join(chunks))
        self.rows, previous = [], bytearray(stride)
        for y in range(self.height):
            start = y * (stride + 1)
            kind = raw[start]
            line = bytearray(raw[start + 1:start + 1 + stride])
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                up = previous[i]
                corner = previous[i - bpp] if i >= bpp else 0
                if kind == 1:
                    line[i] = (line[i] + left) & 255
                elif kind == 2:
                    line[i] = (line[i] + up) & 255
                elif kind == 3:
                    line[i] = (line[i] + ((left + up) >> 1)) & 255
                elif kind == 4:
                    guess = left + up - corner
                    pa, pb, pc = abs(guess - left), abs(guess - up), abs(guess - corner)
                    line[i] = (line[i] + (left if pa <= pb and pa <= pc else up if pb <= pc else corner)) & 255
            self.rows.append(bytes(line))
            previous = line
        self.colours = {name: [int(c[i:i + 2], 16) for i in (0, 2, 4)] for name, c in ZONE_COLORS.items()}

    def zone(self, x, y):
        j = min(max(int(round((x - self.origin[0]) / PIXEL)), 0), self.width - 1)
        i = min(max(int(round((y - self.origin[1]) / PIXEL)), 0), self.height - 1)
        rgb = [self.rows[i][j * self.channels + k] for k in range(3)]
        best, best_error = None, None
        for name, colour in self.colours.items():
            # The map is hill-shaded, so each colour is compared at its best brightness
            scale = min(max(sum(a * b for a, b in zip(rgb, colour)) / max(1, sum(b * b for b in colour)), 0.6), 1.3)
            error = sum((a - b * scale) ** 2 for a, b in zip(rgb, colour))
            if best_error is None or error < best_error:
                best, best_error = name, error
        return best


def berry_spots(landscape):
    """Places in the Hain and the Forstsaum, and around each the shrubs of its group, all on open land"""
    centre, extent = landscape.get_actor_bounds(False)
    corner = landscape.get_actor_location()
    zones = ZoneMap((corner.x, corner.y))
    top, bottom = centre.z + extent.z + 5000, centre.z - extent.z - 5000
    taken, places, tries = [], [], 0
    while len(places) < SPOTS and tries < SPOTS * 400:
        tries += 1
        x = rng.uniform(centre.x - extent.x, centre.x + extent.x)
        y = rng.uniform(centre.y - extent.y, centre.y + extent.y)
        if not far_enough((x, y), taken) or zones.zone(x, y) not in BERRY_ZONES:
            continue
        hit = trace(unreal.Vector(x, y, top), unreal.Vector(x, y, bottom))
        if hit is None or not on_land(hit):
            continue
        group = []
        for index in range(rng.randint(GROUP[0], GROUP[1])):
            for _ in range(12):
                angle = rng.uniform(0.0, 6.283)
                reach = 0.0 if index == 0 else rng.uniform(GROUP_REACH * 0.5, GROUP_REACH)
                gx, gy = x + reach * math.cos(angle), y + reach * math.sin(angle)
                shrub_hit = trace(unreal.Vector(gx, gy, top), unreal.Vector(gx, gy, bottom))
                if shrub_hit is not None and on_land(shrub_hit):
                    group.append(unreal.Vector(gx, gy, shrub_hit[0].z - SINK))
                    break
        if group:
            places.append(group)
            taken.append((x, y))
    return places, tries


def main():
    eye_data = json.load(open(EYE_DATA, encoding="utf-8"))
    mesh = unreal.load_asset(BERRY + "/SM_Wachbeere")
    if mesh is None:
        log("SM_Wachbeere missing: import Vael_Quellen/Wachbeere with Tools/Kreaturenfabrik/unreal/vael_kreatur_import.py first")
        return
    lighten(mesh)
    beam_mesh = import_beam_mesh()
    material = build_material()
    beam_material = build_beam_material()
    eyes = [unreal.VaelWatchEye(location=unreal.Vector(*eye["ort"]), direction=unreal.Vector(*eye["blick"]), phase=eye["takt"])
            for eye in eye_data["augen"]]

    world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    WORLD[0] = world
    level_actors = actors.get_all_level_actors()
    removed = 0
    for actor in level_actors:
        if unreal.Name(TAG) in actor.tags:
            actors.destroy_actor(actor)
            removed += 1
    landscape = next((a for a in level_actors if isinstance(a, unreal.LandscapeProxy)), None)
    if landscape is None:
        log("no landscape in the level, nothing placed")
        return

    places, tries = berry_spots(landscape)
    shrubs = 0
    for place_index, group in enumerate(places):
        for shrub_index, spot in enumerate(group):
            shrub = actors.spawn_actor_from_class(unreal.VaelWatchBerry, spot, unreal.Rotator(0.0, 0.0, rng.uniform(0.0, 360.0)))
            scale = rng.uniform(HEIGHTS[0], HEIGHTS[1]) / MESH_HEIGHT
            shrub.set_actor_scale3d(unreal.Vector(scale, scale, scale))
            shrub.set_editor_property("shrub_mesh", mesh)
            shrub.set_editor_property("shrub_material", material)
            shrub.set_editor_property("beam_mesh", beam_mesh)
            shrub.set_editor_property("beam_material", beam_material)
            shrub.set_editor_property("beam_tip_z", 0.0)
            shrub.set_editor_property("beam_mesh_length", 100.0)
            shrub.set_editor_property("beam_mesh_radius", 50.0)
            shrub.set_editor_property("beam_half_angle", BEAM_HALF_ANGLE)
            shrub.set_editor_property("beam_length", BEAM_LENGTH)
            shrub.set_editor_property("eyes", eyes)
            shrub.set_editor_property("phase_shift", rng.random())
            shrub.build_shrub()
            shrub.set_actor_label("Wachbeere_{:02d}_{}".format(place_index + 1, shrub_index + 1))
            shrub.set_folder_path(FOLDER)
            shrub.set_editor_property("tags", [unreal.Name(TAG)])
            shrubs += 1

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log("{} places, {} shrubs ({} tries), {} eyes each; {} old shrubs removed".format(len(places), shrubs, tries, len(eyes), removed))
    for place_index, group in enumerate(places):
        log("place {}: {:.0f} {:.0f} m".format(place_index + 1, group[0].x / 100.0, group[0].y / 100.0))
    for note in notes:
        log("NOTE " + note)


main()
