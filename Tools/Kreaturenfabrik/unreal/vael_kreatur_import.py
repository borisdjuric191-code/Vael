"""
Vael – Kreaturenfabrik, Station 7 (Unreal-Import)
=================================================

Importiert eine aufbereitete Kreatur (SK_<Name>.fbx aus Station 5) einheitlich ins Projekt:
  /Game/Vael/Kreaturen/<Region>/<Name>/
    SK_<Name>           Skelett-Mesh (+ Skelett, Physik-Asset)
    T_<Name>_*          Texturen aus dem FBX
    MI_<Name>           Material-Instanz vom gemeinsamen Kreatur-Master-Material (falls vorhanden)

Ausführen im Unreal-Editor:
  Werkzeuge → Python-Skript ausführen … → diese Datei wählen
  (oder im Ausgabe-Log, Python-Modus:  py "D:/Vael_Tools/vael_kreatur_import.py" "D:/Vael_Quellen/Spannhornkaefer")
Ohne Pfadangabe öffnet sich ein Ordnerdialog.

Voraussetzung: Plugin „Python Editor Script Plugin“ ist aktiv (Bearbeiten → Plugins).
Noch nicht im Skript (folgt, sobald die C++-Klassen stehen): Datensatz DA_<Name> und Blueprint BP_<Name>.
"""
import json, os, sys
import unreal

MASTER_MATERIAL = "/Game/Vael/Materialien/M_Kreatur_Master"   # legt vael_kreatur_master.py an; fehlt es, bleibt das Tripo-Material

def log(msg): unreal.log(f"[Vael] {msg}")

def ordner_waehlen():
    if len(sys.argv) > 1:
        return sys.argv[1]
    try:
        import tkinter, tkinter.filedialog
        root = tkinter.Tk(); root.withdraw()
        return tkinter.filedialog.askdirectory(title="Kreatur-Ordner wählen (enthält kreatur.json)")
    except Exception:
        raise RuntimeError("Bitte den Kreatur-Ordner als Argument angeben.")

def importiere(ordner):
    with open(os.path.join(ordner, "kreatur.json"), encoding="utf-8") as f:
        sb = json.load(f)
    name, region = sb["name_datei"], sb["region"]
    fbx = os.path.join(ordner, "04_export", f"SK_{name}.fbx")
    if not os.path.exists(fbx):
        raise FileNotFoundError(f"{fbx} fehlt – zuerst das Blender-Skript ('binden') ausführen.")
    ziel = f"/Game/Vael/Kreaturen/{region}/{name}"

    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("import_materials", True)
    ui.set_editor_property("import_textures", True)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("create_physics_asset", True)
    ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
    ui.skeletal_mesh_import_data.set_editor_property("convert_scene", True)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", fbx)
    task.set_editor_property("destination_path", ziel)
    task.set_editor_property("destination_name", f"SK_{name}")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    pfade = list(task.get_editor_property("imported_object_paths"))
    log(f"importiert: {pfade}")

    eal = unreal.EditorAssetLibrary
    # Texturen und Materialien nach Fabrikregeln umbenennen
    for p in eal.list_assets(ziel, recursive=False):
        a = eal.load_asset(p)
        kurz = p.split(".")[-1]
        if isinstance(a, unreal.Texture2D) and not kurz.startswith("T_"):
            # Tripo-Namen wie "Spannhornkaefer_v3_basecolor" werden zu "T_Spannhornkaefer_basecolor"
            rest = kurz[len(name):].lstrip("_") if kurz.startswith(name) else kurz
            kopf, _, schwanz = rest.partition("_")
            if schwanz and kopf[:1] == "v" and kopf[1:].isdigit():
                rest = schwanz
            eal.rename_asset(p, f"{ziel}/T_{name}_{rest}")

    skm = eal.load_asset(f"{ziel}/SK_{name}")
    if skm is None:
        raise RuntimeError("Skelett-Mesh wurde nicht angelegt – Ausgabe-Log prüfen.")

    # Interchange (Standard in UE 5.8) übergeht create_physics_asset der FbxImportUI – daher hier selbst anlegen
    if skm.get_editor_property("physics_asset") is None:
        unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).create_physics_asset(skm)
        log(f"Physik-Asset angelegt: {skm.get_editor_property('physics_asset')}")

    material_anlegen(ziel, name, skm)

    eal.save_directory(ziel)
    log(f"Fertig: {sb['name']} liegt in {ziel}")


def textur_parameter(pfad):
    """Parameter des Master-Materials für eine Tripo-Textur, None für Texturen, die der Master nicht braucht"""
    n = pfad.split(".")[-1].lower()
    if n.endswith("_rm") or "_rm_" in n:
        return None   # Roughness+Metallic in einer Textur – der Master nimmt die einzelnen Karten
    if "normal" in n:
        return "Normal"
    if "rough" in n:
        return "Roughness"
    if "metal" in n:
        return "Metallic"
    if any(k in n for k in ("basecolor", "base_color", "diffuse", "albedo")):
        return "BaseColor"
    return None


def material_anlegen(ziel, name, skm):
    """Material-Instanz vom gemeinsamen Master (Mark-Leuchten, später Adern und Glutkanten) – nur wenn der Master existiert"""
    eal = unreal.EditorAssetLibrary
    if not eal.does_asset_exist(MASTER_MATERIAL):
        log("Master-Material fehlt noch – vael_kreatur_master.py ausführen. Tripo-Material bleibt vorerst.")
        return

    mi_pfad = f"{ziel}/MI_{name}"
    mi = eal.load_asset(mi_pfad) if eal.does_asset_exist(mi_pfad) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        f"MI_{name}", ziel, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(mi, eal.load_asset(MASTER_MATERIAL))
    for p in eal.list_assets(ziel, recursive=False):
        t = eal.load_asset(p)
        param = textur_parameter(p) if isinstance(t, unreal.Texture2D) else None
        if param is not None:
            # Roughness und Metallic sind Messwerte, keine Farben: linear lesen
            if param in ("Roughness", "Metallic") and t.get_editor_property("srgb"):
                t.set_editor_property("srgb", False)
                eal.save_asset(t.get_path_name(), only_if_is_dirty=False)
            unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, param, t)
            log(f"{param} = {p}")
    eal.save_asset(mi_pfad)

    # Jeder Slot außer dem leeren Mark-Slot bekommt die Instanz
    mats = list(skm.get_editor_property("materials"))
    for index, slot in enumerate(mats):
        if "mark" not in str(slot.get_editor_property("material_slot_name")).lower():
            slot.set_editor_property("material_interface", mi)
            mats[index] = slot
    skm.modify()
    skm.set_editor_property("materials", mats)
    eal.save_asset(skm.get_path_name(), only_if_is_dirty=False)
    belegt = [m.get_editor_property("material_interface").get_name() for m in skm.get_editor_property("materials")]
    log(f"Slots: {belegt}")
    log(f"Material-Instanz angelegt: {mi_pfad}")


if __name__ == "__main__":
    if "--nur-material" in sys.argv:
        sys.argv.remove("--nur-material")
        ordner = ordner_waehlen()
        with open(os.path.join(ordner, "kreatur.json"), encoding="utf-8") as f:
            sb = json.load(f)
        ziel = f"/Game/Vael/Kreaturen/{sb['region']}/{sb['name_datei']}"
        material_anlegen(ziel, sb["name_datei"], unreal.EditorAssetLibrary.load_asset(f"{ziel}/SK_{sb['name_datei']}"))
    else:
        importiere(ordner_waehlen())
