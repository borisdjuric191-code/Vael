# The HUD of the Floromant for its own controls (Pflanzenarm, 2026-10-22):
#
# - M_HUD_Pelz / MI_HUD_Pelz: the furry green bar of the Keimpelz pelt over the top of the panel. Mossy strands with a ragged
#   top, dark cracked seeds in them; parameter Fill (0-1) lets the fur shrink to the left and grow back to the right, the
#   rest of the track stays a dark groove.
# - DA_HUDKlasse_Floromant: the ability slots and the strip follow the new controls (hands on L1 / R1, Dornenpeitsche,
#   Wurzelsprung, Ruf des Gartens on L2 + R2, Keimpelz on Triangle; the strip shows the seed belt of the hands). Boris asked
#   for this; the asset is only changed while it still shows the old controls (sowing on Square).
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/UpdateFloromantHud.py"

import unreal

HUD = "/Game/Vael/UI/HUD"
CLASS_FOLDER = HUD + "/Klassen"
SYMBOLS = HUD + "/Symbole"
VERSION = 1.2

library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
done = []

B = unreal.VaelHUDButton
A = unreal.VaelHUDAbility
ABILITIES = [
    ("Linke Hand", "Saat", B.L1, "LMT", A.HAND_LEFT),
    ("Rechte Hand", "Saat", B.R1, "RMT", A.HAND_RIGHT),
    ("Dornenpeitsche", "Dornenpeitsche", B.SQUARE, "F", A.THORN_WHIP),
    ("Wurzelsprung", "Wurzelsprung", B.CROSS, "Leer", A.ROOT_LEAP),
    ("Ruf des Gartens", "Wuchern", B.L2R2, "Q+E", A.OVERGROWTH),
    ("Keimpelz", "Saeen", B.TRIANGLE, "R", A.RELOAD),
]


def log(message):
    unreal.log_warning("[UpdateFloromantHud] " + message)


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

    def link(self, source, target, target_input="", source_output=""):
        if not library.connect_material_expressions(source, source_output, target, target_input):
            log("link failed: {} -> {}.{}".format(source.get_name(), target.get_name(), target_input))

    def const(self, value, column):
        return self.node(unreal.MaterialExpressionConstant, column, r=value)

    def color(self, rgb, column):
        return self.node(unreal.MaterialExpressionConstant3Vector, column, constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))

    def op(self, expression_class, column, a, b):
        expression = self.node(expression_class, column)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        return expression

    def mul(self, c, a, b): return self.op(unreal.MaterialExpressionMultiply, c, a, b)
    def add(self, c, a, b): return self.op(unreal.MaterialExpressionAdd, c, a, b)
    def sub(self, c, a, b): return self.op(unreal.MaterialExpressionSubtract, c, a, b)

    def sat(self, c, a):
        expression = self.node(unreal.MaterialExpressionSaturate, c)
        self.link(a, expression)
        return expression

    def sharp(self, c, a, k):
        """saturate(a * k): a soft step at 0"""
        return self.sat(c, self.mul(c, a, self.const(k, c + 1)))

    def lerp(self, c, a, b, alpha):
        expression = self.node(unreal.MaterialExpressionLinearInterpolate, c)
        self.link(a, expression, "A")
        self.link(b, expression, "B")
        self.link(alpha, expression, "Alpha")
        return expression

    def mask(self, source, c, r, g):
        expression = self.node(unreal.MaterialExpressionComponentMask, c, r=r, g=g, b=False, a=False)
        self.link(source, expression)
        return expression

    def noise(self, u, v, sx, sy, c):
        """Smooth noise 0..1 over the bar, stretched by sx along it and sy across it"""
        position = self.node(unreal.MaterialExpressionAppendVector, c + 1)
        self.link(self.mul(c + 2, u, self.const(sx, c + 3)), position, "A")
        across = self.node(unreal.MaterialExpressionAppendVector, c + 2)
        self.link(self.mul(c + 3, v, self.const(sy, c + 4)), across, "A")
        self.link(self.const(0.0, c + 3), across, "B")
        self.link(across, position, "B")
        expression = self.node(unreal.MaterialExpressionNoise, c, scale=1.0, quality=2, levels=3, output_min=0.0, output_max=1.0)
        self.link(position, expression)
        return expression


