# Imports the engineer's pieces (Tools/Ingenieur/vael_ingenieur_werkstuecke.py) and gives their slots the materials of the
# Glutkammer kit (wood, leather, metal, thread, ember, moss).
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ImportEngineerPieces.py" -unattended -nosplash
import os

import unreal

SOURCE = "C:/Projekte/Vael_Quellen/Ingenieur/export"
FOLDER = "/Game/Vael/Characters/Ingenieur/Werkstuecke"
KIT = "/Game/Vael/Refugium/Glutkammer/Probe"
PIECES = ["SM_Ing_Schleuder", "SM_Ing_Zwillenschale", "SM_Ing_Bogen", "SM_Ing_Armbrust", "SM_Ing_Fangeisen", "SM_Ing_Knallmine", "SM_Ing_Schleuderturm",
          "SM_Ing_Rucksack", "SM_Ing_Beutel_Leder", "SM_Ing_Beutel_Panzer", "SM_Ing_Beutel_Seide", "SM_Ing_Brille"]
# the armour sets, four pieces each (VAEL_ING_RUESTUNG in the Blender script)
PIECES += ["SM_Ing_R_{}_{}".format(s, p) for s in ("Rollpanzer", "Sichelklinge", "Russfeder", "Blutschuppen") for p in ("Kopf", "Brust", "Arm", "Bein")]
PIECES += ["SM_Ing_Glutschleuder", "SM_Ing_Spannschleuder", "SM_Ing_Dornenbogen", "SM_Ing_Sehnenbogen", "SM_Ing_SchwereArmbrust"]
if __import__("os").environ.get("VAEL_ING_NUR_ZWEIGE"):
    PIECES = [p for p in PIECES if p in ("SM_Ing_Glutschleuder", "SM_Ing_Spannschleuder", "SM_Ing_Dornenbogen", "SM_Ing_Sehnenbogen", "SM_Ing_SchwereArmbrust")]
if __import__("os").environ.get("VAEL_ING_NUR_RUESTUNG"):
    PIECES = [p for p in PIECES if p.startswith("SM_Ing_R_")]
SLOTS = {"Holz": "MI_GK_Holz", "Leder": "M_GK_Leder", "Metall": "M_GK_Metall", "Faden": "M_GK_Faden", "Glut": "M_GK_Glut", "Moos": "M_GK_Moos"}

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
materials = {slot: unreal.load_asset("{}/{}".format(KIT, name)) for slot, name in SLOTS.items()}

for name in PIECES:
    fbx = os.path.join(SOURCE, name + ".fbx")
    if not os.path.exists(fbx):
        raise FileNotFoundError(fbx)
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    data = ui.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", fbx)
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset("{}/{}".format(FOLDER, name))
    if mesh is None:
        raise RuntimeError(name + " wurde nicht importiert")
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.get_editor_property("material_slot_name")).split(".")[0]
        if materials.get(slot_name) is not None:
            mesh.set_material(index, materials[slot_name])
    assets.save_loaded_asset(mesh, False)
    used = [s.get_editor_property("material_interface").get_name() if s.get_editor_property("material_interface") else "-" for s in mesh.get_editor_property("static_materials")]
    unreal.log_warning("[Ingenieur] {}: {}".format(name, used))
