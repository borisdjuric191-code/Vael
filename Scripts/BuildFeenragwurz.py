# Puts Feenragwurzen (Herbarium, Tripo models SM_Feenragwurz and SM_Ragwurzfee) into L_Wurzelforst_Blockout: tall orchids
# with a few fairies around them, alone or in pairs, in the Hain.
#
# - Material M_WF_Feenragwurz: the Tripo picture; the plant sways a little and every blossom nods on its stalk in its own
#   rhythm (UV map "Takt" from Tools/Kreaturenfabrik/blender/vael_feenragwurz_takt.py: U = phase, V = share of the nod).
# - Material M_WF_Ragwurzfee: the Tripo picture; wings and legs bent by World Position Offset, without a skeleton
#   (UV map "Takt": U = wing, V = leg). The angles come from AVaelFairyOrchid through the custom data of the instances:
#   0 upper wings, 1 lower wings, 2 legs trailing, 3 legs swinging, all in radians.
# - Each orchid is one AVaelFairyOrchid (C++), with the blossoms from Vael_Quellen/Feenragwurz/03_analyse/blueten.json.
# - LODs on both meshes, textures at 2K except the normal maps.
# Everything it adds is tagged WF_Feenragwurz (Outliner folder Wurzelforst/Feenragwurzen); rerunning replaces only that.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildFeenragwurz.py" -EnablePlugins=PythonScriptPlugin

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
BLOSSOM_DATA = os.path.join(os.path.dirname(ROOT), "Vael_Quellen", "Feenragwurz", "03_analyse", "blueten.json")
ORCHID = "/Game/Vael/Requisiten/Wurzelforst/Feenragwurz"
FAIRY = "/Game/Vael/Requisiten/Wurzelforst/Ragwurzfee"
ORCHID_MATERIAL_PATH = ORCHID + "/M_WF_Feenragwurz"
FAIRY_MATERIAL_PATH = FAIRY + "/M_WF_Ragwurzfee"
TAG = "WF_Feenragwurz"
FOLDER = "Wurzelforst/Feenragwurzen"
SEED = 71

SPOTS = 9                     # places with Feenragwurzen in the Hain (rarity "besonders": fewer than the Wachbeeren)
GROUP = (1, 2)                # orchids at one place
GROUP_REACH = 380.0           # cm from the place to the second orchid
FAIRIES = (1, 3)              # fairies around one orchid
HEIGHTS = (195.0, 240.0)      # cm
FAIRY_SCALES = (1.0, 1.2)     # against the 60 cm mesh, in world size
MESH_HEIGHT = 220.0           # cm, as exported by the 'requisite' step
SINK = 4.0                    # cm the moss mound sinks into the ground
DRY_Z = 60.0                  # lowest ground in cm (sea and moor water stand at 0-6 cm)
ORCHID_ZONES = ("hain",)
PIXEL = 200.0                 # 1 zone map pixel = 2 m (as in ScatterWurzelforst.py)
ZONE_COLORS = {               # zone colours of T_WF_Zonenkarte.png (as in ScatterWurzelforst.py)
    "saum": "6e6448", "moor": "3e6b55", "hain": "3f7f45", "kamm": "3b6670", "grat": "6a6d60", "asche": "7a4a2a",
    "jaw": "4a5f78", "grat_hoch": "9a9c90", "lagune": "2f6f74", "nid": "5a3a7a", "weg": "b9a77a",
}
MIN_SPACING = 6000.0          # cm between two places

# The fairy mesh, in cm (Unreal space: she looks along +Y)
WING_ROOT = (2.5, 2.0)        # |x|, y of the upright hinge of the wings
WING_SPLIT = 44.0             # above this height the upper wings
HIP = (0.0, 31.0)             # y, z of the axis the legs swing around

library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(SEED)
notes = []
WORLD = [None]


