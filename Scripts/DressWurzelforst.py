# Brings textures into the Wurzelforst blockout (region II), like DressAschenmark did for Aschenmark.
#
# - The ground (M_WF_Zonenkarte, which wf_blockout.py lays on the landscape) is rebuilt: forest floor laid by world
#   position, mud where the ground lies low by the water, rock on steep slopes, sand on the campaign path, all tinted
#   by the zone map so the regions stay readable.
# - The grey box master M_WF_Graubox gets a texture laid over the world (bark, rock, moss ...); every MI_WF_* instance
#   picks its texture and how strongly it shows ("Muster", 0 = plain colour as before).
# - The level template's own sun, sky light, atmosphere and fog (folder "Lighting") are switched off, the blockout brings its own.
# - The sea gets MI_WF_Wasser_Teich: the pond water, darker and with waves sized for the 4 km water plane.
# - A post process volume "Stimmung_Wurzelforst" lowers the exposure (once; tune it by hand).
# Both materials keep their names and parameters, so rerunning wf_blockout.py keeps the textures.
# Only assets under /Game/Vael/Wurzelforst/Blockout, the water plane and the template lights of L_Wurzelforst_Blockout are touched.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/DressWurzelforst.py" -EnablePlugins=PythonScriptPlugin

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
MATERIAL_FOLDER = "/Game/Vael/Wurzelforst/Blockout/Materialien"
ZONE_TEXTURE = "/Game/Vael/Wurzelforst/Blockout/T_WF_Zonenkarte"
WATER_PARENT = "/Game/VFX/Water/Materials/MI_Water_Pond"
WATER_NAME = "MI_WF_Wasser_Teich"
WATER_ACTOR = "Wasserspiegel_Ringsee_Westmeer"
TAG = "WF_Blockout"
OFF_TAG = "WF_VorlageAus"
DRESS_TAG = "WF_Kulisse"  # not WF_Blockout, so rerunning wf_blockout.py keeps it
MOOD_LABEL = "Stimmung_Wurzelforst"
DOUBLED = (unreal.DirectionalLight, unreal.SkyLight, unreal.SkyAtmosphere, unreal.ExponentialHeightFog)

# Landscape from WF_Hoehenkarte_2017.png: lower corner and size of the zone map in cm
MAP_ORIGIN = (-201600.0, -201600.0)
MAP_SIZE = 2017 * 200.0

TEXTURES = {
    "forest": "/Game/Megascans/Surfaces/ForestGround/T_ForestGround_D",
    "forest_normal": "/Game/Megascans/Surfaces/ForestGround/T_ForestGround_N",
    "mud": "/Game/Megascans/Surfaces/HeavyMud/T_HeavyMud_D",
    "sand": "/Game/MS/Surfaces/Dry_Sandy_Mud_wdunfjf/T_Dry_Sandy_Mud_wdunfjf_D",
    "rock": "/Game/Rock_Collection_04/Meshes/Rock_01/Textures/T_Rock_01_A",
    "bark": "/Game/Stylized_Tree_Pack/Textures/Stylized_Bark_Texture/T_Stylized_Bark_1_COLOR",
    "moss": "/Game/Megascans/3D_Assets/IcelandicMossClusters/T_IcelandicMossClusters_A",
}

# How much a texture brightens the colour it lies on (its grey is about 1 / this); the stylized bark is light
BRIGHTEN = {"bark": 1.1}

# Grey box instance: texture, size of one tile in cm, how strongly it shows
SURFACES = {
    "Rinde": ("bark", 250.0, 1.0), "Urstamm": ("bark", 500.0, 1.0), "Totholz": ("bark", 200.0, 1.0),
    "Mangrove": ("bark", 150.0, 1.0), "Holz": ("bark", 120.0, 0.8), "Wurzel": ("bark", 300.0, 1.0),
    "Fels": ("rock", 700.0, 1.0), "FelsMoos": ("rock", 600.0, 1.0), "Zahn": ("rock", 500.0, 0.7),
    "Moos": ("moss", 300.0, 0.8), "Krone": ("moss", 500.0, 0.7), "KroneHell": ("moss", 500.0, 0.7),
    "MangroveKrone": ("moss", 400.0, 0.7), "Seerose": ("moss", 150.0, 0.5),
    "Pilzstiel": ("mud", 200.0, 0.4), "Pilz_Orange": ("mud", 300.0, 0.35), "Pilz_Rot": ("mud", 300.0, 0.35),
    "Pilz_Tuerkis": ("mud", 300.0, 0.35), "Weg": ("sand", 300.0, 1.0),
}

