# Lists every material of the bag for the picture renderer Tools/Loot/vael_material_bilder.py (Boris 17.11.: all materials get
# a new picture). Writes C:/Projekte/Vael_Quellen/Icons/material_list.json: asset name, shown name, category, ingredient kind,
# colour, way, rarity, description.
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ExportMaterialList.py" -unattended -nosplash

import json
import os

import unreal

FOLDER = "/Game/Vael/Items/Materials"
OUT = "C:/Projekte/Vael_Quellen/Icons/material_list.json"


def enum_name(asset, prop):
    try:
        value = asset.get_editor_property(prop)
    except Exception:
        return "NONE"
    return str(value).split(".")[-1].split(":")[0].strip("<> ").upper()


entries = []
for path in unreal.EditorAssetLibrary.list_assets(FOLDER, recursive=False):
    asset = unreal.load_asset(path)
    if not isinstance(asset, unreal.VaelMaterial):
        continue
    color = asset.get_editor_property("color")
    entries.append({
        "asset": asset.get_name(),
        "name": str(asset.get_editor_property("display_name")),
        "category": enum_name(asset, "category"),
        "kind": enum_name(asset, "ingredient_kind"),
        "way": enum_name(asset, "way"),
        "rarity": enum_name(asset, "rarity"),
        "color": [color.r, color.g, color.b],
        "text": str(asset.get_editor_property("description")),
    })

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8") as f:
    json.dump(entries, f, ensure_ascii=False, indent=1)
unreal.log("[Vael] Materialliste: {} Eintraege".format(len(entries)))
