# Auftrag für Claude Code: Spannhornkäfer in Vael importieren

Diesen Text in Claude Code (auf dem PC, im Vael-Projektordner gestartet) einfügen:

---

Im Ordner `C:\Users\boris\Downloads\Vael_Kreaturenfabrik\` liegt die Kreaturenfabrik für Vael (siehe `LIESMICH.md`).
Der Spannhornkäfer ist bereits fertig aufbereitet und geriggt:
`Vael_Quellen\Spannhornkaefer\04_export\SK_Spannhornkaefer.fbx` (14.740 Dreiecke, 30 Knochen, Vielbeiner-Skelett mit festen Namen: root, body, head, horn_L/R/M, leg_<L|R><1-3>_coxa/femur/tibia/foot).

Bitte:
1. Den Ordner `Vael_Kreaturenfabrik` nach `<Projektordner>\..\Vael_Tools\` bzw. `Vael_Quellen\` verschieben (außerhalb des Unreal-Projekts, damit es klein bleibt).
2. Prüfen, ob das Plugin „Python Editor Script Plugin“ im Projekt aktiv ist (.uproject → Plugins → `PythonScriptPlugin`). Falls nicht: aktivieren.
3. `unreal\vael_kreatur_import.py` ausführen – im offenen Editor über das Ausgabe-Log (Python):
   `py "<Pfad>/unreal/vael_kreatur_import.py" "<Pfad>/Vael_Quellen/Spannhornkaefer"`
   oder ohne offenen Editor:
   `UnrealEditor-Cmd.exe "<Projekt>.uproject" -run=pythonscript -script="<Pfad>/unreal/vael_kreatur_import.py <Pfad>/Vael_Quellen/Spannhornkaefer"`
   Das Skript ist neu und noch ungetestet – Fehler im Log lesen und das Skript reparieren.
4. Ergebnis prüfen: `/Game/Vael/Kreaturen/Wurzelforst/Spannhornkaefer/SK_Spannhornkaefer` existiert, mit Skelett und Physik-Asset; Größe etwa 120 cm; schaut entlang +X bzw. in Unreal-Vorwärtsrichtung (sonst `drehung_z_grad` in `kreatur.json` anpassen und Blender-Schritt neu laufen lassen).
5. Den Käfer testweise in eine Testmap stellen und einen Screenshot machen.

Danach (nächster Schritt): C++-Komponente `UVaelLegIKComponent` für prozedurale Beine (Vielbeiner) – Plan steht im Vael-Dokument, Tab „Kreaturenfabrik“.
