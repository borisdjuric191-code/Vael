# The HUD of the Floromant for its own controls (Pflanzenarm, 2026-10-22):
#
# - M_HUD_Pelz / MI_HUD_Pelz: the Keimpelz pelt in the moss groove of the panel (2026-11-04: rendered fur of
#   Tools/HUD/vael_keimpelz_pelz.py, strands draw back and grow out at the edge of the fill). Before: mossy strands with a ragged
#   top, dark cracked seeds in them; parameter Fill (0-1) lets the fur shrink to the left and grow back to the right, the
#   rest of the track stays a dark groove.
# - DA_HUDKlasse_Floromant: the ability slots and the strip follow the new controls (hands on L1 / R1, Dornenpeitsche,
#   Wurzelsprung, Ruf des Gartens on L2 + R2, Keimpelz on Triangle; the strip shows the seed belt of the hands). Boris asked
#   for this; the asset is only changed while it still shows the old controls (sowing on Square).
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/UpdateFloromantHud.py"

import os
import unreal

HUD = "/Game/Vael/UI/HUD"
CLASS_FOLDER = HUD + "/Klassen"
SYMBOLS = HUD + "/Symbole"
VERSION = 1.3

library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
done = []

B = unreal.VaelHUDButton
A = unreal.VaelHUDAbility
# Controls of 2026-11-04 (Boris): Square takes root, Cross crosses the hands, L2 / R2 grow towers (both: Ruf des Gartens),
# Triangle held reloads from the Keimpelz; the last field is the ink picture T_Faehigkeit_<name> (the hands show their plant)
ABILITIES = [
    ("Linke Hand", "Saat", B.L1, "LMT", A.HAND_LEFT, None),
    ("Rechte Hand", "Saat", B.R1, "RMT", A.HAND_RIGHT, None),
    ("Verwurzeln", "Wurzelsprung", B.SQUARE, "F", A.ROOT, "Verwurzeln"),
    ("Kreuzen", "Saat", B.CROSS, "Leer", A.CROSS, "Kreuzen"),
    ("Türme", "Wuchern", B.L2R2, "Q+E", A.TOWERS, "Tuerme"),
    ("Keimpelz", "Saeen", B.TRIANGLE, "R", A.RELOAD, "Keimpelz"),
]
PICTURES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Data", "HUD", "Faehigkeiten")
PICTURE_FOLDER = HUD + "/Faehigkeiten"


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


FUR_PICTURES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Data", "HUD", "Keimpelz")


def import_fur_texture(name, srgb):
    """The fur pictures of Tools/HUD/vael_keimpelz_pelz.py, uncompressed and without mips; the data picture is linear"""
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(FUR_PICTURES, name + ".png"))
    task.set_editor_property("destination_path", HUD)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(HUD + "/" + name)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("srgb", srgb)
    assets.save_loaded_asset(texture, False)
    done.append(HUD + "/" + name)
    return texture


