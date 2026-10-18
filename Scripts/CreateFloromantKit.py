# Creates the look of the Floromant (Schritt "Floromant", fifth class): one master material for all of its plants, the material of its
# moth wings, their instances and the picture of the wing.
#
# - M_Floromant_Pflanze: masked and two-sided. GrowHeight fades the plant in from the ground up with a glowing seam in GrowColor,
#   Flash lets it glow up in FlashColor (the telegraph before it acts), World Position Offset sways it in the wind, more towards the top.
#   GroundZ and PlantHeight are set by the game for each plant.
# - M_Floromant_Fluegel: thin, translucent-looking wing (two-sided foliage shading), the picture's alpha cuts the outline and the holes.
# - Only creates what is missing, except that the two master materials get their graph rebuilt when it is older than KIT_VERSION.
# - Everything lives under /Game/Vael/Characters/Floromant/Materialien.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateFloromantKit.py"

import os
import unreal

FOLDER = "/Game/Vael/Characters/Floromant/Materialien"
WING_PICTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Data", "T_Floromant_Fluegel.png")

library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
created = []

KIT_VERSION = 1.1

WHITE = "/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"
FLAT_NORMAL = "/Engine/EngineMaterials/FlatNormal.FlatNormal"


class Graph:
    """Places material expressions in columns, so the graph stays readable when Boris opens it"""

    def __init__(self, material):
        self.material = material
        self.row = {}

    def node(self, expression_class, column, **properties):
        y = self.row.get(column, 0)
        self.row[column] = y + 130
        expression = library.create_material_expression(self.material, expression_class, -320 * column, y)
        for key, value in properties.items():
            expression.set_editor_property(key, value)
        return expression

    def scalar(self, name, default, column=7):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=default)

    def color(self, name, default, column=7):
        return self.node(unreal.MaterialExpressionVectorParameter, column, parameter_name=name, default_value=default)

    def const(self, value, column=6):
        return self.node(unreal.MaterialExpressionConstant, column, r=value)

    def link(self, source, target, target_input, source_output=""):
        if not library.connect_material_expressions(source, source_output, target, target_input):
            unreal.log_error("Vael Floromant kit: could not connect {} -> {}.{}".format(source.get_name(), target.get_name(), target_input))

    def op(self, expression_class, column, a, b=None, a_input="A", b_input="B", **properties):
        expression = self.node(expression_class, column, **properties)
        self.link(a, expression, a_input)
        if b is not None:
            self.link(b, expression, b_input)
        return expression

    def mul(self, column, a, b):
        return self.op(unreal.MaterialExpressionMultiply, column, a, b)

    def add(self, column, a, b):
        return self.op(unreal.MaterialExpressionAdd, column, a, b)

    def sub(self, column, a, b):
        return self.op(unreal.MaterialExpressionSubtract, column, a, b)

    def div(self, column, a, b):
        return self.op(unreal.MaterialExpressionDivide, column, a, b)

    def lerp(self, column, a, b, alpha):
        expression = self.op(unreal.MaterialExpressionLinearInterpolate, column, a, b)
        self.link(alpha, expression, "Alpha")
        return expression

    def saturate(self, column, a):
        return self.op(unreal.MaterialExpressionSaturate, column, a, a_input="")

    def mask(self, column, source, r=False, g=False, b=False, a=False, source_output=""):
        expression = self.node(unreal.MaterialExpressionComponentMask, column, r=r, g=g, b=b, a=a)
        self.link(source, expression, "", source_output)
        return expression

    def output(self, expression, target, source_output=""):
        if not library.connect_material_property(expression, source_output, target):
            unreal.log_error("Vael Floromant kit: could not connect {} to {}".format(self.material.get_name(), target))


def try_set(target, key, value):
    try:
        target.set_editor_property(key, value)
        return True
    except Exception as error:
        unreal.log_warning("Vael Floromant kit: could not set {} on {}: {}".format(key, target.get_name(), error))
        return False


def new_material(name):
    """A new master material, or one of this kit whose graph is older than the script"""
    path = "{}/{}".format(FOLDER, name)
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        if "KitVersion" in [str(parameter) for parameter in library.get_scalar_parameter_names(material)]:
            if library.get_material_default_scalar_parameter_value(material, "KitVersion") >= KIT_VERSION - 0.001:
                return None
        library.delete_all_material_expressions(material)
        created.append(path + " (graph rebuilt)")
        return material
    created.append(path)
    return tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


# ---------------------------------------------------------------- M_Floromant_Pflanze

