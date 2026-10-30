# Creates the player HUD for all five classes (Boris' HUD spec, 2026-10-18):
# - fonts (OFL): IM Fell English SC for titles, Alegreya Sans for text -> /Game/Vael/UI/Fonts, made by a game run with -VaelMakeFonts
# - shape masks drawn by Tools/HUD/vael_hud_formen.py (Scripts/Data/HUD) -> /Game/Vael/UI/HUD/Formen and /Symbole
# - UI materials: M_HUD_Gefaess (vessel: mask, fill, wobbling surface, glass reflex, cracks below 25 %), M_HUD_Rahmen (class frame,
#   Nid veins creeping in with corruption, light / shadow / twilight after the oath), M_HUD_Feld (slot, medallion, badge),
#   M_HUD_Wisch (cooldown wipe), M_HUD_Schwanz (experience bar as the tail of the Urdrachin), M_HUD_Fenster (menu windows:
#   parchment, dark iron with gold filigree, class material)
# - one instance of vessel, frame and slot per class, the data assets DA_HUDKlasse_<Klasse> and WBP_SpielerHUD
# Only creates what is missing; master materials are rebuilt when they are older than KIT_VERSION.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateHudKit.py"

import math
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
SHAPES = os.path.join(HERE, "Data", "HUD")
PAINTED = os.path.join(HERE, "Data", "HUD", "Gemalt")
FONTS = os.path.join(HERE, "Data", "Fonts")
HUD = "/Game/Vael/UI/HUD"
FONT_FOLDER = "/Game/Vael/UI/Fonts"
CLASS_FOLDER = HUD + "/Klassen"

library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
created = []

KIT_VERSION = 1.5


def srgb(hex_color, a=1.0):
    """Colour of the spec (sRGB hex) as the linear colour a material works with"""
    hex_color = hex_color.lstrip("#")
    def lin(c):
        c = c / 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    return unreal.LinearColor(lin(int(hex_color[0:2], 16)), lin(int(hex_color[2:4], 16)), lin(int(hex_color[4:6], 16)), a)


def rgb(r, g, b, a=1.0):
    return unreal.LinearColor(r, g, b, a)


# ---------------------------------------------------------------- Graph helpers

class Graph:
    def __init__(self, material):
        self.material = material
        self.row = {}

    def node(self, expression_class, column, **properties):
        y = self.row.get(column, 0)
        self.row[column] = y + 120
        expression = library.create_material_expression(self.material, expression_class, -300 * column, y)
        for key, value in properties.items():
            expression.set_editor_property(key, value)
        return expression

    def scalar(self, name, default, column=10):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=default)

    def color(self, name, default, column=10):
        return self.node(unreal.MaterialExpressionVectorParameter, column, parameter_name=name, default_value=default)

    def const(self, value, column=9):
        return self.node(unreal.MaterialExpressionConstant, column, r=value)

    def const2(self, x, y, column=9):
        return self.node(unreal.MaterialExpressionConstant2Vector, column, r=x, g=y)

    def const3(self, c, column=9):
        return self.node(unreal.MaterialExpressionConstant3Vector, column, constant=c)

    def link(self, source, target, target_input, source_output=""):
        if not library.connect_material_expressions(source, source_output, target, target_input):
            unreal.log_error("Vael HUD kit: could not connect {} -> {}.{}".format(source.get_name(), target.get_name(), target_input))

    def op(self, expression_class, column, a, b=None, a_input="A", b_input="B", **properties):
        expression = self.node(expression_class, column, **properties)
        self.link(a, expression, a_input)
        if b is not None:
            self.link(b, expression, b_input)
        return expression

    def mul(self, c, a, b): return self.op(unreal.MaterialExpressionMultiply, c, a, b)
    def add(self, c, a, b): return self.op(unreal.MaterialExpressionAdd, c, a, b)
    def sub(self, c, a, b): return self.op(unreal.MaterialExpressionSubtract, c, a, b)
    def div(self, c, a, b): return self.op(unreal.MaterialExpressionDivide, c, a, b)
    def max(self, c, a, b): return self.op(unreal.MaterialExpressionMax, c, a, b)
    def min(self, c, a, b): return self.op(unreal.MaterialExpressionMin, c, a, b)
    def abs(self, c, a): return self.op(unreal.MaterialExpressionAbs, c, a, a_input="")
    def frac(self, c, a): return self.op(unreal.MaterialExpressionFrac, c, a, a_input="")
    def sat(self, c, a): return self.op(unreal.MaterialExpressionSaturate, c, a, a_input="")
    def one_minus(self, c, a): return self.op(unreal.MaterialExpressionOneMinus, c, a, a_input="")
    def sqrt(self, c, a): return self.op(unreal.MaterialExpressionSquareRoot, c, a, a_input="")
    def sin(self, c, a): return self.op(unreal.MaterialExpressionSine, c, a, a_input="", period=2.0 * math.pi)
    def power(self, c, a, e): return self.op(unreal.MaterialExpressionPower, c, a, e, a_input="Base", b_input="Exp")
    def append(self, c, a, b): return self.op(unreal.MaterialExpressionAppendVector, c, a, b)
    def dist(self, c, a, b): return self.op(unreal.MaterialExpressionDistance, c, a, b)

    def lerp(self, c, a, b, alpha):
        expression = self.op(unreal.MaterialExpressionLinearInterpolate, c, a, b)
        self.link(alpha, expression, "Alpha")
        return expression

    def channel(self, c, source, output):
        """One output of a node (R, G, B, A) as its own expression"""
        single = self.node(unreal.MaterialExpressionMultiply, c, const_b=1.0)
        self.link(source, single, "A", output)
        return single

    def sharp(self, c, a, k):
        """saturate(a * k): a soft step at 0"""
        return self.sat(c, self.mul(c, a, self.const(k, c + 1)))

    def output(self, expression, target, source_output=""):
        if not library.connect_material_property(expression, source_output, target):
            unreal.log_error("Vael HUD kit: could not connect {} to {}".format(self.material.get_name(), target))


