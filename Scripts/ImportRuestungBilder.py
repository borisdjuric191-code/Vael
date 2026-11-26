# Imports the pictures of the Glutpanzer for the engineer's toolbox (Tools/Ingenieur/vael_glutpanzer_bilder.py): one picture per
# armour piece and the Glutzeichnung of the whole armour, into /Game/Vael/UI/HUD/Ruestung. Always replaces them, and the
# painted toolbox (T_HUD_Gemalt_Inventar_Ingenieur) too.
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ImportRuestungBilder.py"

import os

import unreal

SOURCES = [
    # 26.11.: the other Tripo sets (Aschenjaeger, Aschenjaeger2, Schwarzglut2 = Glutkrone)
    ("C:/Projekte/Vael_Quellen/Ingenieur/Ruestung_Bilder", "T_Ruestung_", "/Game/Vael/UI/HUD/Ruestung"),
    ("C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/08_bilder", "T_Ruestung_", "/Game/Vael/UI/HUD/Ruestung"),
    ("C:/Projekte/Vael/Scripts/Data/HUD/Gemalt", "T_HUD_Gemalt_Inventar_Ingenieur", "/Game/Vael/UI/HUD/Gemalt"),
]

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary

for folder, prefix, destination in SOURCES:
    for file in sorted(os.listdir(folder)):
        if not file.lower().endswith(".png") or not file.startswith(prefix):
            continue
        name = os.path.splitext(file)[0]
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(folder, file))
        task.set_editor_property("destination_path", destination)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", False)
        tools.import_asset_tasks([task])
        texture = unreal.load_asset(destination + "/" + name)
        if texture is None:
            unreal.log_error("[Vael] Ruestungsbild nicht importiert: " + file)
            continue
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("srgb", True)
        texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
        texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
        assets.save_loaded_asset(texture, False)
        unreal.log("[Vael] Ruestungsbild: " + destination + "/" + name)
