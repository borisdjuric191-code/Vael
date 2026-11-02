# Mondtraufe (Boris 2026-11-02): builds the materials of the plant and puts one AVaelMoonEave on a bank of the Pfuhlmoor in
# L_Wurzelforst_Blockout, for a first look. Sowing by the Floromant, harvesting Mondtropfen and the light wall come later.
#
# - M_WF_Mondtraufe: the Tripo picture of the bud; the tip glows from inside in the pearly moonlight (Glut, Leuchten),
#   and with "Welke" (set by the game from the region's corruption) it pales, browns and its glow fades.
# - M_Mondtraufe_Faden: the thin glowing threads. M_Mondtraufe_Vorhang: light running down like water (unlit, additive,
#   streaks of the fire noise drifting down, soft at its sides and ends). M_Mondtraufe_Ring: the bright ring on the water.
#   All three have "Staerke", set by the game for night and wilt.
# - M_Mondtraufe_Blatt: a fallen petal, wine red at its base through rose to pearl at its tip, fine veins.
# - Measures on SM_Wurzelbogen the point under its crown where the stalk hangs (HangPoint of the actor).
# Materials are rebuilt when older than KIT_VERSION. The placed plant is tagged WF_Mondtraufe; rerunning replaces it.
#
# Run with the editor closed, after the Kreaturenfabrik import of Vael_Quellen/Mondtraufe and CreateLootKit.py:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildMondtraufe.py"

import json
import os

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MOOR = os.path.join(ROOT, "Blockout", "Vael_Wurzelforst_Blockout", "WF_Moor.json")
FOLDER = "/Game/Vael/Requisiten/Wurzelforst/Mondtraufe"
ARCH = "/Game/Vael/Requisiten/Wurzelforst/Wurzelbogen/SM_Wurzelbogen"
NOISE = "/Game/Vael/Effects/Feuer/T_Feuer_Rauschen"
TAG = "WF_Mondtraufe"
OUTLINER = "Wurzelforst/Mondtraufe"
KIT_VERSION = 1.3

library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
notes = []


def log(message):
    unreal.log_warning("[BuildMondtraufe] " + message)


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

    def color(self, rgb, column):
        return self.node(unreal.MaterialExpressionConstant3Vector, column, constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))

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
    def abs_(self, c, a): return self.one(unreal.MaterialExpressionAbs, c, a)
    def sin(self, c, a): return self.one(unreal.MaterialExpressionSine, c, a, period=6.283185)

    def power(self, c, a, exponent):
        expression = self.node(unreal.MaterialExpressionPower, c)
        self.link(a, expression, "Base")
        self.link(self.const(exponent, c + 1), expression, "Exp")
        return expression

    def ramp(self, c, x, low, high):
        return self.sat(c, self.mul(c + 1, self.sub(c + 2, x, self.const(low, c + 3)), self.const(1.0 / (high - low), c + 2)))

    def lerp(self, c, a, b, alpha):
        expression = self.node(unreal.MaterialExpressionLinearInterpolate, c)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        self.link(alpha, expression, "Alpha")
        return expression

    def sample(self, texture, uv, c, normal=False, linear=False, masks=False):
        sampler = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS if masks else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR if linear else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
        expression = self.node(unreal.MaterialExpressionTextureSample, c, texture=texture, sampler_type=sampler)
        if uv is not None:
            self.link(uv, expression, "UVs")
        return expression


def new_material(name):
    path = FOLDER + "/" + name
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        if "KitVersion" in [str(p) for p in library.get_scalar_parameter_names(material)] and \
                library.get_material_default_scalar_parameter_value(material, "KitVersion") >= KIT_VERSION - 0.001:
            return None
        library.delete_all_material_expressions(material)
        for expression in list(unreal.ObjectIterator(unreal.MaterialExpression)):
            if expression.get_outer() == material:
                library.delete_material_expression(material, expression)
        return material
    return tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def finish(g, slots):
    g.scalar("KitVersion", KIT_VERSION, 30)
    for prop, node in slots.items():
        library.connect_material_property(node, "", prop)
    library.recompile_material(g.material)
    assets.save_loaded_asset(g.material, False)
    log("made " + g.material.get_path_name())


def texture(kind):
    loaded = unreal.load_asset("{}/T_Mondtraufe_{}".format(FOLDER, kind))
    if loaded is None:
        notes.append("missing texture T_Mondtraufe_" + kind)
    return loaded


def unlit_additive(material):
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)


P = unreal.MaterialProperty
MOON = (1.0, 0.82, 0.9)


