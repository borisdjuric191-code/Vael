# Imports the mage's painted menus (Boris 18.11.: painted by Tripo, tidied by Tools/HUD/vael_tripo_magier.py) into
# /Game/Vael/UI/HUD/Gemalt: the inventory, the formula page, the menu frame and the HUD frame. The game draws into them
# (Source/Vael/UI/VaelHUDMage.cpp).
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ImportTripoMagier.py" -unattended -nosplash

import os

import unreal

SOURCE = "C:/Projekte/Vael/Scripts/Data/HUD/Gemalt"
TARGET = "/Game/Vael/UI/HUD/Gemalt"
NAMES = ["T_HUD_Tripo_Magier_Inventar", "T_HUD_Tripo_Magier_Formeln", "T_HUD_Tripo_Magier_Menue", "T_HUD_Tripo_Magier_HUD",
         "T_HUD_Tripo_Magier_Drachenkopf", "T_HUD_Tripo_Magier_Maske", "T_HUD_Tripo_Magier_Adern"]
# data, not colour: the mask of the frame and its Nid veins are read linear, like the other vein textures (CreateHudKit.py)
DATA = ("T_HUD_Tripo_Magier_Maske", "T_HUD_Tripo_Magier_Adern")
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

tasks = []
for name in NAMES:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(SOURCE, name + ".png"))
    task.set_editor_property("destination_path", TARGET)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    tasks.append(task)
tools.import_asset_tasks(tasks)

for name in NAMES:
    texture = unreal.load_asset(TARGET + "/" + name)
    if texture is None:
        unreal.log_error("[Vael] Magierbild nicht importiert: " + name)
        continue
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", name not in DATA)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    assets.save_loaded_asset(texture, False)
    unreal.log("[Vael] Magierbild importiert: " + name)
