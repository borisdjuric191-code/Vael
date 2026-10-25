# Creates the mist and sand kit, built like the water kit (CreateWaterKit.py): only Unreal, no pack.
#
# - M_Vael_Nebel: the white cloudy substance of the mist veil (Nebelschleier) and of the steam cloud (Dampfwolke).
#   On the meshes the game builds, the vertex colour says what each point is: R how dense (0 at the creeping edge),
#   G 1 for the see-through wisps above the blanket. With the parameter Kugel = 1 it is a ball of steam instead:
#   dense at its rim, thinner in the middle, so only the silhouette of who stands inside shows (MI_Vael_Dampfkugel).
# - M_Vael_Sandwirbel: fast whirling sand on the funnel and storm shells the game builds (Staubteufel, Sandsturm).
#   UVs: U around the whirl (in turns), V up it (0..1); vertex colour A how dense, R which shell (inner ones whirl faster).
#
# Graphs are rebuilt when KIT_VERSION is newer than the one stored on the material; parameter names stay, instances stay valid.
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateMistSandKit.py"

import unreal

KIT_VERSION = "2.7"
FOLDER = "/Game/Vael/Effects/Nebel"

# The rolling dust cloud of the NiagaraExamples dirt explosion: an 8x8 looping flipbook (R light, G opacity)
DUST_ATLAS = "/Game/NiagaraExamples/Textures/Sprites/T_ExplosionRoil_EOO_Loop"
DUST_NORMALS = "/Game/NiagaraExamples/Textures/Sprites/T_ExplosionRoil_Normals_Loop"

library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary


class Graph:
    """Places material expressions in columns, so the graph stays readable when Boris opens it (as in the water kit)"""

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

    def scalar(self, name, default, column=8):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=default)

    def color(self, name, default, column=8):
        return self.node(unreal.MaterialExpressionVectorParameter, column, parameter_name=name, default_value=default)

    def const(self, value, column=7):
        return self.node(unreal.MaterialExpressionConstant, column, r=value)

    def vec3(self, r, g, b, column=7):
        return self.node(unreal.MaterialExpressionConstant3Vector, column, constant=unreal.LinearColor(r, g, b, 1.0))

    def link(self, source, target, target_input, source_output=""):
        if not library.connect_material_expressions(source, source_output, target, target_input):
            unreal.log_error("Vael mist kit: could not connect {} -> {}.{}".format(source.get_name(), target.get_name(), target_input))

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

    def lerp(self, column, a, b, alpha):
        expression = self.op(unreal.MaterialExpressionLinearInterpolate, column, a, b)
        self.link(alpha, expression, "Alpha")
        return expression

    def saturate(self, column, a):
        return self.op(unreal.MaterialExpressionSaturate, column, a, a_input="")

    def append(self, column, a, b):
        return self.op(unreal.MaterialExpressionAppendVector, column, a, b)

    def channel(self, column, source, name):
        """One channel of a vector, through a multiply by 1 so the other helpers can take it"""
        single = self.node(unreal.MaterialExpressionMultiply, column, const_b=1.0)
        self.link(source, single, "A", name)
        return single

    def noise(self, column, position, levels, low=0.0, high=1.0):
        expression = self.node(unreal.MaterialExpressionNoise, column, scale=1.0, levels=levels, turbulence=False,
                               noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX, output_min=low, output_max=high)
        # (the input must be linked by "": by its name "Position" the link fails)
        self.link(position, expression, "")
        return expression


def try_set(target, key, value):
    try:
        target.set_editor_property(key, value)
        return True
    except Exception:
        return False


def fresh_material(name):
    """The material to build: new, or an older one of this kit with its graph cleared; None if it is up to date"""
    path = "{}/{}".format(FOLDER, name)
    material = unreal.load_asset(path)
    if material is not None:
        if assets.get_metadata_tag(material, "VaelKit") == KIT_VERSION:
            unreal.log("Vael: {} is up to date".format(path))
            return None
        library.delete_all_material_expressions(material)
        return material
    return tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def finish(material, name):
    library.recompile_material(material)
    assets.set_metadata_tag(material, "VaelKit", KIT_VERSION)
    if assets.save_loaded_asset(material, False):
        unreal.log("Vael: built {}/{} ({})".format(FOLDER, name, KIT_VERSION))
    else:
        unreal.log_error("Vael: {} could not be saved".format(name))


