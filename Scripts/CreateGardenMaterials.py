# Creates the ground cover that spreads around every sown plant of the seed garden.
#
# - M_Vael_Gruen: a decal of moss green with a soft, ragged edge, so the green of plants near each other grows into one patch.
#   Parameters: Color (dark green), ColorLight (light green), Strength (how much of the ground it covers, 0 to 1).
# - Never overwrites: a material that exists is left alone, so it can be reworked in the editor.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="Scripts/CreateGardenMaterials.py" -EnablePlugins=PythonScriptPlugin

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


def expression(material, kind, x, y):
    return library.create_material_expression(material, kind, x, y)


def noise(material, scale, x, y):
    """Cloudy noise from 0 to 1 over the ground, the same wherever decals overlap"""
    position = expression(material, unreal.MaterialExpressionWorldPosition, x - 250, y)
    clouds = expression(material, unreal.MaterialExpressionNoise, x, y)
    clouds.set_editor_property("scale", scale)
    clouds.set_editor_property("levels", 3)
    clouds.set_editor_property("output_min", 0.0)
    clouds.set_editor_property("output_max", 1.0)
    library.connect_material_expressions(position, "", clouds, "Position")
    return clouds


def create_green():
    material = new_material("M_Vael_Gruen")
    if material is None:
        return

    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    # Distance from the middle of the decal: 0 in the middle, 1 at its edge
    coordinates = expression(material, unreal.MaterialExpressionTextureCoordinate, -1500, 300)
    middle = expression(material, unreal.MaterialExpressionConstant2Vector, -1500, 450)
    middle.set_editor_property("r", 0.5)
    middle.set_editor_property("g", 0.5)
    offset = expression(material, unreal.MaterialExpressionSubtract, -1250, 350)
    library.connect_material_expressions(coordinates, "", offset, "A")
    library.connect_material_expressions(middle, "", offset, "B")
    length = expression(material, unreal.MaterialExpressionLength, -1050, 350)
    library.connect_material_expressions(offset, "", length, "")
    distance = expression(material, unreal.MaterialExpressionMultiply, -880, 350)
    distance.set_editor_property("const_b", 2.0)
    library.connect_material_expressions(length, "", distance, "A")

    # The edge is ragged: coarse clouds push it in and out
    ragged = noise(material, 0.012, -1250, 600)
    push = expression(material, unreal.MaterialExpressionSubtract, -1000, 600)
    push.set_editor_property("const_b", 0.5)
    library.connect_material_expressions(ragged, "", push, "A")
    pushed = expression(material, unreal.MaterialExpressionMultiply, -830, 600)
    pushed.set_editor_property("const_b", 0.45)
    library.connect_material_expressions(push, "", pushed, "A")
    edge = expression(material, unreal.MaterialExpressionAdd, -660, 450)
    library.connect_material_expressions(distance, "", edge, "A")
    library.connect_material_expressions(pushed, "", edge, "B")

    # Full in the middle, fading over the outer third
    inside = expression(material, unreal.MaterialExpressionOneMinus, -490, 450)
    library.connect_material_expressions(edge, "", inside, "")
    steep = expression(material, unreal.MaterialExpressionMultiply, -330, 450)
    steep.set_editor_property("const_b", 3.0)
    library.connect_material_expressions(inside, "", steep, "A")
    mask = expression(material, unreal.MaterialExpressionSaturate, -170, 450)
    library.connect_material_expressions(steep, "", mask, "")

    strength = expression(material, unreal.MaterialExpressionScalarParameter, -330, 650)
    strength.set_editor_property("parameter_name", "Strength")
    strength.set_editor_property("default_value", 0.9)
    opacity = expression(material, unreal.MaterialExpressionMultiply, 0, 500)
    library.connect_material_expressions(mask, "", opacity, "A")
    library.connect_material_expressions(strength, "", opacity, "B")

    # Two greens mixed by finer clouds, so it doesn't look painted
    dark = expression(material, unreal.MaterialExpressionVectorParameter, -700, -250)
    dark.set_editor_property("parameter_name", "Color")
    dark.set_editor_property("default_value", unreal.LinearColor(0.035, 0.13, 0.02, 1.0))
    light = expression(material, unreal.MaterialExpressionVectorParameter, -700, -30)
    light.set_editor_property("parameter_name", "ColorLight")
    light.set_editor_property("default_value", unreal.LinearColor(0.13, 0.30, 0.04, 1.0))
    mottled = noise(material, 0.06, -700, 180)
    color = expression(material, unreal.MaterialExpressionLinearInterpolate, -330, -100)
    library.connect_material_expressions(dark, "", color, "A")
    library.connect_material_expressions(light, "", color, "B")
    library.connect_material_expressions(mottled, "", color, "Alpha")

    roughness = expression(material, unreal.MaterialExpressionConstant, -330, 150)
    roughness.set_editor_property("r", 0.9)

    library.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    library.recompile_material(material)
    if unreal.EditorAssetLibrary.save_loaded_asset(material, False):
        unreal.log("Vael: created {}".format(material.get_path_name()))


create_green()
