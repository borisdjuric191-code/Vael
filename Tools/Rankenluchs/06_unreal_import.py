"""Step 7: imports the Rankenluchs into Unreal, everything under /Game/Vael/Kreaturen/Rankenluchs/.

  SK_Rankenluchs (skeletal mesh), its skeleton and physics asset, the six animations as A_Rankenluchs_<Aktion>,
  the Tripo textures T_Rankenluchs_basecolor / _normal / _rm. No materials from the FBX: 07_unreal_materialien.py
  builds them. Nothing outside this folder is created or changed.

Run with the editor closed:
  UnrealEditor-Cmd.exe <Vael.uproject> -run=pythonscript -script="<this file>" -EnablePlugins=PythonScriptPlugin
"""
import os

import unreal

ORDNER = os.path.join(os.path.expanduser("~"), "Desktop", "Vael_Rankenluchs")
EXPORT = os.path.join(ORDNER, "export")
ZIEL = "/Game/Vael/Kreaturen/Rankenluchs"
AKTIONEN = ("Stehen_Idle", "Tarn_Ruhe", "Erwachen", "Sprung_Angriff", "Laufen", "Rennen")
eal = unreal.EditorAssetLibrary


def log(text):
    unreal.log_warning("[Rankenluchs] " + text)


def texturen():
    tasks = []
    for art, datei in (("basecolor", "T_Rankenluchs_basecolor.jpg"), ("normal", "T_Rankenluchs_normal.png"), ("rm", "T_Rankenluchs_rm.png")):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(EXPORT, datei))
        task.set_editor_property("destination_path", ZIEL)
        task.set_editor_property("destination_name", "T_Rankenluchs_" + art)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", False)
        tasks.append((art, task))
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t for _, t in tasks])
    for art, _ in tasks:
        tex = eal.load_asset("{}/T_Rankenluchs_{}".format(ZIEL, art))
        if tex is None:
            log("Textur fehlt: " + art)
            continue
        if art == "normal":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        if art == "rm":
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        eal.save_asset(tex.get_path_name(), only_if_is_dirty=False)
        log("Textur: T_Rankenluchs_" + art)


def modell():
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", True)
    ui.set_editor_property("create_physics_asset", True)
    ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
    ui.skeletal_mesh_import_data.set_editor_property("convert_scene", True)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(EXPORT, "SK_Rankenluchs.fbx"))
    task.set_editor_property("destination_path", ZIEL)
    task.set_editor_property("destination_name", "SK_Rankenluchs")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    log("importiert: {}".format(list(task.get_editor_property("imported_object_paths"))))


def aufraeumen():
    """Animations get project names; stray materials an importer may still create are reported, not deleted"""
    for pfad in eal.list_assets(ZIEL, recursive=True):
        asset = eal.load_asset(pfad)
        kurz = pfad.split(".")[-1]
        if isinstance(asset, unreal.AnimSequence):
            aktion = next((a for a in AKTIONEN if kurz.endswith(a) or ("_" + a) in kurz or kurz.endswith(a.lower())), None)
            if aktion and kurz != "A_Rankenluchs_" + aktion:
                ziel = "{}/A_Rankenluchs_{}".format(ZIEL, aktion)
                if eal.does_asset_exist(ziel):
                    eal.delete_asset(ziel)  # our own older import of the same animation
                eal.rename_asset(pfad, ziel)
                log("Animation: {} -> A_Rankenluchs_{}".format(kurz, aktion))
            elif aktion is None:
                log("Animation ohne bekannte Aktion: " + kurz)
        elif isinstance(asset, (unreal.Material, unreal.MaterialInstance)) and not kurz.startswith(("M_Ranken", "MI_Ranken")):
            log("Vom Import angelegtes Material (bitte pruefen): " + pfad)

    skm = eal.load_asset(ZIEL + "/SK_Rankenluchs")
    if skm is None:
        log("SK_Rankenluchs fehlt - Ausgabe-Log pruefen")
        return
    if skm.get_editor_property("physics_asset") is None:
        unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).create_physics_asset(skm)
    skelett = skm.get_editor_property("skeleton")
    knochen = skelett.get_editor_property("bone_tree") if skelett else []
    log("Skelett: {}, Knochen: {}, Materialplaetze: {}".format(
        skelett.get_name() if skelett else None, len(knochen),
        [str(m.get_editor_property("material_slot_name")) for m in skm.get_editor_property("materials")]))
    for aktion in AKTIONEN:
        anim = eal.load_asset("{}/A_Rankenluchs_{}".format(ZIEL, aktion))
        if anim:
            log("  {}: {:.2f} s".format(aktion, anim.get_editor_property("sequence_length") if hasattr(anim, "sequence_length") else unreal.AnimationLibrary.get_sequence_length(anim)))
        else:
            log("  {} fehlt".format(aktion))
    eal.save_directory(ZIEL)


texturen()
modell()
aufraeumen()
