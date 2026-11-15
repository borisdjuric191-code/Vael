# Die Höhlen des Eisenschlunds

Stand 15. November 2026. Boris hat die Liste mit „Passt perfekt, kannst starten“ angenommen. Gebaut ist die erste Höhle, die Gildenmine.

## Die acht Höhlenarten

| # | Höhle (Ort auf der Karte) | Form | Licht | Bewohner | Erze |
|---|---|---|---|---|---|
| 1 | **Gildenmine** (Kernerz-Stollen) | gerade, gebaute Stollen: Holzverbau, Grubengleis, Loren, Kisten | warme Grubenlaternen | befallene Bergleute (Wirte) | Kernerz, Rosteisen |
| 2 | **Funkenerz-Adern** | Spalten mit glühenden Adern, heiß, Funkenregen | orange von unten | Feuerquellen statt Laternen | Funkenerz, Schwefelkies |
| 3 | **Olmgrotte** (Geflutete Stollen) | knietiefes Wasser, Tropfen, stille Becken | kalt-blau vom Salzspat (echte Lampe) | Drachenbrut | Salzspat |
| 4 | **Ameisenbau** mit Erzmutterkammer | runde, gefressene Gänge, Kristallwände, Wabenkammern | Kristalle leuchten selbst | Schürfameisen, Ahnenpanzer, Erzmutter | viele, angenagt |
| 5 | **Kobold-Stollen** mit Hort | eng, verwinkelt, falsche Wegzeichen, Fallen, Sackgassen | trügerisches Licht | Kobolde | Kobaltblüte (Hort) |
| 6 | **Gebeinkammer** (Katakomben) | Wände aus geordneten Schädel- und Knochenreihen | farbige Schleimdecken | Grabschleim, Madenwirt, Knochenwesen | – |
| 7 | **Rostschwamm-Höhle** | Schwamm- und Pilzwucherung, giftige Luft | kränkliches Grün | – | Siechglas, Grünspan |
| 8 | **Tiefster Schacht** | senkrecht, eine Spirale hinab | – | Sternmaul (Magnetismus, dreht die Karte) | Rippenspat |

Später kommen die **Rippennager-Gänge** dazu: kurze Abkürzungen in der offenen Welt. Sie brauchen einen Kamera-Ausschnitt, damit man die Figur im Tunnel sieht.

## Wie eine Höhle entsteht

**1. Baukasten (Blender):** `Tools/Welt/hoehlen_baukasten.py` → `C:/Projekte/Vael_Quellen/Hoehlen/export`.
- Fels:
  - Felswand je Rasterkante: 7 m hoch, auf der Kameraseite 1,4 m.
  - Eckpfeiler.
  - Bodenplatten, 4 m.
- Gildenmine: Türstöcke in drei Breiten, Gleis, Lore, Kiste, Balkenstapel, Grubenlaterne.

**2. Grundriss:** `Scripts/Data/Hoehlen/<Name>.json`.
- Ein Raster aus Zeichen, 4 m je Zelle, oben ist Norden.
- Die Legende steht in der Datei. Die wichtigsten Zeichen:

| Zeichen | Bedeutung |
|---|---|
| `#` | Fels |
| `.` | Boden |
| `E` | Ausgang |
| `=` | Gleis |
| `L` | Laterne |
| `S` | Lichtschacht |
| `Z` | Erzstelle |
| `G` | Platz für Gegner |
| `T` | Geröll |

- Den Grundriss kann man von Hand ändern und neu bauen.

**3. Bauen (Unreal):** `Scripts/BuildHoehle.py`, mit geschlossenem Editor.
- Aufruf: `set VAEL_HOEHLE=Gildenmine`, dann `UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script=".../Scripts/BuildHoehle.py" -AllowCommandletRendering -unattended -nosplash`.
- Das Skript baut `/Game/Vael/Maps/Hoehlen/L_<Name>` neu als Innenlevel:
  - kein Himmel, kein Wetter
  - ein kühles Grundlicht von oben
  - Laternen mit Schatten
  - Grubenluft und feste Belichtung
- Es setzt außerdem in `L_Vael` den Eingang (Tag `VaelHoehle_<Name>`).
- Mit `VAEL_HOEHLE_OHNE_WELT=1` bleibt `L_Vael` unberührt.
- Jede Höhlenart bekommt in `ART` im Skript eigene Felsfarbe, Lichtfarbe und Nebel.

**4. Übergang:** `AVaelCaveEntrance` und `UVaelCaveTravel` (`Source/Vael/World/VaelCave.*`).
- Wer in den Eingang tritt, nimmt die ganze Gruppe mit.
- Der Spielstand wird vorher übernommen: Inventar, Material, Formeln, Ingenieur.
- In der Höhle landet man am Spielerstart.
- Beim Hinausgehen landet man 7 m vor dem Stolleneingang in der Welt.
- Testbefehl: `VaelHoehle L_Gildenmine` bzw. `VaelHoehle L_Vael`.

## Gildenmine (Kernerz-Stollen), erster Stand

- **Größe:** 144 × 160 m.
- **Räume:**
  - Eingangshalle mit dem Ausgang im Süden
  - Förderstollen mit Gleis nach Norden bis zur Förderkammer, mit Wetterschacht
  - Kernerz-Abbau im Westen
  - Rosteisen-Kammer im Osten, mit eingestürztem Teil
  - enger Rundgang
- **Eingang in der Welt:** am Kernerz-Stollen (1000 / 5368 m).
- **Getestet:**
  - Welt → Mine und Mine → Welt
  - Spieltest mit 0 Fehlern
  - Bilder angesehen

## Offen

- **Aussehen:** Die Wände wirken noch glatt und matt. Es fehlt eine Normal-Map für den Fels, mehr Relief und mehr Requisiten. Die Mine soll noch nicht abgenommen werden, Boris schaut zuerst.
- **Erzstellen:** Es sind Platzhalter (rostige Brocken mit Glanz). Die 11 Tripo-Erze sind noch nicht im Spiel.
- **Gegner:** Die Plätze sind markiert (`VaelHoehleGegner`). Die befallenen Bergleute gibt es noch nicht.
- **Anzeige in der Höhle:** Die Regionsanzeige zeigt in der Höhle „Aschenmark / Klarer Himmel“, und es gibt keine Minikarte. Sie soll stattdessen den Höhlennamen zeigen.
- **Weitere Höhlen:** Die nächsten sind Olmgrotte und Gebeinkammer.
