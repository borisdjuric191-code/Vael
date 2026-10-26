# Funkenmohn arm look after the first screenshots 2026-10-26: the ember glow of the stalk material ("Glut" lights the red parts of
# the picture) blew the shut buds out to white. Turns it down on MI_Halm_Funkenmohn (made by CreatePflanzenarmKit.py the same day),
# so the bud stays a deep glowing red; the charge brightens it from the game.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/TuneFunkenmohn.py"

import unreal

FOLDER = "/Game/Vael/Characters/Floromant/Pflanzenarm"
library = unreal.MaterialEditingLibrary

VALUES = {
    "Glut": 0.9,
    "Leuchten": 0.05,
}

instance = unreal.load_asset(FOLDER + "/MI_Halm_Funkenmohn")
if instance is None:
    unreal.log_warning("[TuneFunkenmohn] MI_Halm_Funkenmohn missing, run CreatePflanzenarmKit.py first")
else:
    for key, value in VALUES.items():
        library.set_material_instance_scalar_parameter_value(instance, key, value)
    unreal.EditorAssetLibrary.save_loaded_asset(instance, False)
    unreal.log_warning("[TuneFunkenmohn] done")
