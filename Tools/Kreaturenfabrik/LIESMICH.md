# Vael – Kreaturenfabrik

Werkzeuge für Station 4, 5 und 7 aus dem Tab „Kreaturenfabrik“ im Vael-Dokument.

## Ordner auf deinem PC

Entpacke alles z. B. nach `D:\Vael_Tools\` und lege die Quellen außerhalb des Unreal-Projekts ab:

```
Vael_Quellen\
  Spannhornkaefer\
    kreatur.json        Steckbrief-Daten (von Claude vorbereitet)
    01_konzept\         Konzeptbild aus Tripo
    02_tripo\           Tripo-Export (.fbx oder .glb) hier hineinlegen
    03_blender\         legt das Skript an
    04_export\          legt das Skript an (SK_Spannhornkaefer.fbx für Unreal)
```

## Ablauf pro Kreatur

1. Tripo-Modell als FBX oder GLB in `02_tripo` legen.
2. Eingabeaufforderung öffnen und ausführen:
   ```
   "C:\Users\boris\Tools\blender-5.2.2-windows-x64\blender.exe" -b -P C:\Projekte\Vael\Tools\Kreaturenfabrik\blender\vael_kreatur_aufbereiten.py -- vorbereiten C:\Projekte\Vael_Quellen\Spannhornkaefer
   ```
3. `03_blender\Spannhornkaefer.blend` in Blender öffnen. Das Vorlage-Skelett sitzt grob, die Knochen bei Bedarf im Bearbeitungsmodus an Beine und Hörner schieben (ein paar Minuten). Speichern.
4. Binden und exportieren:
   ```
   "C:\Users\boris\Tools\blender-5.2.2-windows-x64\blender.exe" -b -P C:\Projekte\Vael\Tools\Kreaturenfabrik\blender\vael_kreatur_aufbereiten.py -- binden C:\Projekte\Vael_Quellen\Spannhornkaefer
   ```
5. In Unreal: Werkzeuge → Python-Skript ausführen → `unreal\vael_kreatur_import.py`, dann den Kreatur-Ordner wählen.

## Einstellungen in kreatur.json

| Feld | Bedeutung |
| --- | --- |
| `laenge_cm` | Gesamtlänge inkl. Hörner und Beine. **Beim Spannhornkäfer noch offen, vorerst 120 cm.** |
| `klasse` | `normal`, `elite` oder `boss` – bestimmt das Polygonbudget |
| `drehung_z_grad` | falls das Tripo-Modell falsch herum schaut (z. B. 180) |
| `tripo_rig_verwenden` | `true`, wenn Tripos Sechsbeiner-Rig gut sitzt; sonst baut das Skript das Vorlage-Skelett |
| `tripo_prompt` | der englische Prompt für das Konzeptbild |

## Was die Skripte erledigen

- Teile vereinen, Maßstab auf echte Größe, Ursprung unten mittig, auf das Polygonbudget reduzieren
- Namen nach Fabrikregeln (`SK_`, `M_`, `T_`, `MI_`), leerer Slot für das gemeinsame Mark-Material
- Vorlage-Skelett der Familie mit festen Knochennamen (bisher: Vielbeiner)
- Gewichtung: erst Blenders automatische, bei KI-Meshes ersatzweise eine robuste Abstands-Gewichtung
- Unreal: Import in `/Game/Vael/Kreaturen/<Region>/<Name>/`, Physik-Asset, Material-Instanz (sobald das Master-Material existiert)

Getestet: Blender-Skript mit Blender 5.1 an einem Ersatzmodell. Das Unreal-Skript ist noch ungetestet und läuft beim ersten echten Import zum ersten Mal.

## Stand auf diesem PC (2026-10-10)

- Diese Skripte liegen versioniert im Vael-Repo unter `Tools\Kreaturenfabrik\` – das ist die maßgebliche Fassung. `C:\Projekte\Vael_Tools` ist nur die erste, unversionierte Kopie.
- Quellen jeder Kreatur (Tripo-Export, .blend, Export-FBX, Testbilder) liegen in `C:\Projekte\Vael_Quellen\<Name>\`, außerhalb des Repos.
- Blender 5.2.2 LTS portabel unter `C:\Users\boris\Tools\blender-5.2.2-windows-x64\` (keine Installation, keine Admin-Rechte nötig). Das Skript `vorbereiten` ist damit getestet.
- Unreal-Schritte ohne offenen Editor (`UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="..."`):
  - Master-Material einmalig: `unreal/vael_kreatur_master.py`
  - Import: `unreal/vael_kreatur_import.py C:/Projekte/Vael_Quellen/<Name>` – mit `--nur-material` nur Material-Instanz und Slots erneuern
  - Testbilder: `UnrealEditor.exe Vael.uproject "-ExecCmds=py <Pfad>/unreal/vael_kreatur_testbild.py"` (Testmap L_Kreaturentest; Pfade im Skript anpassen)