def build_bud():
    material = new_material("M_WF_Mondtraufe")
    if material is None:
        return
    material.set_editor_property("two_sided", True)
    g = Graph(material)
    picture = g.mask(g.sample(texture("basecolor"), None, 14), 13, "RGB")
    normal = g.sample(texture("normal"), None, 6, normal=True)
    roughness = g.mask(g.sample(texture("roughness"), None, 8, linear=True), 7, "R")

    leuchten = g.scalar("Leuchten", 1.0, 16)
    welke = g.scalar("Welke", 0.0, 16)
    glut = g.vector("Glut", MOON, 16)
    staerke = g.scalar("GlutStaerke", 2.5, 16)

    # Wilting: the petals pale and brown, they lose their color first
    grey = g.node(unreal.MaterialExpressionDesaturation, 11)
    g.link(picture, grey)
    g.link(g.const(1.0, 12), grey, "Fraction")
    withered = g.mul(10, grey, g.color((0.62, 0.48, 0.38), 11))
    base = g.lerp(8, picture, withered, g.mul(9, welke, g.const(0.75, 10)))

    # The tip glows from inside: strongest at the very tip (the model's foot), fading up the bud
    height = g.mask(g.node(unreal.MaterialExpressionLocalPosition, 14), 13, "B")
    tip = g.power(10, g.sat(11, g.sub(12, g.const(1.0, 13), g.mul(13, height, g.const(1.0 / 45.0, 14)))), 1.8)
    lit = g.mul(8, g.mul(9, g.mul(10, picture, glut), g.mul(10, tip, staerke)), g.mul(9, leuchten, g.sub(10, g.const(1.0, 11), g.mul(11, welke, g.const(0.6, 12)))))
    finish(g, {P.MP_BASE_COLOR: base, P.MP_NORMAL: normal, P.MP_ROUGHNESS: roughness, P.MP_EMISSIVE_COLOR: lit})


def build_thread():
    material = new_material("M_Mondtraufe_Faden")
    if material is None:
        return
    g = Graph(material)
    farbe = g.vector("Farbe", MOON, 8)
    glow = g.mul(4, g.mul(5, farbe, g.scalar("Staerke", 1.0, 6)), g.const(5.0, 5))
    finish(g, {P.MP_BASE_COLOR: farbe, P.MP_EMISSIVE_COLOR: glow, P.MP_ROUGHNESS: g.const(0.3, 4)})


def build_curtain():
    material = new_material("M_Mondtraufe_Vorhang")
    if material is None:
        return
    unlit_additive(material)
    g = Graph(material)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 20)
    u = g.mask(uv, 19, "R")
    v = g.mask(uv, 19, "G")
    time = g.node(unreal.MaterialExpressionTime, 20)
    fliessen = g.scalar("Fliessen", 0.35, 20)
    noise = unreal.load_asset(NOISE)

    # Streaks: thin in width, long along the fall, drifting along v; two layers at different speeds
    drift = g.mul(16, time, fliessen)
    first = g.append(15, g.mul(16, u, g.const(7.0, 17)), g.add(16, g.mul(17, v, g.const(0.7, 18)), drift))
    second = g.append(15, g.mul(16, u, g.const(13.0, 17)), g.add(16, g.mul(17, v, g.const(1.1, 18)), g.mul(17, drift, g.const(1.6, 18))))
    streak = g.mul(11, g.mask(g.sample(noise, first, 13, masks=True), 12, "R"), g.mask(g.sample(noise, second, 13, masks=True), 12, "R"))
    streaks = g.power(9, g.mul(10, streak, g.const(2.6, 11)), 3.2)

    # Soft at its sides and at both ends, so it has no edges
    sides = g.power(9, g.sin(10, g.mul(11, u, g.const(0.5, 12))), 1.4)
    ends = g.power(9, g.sin(10, g.mul(11, v, g.const(0.5, 12))), 0.6)
    shape = g.mul(7, g.mul(8, sides, ends), streaks)
    glow = g.mul(4, g.mul(5, g.vector("Farbe", MOON, 7), shape), g.mul(5, g.scalar("Staerke", 1.0, 7), g.const(0.6, 6)))
    finish(g, {P.MP_EMISSIVE_COLOR: glow})


def build_ring():
    material = new_material("M_Mondtraufe_Ring")
    if material is None:
        return
    unlit_additive(material)
    g = Graph(material)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 20)
    centred = g.mul(17, g.sub(18, uv, g.node(unreal.MaterialExpressionConstant2Vector, 19, r=0.5, g=0.5)), g.const(2.0, 18))
    distance = g.one(unreal.MaterialExpressionLength, 16, centred)

    # A bright ring where the light meets the water, a soft glow inside, breathing slowly
    band = g.sat(13, g.sub(14, g.const(1.0, 15), g.mul(15, g.abs_(16, g.sub(17, distance, g.const(0.62, 18))), g.const(9.0, 16))))
    inner = g.sat(13, g.sub(14, g.const(1.0, 15), g.mul(15, distance, g.const(1.7, 16))))
    breath = g.add(10, g.const(0.8, 11), g.mul(11, g.sin(12, g.mul(13, g.node(unreal.MaterialExpressionTime, 14), g.const(0.25, 14))), g.const(0.2, 12)))
    shape = g.mul(8, g.add(9, g.power(10, band, 2.0), g.mul(10, inner, g.const(0.35, 11))), breath)
    glow = g.mul(4, g.mul(5, g.vector("Farbe", MOON, 7), shape), g.mul(5, g.scalar("Staerke", 1.0, 7), g.const(1.4, 6)))
    finish(g, {P.MP_EMISSIVE_COLOR: glow})