def build_fur():
    """Boris 2026-11-04: a juicy dark green pelt lying in the moss groove like the one on her arms. The rendered fur shows
    strand by strand: near the edge of the fill each strand is shorter (it draws back into the ground, only stubs stay)
    and grows out again; Growing / Shed (set by the widget) light the edge while it fills or was just spent"""
    path = HUD + "/M_HUD_Pelz"
    picture = import_fur_texture("T_HUD_Keimpelz_Pelz", True)
    data = import_fur_texture("T_HUD_Keimpelz_Daten", False)
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        library.delete_all_material_expressions(material)
    else:
        material = tools.create_asset("M_HUD_Pelz", HUD, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    g = Graph(material)
    g.node(unreal.MaterialExpressionScalarParameter, 18, parameter_name="KitVersion", default_value=VERSION)
    fill = g.node(unreal.MaterialExpressionScalarParameter, 14, parameter_name="Fill", default_value=1.0)
    edge = g.node(unreal.MaterialExpressionScalarParameter, 14, parameter_name="Edge", default_value=0.08)
    growing = g.node(unreal.MaterialExpressionScalarParameter, 6, parameter_name="Growing", default_value=0.0)
    shed = g.node(unreal.MaterialExpressionScalarParameter, 6, parameter_name="Shed", default_value=0.0)
    brightness = g.node(unreal.MaterialExpressionScalarParameter, 6, parameter_name="Brightness", default_value=0.6)
    sway = g.node(unreal.MaterialExpressionScalarParameter, 16, parameter_name="Sway", default_value=0.0015)

    # The tips sway softly: the top of the picture moves sideways a little, slowly, in waves along the bar
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 18)
    u = g.mask(uv, 17, True, False)
    v = g.mask(uv, 17, False, True)
    time = g.node(unreal.MaterialExpressionTime, 18)
    wave = g.node(unreal.MaterialExpressionSine, 16, period=4.5)
    g.link(g.add(17, time, g.mul(17, u, g.const(12.0, 18))), wave)
    tips = g.sat(16, g.mul(16, g.sub(17, g.const(0.75, 17), v), g.const(2.0, 17)))
    offset = g.mul(15, g.mul(15, wave, sway), tips)
    shift = g.node(unreal.MaterialExpressionAppendVector, 14)
    g.link(offset, shift, "A")
    g.link(g.const(0.0, 15), shift, "B")
    swayed = g.add(13, uv, shift)

    fur = g.node(unreal.MaterialExpressionTextureSample, 12, texture=picture, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    g.link(swayed, fur, "UVs")
    info = g.node(unreal.MaterialExpressionTextureSample, 12, texture=data, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    g.link(swayed, info, "UVs")
    along = g.mask(info, 11, True, False)                    # place along the strand, 0 root, 1 tip
    place = g.node(unreal.MaterialExpressionComponentMask, 11, r=False, g=True, b=False, a=False)
    g.link(info, place)                                      # where the strand stands along the bar
    chance = g.node(unreal.MaterialExpressionComponentMask, 11, r=False, g=False, b=True, a=False)
    g.link(info, chance)                                     # its own random number

    # How much of each strand stands: whole well inside the fill, shorter towards its edge, a little uneven strand by strand.
    # The fill is stretched so that full and empty really are full and empty
    reach = g.sub(10, g.mul(11, fill, g.add(12, g.const(1.0, 13), g.mul(13, edge, g.const(2.0, 14)))), g.mul(11, edge, g.const(0.5, 12)))
    to_edge = g.sub(10, reach, place)
    local = g.sat(8, g.add(9, g.op(unreal.MaterialExpressionDivide, 9, to_edge, edge),
                           g.mul(9, g.sub(10, chance, g.const(0.5, 11)), g.const(0.6, 10))))
    shown = g.sharp(7, g.sub(8, g.mul(8, local, g.const(1.05, 9)), along), 20.0)
    stub = g.sharp(7, g.sub(8, g.const(0.14, 9), along), 30.0)
    visible = g.op(unreal.MaterialExpressionMax, 6, shown, stub)

    # Colour: the fur, darker where only the stubs are left
    rgb = g.node(unreal.MaterialExpressionComponentMask, 6, r=True, g=True, b=True, a=False)
    g.link(fur, rgb)
    alpha = g.node(unreal.MaterialExpressionComponentMask, 6, r=False, g=False, b=False, a=True)
    g.link(fur, alpha, "", "RGBA")
    base = g.mul(4, g.mul(5, rgb, brightness), g.lerp(5, g.const(0.35, 6), g.const(1.0, 6), shown))

    # The edge of the fill glows while it grows back (fresh sap green), and flashes pale when Keimpelz was just taken;
    # the tips glow more than the roots
    distance = g.node(unreal.MaterialExpressionAbs, 8)
    g.link(to_edge, distance)
    near = g.sat(6, g.sub(7, g.const(1.0, 8), g.op(unreal.MaterialExpressionDivide, 7, distance, g.mul(8, edge, g.const(1.4, 9)))))
    lit = g.mul(4, g.mul(5, near, shown), g.add(5, along, g.const(0.2, 6)))
    glow = g.add(3, g.mul(4, g.color((0.30, 0.75, 0.10), 5), growing), g.mul(4, g.color((0.55, 0.70, 0.35), 5), shed))
    color = g.add(2, base, g.mul(3, glow, lit))
    opacity = g.mul(2, alpha, visible)

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


def import_picture(name):
    """An ink picture of Tools/HUD/vael_pflanzen_tusche.py (argument Faehigkeiten), always imported anew"""
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(PICTURES, name + ".png"))
    task.set_editor_property("destination_path", PICTURE_FOLDER)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(PICTURE_FOLDER + "/" + name)
    if texture is None:
        log("NOTE could not import " + name)
        return None
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    assets.save_loaded_asset(texture, False)
    return texture


def update_class_data():
    path = CLASS_FOLDER + "/DA_HUDKlasse_Floromant"
    data = unreal.load_asset(path)
    if data is None:
        log("NOTE " + path + " is missing (CreateHudKit.py makes it)")
        return
    slots = []
    for label, icon, button, key, ability, picture in ABILITIES:
        entry = unreal.VaelHUDAbilitySlot()
        entry.set_editor_property("name", unreal.Text(label))
        entry.set_editor_property("icon", unreal.load_asset("{}/T_Symbol_{}".format(SYMBOLS, icon)))
        entry.set_editor_property("button", button)
        entry.set_editor_property("keyboard_key", unreal.Text(key))
        entry.set_editor_property("ability", ability)
        if picture:
            entry.set_editor_property("painting", import_picture("T_Faehigkeit_" + picture))
        slots.append(entry)
    data.set_editor_property("abilities", slots)
    data.set_editor_property("strip", unreal.VaelHUDStrip.HAND_BELT)
    assets.save_loaded_asset(data, False)
    done.append(path + " (controls of 2026-11-04 with ink pictures)")


fur_instance(build_fur())
update_class_data()
for line in done:
    log("made " + line)
log("done")