def build_plant_material():
    material = new_material("M_Floromant_Pflanze")
    if material is None:
        return unreal.load_asset(FOLDER + "/M_Floromant_Pflanze")

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("two_sided", True)

    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, column=9)
    base_map = g.node(unreal.MaterialExpressionTextureObjectParameter, 8, parameter_name="BaseColorMap", texture=unreal.load_asset(WHITE))
    normal_map = g.node(unreal.MaterialExpressionTextureObjectParameter, 8, parameter_name="NormalMap", texture=unreal.load_asset(FLAT_NORMAL),
                        sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    texture_amount = g.scalar("TextureAmount", 1.0)
    tint = g.color("Tint", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    roughness = g.scalar("Roughness", 0.75)
    ground_z = g.scalar("GroundZ", 0.0)
    plant_height = g.scalar("PlantHeight", 100.0)
    grow_height = g.scalar("GrowHeight", 100000.0)
    grow_color = g.color("GrowColor", unreal.LinearColor(1.0, 0.35, 0.65, 1.0))
    grow_edge = g.scalar("GrowEdgeWidth", 14.0)
    grow_glow = g.scalar("GrowGlow", 6.0)
    flash = g.scalar("Flash", 0.0)
    flash_color = g.color("FlashColor", unreal.LinearColor(1.0, 0.35, 0.65, 1.0))
    flash_glow = g.scalar("FlashGlow", 4.0)
    wind_strength = g.scalar("WindStrength", 3.0)
    wind_speed = g.scalar("WindSpeed", 0.35)

    # Colour: the model's picture, tinted; without a picture only the tint (the roots of the wall)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 7)
    base_sample = g.node(unreal.MaterialExpressionTextureSample, 6)
    g.link(base_map, base_sample, "Tex")
    g.link(uv, base_sample, "UVs")
    tinted = g.mul(5, g.mask(5, base_sample, r=True, g=True, b=True, source_output="RGB"), tint)
    base_color = g.lerp(3, tint, tinted, texture_amount)

    normal_sample = g.node(unreal.MaterialExpressionTextureSample, 6, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    g.link(normal_map, normal_sample, "Tex")
    g.link(uv, normal_sample, "UVs")

    # Height above the ground the plant grows from; a little noise makes the edge of the growth ragged
    world = g.node(unreal.MaterialExpressionWorldPosition, 8)
    height = g.sub(6, g.mask(7, world, b=True), ground_z)
    ragged = g.node(unreal.MaterialExpressionNoise, 6, scale=0.08, levels=2, noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX, output_min=-5.0, output_max=5.0)
    g.link(world, ragged, "")
    front = g.sub(5, grow_height, g.add(5, height, ragged))
    opacity_mask = g.saturate(4, g.add(4, g.div(4, front, g.const(3.0)), g.const(0.5)))

    # Glowing seam along the edge of the growth, and the flash before an action
    seam = g.saturate(4, g.sub(4, g.const(1.0), g.div(4, g.op(unreal.MaterialExpressionAbs, 4, front, a_input=""), grow_edge)))
    emissive = g.add(2, g.mul(3, g.mul(3, grow_color, seam), grow_glow), g.mul(3, g.mul(3, flash_color, flash), flash_glow))

    # Wind: the plant bends more towards its top, two waves of different speed so it never looks like a metronome
    share = g.saturate(5, g.div(5, height, plant_height))
    bend = g.mul(4, g.mul(4, share, share), wind_strength)
    time = g.node(unreal.MaterialExpressionTime, 7)
    place = g.mul(6, g.add(6, g.mask(7, world, r=True), g.mask(7, world, g=True)), g.const(0.004))
    phase = g.add(5, g.mul(5, time, wind_speed), place)
    wave = g.add(4, g.op(unreal.MaterialExpressionSine, 4, phase, a_input=""),
                 g.mul(4, g.op(unreal.MaterialExpressionSine, 4, g.mul(5, phase, g.const(2.3)), a_input=""), g.const(0.35)))
    direction = g.node(unreal.MaterialExpressionConstant3Vector, 4, constant=unreal.LinearColor(1.0, 0.6, 0.0, 0.0))
    offset = g.mul(2, g.mul(3, direction, wave), bend)

    g.output(base_color, unreal.MaterialProperty.MP_BASE_COLOR)
    g.output(normal_sample, unreal.MaterialProperty.MP_NORMAL, "RGB")
    g.output(roughness, unreal.MaterialProperty.MP_ROUGHNESS)
    g.output(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.output(opacity_mask, unreal.MaterialProperty.MP_OPACITY_MASK)
    g.output(offset, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


# ---------------------------------------------------------------- M_Floromant_Fluegel

def import_wing_picture():
    path = FOLDER + "/T_Floromant_Fluegel"
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", WING_PICTURE)
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", "T_Floromant_Fluegel")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    tools.import_asset_tasks([task])

    texture = unreal.load_asset(path)
    if texture is not None:
        # The edge of the wing must not bleed into its neighbours when the picture is made smaller
        texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
        texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
        assets.save_loaded_asset(texture, False)
        created.append(path)
    return texture


def build_wing_material(picture):
    material = new_material("M_Floromant_Fluegel")
    if material is None:
        return unreal.load_asset(FOLDER + "/M_Floromant_Fluegel")

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("shading_model", getattr(unreal.MaterialShadingModel, "MSM_TWO_SIDED_FOLIAGE", unreal.MaterialShadingModel.MSM_DEFAULT_LIT))
    material.set_editor_property("two_sided", True)

    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, column=8)
    wing_map = g.node(unreal.MaterialExpressionTextureObjectParameter, 7, parameter_name="WingMap", texture=picture if picture is not None else unreal.load_asset(WHITE))
    tint = g.color("Tint", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    glow = g.scalar("Glow", 0.12)
    glow_color = g.color("GlowColor", unreal.LinearColor(1.0, 0.55, 0.8, 1.0))
    light_through = g.scalar("LightThrough", 0.8)

    sample = g.node(unreal.MaterialExpressionTextureSample, 6)
    g.link(wing_map, sample, "Tex")
    g.link(g.node(unreal.MaterialExpressionTextureCoordinate, 7), sample, "UVs")
    picture_color = g.mask(5, sample, r=True, g=True, b=True, source_output="RGB")
    base_color = g.mul(4, picture_color, tint)

    # The membrane lets light through in its own colours and shimmers faintly, like fairy threads in the dark
    g.output(base_color, unreal.MaterialProperty.MP_BASE_COLOR)
    g.output(g.mul(3, base_color, light_through), unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    g.output(g.mul(3, g.mul(4, picture_color, glow_color), glow), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.output(g.const(0.45, column=4), unreal.MaterialProperty.MP_ROUGHNESS)
    g.output(sample, unreal.MaterialProperty.MP_OPACITY_MASK, "A")

    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


# ---------------------------------------------------------------- Instances

def texture_fits(path, normal):
    """True if the texture can go into the slot without breaking the material: normal maps need normal compression, colour needs sRGB"""
    texture = unreal.load_asset(path) if assets.does_asset_exist(path) else None
    if texture is None:
        return None
    if normal:
        return texture if texture.get_editor_property("compression_settings") == unreal.TextureCompressionSettings.TC_NORMALMAP else None
    return texture if texture.get_editor_property("srgb") else None


def make_instance(parent, name, textures=None, scalars=None, colors=None):
    path = "{}/{}".format(FOLDER, name)
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)

    instance = tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(instance, parent)
    for key, (texture_path, normal) in (textures or {}).items():
        texture = texture_fits(texture_path, normal)
        if texture is not None:
            library.set_material_instance_texture_parameter_value(instance, key, texture)
        else:
            unreal.log_warning("Vael Floromant kit: {} left out of {}".format(texture_path, name))
    for key, value in (scalars or {}).items():
        library.set_material_instance_scalar_parameter_value(instance, key, value)
    for key, value in (colors or {}).items():
        library.set_material_instance_vector_parameter_value(instance, key, unreal.LinearColor(value[0], value[1], value[2], 1.0))
    assets.save_loaded_asset(instance, False)
    created.append(path)
    return instance


# Look of the roots of the Wurzelwand; also applied by Scripts/TuneFloromantKit.py to an instance that exists already
ROOT_SCALARS = {"TextureAmount": 0.0, "WindStrength": 0.6, "Roughness": 0.85, "GrowEdgeWidth": 20.0}
ROOT_COLORS = {"Tint": (0.16, 0.095, 0.055)}


def prop_textures(folder, name):
    return {
        "BaseColorMap": ("{}/{}/T_{}_basecolor".format(folder, name, name), False),
        "NormalMap": ("{}/{}/T_{}_normal".format(folder, name, name), True),
    }


# ---------------------------------------------------------------- Plants of the Floromant (DA_Pflanze_<Id>)

PLANT_FOLDER = "/Game/Vael/Characters/Floromant/Pflanzen"
E = None   # filled in main: unreal.VaelPlantEffect

# Same ids as the seed plants of the garden; light form, shadow form, what each does on the Ruf des Gartens, look and values.
# Colours: light form like its glow in the garden, shadow form drained and dark. Values first guesses, tuned in the data assets.
PLANTS = [
    # id, light name, shadow name, light effect, shadow effect, height, colour, shadow colour, sow, call, health, power, radius
    ("Glutdistel", "Glutdistel", "", "FIRE_SEEDS", "NONE", 110.0, (1.0, 0.32, 0.12), (0.5, 0.2, 0.3), 35.0, 7.0, 70.0, 9.0, 400.0),
    ("Glimmklee", "Glimmklee", "Schwelklee", "BLIND_AND_SAFT", "SMOKE", 55.0, (1.0, 0.62, 0.15), (0.5, 0.38, 0.6), 20.0, 5.0, 50.0, 10.0, 380.0),
    ("Taublatt", "Taublatt", "Faulblatt", "WET_AND_SLOW", "POISON_POOL", 70.0, (0.3, 0.85, 1.0), (0.45, 0.7, 0.2), 20.0, 5.0, 60.0, 6.0, 380.0),
    ("Windhalm", "Windhalm", "Fl\u00fcsterhalm", "POLLEN_DARTS", "CONFUSE", 110.0, (0.4, 0.95, 1.0), (0.6, 0.4, 0.8), 25.0, 6.0, 55.0, 7.0, 420.0),
    ("Schlotwinde", "Schlotwinde", "W\u00fcrgwinde", "VINE_WALL", "HOLD", 120.0, (1.0, 0.8, 0.2), (0.5, 0.3, 0.55), 30.0, 8.0, 90.0, 8.0, 320.0),
    ("Russrose", "Ru\u00dfrose", "Blutrose", "HEAL_ALLIES", "LIFE_STEAL", 80.0, (0.2, 1.0, 0.5), (0.75, 0.1, 0.2), 35.0, 8.0, 60.0, 7.0, 400.0),
    ("Milchstern", "Milchstern", "Gallstern", "CLEANSE", "WEAKEN", 70.0, (0.85, 0.8, 1.0), (0.55, 0.6, 0.2), 25.0, 6.0, 50.0, 6.0, 420.0),
    ("Funkenmohn", "Funkenmohn", "Schwarzmohn", "FIRE_BURST", "SLEEP", 75.0, (1.0, 0.4, 0.1), (0.25, 0.15, 0.35), 30.0, 8.0, 40.0, 10.0, 380.0),
]


def make_plants(plant_material):
    effects = unreal.VaelPlantEffect
    props = "/Game/Vael/Requisiten/Aschenmark"
    for (pid, light, shadow, light_effect, shadow_effect, height, color, shadow_color, sow, call, health, power, radius) in PLANTS:
        mi = make_instance(plant_material, "MI_Floromant_" + pid, prop_textures(props, pid), {"WindStrength": 2.0})
        path = "{}/DA_Pflanze_{}".format(PLANT_FOLDER, pid)
        if assets.does_asset_exist(path):
            continue
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.VaelPlantData)
        data = tools.create_asset("DA_Pflanze_" + pid, PLANT_FOLDER, unreal.VaelPlantData, factory)
        data.set_editor_property("plant_id", pid)
        data.set_editor_property("light_name", unreal.Text(light.encode().decode("unicode_escape")))
        data.set_editor_property("shadow_name", unreal.Text(shadow.encode().decode("unicode_escape")))
        data.set_editor_property("light_effect", getattr(effects, light_effect))
        data.set_editor_property("shadow_effect", getattr(effects, shadow_effect))
        mesh = unreal.load_asset("{}/{}/SM_{}".format(props, pid, pid))
        if mesh is not None:
            data.set_editor_property("mesh", mesh)
        data.set_editor_property("material", mi)
        data.set_editor_property("height", height)
        data.set_editor_property("color", unreal.LinearColor(color[0], color[1], color[2], 1.0))
        data.set_editor_property("shadow_color", unreal.LinearColor(shadow_color[0], shadow_color[1], shadow_color[2], 1.0))
        data.set_editor_property("sow_cost", sow)
        data.set_editor_property("call_cost", call)
        data.set_editor_property("health", health)
        data.set_editor_property("power", power)
        data.set_editor_property("radius", radius)
        assets.save_loaded_asset(data, False)
        created.append(path)


def main():
    plant = build_plant_material()
    make_instance(plant, "MI_Floromant_Glutdistel", prop_textures("/Game/Vael/Requisiten/Aschenmark", "Glutdistel"), {"WindStrength": 2.5})
    # No Tränenkelch model yet: the Heilkelch stands in, a little cooler in colour
    make_instance(plant, "MI_Floromant_Traenenkelch", prop_textures("/Game/Vael/Requisiten/Wurzelforst", "Heilkelch"), {"WindStrength": 2.0}, {"Tint": (0.9, 1.0, 1.05)})
    make_instance(plant, "MI_Floromant_Wurzel", None, ROOT_SCALARS, ROOT_COLORS)
    make_plants(plant)

    picture = import_wing_picture()
    wing = build_wing_material(picture)
    make_instance(wing, "MI_Floromant_Fluegel")

    assets.save_directory(FOLDER)
    assets.save_directory(PLANT_FOLDER)
    unreal.log("[Vael] Floromant kit: " + (", ".join(created) if created else "nothing to do"))


main()
