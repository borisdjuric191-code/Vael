# Exports the body of an assembled MetaHuman as FBX, the template clothes are fitted to in Blender (vael_kleidung_anpassen.py).
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script=<this file> -unattended -nosplash -nullrhi
# The character is chosen with the environment variable VAEL_MH_NAME (default MH_Magier).
import os

import unreal

NAME = os.environ.get("VAEL_MH_NAME", "MH_Magier")
BODY = f"/Game/Vael/Characters/Menschen/{NAME}_Build/{NAME}/Body/SKM_{NAME}_BodyMesh"
TARGET = f"C:/Projekte/Vael_Quellen/_Skelett/SKM_{NAME}_Body.fbx"

mesh = unreal.load_asset(BODY)
if mesh is None:
    raise RuntimeError(f"Vael MetaHuman: {BODY} not found")

options = unreal.FbxExportOption()
options.set_editor_property("level_of_detail", False)
options.set_editor_property("collision", False)
options.set_editor_property("export_morph_targets", False)
options.set_editor_property("export_preview_mesh", False)

task = unreal.AssetExportTask()
task.set_editor_property("object", mesh)
task.set_editor_property("filename", TARGET)
task.set_editor_property("automated", True)
task.set_editor_property("replace_identical", True)
task.set_editor_property("prompt", False)
task.set_editor_property("options", options)
if not unreal.Exporter.run_asset_export_task(task):
    raise RuntimeError("Vael MetaHuman: export failed")
unreal.log(f"Vael MetaHuman: exported {TARGET}")
