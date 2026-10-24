# Nid-Adern: the materials of the veins of the Nid (AVaelNidVeins), the base for the veins of every region.
# Boris 2026-10-22: a thick, dark red-violet liquid, thicker than water, dangerous, natural and never too perfect; foam here
# and there, bubbles, poisonous mist rising from them; a faint violet glow from the liquid itself (no lights).
#
# The vein mesh has UV0 u across the vein (0..1) and v along its way from the spring in metres, UV1 its width in metres and
# a random number per vein.
# - M_Nid_Ader: the open liquid. Large, slow, lazy waves on a glossy skin, like oil or blood; deep red-violet, darker where
#   it is thick, redder streaks drifting along; a dark crust at its edges; foam gathers in patches and along the edges and
#   drifts with it; the liquid glows faintly violet from inside, breathing slowly. Parameters: Fliessen (m/s, set by the
#   game), Leuchten, Schaum, Farbe/Tief/Kruste/Schaumfarbe/Glut.
# - M_Nid_Wulst: where a vein runs under the ground: the ground swells, dark and earthy, a faint violet pulse running along it.
# - M_Nid_Blase: the skin of a bubble swelling out of the liquid (per instance: how far through its life, set by the game).
# - M_Nid_Nebel: the poisonous mist out of a burst bubble, soft, sickly violet-grey, thinning away (per instance: life).
# Creates what is missing; a material is rebuilt when it is older than KIT_VERSION.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateNidKit.py"

import unreal

FOLDER = "/Game/Vael/Nid"
NOISE = "/Game/Vael/Effects/Feuer/T_Feuer_Rauschen"
WAVES = "/Engine/Functions/Engine_MaterialFunctions02/ExampleContent/Textures/water_n.water_n"
KIT_VERSION = 1.6

library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
made = []
notes = []


def log(message):
    unreal.log_warning("[CreateNidKit] " + message)