def build_petal():
    material = new_material("M_Mondtraufe_Blatt")
    if material is None:
        return
    material.set_editor_property("two_sided", True)
    g = Graph(material)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 20)
    v = g.mask(uv, 19, "G")
    u = g.mask(uv, 19, "R")
    # Wine at the base, rose in the middle, pearl at the tip, like the petals of the bud
    lower = g.lerp(12, g.color((0.32, 0.03, 0.1), 13), g.color((0.78, 0.22, 0.48), 13), g.ramp(14, v, 0.0, 0.55))
    colour = g.lerp(10, lower, g.color((0.93, 0.74, 0.84), 11), g.ramp(12, v, 0.5, 1.0))
    # Fine darker veins along the petal
    veins = g.ramp(10, g.sin(11, g.mul(12, u, g.const(9.0, 13))), 0.75, 1.0)
    base = g.lerp(6, colour, g.mul(7, colour, g.const(0.7, 8)), g.mul(7, veins, g.const(0.5, 8)))
    finish(g, {P.MP_BASE_COLOR: base, P.MP_ROUGHNESS: g.const(0.45, 5)})


def measure_arch():
    """The point under the arch's crown: the middle of its highest part, at the underside there"""
    mesh = unreal.load_asset(ARCH)
    description = mesh.get_static_mesh_description(0)
    count = description.get_vertex_count()
    step = max(1, count // 30000)
    points = [description.get_vertex_position(unreal.VertexID(i)) for i in range(0, count, step)]
    top = max(p.z for p in points)
    crown = [p for p in points if p.z > top * 0.93]
    cx = sum(p.x for p in crown) / len(crown)
    cy = sum(p.y for p in crown) / len(crown)
    column = [p for p in points if abs(p.x - cx) < 12.0 and abs(p.y - cy) < 12.0 and p.z > top * 0.5]
    underside = min(p.z for p in column) if column else top * 0.9
    box = mesh.get_bounding_box()
    log("arch {:.0f} x {:.0f} x {:.0f} cm, crown at ({:.0f} {:.0f}), underside {:.0f}, top {:.0f}".format(
        box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z, cx, cy, underside, top))
    return unreal.Vector(cx, cy, underside + 3.0)


def place(hang_point):
    moor = json.load(open(MOOR, encoding="utf-8"))
    banks = moor["ufer"]
    # A quiet bank: the one farthest from the moor's middle, so the plant stands apart
    mx = sum(p[0] for p in banks) / len(banks)
    my = sum(p[1] for p in banks) / len(banks)
    spot = max(banks, key=lambda p: (p[0] - mx) ** 2 + (p[1] - my) ** 2)

    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    for actor in actors.get_all_level_actors():
        if unreal.Name(TAG) in actor.tags:
            actors.destroy_actor(actor)

    plant = actors.spawn_actor_from_class(unreal.VaelMoonEave, unreal.Vector(spot[0], spot[1], spot[2]), unreal.Rotator(0.0, 0.0, 0.0))
    plant.set_editor_property("hang_point", hang_point)
    plant.set_actor_label("Mondtraufe")
    plant.tags = [unreal.Name(TAG)]
    plant.set_folder_path(OUTLINER)
    # The arch is scaled so the bud tip hangs about 1.8 m above the water (the crown is thick: its underside is at 71 % of its height)
    plant.set_editor_property("arch_height", 420.0)
    unreal.EditorLoadingAndSavingUtils.save_current_level()
    log("placed at {:.0f} {:.0f} {:.0f} cm (VaelTeleport {:.0f} {:.0f})".format(spot[0], spot[1], spot[2], spot[0] / 100.0, spot[1] / 100.0))


def main():
    build_bud()
    build_thread()
    build_curtain()
    build_ring()
    build_petal()
    petal = unreal.load_asset("/Game/Vael/Items/Loot/Modelle/SM_Mondtraufe_Blatt")
    if petal is not None:
        petal.set_material(0, unreal.load_asset(FOLDER + "/M_Mondtraufe_Blatt"))
        assets.save_loaded_asset(petal, False)
    place(measure_arch())
    for line in notes:
        log("NOTE " + line)
    log("done, {} notes".format(len(notes)))


main()
