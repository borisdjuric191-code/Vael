"""
Vael – Kreaturenfabrik, Menschen (Unreal-Import)
================================================

Importiert die Teile eines Menschen (04_export/SK_<Name>_Koerper.fbx und SK_<Name>_Kopf.fbx aus vael_mensch_aufbereiten.py)
auf das vorhandene Unreal-Skelett SK_Mannequin, damit alle Menschen dieselben Animationen benutzen:
  /Game/Vael/Characters/Menschen/<Name>/SK_<Name>_Koerper, SK_<Name>_Kopf, T_<Name>_*, MI_<Name>

Ausführen bei geschlossenem Editor:
  UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="<diese Datei> C:/Projekte/Vael_Quellen/MagierMann"
"""
import json, os, sys
import unreal

SKELETT = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
MASTER_MATERIAL = "/Game/Vael/Materialien/M_Kreatur_Master"


def log(msg): unreal.log(f"[Vael] {msg}")


def importiere(ordner):
    with open(os.path.join(ordner, "mensch.json"), encoding="utf-8") as f:
        name = json.load(f)["name"]
    ziel = f"/Game/Vael/Characters/Menschen/{name}"
    eal = unreal.EditorAssetLibrary
    skelett = eal.load_asset(SKELETT)
    if skelett is None:
        raise RuntimeError(f"{SKELETT} fehlt")

    teile = []
    for teil in ("Koerper", "Kopf"):
        fbx = os.path.join(ordner, "04_export", f"SK_{name}_{teil}.fbx")
        if not os.path.exists(fbx):
            raise FileNotFoundError(f"{fbx} fehlt – zuerst vael_mensch_aufbereiten.py ausführen.")

        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_as_skeletal", True)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
        ui.set_editor_property("skeleton", skelett)
        ui.set_editor_property("import_materials", True)
        ui.set_editor_property("import_textures", True)
        ui.set_editor_property("import_animations", False)
        ui.set_editor_property("create_physics_asset", False)
        ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)

        task = unreal.AssetImportTask()
        task.set_editor_property("filename", fbx)
        task.set_editor_property("destination_path", ziel)
        task.set_editor_property("destination_name", f"SK_{name}_{teil}")
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", ui)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

        skm = eal.load_asset(f"{ziel}/SK_{name}_{teil}")
        if skm is None:
            raise RuntimeError(f"SK_{name}_{teil} wurde nicht angelegt – Ausgabe-Log prüfen.")
        benutzt = skm.get_editor_property("skeleton")
        log(f"SK_{name}_{teil}: Skelett {benutzt.get_path_name() if benutzt else None}")
        teile.append(skm)

    # Texturen nach Fabrikregeln benennen
    for p in eal.list_assets(ziel, recursive=False):
        kurz = p.split(".")[-1]
        if isinstance(eal.load_asset(p), unreal.Texture2D) and not kurz.startswith("T_"):
            art = next((a for a in ("basecolor", "normal", "roughness", "metallic") if kurz.lower().endswith(a)), None)
            if art is not None and not eal.does_asset_exist(f"{ziel}/T_{name}_{art}"):
                eal.rename_asset(p, f"{ziel}/T_{name}_{art}")

    # Eine Material-Instanz vom gemeinsamen Master für beide Teile
    if eal.does_asset_exist(MASTER_MATERIAL):
        mi_pfad = f"{ziel}/MI_{name}"
        mi = eal.load_asset(mi_pfad) if eal.does_asset_exist(mi_pfad) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            f"MI_{name}", ziel, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(mi, eal.load_asset(MASTER_MATERIAL))
        for art, param in (("basecolor", "BaseColor"), ("normal", "Normal"), ("roughness", "Roughness"), ("metallic", "Metallic")):
            t = eal.load_asset(f"{ziel}/T_{name}_{art}")
            if t is None:
                continue
            if art in ("roughness", "metallic") and t.get_editor_property("srgb"):
                t.set_editor_property("srgb", False)
                eal.save_asset(t.get_path_name(), only_if_is_dirty=False)
            unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, param, t)
        eal.save_asset(mi_pfad)

        for skm in teile:
            mats = list(skm.get_editor_property("materials"))
            for index, slot in enumerate(mats):
                slot.set_editor_property("material_interface", mi)
                mats[index] = slot
            skm.modify()
            skm.set_editor_property("materials", mats)
            eal.save_asset(skm.get_path_name(), only_if_is_dirty=False)

    eal.save_directory(ziel)
    log(f"Fertig: {name} liegt in {ziel}")


if __name__ == "__main__":
    importiere(sys.argv[1])