class Graph:
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
            notes.append("link failed: {}.{} -> {}.{}".format(source.get_name(), source_output, target.get_name(), target_input))

    def scalar(self, name, value, column):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=value)

    def const(self, value, column):
        return self.node(unreal.MaterialExpressionConstant, column, r=value)

    def vector(self, name, rgb, column):
        parameter = self.node(unreal.MaterialExpressionVectorParameter, column, parameter_name=name, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return self.mask(parameter, column - 1, "RGB")

    def mask(self, source, column, channels, source_output=""):
        expression = self.node(unreal.MaterialExpressionComponentMask, column, r="R" in channels, g="G" in channels, b="B" in channels, a="A" in channels)
        self.link(source, expression, "", source_output)
        return expression

    def op(self, expression_class, column, a, b):
        expression = self.node(expression_class, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        return expression

    def mul(self, c, a, b): return self.op(unreal.MaterialExpressionMultiply, c, a, b)
    def add(self, c, a, b): return self.op(unreal.MaterialExpressionAdd, c, a, b)
    def sub(self, c, a, b): return self.op(unreal.MaterialExpressionSubtract, c, a, b)
    def append(self, c, a, b): return self.op(unreal.MaterialExpressionAppendVector, c, a, b)

    def one(self, expression_class, c, a, **properties):
        expression = self.node(expression_class, c, **properties)
        self.link(a, expression)
        return expression

    def sat(self, c, a): return self.one(unreal.MaterialExpressionSaturate, c, a)
    def abs(self, c, a): return self.one(unreal.MaterialExpressionAbs, c, a)
    def sin(self, c, a): return self.one(unreal.MaterialExpressionSine, c, a, period=6.283185)

    def ramp(self, c, x, low, high):
        return self.sat(c, self.mul(c + 1, self.sub(c + 2, x, self.const(low, c + 3)), self.const(1.0 / (high - low), c + 2)))

    def lerp(self, c, a, b, alpha):
        expression = self.node(unreal.MaterialExpressionLinearInterpolate, c)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        self.link(alpha, expression, "Alpha")
        return expression

    def sample(self, texture, uv, c, channel, normal=False):
        expression = self.node(unreal.MaterialExpressionTextureSample, c, texture=texture,
                               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        self.link(uv, expression, "UVs")
        return expression if normal else self.mask(expression, c - 1, channel)


def new_material(name):
    path = FOLDER + "/" + name
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        if "KitVersion" in [str(p) for p in library.get_scalar_parameter_names(material)] and \
                library.get_material_default_scalar_parameter_value(material, "KitVersion") >= KIT_VERSION - 0.001:
            return None
        library.delete_all_material_expressions(material)
        # Nodes of older graphs survive that in the commandlet; with two parameters of one name the older default wins
        for expression in list(unreal.ObjectIterator(unreal.MaterialExpression)):
            if expression.get_outer() == material:
                library.delete_material_expression(material, expression)
        made.append(path + " (graph rebuilt)")
        return material
    made.append(path)
    return tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def finish(material, slots):
    """Connects every slot explicitly (a slot left out keeps pointing at a node of the graph this one replaced)"""
    for prop, node in slots.items():
        library.connect_material_property(node, "", prop)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)


def vein_coordinates(g):
    """u across, v along in metres, the width in metres, the random number of the vein, and the time"""
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 24)
    second = g.node(unreal.MaterialExpressionTextureCoordinate, 24, coordinate_index=1)
    u = g.mask(uv, 23, "R")
    v = g.mask(uv, 23, "G")
    width = g.mask(second, 23, "R")
    vein = g.mask(second, 23, "G")
    time = g.node(unreal.MaterialExpressionTime, 24)
    return u, v, width, vein, time


def build_liquid(noise, waves):
    material = new_material("M_Nid_Ader")
    if material is None:
        return
    # Masked: its edges are ragged, no vein has a clean outline
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, 26)
    u, v, width, vein, time = vein_coordinates(g)

    # The liquid creeps along the vein; coordinates in metres, the same size of detail on thick and thin veins
    flow = g.sub(21, v, g.mul(22, time, g.scalar("Fliessen", 0.12, 23)))
    across = g.mul(21, g.sub(22, u, g.const(0.5, 23)), width)
    flowing = g.append(20, g.add(21, across, g.mul(22, vein, g.const(17.0, 23))), flow)

    # Large lazy waves on a glossy skin: two layers of waves, the slower one larger, laid flat (thick, not splashing)
    slow = g.sample(waves, g.mul(18, flowing, g.const(0.18, 19)), 17, "", normal=True)
    lazy = g.sample(waves, g.add(18, g.mul(19, flowing, g.const(0.33, 20)), g.mul(19, time, g.const(0.01, 20))), 17, "", normal=True)
    flat = g.node(unreal.MaterialExpressionConstant3Vector, 15, constant=unreal.LinearColor(0, 0, 1, 1))
    mixed = g.mul(15, g.add(16, g.mask(slow, 17, "RGB"), g.mask(lazy, 17, "RGB")), g.const(0.5, 16))
    waves_normal = g.one(unreal.MaterialExpressionNormalize, 13, g.lerp(14, flat, mixed, g.scalar("Wellen", 0.35, 15)))

    # Streaks of redder liquid drift along; where it is thick it is darker
    streak = g.sample(noise, g.mul(18, flowing, g.const(0.12, 19)), 17, "R")
    centre = g.sub(16, g.const(1.0, 17), g.abs(17, g.mul(18, g.sub(19, u, g.const(0.5, 20)), g.const(2.0, 20))))
    deep = g.mul(14, centre, g.ramp(15, width, 0.2, 1.4))
    body = g.lerp(12, g.lerp(13, g.vector("Farbe", (0.09, 0.008, 0.014), 14), g.vector("Tief", (0.04, 0.004, 0.012), 14), deep), g.vector("Rot", (0.16, 0.014, 0.012), 13), g.mul(13, g.ramp(14, streak, 0.6, 0.9), g.const(0.6, 14)))

    # A dark crust where it touches the ground
    crust = g.sub(14, g.const(1.0, 15), g.ramp(15, centre, 0.05, 0.3))

    # Foam: gathers in patches and along the edges, drifts with the liquid and comes and goes
    patches = g.sample(noise, g.add(18, g.mul(19, flowing, g.const(0.3, 20)), g.mul(19, time, g.const(0.004, 20))), 17, "G")
    fine = g.sample(noise, g.mul(18, flowing, g.const(1.4, 19)), 17, "B")
    edge_band = g.mul(14, g.ramp(15, centre, 0.15, 0.3), g.sub(15, g.const(1.0, 16), g.ramp(16, centre, 0.35, 0.55)))
    froth = g.add(13, g.mul(14, patches, fine), g.mul(14, edge_band, g.const(0.25, 15)))
    foam = g.mul(10, g.sat(11, g.mul(12, g.sub(13, froth, g.const(0.32, 14)), g.const(6.0, 14))), g.scalar("Schaum", 1.0, 11))

    # Ragged edges: a noise along the vein eats into its outline, more into the thin veins
    nibble = g.sample(noise, g.mul(18, flowing, g.const(0.5, 19)), 17, "R")
    ragged = g.sub(9, g.add(10, centre, g.mul(11, g.sub(12, nibble, g.const(0.5, 13)), g.const(0.5, 12))), g.const(0.12, 10))

    colour = g.lerp(8, g.lerp(9, body, g.vector("Kruste", (0.025, 0.01, 0.018), 10), crust), g.vector("Schaumfarbe", (0.3, 0.2, 0.19), 10), foam)
    roughness = g.lerp(8, g.lerp(9, g.const(0.38, 10), g.const(0.85, 10), crust), g.const(0.6, 10), foam)
    normal = g.lerp(8, waves_normal, g.node(unreal.MaterialExpressionConstant3Vector, 10, constant=unreal.LinearColor(0, 0, 1, 1)), g.sat(9, g.add(10, crust, foam)))

    # It glows faintly violet from inside, breathing slowly; foam and crust do not glow
    breath = g.add(12, g.const(0.7, 13), g.mul(13, g.sin(14, g.add(15, g.mul(16, time, g.const(0.25, 17)), g.mul(16, v, g.const(0.03, 17)))), g.const(0.3, 14)))
    glow = g.mul(6, g.mul(7, g.vector("Glut", (0.28, 0.015, 0.22), 8), g.scalar("Leuchten", 0.08, 8)),
                 g.mul(7, g.mul(8, centre, breath), g.sub(8, g.const(1.0, 9), g.sat(9, g.add(10, foam, crust)))))

    finish(material, {
        unreal.MaterialProperty.MP_BASE_COLOR: colour,
        unreal.MaterialProperty.MP_ROUGHNESS: roughness,
        unreal.MaterialProperty.MP_SPECULAR: g.const(0.08, 3),
        unreal.MaterialProperty.MP_NORMAL: normal,
        unreal.MaterialProperty.MP_EMISSIVE_COLOR: glow,
        unreal.MaterialProperty.MP_OPACITY_MASK: ragged,
    })


