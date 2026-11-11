"""Brings the glint star into Unreal: /Game/Vael/Requisiten/Effekte/SM_Funkeln (from Vael_Quellen/_Effekte/SM_Funkeln.fbx,
built by blender/vael_funkeln.py) and its material M_Funkeln: unlit, additive, two-sided, Farbe * Glut, fading from the
middle of the star to its tips by the U of its UVs. What is there already is left alone.

Run with the editor closed:
  UnrealEditor-Cmd.exe <Vael.uproject> -run=pythonscript -script="<this file>"
"""
import os

import unreal

QUELLEN = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))),
                       "Vael_Quellen")
ZIEL = "/Game/Vael/Requisiten/Effekte"
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary


def log(text):
    unreal.log_warning("[Funkeln] " + text)


def main():
    if not eal.does_asset_exist(ZIEL + "/SM_Funkeln"):
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_as_skeletal", False)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
        ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(QUELLEN, "_Effekte", "SM_Funkeln.fbx"))
        task.set_editor_property("destination_path", ZIEL)
        task.set_editor_property("destination_name", "SM_Funkeln")
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", ui)
        tools.import_asset_tasks([task])

    if not eal.does_asset_exist(ZIEL + "/M_Funkeln"):
        mat = tools.create_asset("M_Funkeln", ZIEL, unreal.Material, unreal.MaterialFactoryNew())
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property("two_sided", True)
        farbe = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, 0)
        farbe.set_editor_property("parameter_name", "Farbe")
        farbe.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
        glut = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 250)
        glut.set_editor_property("parameter_name", "Glut")
        glut.set_editor_property("default_value", 8.0)
        uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1000, 450)
        maske = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -850, 450)
        maske.set_editor_property("r", True)
        maske.set_editor_property("g", False)
        maske.set_editor_property("b", False)
        maske.set_editor_property("a", False)
        innen = mel.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -700, 450)
        hoch = mel.create_material_expression(mat, unreal.MaterialExpressionPower, -550, 450)
        hoch.set_editor_property("const_exponent", 1.6)
        mal1 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -450, 100)
        mal2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 200)
        mel.connect_material_expressions(uv, "", maske, "")
        mel.connect_material_expressions(maske, "", innen, "")
        mel.connect_material_expressions(innen, "", hoch, "Base")
        mel.connect_material_expressions(farbe, "", mal1, "A")
        mel.connect_material_expressions(glut, "", mal1, "B")
        mel.connect_material_expressions(mal1, "", mal2, "A")
        mel.connect_material_expressions(hoch, "", mal2, "B")
        mel.connect_material_property(mal2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
        mel.recompile_material(mat)
        eal.save_asset(ZIEL + "/M_Funkeln", only_if_is_dirty=False)
        log("Material: " + ZIEL + "/M_Funkeln")

    sm = eal.load_asset(ZIEL + "/SM_Funkeln")
    if sm is None:
        log("SM_Funkeln fehlt - Ausgabe-Log pruefen")
        return
    sm.set_material(0, eal.load_asset(ZIEL + "/M_Funkeln"))
    eal.save_asset(sm.get_path_name(), only_if_is_dirty=False)
    log("SM_Funkeln: halbe Box %s" % sm.get_bounds().box_extent)
    eal.save_directory(ZIEL)


main()
