"""
Vael – Kreaturenfabrik, Kleidung: Körper ohne verdeckte Teile (Unreal-Import)
=============================================================================

Importiert den Körper aus vael_koerper_verdecken.py auf das Skelett des MetaHuman:
  /Game/Vael/Characters/Menschen/MH_Kleidung/SK_<MetaHuman>_Koerper_<Teil>
Materialien (nach Slot-Namen) und das Post-Process-Animationsblueprint übernimmt er vom zusammengebauten Körper.

Ausführen bei geschlossenem Editor:
  UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="<diese Datei> C:/Projekte/Vael_Quellen/MagierMann Robe MH_Magier"
"""
import os, sys
import unreal

SKELETT = "/Game/Vael/Characters/Menschen/MH_Common/Female/Medium/NormalWeight/Body/metahuman_base_skel"
ZIEL = "/Game/Vael/Characters/Menschen/MH_Kleidung"


def importiere(ordner, kleidung, metahuman):
    fertig = f"SK_{metahuman}_Koerper_{kleidung}"
    eal = unreal.EditorAssetLibrary
    vorlage = eal.load_asset(f"/Game/Vael/Characters/Menschen/{metahuman}_Build/{metahuman}/Body/SKM_{metahuman}_BodyMesh")

    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("skeleton", eal.load_asset(SKELETT))
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("create_physics_asset", False)
    ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(ordner, "04_export", f"{fertig}.fbx"))
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

    alte = vorlage.get_editor_property("materials")
    nach_name = {str(m.get_editor_property("material_slot_name")): m.get_editor_property("material_interface") for m in alte}
    mats = list(skm.get_editor_property("materials"))
    for index, slot in enumerate(mats):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        material = nach_name.get(slot_name) or next(iter(nach_name.values()))
        unreal.log(f"[Vael] Slot {slot_name} -> {material.get_name()}")
        slot.set_editor_property("material_interface", material)
        mats[index] = slot
    skm.modify()
    skm.set_editor_property("materials", mats)
    skm.set_editor_property("post_process_anim_blueprint", vorlage.get_editor_property("post_process_anim_blueprint"))
    eal.save_asset(skm.get_path_name(), only_if_is_dirty=False)
    unreal.log(f"[Vael] {fertig} fertig, {len(alte)} alte und {len(mats)} neue Material-Slots")


if __name__ == "__main__":
    importiere(sys.argv[1], sys.argv[2], sys.argv[3])