def build_ridge(noise):
    material = new_material("M_Nid_Wulst")
    if material is None:
        return
    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, 26)
    u, v, width, vein, time = vein_coordinates(g)
    centre = g.sub(16, g.const(1.0, 17), g.abs(17, g.mul(18, g.sub(19, u, g.const(0.5, 20)), g.const(2.0, 20))))
    dirt = g.sample(noise, g.mul(18, g.append(19, g.mul(20, u, width), v), g.const(0.4, 19)), 17, "B")
    colour = g.lerp(10, g.vector("Erde", (0.05, 0.035, 0.03), 11), g.vector("Ader", (0.05, 0.015, 0.035), 11), g.mul(11, centre, g.add(12, g.const(0.5, 13), g.mul(13, dirt, g.const(0.5, 14)))))
    # A faint violet pulse runs along the vein under the ground, away from the spring like its liquid
    pulse = g.sat(10, g.mul(11, g.sin(12, g.sub(13, g.mul(14, v, g.const(0.15, 15)), g.mul(14, time, g.scalar("Fliessen", 0.12, 15)))), g.const(1.5, 12)))
    glow = g.mul(6, g.mul(7, g.vector("Glut", (0.35, 0.02, 0.4), 8), g.scalar("Leuchten", 0.05, 8)), g.mul(7, g.mul(8, centre, centre), pulse))
    finish(material, {
        unreal.MaterialProperty.MP_BASE_COLOR: colour,
        unreal.MaterialProperty.MP_ROUGHNESS: g.const(0.9, 3),
        unreal.MaterialProperty.MP_EMISSIVE_COLOR: glow,
    })


