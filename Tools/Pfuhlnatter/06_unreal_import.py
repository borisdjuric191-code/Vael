"""Step 6: imports the Pfuhlnatter into Unreal, everything under /Game/Vael/Kreaturen/Pfuhlnatter/.

  SK_Pfuhlnatter (skeletal mesh), its skeleton and physics asset, the eight animations as A_Pfuhlnatter_<Aktion>,
  the Tripo textures T_Pfuhlnatter_basecolor / _normal / _roughness / _metallic and MI_Pfuhlnatter, an instance of the
  creature factory's M_Kreatur_Master (wetness and scars follow in their own package). Nothing outside this folder is
  created or changed.

Run with the editor closed:
  UnrealEditor-Cmd.exe <Vael.uproject> -run=pythonscript -script="<this file>" -EnablePlugins=PythonScriptPlugin
"""
import os

import unreal

ORDNER = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))),
                      "Vael_Quellen", "Pfuhlnatter")
EXPORT = os.path.join(ORDNER, "export")
ZIEL = "/Game/Vael/Kreaturen/Pfuhlnatter"
MASTER = "/Game/Vael/Materialien/M_Kreatur_Master"
AKTIONEN = ("Kriechen", "Schwimmen", "Aufrichten", "Kampf", "Spucken", "Biss", "Ablegen", "Tod")
TEXTUREN = (("basecolor", "jpg", "BaseColor"), ("normal", "png", "Normal"), ("roughness", "jpg", "Roughness"),
            ("metallic", "jpg", "Metallic"))
eal = unreal.EditorAssetLibrary


def log(text):
    unreal.log_warning("[Pfuhlnatter] " + text)


def texturen():
    tasks = []
    for art, endung, _ in TEXTUREN:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(EXPORT, "T_Pfuhlnatter_%s.%s" % (art, endung)))
        task.set_editor_property("destination_path", ZIEL)
        task.set_editor_property("destination_name", "T_Pfuhlnatter_" + art)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", False)
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for art, _, _ in TEXTUREN:
        tex = eal.load_asset("{}/T_Pfuhlnatter_{}".format(ZIEL, art))
        if tex is None:
            log("Textur fehlt: " + art)
            continue
        if art == "normal":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        if art in ("roughness", "metallic"):
            # Measured values, not colours
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
        eal.save_asset(tex.get_path_name(), only_if_is_dirty=False)
        log("Textur: T_Pfuhlnatter_" + art)


def neu_importieren():
    """Over an existing SK_Pfuhlnatter (new weights): Interchange replaces the mesh, in the right units, and keeps its
    skeleton and animations; it can't re-import animations onto a skeleton by themselves, and the classic FBX importer
    keeps the settings of the first import (a 6 cm snake from this FBX), so changed animations need a fresh import:
    mesh, skeleton and physics asset hold on to each other and can't be deleted by script, Boris deletes the folder's
    SK_/A_ assets in the editor first"""
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("skeleton", eal.load_asset(ZIEL + "/SK_Pfuhlnatter_Skeleton"))
    ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
    # The fin mask of 03_skelett.py is in the vertex colour (M_Pfuhlnatter_Koerper hides drawn-in membranes with it)
    ui.skeletal_mesh_import_data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    ui.skeletal_mesh_import_data.set_editor_property("convert_scene", True)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(EXPORT, "SK_Pfuhlnatter.fbx"))
    task.set_editor_property("destination_path", ZIEL)
    task.set_editor_property("destination_name", "SK_Pfuhlnatter")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    log("Modell ersetzt: {}".format(list(task.get_editor_property("imported_object_paths"))))
    return True


