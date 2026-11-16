# Imports the engineer's shots (Tools/Ingenieur/vael_ingenieur_ladungen.py, goal 16.11.) to /Game/Vael/Characters/Ingenieur/Ladungen
# and makes their own materials: the core that glows in the colour of the formula (M_Ing_Ladung_Kern: Farbe, Glut), the latex
# that the filling tints (M_Ing_Latex: Farbe), sealing wax, quill, soot feather, and the streak behind a shot in flight
# (M_Ing_Spur: additive, unlit, Farbe, Staerke, fading from head to tail). The rest comes from the Glutkammer kit.
# Idempotent: materials are rebuilt only when their KitVersion is older than KIT_VERSION.
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ImportEngineerShots.py" -unattended -nosplash
import os

import unreal

SOURCE = "C:/Projekte/Vael_Quellen/Ingenieur/export"
FOLDER = "/Game/Vael/Characters/Ingenieur/Ladungen"
KIT = "/Game/Vael/Refugium/Glutkammer/Probe"
KIT_VERSION = 1.0
PIECES = ["SM_Ing_Ladung_Schlacke", "SM_Ing_Ladung_Blasenhaut", "SM_Ing_Ladung_Latex", "SM_Ing_Ladung_Glas", "SM_Ing_Ladung_Kiel",
          "SM_Ing_Stein", "SM_Ing_Spur"]

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
library = unreal.MaterialEditingLibrary


def log(message):
    unreal.log_warning("[ImportEngineerShots] " + message)


class Graph:
    def __init__(self, material):
        self.m = material
        self.row = 0

    def node(self, cls, column, **props):
        self.row += 1
        n = library.create_material_expression(self.m, cls, -260 * column, 130 * (self.row % 14))
        for key, value in props.items():
            n.set_editor_property(key, value)
        return n

    def const(self, value, column=6):
        return self.node(unreal.MaterialExpressionConstant, column, r=value)

    def scalar(self, name, value, column=6):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=value)

    def color(self, name, rgb, column=6):
        return self.node(unreal.MaterialExpressionVectorParameter, column, parameter_name=name, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))

    def op(self, cls, column, a, b=None, a_input="A", b_input="B"):
        n = self.node(cls, column)
        library.connect_material_expressions(a, "", n, a_input)
        if b is not None:
            library.connect_material_expressions(b, "", n, b_input)
        return n

    def mul(self, c, a, b):
        return self.op(unreal.MaterialExpressionMultiply, c, a, b)

    def add(self, c, a, b):
        return self.op(unreal.MaterialExpressionAdd, c, a, b)

    def one_minus(self, c, a):
        return self.op(unreal.MaterialExpressionOneMinus, c, a, a_input="")

    def power(self, c, a, e):
        return self.op(unreal.MaterialExpressionPower, c, a, e, a_input="Base", b_input="Exp")

    def mask(self, c, a, r=True, g=False):
        n = self.node(unreal.MaterialExpressionComponentMask, c, r=r, g=g, b=False, a=False)
        library.connect_material_expressions(a, "", n, "")
        return n

    def out(self, expression, prop):
        library.connect_material_property(expression, "", prop)


def new_material(name):
    path = "{}/{}".format(FOLDER, name)
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        if library.get_material_default_scalar_parameter_value(material, "KitVersion") >= KIT_VERSION - 0.001:
            return None
        library.delete_all_material_expressions(material)
        return material
    return tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def finish(material, g):
    g.scalar("KitVersion", KIT_VERSION, 12)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def plain(name, rgb, roughness, metallic=0.0, specular=0.5):
    material = new_material(name)
    if material is None:
        return unreal.load_asset("{}/{}".format(FOLDER, name))
    g = Graph(material)
    g.out(g.mul(2, g.color("Farbe", rgb, 3), g.node(unreal.MaterialExpressionNoise, 3, scale=0.6, output_min=0.8, output_max=1.05)), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.const(roughness), unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.const(metallic), unreal.MaterialProperty.MP_METALLIC)
    g.out(g.const(specular), unreal.MaterialProperty.MP_SPECULAR)
    return finish(material, g)