def build_bubble():
    material = new_material("M_Nid_Blase")
    if material is None:
        return
    material.set_editor_property("used_with_instanced_static_meshes", True)
    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, 26)
    life = g.node(unreal.MaterialExpressionPerInstanceCustomData, 20, data_index=0)
    fresnel = g.node(unreal.MaterialExpressionFresnel, 20, exponent=2.0)
    # The skin thins as the bubble swells: lighter, more violet before it bursts
    thin = g.ramp(15, life, 0.4, 1.0)
    colour = g.lerp(10, g.vector("Farbe", (0.12, 0.01, 0.045), 11), g.vector("Duenn", (0.3, 0.08, 0.16), 11), thin)
    glow = g.mul(6, g.vector("Glut", (0.45, 0.04, 0.42), 7), g.mul(7, g.add(8, g.mul(9, fresnel, g.const(0.6, 10)), g.mul(9, thin, g.const(0.4, 10))), g.scalar("Leuchten", 0.5, 8)))
    finish(material, {
        unreal.MaterialProperty.MP_BASE_COLOR: colour,
        unreal.MaterialProperty.MP_ROUGHNESS: g.const(0.08, 3),
        unreal.MaterialProperty.MP_SPECULAR: g.const(0.7, 3),
        unreal.MaterialProperty.MP_EMISSIVE_COLOR: glow,
    })


def build_mist(noise):
    material = new_material("M_Nid_Nebel")
    if material is None:
        return
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, 26)
    life = g.node(unreal.MaterialExpressionPerInstanceCustomData, 20, data_index=0)
    phase = g.node(unreal.MaterialExpressionPerInstanceRandom, 20)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 20)
    time = g.node(unreal.MaterialExpressionTime, 20)
    # Soft at its outline, torn by a slowly turning noise, coming up quickly and thinning away
    fresnel = g.node(unreal.MaterialExpressionFresnel, 18, exponent=1.5)
    soft = g.sub(16, g.const(1.0, 17), fresnel)
    wisps = g.sample(noise, g.add(17, g.mul(18, uv, g.const(1.5, 19)), g.add(18, g.mul(19, time, g.const(0.03, 20)), phase)), 16, "R")
    coming = g.mul(13, g.sat(14, g.mul(15, life, g.const(6.0, 16))), g.sub(14, g.const(1.0, 15), life))
    opacity = g.mul(10, g.mul(11, g.mul(12, soft, soft), g.ramp(12, wisps, 0.25, 0.8)), g.mul(11, coming, g.scalar("Dichte", 0.5, 12)))
    colour = g.mul(8, g.vector("Farbe", (0.3, 0.14, 0.34), 9), g.scalar("Leuchten", 0.6, 9))
    finish(material, {
        unreal.MaterialProperty.MP_EMISSIVE_COLOR: colour,
        unreal.MaterialProperty.MP_OPACITY: opacity,
    })


def main():
    noise = unreal.load_asset(NOISE)
    waves = unreal.load_asset(WAVES)
    if noise is None or waves is None:
        notes.append("missing noise or wave texture")
    else:
        build_liquid(noise, waves)
        build_ridge(noise)
        build_bubble()
        build_mist(noise)
    for line in made:
        log("made " + line)
    for line in notes:
        log("NOTE " + line)
    log("done")


main()
