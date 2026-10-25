"""Copies of pack effects that Vael uses for another purpose, so each copy gets its own tuning in the magic settings
(Project Settings -> Vael Magic -> Pack Tunings). Only new assets are made; the packs stay untouched.
Run headless with the editor closed:
UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script=Scripts/CreatePackEffectCopies.py
"""
import unreal

FOLDER = "/Game/Vael/Effects/Paket"

COPIES = [
    # Smoke of the mushroom cloud of the Feuerball
    ("/Game/Fire_Solution_FX/Fire_Common/NS_FireCommon", "NS_Vael_Feuerpilz_Rauch"),
    # Small fire burst where a Funke hits
    ("/Game/Fire_Solution_FX/Fire_Small/NS_FireSmall_var2", "NS_Vael_Funke_Treffer"),
]

tools = unreal.AssetToolsHelpers.get_asset_tools()
for source, name in COPIES:
    path = "{}/{}".format(FOLDER, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log("Vael: {} exists, left untouched".format(path))
        continue
    original = unreal.load_asset(source)
    if original is None:
        unreal.log_error("Vael: pack effect {} missing".format(source))
        continue
    copy = tools.duplicate_asset(name, FOLDER, original)
    if copy is not None and unreal.EditorAssetLibrary.save_loaded_asset(copy, False):
        unreal.log("Vael: created {}".format(path))
    else:
        unreal.log_error("Vael: {} could not be created".format(path))
