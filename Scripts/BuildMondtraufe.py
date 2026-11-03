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
import math
import os
import random

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MOOR = os.path.join(ROOT, "Blockout", "Vael_Wurzelforst_Blockout", "WF_Moor.json")
FOLDER = "/Game/Vael/Requisiten/Wurzelforst/Mondtraufe"
ARCH = "/Game/Vael/Requisiten/Wurzelforst/Wurzelbogen/SM_Wurzelbogen"
NOISE = "/Game/Vael/Effects/Feuer/T_Feuer_Rauschen"
TAG = "WF_Mondtraufe"
HIDDEN_TAG = "WF_MondtraufeVersteckt"
OUTLINER = "Wurzelforst/Mondtraufe"
TREE = "/Game/Vael/Requisiten/Wurzelforst/Mangrove/SM_Mangrove"
SEED = 41
THICK_LIMB = 60            # sampled points around a spot for it to count as a thick limb

# Small ones (Boris 2026-11-02): under root arches with their own pond along the moor banks; they only show how the land is
SMALL_COUNT = 8
SMALL_SPACING = 4000.0      # cm between two

# The great ones: one or two, from a giant mangrove over open water, three to four times bigger, pinker, harvestable
GREAT_COUNT = 2
GREAT_SPACING = 12000.0
GREAT_TREE_HEIGHT = 1400.0  # cm, the mangrove scaled to this (its own size): its long limb then ends 11 m up
GREAT_SET_BACK = 100.0      # cm the trunk's foot stands back from the bank
GREAT = {
    "harvestable": True,
    "bud_length": 420.0,
    "num_threads": 24,
    "thread_length": unreal.Vector2D(80.0, 400.0),
    "thread_spread": 26.0,
    "thread_thickness": 0.9,
    "drop_scale": 3.2,
    "curtain_width": 240.0,
    "ring_size": 6.0,
    "own_pond": False,
    "light_color": unreal.LinearColor(1.0, 0.55, 0.8, 1.0),
    "light_intensity": unreal.Vector2D(320.0, 90.0),
    "light_radius": 1600.0,
    "max_fallen_petals": 24,
    "petal_scale": 3.0,
    "stalk_extension": 90.0,
    "stalk_radius": 7.0,
}
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


