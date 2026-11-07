# Materials of Die Waage (Lore-Bibel, Die drei Wege): how corruption shows on a character and on a region.
#
# - M_Vael_Zeichnung: overlay on the meshes of a player. Shadow: violet Nid veins that creep and pulse, at the rim of the body;
#   light: a faint warm shimmer of fairy threads. Parameters Staerke (0..1), Farbe, Adern (0 = shimmer, 1 = veins).
# - M_PP_Verderbnis: post process of a region. Verderbnis 0..1 of the region: in the shadow the greens and browns of grass and
#   ground fade to ash and violet, the shadows turn violet; in the light the greens grow warm and full. Only colours change.
# Creates only its own assets in /Game/Vael/Balance; run again to update.
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateBalanceAssets.py" -unattended -nosplash

import unreal

FOLDER = "/Game/Vael/Balance"
library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
notes = []


class Graph:
    def __init__(self, material):
        self.material = material
        self.rows = {}

    def node(self, cls, column, **props):
        row = self.rows.get(column, 0)
        self.rows[column] = row + 1
        e = library.create_material_expression(self.material, cls, -300 * column, 140 * row)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, a, b, pin="", out=""):
        if not library.connect_material_expressions(a, out, b, pin):
            notes.append("link failed {} -> {}.{}".format(a.get_name(), b.get_name(), pin))

    def const(self, v, c):
        return self.node(unreal.MaterialExpressionConstant, c, r=v)

    def scalar(self, name, v, c):
        return self.node(unreal.MaterialExpressionScalarParameter, c, parameter_name=name, default_value=v)

    def vec(self, name, rgb, c):
        p = self.node(unreal.MaterialExpressionVectorParameter, c, parameter_name=name, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return self.mask(p, c - 1)

    def c3(self, rgb, c):
        return self.node(unreal.MaterialExpressionConstant3Vector, c, constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))

    def op(self, cls, c, a, b):
        e = self.node(cls, c)
        self.link(a, e, "A")
        self.link(b, e, "B")
        return e

    def mul(self, c, a, b):
        return self.op(unreal.MaterialExpressionMultiply, c, a, b)

    def add(self, c, a, b):
        return self.op(unreal.MaterialExpressionAdd, c, a, b)

    def sub(self, c, a, b):
        return self.op(unreal.MaterialExpressionSubtract, c, a, b)

    def lerp(self, a, b, t, c):
        e = self.node(unreal.MaterialExpressionLinearInterpolate, c)
        self.link(a, e, "A")
        self.link(b, e, "B")
        self.link(t, e, "Alpha")
        return e

    def sat(self, a, c):
        e = self.node(unreal.MaterialExpressionSaturate, c)
        self.link(a, e)
        return e

    def mask(self, a, c, r=True, g=True, b=True):
        e = self.node(unreal.MaterialExpressionComponentMask, c, r=r, g=g, b=b, a=False)
        self.link(a, e)
        return e

    def power(self, c, a, b):
        e = self.node(unreal.MaterialExpressionPower, c)
        self.link(a, e, "Base")
        self.link(b, e, "Exp")
        return e

    def dot(self, c, a, b):
        return self.op(unreal.MaterialExpressionDotProduct, c, a, b)

    def noise(self, scale, c, position, lo=0.0, hi=1.0, levels=3, fn=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU):
        e = self.node(unreal.MaterialExpressionNoise, c, scale=scale, levels=levels, output_min=lo, output_max=hi, noise_function=fn)
        self.link(position, e)
        return e


def new_material(name):
    path = FOLDER + "/" + name
    if assets.does_asset_exist(path):
        m = unreal.load_asset(path)
        library.delete_all_material_expressions(m)
    else:
        m = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    return m


