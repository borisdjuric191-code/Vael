"""
Vael – Kreaturenfabrik, Kleidung (Unreal-Import)
================================================

Importiert ein auf den MetaHuman umgesetztes Kleidungsstück (vael_kleidung_anpassen.py) auf das Skelett des MetaHuman:
  /Game/Vael/Characters/Menschen/MH_Kleidung/SK_<Name>_<Teil>_MH
Das Material kommt von der Vorlage (MI_<Name>), die UVs sind dieselben.

Ausführen bei geschlossenem Editor:
  UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="<diese Datei> C:/Projekte/Vael_Quellen/MagierMann Robe"
"""
import os, sys
import unreal

SKELETT = "/Game/Vael/Characters/Menschen/MH_Common/Female/Medium/NormalWeight/Body/metahuman_base_skel"
ZIEL = "/Game/Vael/Characters/Menschen/MH_Kleidung"


def importiere(ordner, kleidung):
    name = os.path.basename(ordner.rstrip("\\/"))
    fertig = f"SK_{name}_{kleidung}_MH"
    fbx = os.path.join(ordner, "04_export", f"{fertig}.fbx")
    eal = unreal.EditorAssetLibrary
    skelett = eal.load_asset(SKELETT)
    if skelett is None:
        raise RuntimeError(f"{SKELETT} fehlt")

    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("skeleton", skelett)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("create_physics_asset", False)
    ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", fbx)
    task.set_editor_property("destination_path", ZIEL)
    task.set_editor_property("destination_name", fertig)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    skm = eal.load_asset(f"{ZIEL}/{fertig}")
    if skm is None:
        raise RuntimeError(f"{fertig} wurde nicht angelegt – Ausgabe-Log prüfen.")

    material = eal.load_asset(f"/Game/Vael/Characters/Menschen/{name}/MI_{name}")
    mats = list(skm.get_editor_property("materials"))
    for index, slot in enumerate(mats):
        slot.set_editor_property("material_interface", material)
        mats[index] = slot
    skm.modify()
    skm.set_editor_property("materials", mats)
    eal.save_asset(skm.get_path_name(), only_if_is_dirty=False)

    benutzt = skm.get_editor_property("skeleton")
    unreal.log(f"[Vael] {fertig}: Skelett {benutzt.get_path_name() if benutzt else None}, {len(mats)} Material(ien)")


if __name__ == "__main__":
    importiere(sys.argv[1], sys.argv[2])
