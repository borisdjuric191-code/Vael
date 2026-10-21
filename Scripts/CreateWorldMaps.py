# Makes the maps of the levels for the mini map and the big map (UVaelWorldMap), from the map pictures the blockout kits paint.
#
# Per map: imports the picture as /Game/Vael/UI/Karten/T_Karte_<Name> (clamped at the edges, never streamed) and writes
# DA_Karte_<Name> with the world square the picture covers and the names on it. Which level shows which map is set in
# Config/DefaultGame.ini under [/Script/Vael.VaelUISettings] LevelMaps (Project Settings > Game > Vael UI > Map).
# Rerunning replaces picture and data; nothing else is touched.
#
# Run with the editor closed, after the kit's wf_karte.py:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateWorldMaps.py" -EnablePlugins=PythonScriptPlugin

import json
import os

import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FOLDER = "/Game/Vael/UI/Karten"
# Name of the map: (picture, data of the kit)
MAPS = {
    "Wurzelforst": (os.path.join(ROOT, "Blockout", "Vael_Wurzelforst_Blockout", "T_WF_Karte.png"),
                    os.path.join(ROOT, "Blockout", "Vael_Wurzelforst_Blockout", "WF_Karte.json")),
}

assets = unreal.EditorAssetLibrary


def log(message):
    unreal.log_warning("[CreateWorldMaps] " + message)


def import_picture(source, name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset("{}/{}".format(FOLDER, name))
    # Outside the picture lies open sea: the edge pixels are stretched instead of the map repeating
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    # Drawn on the HUD at any size: all mips always there
    texture.set_editor_property("never_stream", True)
    assets.save_loaded_asset(texture, False)
    return texture


def main():
    made = []
    for name, (picture_file, data_file) in MAPS.items():
        if not (os.path.isfile(picture_file) and os.path.isfile(data_file)):
            log("{}: picture or data missing, run the kit's wf_karte.py first".format(name))
            continue
        data = json.load(open(data_file, encoding="utf-8"))
        texture = import_picture(picture_file, "T_Karte_" + name)

        path = "{}/DA_Karte_{}".format(FOLDER, name)
        if assets.does_asset_exist(path):
            world_map = unreal.load_asset(path)
        else:
            world_map = unreal.AssetToolsHelpers.get_asset_tools().create_asset("DA_Karte_" + name, FOLDER, unreal.VaelWorldMap,
                                                                               unreal.DataAssetFactory())
        world_map.set_editor_property("title", data["title"])
        world_map.set_editor_property("picture", texture)
        world_map.set_editor_property("world_min", unreal.Vector2D(data["world_min"][0], data["world_min"][1]))
        world_map.set_editor_property("world_size", float(data["world_size"]))
        if "focus_min" in data:
            world_map.set_editor_property("focus_min", unreal.Vector2D(data["focus_min"][0], data["focus_min"][1]))
            world_map.set_editor_property("focus_max", unreal.Vector2D(data["focus_max"][0], data["focus_max"][1]))
        labels = []
        for entry in data["labels"]:
            label = unreal.VaelMapLabel()
            label.set_editor_property("name", entry["name"])
            label.set_editor_property("location", unreal.Vector2D(entry["world"][0], entry["world"][1]))
            label.set_editor_property("size", int(entry["size"]))
            labels.append(label)
        world_map.set_editor_property("labels", labels)
        assets.save_loaded_asset(world_map, False)
        made.append("{} ({} names)".format(name, len(labels)))

    log("maps made: {}".format(", ".join(made) or "none"))


main()