def overlay():
    m = new_material("M_Vael_Zeichnung")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    # worn by the skeletal meshes of the players (and MetaHumans with morph targets)
    for usage in ("used_with_skeletal_mesh", "used_with_morph_targets"):
        try:
            m.set_editor_property(usage, True)
        except Exception as error:
            notes.append("{}: {}".format(usage, error))
    g = Graph(m)
    strength = g.scalar("Staerke", 0.0, 2)
    veins_on = g.scalar("Adern", 1.0, 4)
    # rim of the body: more at the edges
    fresnel = g.node(unreal.MaterialExpressionFresnel, 4, exponent=2.2, base_reflect_fraction=0.0)
    # creeping veins: ridged noise on the world position, slowly moving upward
    time = g.node(unreal.MaterialExpressionTime, 8)
    rise = g.mul(7, time, g.c3((0.0, 0.0, 6.0), 8))
    pos = g.add(6, g.node(unreal.MaterialExpressionWorldPosition, 7), rise)
    n = g.noise(0.035, 5, pos, lo=-1.0, hi=1.0)
    absn = g.node(unreal.MaterialExpressionAbs, 4)
    g.link(n, absn)
    veins = g.sat(g.sub(3, g.const(1.0, 4), g.mul(4, absn, g.const(9.0, 5))), 3)
    # light shimmer: soft drifting specks
    specks = g.power(4, g.noise(0.4, 5, pos), g.const(9.0, 5))
    pattern = g.lerp(specks, veins, veins_on, 2)
    pulse_sine = g.node(unreal.MaterialExpressionSine, 4, period=6.2832)
    g.link(g.mul(5, time, g.const(1.4, 6)), pulse_sine)
    pulse = g.add(3, g.mul(4, pulse_sine, g.const(0.25, 5)), g.const(1.0, 4))
    shape = g.mul(1, g.mul(2, pattern, g.add(3, fresnel, g.const(0.25, 4))), g.mul(2, strength, pulse))
    emissive = g.mul(0, shape, g.mul(1, g.vec("Farbe", (0.65, 0.25, 1.0), 2), g.scalar("Leuchten", 4.0, 2)))
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.recompile_material(m)
    assets.save_loaded_asset(m, False)


def post_process():
    m = new_material("M_PP_Verderbnis")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    try:
        m.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    except Exception as error:
        notes.append("blendable location: {}".format(error))
    g = Graph(m)
    scene = g.node(unreal.MaterialExpressionSceneTexture, 6, scene_texture_id=unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    color = g.mask(scene, 5)
    value = g.scalar("Verderbnis", 0.3, 6)
    shadow = g.sat(g.mul(4, g.sub(5, value, g.const(0.3, 6)), g.const(1.43, 5)), 3)
    light = g.sat(g.mul(4, g.sub(5, g.const(0.3, 6), value), g.const(3.33, 5)), 3)
    lum = g.dot(4, color, g.c3((0.3, 0.59, 0.11), 5))
    # how green / earthy a pixel is: grass and ground change, sky and fire hardly
    greenness = g.sat(g.mul(4, g.sub(5, g.mask(color, 6, r=False, g=True, b=False), g.mask(color, 6, r=False, g=False, b=True)), g.const(3.0, 5)), 3)
    earth = g.sat(g.add(3, greenness, g.const(0.35, 4)), 2)
    # shadow: ash and violet; dark areas lean violet
    ash = g.mul(3, lum, g.c3((0.92, 0.82, 1.08), 4))
    violet_shadows = g.mul(3, g.sat(g.sub(4, g.const(0.35, 5), lum), 4), g.c3((0.12, 0.0, 0.2), 4))
    shadow_look = g.mul(1, g.add(2, g.lerp(color, ash, g.mul(3, earth, g.const(0.5, 4)), 3), violet_shadows), g.c3((0.86, 0.82, 0.9), 2))
    # light: warm, full greens
    rich = g.add(3, g.mul(4, g.sub(5, color, lum), g.const(1.35, 5)), lum)
    light_look = g.mul(2, g.lerp(color, rich, earth, 3), g.c3((1.04, 1.03, 0.96), 3))
    out = g.lerp(g.lerp(color, light_look, light, 1), shadow_look, shadow, 1)
    library.connect_material_property(out, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.recompile_material(m)
    assets.save_loaded_asset(m, False)


overlay()
post_process()
for note in notes:
    unreal.log_warning("[Waage] Hinweis: " + note)
unreal.log_warning("[Waage] Materialien fertig")
