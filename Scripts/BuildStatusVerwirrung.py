# Makes the look of the confusion sign (UVaelConfusionHalo): the spiral mesh SM_Status_Spirale (from
# Vael_Quellen/Status/SM_Status_Spirale.fbx, a tapering spiral tube made in Blender, ~21 cm wide) and the material
# M_Status_Verwirrung: unlit and glowing, the colour of each spiral wandering between turquoise and raspberry pink
# (Boris: turquoise and lilac alternating; violet stays the Nid's, so the Wachbeere's pink takes its place),
# each spiral at its own point of the change (custom data 0).
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildStatusVerwirrung.py" -EnablePlugins=PythonScriptPlugin

import os

import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPIRAL_FBX = os.path.join(os.path.dirname(ROOT), "Vael_Quellen", "Status", "SM_Status_Spirale.fbx")
FOLDER = "/Game/Vael/Effects/Status"
MATERIAL_PATH = FOLDER + "/M_Status_Verwirrung"
TURQUOISE = (0.1, 0.9, 1.0)
PINK = (1.0, 0.08, 0.38)
GLOW = 0.6
CHANGE_SPEED = 2.2   # radians per second of the colour change

library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
notes = []


def log(message):
    unreal.log_warning("[BuildStatusVerwirrung] " + message)


def import_spiral():
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", SPIRAL_FBX)
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", "SM_Status_Spirale")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.load_asset(FOLDER + "/SM_Status_Spirale")


def node(material, expression_class, x, y, **properties):
    expression = library.create_material_expression(material, expression_class, x, y)
    for key, value in properties.items():
        expression.set_editor_property(key, value)
    return expression


def link(source, target, target_input=""):
    if not library.connect_material_expressions(source, "", target, target_input):
        notes.append("link failed: {} -> {}.{}".format(source.get_name(), target.get_name(), target_input))


def build_material():
    if assets.does_asset_exist(MATERIAL_PATH):
        material = unreal.load_asset(MATERIAL_PATH)
        library.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_Status_Verwirrung", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    # Translucent, drawn after the Wachbeere's additive beams (sort priority in UVaelConfusionHalo), which would
    # otherwise wash the colours out to white
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("used_with_instanced_static_meshes", True)

    time = node(material, unreal.MaterialExpressionTime, -1400, 0)
    speed = node(material, unreal.MaterialExpressionScalarParameter, -1400, 120, parameter_name="Wechseltempo", default_value=CHANGE_SPEED)
    phase = node(material, unreal.MaterialExpressionPerInstanceCustomData, -1400, 240, data_index=0, const_default_value=0.0)
    turn = node(material, unreal.MaterialExpressionConstant, -1400, 360, r=6.283)
    timed = node(material, unreal.MaterialExpressionMultiply, -1200, 40)
    link(time, timed, "A")
    link(speed, timed, "B")
    shifted = node(material, unreal.MaterialExpressionMultiply, -1200, 260)
    link(phase, shifted, "A")
    link(turn, shifted, "B")
    angle = node(material, unreal.MaterialExpressionAdd, -1000, 120)
    link(timed, angle, "A")
    link(shifted, angle, "B")
    wave = node(material, unreal.MaterialExpressionSine, -850, 120)
    link(angle, wave)
    half = node(material, unreal.MaterialExpressionConstant, -850, 260, r=0.5)
    scaled = node(material, unreal.MaterialExpressionMultiply, -700, 120)
    link(wave, scaled, "A")
    link(half, scaled, "B")
    blend = node(material, unreal.MaterialExpressionAdd, -550, 120)
    link(scaled, blend, "A")
    link(half, blend, "B")
    first = node(material, unreal.MaterialExpressionVectorParameter, -700, -200, parameter_name="FarbeA",
                 default_value=unreal.LinearColor(TURQUOISE[0], TURQUOISE[1], TURQUOISE[2], 1.0))
    second = node(material, unreal.MaterialExpressionVectorParameter, -700, -40, parameter_name="FarbeB",
                  default_value=unreal.LinearColor(PINK[0], PINK[1], PINK[2], 1.0))
    colour = node(material, unreal.MaterialExpressionLinearInterpolate, -400, 0)
    link(first, colour, "A")
    link(second, colour, "B")
    link(blend, colour, "Alpha")
    glow = node(material, unreal.MaterialExpressionScalarParameter, -400, 200, parameter_name="Leuchten", default_value=GLOW)
    emissive = node(material, unreal.MaterialExpressionMultiply, -200, 60)
    link(colour, emissive, "A")
    link(glow, emissive, "B")
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(node(material, unreal.MaterialExpressionConstant, -200, 220, r=1.0), "", unreal.MaterialProperty.MP_OPACITY)
    library.recompile_material(material)
    assets.save_loaded_asset(material, False)
    return material


def main():
    spiral = import_spiral()
    material = build_material()
    if spiral is not None:
        spiral.set_material(0, material)
        assets.save_loaded_asset(spiral, False)
    log("spiral {}, material {}".format("ok" if spiral else "MISSING", material.get_path_name()))
    for note in notes:
        log("NOTE " + note)


main()
