# Makes the material of the lightning look (AVaelLightningBolt). Run headless with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script=C:/Projekte/Vael/Scripts/CreateLightningKit.py
# Rerun-safe: the graph is only rebuilt when KIT_VERSION grew.

import unreal

FOLDER = "/Game/Vael/Effects/Blitz"
KIT_VERSION = 1.2

assets = unreal.EditorAssetLibrary
library = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
notes = []

# Distance from the middle of a ribbon (B of the vertex colour 0) or of a round flash (B 1), 0 in the middle, 1 at the edge
DISTANCE = "float Line = abs(UV.y * 2.0 - 1.0); float Round = length(UV * 2.0 - 1.0); return lerp(Line, Round, VC.b);"

# Hot, thin core and a wide halo falling off; R of the vertex colour is the brightness, G how much of it is core. A dark bolt is hollow: no light where its black core runs
EMISSIVE = """float Core = saturate(1.0 - D / 0.14);
float Halo = pow(saturate(1.0 - D), 3.0);
float Hollow = 1.0 - Dunkel * saturate(1.0 - D / 0.28);
return (KernFarbe * Core * VC.g * 4.0 + Farbe * (Halo + Core * 0.5) * Hollow) * VC.r * Leuchten;"""

# Only the dark core of the black lightning covers what lies behind it, everything else just adds light
OPACITY = "return Dunkel * saturate(1.0 - D / 0.2) * saturate(VC.r * 3.0) * VC.g;"


def log(message):
    unreal.log_warning("[CreateLightningKit] " + message)


def build():
    path = "{}/M_Blitz".format(FOLDER)
    if assets.does_asset_exist(path):
        material = unreal.load_asset(path)
        if "KitVersion" in [str(parameter) for parameter in library.get_scalar_parameter_names(material)]:
            if library.get_material_default_scalar_parameter_value(material, "KitVersion") >= KIT_VERSION - 0.001:
                log("up to date")
                return
        library.delete_all_material_expressions(material)
        for expression in list(unreal.ObjectIterator(unreal.MaterialExpression)):
            if expression.get_outer() == material:
                library.delete_material_expression(material, expression)
    else:
        material = tools.create_asset("M_Blitz", FOLDER, unreal.Material, unreal.MaterialFactoryNew())

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    row = [0]

    def node(expression_class, **properties):
        row[0] += 1
        expression = library.create_material_expression(material, expression_class, -500, 130 * row[0])
        for key, value in properties.items():
            expression.set_editor_property(key, value)
        return expression

    def custom(code, output_type, sources):
        expression = node(unreal.MaterialExpressionCustom, code=code, output_type=output_type)
        inputs = []
        for name, _ in sources:
            entry = unreal.CustomInput()
            entry.set_editor_property("input_name", name)
            inputs.append(entry)
        expression.set_editor_property("inputs", inputs)
        for name, source in sources:
            if not library.connect_material_expressions(source, "", expression, name):
                notes.append("link failed: " + name)
        return expression

    uv = node(unreal.MaterialExpressionTextureCoordinate)
    vertex = node(unreal.MaterialExpressionVertexColor)
    color = node(unreal.MaterialExpressionVectorParameter, parameter_name="Farbe", default_value=unreal.LinearColor(0.35, 0.55, 1.0, 1.0))
    core = node(unreal.MaterialExpressionVectorParameter, parameter_name="KernFarbe", default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    glow = node(unreal.MaterialExpressionScalarParameter, parameter_name="Leuchten", default_value=10.0)
    dark = node(unreal.MaterialExpressionScalarParameter, parameter_name="Dunkel", default_value=0.0)

    distance = custom(DISTANCE, unreal.CustomMaterialOutputType.CMOT_FLOAT1, [("UV", uv), ("VC", vertex)])
    emissive = custom(EMISSIVE, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                      [("D", distance), ("VC", vertex), ("Farbe", color), ("KernFarbe", core), ("Leuchten", glow), ("Dunkel", dark)])
    opacity = custom(OPACITY, unreal.CustomMaterialOutputType.CMOT_FLOAT1, [("D", distance), ("VC", vertex), ("Dunkel", dark)])

    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    # Only a marker for reruns, not wired
    node(unreal.MaterialExpressionScalarParameter, parameter_name="KitVersion", default_value=KIT_VERSION)

    library.recompile_material(material)
    assets.save_loaded_asset(material, False)


def main():
    assets.make_directory(FOLDER)
    build()
    for note in notes:
        log("NOTE " + note)
    log("done, {} notes".format(len(notes)))


main()