def new_material(name):
    path = "{}/{}".format(HUD, name)
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        if "KitVersion" in [str(p) for p in library.get_scalar_parameter_names(material)]:
            if library.get_material_default_scalar_parameter_value(material, "KitVersion") >= KIT_VERSION - 0.001:
                return None
        library.delete_all_material_expressions(material)
        created.append(path + " (graph rebuilt)")
        return material
    created.append(path)
    material = tools.create_asset(name, HUD, unreal.Material, unreal.MaterialFactoryNew())
    return material


def ui_material(name):
    material = new_material(name)
    if material is None:
        return None, None
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    g = Graph(material)
    g.scalar("KitVersion", KIT_VERSION, column=12)
    return material, g


def finish(material, g, color, opacity):
    g.output(color, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.output(opacity, unreal.MaterialProperty.MP_OPACITY)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def load(name):
    return unreal.load_asset("{}/{}".format(HUD, name))


def uv_parts(g):
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 11)
    u = g.node(unreal.MaterialExpressionComponentMask, 10, r=True, g=False, b=False, a=False)
    g.link(uv, u, "")
    v = g.node(unreal.MaterialExpressionComponentMask, 10, r=False, g=True, b=False, a=False)
    g.link(uv, v, "")
    return uv, u, v


def vertex_tint(g):
    vc = g.node(unreal.MaterialExpressionVertexColor, 11)
    rgb_ = g.node(unreal.MaterialExpressionComponentMask, 10, r=True, g=True, b=True, a=False)
    g.link(vc, rgb_, "")
    alpha = g.channel(10, vc, "A")
    return rgb_, alpha


# ---------------------------------------------------------------- Vessel

