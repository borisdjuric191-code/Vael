# Puts Harzkolben (Herbarium, Tripo model SM_Harzkolben) into L_Wurzelforst_Blockout: small groups on the banks of the moor,
# with the opening of the hood turned to the game camera, and one faint ember light per place.
#
# - Material M_WF_Harzkolben: the Tripo picture. The orange resin is found by its colour and glows like embers, slowly
#   pulsing, every plant in its own rhythm; the wine red hood gets a faint glow as if the embers shone through it.
#   The plant sways a little.
# - LODs on the mesh, textures at 2K except the normal map.
# Everything it adds is tagged WF_Harzkolben (Outliner folder Wurzelforst/Harzkolben); rerunning replaces only that.
# The plants have no collision and no gameplay yet: Pechschuss, fire source and shadow form come later.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildHarzkolben.py" -EnablePlugins=PythonScriptPlugin

import math
import os
import random
import struct
import zlib

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KIT = os.path.join(ROOT, "Blockout", "Vael_Wurzelforst_Blockout")
FOLDER = "/Game/Vael/Requisiten/Wurzelforst/Harzkolben"
NAME = "Harzkolben"
MATERIAL_PATH = FOLDER + "/M_WF_Harzkolben"
MESH_HEIGHT = 200.0           # cm, as exported by the 'requisite' step
TAG = "WF_Harzkolben"
OUTLINER = "Wurzelforst/Harzkolben"
SEED = 83

SPOTS = 13                    # places on the banks
GROUP = (2, 4)                # plants at one place
GROUP_REACH = 420.0           # cm from the place to the others
HEIGHTS = (170.0, 235.0)      # cm
SINK = 22.0                   # cm the peat clod sinks into the ground
CAMERA_YAW = 135.0            # the mesh opens along +Y; this turns the opening to the shared camera (its yaw is 45)
YAW_SPREAD = 35.0             # degrees to both sides
DRY_Z = 60.0                  # lowest ground in cm (sea and moor water stand at 0-6 cm)
BANK_REACH = 900.0            # cm: water within this distance makes it a bank
PLANT_ZONES = ("moor",)
PIXEL = 200.0                 # 1 zone map pixel = 2 m (as in ScatterWurzelforst.py)
ZONE_COLORS = {               # zone colours of T_WF_Zonenkarte.png (as in ScatterWurzelforst.py)
    "saum": "6e6448", "moor": "3e6b55", "hain": "3f7f45", "kamm": "3b6670", "grat": "6a6d60", "asche": "7a4a2a",
    "jaw": "4a5f78", "grat_hoch": "9a9c90", "lagune": "2f6f74", "nid": "5a3a7a", "weg": "b9a77a",
}
MIN_SPACING = 5000.0          # cm between two places
LIGHT_HEIGHT = 110.0          # cm above the ground
LIGHT_RADIUS = 480.0          # cm
LIGHT_CANDELA = 5.0

library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(SEED)
notes = []
WORLD = [None]