def modell():
    if eal.does_asset_exist(ZIEL + "/SK_Pfuhlnatter"):
        return neu_importieren()
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", True)
    ui.set_editor_property("create_physics_asset", True)
    ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
    # The fin mask of 03_skelett.py is in the vertex colour (M_Pfuhlnatter_Koerper hides drawn-in membranes with it)
    ui.skeletal_mesh_import_data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    ui.skeletal_mesh_import_data.set_editor_property("convert_scene", True)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(EXPORT, "SK_Pfuhlnatter.fbx"))
    task.set_editor_property("destination_path", ZIEL)
    # No destination name and no replacing: with either, Interchange leaves the animations out (the mesh is called
    # SK_Pfuhlnatter in the FBX already, and the old one was deleted above)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    log("importiert: {}".format(list(task.get_editor_property("imported_object_paths"))))
    return True


def aufraeumen():
    """Animations get project names; stray materials an importer may still create are reported, not deleted"""
    for pfad in eal.list_assets(ZIEL, recursive=True):
        asset = eal.load_asset(pfad)
        kurz = pfad.split(".")[-1]
        if isinstance(asset, unreal.AnimSequence):
            aktion = next((a for a in AKTIONEN if kurz.endswith(a) or ("_" + a) in kurz or kurz.endswith(a.lower())), None)
            if aktion and kurz != "A_Pfuhlnatter_" + aktion:
                ziel = "{}/A_Pfuhlnatter_{}".format(ZIEL, aktion)
                if eal.does_asset_exist(ziel):
                    eal.delete_asset(ziel)  # our own older import of the same animation
                eal.rename_asset(pfad, ziel)
                log("Animation: {} -> A_Pfuhlnatter_{}".format(kurz, aktion))
            elif aktion is None:
                log("Animation ohne bekannte Aktion: " + kurz)
        elif isinstance(asset, (unreal.Material, unreal.MaterialInstance)) and not kurz.startswith("MI_Pfuhlnatter"):
            log("Vom Import angelegtes Material (bitte pruefen): " + pfad)


def material():
    skm = eal.load_asset(ZIEL + "/SK_Pfuhlnatter")
    if skm is None:
        log("SK_Pfuhlnatter fehlt - Ausgabe-Log pruefen")
        return
    if skm.get_editor_property("physics_asset") is None:
        unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).create_physics_asset(skm)
    if eal.does_asset_exist(ZIEL + "/M_Pfuhlnatter_Koerper"):
        log("M_Pfuhlnatter_Koerper vorhanden: 09_unreal_koerpermaterial.py setzt es erneut")
    elif eal.does_asset_exist(MASTER):
        mi_pfad = ZIEL + "/MI_Pfuhlnatter"
        mi = eal.load_asset(mi_pfad) if eal.does_asset_exist(mi_pfad) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "MI_Pfuhlnatter", ZIEL, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(mi, eal.load_asset(MASTER))
        for art, _, param in TEXTUREN:
            tex = eal.load_asset("{}/T_Pfuhlnatter_{}".format(ZIEL, art))
            if tex:
                unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, param, tex)
        eal.save_asset(mi_pfad)
        mats = list(skm.get_editor_property("materials"))
        for slot in mats:
            slot.set_editor_property("material_interface", mi)
        skm.modify()
        skm.set_editor_property("materials", mats)
        eal.save_asset(skm.get_path_name(), only_if_is_dirty=False)
    else:
        log("M_Kreatur_Master fehlt - Slots bleiben leer")
    skelett = skm.get_editor_property("skeleton")
    log("Skelett: {}, Materialplaetze: {}".format(
        skelett.get_name() if skelett else None,
        [str(m.get_editor_property("material_slot_name")) for m in skm.get_editor_property("materials")]))
    for aktion in AKTIONEN:
        anim = eal.load_asset("{}/A_Pfuhlnatter_{}".format(ZIEL, aktion))
        log("  {}: {}".format(aktion, "{:.2f} s".format(unreal.AnimationLibrary.get_sequence_length(anim)) if anim else "fehlt"))
    eal.save_directory(ZIEL)


texturen()
if modell():
    aufraeumen()
    material()
