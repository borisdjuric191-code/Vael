"""Step 8b: imports only the animations of export/SK_Kelchreisser.fbx onto the existing SK_Kelchreisser_Skeleton, as
A_Kelchreisser_<Aktion> (the bones did not change). For new animations without a new skeleton: 08a removes the old
animations first (it can't remove mesh, skeleton and physics asset, they hold each other), then this.

Checks the result: the root bone carries the scale 100 (metres to cm, as with Pfuhlnatter and Rankenluchs), the pelvis
sits about 1.7 / 2.9 (m) from it. Don't set import_uniform_scale: it goes onto the root on top (scale 10000).

    UnrealEditor-Cmd.exe <Vael.uproject> -run=pythonscript -script="<this file>"
"""
import os

import unreal

ORDNER = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))),
                      "Vael_Quellen", "Kelchreisser")
ZIEL = "/Game/Vael/Kreaturen/Kelchreisser"
AKTIONEN = ("Stehen_Idle", "Galopp", "Gehen_Aufrecht", "Lauern", "Prankenhieb", "Klaue", "Biss", "Sprung", "Schrei")
eal = unreal.EditorAssetLibrary


def log(text):
    unreal.log_warning("[Kelchreisser] " + text)


def main():
    skelett = eal.load_asset(ZIEL + "/SK_Kelchreisser_Skeleton")
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", False)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", True)
    ui.set_editor_property("skeleton", skelett)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(ORDNER, "export", "SK_Kelchreisser.fbx"))
    task.set_editor_property("destination_path", ZIEL)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    log("importiert: {}".format(list(task.get_editor_property("imported_object_paths"))))

    for pfad in eal.list_assets(ZIEL, recursive=False):
        asset = eal.load_asset(pfad)
        if not isinstance(asset, unreal.AnimSequence):
            continue
        kurz = pfad.split(".")[-1]
        aktion = next((a for a in AKTIONEN if kurz.endswith("_" + a) or kurz == a), None)
        if aktion and kurz != "A_Kelchreisser_" + aktion:
            ziel = "{}/A_Kelchreisser_{}".format(ZIEL, aktion)
            if eal.does_asset_exist(ziel):
                eal.delete_asset(ziel)
            eal.rename_asset(pfad, ziel)
            log("Animation: {} -> A_Kelchreisser_{}".format(kurz, aktion))

    for aktion in AKTIONEN:
        anim = eal.load_asset("{}/A_Kelchreisser_{}".format(ZIEL, aktion))
        if anim is None:
            log("  {}: fehlt".format(aktion))
            continue
        becken = unreal.AnimationLibrary.get_bone_pose_for_time(anim, "becken", 0.0, False)
        wurzel = unreal.AnimationLibrary.get_bone_pose_for_time(anim, "wurzel", 0.0, False)
        log("  {}: {:.2f} s, Becken bei Bild 0 {}, Wurzel-Skalierung {:.0f}".format(
            aktion, unreal.AnimationLibrary.get_sequence_length(anim), becken.translation, wurzel.scale3d.x))
    eal.save_directory(ZIEL)


main()
