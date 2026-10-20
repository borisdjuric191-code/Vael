# Vael · Wurzelforst · Graubox-Blockout

Dieser Ordner baut die Region II (Wurzelforst mit Schwanzgrat und den Fängen) als Graubox in Unreal auf, genau nach dem 2D-Kartenentwurf.

| Datei | Inhalt |
|---|---|
| `WF_Hoehenkarte_2017.png` | 16-Bit-Höhenkarte, 2017 × 2017, 1 Pixel = 2 m, Gelände ca. 4 × 4 km |
| `T_WF_Zonenkarte.png` | Farbkarte der Zonen, wird automatisch als Boden-Material verwendet |
| `WF_Platzhalter.json` | Lage aller Platzhalter (1058 Formen, 25 Lichter, 10 Beschriftungen) |
| `wf_blockout.py` | Unreal-Python-Skript, das alles aufbaut |

## Schritte in Unreal

1. **Python aktivieren:** Bearbeiten › Plugins › „Python Editor Script Plugin“ einschalten und Unreal neu starten. Das ist meist schon aktiv.
2. **Neues Level anlegen:** Datei › Neues Level › „Leeres Level“ (Empty Level). Speichert es zum Beispiel als `/Game/Vael/Wurzelforst/Maps/L_Wurzelforst_Blockout`.
3. **Landschaft importieren:** Modus „Landschaft“ (Umschalt + 2) › Verwalten › Neu › **„Aus Datei importieren“** (Import from File).
   - Höhenkarte: `WF_Hoehenkarte_2017.png`
   - Position **0 / 0 / 0**, Rotation 0
   - Maßstab **X 200, Y 200, Z 100** (wichtig, sonst passen die Platzhalter nicht auf das Gelände)
   - Abschnittsgröße 63 × 63 Quads, 2 × 2 Abschnitte pro Komponente, Gesamtauflösung 2017 × 2017
   - Material leer lassen, das setzt das Skript
   - „Importieren“ klicken
4. **Optional, euer Wasser:** In `wf_blockout.py` oben bei `WASSER_MATERIAL` den Pfad eures Wassermaterials eintragen. Den Pfad bekommt ihr per Rechtsklick auf das Material › „Referenz kopieren“; nur den Teil in Anführungszeichen ohne `Material'…'` verwenden.
5. **Skript ausführen:** Werkzeuge › „Python-Skript ausführen …“ › `wf_blockout.py`. Das dauert etwa 1–3 Minuten, mit Fortschrittsbalken.
6. **Level speichern.**

Ihr könnt das Skript beliebig oft erneut ausführen. Es löscht nur seine eigenen Actors (Tag `WF_Blockout`) und baut sie neu.

## Was drin ist

- **Gelände:** Forstsaum flach mit Ascheübergang; Pfuhlmoor knapp über dem Wasser mit 6 Lagunen; Hain-Plateau mit Hügel unter dem Urstamm; Kammweg steigt nach Norden an; Schwanzgrat als Gratweg zwischen Gipfeln bis etwa 145 m, mit Steilklippen zum Ringsee; Flachwasser rund um alle Küsten (−0,6 m, begehbar); Tiefwasser bis −18 m; das Watt der Fänge auf −1,2 m.
- **Ewiger Hain:**
  - Urstamm (32 m Stamm, 90 m hoch, 10 Riesenwurzeln)
  - 24 Riesenbäume mit Brettwurzeln
  - 7 Hängestege
  - 5 Feenhöhlen mit Licht
  - Feenkreis
  - Eihöhle mit rosa Baum, Teich und Lichtschacht
  - 34 Wurzelbögen
  - 3 Lichtnymphen
- **Pfuhlmoor:**
  - 56 Mangroven mit Stelzwurzeln
  - 4 Moos-Hütten auf Stelzen
  - 4 Stammbrücken
  - Seerosen
  - Nid-Quelle mit violetten Adern und Licht
  - Quellwächter-Platzhalter (Schlange um einen toten Baum)
  - 3 Schlicknixen
- **Forstsaum:**
  - 30 Bäume, teils tot
  - Farnfelder
  - 2 Blattkobold-Späher
  - Spielerstart
  - eine 1,80-m-Figur als Größenvergleich
- **Kammweg:**
  - 28 Riesenpilze (türkis leuchtend, orange, rot)
  - 4 schwebende Pilzinseln
  - Pilzbrücke mit Laternen
  - Pilzhöhle
  - Felsbrocken
- **Schwanzgrat:**
  - Felstürme
  - Lianentreppe
  - 4 Wasserfälle
  - Schwebende Klippe mit Wasserfall
  - Drachensicht mit Steinbogen
  - Schwanzspitze mit Fähranleger
- **Die Fänge:**
  - 5 Zahnfelsen
  - Ebbe-Trittsteine knapp unter Wasser
  - Fährboot mit Fährnymphe
  - Anleger am Kiefer
- **Sonstiges:**
  - Kampagnenweg als Sandstreifen
  - Beschriftungen
  - Sonne, Himmel, Himmelslicht, grüner Bodennebel mit volumetrischem Nebel

## Tipps

