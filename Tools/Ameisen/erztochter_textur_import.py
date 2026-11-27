"""
Unreal (Editor geschlossen, -run=pythonscript): Körperfarbe der Erztochter Richtung Elfenbein (Boris 27.11.).
Importiert die umgefärbte Grundfarbe und die abgeschwächte Metallkarte (Vael_Quellen/Erztochter/03_blender/elfenbein,
erzeugt aus den Tripo-Texturen: 70 % Elfenbein mit Bernstein an den dunklen Rändern, 30 % Original; Metall x 0,15,
sonst glänzt das Chitin wie Messing) über T_Erztochter_basecolor und T_Erztochter_metallic.
"""
import os

import unreal

QUELLE = "C:/Projekte/Vael_Quellen/Erztochter/03_blender/elfenbein"
ZIEL = "/Game/Vael/Kreaturen/Eisenschlund/Erztochter"

tasks = []
for art in ("basecolor", "metallic"):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", os.path.join(QUELLE, "Erztochter_{}.png".format(art)))
    t.set_editor_property("destination_path", ZIEL)
    t.set_editor_property("destination_name", "T_Erztochter_{}".format(art))
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", True)
    tasks.append((art, t))
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t for _, t in tasks])
eal = unreal.EditorAssetLibrary
for art, _ in tasks:
    tex = eal.load_asset("{}/T_Erztochter_{}".format(ZIEL, art))
    if tex is None:
        unreal.log_error("[Vael] FEHLT T_Erztochter_" + art)
        continue
    if art != "basecolor":
        tex.set_editor_property("srgb", False)
    eal.save_asset(tex.get_path_name(), only_if_is_dirty=False)
    unreal.log_warning("[Vael] Textur ersetzt: T_Erztochter_" + art)
