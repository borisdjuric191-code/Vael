# Imports the buildings of the settlements (Tools/Welt/aschenmark_huetten.py) into /Game/Vael/Maps/Aschenmark/Bauten
# with the materials of the Glutkammer kit, like AusbauAschenmark.py does for the camp buildings.
# Hut and workbench get simple collision; pen, field and drying rack collide as they look (posts block, gaps let through).
#
# Run with the editor closed, after the Blender export:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ImportSiedlungsBauten.py" -EnablePlugins=PythonScriptPlugin -unattended

import os

import unreal

SOURCE = "C:/Projekte/Vael_Quellen/Aschenmark/export"
FOLDER = "/Game/Vael/Maps/Aschenmark/Bauten"
KIT = "/Game/Vael/Refugium/Glutkammer/Probe"
SLOTS = {"Holz": "MI_GK_Holz", "Leder": "M_GK_Leder", "Metall": "M_GK_Metall", "Faden": "M_GK_Faden", "Glut": "M_GK_Glut",
         "Moos": "M_GK_Moos", "Erde": "M_GK_Erde"}
# name: collision as it looks (complex as simple)
BUILDINGS = {"SM_AM_Huette": False, "SM_AM_Werkbank": False, "SM_AM_Gehege": True, "SM_AM_Acker": True, "SM_AM_Trockengestell": True}


def log(message):
    unreal.log_warning("[ImportSiedlungsBauten] " + message)


def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    materials = {slot: unreal.load_asset("{}/{}".format(KIT, name)) for slot, name in SLOTS.items()}
    for slot, material in materials.items():
        if material is None:
            log("material missing: " + slot)
    for name, complex_collision in BUILDINGS.items():
        fbx = os.path.join(SOURCE, name + ".fbx")
        if not os.path.isfile(fbx):
            log("missing " + fbx)
            continue
        ui = unreal.FbxImportUI()
        ui.set_editor_property("import_mesh", True)
        ui.set_editor_property("import_as_skeletal", False)
        ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        ui.set_editor_property("import_materials", False)
        ui.set_editor_property("import_textures", False)
        ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
        ui.static_mesh_import_data.set_editor_property("auto_generate_collision", not complex_collision)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", fbx)
        task.set_editor_property("destination_path", FOLDER)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", ui)
        tools.import_asset_tasks([task])
        built = unreal.load_asset("{}/{}".format(FOLDER, name))
        if built is None:
            log("import failed: " + name)
            continue
        for index, slot in enumerate(built.get_editor_property("static_materials")):
            slot_name = str(slot.get_editor_property("material_slot_name")).split(".")[0]
            if materials.get(slot_name) is not None:
                built.set_material(index, materials[slot_name])
        if complex_collision:
            built.get_editor_property("body_setup").set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        unreal.EditorAssetLibrary.save_loaded_asset(built, False)
        box = built.get_bounding_box()
        log("{}: {:.0f} x {:.0f} x {:.0f} cm, {} slots".format(name, box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z,
                                                              len(built.get_editor_property("static_materials"))))


main()
