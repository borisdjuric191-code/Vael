# Pfuhlnatter – Skripte

Arbeitsdateien liegen außerhalb des Repos in `C:\Projekte\Vael_Quellen\Pfuhlnatter` (02_tripo = Tripo-FBX, arbeit, vorschau, export).
Alles in Blender selbst gebaut: Skelett, faltbare Flossen, Animationen (Boris 2026-10-28: keine weiteren 3D-Modelle).

| Skript | Was |
|---|---|
| 01_vorbereiten.py | FBX laden, Kopf nach +X, 4,2 m lang, Ursprung am Boden in der Mitte |
| 02_analyse.py | Texturfarbe je Vertex (helle Flossenhäute), arbeit/analyse_roh.npz |
| 03_skelett.py | Mittellinie, 26 Rumpfknochen + Kopf, Kronen- und Flossenknochen, Gewichte (übers Netz geglättet), Falt-Test |
| 04_animationen.py | Kriechen, Schwimmen, Aufrichten, Kampf, Spucken, Biss, Ablegen, Tod (30 Bilder/s) |
| 05_export.py | export/SK_Pfuhlnatter.fbx + Tripo-Texturen |
| 06a_unreal_aufraeumen.py | alte Animationen vor einem frischen Import entfernen (Mesh, Skelett, Physik halten sich gegenseitig fest: im Editor löschen) |
| 07_unreal_giftmaterial.py | M_Pfuhlnatter_Gift (erste Giftpfützen, seit 08 nicht mehr benutzt) |
| 08_unreal_giftwasser.py | MI_Water_Gift (M_Water in Giftgrün) + DA_ElementSource_Gift (bPoison): Spuckkugeln und Giftpfützen wie das Wasser-Kit |
| 06_unreal_import.py | Import nach /Game/Vael/Kreaturen/Pfuhlnatter, A_Pfuhlnatter_<Aktion>, MI_Pfuhlnatter (M_Kreatur_Master) |

Blender: `blender -b --factory-startup -P Tools/Pfuhlnatter/<skript>`. Unreal-Skripte bei geschlossenem Editor über
`UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script=<skript>`.

Flossen falten: höchstens 50° nach hinten drehen und auf 38 % Länge einziehen (eine große Haut an einem Knochen weiter
zu drehen ergibt Bretter). Flossen am Halsende gehören zur Krone (Kind von `kopf`), sonst reißt beim Liegen eine Haut
zwischen Kopf und Hals auf. Der Schwanzfächer ist ein einziger Knochen.

Import-Fallen (Interchange, UE 5.8): Über ein bestehendes SK_Pfuhlnatter importiert kommen keine Animationen mit, und
`destination_name`/`replace_existing` beim Mesh lassen sie ebenfalls weg. Löschen und Import im selben Lauf hinterlassen
ein leeres Skelett; ein Import über ein leeres Skelett füllt es wieder auf.

Neuimport nach geänderten Gewichten: 05 exportieren, dann 06 – liegt SK_Pfuhlnatter schon da, tauscht Interchange nur das
Mesh aus (Skelett und Animationen bleiben). Der klassische FBX-Importer liest diese FBX in Metern und behält beim
Ersetzen seine ersten Einstellungen (6-cm-Natter) – nicht benutzen. Krone: Alles, was weiter als SCHAEDEL_RADIUS von
der Kopfachse absteht (nicht unterhalb des Kopfes), hängt ganz an seinem Kronenknochen, damit es eingezogen verschwindet.
