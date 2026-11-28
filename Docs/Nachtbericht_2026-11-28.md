# Nachtbericht 27./28.11.2026

Ziel der Nacht (Boris): Wasser im Eisenschlund, Höhlen weniger perfekt, Ruckeln und Hinausschleudern weg, Treffer-Feedback, Todesanimationen.

## Was fertig ist

**1. Ruckeln beim Kämpfen (das Wichtigste)**
- Kampftest mit 12 Gegnern, alle 0,5 s ein Zauber (Lager in L_Vael): **35 → 55 fps**, schlimmster Hänger **671 → 36 ms**, Hänger über 50 ms **24 → 0**.
- In den Höhlen: 58 fps, 99 %-Wert 30 ms.
- Ursachen: Effekte meldeten sich jedes Bild neu beim Renderer an; ~160 Funkel-Effekte rechneten auch unsichtbar; Effekte wurden mitten im Kampf von der Platte geladen; Niagara wurde beim ersten Auslösen neu vorbereitet. Alles behoben, Einzelheiten in `Docs/Kampfgefuehl_und_Leistung.md`.
- Dafür lädt der Level beim Start ein paar Sekunden länger (er bereitet die Effekte vor).

**2. Hinausgeschleudert / Teleport im Eisenschlund**
- Die Höhlen lagen als ~35 000 Einzelteile in der Welt; nach einem Höhleneingang kam der Boden erst nach über einer Minute, dann fiel die Gruppe ins Leere. Jetzt sind die Teile zu Instanzen zusammengefasst: **Boden nach ~2 s da**.
- Die Landung wartet unter einem Dach jetzt, bis der Boden da ist, statt ins Leere zu setzen.
- Neuer Positionswächter: Wer doch durch den Boden fällt, steht sofort wieder an der letzten sicheren Stelle in der Höhle. Ungeklärte Sprünge stehen im Log (`VaelGuard:`), falls es noch einmal passiert.

**3. Wasser in den Höhlen**
- Das Höhlenwasser ist jetzt das Wasser des Wasser-Kits wie im Teich der Aschenmark: Wellen, Lichtspiegelung, weiches Ufer.
- Dazu dunkle, spiegelnde Pfützen in allen Höhlen (in der Olmgrotte mehr, in den Funkenerz-Adern keine).

**4. Höhlen weniger perfekt**
- Bruchplatten im Boden (~9 000, etwas dunkler als der Boden, leicht schief), Kieselgruppen (~5 000, gern an den Wänden), Geröllhaufen am Wandfuß, Felsnasen und Tropfsteinsäulen in weiten Hallen, Felsbuckel, die die Wände unregelmäßig vorspringen lassen.
- Tropfen fallen von Stalaktiten und leuchtenden Schleimzapfen und zerspritzen am Boden (nur in der Nähe der Spieler, kostet sonst nichts).

**5. Treffer-Feedback und Zielhilfe**
- Getroffene Gegner blitzen kurz hell auf (Farbe nach Element), Funken fliegen auf der getroffenen Seite; Flieger werden in der Luft zurückgestoßen; der Todesschlag hält die Welt kurz an.
- Der Held blinkt rot, wenn er getroffen wird.
- Zielhilfe für Zauber und Ingenieur-Schüsse: hält auf bewegte Gegner vor, zielt auch in die Höhe auf Flieger, folgt sanft, lässt los, wenn der Gegner aus der Schusslinie geht. Gemessen auf freier Fläche: **fliegende Harpyien 30 % → 91 % Treffer**.

**6. Todesanimationen für alle Gegner**
- Umkippen, Wegschleudern bei Wucht (fliegt, prallt ab, rutscht aus), Absturz aus der Luft, Verkohlen bei Feuer (schwarz, glühende Risse, zerfällt zu Asche), Zerspringen wenn gefroren, Zucken bei Blitz, Zerquetschen bei wuchtiger Erde, violettes Auflösen bei Nid. Danach versinkt die Leiche.
- Gegner mit eigenem Sterbe-Clip (Nachtwesen, Kobolde, Pfuhlnatter …) behalten ihn und bekommen Element-Look und Wegfliegen dazu.