def core():
    """The core of a formula: its colour, glowing; Glut 0 is a dull filling (water, glue), 3 a bright one (light, fire)"""
    material = new_material("M_Ing_Ladung_Kern")
    if material is None:
        return unreal.load_asset(FOLDER + "/M_Ing_Ladung_Kern")
    g = Graph(material)
    colour = g.color("Farbe", (1.0, 0.45, 0.1), 4)
    glow = g.scalar("Glut", 1.5, 4)
    flicker = g.node(unreal.MaterialExpressionNoise, 4, scale=2.5, output_min=0.75, output_max=1.2)
    g.out(g.mul(2, colour, g.const(0.35, 3)), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.mul(1, g.mul(2, colour, glow), g.mul(3, flicker, g.const(2.0, 4))), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(g.const(0.35), unreal.MaterialProperty.MP_ROUGHNESS)
    return finish(material, g)


def latex():
    """Sternmilch latex: pale, glossy, the filling tints it a little (Farbe)"""
    material = new_material("M_Ing_Latex")
    if material is None:
        return unreal.load_asset(FOLDER + "/M_Ing_Latex")
    g = Graph(material)
    pale = g.node(unreal.MaterialExpressionConstant3Vector, 4, constant=unreal.LinearColor(0.72, 0.68, 0.55, 1.0))
    tint = g.color("Farbe", (0.4, 0.6, 0.9), 4)
    mixed = g.op(unreal.MaterialExpressionLinearInterpolate, 2, pale, tint)
    library.connect_material_expressions(g.const(0.45, 3), "", mixed, "Alpha")
    fresnel = g.node(unreal.MaterialExpressionFresnel, 3, exponent=2.5, base_reflect_fraction=0.04)
    g.out(mixed, unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.mul(1, g.mul(2, tint, fresnel), g.const(0.6, 2)), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(g.const(0.18), unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.const(0.6), unreal.MaterialProperty.MP_SPECULAR)
    return finish(material, g)


def streak():
    """The streak behind a shot in flight: additive, unlit, two-sided; bright at the head (U 0), gone at the tail (U 1),
    soft at the edges (V 0 and 1)"""
    material = new_material("M_Ing_Spur")
    if material is None:
        return unreal.load_asset(FOLDER + "/M_Ing_Spur")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    g = Graph(material)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, 7)
    u = g.mask(6, uv, r=True)
    v = g.mask(6, uv, r=False, g=True)
    along = g.power(4, g.one_minus(5, u), g.const(1.6, 5))
    # edges: 4 v (1 - v), 1 in the middle of the ribbon
    across = g.mul(4, g.mul(5, v, g.one_minus(6, v)), g.const(4.0, 5))
    shape = g.mul(3, along, g.power(4, across, g.const(1.5, 5)))
    g.out(g.mul(1, g.mul(2, shape, g.color("Farbe", (1.0, 0.9, 0.75), 3)), g.scalar("Staerke", 1.0, 3)), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return finish(material, g)


def import_pieces():
    kit = lambda name: unreal.load_asset("{}/{}".format(KIT, name))  # noqa: E731
    rock = unreal.load_asset("/Game/Vael/Maps/Aschenmark/Kulisse/Pack/MI_AM_Rock_03")
    slots = {
        "Schlacke": rock, "Stein": rock, "Eisen": kit("M_GK_Metall"), "Faden": kit("M_GK_Faden"), "Blase": kit("M_GK_Leder"),
        "Glas": kit("M_GK_Glas"), "Wachs": plain("M_Ing_Wachs", (0.32, 0.04, 0.03), 0.22, specular=0.6),
        "Kiel": plain("M_Ing_Kiel", (0.62, 0.55, 0.42), 0.4), "Feder": plain("M_Ing_Feder", (0.035, 0.033, 0.036), 0.8),
        "Kern": core(), "Latex": latex(), "Spur": streak(),
    }
    blase = plain("M_Ing_Blasenhaut", (0.3, 0.2, 0.15), 0.35, specular=0.55)
    slots["Blase"] = blase
    for slot, material in slots.items():
        if material is None:
            log("material missing: " + slot)
    for name in PIECES:
        fbx = os.path.join(SOURCE, name + ".fbx")
        if not os.path.isfile(fbx):
            log("missing " + fbx)
            continue
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_as_skeletal", False)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
        ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", fbx)
        task.set_editor_property("destination_path", FOLDER)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", ui)
        tools.import_asset_tasks([task])
        mesh = unreal.load_asset(FOLDER + "/" + name)
        if mesh is None:
            log("import failed: " + name)
            continue
        names = []
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            slot_name = str(slot.get_editor_property("material_slot_name")).split(".")[0]
            names.append(slot_name)
            if slots.get(slot_name) is not None:
                mesh.set_material(index, slots[slot_name])
        if name == "SM_Ing_Spur":
            mesh.set_editor_property("cast_shadow", False) if hasattr(mesh, "cast_shadow") else None
        assets.save_loaded_asset(mesh, False)
        log("{}: slots {}".format(name, ", ".join(names)))


import_pieces()
log("fertig")
