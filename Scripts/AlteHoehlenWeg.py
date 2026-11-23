# Removes the eight old single cave levels of the Eisenschlund and their minimaps (Boris 2026-11-23: "du kannst alle anderen Karten
# wegtun"). Since 2026-11-22 they are joined in L_Eisenschlund (the workshop, kept), since 2026-11-23 inside L_Vael; nothing leads
# into the old ones any more. Their names live on only as region tags ("L_Gildenmine" ...). Git keeps them in its history.
# With the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/AlteHoehlenWeg.py" -unattended -nosplash
import unreal

ALTE = ["Gildenmine", "FunkenerzAdern", "Olmgrotte", "Gebeinkammer", "Ameisenbau", "KoboldStollen", "Rostschwamm", "TiefsterSchacht"]
assets = unreal.EditorAssetLibrary
weg = []
for name in ALTE:
    for path in ("/Game/Vael/Maps/Hoehlen/L_" + name, "/Game/Vael/UI/Karten/DA_Karte_" + name, "/Game/Vael/UI/Karten/T_Karte_" + name):
        if assets.does_asset_exist(path):
            if assets.delete_asset(path):
                weg.append(path)
            else:
                unreal.log_warning("[AlteHoehlen] konnte nicht loeschen: " + path)
unreal.log_warning("[AlteHoehlen] entfernt ({}): {}".format(len(weg), ", ".join(weg)))
