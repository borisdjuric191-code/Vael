# Imports the painted pictures of the materials (Boris 17.11., second take after the reference of painted item pictures:
# Tools/Loot/vael_bildbogen_prompts.py -> image generation -> vael_bildbogen_schneiden.py -> C:/Projekte/Vael_Quellen/Icons/gemalt)
# into /Game/Vael/Items/Icons/Bilder and gives every material of the bag its new picture, in place of the old one. The
# background is drawn by the game in the colour of the material's group (Items/VaelItemGroups).
# Note: Scripts/CreateLootKit.py sets the pictures of its five loot items again when it runs; run this one after it.
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ImportMaterialBilder.py" -unattended -nosplash

import os

import unreal

SOURCE = "C:/Projekte/Vael_Quellen/Icons/gemalt"
# the frames of the fields by rarity (Tools/HUD/vael_hud_gemalt.py, rahmen_*)
FRAMES = "C:/Projekte/Vael/Scripts/Data/HUD/Gemalt"
TARGET = "/Game/Vael/Items/Icons/Bilder"
MATERIALS = "/Game/Vael/Items/Materials"
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

tasks = []
for file in sorted(os.listdir(SOURCE)):
    if not file.startswith("T_Icon_") or not file.endswith(".png"):
        continue
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(SOURCE, file))
    task.set_editor_property("destination_path", TARGET)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    tasks.append(task)
tools.import_asset_tasks(tasks)

given = 0
for task in tasks:
    name = os.path.splitext(os.path.basename(task.get_editor_property("filename")))[0]
    texture = unreal.load_asset(TARGET + "/" + name)
    if texture is None:
        unreal.log_error("[Vael] Materialbild nicht importiert: " + name)
        continue
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    assets.save_loaded_asset(texture, False)
    material = unreal.load_asset(MATERIALS + "/DA_Material_" + name.replace("T_Icon_", ""))
    if material is not None:
        material.set_editor_property("icon", texture)
        assets.save_loaded_asset(material, False)
        given += 1
for file in sorted(os.listdir(FRAMES)):
    if not file.startswith("T_HUD_Gemalt_Itemrahmen_") or not file.endswith(".png"):
        continue
    name = os.path.splitext(file)[0]
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(FRAMES, file))
    task.set_editor_property("destination_path", "/Game/Vael/UI/HUD/Gemalt")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    texture = unreal.load_asset("/Game/Vael/UI/HUD/Gemalt/" + name)
    if texture is not None:
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("srgb", True)
        assets.save_loaded_asset(texture, False)
        unreal.log("[Vael] Itemrahmen: " + name)
unreal.log("[Vael] Materialbilder: {} importiert, {} Materialien bekommen ihr neues Bild".format(len(tasks), given))