def build_mist():
    material = fresh_material("M_Vael_Nebel")
    if material is None:
        return
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("two_sided", True)
    try_set(material, "translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)

    g = Graph(material)
    tint = g.color("Farbe", unreal.LinearColor(0.9, 0.92, 0.95, 1.0))
    density = g.scalar("Dichte", 1.0)
    size = g.scalar("Groesse", 0.0045)
    drift = g.scalar("Treiben", 1.6)
    wisps = g.scalar("Schwaden", 0.45)
    ball = g.scalar("Kugel", 0.0)
    glow = g.scalar("Eigenlicht", 0.18)
    contact = g.scalar("Kontakt", 35.0)

    world = g.node(unreal.MaterialExpressionWorldPosition, 7)
    time = g.node(unreal.MaterialExpressionTime, 7)
    vertex = g.node(unreal.MaterialExpressionVertexColor, 7)
    dense = g.channel(6, vertex, "R")
    wisp = g.channel(6, vertex, "G")

    # Billows: two layers of soft noise drifting slowly through the world at different sizes and ways
    flow = g.mul(6, time, drift)
    billows = []
    for scale, way, column in ((1.0, (1.0, 0.6, 0.1), 5), (2.6, (0.7, 0.4, 0.18), 5)):
        moved = g.add(5, g.mul(5, world, g.mul(5, size, g.const(scale))), g.mul(5, flow, g.vec3(way[0], way[1], way[2])))
        billows.append(g.noise(4, g.mul(4, moved, g.const(1.0)), levels=4))
    mixed = g.add(3, g.mul(3, billows[0], g.const(0.65)), g.mul(3, billows[1], g.const(0.35)))
    # More contrast: clear billows with hollows between them, not an even haze
    cloud = g.saturate(3, g.mul(3, g.sub(3, mixed, g.const(0.3)), g.const(2.0)))

    # Wisps: the same noise stretched long across the ground, so they draw thin drifting veils
    stretched = g.mul(5, world, g.mul(5, size, g.vec3(0.5, 1.8, 2.5)))
    wisp_noise = g.noise(4, g.add(4, stretched, g.mul(4, flow, g.vec3(0.7, 0.2, 0.1))), levels=3)

    # The blanket: dense where the vertex colour says so, its billows tearing its edge; wisps faint and streaky above it
    ground = g.saturate(2, g.mul(2, dense, g.add(2, g.mul(2, cloud, g.const(1.4)), g.const(0.35))))
    veils = g.mul(2, g.saturate(2, g.mul(2, dense, g.sub(2, g.mul(2, wisp_noise, g.const(1.4)), g.const(0.25)))), wisps)
    layered = g.lerp(1, ground, veils, wisp)

    # A ball of steam: thick at the rim where the eye looks through much of it, thin in the middle
    rim = g.node(unreal.MaterialExpressionFresnel, 3, exponent=1.5)
    # (the very edge thins out again, so the ball has no hard outline like a bubble of glass)
    edge_soft = g.saturate(2, g.mul(2, g.sub(2, g.const(1.0), g.op(unreal.MaterialExpressionPower, 2, rim, a_input="Base", const_exponent=4.0)), g.const(2.5)))
    shell = g.saturate(2, g.mul(2, g.mul(2, g.add(2, g.const(0.45), g.mul(2, rim, g.const(0.9))), edge_soft), g.sub(2, g.mul(2, cloud, g.const(1.5)), g.const(0.1))))
    shape = g.lerp(1, layered, shell, ball)

    # A ball bulges into billows: its surface pushed out and in along its normal by slow noise, so its outline is a cloud, not a sphere
    bulges = g.scalar("Beulen", 90.0)
    lumps = g.noise(3, g.add(3, g.mul(3, world, g.const(0.004)), g.mul(3, time, g.vec3(0.05, 0.03, 0.12))), levels=2)
    push = g.mul(1, g.mul(1, g.node(unreal.MaterialExpressionVertexNormalWS, 2), g.mul(2, g.sub(2, lumps, g.const(0.4)), bulges)), ball)
    library.connect_material_property(push, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    soft = g.node(unreal.MaterialExpressionDepthFade, 1)
    try_set(soft, "fade_distance_default", 35.0)
    g.link(contact, soft, "FadeDistance")
    opacity = g.saturate(0, g.mul(0, g.mul(0, shape, density), soft))
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    # White, a little darker in the hollows between the billows, with a faint light of its own so it stays white in shade
    shade = g.lerp(1, g.const(0.78), g.const(1.08), cloud)
    colour = g.mul(0, tint, shade)
    library.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(g.mul(0, tint, glow), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(g.const(1.0, column=0), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(g.const(0.0, column=0), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(material, "M_Vael_Nebel")


def build_sand():
    material = fresh_material("M_Vael_Sandwirbel")
    if material is None:
        return
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("two_sided", True)
    try_set(material, "translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)

    g = Graph(material)
    sand = g.color("Farbe", unreal.LinearColor(0.55, 0.36, 0.17, 1.0))
    dark = g.color("Schatten", unreal.LinearColor(0.16, 0.11, 0.07, 1.0))
    spin = g.scalar("Wirbeln", 1.6)
    rise = g.scalar("Steigen", 0.5)
    streaks = g.scalar("Streifen", 3.0)
    density = g.scalar("Dichte", 0.75)
    contact = g.scalar("Kontakt", 40.0)

    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 7)
    time = g.node(unreal.MaterialExpressionTime, 7)
    vertex = g.node(unreal.MaterialExpressionVertexColor, 7)
    solid = g.channel(6, vertex, "A")
    inner = g.channel(6, vertex, "R")
    # (texture coordinates have no channel outputs: masks pick U and V)
    around = g.node(unreal.MaterialExpressionComponentMask, 6, r=True, g=False, b=False, a=False)
    g.link(uv, around, "")
    up = g.node(unreal.MaterialExpressionComponentMask, 6, r=False, g=True, b=False, a=False)
    g.link(uv, up, "")

    # Sand races round (U) and climbs (V); inner shells race faster. Streaks are long around and short up: bands of blown sand
    speed = g.mul(5, spin, g.add(5, g.const(1.0), g.mul(5, inner, g.const(0.8))))
    # The streaks wind up the whirl like a screw (Verdrehung), and race fastest low down where the whirl is tight
    twist = g.scalar("Verdrehung", 0.7)
    tight = g.sub(5, g.const(1.5), g.mul(5, up, g.const(0.7)))
    wound = g.add(5, around, g.mul(5, up, twist))
    racing = g.mul(5, g.sub(5, wound, g.mul(5, time, g.mul(5, speed, tight))), streaks)
    climbing = g.mul(5, g.sub(5, up, g.mul(5, time, rise)), g.const(9.0))
    position = g.append(4, g.append(4, racing, climbing), g.mul(4, time, g.const(0.4)))
    coarse = g.noise(3, position, levels=4)
    fine = g.noise(3, g.mul(3, position, g.vec3(3.0, 2.5, 1.0)), levels=3)
    grain = g.add(2, g.mul(2, coarse, g.const(0.7)), g.mul(2, fine, g.const(0.3)))

    # Torn bands: only the dense parts of the noise hold sand, the rest is air
    bands = g.saturate(1, g.mul(1, g.sub(1, grain, g.const(0.38)), g.const(2.6)))

    # Billows of dust: the rolling dust cloud of the NiagaraExamples dirt explosion (an 8x8 looping flipbook) laid on the
    # shells in tiles that race round with the sand; each tile plays from its own frame, so they don't billow in step
    puffs_around = g.scalar("StaubRund", 5.0)
    puffs_up = g.scalar("StaubHoch", 3.0)
    puff_rate = g.scalar("StaubTempo", 18.0)
    tile_u = g.mul(5, g.sub(5, wound, g.mul(5, time, g.mul(5, speed, tight))), puffs_around)
    tile_v = g.mul(5, g.sub(5, up, g.mul(5, time, rise)), puffs_up)
    tiles = g.append(4, tile_u, tile_v)
    cell = g.op(unreal.MaterialExpressionFloor, 4, tiles, a_input="")
    inside = g.op(unreal.MaterialExpressionFrac, 4, tiles, a_input="")
    # (a floor has no channel outputs: masks pick its two numbers)
    cell_u = g.node(unreal.MaterialExpressionComponentMask, 4, r=True, g=False, b=False, a=False)
    g.link(cell, cell_u, "")
    cell_v = g.node(unreal.MaterialExpressionComponentMask, 4, r=False, g=True, b=False, a=False)
    g.link(cell, cell_v, "")
    seed = g.add(4, g.mul(4, cell_u, g.const(7.0)), g.mul(4, cell_v, g.const(13.0)))
    frame_raw = g.add(3, g.mul(3, time, puff_rate), seed)
    frame = g.op(unreal.MaterialExpressionFmod, 3, g.op(unreal.MaterialExpressionFloor, 3, frame_raw, a_input=""), g.const(64.0))
    column_index = g.op(unreal.MaterialExpressionFmod, 3, frame, g.const(8.0))
    row_index = g.op(unreal.MaterialExpressionFloor, 3, g.mul(3, frame, g.const(0.125)), a_input="")
    atlas_uv = g.mul(2, g.add(2, inside, g.append(2, column_index, row_index)), g.const(0.125))
    dust = g.node(unreal.MaterialExpressionTextureSample, 1, texture=unreal.load_asset(DUST_ATLAS))
    g.link(atlas_uv, dust, "UVs")
    dust_alpha = g.channel(1, dust, "A")
    dust_light = g.channel(1, dust, "R")
    billows = g.mul(1, dust_alpha, g.scalar("StaubDeckkraft", 1.1))

    soft = g.node(unreal.MaterialExpressionDepthFade, 1)
    try_set(soft, "fade_distance_default", 40.0)
    g.link(contact, soft, "FadeDistance")
    body = g.saturate(1, g.add(1, g.mul(1, bands, g.const(0.45)), billows))
    opacity = g.saturate(0, g.mul(0, g.mul(0, g.mul(0, body, solid), density), soft))
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    colour = g.lerp(0, dark, sand, g.saturate(0, g.add(0, g.mul(0, grain, g.const(0.5)), g.mul(0, dust_light, g.const(0.7)))))
    library.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(g.const(1.0, column=0), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(g.const(0.0, column=0), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(material, "M_Vael_Sandwirbel")


def build_instances():
    """MI_Vael_Dampfkugel: the mist as a ball of steam"""
    path = FOLDER + "/MI_Vael_Dampfkugel"
    instance = unreal.load_asset(path)
    if instance is None:
        factory = unreal.MaterialInstanceConstantFactoryNew()
        instance = tools.create_asset("MI_Vael_Dampfkugel", FOLDER, unreal.MaterialInstanceConstant, factory)
        instance.set_editor_property("parent", unreal.load_asset(FOLDER + "/M_Vael_Nebel"))
    elif assets.get_metadata_tag(instance, "VaelKit") == KIT_VERSION:
        return
    # Thick and dense, slowly churning
    library.set_material_instance_scalar_parameter_value(instance, "Kugel", 1.0)
    library.set_material_instance_scalar_parameter_value(instance, "Dichte", 1.05)
    library.set_material_instance_scalar_parameter_value(instance, "Treiben", 1.0)
    assets.set_metadata_tag(instance, "VaelKit", KIT_VERSION)
    assets.save_loaded_asset(instance, False)
    unreal.log("Vael: built {}".format(path))


def build_cards():
    """M_Vael_Wolkenkarte: one soft card of a cloud for the cloud cards the game lays out (UVaelCloudCardsComponent):
    the rolling cloud of the NiagaraExamples dirt explosion, lit by its own normals, any colour.
    Per card (instance custom data): 0 the frame it starts on, 1 how solid it is, 2 how bright"""
    material = fresh_material("M_Vael_Wolkenkarte")
    if material is None:
        return
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("two_sided", True)
    try_set(material, "translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    try_set(material, "used_with_instanced_static_meshes", True)

    g = Graph(material)
    tint = g.color("Farbe", unreal.LinearColor(0.92, 0.93, 0.95, 1.0))
    shadow = g.scalar("Schatten", 0.55)
    glow = g.scalar("Eigenlicht", 0.2)
    density = g.scalar("Dichte", 1.0)
    rate = g.scalar("Tempo", 10.0)
    contact = g.scalar("Kontakt", 60.0)

    def custom(index):
        return g.node(unreal.MaterialExpressionPerInstanceCustomData, 7, data_index=index, const_default_value=[0.0, 1.0, 1.0][index])

    start = custom(0)
    solid = custom(1)
    bright = custom(2)
    time = g.node(unreal.MaterialExpressionTime, 7)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 7)

    # The frame this card shows now, and where it lies in the 8x8 atlas
    frame = g.op(unreal.MaterialExpressionFmod, 5, g.op(unreal.MaterialExpressionFloor, 5, g.add(5, g.mul(5, time, rate), start), a_input=""), g.const(64.0))
    column_index = g.op(unreal.MaterialExpressionFmod, 4, frame, g.const(8.0))
    row_index = g.op(unreal.MaterialExpressionFloor, 4, g.mul(4, frame, g.const(0.125)), a_input="")
    atlas_uv = g.mul(3, g.add(3, uv, g.append(3, column_index, row_index)), g.const(0.125))

    roil = g.node(unreal.MaterialExpressionTextureSample, 2, texture=unreal.load_asset(DUST_ATLAS))
    g.link(atlas_uv, roil, "UVs")
    normals = g.node(unreal.MaterialExpressionTextureSample, 2, texture=unreal.load_asset(DUST_NORMALS))
    try_set(normals, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    g.link(atlas_uv, normals, "UVs")
    light = g.channel(1, roil, "R")
    alpha = g.channel(1, roil, "A")

    soft = g.node(unreal.MaterialExpressionDepthFade, 1)
    try_set(soft, "fade_distance_default", 60.0)
    g.link(contact, soft, "FadeDistance")
    # Cards close to the camera fade out, so a cloud the camera looks through never veils the whole picture
    # (as the engine's camera depth fade: 0 up to 250 cm from the camera, full 350 cm further)
    depth = g.node(unreal.MaterialExpressionPixelDepth, 2)
    near = g.saturate(1, g.mul(1, g.sub(1, depth, g.const(250.0)), g.const(1.0 / 350.0)))
    opacity = g.saturate(0, g.mul(0, g.mul(0, g.mul(0, g.mul(0, alpha, solid), density), soft), near))
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    lifted = g.saturate(1, g.mul(1, light, g.const(1.7)))
    shaded = g.mul(0, g.mul(0, tint, g.lerp(1, shadow, g.const(1.0), lifted)), bright)
    library.connect_material_property(shaded, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(g.mul(0, g.mul(0, tint, glow), g.add(0, g.const(0.4), lifted)), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(normals, "RGB", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(g.const(1.0, column=0), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(g.const(0.0, column=0), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(material, "M_Vael_Wolkenkarte")


def build_card_instances():
    """White cloud for mist and steam, sand for the whirls, dark storm cloud for the hail"""
    for name, colour, glow, shadow in (("MI_Vael_Wolke_Weiss", (0.95, 0.96, 0.98), 0.55, 0.72),
                                       ("MI_Vael_Wolke_Sand", (0.72, 0.54, 0.34), 0.2, 0.6),
                                       ("MI_Vael_Wolke_Sturm", (0.3, 0.33, 0.4), 0.06, 0.35)):
        path = "{}/{}".format(FOLDER, name)
        instance = unreal.load_asset(path)
        if instance is None:
            instance = tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            instance.set_editor_property("parent", unreal.load_asset(FOLDER + "/M_Vael_Wolkenkarte"))
        elif assets.get_metadata_tag(instance, "VaelKit") == KIT_VERSION:
            continue
        library.set_material_instance_vector_parameter_value(instance, "Farbe", unreal.LinearColor(colour[0], colour[1], colour[2], 1.0))
        library.set_material_instance_scalar_parameter_value(instance, "Eigenlicht", glow)
        library.set_material_instance_scalar_parameter_value(instance, "Schatten", shadow)
        assets.set_metadata_tag(instance, "VaelKit", KIT_VERSION)
        assets.save_loaded_asset(instance, False)
        unreal.log("Vael: built {}".format(path))


def build_ice():
    """M_Vael_Hageleis: the lances of ice of the Hagelsturm (instanced): pale, glowing a little, bright at the rim"""
    material = fresh_material("M_Vael_Hageleis")
    if material is None:
        return
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    try_set(material, "used_with_instanced_static_meshes", True)
    g = Graph(material)
    tint = g.color("Farbe", unreal.LinearColor(0.72, 0.88, 1.0, 1.0))
    glow = g.scalar("Leuchten", 1.4)
    rim = g.node(unreal.MaterialExpressionFresnel, 2, exponent=2.5)
    colour = g.mul(0, g.mul(1, tint, glow), g.add(1, g.const(0.7), g.mul(1, rim, g.const(1.6))))
    library.connect_material_property(colour, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(material, "M_Vael_Hageleis")


build_ice()
build_mist()
build_sand()
build_instances()
build_cards()
build_card_instances()
