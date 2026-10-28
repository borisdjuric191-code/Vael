# Puts rings of Flüsterkappen (Herbarium, Tripo model SM_Fluesterkappe) into L_Wurzelforst_Blockout, scattered at
# random over the whole forest so finding one is a surprise; about a third sit on a fallen trunk (deadwood).
#
# - Material M_WF_Fluesterkappe: the Tripo picture; the strongly blue and cyan parts (eye sockets and mouth) glow,
#   by day softly and at night much stronger (parameter "Nacht", set by AVaelWhisperCaps from the clock), and the
#   hood shimmers light turquoise at night. A whispering cap (custom data 0) flares its eyes and mouth and its hood
#   swells; every cap breathes slowly in its own rhythm (custom data 1) and sways a little (World Position Offset).
# - Each ring is one AVaelWhisperCaps (C++): it lays out 5-9 caps of 1.2-1.8 m with their faces to the middle,
#   leaning in, sunk into the ground with a moss cushion at each foot, and makes them whisper and turn after players.
# - LODs on the mesh, textures at 2K except the normal map (it carries the faces).
# Everything it adds is tagged WF_Fluesterkappe (Outliner folder Wurzelforst/Fluesterkappen); rerunning replaces only
# that. Needs the imported SM_Fluesterkappe (Tools/Kreaturenfabrik/unreal/vael_kreatur_import.py) and
# Vael_Quellen/Fluesterkappe/03_analyse/gesicht.json (Tools/Kreaturenfabrik/blender/vael_gesicht_vermessen.py).
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildFluesterkappen.py" -EnablePlugins=PythonScriptPlugin

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
FACE_DATA = os.path.join(os.path.dirname(ROOT), "Vael_Quellen", "Fluesterkappe", "03_analyse", "gesicht.json")
CAP = "/Game/Vael/Requisiten/Wurzelforst/Fluesterkappe"
MATERIAL_PATH = CAP + "/M_WF_Fluesterkappe"
MOSS_NAMES = ["SM_IcelandicMossClusters_0{}".format(i) for i in (0, 1, 2, 4, 5, 6, 7, 8)]
TAG = "WF_Fluesterkappe"
FOLDER = "Wurzelforst/Fluesterkappen"
SEED = 41

RINGS = 26               # rings over the whole forest
ON_DEADWOOD = 0.35       # share of tries that look for a fallen trunk near the random spot
DEADWOOD_REACH = 2500.0  # cm around the random spot to look for a trunk
DEADWOOD_SIDE = 260.0    # cm from the trunk's middle to the ring's middle
DRY_Z = 60.0             # lowest ground in cm (sea and moor water stand at 0-6 cm)
FOREST_ZONES = ("hain", "saum", "moor", "kamm")  # no rings on the ridges, in the ash or at the jaw
PIXEL = 200.0            # 1 zone map pixel = 2 m (as in ScatterWurzelforst.py)
ZONE_COLORS = {          # zone colours of T_WF_Zonenkarte.png (as in ScatterWurzelforst.py)
    "saum": "6e6448", "moor": "3e6b55", "hain": "3f7f45", "kamm": "3b6670", "grat": "6a6d60", "asche": "7a4a2a",
    "jaw": "4a5f78", "grat_hoch": "9a9c90", "lagune": "2f6f74", "nid": "5a3a7a", "weg": "b9a77a",
}
MIN_SPACING = 4500.0     # cm between two rings
GLOW_COLOUR = (0.2, 0.8, 1.0)  # light turquoise blue

library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(SEED)
notes = []
WORLD = [None]


def log(message):
    unreal.log_warning("[BuildFluesterkappen] " + message)


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


def texture(path):
    loaded = unreal.load_asset(path)
    if loaded is None:
        notes.append("missing texture " + path)
    return loaded