def sample_points(mesh, wanted=30000):
    description = mesh.get_static_mesh_description(0)
    count = description.get_vertex_count()
    step = max(1, count // wanted)
    return [description.get_vertex_position(unreal.VertexID(i)) for i in range(0, count, step)]


def measure_arch():
    """The point under the arch's crown: the middle of its highest part, at the underside there"""
    mesh = unreal.load_asset(ARCH)
    points = sample_points(mesh)
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


def measure_tree():
    """On the mangrove: the foot of its trunk and the far end of its longest overhanging limb, where the great bud hangs"""
    mesh = unreal.load_asset(TREE)
    points = sample_points(mesh, 60000)
    box = mesh.get_bounding_box()
    height = box.max.z - box.min.z
    foot = [p for p in points if p.z < box.min.z + height * 0.08]
    fx = sum(p.x for p in foot) / len(foot)
    fy = sum(p.y for p in foot) / len(foot)
    # Only thick limbs can carry it: points in cells of 25 cm that hold many points (a twig tip holds few)
    cells = {}
    for p in points:
        key = (int(p.x // 25), int(p.y // 25), int(p.z // 25))
        cells[key] = cells.get(key, 0) + 1
    thick = lambda p: sum(cells.get((int(p.x // 25) + i, int(p.y // 25) + j, int(p.z // 25) + k), 0) for i in (-1, 0, 1) for j in (-1, 0, 1) for k in (-1, 0, 1))
    limbs = [p for p in points if box.min.z + height * 0.5 < p.z < box.min.z + height * 0.8 and thick(p) >= THICK_LIMB]
    tip = max(limbs, key=lambda p: (p.x - fx) ** 2 + (p.y - fy) ** 2)
    reach = math.hypot(tip.x - fx, tip.y - fy)
    log("tree {:.0f} x {:.0f} x {:.0f} cm, foot ({:.0f} {:.0f}), limb end ({:.0f} {:.0f} {:.0f}), reach {:.0f}".format(
        box.max.x - box.min.x, box.max.y - box.min.y, height, fx, fy, tip.x, tip.y, tip.z, reach))
    return unreal.Vector(fx, fy, box.min.z), unreal.Vector(tip.x, tip.y, tip.z - 8.0), height


def turned(vector, yaw):
    """A vector turned around Z by yaw degrees"""
    a = math.radians(yaw)
    return unreal.Vector(vector.x * math.cos(a) - vector.y * math.sin(a), vector.x * math.sin(a) + vector.y * math.cos(a), vector.z)


def spread(points, count, spacing, taken=()):
    rng = random.Random(SEED)
    points = list(points)
    rng.shuffle(points)
    chosen = []
    for p in points:
        if all((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 > spacing ** 2 for q in list(chosen) + list(taken)):
            chosen.append(p)
        if len(chosen) >= count:
            break
    return chosen


def hide_in_pond(centre, radius, kept):
    """Rocks and props standing in a new pond spoil it: they are hidden (never deleted) and tagged, so a rerun shows them again"""
    hidden = 0
    for actor in actors.get_all_level_actors():
        if not isinstance(actor, unreal.StaticMeshActor) or actor in kept:
            continue
        origin, extent = actor.get_actor_bounds(False)
        if max(extent.x, extent.y, extent.z) > 400.0:
            continue
        if (origin.x - centre.x) ** 2 + (origin.y - centre.y) ** 2 < (radius + max(extent.x, extent.y) * 0.5) ** 2 and abs(origin.z - centre.z) < 400.0:
            actor.set_actor_hidden_in_game(True)
            actor.set_is_temporarily_hidden_in_editor(True)
            actor.set_actor_enable_collision(False)
            actor.tags = list(actor.tags) + [unreal.Name(HIDDEN_TAG)]
            hidden += 1
    return hidden


def show_hidden():
    for actor in actors.get_all_level_actors():
        if unreal.Name(HIDDEN_TAG) in actor.tags:
            actor.set_actor_hidden_in_game(False)
            actor.set_is_temporarily_hidden_in_editor(False)
            actor.set_actor_enable_collision(True)
            actor.tags = [t for t in actor.tags if t != unreal.Name(HIDDEN_TAG)]


def spawn(location, label):
    plant = actors.spawn_actor_from_class(unreal.VaelMoonEave, location, unreal.Rotator(0.0, 0.0, 0.0))
    plant.set_actor_label(label)
    plant.tags = [unreal.Name(TAG)]
    plant.set_folder_path(OUTLINER)
    return plant


def place(arch_point, tree_foot, tree_tip, tree_height):
    moor = json.load(open(MOOR, encoding="utf-8"))
    banks = [tuple(p[:3]) for p in moor["ufer"]]
    water = moor.get("wasserspiegel_cm", 0.0)
    lilies = [tuple(p[:2]) for p in moor["seerosen"]]

    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    for actor in actors.get_all_level_actors():
        if unreal.Name(TAG) in actor.tags:
            actors.destroy_actor(actor)
    show_hidden()

    # The great ones first: banks with open water before them (lily pads within reach), far apart
    open_banks = []
    for bank in banks:
        near = min(lilies, key=lambda l: (l[0] - bank[0]) ** 2 + (l[1] - bank[1]) ** 2)
        d = math.hypot(near[0] - bank[0], near[1] - bank[1])
        if 600.0 < d < 2500.0:
            open_banks.append((bank, near))
    great = spread([b for b, _ in open_banks], GREAT_COUNT, GREAT_SPACING)
    scale = GREAT_TREE_HEIGHT / tree_height
    limb = math.atan2(tree_tip.y - tree_foot.y, tree_tip.x - tree_foot.x)
    placed_great = []
    for index, bank in enumerate(great):
        near = next(l for b, l in open_banks if b == bank)
        towards = math.atan2(near[1] - bank[1], near[0] - bank[0])
        yaw = math.degrees(towards - limb)
        # The trunk stands a little back from the bank, its limb reaching out over the water
        back = unreal.Vector(-math.cos(towards) * GREAT_SET_BACK, -math.sin(towards) * GREAT_SET_BACK, -30.0)
        foot = turned(unreal.Vector(tree_foot.x * scale, tree_foot.y * scale, tree_foot.z * scale), yaw)
        offset = unreal.Vector(back.x - foot.x, back.y - foot.y, back.z - foot.z)
        plant = spawn(unreal.Vector(bank[0], bank[1], bank[2]), "Mondtraufe_Gross_{}".format(index + 1))
        for key, value in GREAT.items():
            plant.set_editor_property(key, value)
        plant.set_editor_property("arch_mesh", unreal.load_asset(TREE))
        plant.set_editor_property("arch_height", GREAT_TREE_HEIGHT)
        plant.set_editor_property("hang_point", tree_tip)
        plant.set_editor_property("arch_yaw", yaw)
        plant.set_editor_property("arch_offset", offset)
        plant.set_editor_property("water_level", water - bank[2] + 2.0)
        placed_great.append(bank)
        hang = turned(unreal.Vector(tree_tip.x * scale, tree_tip.y * scale, tree_tip.z * scale), yaw)
        log("great Mondtraufe {} at {:.0f} {:.0f} (VaelTeleport {:.0f} {:.0f}), bud {:.0f} cm out over the water, {:.0f} cm above it".format(
            index + 1, bank[0], bank[1], bank[0] / 100.0, bank[1] / 100.0, math.hypot(offset.x + hang.x, offset.y + hang.y),
            bank[2] + offset.z + hang.z - water))

    # The small ones under their arches, each with its pond, spread along the banks
    small = spread(banks, SMALL_COUNT, SMALL_SPACING, taken=placed_great)
    for index, bank in enumerate(small):
        plant = spawn(unreal.Vector(bank[0], bank[1], bank[2]), "Mondtraufe_{}".format(index + 1))
        plant.set_editor_property("hang_point", arch_point)
        # The arch is scaled so the bud tip hangs about 1.8 m above its pond (the crown is thick: its underside is at 71 % of its height)
        plant.set_editor_property("arch_height", 420.0)
        under = unreal.Vector(bank[0] + arch_point.x * 0.7, bank[1] + arch_point.y * 0.7, bank[2])
        hidden = hide_in_pond(under, plant.get_editor_property("pond_radius"), [plant])
        log("Mondtraufe {} at {:.0f} {:.0f} (VaelTeleport {:.0f} {:.0f}), {} props hidden in its pond".format(
            index + 1, bank[0], bank[1], bank[0] / 100.0, bank[1] / 100.0, hidden))

    unreal.EditorLoadingAndSavingUtils.save_current_level()


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
    foot, tip, height = measure_tree()
    place(measure_arch(), foot, tip, height)
    for line in notes:
        log("NOTE " + line)
    log("done, {} notes".format(len(notes)))


main()
