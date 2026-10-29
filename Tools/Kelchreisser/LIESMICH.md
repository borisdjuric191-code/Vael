# Kelchreißer – Skripte

Arbeitsdateien liegen außerhalb des Repos in `C:\Projekte\Vael_Quellen\Kelchreisser` (02_tripo = Tripo-FBX, arbeit, vorschau, export).
Boris 2026-10-29: 4–5 m Schulterhöhe (fast ein Bosskampf), alle Rücken-Ranken bekommen ein Maul, das eigenständig zuschnappt.

| Skript | Was |
|---|---|
| 01_vorbereiten.py | FBX laden, Blütenkopf nach +X (Tripo schaut nach -Y), 4,5 m Schulterhöhe, Ursprung am Boden |
| 02_analyse.py | Vertices, Texturfarbe, Kanten nach arbeit/analyse_roh.npz |
| 03_ranken.py | **venv-Python** (numpy+scipy): Ranken finden, Mittellinie, Radius, Knospen → ranken.json/.npz |
| 04_maeuler.py | Knospe von Ranke 3 auf die kahlen Ranken kopieren (Hakenspitze ab), Schichten ranke/maul/kiefer |
| 05_skelett.py | 103 Knochen (Körper, Arme, Beine, 5 Blütenblätter, 4 Innenkiefer, Rankenketten mit Knospenkiefern), Gewichte, Testpose |
| 06_animationen.py | Stehen_Idle, Galopp, Gehen_Aufrecht, Schlag, Klaue, Rankenbiss, Schrei (30 Bilder/s, auf der Stelle) |
| 07_export.py | export/SK_Kelchreisser.fbx + Tripo-Texturen |
| 08_unreal_import.py | Import nach /Game/Vael/Kreaturen/Kelchreisser, A_Kelchreisser_<Aktion>, MI_Kelchreisser (M_Kreatur_Master) |

Blender: `blender -b --factory-startup -P Tools/Kelchreisser/<skript>`; 03 mit
`C:/Users/boris/Tools/wf_generator_venv/Scripts/python.exe -I Tools/Kelchreisser/03_ranken.py`.
Unreal-Skripte bei geschlossenem Editor über `UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script=<skript> -DisablePlugin=Fab`.

Ranken: Die grünen Röhren werden über kürzeste Wege durch das Grün vom Rücken aus gefunden; Ringe gleichen
Oberflächenabstands geben die Mitte. Nahe am Rücken geht das Grün in die Hautfarbe über: die Röhre wird geometrisch
weiter verfolgt (höchstens 0,5 m, nur nach unten). Der kurze dicke Stummel links liegt flach am Rücken, seine Wurzel
steht von Hand in `WURZEL_VON_HAND`.

Gewichte: Die Voxel-Hülle für Blenders Hitze-Gewichte kommt aus dem unveränderten Modell von Schritt 1 (die
abgeschnittenen Ranken und kopierten Knospen haben offene Ränder, daran zerfällt die Hülle) und nur ihr größtes
Stück zählt (mit losen Dornspitzen findet der Löser gar keine Lösung). Dornen auf den Ranken wachsen über das Netz
in die Rankengewichte hinein (`DORN_*`).

Animationen: Hände und Füße stehen über IK-Ziele (leere Objekte), Hand und Mittelfuß drehen wie ihr Ziel; das Ergebnis
wird Bild für Bild in reine Knochenschlüssel gerechnet, die Constraints danach entfernt. Kiefer, Blätter und
Knospenhälften drehen um ihre eigene X-Achse (Rolle in 05 gesetzt).

Neuimport nach geänderten Animationen: wie bei der Pfuhlnatter (Tools/Pfuhlnatter/LIESMICH.md) – Interchange holt über ein
bestehendes SK_ keine Animationen, Boris löscht dafür SK_/A_ im Editor.
