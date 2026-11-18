# Imports the Panzerkugel (Tools/Ingenieur/vael_panzerkugel.py, Boris 17.11.): the curled Aschenroller covered with the
# Glutpanzer's back shell, the ball the engineer becomes when he rolls in the whole Schwarzglut set. Static mesh
# SM_Ing_Panzerkugel in /Game/Vael/Characters/Ingenieur/Glutpanzer: the core slot gets the Aschenroller ball's material, the
# shell slots GP_Teil_<n> the Glutpanzer part materials MI_GP_Teil_<n>.
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ImportPanzerkugel.py" -unattended -nosplash
import unreal

SOURCE = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/07_kugel/SM_Ing_Panzerkugel.fbx"
FOLDER = "/Game/Vael/Characters/Ingenieur/Glutpanzer"
NAME = "SM_Ing_Panzerkugel"
MATS = "/Game/Vael/Characters/Ingenieur/Glutpanzer/Materialien"
CORE = "/Game/Vael/Requisiten/Aschenmark/AschenrollerKugel/MI_AschenrollerKugel"

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary


def log(message):
    unreal.log_warning("[ImportPanzerkugel] " + message)


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
task.set_editor_property("filename", SOURCE)
task.set_editor_property("destination_path", FOLDER)
task.set_editor_property("destination_name", NAME)
task.set_editor_property("replace_existing", True)
task.set_editor_property("automated", True)
task.set_editor_property("save", True)
task.set_editor_property("options", ui)
tools.import_asset_tasks([task])

mesh = unreal.load_asset(FOLDER + "/" + NAME)
if mesh is None:
    log("import failed")
else:
    # Tripo's atlases are patchworks of tiny islands: half-precision UVs land on neighbouring glow islands
    # (the commandlet has no editor subsystems to get, so the instance is made directly)
    sm = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.StaticMeshEditorSubsystem()
    build = sm.get_lod_build_settings(mesh, 0)
    build.set_editor_property("use_full_precision_u_vs", True)
    sm.set_lod_build_settings(mesh, 0, build)
    materials = list(mesh.get_editor_property("static_materials"))
    hits = 0
    for index, slot in enumerate(materials):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        part = slot_name.split("_ncl")[0]
        mi = unreal.load_asset(CORE) if "Aschenroller" in part else (
            unreal.load_asset("{}/MI_{}".format(MATS, part)) if assets.does_asset_exist("{}/MI_{}".format(MATS, part)) else None)
        if mi is not None:
            mesh.set_material(index, mi)
            hits += 1
        else:
            log("no material for slot " + slot_name)
    assets.save_loaded_asset(mesh, False)
    box = mesh.get_bounding_box()
    log("{}: {} slots, {} matched, box {:.0f}..{:.0f} / {:.0f}..{:.0f} / {:.0f}..{:.0f}".format(
        NAME, len(materials), hits, box.min.x, box.max.x, box.min.y, box.max.y, box.min.z, box.max.z))
log("fertig")
