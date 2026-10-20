# Auftrag für Claude Code: Wurzelforst-Blockout einrichten

Kopiere diesen Text in Claude Code auf dem PC.

---

Du hilfst beim Unreal-Projekt **Vael**, einem isometrischen Action-RPG. Die Unreal-Oberfläche ist auf Deutsch. Im Ordner `Vael_Wurzelforst_Blockout` liegen eine Höhenkarte, eine Zonentextur, eine JSON-Datei mit Platzhaltern und das Editor-Skript `wf_blockout.py`. Lies zuerst `ANLEITUNG.md` in diesem Ordner.

Aufgaben:

1. Finde das Vael-Unreal-Projekt (`.uproject`) und notiere die Engine-Version.
2. Kopiere den Ordner `Vael_Wurzelforst_Blockout` nach `<Projektordner>/Blockout/Vael_Wurzelforst_Blockout/`, nicht nach `Content`.
3. Suche im Projekt unter `Content` das vorhandene **Wassermaterial für Teiche** (Stichworte: Wasser, Water, Teich, Pond; `.uasset`). Trag den Objektpfad oben in `wf_blockout.py` bei `WASSER_MATERIAL` ein, im Format `/Game/…/Name`. Bei mehreren Treffern frag nach, welches gemeint ist.
4. Prüfe `wf_blockout.py` gegen die Python-API dieser Engine-Version und passe nur an, was nicht kompatibel ist. Typische Kandidaten:
   - `EditorActorSubsystem.spawn_actor_from_object`
   - `MaterialEditingLibrary`
   - die Nebel-Property `fog_inscattering_luminance` gegenüber `fog_inscattering_color`
   - `HorizTextAligment`

   Ändere Koordinaten und Inhalte nicht.
5. Wenn in den Projekteinstellungen die Python-Remote-Ausführung aktiv ist (Plugins › Python › „Enable Remote Execution“), darfst du das Skript damit im offenen Editor starten. Sonst sag mir genau, welche Schritte ich in Unreal klicken muss (siehe `ANLEITUNG.md`).
6. Den Landschafts-Import macht normalerweise der Mensch in Unreal: Position 0/0/0, Maßstab 200/200/100. Automatisiere ihn nur, wenn das in dieser Version zuverlässig per Python geht, und frag vorher.

Regeln:

- Lösche oder verändere keine bestehenden Assets oder Levels außerhalb von `/Game/Vael/Wurzelforst/Blockout`.
- Keine Commits oder Pushes ohne Rückfrage.
- Melde am Ende kurz: Engine-Version, welches Wassermaterial eingetragen ist, was du am Skript geändert hast, und ob der Blockout gelaufen ist (Zahlen aus dem Ausgabelog, Präfix `[Wurzelforst]`).
