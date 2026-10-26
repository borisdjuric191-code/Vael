# Glimmklee arm look after Boris' feedback 2026-10-26: no long stalks, more and finer glowing clover leaves instead.
# Sets the values on DA_Pflanzenarm_Glimmklee and MI_Halm_Glimmklee (both made by CreatePflanzenarmKit.py the same day):
# the fingers end in short stalks with a clover leaf instead of the flame bud, many glowing leaves overgrow the arm,
# no golden wild stalks.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/TuneGlimmklee.py"

import unreal

FOLDER = "/Game/Vael/Characters/Floromant/Pflanzenarm"
library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary

DATA = {
    "finger_length": 7.0,
    "finger_thickness": 0.18,
    "tip_size": 12.0,
    "leaf_count": 18,
    "leaf_size": 12.0,
    "leaf_open_angle": 30.0,
    "wild_count": 0,
    "overgrowth_wild_count": 0,
    "vine_count": 2,
    "vine_thickness": 0.6,
}

MATERIAL = {
    "Leuchten": 0.3,
    "Leuchtfarbe": (0.35, 1.0, 0.15),
    "Silber": 0.0,
    "Glut": 2.0,
}


def main():
    data = unreal.load_asset(FOLDER + "/DA_Pflanzenarm_Glimmklee")
    leaf = unreal.load_asset(FOLDER + "/Glimmklee/SM_Glimmklee_Blatt")
    if data is None or leaf is None:
        unreal.log_warning("[TuneGlimmklee] DA_Pflanzenarm_Glimmklee or SM_Glimmklee_Blatt missing, run CreatePflanzenarmKit.py first")
        return
    data.set_editor_property("tip_mesh", leaf)
    for key, value in DATA.items():
        data.set_editor_property(key, value)
    assets.save_loaded_asset(data, False)

    instance = unreal.load_asset(FOLDER + "/MI_Halm_Glimmklee")
    for key, value in MATERIAL.items():
        if isinstance(value, tuple):
            library.set_material_instance_vector_parameter_value(instance, key, unreal.LinearColor(value[0], value[1], value[2], 1.0))
        else:
            library.set_material_instance_scalar_parameter_value(instance, key, value)
    assets.save_loaded_asset(instance, False)
    unreal.log_warning("[TuneGlimmklee] done")


main()