def log(message):
    unreal.log_warning("[BuildFeenragwurz] " + message)


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

    def sine(self, source, column):
        return self.one(unreal.MaterialExpressionSine, column, source)

    def cosine(self, source, column):
        return self.one(unreal.MaterialExpressionCosine, column, source)

    def sample(self, texture, column, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR):
        return self.node(unreal.MaterialExpressionTextureSample, column, texture=texture, sampler_type=sampler)

    def to_world(self, local, column):
        expression = self.node(unreal.MaterialExpressionTransform, column,
                               transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                               transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        self.link(local, expression)
        return expression


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


def connect_picture(g, folder, name):
    """Picture, normals, roughness and metal of a Tripo model"""
    linear = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
    library.connect_material_property(g.mask(g.sample(texture("{}/T_{}_basecolor".format(folder, name)), 4), 3), "", unreal.MaterialProperty.MP_BASE_COLOR)
    normal_map = g.sample(texture("{}/T_{}_normal".format(folder, name)), 3, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    library.connect_material_property(normal_map, "RGB", unreal.MaterialProperty.MP_NORMAL)
    roughness = g.sample(texture("{}/T_{}_roughness".format(folder, name)), 4, sampler=linear)
    library.connect_material_property(g.mask(roughness, 3, g=False, b=False), "", unreal.MaterialProperty.MP_ROUGHNESS)
    metallic = g.sample(texture("{}/T_{}_metallic".format(folder, name)), 4, sampler=linear)
    library.connect_material_property(g.mask(metallic, 3, g=False, b=False), "", unreal.MaterialProperty.MP_METALLIC)


def build_orchid_material():
    material = fresh_material(ORCHID_MATERIAL_PATH)
    material.set_editor_property("two_sided", True)
    g = Graph(material)
    connect_picture(g, ORCHID, "Feenragwurz")

    time = g.node(unreal.MaterialExpressionTime, 10)
    takt = g.node(unreal.MaterialExpressionTextureCoordinate, 12, coordinate_index=1)
    phase = g.mask(takt, 11, g=False, b=False)  # Sine and Cosine count whole turns
    share = g.mask(takt, 11, r=False, b=False)

    # The whole plant sways a little, more the higher up
    local_z = g.mask(g.node(unreal.MaterialExpressionLocalPosition, 12), 11, r=False, g=False)
    height_share = g.div(9, local_z, g.const(MESH_HEIGHT, 10))
    sway_amount = g.mul(6, g.mul(7, height_share, height_share), g.scalar("Schwanken", 4.0, 7))
    sway_phase = g.mul(8, time, g.const(0.5, 9))
    sway = g.append(5, g.append(6, g.sine(sway_phase, 7), g.cosine(g.mul(8, sway_phase, g.const(0.7, 9)), 7)), g.const(0.0, 6))

    # Every blossom nods on its stalk in its own rhythm, heavy, with a quicker tremble on top
    nod_phase = g.add(8, g.mul(9, time, g.const(0.4, 10)), phase)
    nod_wave = g.add(6, g.sine(nod_phase, 7), g.mul(7, g.sine(g.mul(8, nod_phase, g.const(2.7, 9)), 8), g.const(0.3, 8)))
    side_wave = g.cosine(g.mul(7, nod_phase, g.const(0.6, 8)), 6)
    nod = g.append(4, g.append(5, g.mul(6, side_wave, g.const(0.5, 7)), g.mul(6, nod_wave, g.const(0.7, 7))),
                   g.mul(5, nod_wave, g.const(1.0, 6)))
    nodding = g.mul(3, nod, g.mul(4, share, g.scalar("Nicken", 2.2, 5)))

    offset = g.add(2, g.mul(3, sway, sway_amount), nodding)
    library.connect_material_property(g.to_world(offset, 1), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def build_fairy_material():
    material = fresh_material(FAIRY_MATERIAL_PATH)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    g = Graph(material)
    connect_picture(g, FAIRY, "Ragwurzfee")

    takt = g.node(unreal.MaterialExpressionTextureCoordinate, 16, coordinate_index=1)
    wing = g.mask(takt, 15, g=False, b=False)
    leg = g.mask(takt, 15, r=False, b=False)
    local = g.node(unreal.MaterialExpressionLocalPosition, 16)
    x = g.mask(local, 15, g=False, b=False)
    y = g.mask(local, 15, r=False, b=False)
    z = g.mask(local, 15, r=False, g=False)
    custom = [g.node(unreal.MaterialExpressionPerInstanceCustomData, 15, data_index=i, const_default_value=0.0) for i in range(4)]
    # A soft side: -1 left of the middle, 1 right of it, between the feet that nearly touch a smooth change
    side = g.mul(13, g.saturate(g.add(14, g.mul(15, x, g.const(0.5, 16)), g.const(0.5, 16)), 13), g.const(2.0, 14))
    side = g.sub(12, side, g.const(1.0, 13))

    # Wings: each turns around its upright hinge at the shoulder blade, the upper ones ahead of the lower ones
    upper = g.saturate(g.sub(13, z, g.const(WING_SPLIT - 0.5, 14)), 12)
    wing_angle = g.mul(9, g.mul(10, g.lerp(custom[1], custom[0], upper, 11), side), wing)
    wing_cos = g.sub(7, g.cosine(g.div(9, wing_angle, g.const(6.283, 10)), 8), g.const(1.0, 8))
    wing_sin = g.sine(g.div(9, wing_angle, g.const(6.283, 10)), 8)
    from_x = g.sub(10, x, g.mul(11, side, g.const(WING_ROOT[0], 12)))
    from_y = g.sub(10, y, g.const(WING_ROOT[1], 11))
    wing_x = g.sub(5, g.mul(6, from_x, wing_cos), g.mul(6, from_y, wing_sin))
    wing_y = g.add(5, g.mul(6, from_x, wing_sin), g.mul(6, from_y, wing_cos))

    # Legs: bend from the hip towards the back, swinging against each other
    leg_angle = g.mul(9, g.add(10, custom[2], g.mul(11, custom[3], side)), leg)
    leg_cos = g.sub(7, g.cosine(g.div(9, leg_angle, g.const(6.283, 10)), 8), g.const(1.0, 8))
    leg_sin = g.sine(g.div(9, leg_angle, g.const(6.283, 10)), 8)
    hip_y = g.sub(10, y, g.const(HIP[0], 11))
    hip_z = g.sub(10, z, g.const(HIP[1], 11))
    leg_y = g.add(5, g.mul(6, hip_y, leg_cos), g.mul(6, hip_z, leg_sin))
    leg_z = g.sub(5, g.mul(6, hip_z, leg_cos), g.mul(6, hip_y, leg_sin))

    offset = g.append(2, g.append(3, wing_x, g.add(4, wing_y, leg_y)), leg_z)
    library.connect_material_property(g.to_world(offset, 1), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def lighten(mesh, folder, name, steps):
    """LODs for the far ones, textures at 2K except the normal map"""
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary
    if subsystem.get_lod_count(mesh) < 3:
        options = unreal.StaticMeshReductionOptions()
        options.set_editor_property("auto_compute_lod_screen_size", False)
        settings = []
        for percent, screen in steps:
            setting = unreal.StaticMeshReductionSettings()
            setting.set_editor_property("percent_triangles", percent)
            setting.set_editor_property("screen_size", screen)
            settings.append(setting)
        options.set_editor_property("reduction_settings", settings)
        subsystem.set_lods(mesh, options)
        assets.save_loaded_asset(mesh, False)
    for kind in ("basecolor", "roughness", "metallic"):
        picture = unreal.load_asset("{}/T_{}_{}".format(folder, name, kind))
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
    return isinstance(actor, unreal.LandscapeProxy) and hit[0].z > DRY_Z and hit[1].z >= 0.9


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


def orchid_spots(landscape):
    """Places in the Hain, and around each the orchids of its group, all on open land"""
    centre, extent = landscape.get_actor_bounds(False)
    corner = landscape.get_actor_location()
    zones = ZoneMap((corner.x, corner.y))
    top, bottom = centre.z + extent.z + 5000, centre.z - extent.z - 5000
    taken, places, tries = [], [], 0
    while len(places) < SPOTS and tries < SPOTS * 800:
        tries += 1
        x = rng.uniform(centre.x - extent.x, centre.x + extent.x)
        y = rng.uniform(centre.y - extent.y, centre.y + extent.y)
        if not far_enough((x, y), taken) or zones.zone(x, y) not in ORCHID_ZONES:
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
                plant_hit = trace(unreal.Vector(gx, gy, top), unreal.Vector(gx, gy, bottom))
                if plant_hit is not None and on_land(plant_hit):
                    group.append(unreal.Vector(gx, gy, plant_hit[0].z - SINK))
                    break
        if group:
            places.append(group)
            taken.append((x, y))
    return places, tries


def main():
    blossom_data = json.load(open(BLOSSOM_DATA, encoding="utf-8"))
    orchid_mesh = unreal.load_asset(ORCHID + "/SM_Feenragwurz")
    fairy_mesh = unreal.load_asset(FAIRY + "/SM_Ragwurzfee")
    if orchid_mesh is None or fairy_mesh is None:
        log("SM_Feenragwurz or SM_Ragwurzfee missing: import them with Tools/Kreaturenfabrik/unreal/vael_kreatur_import.py first")
        return
    lighten(orchid_mesh, ORCHID, "Feenragwurz", ((1.0, 1.0), (0.35, 0.35), (0.12, 0.12)))
    lighten(fairy_mesh, FAIRY, "Ragwurzfee", ((1.0, 1.0), (0.4, 0.2), (0.15, 0.08)))
    orchid_material = build_orchid_material()
    fairy_material = build_fairy_material()
    blossoms = [unreal.VaelOrchidBlossom(location=unreal.Vector(*b["ort"]), direction=unreal.Vector(*b["blick"]))
                for b in blossom_data["blueten"]]

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

    places, tries = orchid_spots(landscape)
    plants = fairies = 0
    for place_index, group in enumerate(places):
        for plant_index, spot in enumerate(group):
            orchid = actors.spawn_actor_from_class(unreal.VaelFairyOrchid, spot, unreal.Rotator(0.0, 0.0, rng.uniform(0.0, 360.0)))
            scale = rng.uniform(HEIGHTS[0], HEIGHTS[1]) / MESH_HEIGHT
            orchid.set_actor_scale3d(unreal.Vector(scale, scale, scale))
            count = rng.randint(FAIRIES[0], FAIRIES[1])
            orchid.set_editor_property("plant_mesh", orchid_mesh)
            orchid.set_editor_property("plant_material", orchid_material)
            orchid.set_editor_property("fairy_mesh", fairy_mesh)
            orchid.set_editor_property("fairy_material", fairy_material)
            orchid.set_editor_property("blossoms", blossoms)
            orchid.set_editor_property("fairy_count", count)
            orchid.set_editor_property("fairy_scale", rng.uniform(FAIRY_SCALES[0], FAIRY_SCALES[1]) / scale)
            orchid.build_orchid()
            orchid.set_actor_label("Feenragwurz_{:02d}_{}".format(place_index + 1, plant_index + 1))
            orchid.set_folder_path(FOLDER)
            orchid.set_editor_property("tags", [unreal.Name(TAG)])
            plants += 1
            fairies += count

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log("{} places, {} orchids, {} fairies ({} tries); {} old orchids removed".format(len(places), plants, fairies, tries, removed))
    for place_index, group in enumerate(places):
        log("place {}: {:.0f} {:.0f} {:.0f} m".format(place_index + 1, group[0].x / 100.0, group[0].y / 100.0, group[0].z / 100.0))
    for note in notes:
        log("NOTE " + note)


main()
