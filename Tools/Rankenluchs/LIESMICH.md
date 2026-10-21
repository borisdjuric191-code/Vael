# Rankenluchs – Skripte

Kopie der Skripte aus `Desktop\Vael_Rankenluchs\skripte` (dort laufen sie, mit `Rankenluchs_tripo.glb` daneben).
Auftrag: `AUFTRAG.md`. Megascans- und andere Fab-Inhalte kommen nie in Blender; die Blatttextur des Kamms setzt Boris in Unreal ein.

| Skript | Was |
|---|---|
| 01_vorbereiten.py | GLB laden, Kopf nach +X, 0,70 m Schulterhöhe, Ursprung zwischen den Pfoten |
| 02_analyse_punkte.py | Landmarken (Pfoten, Beine, Rücken, Ohren, Nase) vermessen |
| 02_skelett.py | 38 Knochen, Gewichte über eine geschlossene Voxel-Hülle (in 10-facher Größe), Testposen |
| 03_kamm_augen.py | 10 Kammblätter mit eigenen Knochen, Leuchtscheiben auf den Augen |
| 04_animationen.py | Stehen_Idle, Tarn_Ruhe (Kamm wehend), Erwachen, Sprung_Angriff, Laufen, Rennen |
| 05_export.py | FBX und die Tripo-Texturen |
| 06_unreal_import.py | Import nach /Game/Vael/Kreaturen/Rankenluchs |
| 07_unreal_materialien.py | M_Rankenluchs, M_Rankenkamm, M_Rankenluchs_Augen mit „Verderbnis“, Instanzen Licht/Schatten |
| 08_vorschau_formen.py | Vorschaubilder beider Formen |

Blender-Skripte: `blender -b --factory-startup -P <skript>`; Unreal-Skripte ohne Editor über `-run=pythonscript`.
