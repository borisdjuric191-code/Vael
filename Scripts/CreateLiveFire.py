# Feuer-Baukasten, live test (Weg B): a fire simulated while the game runs, with the 3D gas solver of the NiagaraFluids
# plugin (Epic's engine plugin, not a pack). Copies the plugin's fire template as NS_Feuer_Live, so the game has its own
# system to tune; UVaelFireComponent spawns it for a fire with bLive and sets its user parameters.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateLiveFire.py"

import unreal

SOURCE = "/NiagaraFluids/Templates/Gas/3D/Systems/Grid3D_Gas_Fire"
TARGET = "/Game/Vael/Effects/Feuer/Live/NS_Feuer_Live"

assets = unreal.EditorAssetLibrary
if assets.does_asset_exist(TARGET):
    unreal.log_warning("[CreateLiveFire] exists " + TARGET)
else:
    # The commandlet does not scan plugin content, so the template is loaded and copied as an object
    original = unreal.load_asset(SOURCE)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    copy = tools.duplicate_asset("NS_Feuer_Live", "/Game/Vael/Effects/Feuer/Live", original) if original is not None else None
    if copy is None:
        unreal.log_warning("[CreateLiveFire] NOTE could not copy " + SOURCE)
    else:
        assets.save_asset(TARGET, False)
        unreal.log_warning("[CreateLiveFire] made " + TARGET)
