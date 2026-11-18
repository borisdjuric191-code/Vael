# Imports the Glutpanzer as its own body (Tools/Ingenieur/vael_glutpanzer_rig.py, goal 16.11.): four skeletal meshes Kopf, Brust,
# Arme, Beine on one skeleton SK_Ing_GP_Skelett in /Game/Vael/Characters/Ingenieur/Glutpanzer/Koerper, with the part materials
# of Scripts/ImportGlutpanzer.py (MI_GP_Teil_<n>). In the game they follow the body like the MetaHuman (VaelPoseFollowerAnimInstance).
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ImportGlutpanzerRig.py" -unattended -nosplash
import os

import unreal

SOURCE = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/05_rig"
FOLDER = "/Game/Vael/Characters/Ingenieur/Glutpanzer/Koerper"
MATS = "/Game/Vael/Characters/Ingenieur/Glutpanzer/Materialien"
PIECES = ["Beine", "Brust", "Arme", "Kopf", "Schild", "UnterarmL"]
# The back shell, the engineer's shield, has a skeleton of its own (root and the chain Schild_0..4 it curls by)
OWN_SKELETON = {"Schild"}

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary


def log(message):
    unreal.log_warning("[ImportGlutpanzerRig] " + message)


skeleton = None
for piece in PIECES:
    name = "SKM_Ing_GP_" + piece
    fbx = os.path.join(SOURCE, name + ".fbx")
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("create_physics_asset", False)
    if piece in OWN_SKELETON:
        own = unreal.load_asset("{}/{}_Skeleton".format(FOLDER, name)) if assets.does_asset_exist("{}/{}_Skeleton".format(FOLDER, name)) else None
        if own is not None:
            ui.set_editor_property("skeleton", own)
    elif skeleton is not None:
        ui.set_editor_property("skeleton", skeleton)
    data = ui.skeletal_mesh_import_data
    data.set_editor_property("import_morph_targets", False)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", fbx)
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", ui)
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset(FOLDER + "/" + name)
    if mesh is None:
        log("import failed: " + name)
        continue
    # Tripo's atlas is a patchwork of tiny islands: half-precision UVs (the skeletal default) landed on neighbouring glow
    # islands and spotted the crust gold
    build = unreal.SkeletalMeshEditorSubsystem.get_lod_build_settings(mesh, 0)
    build.set_editor_property("use_full_precision_u_vs", True)
    unreal.SkeletalMeshEditorSubsystem.set_lod_build_settings(mesh, 0, build)
    if skeleton is None:
        skeleton = mesh.get_editor_property("skeleton")
        log("skeleton " + skeleton.get_path_name())
    materials = list(mesh.get_editor_property("materials"))
    hits = 0
    for slot in materials:
        slot_name = str(slot.get_editor_property("material_slot_name")).split(".")[0]
        part = slot_name.split("_ncl")[0]
        mi = unreal.load_asset("{}/MI_{}".format(MATS, part))
        if mi is not None:
            slot.set_editor_property("material_interface", mi)
            hits += 1
    mesh.set_editor_property("materials", materials)
    assets.save_loaded_asset(mesh, False)
    log("{}: {} slots, {} matched, skeleton {}".format(name, len(materials), hits, mesh.get_editor_property("skeleton").get_name()))
log("fertig")
