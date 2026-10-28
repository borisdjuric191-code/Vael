"""Step 6a: before importing the Pfuhlnatter anew, removes 06_unreal_import.py's own older import from
/Game/Vael/Kreaturen/Pfuhlnatter/: mesh, skeleton, physics asset and animations (textures and MI_Pfuhlnatter stay).

Interchange leaves the animations out when it imports over an existing mesh, and deleting in the same run as the
import leaves it with an unreadable skeleton, so this is a run of its own. Run with the editor closed:
  UnrealEditor-Cmd.exe <Vael.uproject> -run=pythonscript -script="<this file>"
"""
import unreal

ZIEL = "/Game/Vael/Kreaturen/Pfuhlnatter"
AKTIONEN = ("Kriechen", "Schwimmen", "Aufrichten", "Kampf", "Spucken", "Biss", "Ablegen", "Tod")
eal = unreal.EditorAssetLibrary

for name in ["SK_Pfuhlnatter", "SK_Pfuhlnatter_Skeleton", "SK_Pfuhlnatter_PhysicsAsset"] + ["A_Pfuhlnatter_" + a for a in AKTIONEN]:
    pfad = "{}/{}".format(ZIEL, name)
    if eal.does_asset_exist(pfad):
        unreal.log_warning("[Pfuhlnatter] entfernt {}: {}".format(name, eal.delete_asset(pfad)))