def log(message):
    unreal.log_warning("[BuildHarzkolben] " + message)


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

    def saturate(self, source, column):
        return self.one(unreal.MaterialExpressionSaturate, column, source)

    def sine(self, source, column):
        return self.one(unreal.MaterialExpressionSine, column, source)

    def sample(self, picture, column, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR):
        return self.node(unreal.MaterialExpressionTextureSample, column, texture=picture, sampler_type=sampler)

    def to_world(self, local, column):
        expression = self.node(unreal.MaterialExpressionTransform, column,
                               transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                               transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        self.link(local, expression)
        return expression


def texture(kind):
    path = "{}/T_{}_{}".format(FOLDER, NAME, kind)
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
    linear = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR

    picture = g.sample(texture("basecolor"), 8)
    colour = g.mask(picture, 7)
    library.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    normal_map = g.sample(texture("normal"), 3, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    library.connect_material_property(normal_map, "RGB", unreal.MaterialProperty.MP_NORMAL)
    roughness = g.sample(texture("roughness"), 4, sampler=linear)
    library.connect_material_property(g.mask(roughness, 3, g=False, b=False), "", unreal.MaterialProperty.MP_ROUGHNESS)
    metallic = g.sample(texture("metallic"), 4, sampler=linear)
    library.connect_material_property(g.mask(metallic, 3, g=False, b=False), "", unreal.MaterialProperty.MP_METALLIC)

    red = g.mask(picture, 7, g=False, b=False)
    green = g.mask(picture, 7, r=False, b=False)
    blue = g.mask(picture, 7, r=False, g=False)

    # Resin: bright red with almost no blue. Bark and dry leaf edges stay below the threshold
    resin_raw = g.div(5, g.sub(6, g.sub(7, red, blue), g.scalar("HarzSchwelle", 0.46, 7)), g.scalar("HarzWeich", 0.2, 6))
    resin = g.saturate(resin_raw, 4)
    # Hood: red far above green, without the resin
    hood_raw = g.div(5, g.sub(6, g.sub(7, red, green), g.const(0.08, 7)), g.const(0.14, 6))
    hood = g.mul(3, g.saturate(hood_raw, 4), g.sub(4, g.const(1.0, 5), resin))

    # Slow pulse, every plant in its own rhythm
    place = g.node(unreal.MaterialExpressionObjectPositionWS, 8)
    seed = g.mul(6, g.mask(place, 7, b=False, g=False), g.const(0.013, 7))
    time = g.node(unreal.MaterialExpressionTime, 8)
    pulse_phase = g.add(5, g.mul(6, time, g.scalar("PulsTempo", 0.16, 7)), seed)  # Sine counts whole turns
    pulse = g.add(3, g.const(0.72, 4), g.mul(4, g.sine(pulse_phase, 5), g.const(0.28, 5)))

    ember = g.node(unreal.MaterialExpressionVectorParameter, 4, parameter_name="Glutfarbe",
                   default_value=unreal.LinearColor(1.0, 0.24, 0.02, 1.0))
    resin_glow = g.mul(2, g.mul(3, ember, resin), g.mul(3, pulse, g.scalar("Glut", 3.2, 4)))
    hood_glow = g.mul(2, g.mul(3, colour, hood), g.mul(3, pulse, g.scalar("Kapuzenschein", 0.35, 4)))
    library.connect_material_property(g.add(1, resin_glow, hood_glow), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # The plant sways a little, more the higher up
    local_z = g.mask(g.node(unreal.MaterialExpressionLocalPosition, 8), 7, r=False, g=False)
    height_share = g.div(6, local_z, g.const(MESH_HEIGHT, 7))
    sway_amount = g.mul(3, g.mul(4, height_share, height_share), g.scalar("Schwanken", 2.5, 4))
    sway_phase = g.add(4, g.mul(5, time, g.const(0.35, 6)), seed)
    sway = g.append(2, g.append(3, g.sine(sway_phase, 4), g.sine(g.mul(5, sway_phase, g.const(0.7, 6)), 4)), g.const(0.0, 3))
    library.connect_material_property(g.to_world(g.mul(1, sway, sway_amount), 1), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def lighten(mesh):
    """LODs for the far ones, textures at 2K except the normal map"""
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary
    if subsystem.get_lod_count(mesh) < 3:
        options = unreal.StaticMeshReductionOptions()
        options.set_editor_property("auto_compute_lod_screen_size", False)
        settings = []
        for percent, screen in ((1.0, 1.0), (0.4, 0.3), (0.12, 0.1)):
            setting = unreal.StaticMeshReductionSettings()
            setting.set_editor_property("percent_triangles", percent)
            setting.set_editor_property("screen_size", screen)
            settings.append(setting)
        options.set_editor_property("reduction_settings", settings)
        subsystem.set_lods(mesh, options)
    for kind in ("basecolor", "roughness", "metallic"):
        picture = texture(kind)
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


def on_bank(x, y, top, bottom):
    """Water or wet ground close by"""
    for index in range(8):
        wx, wy = x + BANK_REACH * math.cos(index * 0.785), y + BANK_REACH * math.sin(index * 0.785)
        hit = trace(unreal.Vector(wx, wy, top), unreal.Vector(wx, wy, bottom))
        if hit is not None and hit[0].z <= DRY_Z:
            return True
    return False


def bank_spots(landscape):
    """Places on the moor banks, and around each the plants of its group, all on open land"""
    centre, extent = landscape.get_actor_bounds(False)
    corner = landscape.get_actor_location()
    zones = ZoneMap((corner.x, corner.y))
    top, bottom = centre.z + extent.z + 5000, centre.z - extent.z - 5000
    taken, places, tries = [], [], 0
    while len(places) < SPOTS and tries < SPOTS * 1500:
        tries += 1
        x = rng.uniform(centre.x - extent.x, centre.x + extent.x)
        y = rng.uniform(centre.y - extent.y, centre.y + extent.y)
        if any((x - a) ** 2 + (y - b) ** 2 < MIN_SPACING ** 2 for a, b in taken) or zones.zone(x, y) not in PLANT_ZONES:
            continue
        hit = trace(unreal.Vector(x, y, top), unreal.Vector(x, y, bottom))
        if hit is None or not on_land(hit) or not on_bank(x, y, top, bottom):
            continue
        group = []
        for index in range(rng.randint(GROUP[0], GROUP[1])):
            for _ in range(12):
                angle = rng.uniform(0.0, 6.283)
                reach = 0.0 if index == 0 else rng.uniform(GROUP_REACH * 0.4, GROUP_REACH)
                gx, gy = x + reach * math.cos(angle), y + reach * math.sin(angle)
                plant_hit = trace(unreal.Vector(gx, gy, top), unreal.Vector(gx, gy, bottom))
                if plant_hit is not None and on_land(plant_hit):
                    group.append(unreal.Vector(gx, gy, plant_hit[0].z - SINK))
                    break
        if group:
            places.append(group)
            taken.append((x, y))
    return places, tries


def place_plants(mesh):
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

    places, tries = bank_spots(landscape)
    plants = 0
    for place_index, group in enumerate(places):
        for plant_index, spot in enumerate(group):
            yaw = CAMERA_YAW + rng.uniform(-YAW_SPREAD, YAW_SPREAD)
            plant = actors.spawn_actor_from_class(unreal.StaticMeshActor, spot, unreal.Rotator(rng.uniform(-3.0, 3.0), rng.uniform(-3.0, 3.0), yaw))
            scale = rng.uniform(HEIGHTS[0], HEIGHTS[1]) / MESH_HEIGHT
            plant.set_actor_scale3d(unreal.Vector(scale, scale, scale))
            component = plant.static_mesh_component
            component.set_static_mesh(mesh)
            component.set_collision_profile_name("NoCollision")
            plant.set_actor_label("Harzkolben_{:02d}_{}".format(place_index + 1, plant_index + 1))
            plant.set_folder_path(OUTLINER)
            plant.set_editor_property("tags", [unreal.Name(TAG)])
            plants += 1
        first = group[0]
        light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(first.x, first.y, first.z + LIGHT_HEIGHT))
        glow = light.point_light_component
        glow.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        glow.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        glow.set_editor_property("intensity", LIGHT_CANDELA)
        glow.set_editor_property("attenuation_radius", LIGHT_RADIUS)
        glow.set_editor_property("light_color", unreal.Color(r=255, g=120, b=40, a=255))
        glow.set_editor_property("cast_shadows", False)
        light.set_actor_label("Harzkolben_Glut_{:02d}".format(place_index + 1))
        light.set_folder_path(OUTLINER)
        light.set_editor_property("tags", [unreal.Name(TAG)])

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log("{} places, {} plants ({} tries); {} old actors removed".format(len(places), plants, tries, removed))
    for place_index, group in enumerate(places):
        log("place {}: {:.0f} {:.0f} {:.0f} m".format(place_index + 1, group[0].x / 100.0, group[0].y / 100.0, group[0].z / 100.0))


def main():
    mesh = unreal.load_asset("{}/SM_{}".format(FOLDER, NAME))
    if mesh is None:
        log("SM_Harzkolben missing, run the import first")
        return
    material = build_material()
    mesh.set_material(0, material)
    lighten(mesh)
    assets.save_loaded_asset(mesh, False)
    place_plants(mesh)
    for note in notes:
        log(note)
    log("done, {} notes".format(len(notes)))


main()