def build_vessel(default_mask):
    material, g = ui_material("M_HUD_Gefaess")
    if material is None:
        return load("M_HUD_Gefaess")

    mask_tex = g.node(unreal.MaterialExpressionTextureObjectParameter, 11, parameter_name="Mask", texture=default_mask,
                      sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    fill = g.scalar("Fill", 0.7)
    fill_top = g.scalar("FillTop", 0.5)
    split = g.scalar("Split", 0.0)
    bottom = g.scalar("LiquidBottom", 0.9)
    top = g.scalar("LiquidTop", 0.2)
    top_bottom = g.scalar("TopChamberBottom", 0.48)
    top_top = g.scalar("TopChamberTop", 0.18)
    wobble = g.scalar("Wobble", 1.0)
    liquid_color = g.color("LiquidColor", srgb("9C2A2A"))
    liquid_color2 = g.color("LiquidColor2", srgb("E1BD52"))
    surface_color = g.color("SurfaceColor", srgb("D9574A"))
    surface_color2 = g.color("SurfaceColor2", srgb("F2D98A"))
    glass_color = g.color("GlassColor", rgb(0.5, 0.52, 0.55))
    glass_amount = g.scalar("GlassAmount", 1.0)

    uv, u, v = uv_parts(g)
    sample = g.node(unreal.MaterialExpressionTextureSample, 9, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    g.link(mask_tex, sample, "Tex")
    g.link(uv, sample, "UVs")
    inner = g.channel(8, sample, "R")
    glass = g.mul(7, g.channel(8, sample, "G"), glass_amount)
    crack_mask = g.channel(8, sample, "B")
    alpha = g.channel(8, sample, "A")

    # A slowly wobbling surface: two waves across
    time = g.node(unreal.MaterialExpressionTime, 10)
    wave = g.mul(6, g.add(7, g.mul(7, g.sin(8, g.add(8, g.mul(8, u, g.const(14.0)), g.mul(8, time, g.const(2.2)))), g.const(0.006)),
                             g.mul(7, g.sin(8, g.sub(8, g.mul(8, u, g.const(6.0)), g.mul(8, time, g.const(1.4)))), g.const(0.005))), wobble)

    level = g.add(6, g.lerp(7, bottom, top, fill), wave)
    level2 = g.add(6, g.lerp(7, top_bottom, top_top, fill_top), wave)
    in_top = g.mul(6, g.sharp(6, g.sub(7, g.const(0.5), v), 300.0), split)
    lower = g.one_minus(5, in_top)
    liquid = g.mul(5, g.mul(5, inner, g.sharp(6, g.sub(7, v, level), 300.0)), lower)
    liquid2 = g.mul(5, g.mul(5, inner, g.sharp(6, g.sub(7, v, level2), 300.0)), in_top)
    band = g.mul(5, g.mul(5, inner, lower), g.sat(6, g.sub(6, g.const(1.0), g.mul(6, g.abs(7, g.sub(7, v, level)), g.const(80.0)))))
    band2 = g.mul(5, g.mul(5, inner, in_top), g.sat(6, g.sub(6, g.const(1.0), g.mul(6, g.abs(7, g.sub(7, v, level2)), g.const(80.0)))))

    # Darker at the bottom, brighter towards the surface
    shade = g.add(5, g.const(0.55), g.mul(5, g.one_minus(6, v), g.const(0.6)))
    color = g.const3(rgb(0.025, 0.02, 0.025), 5)
    color = g.lerp(4, color, g.mul(4, liquid_color, shade), liquid)
    color = g.lerp(4, color, g.mul(4, liquid_color2, shade), liquid2)
    color = g.lerp(4, color, surface_color, g.sat(4, band))
    color = g.lerp(4, color, surface_color2, g.sat(4, band2))

    # Glass reflex: a soft bright stripe on the left of the inside
    reflex = g.mul(4, g.mul(4, inner, g.sat(5, g.sub(5, g.const(1.0), g.mul(5, g.abs(6, g.sub(6, u, g.const(0.36))), g.const(28.0))))),
                   g.mul(5, g.sat(5, g.sub(5, g.const(1.0), g.mul(5, g.abs(6, g.sub(6, v, g.const(0.58))), g.const(3.0)))), g.const(0.45)))
    color = g.add(3, color, reflex)

    # Cracks show below a quarter and close again as it fills
    crack = g.mul(4, crack_mask, g.sharp(5, g.sub(6, g.const(0.25), fill), 8.0))
    color = g.lerp(3, color, g.const3(rgb(0.82, 0.78, 0.72), 4), crack)

    glass_shade = g.mul(3, glass_color, g.add(4, g.const(0.7), g.mul(4, g.one_minus(5, v), g.const(0.6))))
    color = g.lerp(2, color, glass_shade, glass)

    solid = g.sat(3, g.add(3, g.add(4, g.add(5, liquid, liquid2), g.add(5, glass, crack)), g.add(4, band, band2)))
    opacity = g.mul(2, alpha, g.add(3, g.const(0.55), g.mul(3, solid, g.const(0.45))))
    return finish(material, g, color, opacity)


# ---------------------------------------------------------------- Frame

def build_frame(default_veins):
    material, g = ui_material("M_HUD_Rahmen")
    if material is None:
        return load("M_HUD_Rahmen")

    base_color = g.color("BaseColor", srgb("15120F"))
    frame_color = g.color("FrameColor", srgb("4A74A8"))
    aspect = g.scalar("Aspect", 880.0 / 202.0)
    radius = g.scalar("Radius", 0.08)
    border_width = g.scalar("BorderWidth", 0.03)
    corruption = g.scalar("Corruption", 0.0)
    light = g.scalar("Light", 0.0)
    shadow = g.scalar("Shadow", 0.0)
    twilight = g.scalar("Twilight", 0.0)
    opacity_value = g.scalar("Opacity", 0.93)
    vein_color = g.color("VeinColor", srgb("9B5DE5"))
    vein_tex = g.node(unreal.MaterialExpressionTextureObjectParameter, 11, parameter_name="Veins",
                      texture=default_veins, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)

    uv, u, v = uv_parts(g)
    # Rounded box in height units
    px = g.mul(9, g.sub(9, u, g.const(0.5)), aspect)
    py = g.sub(9, v, g.const(0.5))
    hx = g.sub(8, g.mul(8, aspect, g.const(0.5)), radius)
    hy = g.sub(8, g.const(0.5), radius)
    qx = g.sub(7, g.abs(8, px), hx)
    qy = g.sub(7, g.abs(8, py), hy)
    outside = g.dist(6, g.append(6, g.max(7, qx, g.const(0.0)), g.max(7, qy, g.const(0.0))), g.const2(0.0, 0.0))
    d = g.sub(5, g.add(6, outside, g.min(6, g.max(7, qx, qy), g.const(0.0))), radius)
    inside = g.sharp(5, g.mul(6, d, g.const(-1.0)), 300.0)
    border = g.mul(4, g.sat(5, g.sub(5, g.const(1.0), g.div(5, g.abs(6, g.add(6, d, border_width)), border_width))), inside)
    inner_line = g.mul(4, g.sat(5, g.sub(5, g.const(1.0), g.div(5, g.abs(6, g.add(6, d, g.mul(6, border_width, g.const(3.0)))), g.mul(6, border_width, g.const(0.3))))), g.const(0.6))

    # Light to the left, shadow to the right for twilight
    lit = g.add(6, light, g.mul(6, twilight, g.one_minus(7, u)))
    dark = g.add(6, shadow, g.mul(6, twilight, u))
    warm = g.const3(rgb(1.0, 0.78, 0.45), 6)
    iron = g.const3(rgb(0.11, 0.1, 0.12), 6)
    frame = g.add(4, g.lerp(5, frame_color, iron, g.mul(5, dark, g.const(0.85))), g.mul(5, warm, g.mul(5, lit, g.const(0.35))))
    ground = g.mul(5, base_color, g.add(6, g.const(0.85), g.mul(6, g.one_minus(7, v), g.const(0.3))))
    ground = g.add(4, g.lerp(5, ground, g.mul(5, ground, g.const(0.6)), dark), g.mul(5, warm, g.mul(5, lit, g.const(0.04))))

    # Fairy threads along the edges on the way of light
    time = g.node(unreal.MaterialExpressionTime, 10)
    wiggle = g.mul(7, g.sin(8, g.add(8, g.mul(8, v, g.const(7.0)), g.mul(8, time, g.const(0.6)))), g.const(1.5))
    threads = g.power(5, g.sat(6, g.add(6, g.mul(6, g.sin(7, g.add(7, g.mul(7, g.mul(8, u, aspect), g.const(9.0)), wiggle)), g.const(0.5)), g.const(0.5))), g.const(30.0))
    near_edge = g.sat(5, g.add(5, g.const(1.0), g.mul(5, d, g.const(8.0))))
    threads = g.mul(4, g.mul(4, threads, near_edge), g.mul(4, lit, g.const(0.5)))

    color = g.lerp(3, ground, frame, g.sat(4, g.add(4, border, inner_line)))
    color = g.add(3, color, g.mul(3, warm, threads))

    # Nid veins creep in from the lower left corner with the corruption, from 50 % a second bundle from the upper right
    sample = g.node(unreal.MaterialExpressionTextureSample, 8, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    g.link(vein_tex, sample, "Tex")
    g.link(uv, sample, "UVs")
    flipped = g.sub(9, g.const2(1.0, 1.0), uv)
    sample2 = g.node(unreal.MaterialExpressionTextureSample, 8, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    g.link(vein_tex, sample2, "Tex")
    g.link(flipped, sample2, "UVs")
    vein1 = g.mul(6, g.channel(7, sample, "R"), g.sharp(6, g.sub(7, corruption, g.channel(7, sample, "G")), 25.0))
    second = g.sub(7, g.mul(7, corruption, g.const(2.0)), g.const(1.0))
    vein2 = g.mul(6, g.channel(7, sample2, "R"), g.sharp(6, g.sub(7, second, g.channel(7, sample2, "G")), 25.0))
    pulse = g.add(5, g.const(0.8), g.mul(5, g.sin(6, g.mul(6, time, g.const(2.5))), g.const(0.2)))
    veins = g.sat(4, g.mul(4, g.mul(5, g.add(5, vein1, vein2), g.add(5, g.const(1.0), g.mul(5, dark, g.const(0.6)))), pulse))
    vein_strength = g.scalar("VeinStrength", 0.55)
    color = g.lerp(2, color, g.mul(3, vein_color, g.const(1.2)), g.mul(3, g.mul(4, veins, inside), vein_strength))

    return finish(material, g, color, g.mul(2, inside, opacity_value))


# ---------------------------------------------------------------- Painted frame

def build_painted_frame(default_picture, default_mask, default_veins):
    """The frame rendered in Blender, laid into a material: Nid veins creep over its panel with the corruption, the way of shadow turns
    it to dark iron with violet gems that glow brighter with corruption, the way of light warms it, twilight goes from one to the other"""
    material, g = ui_material("M_HUD_Rahmen_Bild")
    if material is None:
        return load("M_HUD_Rahmen_Bild")

    picture = g.node(unreal.MaterialExpressionTextureObjectParameter, 11, parameter_name="Picture", texture=default_picture)
    mask_tex = g.node(unreal.MaterialExpressionTextureObjectParameter, 11, parameter_name="Mask", texture=default_mask,
                      sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    vein_tex = g.node(unreal.MaterialExpressionTextureObjectParameter, 11, parameter_name="Veins", texture=default_veins,
                      sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    corruption = g.scalar("Corruption", 0.0)
    light = g.scalar("Light", 0.0)
    shadow = g.scalar("Shadow", 0.0)
    twilight = g.scalar("Twilight", 0.0)
    vein_color = g.color("VeinColor", srgb("9B5DE5"))
    vein_strength = g.scalar("VeinStrength", 0.6)

    uv, u, v = uv_parts(g)
    sample = g.node(unreal.MaterialExpressionTextureSample, 9)
    g.link(picture, sample, "Tex")
    g.link(uv, sample, "UVs")
    color = g.mask(8, sample, r=True, g=True, b=True, source_output="RGB") if hasattr(g, "mask") else None
    if color is None:
        color = g.node(unreal.MaterialExpressionComponentMask, 8, r=True, g=True, b=True, a=False)
        g.link(sample, color, "", "RGB")
    alpha = g.channel(8, sample, "A")
    mask = g.node(unreal.MaterialExpressionTextureSample, 9, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    g.link(mask_tex, mask, "Tex")
    g.link(uv, mask, "UVs")
    # The render shades the mask a little, so its channels are cut clean
    panel = g.sharp(7, g.sub(8, g.channel(9, mask, "R"), g.const(0.25)), 4.0)
    gems = g.sharp(7, g.sub(8, g.channel(9, mask, "G"), g.const(0.5)), 5.0)

    lit = g.add(7, light, g.mul(7, twilight, g.one_minus(8, u)))
    dark = g.add(7, shadow, g.mul(7, twilight, u))

    # Shadow: drained to dark iron
    grey = g.op(unreal.MaterialExpressionDotProduct, 7, color, g.const3(rgb(0.3, 0.59, 0.11), 8))
    iron = g.mul(6, grey, g.const3(rgb(0.55, 0.52, 0.62), 7))
    color = g.lerp(5, color, g.mul(6, iron, g.const(0.75)), g.mul(6, dark, g.const(0.85)))
    # Light: a warm glow over everything
    color = g.add(5, color, g.mul(6, g.const3(rgb(0.35, 0.25, 0.12), 7), g.mul(6, lit, g.const(0.45))))
    # Gems turn violet on the way of shadow and glow with corruption
    glow = g.mul(5, vein_color, g.add(6, g.const(0.6), g.mul(6, corruption, g.const(2.0))))
    color = g.lerp(4, color, glow, g.mul(5, gems, dark))

    # Nid veins over the panel
    time = g.node(unreal.MaterialExpressionTime, 10)
    vs1 = g.node(unreal.MaterialExpressionTextureSample, 8, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    g.link(vein_tex, vs1, "Tex")
    g.link(uv, vs1, "UVs")
    vs2 = g.node(unreal.MaterialExpressionTextureSample, 8, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    g.link(vein_tex, vs2, "Tex")
    g.link(g.sub(9, g.const2(1.0, 1.0), uv), vs2, "UVs")
    vein1 = g.mul(6, g.channel(7, vs1, "R"), g.sharp(6, g.sub(7, corruption, g.channel(7, vs1, "G")), 25.0))
    second = g.sub(7, g.mul(7, corruption, g.const(2.0)), g.const(1.0))
    vein2 = g.mul(6, g.channel(7, vs2, "R"), g.sharp(6, g.sub(7, second, g.channel(7, vs2, "G")), 25.0))
    pulse = g.add(5, g.const(0.8), g.mul(5, g.sin(6, g.mul(6, time, g.const(2.5))), g.const(0.2)))
    veins = g.sat(4, g.mul(4, g.mul(5, g.add(5, vein1, vein2), g.add(5, g.const(1.0), g.mul(5, dark, g.const(0.6)))), pulse))
    color = g.lerp(3, color, g.mul(4, vein_color, g.const(1.2)), g.mul(4, g.mul(5, veins, panel), vein_strength))
    return finish(material, g, color, alpha)


# ---------------------------------------------------------------- Slot, medallion and badge

def build_slot():
    material, g = ui_material("M_HUD_Feld")
    if material is None:
        return load("M_HUD_Feld")

    bud = g.scalar("Bud", 0.0)
    round_ = g.scalar("Round", 0.0)
    fill_color = g.color("FillColor", srgb("221C17"))
    rim_color = g.color("RimColor", srgb("4A74A8"))
    rim_width = g.scalar("RimWidth", 0.07)
    tint_fill = g.scalar("TintFill", 1.0)
    tint_rim = g.scalar("TintRim", 1.0)

    uv, u, v = uv_parts(g)
    tint, tint_alpha = vertex_tint(g)
    px = g.sub(9, u, g.const(0.5))
    py = g.sub(9, v, g.const(0.5))
    # Rounded square; buds are rounded much more at the top
    r = g.lerp(8, g.const(0.14), g.const(0.42), g.mul(8, bud, g.sharp(8, g.sub(9, g.const(0.5), v), 1000.0)))
    half = g.sub(7, g.const(0.47), r)
    qx = g.sub(7, g.abs(8, px), half)
    qy = g.sub(7, g.abs(8, py), half)
    box_d = g.sub(5, g.add(6, g.dist(6, g.append(6, g.max(7, qx, g.const(0.0)), g.max(7, qy, g.const(0.0))), g.const2(0.0, 0.0)),
                         g.min(6, g.max(7, qx, qy), g.const(0.0))), r)
    circle_d = g.sub(6, g.dist(7, uv, g.const2(0.5, 0.5)), g.const(0.47))
    d = g.lerp(4, box_d, circle_d, round_)
    inside = g.sharp(4, g.mul(5, d, g.const(-1.0)), 200.0)
    half_rim = g.mul(5, rim_width, g.const(0.5))
    rim = g.mul(3, g.sat(4, g.sub(4, g.const(1.0), g.div(4, g.abs(5, g.add(5, d, half_rim)), half_rim))), inside)

    # Metal: brighter at the top, a highlight on round ones; the fill darkens towards the edge
    metal = g.mul(3, g.mul(4, rim_color, g.lerp(5, g.const(1.0), tint, tint_rim)), g.add(4, g.const(0.6), g.mul(4, g.one_minus(5, v), g.const(0.8))))
    metal = g.add(2, metal, g.mul(3, g.mul(4, g.sat(5, g.sub(5, g.const(1.0), g.mul(5, g.abs(6, g.sub(6, v, g.const(0.22))), g.const(6.0)))), round_), g.const(0.25)))
    fill = g.mul(3, g.mul(4, fill_color, g.lerp(5, g.const(1.0), tint, tint_fill)), g.sub(4, g.const(1.0), g.mul(4, g.dist(5, uv, g.const2(0.5, 0.5)), g.const(0.7))))
    color = g.lerp(2, fill, metal, rim)
    return finish(material, g, color, g.mul(2, inside, tint_alpha))


# ---------------------------------------------------------------- Cooldown wipe

def build_wipe():
    material, g = ui_material("M_HUD_Wisch")
    if material is None:
        return load("M_HUD_Wisch")

    remaining = g.scalar("Remaining", 0.0)
    uv, u, v = uv_parts(g)
    px = g.sub(9, u, g.const(0.5))
    py = g.sub(9, v, g.const(0.5))
    angle = g.op(unreal.MaterialExpressionArctangent2, 8, px, g.mul(9, py, g.const(-1.0)), a_input="Y", b_input="X")
    turn = g.frac(7, g.div(7, angle, g.const(2.0 * math.pi)))
    wiped = g.sharp(6, g.sub(6, remaining, turn), 200.0)
    inside = g.sharp(6, g.sub(7, g.const(0.5), g.max(7, g.abs(8, px), g.abs(8, py))), 200.0)
    opacity = g.mul(4, g.mul(5, wiped, inside), g.mul(5, g.sharp(6, remaining, 100.0), g.const(0.72)))
    return finish(material, g, g.const3(rgb(0.0, 0.0, 0.0), 4), opacity)


# ---------------------------------------------------------------- Experience: the tail of the Urdrachin

def build_tail():
    material, g = ui_material("M_HUD_Schwanz")
    if material is None:
        return load("M_HUD_Schwanz")

    fill = g.scalar("Fill", 0.4)
    scale_color = g.color("ScaleColor", srgb("8C6A3E"))
    dark_color = g.color("DarkColor", srgb("0E0C0A"))
    tick_color = g.color("TickColor", srgb("E9DFCC"))
    aspect = g.scalar("Aspect", 458.0 / 14.0)
    pixels = g.scalar("PixelWidth", 458.0)

    uv, u, v = uv_parts(g)
    across = g.abs(8, g.sub(9, v, g.const(0.5)))
    # The tail thins to a tip at the reached share
    behind = g.sat(7, g.div(7, g.sub(8, fill, u), g.const(0.05)))
    tail_h = g.mul(6, g.sqrt(7, behind), g.const(0.46))
    tail = g.mul(5, g.sharp(6, g.sub(7, tail_h, across), 40.0), g.sharp(6, g.sub(7, fill, u), 1000.0))
    groove = g.sharp(5, g.sub(6, g.const(0.32), across), 40.0)

    # Scales: a bulge per scale, darker towards the flanks
    scale = g.frac(7, g.mul(8, g.mul(8, u, aspect), g.const(1.2)))
    bulge = g.sin(6, g.mul(7, scale, g.const(math.pi)))
    shade = g.add(5, g.const(0.5), g.mul(5, g.mul(6, bulge, g.sub(6, g.const(1.0), g.mul(6, across, g.const(1.5)))), g.const(0.5)))

    # A fine line every 10 %
    tenth = g.frac(7, g.mul(8, u, g.const(10.0)))
    near = g.mul(6, g.min(7, tenth, g.one_minus(7, tenth)), g.const(0.1))
    tick = g.mul(5, g.sat(6, g.sub(6, g.const(1.5), g.mul(6, near, pixels))), groove)

    color = g.lerp(3, dark_color, g.mul(4, scale_color, shade), tail)
    color = g.lerp(2, color, tick_color, g.mul(3, tick, g.sub(3, g.const(0.7), g.mul(4, tail, g.const(0.35)))))
    return finish(material, g, color, g.max(2, g.mul(3, groove, g.const(0.92)), tail))


# ---------------------------------------------------------------- Menu windows

def build_window():
    material, g = ui_material("M_HUD_Fenster")
    if material is None:
        return load("M_HUD_Fenster")

    parchment = g.scalar("Parchment", 1.0)
    iron = g.scalar("Iron", 0.0)
    class_tint = g.scalar("ClassMaterial", 0.0)
    class_color = g.color("ClassColor", srgb("4A74A8"))
    gold = g.color("GoldColor", srgb("C9A443"))
    aspect = g.scalar("Aspect", 1.4)

    uv, u, v = uv_parts(g)
    noise = g.node(unreal.MaterialExpressionNoise, 8, scale=1.0, levels=4, noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX, output_min=0.0, output_max=1.0)
    g.link(g.append(9, g.mul(9, uv, g.const2(6.0, 6.0)), g.const(0.0)), noise, "")
    grain = g.add(7, g.const(0.82), g.mul(7, noise, g.const(0.3)))

    px = g.mul(9, g.sub(9, u, g.const(0.5)), aspect)
    py = g.sub(9, v, g.const(0.5))
    edge = g.min(7, g.sub(8, g.mul(8, aspect, g.const(0.5)), g.abs(8, px)), g.sub(8, g.const(0.5), g.abs(8, py)))   # distance to the edge, height units
    vignette = g.sat(6, g.add(6, g.const(0.55), g.mul(6, edge, g.const(4.0))))

    # Aged dark parchment (light text stays readable on it), dark iron, or the class material
    base = g.mul(5, g.const3(rgb(0.25, 0.18, 0.11), 6), parchment)
    base = g.add(5, base, g.mul(5, g.const3(rgb(0.07, 0.065, 0.07), 6), iron))
    base = g.add(5, base, g.mul(5, g.mul(6, class_color, g.const(0.18)), class_tint))
    base = g.mul(4, g.mul(5, base, grain), vignette)

    # Two edge lines and a scrolling filigree band in the head of the window
    line1 = g.sat(6, g.sub(6, g.const(1.0), g.mul(6, g.abs(7, g.sub(7, edge, g.const(0.012))), g.const(400.0))))
    line2 = g.sat(6, g.sub(6, g.const(1.0), g.mul(6, g.abs(7, g.sub(7, edge, g.const(0.03))), g.const(700.0))))
    scroll_y = g.add(7, g.const(0.06), g.mul(7, g.sin(8, g.mul(8, g.mul(9, u, aspect), g.const(9.0))), g.const(0.018)))
    scroll = g.mul(5, g.sat(6, g.sub(6, g.const(1.0), g.mul(6, g.abs(7, g.sub(7, v, scroll_y)), g.const(250.0)))), g.sharp(6, g.sub(7, g.const(0.1), v), 200.0))
    lines = g.sat(4, g.add(5, g.add(6, line1, line2), scroll))
    line_color = g.add(4, g.add(5, g.mul(5, gold, iron), g.mul(5, g.const3(rgb(0.09, 0.055, 0.03), 6), parchment)), g.mul(5, class_color, class_tint))
    color = g.lerp(3, base, line_color, lines)
    return finish(material, g, color, g.const(0.97, 3))


# ---------------------------------------------------------------- Imports

# Pictures to import again after a new render, names joined by commas in the environment variable VAEL_HUD_RENEW
RENEW = [name for name in os.environ.get("VAEL_HUD_RENEW", "").split(",") if name]


def import_files(folder, prefix_filter, destination, srgb_on=False, compressed=False):
    loaded = {}
    for file in sorted(os.listdir(folder)):
        if not file.lower().endswith(".png") or not file.startswith(prefix_filter):
            continue
        name = os.path.splitext(file)[0]
        path = "{}/{}".format(destination, name)
        if not assets.does_asset_exist(path) or name in RENEW:
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", os.path.join(folder, file))
            task.set_editor_property("destination_path", destination)
            task.set_editor_property("destination_name", name)
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("automated", True)
            task.set_editor_property("save", False)
            tools.import_asset_tasks([task])
            texture = unreal.load_asset(path)
            if texture is None:
                unreal.log_error("Vael HUD kit: could not import " + file)
                continue
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT if compressed else unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
            texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            texture.set_editor_property("srgb", srgb_on)
            texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
            texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
            assets.save_loaded_asset(texture, False)
            created.append(path)
        loaded[name] = unreal.load_asset(path)
    return loaded


def import_fonts():
    """Font face assets made in code (VaelScriptStatics): the import of a .ttf opens a dialog a script without a window can't show"""
    for file in sorted(os.listdir(FONTS)):
        if not file.lower().endswith(".ttf"):
            continue
        name = os.path.splitext(file)[0]
        path = "{}/{}".format(FONT_FOLDER, name)
        if assets.does_asset_exist(path):
            continue
        face = unreal.VaelScriptStatics.create_font_face_from_file(os.path.join(FONTS, file), path)
        if face is None:
            unreal.log_error("Vael HUD kit: could not make font " + file)
            continue
        # Kept inside the asset, so the font travels with the game
        try:
            face.set_editor_property("loading_policy", unreal.FontLoadingPolicy.INLINE)
        except Exception as error:
            unreal.log_warning("Vael HUD kit: loading policy of {} not set: {}".format(name, error))
        assets.save_loaded_asset(face, False)
        created.append(path)


# ---------------------------------------------------------------- Instances and data

def instance(parent, name, scalars=None, colors=None, textures=None):
    path = "{}/{}".format(HUD, name)
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    mi = tools.create_asset(name, HUD, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(mi, parent)
    for key, value in (scalars or {}).items():
        library.set_material_instance_scalar_parameter_value(mi, key, value)
    for key, value in (colors or {}).items():
        library.set_material_instance_vector_parameter_value(mi, key, value)
    for key, value in (textures or {}).items():
        library.set_material_instance_texture_parameter_value(mi, key, value)
    assets.save_loaded_asset(mi, False)
    created.append(path)
    return mi


# Look of each class: frame, vessel (mask, liquid range in the picture, colours), ornaments, abilities
B = unreal.VaelHUDButton
A = unreal.VaelHUDAbility
CLASSES = [
    {"name": "Magier", "title": "Magier", "resource": "Mana", "frame": "4A74A8", "accent": "D8C38E", "strip": unreal.VaelHUDStrip.ELEMENT_QUEUE,
     "vessel": {"LiquidBottom": 0.86, "LiquidTop": 0.43}, "liquid": "2C5F9E", "surface": "6E9BD6", "glass": "8A96A6", "bud": 0.0,
     "abilities": [("Feuer", "Feuer", B.CIRCLE, "1", A.ELEMENT_FIRE), ("Wasser", "Wasser", B.SQUARE, "2", A.ELEMENT_WATER), ("Erde", "Erde", B.CROSS, "3", A.ELEMENT_EARTH),
                   ("Luft", "Luft", B.TRIANGLE, "4", A.ELEMENT_AIR), ("Nid", "Nid", B.R1, "5", A.ELEMENT_NID), ("Wirken", "Wirken", B.R2, "LMT", A.CAST)]},
    {"name": "Tamer", "title": "Tamer", "resource": "Seele", "frame": "5B8F4E", "accent": "D9CFB4", "strip": unreal.VaelHUDStrip.NONE,
     "vessel": {"LiquidBottom": 0.9, "LiquidTop": 0.22}, "liquid": "4F9E5A", "surface": "93D18F", "glass": "D9CFB4", "bud": 0.0, "preview": True,
     "abilities": [("Befehl", "Tamer", B.R2, "LMT", A.PLACEHOLDER), ("Binden", "Tamer", B.CIRCLE, "1", A.PLACEHOLDER), ("Rufen", "Tamer", B.SQUARE, "2", A.PLACEHOLDER),
                   ("Biss", "Tamer", B.CROSS, "3", A.PLACEHOLDER), ("Seelenentladung", "Tamer", B.TRIANGLE, "4", A.PLACEHOLDER), ("Rudel", "Tamer", B.L1R1, "5", A.PLACEHOLDER)]},
    {"name": "Ingenieur", "title": "Ingenieur", "resource": "Munition", "frame": "B0574A", "accent": "C49A4A", "strip": unreal.VaelHUDStrip.NONE,
     "vessel": {"LiquidBottom": 0.84, "LiquidTop": 0.2}, "liquid": "B0402F", "surface": "E07A5C", "glass": "C49A4A", "bud": 0.0, "preview": True,
     "abilities": [("Schuss", "Ingenieur", B.R2, "LMT", A.PLACEHOLDER), ("Vorrichtung", "Ingenieur", B.CIRCLE, "1", A.PLACEHOLDER), ("Z\u00fcnden", "Ingenieur", B.SQUARE, "2", A.PLACEHOLDER),
                   ("Spezialmunition", "Ingenieur", B.CROSS, "3", A.PLACEHOLDER), ("Feldcrafting", "Ingenieur", B.TRIANGLE, "4", A.PLACEHOLDER), ("Salve", "Ingenieur", B.L1R1, "5", A.PLACEHOLDER)]},
    {"name": "Prophet", "title": "Prophet", "resource": "Verehrung / Furcht", "frame": "C9A443", "accent": "E9DFCC", "strip": unreal.VaelHUDStrip.NONE,
     "vessel": {"LiquidBottom": 0.82, "LiquidTop": 0.52, "TopChamberBottom": 0.48, "TopChamberTop": 0.18, "Split": 1.0},
     "liquid": "5B3D84", "liquid2": "E1BD52", "surface": "8A67B8", "surface2": "F5DE96", "glass": "C9A443", "bud": 0.0, "preview": True, "two": True,
     "abilities": [("Segen", "Prophet", B.R2, "LMT", A.PLACEHOLDER), ("Fluch", "Prophet", B.CIRCLE, "1", A.PLACEHOLDER), ("Weissagung", "Prophet", B.SQUARE, "2", A.PLACEHOLDER),
                   ("Licht", "Prophet", B.CROSS, "3", A.PLACEHOLDER), ("Schatten", "Prophet", B.TRIANGLE, "4", A.PLACEHOLDER), ("Gel\u00fcbde", "Prophet", B.L1R1, "5", A.PLACEHOLDER)]},
    {"name": "Floromant", "title": "Floromant", "resource": "Saft", "frame": "C97A9E", "accent": "4FB3A9", "strip": unreal.VaelHUDStrip.HAND_BELT,
     "vessel": {"LiquidBottom": 0.88, "LiquidTop": 0.34}, "liquid": "D0679A", "surface": "F0A5C6", "glass": "4FB3A9", "bud": 1.0,
     # Own controls since 2026-10-22 (UpdateFloromantHud.py changes an existing asset the same way)
     "abilities": [("Linke Hand", "Saat", B.L1, "LMT", A.HAND_LEFT), ("Rechte Hand", "Saat", B.R1, "RMT", A.HAND_RIGHT),
                   ("Dornenpeitsche", "Dornenpeitsche", B.SQUARE, "F", A.THORN_WHIP), ("Wurzelsprung", "Wurzelsprung", B.CROSS, "Leer", A.ROOT_LEAP),
                   ("Ruf des Gartens", "Wuchern", B.L2R2, "Q+E", A.OVERGROWTH), ("Keimpelz", "Saeen", B.TRIANGLE, "R", A.RELOAD)]},
]


def class_data(spec, shapes, symbols, vessel, frame, slot):
    name = spec["name"]
    vessel_colors = {"LiquidColor": srgb(spec["liquid"]), "SurfaceColor": srgb(spec["surface"]), "GlassColor": srgb(spec["glass"])}
    if spec.get("two"):
        vessel_colors["LiquidColor2"] = srgb(spec["liquid2"])
        vessel_colors["SurfaceColor2"] = srgb(spec["surface2"])
    vessel_mi = instance(vessel, "MI_HUD_Gefaess_" + name, spec["vessel"], vessel_colors, {"Mask": shapes["T_HUD_Gefaess_" + name]})
    frame_mi = instance(frame, "MI_HUD_Rahmen_" + name, None, {"FrameColor": srgb(spec["frame"]), "BaseColor": srgb("15120F")}, {"Veins": shapes["T_HUD_Adern"]})
    slot_mi = instance(slot, "MI_HUD_Feld_" + name, {"Bud": spec["bud"]}, {"RimColor": srgb(spec["frame"]), "FillColor": srgb("221C17")})

    path = "{}/DA_HUDKlasse_{}".format(CLASS_FOLDER, name)
    if assets.does_asset_exist(path):
        return
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.VaelHUDClassData)
    data = tools.create_asset("DA_HUDKlasse_" + name, CLASS_FOLDER, unreal.VaelHUDClassData, factory)
    data.set_editor_property("class_name", unreal.Text(spec["title"]))
    data.set_editor_property("resource_name", unreal.Text(spec["resource"]))
    data.set_editor_property("resource", unreal.VaelHUDResource.PREVIEW_TWO if spec.get("two") else unreal.VaelHUDResource.PREVIEW if spec.get("preview") else unreal.VaelHUDResource.MANA)
    frame_color = srgb(spec["frame"])
    data.set_editor_property("frame_color", frame_color)
    data.set_editor_property("accent_color", srgb(spec["accent"]))
    data.set_editor_property("frame_material", frame_mi)
    data.set_editor_property("vessel_material", vessel_mi)
    data.set_editor_property("slot_material", slot_mi)
    data.set_editor_property("corner_ornament", shapes["T_HUD_Ecke_" + name])
    data.set_editor_property("center_ornament", shapes["T_HUD_Mitte_" + name])
    data.set_editor_property("bar_end", shapes["T_HUD_Ende_" + name])
    data.set_editor_property("strip", spec["strip"])
    slots = []
    for label, icon, button, key, ability in spec["abilities"]:
        entry = unreal.VaelHUDAbilitySlot()
        entry.set_editor_property("name", unreal.Text(label))
        entry.set_editor_property("icon", symbols["T_Symbol_" + icon])
        entry.set_editor_property("button", button)
        entry.set_editor_property("keyboard_key", unreal.Text(key))
        entry.set_editor_property("ability", ability)
        slots.append(entry)
    data.set_editor_property("abilities", slots)
    assets.save_loaded_asset(data, False)
    created.append(path)


PAINTED_CLASSES = {
    "Magier": {"frame": "T_HUD_Gemalt_Rahmen_Magier", "vessel": "T_HUD_Gemalt_Gefaess_Magier", "slot": "T_HUD_Gemalt_Feld_Magier", "medallion": "T_HUD_Gemalt_Medaillon_Magier"},
    "Floromant": {"frame": "T_HUD_Gemalt_Rahmen_Floromant", "vessel": "T_HUD_Gemalt_Gefaess_Floromant", "slot": "T_HUD_Gemalt_Feld_Floromant", "medallion": "T_HUD_Gemalt_Medaillon_Floromant"},
}


def apply_painted(painted, painted_frame, veins):
    """Enters the painted pieces into the data of the classes that have them; only these fields are touched"""
    for name, pieces in PAINTED_CLASSES.items():
        data = unreal.load_asset("{}/DA_HUDKlasse_{}".format(CLASS_FOLDER, name))
        if data is None or pieces["frame"] not in painted:
            continue
        mi = instance(painted_frame, "MI_HUD_Rahmen_Bild_" + name, None, None,
                      {"Picture": painted[pieces["frame"]], "Mask": painted[pieces["frame"] + "_Maske"], "Veins": veins})
        data.set_editor_property("frame_picture", painted[pieces["frame"]])
        data.set_editor_property("frame_mask", painted[pieces["frame"] + "_Maske"])
        data.set_editor_property("frame_picture_material", mi)
        data.set_editor_property("vessel_picture", painted[pieces["vessel"]])
        data.set_editor_property("slot_picture", painted[pieces["slot"]])
        data.set_editor_property("medallion_picture", painted[pieces["medallion"]])
        data.set_editor_property("dragon_in_frame", name == "Magier")
        if name == "Magier":
            data.set_editor_property("dragon_picture", painted["T_HUD_Gemalt_Drache_Magier"])
            data.set_editor_property("dragon_tail_picture", painted["T_HUD_Gemalt_Schwanz_Magier"])
            data.set_editor_property("scales_picture", painted["T_HUD_Gemalt_Schuppen_Magier"])
        assets.save_loaded_asset(data, False)
        created.append("DA_HUDKlasse_{} (painted pieces)".format(name))


def widget_blueprint():
    path = HUD + "/WBP_SpielerHUD"
    if assets.does_asset_exist(path):
        return
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", unreal.VaelPlayerHUDWidget)
    blueprint = tools.create_asset("WBP_SpielerHUD", HUD, unreal.WidgetBlueprint, factory)
    if blueprint is None:
        unreal.log_error("Vael HUD kit: WBP_SpielerHUD could not be made")
        return
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assets.save_loaded_asset(blueprint, False)
    created.append(path)


def main():
    # Fonts: made by a game run with -VaelMakeFonts (font faces need the UI system, see VaelScriptStatics::MakeFontAssets)
    shapes = import_files(SHAPES, "T_HUD_", HUD + "/Formen")
    painted = {}
    if os.path.isdir(PAINTED):
        painted.update(import_files(PAINTED, "T_HUD_Gemalt_", HUD + "/Gemalt", srgb_on=True, compressed=True))
        # The masks of the frames are data, not colour
        for key in [k for k in painted if k.endswith("_Maske")]:
            painted[key].set_editor_property("srgb", False)
            assets.save_loaded_asset(painted[key], False)
    symbols = import_files(SHAPES, "T_Symbol_", HUD + "/Symbole")

    vessel = build_vessel(shapes["T_HUD_Gefaess_Leben"])
    frame = build_frame(shapes["T_HUD_Adern"])
    slot = build_slot()
    build_wipe()
    tail = build_tail()
    window = build_window()

    instance(vessel, "MI_HUD_Gefaess_Leben", {"LiquidBottom": 0.885, "LiquidTop": 0.25},
             {"LiquidColor": srgb("9C2A2A"), "SurfaceColor": srgb("D9574A"), "GlassColor": rgb(0.62, 0.64, 0.66)}, {"Mask": shapes["T_HUD_Gefaess_Leben"]})
    instance(slot, "MI_HUD_Medaillon", {"Round": 1.0, "RimWidth": 0.16, "TintFill": 0.0, "TintRim": 1.0}, {"FillColor": srgb("15120F"), "RimColor": rgb(0.9, 0.88, 0.82)})
    instance(slot, "MI_HUD_Abzeichen", {"Round": 1.0, "RimWidth": 0.1, "TintFill": 1.0, "TintRim": 0.0}, {"FillColor": srgb("E9DFCC"), "RimColor": srgb("6B5A48")})
    instance(tail, "MI_HUD_Schwanz")
    instance(window, "MI_HUD_Fenster_Pergament", {"Parchment": 1.0, "Iron": 0.0, "ClassMaterial": 0.0})
    instance(window, "MI_HUD_Fenster_Eisen", {"Parchment": 0.0, "Iron": 1.0, "ClassMaterial": 0.0})
    instance(window, "MI_HUD_Fenster_Klasse", {"Parchment": 0.0, "Iron": 0.0, "ClassMaterial": 1.0})

    for spec in CLASSES:
        class_data(spec, shapes, symbols, vessel, frame, slot)

    if painted:
        painted_frame = build_painted_frame(painted["T_HUD_Gemalt_Rahmen_Magier"], painted["T_HUD_Gemalt_Rahmen_Magier_Maske"], shapes["T_HUD_Adern"])
        apply_painted(painted, painted_frame, shapes["T_HUD_Adern"])

    widget_blueprint()
    assets.save_directory("/Game/Vael/UI")
    unreal.log("[Vael] HUD kit: {} made".format(len(created)) + ("" if not created else ": " + ", ".join(p.split("/")[-1] for p in created[:60])))


main()
