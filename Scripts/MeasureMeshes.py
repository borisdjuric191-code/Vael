# Lists the size of every static mesh of the downloaded packs, as a planning aid for dressing levels by script.
#
# - Changes nothing: it only loads the meshes and writes Saved/MeshSizes.txt.
# - One line per mesh: name, size x/y/z in cm, lowest point (0 means the pivot sits on the ground), simple collision shapes, path.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="Scripts/MeasureMeshes.py" -EnablePlugins=PythonScriptPlugin

import os

import unreal

PACK_FOLDERS = ["/Game/Megascans", "/Game/Meshes", "/Game/Rock_Collection_04", "/Game/Megaplant_Library"]


def measure():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    meshes = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)

    lines = []
    for folder in PACK_FOLDERS:
        for data in registry.get_assets_by_path(folder, recursive=True):
            if str(data.asset_class_path.asset_name) != "StaticMesh":
                continue

            mesh = data.get_asset()
            if mesh is None:
                continue

            box = mesh.get_bounding_box()
            size = box.max - box.min
            lines.append("{:<44} {:>8.0f} {:>8.0f} {:>8.0f}  low {:>7.0f}  collision {:>2}  {}".format(
                str(data.asset_name), size.x, size.y, size.z, box.min.z, meshes.get_simple_collision_count(mesh), str(data.package_name)))

    path = os.path.join(unreal.Paths.project_saved_dir(), "MeshSizes.txt")
    with open(path, "w", encoding="utf-8") as file:
        file.write("\n".join(sorted(lines)))

    unreal.log("Vael: measured {} meshes, see {}".format(len(lines), path))


measure()
