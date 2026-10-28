# Pfuhlnatter – Skripte

Arbeitsdateien liegen außerhalb des Repos in `C:\Projekte\Vael_Quellen\Pfuhlnatter` (02_tripo = Tripo-FBX, arbeit, vorschau).
Alles in Blender selbst gebaut: Skelett, faltbare Flossen, Animationen (Boris 2026-10-28: keine weiteren 3D-Modelle).

| Skript | Was |
|---|---|
| 01_vorbereiten.py | FBX laden, Kopf nach +X, 4,2 m lang, Ursprung am Boden in der Mitte |
| 02_analyse.py | Texturfarbe je Vertex (helle Flossenhäute), arbeit/analyse_roh.npz |
| 03_skelett.py | Mittellinie, 26 Rumpfknochen + Kopf, Kronen- und Flossenknochen, Gewichte, Falt-Test |
| 04_animationen.py | Kriechen, Schwimmen, Aufrichten, Kampf, Spucken, Biss, Ablegen, Tod |

Blender: `blender -b --factory-startup -P Tools/Pfuhlnatter/<skript>`.
Flossen falten: höchstens 50° nach hinten drehen und auf 38 % Länge einziehen (eine große Haut an einem Knochen weiter
zu drehen ergibt Bretter).
