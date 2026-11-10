"""Step 5: imports a night creature into Unreal, everything under /Game/Vael/Kreaturen/Wurzelforst/<Name>/.

  SK_<Name> (skeletal mesh), its skeleton and physics asset, the animations as A_<Name>_<Aktion>, the Tripo textures
  T_<Name>_basecolor / _normal / _roughness / _metallic and MI_<Name>, an instance of the creature factory's
  M_Kreatur_Master. Nothing outside this folder is created or changed. A creature that is there already is left
  alone (changed animations need a fresh import: Boris deletes the folder's assets in the editor first).

Run with the editor closed:
  UnrealEditor-Cmd.exe <Vael.uproject> -run=pythonscript -script="<this file> <Name> [<Name> ...]"
"""
import os
import sys

import unreal

QUELLEN = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))),
                       "Vael_Quellen")
MASTER = "/Game/Vael/Materialien/M_Kreatur_Master"
TEXTUREN = (("basecolor", "jpg", "BaseColor"), ("normal", "png", "Normal"), ("roughness", "jpg", "Roughness"),
            ("metallic", "jpg", "Metallic"))
eal = unreal.EditorAssetLibrary


def log(text):
    unreal.log_warning("[Nachtwesen] " + text)


def texturen(name, ziel, export):
    tasks = []
    for art, endung, _ in TEXTUREN:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(export, "T_%s_%s.%s" % (name, art, endung)))
        task.set_editor_property("destination_path", ziel)
        task.set_editor_property("destination_name", "T_%s_%s" % (name, art))
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", False)
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for art, _, _ in TEXTUREN:
        tex = eal.load_asset("%s/T_%s_%s" % (ziel, name, art))
        if tex is None:
            log("Textur fehlt: " + art)
            continue
        if art == "normal":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        if art in ("roughness", "metallic"):
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
        eal.save_asset(tex.get_path_name(), only_if_is_dirty=False)


def modell(name, ziel, export):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", True)
    ui.set_editor_property("create_physics_asset", True)
    ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
    ui.skeletal_mesh_import_data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.IGNORE)
    ui.skeletal_mesh_import_data.set_editor_property("convert_scene", True)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(export, "SK_%s.fbx" % name))
    task.set_editor_property("destination_path", ziel)
    # No destination name and no replacing: with either, Interchange leaves the animations out
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    log("importiert: %s" % list(task.get_editor_property("imported_object_paths")))


def aufraeumen(name, ziel):
    """Animations get project names: A_<Name>_<Aktion>"""
    anims = []
    for pfad in eal.list_assets(ziel, recursive=True):
        asset = eal.load_asset(pfad)
        kurz = pfad.split(".")[-1]
        if isinstance(asset, unreal.AnimSequence):
            if not kurz.startswith("A_%s_" % name):
                # Interchange names them <file>_<object>_<action> or just the action: the action is the tail after "Armature_"
                aktion = kurz.split("Armature_")[-1] if "Armature_" in kurz else kurz.replace("SK_%s_" % name, "")
                neu = "%s/A_%s_%s" % (ziel, name, aktion)
                if not eal.does_asset_exist(neu):
                    eal.rename_asset(pfad, neu)
                    log("Animation: %s -> A_%s_%s" % (kurz, name, aktion))
                    kurz = "A_%s_%s" % (name, aktion)
            anims.append(kurz)
        elif isinstance(asset, (unreal.Material, unreal.MaterialInstance)) and not kurz.startswith("MI_" + name):
            log("Vom Import angelegtes Material (bitte pruefen): " + pfad)
    return anims


def material(name, ziel):
    skm = eal.load_asset("%s/SK_%s" % (ziel, name))
    if skm is None:
        log("SK_%s fehlt - Ausgabe-Log pruefen" % name)
        return
    if skm.get_editor_property("physics_asset") is None:
        unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).create_physics_asset(skm)
    if eal.does_asset_exist(MASTER):
        mi_pfad = "%s/MI_%s" % (ziel, name)
        mi = eal.load_asset(mi_pfad) if eal.does_asset_exist(mi_pfad) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "MI_" + name, ziel, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(mi, eal.load_asset(MASTER))
        for art, _, param in TEXTUREN:
            tex = eal.load_asset("%s/T_%s_%s" % (ziel, name, art))
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
    grenzen = skm.get_bounds()
    log("%s: Groesse (halbe Box) %s, Materialplaetze %d" % (name, grenzen.box_extent, len(skm.get_editor_property("materials"))))


def main():
    for name in sys.argv[1:]:
        ziel = "/Game/Vael/Kreaturen/Wurzelforst/" + name
        export = os.path.join(QUELLEN, name, "export")
        if eal.does_asset_exist("%s/SK_%s" % (ziel, name)):
            log("%s ist schon da - nichts importiert (fuer neue Animationen zuerst die Assets im Editor loeschen)" % name)
            continue
        texturen(name, ziel, export)
        modell(name, ziel, export)
        anims = aufraeumen(name, ziel)
        material(name, ziel)
        for kurz in sorted(anims):
            anim = eal.load_asset("%s/%s" % (ziel, kurz))
            log("  %s: %.2f s" % (kurz, unreal.AnimationLibrary.get_sequence_length(anim)))
        eal.save_directory(ziel)


main()
