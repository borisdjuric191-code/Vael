# Creates the two simple materials of the element orbs that circle above the head of a mage.
#
# - M_Vael_Kugel: solid and glowing, for the cores. Parameters: Color, Glow.
# - M_Vael_Schein: a see-through glow that adds its light to the picture, for halos, flames and wind.
#   Parameters: Color, Glow and Rim (0 = bright in the middle and soft at the edge, 1 = only the edge shines, like a bubble).
# - Never overwrites: a material that exists is left alone, so it can be reworked in the editor.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="Scripts/CreateEffectMaterials.py" -EnablePlugins=PythonScriptPlugin

import unreal

FOLDER = "/Game/Vael/Effects"

library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def new_material(name):
    path = "{}/{}".format(FOLDER, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log("Vael: {} exists already, left untouched".format(path))
        return None
    return tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def color_and_glow(material):
    """Color * Glow, the light every orb material gives off"""
    color = library.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -700, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.6, 0.2, 1.0))

    glow = library.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -700, 220)
    glow.set_editor_property("parameter_name", "Glow")
    glow.set_editor_property("default_value", 2.0)

    light = library.create_material_expression(material, unreal.MaterialExpressionMultiply, -450, 100)
    library.connect_material_expressions(color, "", light, "A")
    library.connect_material_expressions(glow, "", light, "B")
    return color, light


def finish(material):
    library.recompile_material(material)
    if unreal.EditorAssetLibrary.save_loaded_asset(material, False):
        unreal.log("Vael: created {}".format(material.get_path_name()))


def create_core():
    material = new_material("M_Vael_Kugel")
    if material is None:
        return

    color, light = color_and_glow(material)

    # A dark body in the same color, smooth enough to catch a highlight
    body = library.create_material_expression(material, unreal.MaterialExpressionMultiply, -450, -150)
    body.set_editor_property("const_b", 0.35)
    library.connect_material_expressions(color, "", body, "A")

    roughness = library.create_material_expression(material, unreal.MaterialExpressionConstant, -450, 320)
    roughness.set_editor_property("r", 0.25)

    library.connect_material_property(body, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(light, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(material)


def create_glow():
    material = new_material("M_Vael_Schein")
    if material is None:
        return

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    color, light = color_and_glow(material)

    # How much the surface faces away from the camera: 0 in the middle of a sphere, 1 at its edge
    edge = library.create_material_expression(material, unreal.MaterialExpressionFresnel, -700, 420)
    edge.set_editor_property("exponent", 2.0)
    edge.set_editor_property("base_reflect_fraction", 0.0)

    middle = library.create_material_expression(material, unreal.MaterialExpressionOneMinus, -500, 420)
    library.connect_material_expressions(edge, "", middle, "")
    soft = library.create_material_expression(material, unreal.MaterialExpressionMultiply, -330, 420)
    library.connect_material_expressions(middle, "", soft, "A")
    library.connect_material_expressions(middle, "", soft, "B")

    rim = library.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 620)
    rim.set_editor_property("parameter_name", "Rim")
    rim.set_editor_property("default_value", 0.0)

    shape = library.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -150, 480)
    library.connect_material_expressions(soft, "", shape, "A")
    library.connect_material_expressions(edge, "", shape, "B")
    library.connect_material_expressions(rim, "", shape, "Alpha")

    shaped = library.create_material_expression(material, unreal.MaterialExpressionMultiply, 60, 200)
    library.connect_material_expressions(light, "", shaped, "A")
    library.connect_material_expressions(shape, "", shaped, "B")

    library.connect_material_property(shaped, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(material)


create_core()
create_glow()
