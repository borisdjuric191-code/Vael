"""Step 9: brings new Rankenluchs animations into Unreal without touching mesh, skeleton, materials or physics.

Imports export/SK_Rankenluchs.fbx animation-only onto the existing skeleton into a temporary folder, then puts every
action in place of its A_Rankenluchs_<Aktion> (same path, so the data asset keeps finding it). Used for the cat-like
walk and run of 2026-10-28 (Boris: too stiff).

Run with the editor closed:
  UnrealEditor-Cmd.exe <Vael.uproject> -run=pythonscript -script="<this file>" -EnablePlugins=PythonScriptPlugin
"""
import os

import unreal

ORDNER = os.path.join(os.path.expanduser("~"), "Desktop", "Vael_Rankenluchs")
FBX = os.path.join(ORDNER, "export", "SK_Rankenluchs.fbx")
ZIEL = "/Game/Vael/Kreaturen/Rankenluchs"
NEU = ZIEL + "/_Neu"
AKTIONEN = ("Stehen_Idle", "Tarn_Ruhe", "Erwachen", "Sprung_Angriff", "Laufen", "Rennen")

eal = unreal.EditorAssetLibrary


def log(text):
    unreal.log_warning("[Rankenluchs] " + text)


def main():
    skm = eal.load_asset(ZIEL + "/SK_Rankenluchs")
    if skm is None:
        log("SK_Rankenluchs fehlt")
        return
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", False)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    ui.set_editor_property("skeleton", skm.get_editor_property("skeleton"))
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", True)
    ui.anim_sequence_import_data.set_editor_property("convert_scene", True)
    ui.anim_sequence_import_data.set_editor_property("import_bone_tracks", True)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", FBX)
    task.set_editor_property("destination_path", NEU)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    for pfad in eal.list_assets(NEU, recursive=True):
        asset = eal.load_asset(pfad)
        kurz = pfad.split(".")[-1]
        if not isinstance(asset, unreal.AnimSequence):
            continue
        aktion = next((a for a in AKTIONEN if kurz.endswith(a) or ("_" + a) in kurz), None)
        if aktion is None:
            log("Animation ohne bekannte Aktion: " + kurz)
            continue
        ziel = "{}/A_Rankenluchs_{}".format(ZIEL, aktion)
        if eal.does_asset_exist(ziel):
            eal.delete_asset(ziel)
        eal.rename_asset(pfad, ziel)
        log("neu: A_Rankenluchs_{} ({:.2f} s)".format(aktion, unreal.AnimationLibrary.get_sequence_length(eal.load_asset(ziel))))
    eal.save_directory(ZIEL)
    if not eal.list_assets(NEU, recursive=True):
        eal.delete_directory(NEU)


main()
