# Feuer-Baukasten: the materials every fire in Vael is made of (world fires, campfires, torches, burning enemies, fire spells).
# No effects of other packs (Boris 2026-10-22): flames, sparks and scorch marks are drawn by these materials on cards that
# UVaelFireComponent builds in code.
#
# - T_Feuer_Rauschen: tiling noise (Tools/Feuer/feuer_texturen.py), R coarse billows, G tongues, B flicker, A tall streaks.
# - M_Feuer: one flame tongue per card (UV: u across, v from the base up). Two noises pan upwards and bend the tongue, so it
#   wavers, licks and snakes; heat runs from a white-yellow core over orange to a deep red-orange edge, the tip turns into
#   dark smoke. Vertex colour R = phase of the tongue, G = its heat. Parameters: Staerke (fade in/out), Hitze, Tempo,
#   Dunkel (smoke), Leuchtkraft, the four colours.
# - M_Feuer_Funken: sparks on tiny cards that rise by World Position Offset, sway and fade, each in its own time
#   (vertex colour R phase, G speed, B height share); Hoehe = how high they fly in cm.
# - M_Feuer_Brandfleck: deferred decal of a scorch mark, charred and cracked, its embers glow and die down (Glut 1 -> 0).
# Only creates what is missing; masters are rebuilt when they are older than KIT_VERSION.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateFireKit.py"

import os
import unreal

FOLDER = "/Game/Vael/Effects/Feuer"
DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Data", "Feuer")
KIT_VERSION = 4.4

library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
made = []
notes = []


