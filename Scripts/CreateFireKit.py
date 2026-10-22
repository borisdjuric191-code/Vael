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
KIT_VERSION = 2.0

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
        return self.mask(expression, c - 1, r=channel == "R", g=channel == "G", b=channel == "B", a=channel == "A")


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
    up = g.one(unreal.MaterialExpressionNormalize, c + 2, g.vector("Richtung", (0.0, 0.0, 1.0), c + 4))
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
    g.scalar("KitVersion", KIT_VERSION, 20)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 19)
    u = g.mask(uv, 18, r=True, g=False, b=False)
    v = g.mask(uv, 18, r=False, g=True, b=False)
    vertex = g.node(unreal.MaterialExpressionVertexColor, 19)
    phase = g.mask(vertex, 18, r=True, g=False, b=False)
    tongue_heat = g.mask(vertex, 18, r=False, g=True, b=False)
    time = g.mul(17, g.node(unreal.MaterialExpressionTime, 18), g.scalar("Tempo", 1.2, 18))

    # Three noises rising through the tongue at different speeds
    coarse = g.sample(noise, scrolling_uv(g, u, v, phase, time, 0.6, 0.7, 0.55, 3.7, 14), 13, "R")
    middle = g.sample(noise, scrolling_uv(g, u, v, phase, time, 1.1, 1.3, 1.1, 5.3, 14), 13, "G")
    fine = g.sample(noise, scrolling_uv(g, u, v, phase, time, 2.0, 2.4, 1.9, 1.9, 14), 13, "B")

    # The tongue bends and snakes, more towards its tip
    bend = g.mul(11, g.sub(12, coarse, g.const(0.5, 13)), g.mul(12, g.power(13, v, g.const(1.2, 14)), g.const(0.55, 13)))
    snake = g.mul(11, g.sin(12, g.add(13, g.add(14, g.mul(15, time, g.const(1.7, 16)), g.mul(15, phase, g.const(6.28, 16))), g.mul(14, v, g.const(3.0, 15)))),
                  g.mul(12, v, g.const(0.07, 13)))
    across = g.add(10, u, g.add(11, bend, snake))
    distance = g.mul(9, g.abs(10, g.sub(11, across, g.const(0.5, 12))), g.const(2.0, 10))

    # Round at the base, pointed at the tip
    width = g.mul(9, g.power(10, g.sat(11, g.sub(12, g.const(1.0, 13), v)), g.const(0.6, 11)), g.const(0.62, 10))
    shape = g.sat(8, g.mul(9, g.sub(10, width, distance), g.const(3.5, 10)))

    # Heat: the core of the tongue is hottest, noise eats the edges, the top cools down
    body = g.mul(7, g.mul(8, shape, g.sub(9, g.const(1.15, 10), v)), g.mul(8, tongue_heat, g.scalar("Hitze", 1.0, 9)))
    eaten = g.add(7, g.mul(8, g.sub(9, middle, g.const(0.5, 10)), g.const(0.55, 9)), g.mul(8, g.sub(9, fine, g.const(0.5, 10)), g.const(0.25, 9)))
    flicker = g.add(7, g.const(0.85, 8), g.mul(8, g.sin(9, g.add(10, g.mul(11, time, g.const(9.0, 12)), g.mul(11, phase, g.const(20.0, 12)))), g.const(0.15, 9)))
    heat = g.sat(5, g.mul(6, g.sub(7, g.add(8, body, eaten), g.mul(8, v, g.const(0.25, 9))), flicker))

    # Colours from the dark red-orange edge over orange to the white-yellow core
    edge = g.vector("Rand", (0.55, 0.06, 0.01), 6)
    middle_colour = g.vector("Mitte", (1.0, 0.3, 0.025), 6)
    core = g.vector("Kern", (1.0, 0.74, 0.32), 6)
    colour = g.lerp(3, g.lerp(4, edge, middle_colour, g.ramp(5, heat, 0.3, 0.62)), core, g.ramp(4, heat, 0.74, 0.98))
    glow = g.mul(2, colour, g.mul(3, g.add(4, heat, g.const(0.15, 5)), g.scalar("Leuchtkraft", 2.2, 4)))

    # Smoke: the cool top of the tongue darkens what lies behind it
    smoke = g.mul(4, g.mul(5, shape, g.ramp(6, v, 0.4, 0.9)), g.mul(5, coarse, g.scalar("Dunkel", 0.6, 6)))
    burning = g.ramp(3, heat, 0.06, 0.28)
    strength = g.scalar("Staerke", 1.0, 3)
    # Soft at the foot, so the tongues grow out of the ground; the dark smoke only where nothing burns, so no dark seam at the edges
    foot = g.ramp(3, v, 0.08, 0.42)
    emissive = g.mul(1, g.lerp(2, g.vector("Rauch", (0.03, 0.02, 0.015), 3), glow, g.ramp(3, heat, 0.0, 0.06)), strength)
    opacity = g.mul(1, g.mul(2, g.sat(3, g.add(4, burning, g.mul(5, smoke, g.const(0.3, 6)))), foot), strength)

    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    library.connect_material_property(billboard(g, u, v, 26), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
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


def main():
    noise = import_noise()
    if noise is None:
        log("NOTE noise picture missing, run Tools/Feuer/feuer_texturen.py first")
        return
    build_flame(noise)
    build_sparks()
    build_scorch(noise)
    for line in made:
        log("made " + line)
    for line in notes:
        log("NOTE " + line)
    log("done")


main()
