"""Step 8a: before importing the Kelchreisser anew, removes 08_unreal_import.py's own older import from
/Game/Vael/Kreaturen/Kelchreisser/: mesh, skeleton, physics asset and animations (textures and materials stay).

Interchange leaves the animations out when it imports over an existing mesh, and deleting in the same run as the
import leaves it with an unreadable skeleton, so this is a run of its own. The assets are loaded before they are
deleted: delete_asset on an asset that isn't loaded reports success but leaves the file. Run with the editor closed:
  UnrealEditor-Cmd.exe <Vael.uproject> -run=pythonscript -script="<this file>"
"""
import os

import unreal

ZIEL = "/Game/Vael/Kreaturen/Kelchreisser"
# All actions this creature ever had (renamed ones too), so none is left behind
AKTIONEN = ("Stehen_Idle", "Galopp", "Gehen_Aufrecht", "Schlag", "Prankenhieb", "Klaue", "Rankenbiss", "Biss", "Sprung", "Schrei", "Lauern")
eal = unreal.EditorAssetLibrary

unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([ZIEL], True)
namen = ["A_Kelchreisser_" + a for a in AKTIONEN] + ["SK_Kelchreisser_PhysicsAsset", "SK_Kelchreisser", "SK_Kelchreisser_Skeleton"]
geladen = [eal.load_asset("{}/{}".format(ZIEL, n)) for n in namen if eal.does_asset_exist("{}/{}".format(ZIEL, n))]
geladen = [a for a in geladen if a is not None]
unreal.log_warning("[Kelchreisser] entfernt {} Assets: {}".format(len(geladen), eal.delete_loaded_assets(geladen)))
ordner = os.path.join(unreal.Paths.project_content_dir(), "Vael", "Kreaturen", "Kelchreisser")
uebrig = [n for n in namen if os.path.exists(os.path.join(ordner, n + ".uasset"))]
if uebrig:
    unreal.log_warning("[Kelchreisser] noch da (erneut ausfuehren): {}".format(uebrig))