def build_fur():
    path = HUD + "/M_HUD_Pelz"
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        if library.get_material_default_scalar_parameter_value(material, "KitVersion") >= VERSION - 0.001:
            return material
        library.delete_all_material_expressions(material)
    else:
        material = tools.create_asset("M_HUD_Pelz", HUD, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    g = Graph(material)
    g.node(unreal.MaterialExpressionScalarParameter, 14, parameter_name="KitVersion", default_value=VERSION)
    fill = g.node(unreal.MaterialExpressionScalarParameter, 12, parameter_name="Fill", default_value=1.0)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 14)
    u = g.mask(uv, 13, True, False)
    v = g.mask(uv, 13, False, True)

    strands = g.noise(u, v, 160.0, 3.0, 9)
    tufts = g.noise(u, v, 18.0, 1.5, 9)
    specks = g.noise(u, v, 70.0, 9.0, 9)

    # The fur: a ragged top made of strands and tufts, a softer bottom
    top = g.add(7, g.const(0.04, 8), g.add(9, g.mul(10, strands, g.const(0.16, 11)), g.mul(10, tufts, g.const(0.1, 11))))
    bottom = g.sub(7, g.const(0.99, 8), g.mul(8, tufts, g.const(0.06, 9)))
    inside = g.mul(5, g.sharp(6, g.sub(7, v, top), 30.0), g.sharp(6, g.sub(7, bottom, v), 30.0))

    # Shrinks to the left: the right edge is ragged too
    edge = g.add(6, fill, g.mul(7, g.sub(8, strands, g.const(0.5, 9)), g.const(0.03, 8)))
    filled = g.sharp(5, g.sub(6, edge, u), 300.0)

    # Colour: dark moss below, light green tips, single strands lighter; dark cracked seeds in between
    dark = g.color((0.035, 0.09, 0.025), 8)
    light = g.color((0.30, 0.58, 0.12), 8)
    height = g.sat(6, g.sub(7, g.const(1.0, 8), v))
    shade = g.sat(5, g.mul(6, height, g.add(7, g.mul(8, strands, g.const(0.8, 9)), g.const(0.3, 8))))
    fur = g.lerp(4, dark, light, shade)
    seeds = g.sharp(5, g.sub(6, specks, g.const(0.8, 7)), 25.0)
    fur = g.lerp(3, fur, g.color((0.05, 0.035, 0.025), 4), seeds)

    # The empty groove where the fur has gone
    groove_inside = g.mul(5, g.sharp(6, g.sub(7, v, g.const(0.38, 8)), 40.0), g.sharp(6, g.sub(7, g.const(0.85, 8), v), 40.0))
    groove = g.color((0.035, 0.03, 0.025), 4)
    color = g.lerp(2, groove, fur, filled)
    opacity = g.lerp(2, g.mul(3, groove_inside, g.const(0.55, 4)), inside, g.mul(3, filled, inside))

    library.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    done.append(path)
    return material


def fur_instance(parent):
    path = HUD + "/MI_HUD_Pelz"
    if assets.does_asset_exist(path):
        return
    instance = tools.create_asset("MI_HUD_Pelz", HUD, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(instance, parent)
    assets.save_loaded_asset(instance, False)
    done.append(path)


def update_class_data():
    path = CLASS_FOLDER + "/DA_HUDKlasse_Floromant"
    data = unreal.load_asset(path)
    if data is None:
        log("NOTE " + path + " is missing (CreateHudKit.py makes it)")
        return
    if not any(entry.get_editor_property("ability") == A.SOW for entry in data.get_editor_property("abilities")):
        return
    slots = []
    for label, icon, button, key, ability in ABILITIES:
        entry = unreal.VaelHUDAbilitySlot()
        entry.set_editor_property("name", unreal.Text(label))
        entry.set_editor_property("icon", unreal.load_asset("{}/T_Symbol_{}".format(SYMBOLS, icon)))
        entry.set_editor_property("button", button)
        entry.set_editor_property("keyboard_key", unreal.Text(key))
        entry.set_editor_property("ability", ability)
        slots.append(entry)
    data.set_editor_property("abilities", slots)
    data.set_editor_property("strip", unreal.VaelHUDStrip.HAND_BELT)
    assets.save_loaded_asset(data, False)
    done.append(path + " (new controls)")


fur_instance(build_fur())
update_class_data()
for line in done:
    log("made " + line)
log("done")