library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
notes = []

WORLD_ALIGNED = "/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture"


def log(message):
    unreal.log_warning("[DressWurzelforst] " + message)


class Graph:
    """Small helper that places material nodes in columns and checks every link"""

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

    def mask(self, source, column, r=False, g=False, b=False):
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
        expression = self.node(unreal.MaterialExpressionSaturate, column)
        self.link(source, expression)
        return expression

    def sample(self, texture, uvs, column):
        expression = self.node(unreal.MaterialExpressionTextureSample, column, texture=texture,
                               sampler_type=sampler_of(texture))
        self.link(uvs, expression, "UVs")
        return expression

    def world_aligned(self, texture_object, size, column):
        """Texture projected from the three axes, so walls and trunks don't stretch"""
        call = self.node(unreal.MaterialExpressionMaterialFunctionCall, column)
        call.set_material_function(unreal.load_asset(WORLD_ALIGNED))
        self.link(texture_object, call, "TextureObject")
        size3 = self.op(unreal.MaterialExpressionMultiply, column + 1, size,
                        self.node(unreal.MaterialExpressionConstant3Vector, column + 2, constant=unreal.LinearColor(1.0, 1.0, 1.0, 1.0)))
        self.link(size3, call, "TextureSize")
        return call


def sampler_of(texture):
    compression = texture.get_editor_property("compression_settings")
    if compression == unreal.TextureCompressionSettings.TC_NORMALMAP:
        return unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    if compression == unreal.TextureCompressionSettings.TC_MASKS:
        return unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    return unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if texture.get_editor_property("srgb") else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR


def has_parameter(material, name):
    return name in [str(parameter) for parameter in library.get_scalar_parameter_names(material)]


# ---------------------------------------------------------------- Ground

