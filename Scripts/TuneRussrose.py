# Rußrose arm look after the first screenshots 2026-10-26: the glow of the stalk material washed the black petals and the thorn
# vines mint green. Turns the glow nearly off on MI_Halm_Russrose (made by CreatePflanzenarmKit.py the same day), so the rose keeps
# its own dark petals and green heart; the heart's light comes from the game.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/TuneRussrose.py"

import unreal

FOLDER = "/Game/Vael/Characters/Floromant/Pflanzenarm"
library = unreal.MaterialEditingLibrary

VALUES = {
    "Leuchten": 0.03,
    "Silber": 0.0,
    "Grundton": (1.0, 1.0, 1.0),
}

instance = unreal.load_asset(FOLDER + "/MI_Halm_Russrose")
if instance is None:
    unreal.log_warning("[TuneRussrose] MI_Halm_Russrose missing, run CreatePflanzenarmKit.py first")
else:
    for key, value in VALUES.items():
        if isinstance(value, tuple):
            library.set_material_instance_vector_parameter_value(instance, key, unreal.LinearColor(value[0], value[1], value[2], 1.0))
        else:
            library.set_material_instance_scalar_parameter_value(instance, key, value)
    unreal.EditorAssetLibrary.save_loaded_asset(instance, False)
    unreal.log_warning("[TuneRussrose] done")
