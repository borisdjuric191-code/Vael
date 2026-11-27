"""
Unreal (Editor geschlossen, -run=pythonscript): importiert die Kristallstücke der Schürfameise
(Vael_Quellen/Schuerfameise/04_export/kristalle/SM_AK_<Erz>_<k>.fbx aus ameise_kristalle.py, Modus 'export') nach
/Game/Vael/Kreaturen/Eisenschlund/Schuerfameise/Kristalle und gibt jedem Stück das Material seines Erzes
(/Game/Vael/Requisiten/Eisenschlund/<Erz>/MI_<Erz>; die Stücke sind aus diesem Erz geschnitten, die UVs passen).
Ohne Kollision: die Kristalle sitzen auf der Ameise, getroffen wird ihre Kapsel.

Optional ein Dateimuster, z. B. "SM_AK_*_B.fbx" für nur die gebogenen Stücke (kristalle_biegen.py).
"""
import glob
import os
import sys

import unreal

QUELLE = "C:/Projekte/Vael_Quellen/Schuerfameise/04_export/kristalle"
ZIEL = "/Game/Vael/Kreaturen/Eisenschlund/Schuerfameise/Kristalle"
MUSTER = sys.argv[1] if len(sys.argv) > 1 else "SM_AK_*.fbx"


def log(msg):
    unreal.log("[Vael] " + msg)


tasks = []
for fbx in sorted(glob.glob(os.path.join(QUELLE, MUSTER))):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
    ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", fbx)
    t.set_editor_property("destination_path", ZIEL)
    t.set_editor_property("destination_name", os.path.splitext(os.path.basename(fbx))[0])
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", True)
    t.set_editor_property("options", ui)
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

eal = unreal.EditorAssetLibrary
fertig = 0
for t in tasks:
    name = t.get_editor_property("destination_name")
    erz = name.split("_")[2]
    sm = eal.load_asset(ZIEL + "/" + name)
    mi = eal.load_asset("/Game/Vael/Requisiten/Eisenschlund/{0}/MI_{0}".format(erz))
    if not sm or not mi:
        log("FEHLT " + name)
        continue
    for i in range(len(sm.get_editor_property("static_materials"))):
        sm.set_material(i, mi)
    eal.save_loaded_asset(sm)
    fertig += 1
log("Kristalle importiert: {} von {}".format(fertig, len(tasks)))
