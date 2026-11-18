# Imports the Panzerschleuder (Tools/Ingenieur/vael_armschleuder.py, Boris 16.11.): the heavy sling on the left forearm, as two
# static meshes SM_Ing_Panzerschleuder_Schiene (bracer) and _Gabel (fork that slides forward while he aims) into
# /Game/Vael/Characters/Ingenieur/Werkstuecke, with its 4K Tripo colour texture on the Glutpanzer master material (glow in the
# orange-red seams): MI_Ing_Panzerschleuder.
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ImportPanzerschleuder.py" -unattended -nosplash
import os

import unreal

SOURCE = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/06_waffen"
FOLDER = "/Game/Vael/Characters/Ingenieur/Werkstuecke"
MASTER = "/Game/Vael/Characters/Ingenieur/Glutpanzer/M_Ing_Glutpanzer"
PIECES = ["SM_Ing_Panzerschleuder_Schiene", "SM_Ing_Panzerschleuder_Gabel"]

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
library = unreal.MaterialEditingLibrary


def log(message):
    unreal.log_warning("[ImportPanzerschleuder] " + message)


def task_for(filename, name, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    if options is not None:
        task.set_editor_property("options", options)
    return task


tools.import_asset_tasks([task_for(os.path.join(SOURCE, "T_Ing_Panzerschleuder.png"), "T_Ing_Panzerschleuder")])
tex = unreal.load_asset(FOLDER + "/T_Ing_Panzerschleuder")
# worn on the body, seen up close: always fully loaded
tex.set_editor_property("never_stream", True)
assets.save_loaded_asset(tex, False)

path = FOLDER + "/MI_Ing_Panzerschleuder"
mi = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
    "MI_Ing_Panzerschleuder", FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
library.set_material_instance_parent(mi, unreal.load_asset(MASTER))
library.set_material_instance_texture_parameter_value(mi, "Farbe", tex)
assets.save_loaded_asset(mi, False)

for name in PIECES:
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    data = ui.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    tools.import_asset_tasks([task_for(os.path.join(SOURCE, name + ".fbx"), name, ui)])
    mesh = unreal.load_asset(FOLDER + "/" + name)
    if mesh is None:
        log("import failed: " + name)
        continue
    for index in range(len(mesh.get_editor_property("static_materials"))):
        mesh.set_material(index, mi)
    assets.save_loaded_asset(mesh, False)
    box = mesh.get_bounding_box()
    log("{}: box {:.0f}..{:.0f} / {:.0f}..{:.0f} / {:.0f}..{:.0f}".format(name, box.min.x, box.max.x, box.min.y, box.max.y, box.min.z, box.max.z))
log("fertig")
