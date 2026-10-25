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

KIT_VERSION = "1.6"
FOLDER = "/Game/Vael/Effects/Nebel"

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
    drift = g.scalar("Treiben", 9.0)
    wisps = g.scalar("Schwaden", 0.35)
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
    for scale, way, column in ((1.0, (1.0, 0.6, 0.15), 5), (2.6, (-1.3, 0.9, 0.25), 5)):
        moved = g.add(5, g.mul(5, world, g.mul(5, size, g.const(scale))), g.mul(5, flow, g.vec3(way[0], way[1], way[2])))
        billows.append(g.noise(4, g.mul(4, moved, g.const(1.0)), levels=4))
    mixed = g.add(3, g.mul(3, billows[0], g.const(0.65)), g.mul(3, billows[1], g.const(0.35)))
    # More contrast: clear billows with hollows between them, not an even haze
    cloud = g.saturate(3, g.mul(3, g.sub(3, mixed, g.const(0.3)), g.const(2.0)))

    # Wisps: the same noise stretched long across the ground, so they draw thin drifting veils
    stretched = g.mul(5, world, g.mul(5, size, g.vec3(0.5, 1.8, 2.5)))
    wisp_noise = g.noise(4, g.add(4, stretched, g.mul(4, flow, g.vec3(0.7, 0.2, 0.1))), levels=3)

    # The blanket: dense where the vertex colour says so, its billows tearing its edge; wisps faint and streaky above it
    ground = g.saturate(2, g.mul(2, dense, g.sub(2, g.mul(2, cloud, g.const(1.8)), g.const(0.2))))
    veils = g.mul(2, g.saturate(2, g.mul(2, dense, g.sub(2, g.mul(2, wisp_noise, g.const(1.7)), g.const(0.65)))), wisps)
    layered = g.lerp(1, ground, veils, wisp)

    # A ball of steam: thick at the rim where the eye looks through much of it, thin in the middle
    rim = g.node(unreal.MaterialExpressionFresnel, 3, exponent=1.5)
    # (the very edge thins out again, so the ball has no hard outline like a bubble of glass)
    edge_soft = g.saturate(2, g.mul(2, g.sub(2, g.const(1.0), g.op(unreal.MaterialExpressionPower, 2, rim, a_input="Base", const_exponent=4.0)), g.const(2.5)))
    shell = g.saturate(2, g.mul(2, g.mul(2, g.add(2, g.const(0.5), g.mul(2, rim, g.const(0.8))), edge_soft), g.sub(2, g.mul(2, cloud, g.const(1.5)), g.const(0.1))))
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
    shade = g.lerp(1, g.const(0.78), g.const(1.05), cloud)
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
    sand = g.color("Farbe", unreal.LinearColor(0.42, 0.3, 0.18, 1.0))
    dark = g.color("Schatten", unreal.LinearColor(0.16, 0.11, 0.07, 1.0))
    spin = g.scalar("Wirbeln", 1.6)
    rise = g.scalar("Steigen", 0.5)
    streaks = g.scalar("Streifen", 3.0)
    density = g.scalar("Dichte", 0.85)
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
    racing = g.mul(5, g.sub(5, around, g.mul(5, time, speed)), streaks)
    climbing = g.mul(5, g.sub(5, up, g.mul(5, time, rise)), g.const(9.0))
    position = g.append(4, g.append(4, racing, climbing), g.mul(4, time, g.const(0.4)))
    coarse = g.noise(3, position, levels=4)
    fine = g.noise(3, g.mul(3, position, g.vec3(3.0, 2.5, 1.0)), levels=3)
    grain = g.add(2, g.mul(2, coarse, g.const(0.7)), g.mul(2, fine, g.const(0.3)))

    # Torn bands: only the dense parts of the noise hold sand, the rest is air
    bands = g.saturate(1, g.mul(1, g.sub(1, grain, g.const(0.38)), g.const(2.6)))
    soft = g.node(unreal.MaterialExpressionDepthFade, 1)
    try_set(soft, "fade_distance_default", 40.0)
    g.link(contact, soft, "FadeDistance")
    opacity = g.saturate(0, g.mul(0, g.mul(0, g.mul(0, bands, solid), density), soft))
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    colour = g.lerp(0, dark, sand, grain)
    library.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(g.const(1.0, column=0), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(g.const(0.0, column=0), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(material, "M_Vael_Sandwirbel")


def build_instances():
    """MI_Vael_Dampfkugel: the mist as a ball of steam"""
    path = FOLDER + "/MI_Vael_Dampfkugel"
    if assets.does_asset_exist(path):
        return
    factory = unreal.MaterialInstanceConstantFactoryNew()
    instance = tools.create_asset("MI_Vael_Dampfkugel", FOLDER, unreal.MaterialInstanceConstant, factory)
    instance.set_editor_property("parent", unreal.load_asset(FOLDER + "/M_Vael_Nebel"))
    library.set_material_instance_scalar_parameter_value(instance, "Kugel", 1.0)
    library.set_material_instance_scalar_parameter_value(instance, "Dichte", 0.95)
    assets.save_loaded_asset(instance, False)
    unreal.log("Vael: created {}".format(path))


build_mist()
build_sand()
build_instances()
