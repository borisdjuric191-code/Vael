# Bringt die elf Erze des Eisenschlunds ins Spiel (15.11.2026): erst Materialien und Erntedaten anlegen, dann je Erz das
# aufbereitete Tripo-Modell (Vael_Quellen/<Erz>/04_export, Blender-Modus 'requisite') importieren und in DA_Harvest_<Erz> eintragen.
#
# Mit geschlossenem Editor:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ImportEisenschlundErze.py" -AllowCommandletRendering -unattended -nosplash
import runpy
import sys

import unreal

ERZE = ["Rosteisen", "Schwefelkies", "Funkenerz", "Gruenspan", "Salzspat", "Drahtsilber", "Blutzinnober", "Siechglas", "Rippenspat",
        "Kernerz", "Kobaltbluete"]
PROJEKT = "C:/Projekte/Vael"

runpy.run_path(PROJEKT + "/Scripts/CreateMaterialAssets.py", run_name="__main__")
runpy.run_path(PROJEKT + "/Scripts/CreateHarvestableAssets.py", run_name="__main__")
for erz in ERZE:
    sys.argv = ["vael_kreatur_import.py", "C:/Projekte/Vael_Quellen/" + erz]
    try:
        runpy.run_path(PROJEKT + "/Tools/Kreaturenfabrik/unreal/vael_kreatur_import.py", run_name="__main__")
    except Exception as error:  # noqa: BLE001
        unreal.log_error("[ImportEisenschlundErze] {}: {}".format(erz, error))
unreal.log_warning("[ImportEisenschlundErze] fertig")
