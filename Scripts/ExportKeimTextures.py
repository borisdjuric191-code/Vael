# Exports a few scanned textures (Megascans, read only) as PNG for the Blender renders of the Keimgeflecht (Boris 2026-11-05:
# "realistisch, nicht KI-perfekt"): linen and fibre macros for the papyrus, the old tree branch bark for the roots of the
# frame, English ivy leaves, the root and moss forest ground. Nothing in Content is changed.
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ExportKeimTextures.py"

import os
import unreal

# into Saved (not into git: the branch alone is 100 MB as PNG)
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Saved", "Scans")
os.makedirs(OUT, exist_ok=True)

TEXTURES = {
    "Leinen": "/Game/Vael/Characters/Menschen/MH_Common/Optional/Clothing/Common/Materials/Textures/Clothing/Macros/macro_linen_rbess3s_4K",
    "Fasern": "/Game/Vael/Characters/Menschen/MH_Common/Optional/Clothing/Common/Materials/Textures/Clothing/Macros/macro_fibers_distort_h_A",
    "Ast_B": "/Game/Fab/Megascans/3D/Old_Tree_Branch_whnnajh/Raw/whnnajh_tier_0/Textures/T_whnnajh_8K_B",
    "Ast_N": "/Game/Fab/Megascans/3D/Old_Tree_Branch_whnnajh/Raw/whnnajh_tier_0/Textures/T_whnnajh_8K_N",
    "Efeu_B": "/Game/Fab/Megascans/Plants/English_Ivy_xfcnebhqx/Low/xfcnebhqx_tier_3/Textures/T_xfcnebhqx_1K_B",
    "Efeu_N": "/Game/Fab/Megascans/Plants/English_Ivy_xfcnebhqx/Low/xfcnebhqx_tier_3/Textures/T_xfcnebhqx_1K_N",
    "Waldboden_B": "/Game/Fab/Megascans/Surfaces/Nordic_Forest_Ground_Root_Moss_Coarse_xiekec0/Low/xiekec0_tier_3/Textures/T_xiekec0_1K_B",
    "Waldboden_N": "/Game/Fab/Megascans/Surfaces/Nordic_Forest_Ground_Root_Moss_Coarse_xiekec0/Low/xiekec0_tier_3/Textures/T_xiekec0_1K_N",
}

for name, path in TEXTURES.items():
    texture = unreal.load_asset(path)
    if texture is None:
        unreal.log_warning("[Scans] missing " + path)
        continue
    for ext in ("png", "tga"):
        task = unreal.AssetExportTask()
        task.set_editor_property("object", texture)
        task.set_editor_property("filename", os.path.join(OUT, name + "." + ext))
        task.set_editor_property("automated", True)
        task.set_editor_property("prompt", False)
        task.set_editor_property("replace_identical", True)
        if unreal.Exporter.run_asset_export_task(task):
            unreal.log_warning("[Scans] exported {} as {}".format(name, ext))
            break
    else:
        unreal.log_warning("[Scans] could not export " + name)
