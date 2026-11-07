# Lists the materials of the bag that have no picture yet, for the icon renderer Tools/Loot/vael_icons_auto.py.
# Writes C:/Projekte/Vael_Quellen/Icons/auto_list.json: asset name, category, ingredient kind, colour, way, rarity.
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ExportIconList.py" -unattended -nosplash

import json
import os

import unreal

FOLDER = "/Game/Vael/Items/Materials"
OUT = "C:/Projekte/Vael_Quellen/Icons/auto_list.json"

def enum_name(asset, prop):
    try:
        value = asset.get_editor_property(prop)
    except Exception:
        return "NONE"
    text = str(value)
    return text.split(".")[-1].split(":")[0].strip("<> ")


entries = []
for path in unreal.EditorAssetLibrary.list_assets(FOLDER, recursive=False):
    asset = unreal.load_asset(path)
    if not isinstance(asset, unreal.VaelMaterial):
        continue
    if asset.get_editor_property("icon") is not None:
        continue
    color = asset.get_editor_property("color")
    entries.append({
        "asset": asset.get_name(),
        "name": str(asset.get_editor_property("display_name")),
        "category": enum_name(asset, "category"),
        "kind": enum_name(asset, "kind"),
        "way": enum_name(asset, "way"),
        "rarity": enum_name(asset, "rarity"),
        "color": [color.r, color.g, color.b],
    })

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8") as f:
    json.dump(entries, f, ensure_ascii=False, indent=1)
unreal.log_warning("[Icons] {} Materialien ohne Bild -> {}".format(len(entries), OUT))