- **Unter die Kronen sehen:** Im Outliner die Ordner `Kronen` ausblenden. Im fertigen Spiel brauchen die Kronen ein Ausblenden zur Kamera hin, sonst verdecken sie die Spieler.
- **Ebbe testen:** Den Actor `Wasserspiegel_Ringsee_Westmeer` auf Z = −150 setzen. Dann liegen die Trittsteine trocken.
- **Maßstab:** 1 Pixel der 2D-Karte = 2 m. Vom Aschenpass bis zur Schwanzspitze sind es etwa 3,5 km Weg.

## Platzhalter für Tripo (Tags)

| Tag | Was | Anzahl im Level |
|---|---|---|
| `TRIPO_1` | Urstamm: Stamm und Wurzelbasis | 1 + 10 Wurzeln |
| `TRIPO_2` | Riesenbaum (modular, auch im Forstsaum) | 54 Bäume |
| `TRIPO_3` | Riesenpilz | 32 |
| `TRIPO_4` | Mangrove mit Stelzwurzeln | 56 |
| `TRIPO_5` | Wurzelbogen / Moos-Wurzelstück | 34 |
| `KRONE` | Baumkronen, später besser als Laub in Unreal statt Tripo | — |
| `KREATUR`, `KREATUR_BOSS` | Kreaturen-Platzhalter | — |

Wenn ein Tripo-Modell fertig ist, den Pfad oben in `wf_blockout.py` bei `MODELLE` eintragen und das Skript erneut ausführen. Bis dahin bleiben es Graubox-Formen.

- Pro Baum, Pilz, Mangrove usw. steht dann **ein** Modell statt der Einzelteile (Stamm, Hut, Stelzen, Brettwurzeln).
- Das Modell steht mit dem Fuß auf dem Boden und ist so hoch wie der Platzhalter, der Wurzelbogen so lang wie der Platzhalter.
- Jedes Modell ist leicht zufällig gedreht. Nur der Wurzelbogen behält seine Richtung.
- Zwei Varianten gehen als Liste, zum Beispiel `"TRIPO_2": ["…/SM_Riesenbaum_A", "…/SM_Riesenbaum_B"]`.
- Die Riesenpilze tragen ihre Farbe als Tag (`FARBE_Pilz_Tuerkis` usw.), damit sie später das passende Material bekommen.

Schnell ausprobieren geht auch im Python-Ausgabelog: `platzhalter_ersetzen("TRIPO_3", "/Game/Vael/Wurzelforst/Modelle/SM_Riesenpilz_A")` baut einmal mit diesem Modell neu, ohne es dauerhaft einzutragen.

## Texturen (Scripts/DressWurzelforst.py)

Das Skript im Vael-Projekt legt Texturen über den Blockout. Es läuft ohne Oberfläche, der Editor muss dafür geschlossen sein.

- **Boden:** Waldboden, Schlamm am Wasser, Fels an steilen Hängen und Sand auf dem Kampagnenweg, eingefärbt nach Zone.
- **Platzhalter:** Rinde, Fels und Moos auf den Graubox-Formen. Pro `MI_WF_*` bestimmen die Parameter `Textur`, `Kachel` und `Muster`, wie stark die Textur zu sehen ist. Bei `Muster` 0 bleibt die reine Farbe.
- **Wasser:** `MI_WF_Wasser_Teich` (MI_Water_Pond, dunkler und mit Wellen für 4 km).
- **Licht:** Die zweite Sonne aus der Level-Vorlage (Ordner „Lighting“) wird ausgeschaltet, nicht gelöscht. Ein Post-Process-Volume „Stimmung_Wurzelforst“ senkt die Belichtung.

- **Pflanzen:** Farne, Sauerklee und Springkraut, Schilf am Ufer und trockenes Gras am Aschenrand wachsen von selbst um die Kamera (Landschaftsgras `GT_WF_*` im Ordner `Blockout/Gras`). Wo sie wachsen, bestimmt das Bodenmaterial nach Zone. Nicht auf dem Weg, an Steilhängen und im Wasser.
- **Boden:** Im Hain liegt Wurzelmoos, im Forstsaum Wiese mit Kleeflecken (Fab-Oberflächen).

Nach einem erneuten Lauf von `wf_blockout.py` das Texturen-Skript sicherheitshalber noch einmal ausführen.

## Kulisse verteilen (Scripts/ScatterWurzelforst.py)

Danach verteilt das zweite Skript Felsen, Moosfelsen, Steine, Wurzeln, Moospolster, umgestürzte Stämme, Äste, Stümpfe, tote Stämme, Moosböschungen und Totbüsche nach Zone, etwa 20 000 Stück.

- Pro Mesh steht ein Actor `Streu_…` mit vielen Instanzen im Outliner-Ordner `Wurzelforst/_Kulisse/<Art>`, Tag `WF_Streu`.
- Abstand halten sie zu den Graubox-Platzhaltern, zum Kampagnenweg und zum Spielerstart. Im Wasser stehen nur Stümpfe und Stämme in den Moorlagunen.
- Erneut ausführen ersetzt nur die eigenen `WF_Streu`-Actors (gleiches Ergebnis). Einzelne Instanzen kann man im Editor verschieben oder löschen; nach einem erneuten Lauf sind solche Handänderungen weg.
- Dichte je Zone steht oben im Skript unter `DENSITY` (Stück pro Hektar).

Beide Skripte laufen ohne Oberfläche, der Editor muss zu sein. Die Pakete (Fab, Megascans, Rock Collection) liegen nicht in Git, sie müssen auf dem Rechner vorhanden sein.
