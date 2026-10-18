# Applies the wave look of CreateWaterKit.py (WAVE_MAGE_SCALARS / WAVE_MAGE_COLORS) to MI_Water_Wave_Mage when it exists already,
# because CreateWaterKit.py never changes existing instances. Run after CreateWaterKit.py, with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/TuneWaterWave.py" -EnablePlugins=PythonScriptPlugin

import re

import unreal

SOURCE = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()) + "Scripts/CreateWaterKit.py"

with open(SOURCE, encoding="utf-8") as file:
    text = file.read()
scalars = eval(re.search(r"^WAVE_MAGE_SCALARS = (\{.*\})$", text, re.M).group(1))
colors = eval(re.search(r"^WAVE_MAGE_COLORS = (\{.*\})$", text, re.M).group(1))

library = unreal.MaterialEditingLibrary
instance = unreal.load_asset("/Game/VFX/Water/Materials/MI_Water_Wave_Mage")
for key, value in scalars.items():
    library.set_material_instance_scalar_parameter_value(instance, key, value)
for key, value in colors.items():
    library.set_material_instance_vector_parameter_value(instance, key, unreal.LinearColor(value[0], value[1], value[2], 1.0))
library.update_material_instance(instance)
unreal.EditorAssetLibrary.save_loaded_asset(instance, False)
