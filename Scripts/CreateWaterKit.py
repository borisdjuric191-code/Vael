# Creates the water kit, part 1 (Designbibel, section "Wasser"): one master material for all water surfaces and bodies,
# one particle material for the Niagara sprites, their instances, the water sources as data and BP_WaterOrb.
#
# - Only creates what is missing; existing assets are never changed, except that the two master materials get their graph
#   rebuilt when it is older than KIT_VERSION (instances keep working). Assets starting with NE_Water_ or NS_Water_
#   belong to Boris (Niagara) and are never touched.
# - Everything lives under /Game/VFX/Water (Materials, Niagara, Sources, Blueprints).
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateWaterKit.py" -EnablePlugins=PythonScriptPlugin

import unreal

ROOT = "/Game/VFX/Water"
MATERIALS = ROOT + "/Materials"
SOURCES = ROOT + "/Sources"
BLUEPRINTS = ROOT + "/Blueprints"

library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
created = []


# ---------------------------------------------------------------- Graph helpers

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

    def scalar(self, name, default, column=6):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=default)

    def color(self, name, default, column=6):
        return self.node(unreal.MaterialExpressionVectorParameter, column, parameter_name=name, default_value=default)

    def const(self, value, column=5):
        return self.node(unreal.MaterialExpressionConstant, column, r=value)

    def link(self, source, target, target_input, source_output=""):
        if not library.connect_material_expressions(source, source_output, target, target_input):
            unreal.log_error("Vael water kit: could not connect {} -> {}.{}".format(source.get_name(), target.get_name(), target_input))

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

    def one_minus(self, column, a):
        return self.op(unreal.MaterialExpressionOneMinus, column, a, a_input="")

    def saturate(self, column, a):
        return self.op(unreal.MaterialExpressionSaturate, column, a, a_input="")


def try_set(target, key, value):
    try:
        target.set_editor_property(key, value)
        return True
    except Exception as error:
        unreal.log_warning("Vael water kit: could not set {} on {}: {}".format(key, target.get_name(), error))
        return False


# Bumped whenever a graph of this script changes; a master material without the current version gets its graph rebuilt
KIT_VERSION = 5.5

# Water normal map from the engine's example content, always there
WAVE_NORMAL = "/Engine/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/water_n.water_n"

# Plain white: without a painted mask the puddle material shows its whole plane
PUDDLE_MASK_DEFAULT = "/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"


def new_material(name):
    """A new master material, or one of this kit whose graph is older than the script (its parameters keep their names, so instances stay valid)"""
    path = "{}/{}".format(MATERIALS, name)
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        if "KitVersion" in [str(parameter) for parameter in library.get_scalar_parameter_names(material)]:
            if library.get_material_default_scalar_parameter_value(material, "KitVersion") >= KIT_VERSION - 0.001:  # stored as float32, 5.1 reads back as 5.0999
                return None
        library.delete_all_material_expressions(material)
        created.append(path + " (graph rebuilt)")
        return material
    created.append(path)
    return tools.create_asset(name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())



# ---------------------------------------------------------------- M_Water