**7. Nachtrag am Morgen: weggeschleuderte Gegner**
- Test aller 27 Gegnerarten (`VaelTodAlle`): jede stirbt und spielt ihre Todesart. Dabei fiel auf, dass fast jeder Tod als Wegschleudern gewählt wurde und Blitz nie zuckte; die Regeln sind korrigiert.
- Echter alter Fehler gefunden: Die Wackel-Feder des Bein-IK schaukelte sich bei Gegnern in der Ferne (sie rechnen nur alle 0,25 s) auf. Die Kristalle einer Ameise flogen 20 000 km weit, der Renderer meldete tausende ungültige Matrizen (kostet Bildzeit). Behoben: Feder in kleinen Schritten, Gegner-Zeitschritt höchstens 0,1 s, unplausible Sprünge werden zurückgesetzt, Gegner fern aller Spieler werden angehalten (ihr Höhlenboden kann entladen sein). Danach 0 Fehler.
- Werkzeuge dafür: `VaelNaNSuche` (meldet kaputte Positionen), `vael.DeathMotion 0` (alte Todesart zum Vergleich).

## Getestet
- Kompiliert ohne Fehler, Smoke-Test ohne neue Fehler (ein alter Zähler bei Floromant-Paaren meldet wie vorher FAILED).
- Lauftests durch alle Höhlen in L_Vael: siehe Tabelle unten.
- Alle acht Todesarten per Bildserie angeschaut: Wegschleudern, Verkohlen, Zerspringen, Zerquetschen, Blitz (nach der Funken-Änderung), Nid, Umkippen (Prediger), Absturz (Harpyien aus der Luft).
- Testpunkte stehen auch im Aufgaben-Dokument unter „Testrunde am PC“.

| Lauftest in L_Vael | Ergebnis |
|---|---|
| Kobold-Hort (bis zur Truhe) | 297/297 |
| Funkenerz-Adern | 111/111 |
| Geflutete Stollen (Olmgrotte) | 116/116 |
| Rostschwamm-Höhle | 125/125 |
| Gebeinkammer | 53/53 |
| Ameisenbau | 135/135 |
| Tiefster Schacht | 224/224 |
| Zu Fuß durch den Welteingang in die Gefluteten Stollen und durchs Wasser bis zum See | 33/33, Boden nach 1,8 s, kein Hinausschleudern |

## Bitte im Editor testen
1. **Eisenschlund über einen Welteingang betreten**, ein Stück hinein und durchs Wasser laufen: Kommt das Hinausschleudern noch? Falls ja, bitte grob sagen wo; im Log steht dann `VaelGuard:` mit der Ursache.
2. **Kampf mit vielen Gegnern** (z. B. `VaelLastTest 30 12` in der Konsole): läuft es flüssig?
3. **Treffer auf Harpyien** mit Zaubern und dem Ingenieur: fühlt sich das Treffen gut an? Ist die Zielhilfe zu stark oder zu schwach?
4. **Todesarten ansehen:** `VaelTodTest wucht Prediger 2`, `VaelTodTest feuer Distelspinne 2`, `VaelTodTest eis Kriecher 3`, `VaelTodTest blitz Kriecher 3`, `VaelTodTest erde Kriecher 3`.
5. **Höhlenboden und Wasser** ansehen: Olmgrotte (Wasser), Rostschwamm-Höhle (Platten, Kiesel, Pfützen), Tropfen unter Stalaktiten.

## Offen / Hinweise
- **Grafikstufe zum Ausprobieren:** `VaelGrafik hoch|mittel|niedrig` in der Konsole (Standard bleibt hoch). Kampftest: hoch 54 fps, mittel 53 fps, niedrig 64 fps (Grafikkarte 17 → 11 ms pro Bild). Wenn dir eine Stufe gefällt, mache ich sie zum Standard.
- **Entscheidung für dich:** Der Rest der Bildzeit ist jetzt die Grafik selbst (Lumen-Licht, TSR-Kantenglättung, virtuelle Schatten, Hardware-Raytracing ~1,2 ms pro Bild). Wenn es auf deinem Bildschirm noch nicht flüssig genug ist, könnte ich Hardware-Raytracing abschalten oder eine Grafikstufe einbauen; das ändert die Optik leicht und erfordert einmal langes Shader-Kompilieren, deshalb nicht ohne dich.
- Der Levelstart dauert im Editor-Build ein paar Sekunden länger (Effekte werden vorbereitet); in einem fertig gepackten Spiel fällt das weg.
- Die Zielhilfe ist bewusst kräftig. Falls sie zu stark ist: Kegel und Drehgeschwindigkeit in `AVaelSpellProjectile::TickAimAssist`.
