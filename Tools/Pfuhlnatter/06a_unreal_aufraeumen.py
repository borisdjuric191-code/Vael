"""Step 6a: before importing the Pfuhlnatter anew, removes 06_unreal_import.py's own older import from
/Game/Vael/Kreaturen/Pfuhlnatter/: mesh, skeleton, physics asset and animations (textures and materials stay).

Interchange leaves the animations out when it imports over an existing mesh, and deleting in the same run as the
import leaves it with an unreadable skeleton, so this is a run of its own. The assets are loaded before they are
deleted: delete_asset on an asset that isn't loaded reports success but leaves the file. Run with the editor closed:
  UnrealEditor-Cmd.exe <Vael.uproject> -run=pythonscript -script="<this file>"
"""
import os

import unreal

ZIEL = "/Game/Vael/Kreaturen/Pfuhlnatter"
AKTIONEN = ("Kriechen", "Schwimmen", "Aufrichten", "Kampf", "Spucken", "Biss", "Ablegen", "Tod")
eal = unreal.EditorAssetLibrary

unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([ZIEL], True)
namen = ["A_Pfuhlnatter_" + a for a in AKTIONEN] + ["SK_Pfuhlnatter_PhysicsAsset", "SK_Pfuhlnatter", "SK_Pfuhlnatter_Skeleton"]
geladen = [eal.load_asset("{}/{}".format(ZIEL, n)) for n in namen if eal.does_asset_exist("{}/{}".format(ZIEL, n))]
geladen = [a for a in geladen if a is not None]
unreal.log_warning("[Pfuhlnatter] entfernt {} Assets: {}".format(len(geladen), eal.delete_loaded_assets(geladen)))
ordner = os.path.join(unreal.Paths.project_content_dir(), "Vael", "Kreaturen", "Pfuhlnatter")
uebrig = [n for n in namen if os.path.exists(os.path.join(ordner, n + ".uasset"))]
if uebrig:
    unreal.log_warning("[Pfuhlnatter] noch da (erneut ausfuehren): {}".format(uebrig))
