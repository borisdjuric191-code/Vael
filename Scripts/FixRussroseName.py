# Boris 2026-11-04: the light name of DA_Pflanze_Russrose was stored garbled ("RuÃ\x9frose", UTF-8 read as Latin-1).
# Sets it to "Rußrose" again, and to "Russrose" if the sharp s does not survive the round trip.
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/FixRussroseName.py"

import unreal

PATH = "/Game/Vael/Characters/Floromant/Pflanzen/DA_Pflanze_Russrose"

data = unreal.load_asset(PATH)
data.set_editor_property("light_name", unreal.Text("Ru\u00dfrose"))
if str(data.get_editor_property("light_name")) != "Ru\u00dfrose":
    data.set_editor_property("light_name", unreal.Text("Russrose"))
unreal.EditorAssetLibrary.save_loaded_asset(data, False)
unreal.log_warning("[FixRussroseName] light name now " + repr(str(unreal.load_asset(PATH).get_editor_property("light_name"))))