def build_water_material(name="M_Water", puddle=False):
    """Translucent surface: wobbling surface, Fresnel rim, slight refraction, soft edges where it meets geometry.
    The puddle variant lies on a flat plane: its outline comes from the mask texture PuddleMask that the game paints
    (0.5 is the edge), placed in the world by MaskOrigin and MaskSize; its waves are laid out in the world, so they keep their size while it grows."""
    material = new_material(name)
    if material is None:
        return unreal.load_asset(MATERIALS + "/" + name)

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    try_set(material, "translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    try_set(material, "refraction_method", unreal.RefractionMode.RM_INDEX_OF_REFRACTION)

    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, column=8)
    deep = g.color("DeepColor", unreal.LinearColor(0.02, 0.12, 0.35, 1.0))
    shallow = g.color("ShallowColor", unreal.LinearColor(0.25, 0.65, 1.0, 1.0))
    murk = g.scalar("Murkiness", 0.2)
    emissive_strength = g.scalar("EmissiveStrength", 0.5)
    wobble_strength = g.scalar("WobbleStrength", 4.0)
    wobble_speed = g.scalar("WobbleSpeed", 0.6)
    refraction = g.scalar("Refraction", 0.2)
    fresnel_power = g.scalar("FresnelPower", 3.0)
    foam_amount = g.scalar("FoamAmount", 0.2)
    wetness = g.scalar("Wetness", 0.9)

    # Rim: Fresnel brightens the edge towards the shallow color
    fresnel = g.node(unreal.MaterialExpressionFresnel, 5)
    g.link(fresnel_power, fresnel, "ExponentIn")
    body = g.lerp(4, shallow, deep, murk)
    rim = g.lerp(3, body, shallow, fresnel)

    # Soft edge where the water meets the ground or a body; foam gathers there and a little on the rim
    depth = g.node(unreal.MaterialExpressionDepthFade, 5)
    try_set(depth, "fade_distance_default", 40.0)
    # Flat water like puddles needs a much shorter fade, or it vanishes into the ground it lies on
    g.link(g.scalar("ContactFade", 40.0), depth, "FadeDistance")
    contact = g.one_minus(4, depth)
    foam_noise = g.node(unreal.MaterialExpressionNoise, 5, scale=0.05, levels=3, noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX, output_min=0.0, output_max=1.0)
    foam_base = g.add(4, contact, g.mul(4, fresnel, g.const(0.3)))

    world_xy = None
    puddle_mask = None
    if puddle:
        # Where the puddle is: the painted mask, laid over the world; a soft edge around 0.5 and a band of foam along it
        world_xy = g.node(unreal.MaterialExpressionComponentMask, 7, r=True, g=True, b=False, a=False)
        g.link(g.node(unreal.MaterialExpressionWorldPosition, 8), world_xy, "")
        mask_origin = g.node(unreal.MaterialExpressionComponentMask, 7, r=True, g=True, b=False, a=False)
        g.link(g.color("MaskOrigin", unreal.LinearColor(0.0, 0.0, 0.0, 0.0), column=8), mask_origin, "")
        mask_uv = g.div(6, g.sub(6, world_xy, mask_origin), g.scalar("MaskSize", 100.0, column=7))
        mask_texture = g.node(unreal.MaterialExpressionTextureObjectParameter, 7, parameter_name="PuddleMask", texture=unreal.load_asset(PUDDLE_MASK_DEFAULT))
        mask_sample = g.node(unreal.MaterialExpressionTextureSample, 5)
        g.link(mask_texture, mask_sample, "Tex")
        g.link(mask_uv, mask_sample, "UVs")
        field = g.node(unreal.MaterialExpressionComponentMask, 4, r=True, g=False, b=False, a=False)
        g.link(mask_sample, field, "")
        edge_softness = g.scalar("EdgeSoftness", 0.08)
        puddle_mask = g.saturate(3, g.div(4, g.sub(4, field, g.sub(5, g.const(0.5), edge_softness)), g.mul(5, edge_softness, g.const(2.0))))
        edge_offset = g.op(unreal.MaterialExpressionAbs, 4, g.sub(4, field, g.const(0.5)), a_input="")
        edge_band = g.saturate(3, g.one_minus(3, g.div(4, edge_offset, g.scalar("EdgeFoamWidth", 0.12))))
        foam_base = g.add(3, foam_base, g.mul(3, edge_band, g.scalar("EdgeFoam", 0.5)))

    foam = g.saturate(2, g.mul(3, g.mul(3, foam_base, foam_amount), foam_noise))
    white = g.node(unreal.MaterialExpressionConstant3Vector, 3, constant=unreal.LinearColor(0.9, 0.95, 1.0, 1.0))
    base_color = g.lerp(1, rim, white, foam)

    # Small waves: two layers of a water normal map drift across each other; they bend the light (refraction), break up the
    # highlights and the Fresnel rim, and where their crests meet a little light sparkles inside
    wave_strength = g.scalar("WaveStrength", 0.6)
    wave_tiling = g.scalar("WaveTiling", 2.0)
    wave_speed = g.scalar("WaveSpeed", 0.06)
    sparkle = g.scalar("Sparkle", 0.5)
    wave_texture = g.node(unreal.MaterialExpressionTextureObjectParameter, 7, parameter_name="WaveNormal", texture=unreal.load_asset(WAVE_NORMAL), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    wave_base = g.mul(7, world_xy, g.const(1.0 / 150.0, column=8)) if puddle else g.node(unreal.MaterialExpressionTextureCoordinate, 7)
    wave_uv = g.mul(6, wave_base, wave_tiling)
    wave_time = g.mul(6, g.node(unreal.MaterialExpressionTime, 7), wave_speed)
    waves = []
    for scale, direction in ((1.0, (1.0, 0.35)), (1.7, (-0.6, 0.8))):
        drift = g.mul(5, wave_time, g.node(unreal.MaterialExpressionConstant2Vector, 6, r=direction[0], g=direction[1]))
        uv = g.add(5, g.mul(5, wave_uv, g.const(scale, column=6)), drift)
        sample = g.node(unreal.MaterialExpressionTextureSample, 4, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        g.link(wave_texture, sample, "Tex")
        g.link(uv, sample, "UVs")
        waves.append(sample)
    wave_sum = g.add(3, waves[0], waves[1])
    flat = g.node(unreal.MaterialExpressionConstant3Vector, 3, constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    wave_normal = g.lerp(2, flat, wave_sum, wave_strength)

    crest = g.node(unreal.MaterialExpressionComponentMask, 3, r=True, g=False, b=False, a=False)
    g.link(wave_sum, crest, "")
    crest_light = g.saturate(2, g.sub(3, g.op(unreal.MaterialExpressionAbs, 3, crest, a_input=""), g.const(0.9)))
    shimmer = g.mul(2, g.mul(2, crest_light, sparkle), g.const(3.0))

    emissive_body = g.mul(1, g.mul(2, shallow, emissive_strength), g.add(2, fresnel, g.const(0.4)))
    emissive = g.add(1, emissive_body, g.mul(2, shallow, shimmer))

    # Clear water lets more through; the rim and foam are more solid; the soft edge fades it out at contacts
    body_opacity = g.lerp(3, g.const(0.3), g.const(0.95), murk)
    opacity = g.mul(1, g.saturate(2, g.add(2, g.add(3, body_opacity, g.mul(3, fresnel, g.const(0.35))), foam)), depth)
    if puddle:
        opacity = g.mul(0, opacity, puddle_mask)

    roughness = g.lerp(1, g.const(0.6), g.const(0.03), wetness)
    # SpecularScale tones the reflection down without making the surface rough, for water in shade like a well
    specular = g.mul(0, g.lerp(1, g.const(0.3), g.const(0.9), wetness), g.scalar("SpecularScale", 1.0))
    index_of_refraction = g.add(1, refraction, g.const(1.0))
    if puddle:
        # Outside the outline the plane must not bend the light either, or its square shows
        index_of_refraction = g.lerp(0, g.const(1.0), index_of_refraction, puddle_mask)

    # Wobble: animated noise moves the surface along its normal
    world_position = g.node(unreal.MaterialExpressionWorldPosition, 5)
    time = g.node(unreal.MaterialExpressionTime, 5)
    drift = g.mul(4, g.mul(4, time, wobble_speed), g.const(100.0))
    wobble_noise = g.node(unreal.MaterialExpressionNoise, 3, scale=0.015, levels=2, noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX, output_min=-1.0, output_max=1.0)
    g.link(g.add(4, world_position, drift), wobble_noise, "")
    normal = g.node(unreal.MaterialExpressionVertexNormalWS, 3)
    offset = g.mul(1, g.mul(2, normal, wobble_noise), wobble_strength)

    for expression, target in ((base_color, unreal.MaterialProperty.MP_BASE_COLOR), (emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR),
                               (opacity, unreal.MaterialProperty.MP_OPACITY), (roughness, unreal.MaterialProperty.MP_ROUGHNESS),
                               (wave_normal, unreal.MaterialProperty.MP_NORMAL),
                               (specular, unreal.MaterialProperty.MP_SPECULAR), (index_of_refraction, unreal.MaterialProperty.MP_REFRACTION),
                               (offset, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)):
        if not library.connect_material_property(expression, "", target):
            unreal.log_error("Vael water kit: could not connect M_Water output {}".format(target))

    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


# ---------------------------------------------------------------- M_Water_Wave

# Look of the mage's waves; also applied by Scripts/TuneWaterWave.py to an instance that exists already
WAVE_MAGE_SCALARS = {"EmissiveStrength": 0.1, "FoamScale": 2.2, "FoamBoil": 0.6, "BodyOpacity": 0.95, "EdgeRagged": 0.9, "Specular": 0.1, "Roughness": 0.2, "NormalStrength": 0.4, "ContactFade": 6.0}
WAVE_MAGE_COLORS = {"DeepColor": (0.002, 0.012, 0.045), "MidColor": (0.004, 0.06, 0.18), "CrestColor": (0.12, 0.42, 0.6), "FoamColor": (0.9, 0.95, 1.0)}

# Look of the Blutflut: thick Mark blood, black-red at its foot, violet-red at the crest, mauve froth; glows a little more than spell water
WAVE_MARK_SCALARS = {"EmissiveStrength": 0.22, "FoamScale": 1.8, "FoamBoil": 0.45, "BodyOpacity": 0.98, "EdgeRagged": 0.8, "Specular": 0.15, "Roughness": 0.12, "NormalStrength": 0.35, "ContactFade": 6.0}
WAVE_MARK_COLORS = {"DeepColor": (0.012, 0.0, 0.006), "MidColor": (0.09, 0.004, 0.025), "CrestColor": (0.3, 0.035, 0.16), "FoamColor": (0.5, 0.28, 0.42)}


def build_wave_material():
    """Water in motion that the game builds as a mesh every frame: waves, the strand drawn from a pond, spray and drops.
    The vertex colour says what each point is: R how high in the wave (dark blue at its foot, pale at the crest),
    G how much foam, B 1 for a drop (a soft round dot from its UV), A how solid (low values tear into ragged edges).
    UVs are in metres and scroll with the water, so the boiling foam flows with it."""
    material = new_material("M_Water_Wave")
    if material is None:
        return unreal.load_asset(MATERIALS + "/M_Water_Wave")

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("two_sided", True)
    try_set(material, "translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)

    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, column=8)
    deep = g.color("DeepColor", unreal.LinearColor(0.003, 0.02, 0.07, 1.0))
    middle = g.color("MidColor", unreal.LinearColor(0.01, 0.12, 0.26, 1.0))
    crest = g.color("CrestColor", unreal.LinearColor(0.3, 0.65, 0.85, 1.0))
    foam_color = g.color("FoamColor", unreal.LinearColor(0.9, 0.95, 1.0, 1.0))
    foam_scale = g.scalar("FoamScale", 2.2)
    foam_boil = g.scalar("FoamBoil", 0.6)
    foam_sharpness = g.scalar("FoamSharpness", 4.0)
    body_opacity = g.scalar("BodyOpacity", 0.93)
    edge_ragged = g.scalar("EdgeRagged", 0.9)
    normal_strength = g.scalar("NormalStrength", 0.5)
    normal_tiling = g.scalar("NormalTiling", 1.5)
    emissive_strength = g.scalar("EmissiveStrength", 0.12)
    specular_value = g.scalar("Specular", 0.25)
    smooth = g.scalar("Roughness", 0.06)
    contact_fade = g.scalar("ContactFade", 6.0)

    vertex = g.node(unreal.MaterialExpressionVertexColor, 7)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 7)
    time = g.node(unreal.MaterialExpressionTime, 7)

    def channel(name):
        """One channel of the vertex colour, through a multiply by 1 so the other helpers can take it"""
        single = g.node(unreal.MaterialExpressionMultiply, 6, const_b=1.0)
        g.link(vertex, single, "A", name)
        return single

    height = channel("R")
    foam = channel("G")
    drop = channel("B")
    solid_alpha = channel("A")

    # Boiling foam: two layers of ridged noise drift through time at different sizes; the ridges make a lace of thin foam veins,
    # stretched along the flow (V) like the streaks foam draws over a wave
    stretch = g.node(unreal.MaterialExpressionConstant2Vector, 7, r=1.0, g=0.6)
    noise_values = []
    for size, boil, offset, levels, column in ((1.0, 1.0, 0.0, 4, 4), (2.7, 1.7, 7.0, 3, 4)):
        position = g.op(unreal.MaterialExpressionAppendVector, 5, g.mul(6, g.mul(6, uv, stretch), g.mul(6, foam_scale, g.const(size, column=7))),
                        g.add(6, g.mul(6, time, g.mul(6, foam_boil, g.const(boil, column=7))), g.const(offset, column=7)))
        noise = g.node(unreal.MaterialExpressionNoise, column, scale=1.0, levels=levels, noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX, output_min=0.0, output_max=1.0)
        g.link(position, noise, "")
        ridge = g.one_minus(4, g.op(unreal.MaterialExpressionAbs, 4, g.sub(4, g.mul(4, noise, g.const(2.0)), g.const(1.0)), a_input=""))
        thin = g.node(unreal.MaterialExpressionPower, 3, const_exponent=6.0)
        g.link(ridge, thin, "Base")
        noise_values.append((noise, thin))
    # Blotchy noise tears the thin parts; the thin lace of ridges draws the foam veins
    churn = g.add(3, g.mul(3, noise_values[0][0], g.const(0.6)), g.mul(3, noise_values[1][0], g.const(0.4)))
    lace = g.add(3, g.mul(3, noise_values[0][1], g.const(0.6)), g.mul(3, noise_values[1][1], g.const(0.4)))
    foam_pattern = g.add(2, g.mul(3, lace, g.const(0.65)), g.mul(3, churn, g.const(0.35)))

    foam_mask = g.saturate(2, g.mul(2, g.sub(2, g.add(3, foam, foam_pattern), g.const(1.0)), foam_sharpness))

    # Dark blue at the foot, deep turquoise in the body, pale blue-white at the crest, foam on top
    lower = g.lerp(3, deep, middle, g.saturate(4, g.mul(4, height, g.const(1.7))))
    body = g.lerp(2, lower, crest, g.saturate(3, g.mul(3, g.sub(4, height, g.const(0.55)), g.const(2.2))))
    base_color = g.lerp(1, body, foam_color, foam_mask)

    # Thin parts tear into ragged edges; drops are soft round dots
    torn = g.saturate(2, g.add(2, g.mul(3, g.sub(3, solid_alpha, g.mul(4, g.one_minus(4, churn), edge_ragged)), g.const(4.0)),
                                 g.mul(3, solid_alpha, g.one_minus(4, edge_ragged))))
    ragged = g.lerp(1, torn, solid_alpha, drop)
    dot_distance = g.mul(3, g.op(unreal.MaterialExpressionDistance, 4, uv, g.node(unreal.MaterialExpressionConstant2Vector, 5, r=0.5, g=0.5)), g.const(2.0))
    dot = g.saturate(2, g.mul(2, g.one_minus(3, dot_distance), g.const(2.5)))
    shape = g.lerp(1, g.const(1.0), dot, drop)

    soft = g.node(unreal.MaterialExpressionDepthFade, 2)
    try_set(soft, "fade_distance_default", 25.0)
    g.link(contact_fade, soft, "FadeDistance")
    opacity = g.mul(0, g.mul(1, g.mul(1, g.saturate(2, g.add(2, body_opacity, g.mul(3, foam_mask, g.const(0.5)))), ragged), shape), soft)

    # Ripples over the moving water; foam is rough and flat
    wave_texture = g.node(unreal.MaterialExpressionTextureObjectParameter, 7, parameter_name="WaveNormal", texture=unreal.load_asset(WAVE_NORMAL), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    ripples = []
    for scale, direction in ((1.0, (0.3, 1.0)), (1.9, (-0.7, 0.6))):
        drift = g.mul(5, g.mul(5, time, g.const(0.08)), g.node(unreal.MaterialExpressionConstant2Vector, 6, r=direction[0], g=direction[1]))
        sample = g.node(unreal.MaterialExpressionTextureSample, 4, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        g.link(wave_texture, sample, "Tex")
        g.link(g.add(5, g.mul(5, g.mul(6, uv, normal_tiling), g.const(scale)), drift), sample, "UVs")
        ripples.append(sample)
    flat = g.node(unreal.MaterialExpressionConstant3Vector, 3, constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    normal = g.lerp(2, flat, g.add(3, ripples[0], ripples[1]), g.mul(3, normal_strength, g.one_minus(3, foam_mask)))

    roughness = g.lerp(1, smooth, g.const(0.6), foam_mask)
    emissive = g.mul(1, base_color, g.mul(2, emissive_strength, g.add(3, height, g.const(0.3))))

    for expression, target in ((base_color, unreal.MaterialProperty.MP_BASE_COLOR), (emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR),
                               (opacity, unreal.MaterialProperty.MP_OPACITY), (roughness, unreal.MaterialProperty.MP_ROUGHNESS),
                               (normal, unreal.MaterialProperty.MP_NORMAL), (specular_value, unreal.MaterialProperty.MP_SPECULAR)):
        if not library.connect_material_property(expression, "", target):
            unreal.log_error("Vael water kit: could not connect M_Water_Wave output {}".format(target))

    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


# ---------------------------------------------------------------- M_Water_Particle

def build_particle_material():
    """Soft round sprite in the particle color; drop, bubble ring and mist are all instances. Fades out by itself before the particle dies."""
    material = new_material("M_Water_Particle")
    if material is None:
        return unreal.load_asset(MATERIALS + "/M_Water_Particle")

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    try_set(material, "used_with_niagara_sprites", True)
    try_set(material, "used_with_particle_sprites", True)

    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, column=8)
    edge_softness = g.scalar("EdgeSoftness", 0.25)
    inner_opacity = g.scalar("InnerOpacity", 1.0)
    ring_amount = g.scalar("RingAmount", 0.0)
    ring_radius = g.scalar("RingRadius", 0.8)
    ring_width = g.scalar("RingWidth", 0.15)
    highlight_strength = g.scalar("HighlightStrength", 0.0)
    highlight_size = g.scalar("HighlightSize", 0.3)
    cloudiness = g.scalar("Cloudiness", 0.0)
    cloud_scale = g.scalar("CloudScale", 3.0)
    opacity_scale = g.scalar("Opacity", 1.0)
    brightness = g.scalar("Brightness", 1.5)
    fade_out_start = g.scalar("FadeOutStart", 0.6)
    fade_in_end = g.scalar("FadeInEnd", 0.05)

    # Distance from the middle of the sprite: 0 in the middle, 1 at the edge of the disc
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 6)
    middle = g.node(unreal.MaterialExpressionConstant2Vector, 6, r=0.5, g=0.5)
    distance = g.mul(4, g.op(unreal.MaterialExpressionDistance, 5, uv, middle), g.const(2.0))

    disc = g.saturate(3, g.div(4, g.one_minus(4, distance), edge_softness))
    ring_offset = g.op(unreal.MaterialExpressionAbs, 4, g.sub(4, distance, ring_radius), a_input="")
    ring = g.saturate(3, g.one_minus(3, g.div(3, ring_offset, ring_width)))
    shape = g.op(unreal.MaterialExpressionMax, 2, g.mul(3, disc, inner_opacity), g.mul(3, ring, ring_amount))

    # Glint up and to the left, like light on a drop
    glint_spot = g.node(unreal.MaterialExpressionConstant2Vector, 6, r=0.36, g=0.32)
    glint_distance = g.mul(4, g.op(unreal.MaterialExpressionDistance, 5, uv, glint_spot), g.const(2.0))
    glint_falloff = g.saturate(3, g.one_minus(3, g.div(4, glint_distance, highlight_size)))
    glint = g.mul(2, g.mul(3, glint_falloff, glint_falloff), highlight_strength)

    # Clouds for the mist, different for every particle
    random = g.node(unreal.MaterialExpressionParticleRandom, 6)
    cloud_position = g.op(unreal.MaterialExpressionAppendVector, 4, g.mul(5, uv, cloud_scale), g.mul(5, random, g.const(10.0)))
    cloud_noise = g.node(unreal.MaterialExpressionNoise, 3, scale=1.0, levels=3, noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX, output_min=0.0, output_max=1.0)
    g.link(cloud_position, cloud_noise, "")
    cloud = g.lerp(2, g.const(1.0), cloud_noise, cloudiness)

    # Fades in quickly at birth and out over the last part of its life
    life = g.node(unreal.MaterialExpressionParticleRelativeTime, 6)
    fade_out = g.one_minus(2, g.saturate(3, g.div(4, g.sub(5, life, fade_out_start), g.one_minus(5, fade_out_start))))
    fade_in = g.saturate(3, g.div(4, life, fade_in_end))
    fade = g.mul(1, fade_out, fade_in)

    particle_color = g.node(unreal.MaterialExpressionParticleColor, 6)
    alpha = g.node(unreal.MaterialExpressionMultiply, 2)
    g.link(particle_color, alpha, "A", "A")
    g.link(opacity_scale, alpha, "B")
    body = g.mul(1, g.mul(1, g.mul(2, alpha, shape), cloud), fade)
    soft = g.node(unreal.MaterialExpressionDepthFade, 2)
    try_set(soft, "fade_distance_default", 15.0)
    opacity = g.mul(0, g.saturate(1, g.add(1, body, g.mul(1, glint, fade))), soft)

    tinted = g.node(unreal.MaterialExpressionMultiply, 2)
    g.link(particle_color, tinted, "A", "")
    g.link(brightness, tinted, "B")
    emissive = g.add(0, tinted, glint)

    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


# ---------------------------------------------------------------- Instances

def make_instance(parent, name, scalars=None, colors=None):
    path = "{}/{}".format(MATERIALS, name)
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    instance = tools.create_asset(name, MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(instance, parent)
    for key, value in (scalars or {}).items():
        library.set_material_instance_scalar_parameter_value(instance, key, value)
    for key, value in (colors or {}).items():
        library.set_material_instance_vector_parameter_value(instance, key, unreal.LinearColor(value[0], value[1], value[2], 1.0))
    library.update_material_instance(instance)
    assets.save_loaded_asset(instance, False)
    created.append(path)
    return instance


def water_look(deep, shallow, murk, emissive, wobble, speed, refraction, fresnel, foam, wetness):
    return {"colors": {"DeepColor": deep, "ShallowColor": shallow},
            "scalars": {"Murkiness": murk, "EmissiveStrength": emissive, "WobbleStrength": wobble, "WobbleSpeed": speed,
                        "Refraction": refraction, "FresnelPower": fresnel, "FoamAmount": foam, "Wetness": wetness}}


def build_instances(water, particle):
    looks = {
        # Clear, glowing in the blue of Geist
        "MI_Water_Spell_Mage": water_look((0.02, 0.12, 0.35), (0.25, 0.65, 1.0), 0.1, 1.2, 5.0, 0.8, 0.25, 3.0, 0.15, 1.0),
        # Still, dark, cold greenish, murky
        "MI_Water_Pond": water_look((0.005, 0.02, 0.018), (0.06, 0.14, 0.11), 0.75, 0.0, 0.5, 0.2, 0.1, 5.0, 0.05, 0.95),
        # Moving, a little lighter, foam at stones
        "MI_Water_Stream": water_look((0.02, 0.06, 0.06), (0.2, 0.35, 0.33), 0.4, 0.0, 1.5, 1.5, 0.15, 4.0, 0.5, 1.0),
        # Thin, nearly clear drops
        "MI_Water_Rain": water_look((0.3, 0.35, 0.4), (0.7, 0.78, 0.85), 0.05, 0.05, 0.5, 2.0, 0.3, 2.0, 0.0, 1.0),
        # Black-violet, thick, hardly any light
        "MI_Water_Mark": water_look((0.01, 0.0, 0.015), (0.12, 0.02, 0.18), 0.95, 0.08, 3.0, 0.15, 0.05, 6.0, 0.05, 0.7),
        # Murky green, hardly glowing
        "MI_Water_Swamp": water_look((0.01, 0.025, 0.005), (0.12, 0.18, 0.05), 0.85, 0.02, 0.8, 0.25, 0.05, 5.0, 0.15, 0.8),
        # Deep, black-green and cold, a faint film of ash, smooth but reflecting little; never glows like spell water
        "MI_Water_Well": water_look((0.003, 0.01, 0.009), (0.015, 0.04, 0.035), 0.92, 0.0, 0.0, 0.1, 0.08, 5.0, 0.04, 1.0),
    }
    instances = {}
    # The well water lies still in a narrow shaft: hardly any waves, a short soft edge against the stones
    looks["MI_Water_Well"]["scalars"].update({"WaveStrength": 0.15, "WaveTiling": 3.5, "WaveSpeed": 0.02, "Sparkle": 0.1, "ContactFade": 8.0, "SpecularScale": 0.15})
    for name, look in looks.items():
        instances[name] = make_instance(water, name, look["scalars"], look["colors"])

    # Clear drop with a bright glint; bubble ring, nearly see-through inside; soft cloudy mist at about 30 %
    make_instance(particle, "MI_Water_Particle_Drop", {"EdgeSoftness": 0.2, "InnerOpacity": 0.85, "RingAmount": 0.5, "RingRadius": 0.82, "RingWidth": 0.18,
                                                       "HighlightStrength": 1.2, "HighlightSize": 0.32, "Opacity": 0.9, "Brightness": 1.4})
    make_instance(particle, "MI_Water_Particle_Bubble", {"EdgeSoftness": 0.15, "InnerOpacity": 0.06, "RingAmount": 1.0, "RingRadius": 0.8, "RingWidth": 0.14,
                                                         "HighlightStrength": 0.8, "HighlightSize": 0.22, "Opacity": 0.85, "Brightness": 1.6})
    make_instance(particle, "MI_Water_Particle_Mist", {"EdgeSoftness": 1.0, "InnerOpacity": 1.0, "RingAmount": 0.0, "HighlightStrength": 0.0,
                                                       "Cloudiness": 0.7, "CloudScale": 2.5, "Opacity": 0.3, "Brightness": 1.0, "FadeInEnd": 0.15})
    return instances


# ---------------------------------------------------------------- Sources and orb

def make_source(name, label, material, glow, particle, viscosity):
    path = "{}/{}".format(SOURCES, name)
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.VaelElementSource)
    source = tools.create_asset(name, SOURCES, unreal.VaelElementSource, factory)
    source.set_editor_property("source_name", unreal.Text(label))
    source.set_editor_property("element", unreal.VaelElement.WATER)
    source.set_editor_property("material", material)
    source.set_editor_property("glow_color", unreal.LinearColor(*glow))
    source.set_editor_property("particle_color", unreal.LinearColor(*particle))
    source.set_editor_property("viscosity", viscosity)
    assets.save_loaded_asset(source, False)
    created.append(path)
    return source


def build_orb(mage_source):
    path = "{}/BP_WaterOrb".format(BLUEPRINTS)
    if assets.does_asset_exist(path):
        return
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.VaelWaterOrb)
    blueprint = tools.create_asset("BP_WaterOrb", BLUEPRINTS, unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    defaults = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(blueprint))
    defaults.set_editor_property("source", mage_source)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assets.save_loaded_asset(blueprint, False)
    created.append(path)


def main():
    for folder in (MATERIALS, ROOT + "/Niagara", SOURCES, BLUEPRINTS):
        assets.make_directory(folder)

    water = build_water_material()
    build_water_material("M_Water_Puddle", puddle=True)
    particle = build_particle_material()
    instances = build_instances(water, particle)

    # Waves, the drawn strand and their spray: dark blue foot, turquoise body, pale crest, white foam; glows a little as spell water does
    wave = build_wave_material()
    make_instance(wave, "MI_Water_Wave_Mage", WAVE_MAGE_SCALARS, WAVE_MAGE_COLORS)
    make_instance(wave, "MI_Water_Wave_Mark", WAVE_MARK_SCALARS, WAVE_MARK_COLORS)

    mage = make_source("DA_ElementSource_Water_Mage", "Zauberwasser (Magier)", instances["MI_Water_Spell_Mage"], (0.15, 0.55, 1.0, 1.0), (0.45, 0.8, 1.0, 0.8), 0.0)
    make_source("DA_ElementSource_Water_Mark", "Mark-Wasser", instances["MI_Water_Mark"], (0.25, 0.03, 0.35, 1.0), (0.18, 0.03, 0.25, 0.9), 0.85)
    make_source("DA_ElementSource_Water_Swamp", "Sumpfwasser", instances["MI_Water_Swamp"], (0.08, 0.12, 0.03, 1.0), (0.25, 0.32, 0.12, 0.75), 0.4)
    make_source("DA_ElementSource_Water_Rain", "Regenwasser", instances["MI_Water_Rain"], (0.2, 0.26, 0.32, 1.0), (0.6, 0.68, 0.75, 0.6), 0.0)
    make_source("DA_ElementSource_Water_Well", "Brunnenwasser", instances["MI_Water_Well"], (0.05, 0.12, 0.12, 1.0), (0.15, 0.25, 0.25, 0.8), 0.1)

    build_orb(mage)
    unreal.log("Vael water kit: created {} assets: {}".format(len(created), ", ".join(path.split("/")[-1] for path in created)))


main()
