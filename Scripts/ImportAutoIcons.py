# Imports the pictures rendered by Tools/Loot/vael_icons_auto.py (C:/Projekte/Vael_Quellen/Icons/auto/T_Icon_*.png) as UI
# textures into /Game/Vael/Items/Icons/Auto and gives each material of the bag its picture, but only where it has none yet
# (pictures made by hand in the loot kit stay). Run again after new materials or a new render.
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ImportAutoIcons.py" -unattended -nosplash

import os

import unreal

SOURCE = "C:/Projekte/Vael_Quellen/Icons/auto"
TARGET = "/Game/Vael/Items/Icons/Auto"
MATERIALS = "/Game/Vael/Items/Materials"
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

tasks = []
for file in sorted(os.listdir(SOURCE)):
    if not file.endswith(".png"):
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
        continue
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    assets.save_loaded_asset(texture, False)
    material = unreal.load_asset(MATERIALS + "/DA_Material_" + name.replace("T_Icon_", ""))
    if material is not None and material.get_editor_property("icon") is None:
        material.set_editor_property("icon", texture)
        assets.save_loaded_asset(material, False)
        given += 1
unreal.log_warning("[Icons] {} Bilder importiert, {} Materialien haben jetzt ein Bild".format(len(tasks), given))
