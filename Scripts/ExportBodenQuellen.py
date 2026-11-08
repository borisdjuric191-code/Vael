# Exports the pack surfaces the Aschenmark ground is made of as PNG (read only, the pack assets stay as they are),
# for Tools/Welt/aschenmark_boden.py, which grades and packs them with its own procedural layers.
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ExportBodenQuellen.py" -EnablePlugins=PythonScriptPlugin -unattended

import os

import unreal

OUT = "C:/Projekte/Vael_Quellen/Aschenmark/boden/pack"
SOURCES = {
    "asche": "/Game/MS/Surfaces/Dry_Sandy_Mud_wdunfjf/T_Dry_Sandy_Mud_wdunfjf_{}",
    "schlamm": "/Game/Megascans/Surfaces/HeavyMud/T_HeavyMud_{}",
    "waldboden": "/Game/Megascans/Surfaces/ForestGround/T_ForestGround_{}",
    "kies": "/Game/NiagaraExamples/Gallery/Megascans/Surfaces/Gravel_Ground_xbnefjm/Medium/xbnefjm_tier_2/Textures/T_xbnefjm_2K_{}",
    "moos": "/Game/Fab/Megascans/Surfaces/Nordic_Forest_Ground_Root_Moss_Coarse_xiekec0/Low/xiekec0_tier_3/Textures/T_xiekec0_1K_{}",
}
os.makedirs(OUT, exist_ok=True)
for name, pattern in SOURCES.items():
    for key in ("A", "B", "D", "N", "R", "ORM"):
        path = pattern.format(key)
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            continue
        texture = unreal.load_asset(path)
        task = unreal.AssetExportTask()
        task.set_editor_property("object", texture)
        task.set_editor_property("filename", os.path.join(OUT, "{}_{}.png".format(name, key)))
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_identical", True)
        task.set_editor_property("prompt", False)
        ok = unreal.Exporter.run_asset_export_task(task)
        unreal.log_warning("[ExportBodenQuellen] {} {} {} {}x{} srgb={}".format(name, key, "ok" if ok else "FAILED", texture.blueprint_get_size_x(),
                                                                            texture.blueprint_get_size_y(), texture.get_editor_property("srgb")))