def build_material(face):
    if assets.does_asset_exist(MATERIAL_PATH):
        material = unreal.load_asset(MATERIAL_PATH)
        library.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_WF_Fluesterkappe", CAP, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("used_with_instanced_static_meshes", True)
    g = Graph(material)

    picture = g.mask(g.sample(texture(CAP + "/T_Fluesterkappe_basecolor"), 12), 11)
    red = g.mask(picture, 10, g=False, b=False)
    blue = g.mask(picture, 10, r=False, g=False)
    # Eye sockets (deep blue) and mouth (cyan) are far bluer than the pale skin around them
    glow_share = g.saturate(g.mul(7, g.sub(8, g.sub(9, blue, red), g.scalar("LeuchtSchwelle", 0.28, 9)), g.const(4.0, 8)), 6)

    whisper = g.node(unreal.MaterialExpressionPerInstanceCustomData, 10, data_index=0, const_default_value=0.0)
    phase = g.mul(9, g.node(unreal.MaterialExpressionPerInstanceCustomData, 10, data_index=1, const_default_value=0.0), g.const(6.283, 10))
    night = g.scalar("Nacht", 0.0, 9)
    time = g.node(unreal.MaterialExpressionTime, 10)

    # Glow: visible by day, much stronger at night; a slow pulse of its own and a flare while whispering
    strength = g.lerp(g.scalar("LeuchtTag", 3.0, 8), g.scalar("LeuchtNacht", 14.0, 8), night, 6)
    slow = g.one(unreal.MaterialExpressionSine, 7, g.add(8, g.mul(9, time, g.const(0.9, 10)), phase))
    pulse = g.add(5, g.add(6, g.const(0.85, 7), g.mul(7, slow, g.const(0.15, 8))), g.mul(6, whisper, g.scalar("FluesterLeuchten", 2.5, 7)))
    glow_colour = g.node(unreal.MaterialExpressionVectorParameter, 7, parameter_name="LeuchtFarbe",
                         default_value=unreal.LinearColor(GLOW_COLOUR[0], GLOW_COLOUR[1], GLOW_COLOUR[2], 1.0))
    face_glow = g.mul(3, g.mul(4, g.mul(5, glow_share, g.mask(glow_colour, 6)), strength), pulse)

    # At night the hood shimmers light turquoise blue, as if lit from inside (Boris: the night aura should read blue)
    local = g.node(unreal.MaterialExpressionLocalPosition, 12)
    local_z = g.mask(local, 11, r=False, g=False)
    cap_bottom = g.scalar("HutUnterseite", face["hut_unterseite_cm"], 11)
    cap_share = g.saturate(g.div(8, g.sub(9, local_z, g.mul(10, cap_bottom, g.const(0.85, 11))), g.const(40.0, 9)), 7)
    shimmer = g.mul(3, g.mul(4, g.mul(5, g.mask(glow_colour, 6), cap_share), night), g.scalar("HutSchimmer", 0.15, 5))
    emissive = g.add(2, face_glow, shimmer)

    # Breathing: the hood swells and sinks in its own rhythm, more while whispering; the stem stays still
    breath = g.add(6, g.mul(7, g.one(unreal.MaterialExpressionSine, 8, g.add(9, g.mul(10, time, g.scalar("AtemTempo", 0.8, 11)), phase)), g.const(0.5, 8)),
                   g.const(0.5, 7))
    swell = g.mul(4, cap_share, g.add(5, g.mul(6, breath, g.scalar("AtemStaerke", 0.025, 7)), g.mul(6, whisper, g.scalar("FluesterSchwellen", 0.035, 7))))
    centred = g.append(6, g.mask(local, 7, b=False), g.sub(7, local_z, cap_bottom))
    breathing = g.mul(3, centred, swell)
    # A little sway, growing with the height, so a ring never stands stiff
    height_share = g.div(8, local_z, g.const(face["hoehe_cm"], 9))
    sway_amount = g.mul(6, g.mul(7, height_share, height_share), g.scalar("Schwanken", 1.5, 7))
    sway_phase = g.add(8, g.mul(9, time, g.const(0.55, 10)), phase)
    sway = g.append(5, g.append(6, g.one(unreal.MaterialExpressionSine, 7, sway_phase), g.one(unreal.MaterialExpressionCosine, 7, g.mul(8, sway_phase, g.const(0.8, 9)))),
                    g.const(0.0, 6))
    local_offset = g.add(2, breathing, g.mul(3, sway, sway_amount))
    to_world = g.node(unreal.MaterialExpressionTransform, 1,
                      transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                      transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    g.link(local_offset, to_world)

    normal_map = g.sample(texture(CAP + "/T_Fluesterkappe_normal"), 3, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    roughness = g.mask(g.sample(texture(CAP + "/T_Fluesterkappe_roughness"), 4, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR), 3, g=False, b=False)
    library.connect_material_property(picture, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(normal_map, "RGB", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(to_world, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def lighten(mesh):
    """Twenty rings of up to nine: LODs for the far ones, textures at 2K except the normal map that carries the faces"""
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary
    if subsystem.get_lod_count(mesh) < 3:
        options = unreal.StaticMeshReductionOptions()
        options.set_editor_property("auto_compute_lod_screen_size", False)
        settings = []
        for percent, screen in ((1.0, 1.0), (0.35, 0.3), (0.1, 0.1)):
            setting = unreal.StaticMeshReductionSettings()
            setting.set_editor_property("percent_triangles", percent)
            setting.set_editor_property("screen_size", screen)
            settings.append(setting)
        options.set_editor_property("reduction_settings", settings)
        subsystem.set_lods(mesh, options)
        assets.save_loaded_asset(mesh, False)
    for kind in ("basecolor", "roughness", "metallic"):
        picture = unreal.load_asset("{}/T_Fluesterkappe_{}".format(CAP, kind))
        if picture is not None and picture.get_editor_property("max_texture_size") != 2048:
            picture.set_editor_property("max_texture_size", 2048)
            assets.save_loaded_asset(picture, False)


def load_moss():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = []
    for data in registry.get_assets_by_path("/Game/Megascans", recursive=True):
        if str(data.asset_name) in MOSS_NAMES and str(data.asset_class_path.asset_name) == "StaticMesh":
            found.append(data.get_asset())
    return found


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
    """Only the landscape counts: rocks, trees and buildings are not ground for a ring. Water planes have no
    collision, so the trace finds the lake or moor floor below them; the height check keeps rings out of water."""
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


def forest_spots(level_actors, landscape):
    """Rings scattered at random over the whole forest (Boris: a surprise wherever you go, not only at the giants).
    Some stand beside a fallen trunk nearby, the rest on open ground; never in water, on rock or in a tree."""
    trunks = []
    for actor in level_actors:
        if isinstance(actor, unreal.VaelScatterField) and actor.get_actor_label().startswith("Streu_Staemme_"):
            field = actor.get_editor_property("instances")
            for index in range(field.get_instance_count()):
                trunks.append(field.get_instance_transform(index, True).translation)
    centre, extent = landscape.get_actor_bounds(False)
    corner = landscape.get_actor_location()
    zones = ZoneMap((corner.x, corner.y))
    taken = []
    spots = []
    tries = 0
    while len(spots) < RINGS and tries < RINGS * 400:
        tries += 1
        x = rng.uniform(centre.x - extent.x, centre.x + extent.x)
        y = rng.uniform(centre.y - extent.y, centre.y + extent.y)
        kind = "Wald"
        if rng.random() < ON_DEADWOOD and trunks:
            near = [t for t in trunks if (t.x - x) ** 2 + (t.y - y) ** 2 < DEADWOOD_REACH ** 2]
            if near:
                # Beside the trunk, not on it: the caps trace onto the landscape and would stick through the wood
                trunk = rng.choice(near)
                angle = rng.uniform(0.0, 6.283)
                x, y = trunk.x + DEADWOOD_SIDE * math.cos(angle), trunk.y + DEADWOOD_SIDE * math.sin(angle)
                kind = "Totholz"
        if not far_enough((x, y), taken) or zones.zone(x, y) not in FOREST_ZONES:
            continue
        hit = trace(unreal.Vector(x, y, centre.z + extent.z + 5000), unreal.Vector(x, y, centre.z - extent.z - 5000))
        if hit is None or not on_land(hit):
            continue
        spots.append((unreal.Vector(x, y, hit[0].z), kind))
        taken.append((x, y))
    return spots, len(trunks), tries


def main():
    face = json.load(open(FACE_DATA, encoding="utf-8"))

    mesh = unreal.load_asset(CAP + "/SM_Fluesterkappe")
    if mesh is None:
        log("SM_Fluesterkappe missing: import Vael_Quellen/Fluesterkappe with Tools/Kreaturenfabrik/unreal/vael_kreatur_import.py first")
        return
    lighten(mesh)
    material = build_material(face)
    moss = load_moss()

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
    spots, trunk_count, tries = forest_spots(level_actors, landscape)
    caps = 0
    counts = {}
    for spot, kind in spots:
        counts[kind] = counts.get(kind, 0) + 1
        index = counts[kind] - 1
        ring = actors.spawn_actor_from_class(unreal.VaelWhisperCaps, spot, unreal.Rotator(0.0, 0.0, 0.0))
        ring.set_editor_property("cap_mesh", mesh)
        ring.set_editor_property("cap_material", material)
        ring.set_editor_property("moss_meshes", moss)
        ring.set_editor_property("mesh_height", face["hoehe_cm"])
        ring.set_editor_property("face_yaw", face["gesicht_yaw_unreal"])
        ring.set_editor_property("seed", rng.randint(1, 99999))
        ring.set_editor_property("count", rng.randint(5, 9))
        ring.set_editor_property("ring_radius", rng.uniform(80.0, 120.0))
        ring.build_ring()
        ring.set_actor_label("Fluesterkappen_{}_{:02d}".format(kind, index + 1))
        ring.set_folder_path(FOLDER)
        ring.set_editor_property("tags", [unreal.Name(TAG)])
        for component in ring.get_components_by_class(unreal.InstancedStaticMeshComponent):
            if component.get_name() == "Caps":
                caps += component.get_instance_count()

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log("rings placed: {} ({} tries, {} trunks known), {} caps; {} moss meshes; {} old rings removed".format(
        counts, tries, trunk_count, caps, len(moss), removed))
    for note in notes:
        log("NOTE " + note)


main()
