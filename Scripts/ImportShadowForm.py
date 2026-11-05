# Imports the texture of a shadow form (Tools/Pflanzenarm/schattenform.py) as a new asset next to the plant arm:
# /Game/Vael/Characters/Floromant/Pflanzenarm/<Id>/T_<Id>_Schatten. The plant arm finds it by name (UVaelPlantArmData::
# LoadShadowTexture), so no existing asset is changed. Boris 2026-11-05: the first one is the Blutrose (Rußrose).
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ImportShadowForm.py Russrose"

import os
import sys
import unreal

PLANT = sys.argv[-1] if len(sys.argv) > 1 else "Russrose"
SOURCE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Data", "Schattenformen", "T_{}_Schatten_basecolor.png".format(PLANT))
FOLDER = "/Game/Vael/Characters/Floromant/Pflanzenarm/" + PLANT
NAME = "T_{}_Schatten".format(PLANT)

task = unreal.AssetImportTask()
task.set_editor_property("filename", SOURCE)
task.set_editor_property("destination_path", FOLDER)
task.set_editor_property("destination_name", NAME)
task.set_editor_property("replace_existing", True)
task.set_editor_property("automated", True)
task.set_editor_property("save", False)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.load_asset(FOLDER + "/" + NAME)
if texture is None:
    unreal.log_warning("[Schattenform] could not import " + SOURCE)
else:
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    unreal.log_warning("[Schattenform] imported " + FOLDER + "/" + NAME)
