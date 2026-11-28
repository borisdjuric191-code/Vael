# Kampfgefühl und Leistung (Nacht 27./28.11.2026)

Boris: „Treffer müssen treffen, auch schnelle Gegner in der Luft, mehr Satisfaction; alle Gegner brauchen eine Animation, in der sie sterben; es soll immer flüssig laufen."

## Todesarten (`UVaelDeathMotion`, `Source/Vael/Creatures/VaelDeathMotion.*`)

`AVaelCreature::Die` hängt dem Gegner die Komponente an. Sie bewegt den ganzen Akteur (Leiche ohne Kollision und Bewegung), legt ein Overlay-Material über die größten zwölf Meshes und wirft Trümmer. Gewählt wird nach dem letzten Treffer (`FVaelKillingBlow`: Element, Schaden, Leben davor, Herkunft, Rückstoß):

| Art | Wann | Wie |
|---|---|---|
| Umkippen | sonst | hohe Körper kippen vom Angreifer weg um den Fuß, flache rollen auf den Rücken; kleiner Nachfederer, Staub |
| Wegschleudern | Rückstoß ≥ 450 oder Schaden weit über dem Restleben (nicht bei großen Gegnern) | fliegt mit Drall, prallt bis zu viermal ab, rutscht aus, bleibt auf der Seite liegen |
| Absturz | stirbt in der Luft | fällt mit Drall, Aufprall mit Staub und kleinem Ruck |
| Verkohlen | Feuer oder brennend | wird schwarz mit glühenden Rissen, Funken und Rauch, zerfällt nach 3 s zu Asche |
| Zerspringen | gefroren | erstarrt kurz, zittert, zerspringt in Eissplitter |
| Blitz | Luft/Sturm | zuckt mit Funken, wird dunkel, kippt um |
| Zerquetschen | wuchtiger Erdtreffer ohne Rückstoß | wird plattgedrückt, Steine und Staub |
| Nid | Nid-Element | löst sich violett glühend auf |

Am Ende sinkt die Leiche in den Boden (Gesamtdauer 4,6 s statt 0,6 s). Gegner mit eigenem Todes-Clip (Nachtwesen, Kobolde, Pfuhlnatter, Kelchreißer, Rankenluchs) behalten ihn und bekommen nur Element-Look, Flug und Versinken. Der Schleimgänger (steht wieder auf) bekommt nichts. Materialien: `/Game/Vael/VFX/Tod` (`Scripts/CreateDeathMaterials.py`). Test: `VaelTodTest wucht|feuer|eis|blitz|erde|nid|normal <Art> <Anzahl>`.

## Treffer-Feedback (`UVaelHitFeedbackSubsystem`)

- Kurzer heller Blitz über dem getroffenen Körper (Farbe nach Element), Funken an der Seite, von der der Treffer kam. Das Blitz-Material wird pro Körper einmal angelegt und danach nur noch hochgedreht (keine Neuanmeldung beim Renderer je Treffer).
- Tötungsschlag: kurzer Halt der Welt und leichter Kamerastoß, auch bei leichten Treffern.
- Der Held blinkt rot, wenn er getroffen wird.
- Brennen und andere Dauerschäden blitzen nur schwach.

## Zielhilfe

- Zauber-Geschosse (`AVaelSpellProjectile::TickAimAssist`): wählt den Gegner in einem schmalen Kegel vor dem Schuss, hält auf die Stelle vor, an der er sein wird (auch in der Höhe bei Fliegern), und folgt ihm sanft, solange er in der Schusslinie bleibt. Weicht er seitlich aus oder ist der Schuss vorbei, fliegt er gerade weiter.
- Ingenieur-Schüsse (`AVaelEngineerShot::SteerToTarget`): der Zielpunkt wandert zum Gegner nahe der Schusslinie mit; Trefferprüfung nimmt jetzt die ganze Körperhöhe.
- Messung (`VaelZielTest 20 Harpyie 3`, freie Fläche): fliegende Harpyien 30 % → 91 % Treffer, Kriecher 88 % → 91 %. Abschalten zum Vergleich: `vael.AimAssist 0`.

## Leistung

Lasttest `VaelLastTest 30 12` (12 gemischte Gegner, alle 0,5 s ein Zauber), Lager in L_Vael, 1600 x 900:

| | vorher | nachher |
|---|---|---|
| Bilder pro Sekunde | 35 | 55 |
| 99 %-Wert | 90 ms | 28 ms |
| schlimmster Hänger | 671 ms | 36 ms |
| Hänger > 50 ms | 24 | 0 |

In den Höhlen (Kernerz-Stollen in L_Vael): 58 fps, 99 %-Wert 30 ms.

Ursachen und Fixes:
1. Instanz-Meshes (Funkeln, Regen, Zaubereffekte, Feenflug u. a.) wurden mit `bMarkRenderStateDirty = true` aktualisiert. Das meldet die Komponente jedes Bild komplett neu beim Renderer an (~330 Mal pro Bild). Jetzt inkrementell.
2. Rund 160 Funkel-Komponenten (Erze, Pflanzen) rechneten jedes Bild, auch ungesehen. Jetzt ruhen sie ungesehen und weiter als 50 m.
3. Effekte wurden mitten im ersten Kampf von der Platte geladen (bis 700 ms) und von der Speicherbereinigung wieder verworfen. `VaelAssets::LoadOptional` hält sie jetzt fest, merkt sich Fehlendes und lädt beim Levelstart alles vor, was frühere Spiele mitten im Spiel laden mussten (`Config/VaelPreload.txt`, ergänzt sich in `Saved/VaelPreload.txt`).
4. Im Editor-Build „verdaut" Niagara jedes System beim ersten Auslösen neu (80–160 ms). Das passiert jetzt beim Levelstart für alle geladenen Systeme (ein paar Sekunden mehr Ladezeit).
5. Höhlen in L_Vael: 34 646 Einzelteile → Instanzen (siehe `Eisenschlund_Hoehlen.md`).

Werkzeuge: `Saved/lasttest.sh` (LABEL, PRE, TRACE=1 für eine Insights-Aufnahme), Auswertung mit `UnrealInsights.exe -OpenTraceFile=… -NoUI -AutoQuit -ExecOnAnalysisCompleteCmd=@=<rsp>` und `TimingInsights.ExportTimingEvents`.

## Hinausgeschleudert / Teleport im Eisenschlund

- Nach einem Höhleneingang wartete die Landung bis zu 60 s auf den Boden und setzte die Gruppe danach ins Leere. Mit den Instanzen ist der Boden nach ~2 s da; unter einem Dach wartet die Landung jetzt bis zu 3 Minuten, statt ins Leere zu setzen.
- Neuer Positionswächter an jedem Spieler (`UVaelPositionGuard`): Wer durch den Boden fällt, wird an die letzte sichere Stelle zurückgesetzt (nicht aus der Höhle hinaus). Ungeklärte Sprünge stehen als `VaelGuard:` im Log, zu schnelles Wegschleudern (z. B. aus einem Mesh gedrückt) wird gebremst.
