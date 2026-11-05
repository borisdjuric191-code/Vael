# Exports the Tripo model and textures of an Aschenmark plant (read only) for Blender work on its shadow form (Boris 2026-11-05:
# the first shadow form, the Blutrose). Writes Saved/Pflanzenquellen/<Name>/SM_<Name>.fbx and T_<Name>_*.png. Content is not
# changed. Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ExportPlantSource.py Russrose"

import os
import sys
import unreal

NAME = sys.argv[-1] if len(sys.argv) > 1 else "Russrose"
SOURCE = "/Game/Vael/Requisiten/Aschenmark/" + NAME
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Saved", "Pflanzenquellen", NAME)
os.makedirs(OUT, exist_ok=True)


def export(asset_path, filename):
    asset = unreal.load_asset(asset_path)
    if asset is None:
        unreal.log_warning("[Quelle] missing " + asset_path)
        return
    task = unreal.AssetExportTask()
    task.set_editor_property("object", asset)
    task.set_editor_property("filename", os.path.join(OUT, filename))
    task.set_editor_property("automated", True)
    task.set_editor_property("prompt", False)
    task.set_editor_property("replace_identical", True)
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log_warning("[Quelle] {} {}".format("exported" if ok else "FAILED", filename))


export("{}/SM_{}".format(SOURCE, NAME), "SM_{}.fbx".format(NAME))
for kind in ("basecolor", "normal", "roughness", "metallic"):
    export("{0}/T_{1}_{2}".format(SOURCE, NAME, kind), "T_{}_{}.png".format(NAME, kind))
