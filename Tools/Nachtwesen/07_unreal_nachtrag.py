"""Step 7: brings later additions of a night creature into Unreal without touching what is there.

  anim <Name>     the actions of export/A_<Name>_Nachtrag.fbx (06_export_nachtrag.py) as A_<Name>_<Aktion> on the
                  creature's skeleton; an animation that exists already is left alone
  effekte         the Moderglocke's spore and poison meshes (moderglocke_effekte.py) as static meshes and
                  M_Moderglocke_Effekt, the plain material the game colours them with (Farbe, Glut, Rauheit)

Run with the editor closed:
  UnrealEditor-Cmd.exe <Vael.uproject> -run=pythonscript -script="<this file> anim Kahlschwinge effekte"
"""
import os
import sys

import unreal

QUELLEN = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))),
                       "Vael_Quellen")
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary


def log(text):
    unreal.log_warning("[Nachtwesen] " + text)


def anim(name):
    ziel = "/Game/Vael/Kreaturen/Wurzelforst/" + name
    neu = ziel + "/Nachtrag"
    skm = eal.load_asset("%s/SK_%s" % (ziel, name))
    if skm is None:
        log("SK_%s fehlt" % name)
        return
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", False)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", True)
    ui.set_editor_property("skeleton", skm.get_editor_property("skeleton"))
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(QUELLEN, name, "export", "A_%s_Nachtrag.fbx" % name))
    task.set_editor_property("destination_path", neu)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])
    log("importiert: %s" % list(task.get_editor_property("imported_object_paths")))
    for pfad in eal.list_assets(neu, recursive=True):
        asset = eal.load_asset(pfad)
        kurz = pfad.split(".")[-1]
        if not isinstance(asset, unreal.AnimSequence):
            log("Beifang des Imports (bitte pruefen): " + pfad)
            continue
        aktion = kurz.split("Armature_")[-1] if "Armature_" in kurz else kurz.replace("A_%s_Nachtrag_" % name, "")
        if kurz == "A_%s_Nachtrag" % name:
            # a single action comes in under the file's name; 06_export_nachtrag.py wrote down what it is
            with open(os.path.join(QUELLEN, name, "export", "A_%s_Nachtrag.txt" % name), encoding="utf-8") as f:
                aktion = f.read().split()[0]
        fertig = "%s/A_%s_%s" % (ziel, name, aktion)
        if eal.does_asset_exist(fertig):
            log("A_%s_%s ist schon da - nicht ersetzt" % (name, aktion))
            continue
        eal.rename_asset(pfad, fertig)
        log("Animation: A_%s_%s, %.2f s" % (name, aktion, unreal.AnimationLibrary.get_sequence_length(eal.load_asset(fertig))))
    eal.save_directory(ziel)


def effekte():
    name = "Moderglocke"
    ziel = "/Game/Vael/Kreaturen/Wurzelforst/%s/Effekte" % name
    export = os.path.join(QUELLEN, name, "export")
    tasks = []
    for mesh in ("Spore", "Gifttropfen", "Giftlache"):
        kurz = "SM_%s_%s" % (name, mesh)
        if eal.does_asset_exist("%s/%s" % (ziel, kurz)):
            log(kurz + " ist schon da")
            continue
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_as_skeletal", False)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ui.set_editor_property("import_animations", False)
        ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
        ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(export, kurz + ".fbx"))
        task.set_editor_property("destination_path", ziel)
        task.set_editor_property("destination_name", kurz)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", ui)
        tasks.append(task)
    if tasks:
        tools.import_asset_tasks(tasks)

    mat_pfad = "%s/M_%s_Effekt" % (ziel, name)
    if not eal.does_asset_exist(mat_pfad):
        mat = tools.create_asset("M_%s_Effekt" % name, ziel, unreal.Material, unreal.MaterialFactoryNew())
        farbe = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 0)
        farbe.set_editor_property("parameter_name", "Farbe")
        farbe.set_editor_property("default_value", unreal.LinearColor(0.3, 0.08, 0.05, 1.0))
        glut = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 250)
        glut.set_editor_property("parameter_name", "Glut")
        glut.set_editor_property("default_value", 0.5)
        rau = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 400)
        rau.set_editor_property("parameter_name", "Rauheit")
        rau.set_editor_property("default_value", 0.5)
        mal = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 200)
        mel.connect_material_expressions(farbe, "", mal, "A")
        mel.connect_material_expressions(glut, "", mal, "B")
        mel.connect_material_property(farbe, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(mal, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.connect_material_property(rau, "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
        mel.recompile_material(mat)
        eal.save_asset(mat_pfad, only_if_is_dirty=False)
        log("Material: " + mat_pfad)
    mat = eal.load_asset(mat_pfad)
    for mesh in ("Spore", "Gifttropfen", "Giftlache"):
        sm = eal.load_asset("%s/SM_%s_%s" % (ziel, name, mesh))
        if sm is None:
            log("SM_%s_%s fehlt - Ausgabe-Log pruefen" % (name, mesh))
            continue
        sm.set_material(0, mat)
        eal.save_asset(sm.get_path_name(), only_if_is_dirty=False)
        log("%s: halbe Box %s" % (sm.get_name(), sm.get_bounds().box_extent))
    eal.save_directory(ziel)


def main():
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "anim":
            anim(args[i + 1])
            i += 2
        elif args[i] == "effekte":
            effekte()
            i += 1
        else:
            i += 1


main()