def log(message):
    unreal.log_warning("[CreateFireKit] " + message)


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

    def vector(self, name, rgb, column):
        parameter = self.node(unreal.MaterialExpressionVectorParameter, column, parameter_name=name,
                              default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return self.mask(parameter, column - 1)

    def op(self, expression_class, column, a, b):
        expression = self.node(expression_class, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        return expression

    def one(self, expression_class, column, a, **properties):
        expression = self.node(expression_class, column, **properties)
        self.link(a, expression)
        return expression

    def mul(self, c, a, b): return self.op(unreal.MaterialExpressionMultiply, c, a, b)
    def add(self, c, a, b): return self.op(unreal.MaterialExpressionAdd, c, a, b)
    def sub(self, c, a, b): return self.op(unreal.MaterialExpressionSubtract, c, a, b)
    def max(self, c, a, b): return self.op(unreal.MaterialExpressionMax, c, a, b)
    def sat(self, c, a): return self.one(unreal.MaterialExpressionSaturate, c, a)
    def abs(self, c, a): return self.one(unreal.MaterialExpressionAbs, c, a)
    def frac(self, c, a): return self.one(unreal.MaterialExpressionFrac, c, a)
    def sin(self, c, a): return self.one(unreal.MaterialExpressionSine, c, a, period=6.283185)
    def cos(self, c, a): return self.one(unreal.MaterialExpressionCosine, c, a, period=6.283185)

    def power(self, c, a, e):
        expression = self.node(unreal.MaterialExpressionPower, c)
        self.link(a, expression, "Base")
        self.link(e, expression, "Exp")
        return expression

    def mask(self, source, c, r=True, g=True, b=True, a=False):
        expression = self.node(unreal.MaterialExpressionComponentMask, c, r=r, g=g, b=b, a=a)
        self.link(source, expression)
        return expression

    def lerp(self, c, a, b, alpha):
        expression = self.node(unreal.MaterialExpressionLinearInterpolate, c)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        self.link(alpha, expression, "Alpha")
        return expression

    def append(self, c, a, b):
        return self.op(unreal.MaterialExpressionAppendVector, c, a, b)

    def ramp(self, c, x, low, high):
        """Linear step from 0 at low to 1 at high"""
        return self.sat(c, self.mul(c + 1, self.sub(c + 2, x, self.const(low, c + 3)), self.const(1.0 / (high - low), c + 2)))

    def sample(self, texture, uv, c, channel):
        expression = self.node(unreal.MaterialExpressionTextureSample, c, texture=texture, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        self.link(uv, expression, "UVs")
        if channel == "A":
            # Alpha has its own output on the sample; the RGB output has no fourth channel to mask
            single = self.node(unreal.MaterialExpressionMultiply, c - 1, const_b=1.0)
            self.link(expression, single, "A", "A")
            return single
        return self.mask(expression, c - 1, r=channel == "R", g=channel == "G", b=channel == "B")


def import_noise():
    path = FOLDER + "/T_Feuer_Rauschen"
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(DATA, "T_Feuer_Rauschen.png"))
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", "T_Feuer_Rauschen")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(path)
    if texture is not None:
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        assets.save_loaded_asset(texture, False)
        made.append(path)
    return texture


def new_material(name):
    path = "{}/{}".format(FOLDER, name)
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        if "KitVersion" in [str(p) for p in library.get_scalar_parameter_names(material)]:
            if library.get_material_default_scalar_parameter_value(material, "KitVersion") >= KIT_VERSION - 0.001:
                return None
        library.delete_all_material_expressions(material)
        made.append(path + " (graph rebuilt)")
        return material
    made.append(path)
    return tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def billboard(g, u, v, c):
    """World Position Offset that unfolds a card from its base point towards the camera: all four corners sit on the base,
    UV channel 1 holds width and height. The card turns to face the camera and keeps world up as its up on screen, so
    flames rise on screen even under the high camera of the game."""
    size = g.node(unreal.MaterialExpressionTextureCoordinate, c + 3, coordinate_index=1)
    width = g.mask(size, c + 2, r=True, g=False, b=False)
    height = g.mask(size, c + 2, r=False, g=True, b=False)
    base = g.node(unreal.MaterialExpressionWorldPosition, c + 4)
    camera = g.node(unreal.MaterialExpressionCameraPositionWS, c + 4)
    nudge = g.node(unreal.MaterialExpressionConstant3Vector, c + 4, constant=unreal.LinearColor(0.001, 0.0007, 0.0, 1.0))
    to_camera = g.one(unreal.MaterialExpressionNormalize, c + 2, g.add(c + 3, g.sub(c + 3, camera, base), nudge))
    # The way the flames lick: straight up for a fire on the ground, backwards for a burning projectile
    # NormalFluss 1 lets each card lick along its own normal instead: the flames of a ball of fire point out all around
    along_normal = g.node(unreal.MaterialExpressionVertexNormalWS, c + 5)
    flow = g.lerp(c + 3, g.vector("Richtung", (0.0, 0.0, 1.0), c + 5), along_normal, g.scalar("NormalFluss", 0.0, c + 5))
    up = g.one(unreal.MaterialExpressionNormalize, c + 2, g.add(c + 3, flow, g.node(unreal.MaterialExpressionConstant3Vector, c + 4, constant=unreal.LinearColor(0.0, 0.0, 0.001, 1.0))))
    grow = g.scalar("Groesse", 1.0, c + 4)
    width = g.mul(c + 1, width, grow)
    height = g.mul(c + 1, height, grow)
    right = g.one(unreal.MaterialExpressionNormalize, c + 1, g.op(unreal.MaterialExpressionCrossProduct, c + 2, to_camera, up))
    screen_up = g.op(unreal.MaterialExpressionCrossProduct, c + 1, right, to_camera)
    across = g.mul(c, right, g.mul(c + 1, g.sub(c + 2, u, g.const(0.5, c + 3)), width))
    along = g.mul(c, screen_up, g.mul(c + 1, v, height))
    return g.add(c - 1, across, along)


def scrolling_uv(g, u, v, phase, time, sx, sy, speed, offset, c):
    """(u * sx + phase * offset, v * sy - time * speed): a noise that rises through the tongue"""
    across = g.add(c + 1, g.mul(c + 2, u, g.const(sx, c + 3)), g.mul(c + 2, phase, g.const(offset, c + 3)))
    up = g.sub(c + 1, g.mul(c + 2, v, g.const(sy, c + 3)), g.mul(c + 2, time, g.const(speed, c + 3)))
    return g.append(c, across, up)


def build_flame(noise):
    material = new_material("M_Feuer")
    if material is None:
        return unreal.load_asset(FOLDER + "/M_Feuer")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_static_lighting", False)

    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, 30)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 29)
    u = g.mask(uv, 28, r=True, g=False, b=False)
    v = g.mask(uv, 28, r=False, g=True, b=False)
    vertex = g.node(unreal.MaterialExpressionVertexColor, 29)
    phase = g.mask(vertex, 28, r=True, g=False, b=False)
    tongue_heat = g.mask(vertex, 28, r=False, g=True, b=False)
    time = g.mul(27, g.node(unreal.MaterialExpressionTime, 28), g.scalar("Tempo", 1.2, 28))

    # Two slow noises bend the space the flame is drawn in (domain warping): the tongue wavers, snakes and curls
    warp_a = g.sub(21, g.sample(noise, scrolling_uv(g, u, v, phase, time, 0.5, 0.55, 0.45, 3.7, 23), 22, "R"), g.const(0.5, 22))
    warp_b = g.sub(21, g.sample(noise, scrolling_uv(g, u, v, phase, time, 0.9, 0.9, 0.8, 5.3, 23), 22, "G"), g.const(0.5, 22))
    rising = g.power(20, v, g.const(1.1, 21))
    wu = g.add(18, u, g.add(19, g.mul(20, warp_a, g.mul(21, rising, g.const(0.4, 22))), g.mul(20, warp_b, g.const(0.12, 21))))
    wv = g.add(18, v, g.mul(19, warp_b, g.const(0.1, 20)))

    # Details in the warped space: fine flicker, small licks and tall streaks the tip tears into
    fine = g.sample(noise, scrolling_uv(g, wu, wv, phase, time, 2.2, 2.0, 1.7, 1.9, 15), 14, "B")
    licks = g.sample(noise, scrolling_uv(g, wu, wv, phase, time, 3.5, 3.2, 2.4, 1.1, 15), 14, "G")
    streaks = g.sample(noise, scrolling_uv(g, wu, wv, phase, time, 1.6, 0.6, 1.25, 2.3, 15), 14, "A")
    snake = g.mul(17, g.sin(18, g.add(19, g.add(20, g.mul(21, time, g.const(1.7, 22)), g.mul(21, phase, g.const(6.28, 22))), g.mul(20, v, g.const(3.0, 21)))),
                  g.mul(18, v, g.const(0.05, 19)))
    distance = g.mul(12, g.abs(13, g.sub(14, g.add(15, wu, snake), g.const(0.5, 15))), g.const(2.0, 13))

    # A soft body, broad below and narrow above
    width = g.mul(11, g.power(12, g.sat(13, g.sub(14, g.const(1.0, 15), v)), g.const(0.5, 13)), g.const(0.7, 12))
    shape = g.sat(10, g.mul(11, g.sub(12, width, distance), g.const(2.2, 12)))

    # Heat: hottest low in the core, details eat the edges, and towards the top the flame tears into separate licks
    body = g.mul(9, g.mul(10, shape, g.sub(11, g.const(1.2, 12), v)), g.mul(10, tongue_heat, g.scalar("Hitze", 1.0, 11)))
    detail = g.add(9, g.mul(10, g.sub(11, fine, g.const(0.5, 12)), g.const(0.5, 11)), g.mul(10, g.sub(11, licks, g.const(0.5, 12)), g.const(0.35, 11)))
    tear = g.ramp(9, g.add(10, g.sub(11, streaks, g.mul(12, v, g.const(0.85, 13))), g.const(0.45, 12)), 0.0, 0.3)
    flicker = g.add(9, g.const(0.88, 10), g.mul(10, g.sin(11, g.add(12, g.mul(13, time, g.const(9.0, 14)), g.mul(13, phase, g.const(20.0, 14)))), g.const(0.12, 11)))
    heat = g.sat(6, g.mul(7, g.mul(8, g.sub(9, g.add(10, body, detail), g.mul(10, v, g.const(0.2, 11))), tear), flicker))

    # Colours: deep red at the edges, orange in the body, yellow and nearly white deep in the core
    edge = g.vector("Rand", (0.45, 0.05, 0.01), 8)
    middle_colour = g.vector("Mitte", (1.0, 0.27, 0.02), 8)
    core = g.vector("Kern", (1.0, 0.58, 0.16), 8)
    white = g.vector("Weiss", (1.0, 0.9, 0.62), 8)
    colour = g.lerp(4, g.lerp(5, g.lerp(6, edge, middle_colour, g.ramp(7, heat, 0.15, 0.45)), core, g.ramp(6, heat, 0.62, 0.9)), white, g.ramp(5, heat, 0.96, 1.0))
    glow = g.mul(3, colour, g.add(4, g.mul(5, g.power(6, heat, g.const(1.3, 7)), g.scalar("Leuchtkraft", 1.7, 6)), g.const(0.1, 5)))

    # A little dark smoke at the cool tips; the smoke column itself has its own cards
    smoke = g.mul(5, g.mul(6, shape, g.ramp(7, v, 0.5, 1.0)), g.mul(6, warp_a, g.scalar("Dunkel", 0.6, 7)))
    burning = g.ramp(4, heat, 0.03, 0.35)
    strength = g.scalar("Staerke", 1.0, 3)
    foot = g.ramp(3, v, 0.06, 0.4)
    emissive = g.mul(1, g.lerp(2, g.vector("Rauch", (0.03, 0.02, 0.015), 3), glow, g.ramp(3, heat, 0.0, 0.05)), strength)
    opacity = g.mul(1, g.mul(2, g.sat(3, g.add(4, burning, g.mul(5, g.sat(6, smoke), g.const(0.25, 6)))), foot), strength)

    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    library.connect_material_property(billboard(g, u, v, 36), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def build_smoke(noise):
    """Dark smoke billowing up above a fire, slower than the flames, lit a little orange from below"""
    material = new_material("M_Feuer_Rauch")
    if material is None:
        return unreal.load_asset(FOLDER + "/M_Feuer_Rauch")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, 30)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 29)
    u = g.mask(uv, 28, r=True, g=False, b=False)
    v = g.mask(uv, 28, r=False, g=True, b=False)
    vertex = g.node(unreal.MaterialExpressionVertexColor, 29)
    phase = g.mask(vertex, 28, r=True, g=False, b=False)
    time = g.mul(27, g.node(unreal.MaterialExpressionTime, 28), g.scalar("Tempo", 1.2, 28))

    warp = g.sub(21, g.sample(noise, scrolling_uv(g, u, v, phase, time, 0.5, 0.5, 0.18, 2.9, 23), 22, "R"), g.const(0.5, 22))
    wu = g.add(18, u, g.mul(19, warp, g.mul(20, v, g.const(0.5, 21))))
    billows = g.sample(noise, scrolling_uv(g, wu, v, phase, time, 1.2, 1.0, 0.3, 4.1, 15), 14, "G")
    wisps = g.sample(noise, scrolling_uv(g, wu, v, phase, time, 1.8, 0.6, 0.42, 1.7, 15), 14, "A")

    # A soft column that widens as it rises, fading in above the flames and out at the top
    distance = g.mul(12, g.abs(13, g.sub(14, wu, g.const(0.5, 15))), g.const(2.0, 13))
    width = g.add(11, g.const(0.45, 12), g.mul(12, v, g.const(0.45, 13)))
    column = g.sat(10, g.mul(11, g.sub(12, width, distance), g.const(1.6, 12)))
    fade = g.mul(9, g.ramp(10, v, 0.0, 0.25), g.sat(10, g.sub(11, g.const(1.0, 12), v)))
    density = g.mul(7, g.mul(8, column, fade), g.mul(8, g.add(9, g.mul(10, billows, g.const(0.7, 11)), g.mul(10, wisps, g.const(0.5, 11))), g.scalar("Dunkel", 0.6, 9)))
    lit = g.mul(5, g.vector("Glutlicht", (0.5, 0.12, 0.02), 7), g.sat(6, g.sub(7, g.const(0.35, 8), v)))
    colour = g.add(4, g.vector("Rauch", (0.035, 0.03, 0.026), 6), lit)
    strength = g.scalar("Staerke", 1.0, 3)

    library.connect_material_property(g.mul(2, colour, strength), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(g.mul(2, g.sat(3, g.mul(4, density, g.const(0.8, 5))), strength), "", unreal.MaterialProperty.MP_OPACITY)
    library.connect_material_property(billboard(g, u, v, 36), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def build_sparks():
    material = new_material("M_Feuer_Funken")
    if material is None:
        return unreal.load_asset(FOLDER + "/M_Feuer_Funken")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, 20)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 19)
    vertex = g.node(unreal.MaterialExpressionVertexColor, 19)
    phase = g.mask(vertex, 18, r=True, g=False, b=False)
    speed = g.mask(vertex, 18, r=False, g=True, b=False)
    reach = g.mask(vertex, 18, r=False, g=False, b=True)
    time = g.node(unreal.MaterialExpressionTime, 18)

    # Each spark lives through a loop of its own: rises, sways, fades and starts again at the base
    life = g.frac(15, g.add(16, g.mul(17, time, g.add(18, g.mul(19, speed, g.const(0.9, 19)), g.const(0.35, 19))), phase))
    rise = g.mul(13, g.mul(14, life, reach), g.scalar("Hoehe", 120.0, 15))
    sway = g.mul(13, g.sin(14, g.add(15, g.mul(16, time, g.const(3.1, 17)), g.mul(16, phase, g.const(40.0, 17)))), g.mul(14, life, g.scalar("Schwanken", 14.0, 15)))
    swing = g.mul(13, g.cos(14, g.add(15, g.mul(16, time, g.const(2.3, 17)), g.mul(16, phase, g.const(25.0, 17)))), g.mul(14, life, g.const(9.0, 15)))
    offset = g.append(10, g.append(11, sway, swing), rise)
    spark_u = g.mask(uv, 25, r=True, g=False, b=False)
    spark_v = g.mask(uv, 25, r=False, g=True, b=False)
    offset = g.add(9, offset, billboard(g, spark_u, g.sub(24, spark_v, g.const(0.5, 25)), 26))

    # A soft round dot, white-yellow when young, deep orange when it dies
    centre = g.sub(12, uv, g.node(unreal.MaterialExpressionConstant2Vector, 13, r=0.5, g=0.5))
    dot_product = g.node(unreal.MaterialExpressionDotProduct, 11)
    g.link(centre, dot_product, "A")
    g.link(centre, dot_product, "B")
    round_dot = g.sat(9, g.sub(10, g.const(1.0, 11), g.mul(10, dot_product, g.const(4.0, 11))))
    fade = g.mul(8, g.sat(9, g.mul(10, life, g.const(8.0, 11))), g.sub(9, g.const(1.0, 10), life))
    colour = g.lerp(7, g.vector("Jung", (1.0, 0.85, 0.45), 8), g.vector("Alt", (1.0, 0.25, 0.03), 8), life)
    emissive = g.mul(5, g.mul(6, colour, g.mul(7, round_dot, fade)), g.mul(6, g.scalar("Leuchtkraft", 14.0, 7), g.scalar("Staerke", 1.0, 7)))

    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def build_scorch(noise):
    material = new_material("M_Feuer_Brandfleck")
    if material is None:
        return unreal.load_asset(FOLDER + "/M_Feuer_Brandfleck")
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, 20)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 19)
    centre = g.sub(17, uv, g.node(unreal.MaterialExpressionConstant2Vector, 18, r=0.5, g=0.5))
    dot_product = g.node(unreal.MaterialExpressionDotProduct, 16)
    g.link(centre, dot_product, "A")
    g.link(centre, dot_product, "B")
    radius = g.mul(15, g.one(unreal.MaterialExpressionSquareRoot, 16, dot_product), g.const(2.0, 16))
    cracks = g.sample(noise, g.mul(15, uv, g.const(1.7, 16)), 14, "B")
    blot = g.sample(noise, g.mul(15, uv, g.const(0.8, 16)), 14, "R")

    # Ragged edge, charred black inside, browner towards the edge
    edge = g.sub(11, g.const(1.0, 12), g.add(12, radius, g.mul(13, g.sub(14, blot, g.const(0.5, 15)), g.const(0.6, 14))))
    cover = g.mul(9, g.sat(10, g.mul(11, edge, g.const(3.0, 12))), g.scalar("Deckkraft", 0.9, 10))
    char = g.lerp(8, g.vector("Asche", (0.06, 0.045, 0.035), 9), g.vector("Russ", (0.008, 0.006, 0.005), 9), g.sat(9, g.mul(10, edge, g.const(2.0, 11))))

    # Glowing cracks that flicker and die down
    time = g.node(unreal.MaterialExpressionTime, 16)
    pulse = g.sample(noise, g.add(14, g.mul(15, uv, g.const(0.6, 16)), g.mul(15, time, g.const(0.05, 16))), 13, "G")
    crack_lines = g.ramp(10, cracks, 0.7, 0.86)
    ember = g.mul(7, g.mul(8, crack_lines, g.ramp(9, pulse, 0.35, 0.7)), g.mul(8, g.sat(9, g.mul(10, edge, g.const(1.5, 11))), g.scalar("Glut", 1.0, 9)))
    # Under a burning fire the whole middle glows as a bed of embers that breathes with the flames
    bed = g.mul(7, g.mul(8, g.power(9, g.sat(10, g.sub(11, g.const(1.0, 12), radius)), g.const(1.6, 10)), g.ramp(9, pulse, 0.2, 0.8)), g.mul(8, g.scalar("Glut", 1.0, 9), g.const(1.4, 9)))
    glow = g.mul(5, g.vector("Glutfarbe", (1.0, 0.3, 0.04), 6), g.add(6, g.mul(7, ember, g.const(4.0, 8)), bed))

    library.connect_material_property(char, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(g.const(1.0, 3), "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(cover, "", unreal.MaterialProperty.MP_OPACITY)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


FLIPBOOKS = ("Flamme", "Feuer", "Kugel")
FLIPBOOK_COLUMNS = 8


def import_flipbook(kind):
    """A flipbook of simulated fire (Tools/Feuer/feuer_simulation.py): 8 x 8 frames, RGB the glowing fire, A its coverage"""
    name = "T_Feuer_Flipbook_" + kind
    path = FOLDER + "/" + name
    source = os.path.join(DATA, name + ".png")
    if not os.path.exists(source):
        notes.append("missing " + source)
        return None
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(path)
    if texture is not None:
        # The frames must not bleed into each other when the texture gets smaller
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
        assets.save_loaded_asset(texture, False)
        made.append(path)
    return texture


def build_flipbook_material(default_texture, noise):
    """Plays a flipbook of simulated fire on a card that faces the camera, frames blended softly, each card at its own
    moment (vertex colour R); a little extra wavering on top. Staerke fades it, Hitze brightens it."""
    material = new_material("M_Feuer_Flipbook")
    if material is None:
        return unreal.load_asset(FOLDER + "/M_Feuer_Flipbook")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, 30)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 29)
    u = g.mask(uv, 28, r=True, g=False, b=False)
    v = g.mask(uv, 28, r=False, g=True, b=False)
    vertex = g.node(unreal.MaterialExpressionVertexColor, 29)
    phase = g.mask(vertex, 28, r=True, g=False, b=False)
    time = g.node(unreal.MaterialExpressionTime, 28)

    # Which frame: each card runs through the 64 frames on its own, starting at its phase
    frames = float(FLIPBOOK_COLUMNS * FLIPBOOK_COLUMNS)
    position = g.mul(24, g.frac(25, g.add(26, g.mul(27, time, g.mul(28, g.scalar("Bildrate", 24.0, 29), g.const(1.0 / frames, 29))), phase)), g.const(frames, 25))
    frame_a = g.one(unreal.MaterialExpressionFloor, 23, position)
    blend = g.sub(22, position, frame_a)
    frame_b = g.sub(22, g.add(23, frame_a, g.const(1.0, 24)), g.mul(23, g.one(unreal.MaterialExpressionFloor, 24, g.mul(25, g.add(26, frame_a, g.const(1.0, 27)), g.const(1.0 / frames, 26))), g.const(frames, 24)))

    # A little extra wavering, more towards the top
    waver = g.mul(20, g.sub(21, g.sample(noise, scrolling_uv(g, u, v, phase, time, 0.8, 0.8, 0.9, 3.3, 23), 22, "R"), g.const(0.5, 22)), g.mul(21, v, g.const(0.06, 22)))
    cu = g.sat(19, g.add(20, u, waver))
    cv = g.sat(19, g.sub(20, g.const(1.0, 21), v))

    def frame_uv(frame, c):
        column = g.sub(c + 2, frame, g.mul(c + 3, g.one(unreal.MaterialExpressionFloor, c + 4, g.mul(c + 5, frame, g.const(1.0 / FLIPBOOK_COLUMNS, c + 5))), g.const(float(FLIPBOOK_COLUMNS), c + 4)))
        row = g.one(unreal.MaterialExpressionFloor, c + 2, g.mul(c + 3, frame, g.const(1.0 / FLIPBOOK_COLUMNS, c + 3)))
        x = g.mul(c + 1, g.add(c + 2, column, cu), g.const(1.0 / FLIPBOOK_COLUMNS, c + 2))
        y = g.mul(c + 1, g.add(c + 2, row, cv), g.const(1.0 / FLIPBOOK_COLUMNS, c + 2))
        return g.append(c, x, y)

    def flipbook_sample(frame_coordinates, c):
        expression = g.node(unreal.MaterialExpressionTextureSampleParameter2D, c, parameter_name="Flipbook", texture=default_texture,
                            sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        g.link(frame_coordinates, expression, "UVs")
        return expression

    sample_a = flipbook_sample(frame_uv(frame_a, 14), 13)
    sample_b = flipbook_sample(frame_uv(frame_b, 14), 13)
    colour = g.lerp(10, g.mask(sample_a, 11), g.mask(sample_b, 11), blend)
    alpha_a = g.node(unreal.MaterialExpressionMultiply, 11, const_b=1.0)
    g.link(sample_a, alpha_a, "A", "A")
    alpha_b = g.node(unreal.MaterialExpressionMultiply, 11, const_b=1.0)
    g.link(sample_b, alpha_b, "A", "A")
    alpha = g.lerp(10, alpha_a, alpha_b, blend)

    strength = g.scalar("Staerke", 1.0, 4)
    # A ragged, soft foot: noise shifts where the fire fades in, so no straight edge of the simulated source shows
    ragged = g.sample(noise, scrolling_uv(g, u, v, phase, time, 2.5, 1.0, 0.35, 4.7, 9), 8, "G")
    foot = g.ramp(5, g.add(6, v, g.mul(7, g.sub(8, ragged, g.const(0.5, 9)), g.const(0.22, 8))), 0.05, 0.3)
    emissive = g.mul(2, g.mul(3, colour, g.mul(4, g.scalar("Hitze", 1.0, 5), g.scalar("Leuchtkraft", 2.0, 5))), strength)
    # Soft at the top too, so a flame that reaches the edge of its frame never ends in a cut
    top = g.sat(4, g.mul(5, g.sub(6, g.const(1.0, 7), v), g.const(2.6, 6)))
    opacity = g.mul(2, g.mul(3, alpha, g.mul(4, foot, top)), strength)

    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    library.connect_material_property(billboard(g, u, v, 36), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def flipbook_instance(parent, kind, texture, values):
    name = "MI_Feuer_" + kind
    path = FOLDER + "/" + name
    instance = unreal.load_asset(path) if assets.does_asset_exist(path) else None
    if instance is None:
        instance = tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        library.set_material_instance_parent(instance, parent)
        for key, value in values.items():
            library.set_material_instance_scalar_parameter_value(instance, key, value)
        made.append(path)
    if texture is not None:
        library.set_material_instance_texture_parameter_value(instance, "Flipbook", texture)
    assets.save_loaded_asset(instance, False)
    return instance


def main():
    noise = import_noise()
    if noise is None:
        log("NOTE noise picture missing, run Tools/Feuer/feuer_texturen.py first")
        return
    build_flame(noise)
    build_smoke(noise)

    # Simulated fire: one flipbook material, an instance per kind of flipbook
    books = {kind: import_flipbook(kind) for kind in FLIPBOOKS}
    flipbook = build_flipbook_material(books.get("Flamme"), noise)
    for kind in FLIPBOOKS:
        flipbook_instance(flipbook, kind, books[kind], {"Leuchtkraft": {"Flamme": 1.3, "Feuer": 1.2, "Kugel": 1.8}[kind]})
    build_sparks()
    build_scorch(noise)
    for line in made:
        log("made " + line)
    for line in notes:
        log("NOTE " + line)
    log("done")


main()