def build_ground(material, textures):
    library.delete_all_material_expressions(material)
    g = Graph(material)

    position = g.node(unreal.MaterialExpressionWorldPosition, 14)
    plane = g.mask(position, 13, r=True, g=True)
    height = g.mask(position, 13, b=True)

    # Zone map over the whole landscape
    origin = g.mask(g.node(unreal.MaterialExpressionVectorParameter, 13, parameter_name="KartenUrsprung",
                           default_value=unreal.LinearColor(MAP_ORIGIN[0], MAP_ORIGIN[1], 0.0, 0.0)), 12, r=True, g=True)
    zone_uv = g.op(unreal.MaterialExpressionDivide, 11, g.op(unreal.MaterialExpressionSubtract, 12, plane, origin),
                   g.scalar("Kartengroesse", MAP_SIZE, 12))
    zone = g.mask(g.sample(unreal.load_asset(ZONE_TEXTURE), zone_uv, 10), 9, r=True, g=True, b=True)

    # Ground textures laid by world position; the forest floor at two sizes against repeating
    tile = g.scalar("KachelBoden", 400.0, 12)

    def uv(factor):
        size = g.op(unreal.MaterialExpressionMultiply, 11, tile, g.const(factor, 12))
        return g.op(unreal.MaterialExpressionDivide, 10, plane, size)

    near_uv = uv(1.0)
    forest = g.lerp(g.sample(textures["forest"], near_uv, 9), g.sample(textures["forest"], uv(3.37), 9), g.const(0.45, 9), 8)
    mud = g.sample(textures["mud"], uv(0.9), 9)
    sand = g.sample(textures["sand"], uv(0.8), 9)
    rock_object = g.node(unreal.MaterialExpressionTextureObject, 10, texture=textures["rock"])
    rock = g.world_aligned(rock_object, g.scalar("KachelFels", 900.0, 11), 8)

    # Wet mud where the ground lies low by the water (the moor lies at about half a metre)
    wet = g.saturate(g.op(unreal.MaterialExpressionDivide, 8,
                          g.op(unreal.MaterialExpressionSubtract, 9, g.scalar("NassBis", 150.0, 10), height),
                          g.const(120.0, 9)), 7)
    ground = g.lerp(forest, mud, wet, 7)

    # Rock where the slope is steep
    normal_z = g.mask(g.node(unreal.MaterialExpressionVertexNormalWS, 9), 8, b=True)
    steep = g.saturate(g.op(unreal.MaterialExpressionMultiply, 7,
                            g.op(unreal.MaterialExpressionSubtract, 8, g.scalar("FelsAb", 0.8, 9), normal_z),
                            g.const(6.0, 8)), 6)
    ground = g.lerp(ground, g.mask(rock, 7, r=True, g=True, b=True), steep, 6)

    # Sand on the campaign path: the zone map paints it in one light colour
    path_color = g.node(unreal.MaterialExpressionConstant3Vector, 8, constant=unreal.LinearColor(0.485, 0.381, 0.195, 1.0))
    distance = g.node(unreal.MaterialExpressionDistance, 7)
    g.link(zone, distance, "A")
    g.link(path_color, distance, "B")
    path = g.saturate(g.op(unreal.MaterialExpressionDivide, 6,
                           g.op(unreal.MaterialExpressionSubtract, 7, g.const(0.16, 8), distance), g.const(0.06, 7)), 5)
    ground = g.lerp(ground, sand, path, 5)

    # Tinted by the zone colour, so Hain, Moor, Kammweg, Grat and the Nid spring stay what they are on the map
    grey = g.node(unreal.MaterialExpressionDesaturation, 4)
    g.link(ground, grey)
    g.link(g.scalar("Entsaettigung", 0.6, 5), grey, "Fraction")
    tinted = g.op(unreal.MaterialExpressionMultiply, 3, g.op(unreal.MaterialExpressionMultiply, 4, grey, zone),
                  g.scalar("ZonenHelligkeit", 1.4, 4))
    albedo = g.lerp(ground, tinted, g.scalar("ZonenAnteil", 0.7, 3), 2)

    normal = g.sample(textures["forest_normal"], near_uv, 3)
    rough = g.node(unreal.MaterialExpressionLinearInterpolate, 3, const_a=0.92, const_b=0.45)
    g.link(wet, rough, "Alpha")

    library.connect_material_property(albedo, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(g.const(0.25, 2), "", unreal.MaterialProperty.MP_SPECULAR)
    library.recompile_material(material)


# ---------------------------------------------------------------- Grey box

def build_greybox(material, textures):
    """Same parameters as wf_blockout.py makes (Farbe, Leuchten), plus a texture over the world that tints the colour"""
    library.delete_all_material_expressions(material)
    g = Graph(material)

    color = g.node(unreal.MaterialExpressionVectorParameter, 6, parameter_name="Farbe", default_value=unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    glow = g.scalar("Leuchten", 0.0, 6)

    texture = g.node(unreal.MaterialExpressionTextureObjectParameter, 8, parameter_name="Textur", texture=textures["forest"])
    detail = g.world_aligned(texture, g.scalar("Kachel", 300.0, 9), 6)
    grey = g.node(unreal.MaterialExpressionDesaturation, 5)
    g.link(g.mask(detail, 6, r=True, g=True, b=True), grey)
    bright = g.op(unreal.MaterialExpressionMultiply, 4, grey, g.scalar("Aufhellung", 2.2, 5))
    strength = g.lerp(g.const(1.0, 4), bright, g.scalar("Muster", 0.0, 5), 3)
    base = g.op(unreal.MaterialExpressionMultiply, 2, g.mask(color, 3, r=True, g=True, b=True), strength)

    emissive = g.op(unreal.MaterialExpressionMultiply, 2, color, glow)

    library.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(g.const(0.85, 2), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.recompile_material(material)


def dress_instances(textures):
    count = 0
    for name, (key, tile, strength) in SURFACES.items():
        path = "{}/MI_WF_{}".format(MATERIAL_FOLDER, name)
        instance = unreal.load_asset(path)
        if instance is None:
            notes.append("missing " + path)
            continue
        library.set_material_instance_texture_parameter_value(instance, "Textur", textures[key])
        library.set_material_instance_scalar_parameter_value(instance, "Kachel", tile)
        library.set_material_instance_scalar_parameter_value(instance, "Muster", strength)
        library.set_material_instance_scalar_parameter_value(instance, "Aufhellung", BRIGHTEN.get(key, 2.2))
        library.update_material_instance(instance)
        assets.save_loaded_asset(instance, False)
        count += 1
    return count


# ---------------------------------------------------------------- Water

def make_water():
    path = "{}/{}".format(MATERIAL_FOLDER, WATER_NAME)
    if assets.does_asset_exist(path):
        instance = unreal.load_asset(path)
    else:
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(WATER_NAME, MATERIAL_FOLDER, unreal.MaterialInstanceConstant,
                                                                           unreal.MaterialInstanceConstantFactoryNew())
        library.set_material_instance_parent(instance, unreal.load_asset(WATER_PARENT))
    # The plane spans 4 km on one texture repeat: about one wave every 4 m, and a wide soft edge at the shore.
    # Most of the sea is only 60 cm deep over light ground, which the pond look turns milky: darker and murkier here.
    for name, value in (("WaveTiling", 1000.0), ("ContactFade", 150.0), ("Murkiness", 0.93)):
        library.set_material_instance_scalar_parameter_value(instance, name, value)
    library.set_material_instance_vector_parameter_value(instance, "DeepColor", unreal.LinearColor(0.003, 0.012, 0.011, 1.0))
    library.set_material_instance_vector_parameter_value(instance, "ShallowColor", unreal.LinearColor(0.03, 0.075, 0.06, 1.0))
    library.update_material_instance(instance)
    assets.save_loaded_asset(instance, False)
    return instance


def add_mood(actors, level_actors):
    """Darker exposure like in Aschenmark, so the textures don't wash out; placed once, then tuned by hand"""
    if any(MOOD_LABEL == actor.get_actor_label() for actor in level_actors):
        return 0
    volume = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0.0, 0.0, 2000.0), unreal.Rotator(0.0, 0.0, 0.0))
    volume.set_editor_property("unbound", True)
    settings = volume.get_editor_property("settings")
    settings.set_editor_property("override_auto_exposure_bias", True)
    settings.set_editor_property("auto_exposure_bias", -0.8)
    settings.set_editor_property("override_vignette_intensity", True)
    settings.set_editor_property("vignette_intensity", 0.45)
    volume.set_editor_property("settings", settings)
    volume.set_folder_path("Wurzelforst/_Licht_Atmosphaere")
    volume.set_actor_label(MOOD_LABEL)
    volume.set_editor_property("tags", [unreal.Name(DRESS_TAG)])
    return 1


def main():
    textures = {key: unreal.load_asset(path) for key, path in TEXTURES.items()}
    missing = [path for key, path in TEXTURES.items() if textures[key] is None]
    if missing or unreal.load_asset(ZONE_TEXTURE) is None:
        log("missing textures, nothing done: {}".format(missing or [ZONE_TEXTURE]))
        return

    ground = unreal.load_asset(MATERIAL_FOLDER + "/M_WF_Zonenkarte")
    greybox = unreal.load_asset(MATERIAL_FOLDER + "/M_WF_Graubox")
    if ground is None or greybox is None:
        log("run wf_blockout.py first, its materials are missing")
        return

    build_ground(ground, textures)
    assets.save_loaded_asset(ground, False)
    build_greybox(greybox, textures)
    assets.save_loaded_asset(greybox, False)
    dressed = dress_instances(textures)
    water = make_water()

    world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    watered = 0
    doubled = 0
    level_actors = actors.get_all_level_actors()
    mood = add_mood(actors, level_actors)
    for actor in level_actors:
        if actor.get_actor_label() == WATER_ACTOR:
            actor.static_mesh_component.set_material(0, water)
            watered += 1
        # The level template brought its own sun, sky light, atmosphere and fog next to the blockout's: two suns
        # make everything glare. The template's are switched off (not deleted); turning their root visible again undoes it.
        elif str(actor.get_folder_path()) == "Lighting" and isinstance(actor, DOUBLED) and not {TAG, OFF_TAG} & {str(t) for t in actor.tags}:
            actor.root_component.set_visibility(False, True)
            actor.set_actor_hidden_in_game(True)
            actor.set_editor_property("tags", list(actor.tags) + [unreal.Name(OFF_TAG)])
            doubled += 1
    if watered or doubled or mood:
        unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)

    log("ground and grey box rebuilt, {} instances textured, water on {} plane(s), {} doubled template lights off".format(dressed, watered, doubled))
    for note in notes:
        log("NOTE " + note)


main()
