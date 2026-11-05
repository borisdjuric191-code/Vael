# Imports the pieces of the Keimgeflecht, the menu of the Floromant (Boris 2026-11-04), into /Game/Vael/UI/Keimgeflecht:
#   T_Keim_Grund          the fixed background (root chamber, frame, title, strand with the empty capsules, herbarium cards, plate)
#   T_Keim_Knospe         the sealed bud of a wheel place not grown yet
#   T_Keim_Rund_<Id>      the ink picture of each plant cut round for the capsules (and _Schatten)
# The pictures come from Tools/HUD/vael_keimgeflecht_entwurf.py run with --spiel into Scripts/Data/HUD/Keimgeflecht.
# Always imported anew (they are only pictures). Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateKeimgeflechtKit.py"

import os
import unreal

FOLDER = "/Game/Vael/UI/Keimgeflecht"
SOURCE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Data", "HUD", "Keimgeflecht")

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
count = 0
for file in sorted(os.listdir(SOURCE)):
    if not file.endswith(".png"):
        continue
    name = file[:-4]
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(SOURCE, file))
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(FOLDER + "/" + name)
    if texture is None:
        unreal.log_warning("[Keimgeflecht] could not import " + file)
        continue
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("srgb", True)
    if name in ("T_Keim_Ast", "T_Keim_Wurzel"):
        # the bark bands repeat along the moving roots of the frame
        texture.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
        texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    assets.save_loaded_asset(texture, False)
    count += 1
unreal.log_warning("[Keimgeflecht] imported {} pictures".format(count))
