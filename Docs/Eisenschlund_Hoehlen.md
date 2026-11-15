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

## Olmgrotte (Geflutete Stollen), erster Stand

- **Natürliche Höhle:** Die Wände stehen schiefer und versetzt, an ihrem Fuß liegen Brocken. Dazu kommen Tropfsteingruppen.
- **Räume:**
  - Eingangsgrotte im Süden
  - ein Bach aus knietiefem Wasser nach Norden
  - der große See mit tiefem Wasser, das man nicht betreten kann, und einer Insel
  - die Kristallgrotte im Nordwesten mit Salzspat-Lampen
  - das Gelege der Drachenbrut am tiefen Becken im Nordosten
  - ein Nebengang im Osten
- **Wasser:** Das flache Wasser ist an einzelnen Stellen eine Wasserquelle für die Magie.
- **Eingang in der Welt:** bei den Gefluteten Stollen (818 / 4959 m).
- **Grundriss:** erzeugt mit `py Tools/Welt/hoehle_grundriss.py Olmgrotte`. Danach kann man ihn in der JSON von Hand ändern.

## Gebeinkammer (Knochenhalle), erster Stand

- **Katakomben:**
  - gerade, 8 m breite Gänge im Raster
  - vor den hohen Wänden meist Schädelreihen mit Lagen von Langknochen
  - zwei Beinhauskammern mit Knochenhaufen
- **Knochenhöhle im Norden:** Grabschleim-Lachen in Grün, Violett, Rot, Bernstein und Türkis. Jede Lache leuchtet in ihrer Farbe.
- **Eingang in der Welt:** an der Knochenhalle (2046 / 4004 m).
- **Grundriss:** `py Tools/Welt/hoehle_grundriss.py Gebeinkammer`.

## Die übrigen fünf, erster Stand

Alle Grundrisse: `py Tools/Welt/hoehle_grundriss.py <Name>`, danach `set VAEL_HOEHLE=<Name>` und `Scripts/BuildHoehle.py`.

| Höhle (Level) | Eingang in der Welt | Was drin ist |
|---|---|---|
| Funkenerz-Adern (`L_FunkenerzAdern`) | Funkenerz-Adern 1136 / 5596 m, neues Portal | schmale, gewundene Spalten im schwarzen Fels; glühende Adern am Boden, Feuerquellen in den Weitungen (Feuer für die Magie); Funkenerz-Platzhalter |
| Ameisenbau (`L_Ameisenbau`) | Förderturm 955 / 4414 m, neues Portal | runde, gefressene Kammern an gewundenen Gängen; Kristallreste in fünf Farben, die leuchten; Brutkammern (Eisäcke aus der Glutkammer); ganz im Norden die große Kammer der Erzmutter |
| Kobold-Stollen (`L_KoboldStollen`) | Grubenlaternen 1648 / 3686 m, neues Portal | Irrgarten aus 4 m engen Gängen mit Sackgassen und ein paar Schleifen, niedrigere Wände; grünes, trügerisches Laternenlicht; Geröll; im Norden der Hort mit Kobaltblüte und Beutekisten |
| Rostschwamm-Höhle (`L_Rostschwamm`) | Rostschwamm-Höhle 1636 / 3300 m (Portal gab es schon) | weite Grotten voller Rostschwamm, Giftlachen, kränkliches Grün; Siechglas/Grünspan-Platzhalter |
| Tiefster Schacht (`L_TiefsterSchacht`) | Tiefster Schacht 1864 / 3209 m, neues Portal | Ringweg um einen bodenlosen Abgrund mit violettem Leuchten tief unten; Rippenbögen der Urdrachin spannen sich darüber; Rippenspat; hinten die Nische des Sternmauls |

**Getestet:** In allen acht Höhlen steht die Figur auf dem Boden (Lauftest). Von jeder Höhle habe ich Bilder angesehen.

## Was beim Bauen schiefging (für die nächsten Höhlen)

- **Stalagmiten der Schluchten:** In einem Innenlevel ließen sie gar nichts mehr laden: kein Bild, keine Kollision, die Figur fiel endlos. Höhlen nehmen deshalb ihre eigenen Tropfsteine (`SM_HK_Tropfstein_*`).
- **Materialien:** Werden sie bei jedem Lauf neu angelegt, fehlen beim nächsten Spielstart die Teile, bis die Shader übersetzt sind. Darum bleiben sie jetzt stehen. Neu anlegen nur mit `VAEL_HOEHLE_MATERIAL_NEU=1`.
- **Fehler eingrenzen:** `VAEL_HOEHLE_TEST_OHNE=<Zeichen>` behandelt diese Zeichen als Boden, `VAEL_HOEHLE_TEST_GERADE=1` schaltet die unregelmäßigen Wände ab.
- **Prüfen:** Mit `-VaelShowcaseWalkTo` im Log nachsehen, ob die Figur steht (Z ≈ 100) oder fällt.

## Offen

- **Aussehen:** Die Wände wirken noch glatt und matt. Es fehlt eine Normal-Map für den Fels, mehr Relief und mehr Requisiten. Die Mine soll noch nicht abgenommen werden, Boris schaut zuerst.
- **Erzstellen:** Es sind Platzhalter (rostige Brocken mit Glanz). Die 11 Tripo-Erze sind noch nicht im Spiel.
- **Gegner:** Die Plätze sind markiert (`VaelHoehleGegner`). Die befallenen Bergleute gibt es noch nicht.
- **Anzeige in der Höhle:** Die Regionsanzeige zeigt in der Höhle „Aschenmark / Klarer Himmel“, und es gibt keine Minikarte. Sie soll stattdessen den Höhlennamen zeigen.
- **Olmgrotte:** Das Wasser ist nur dunkel und spiegelt kaum. Die Wasserquellen zeigen sich als hellblaue Kreise.
- **Gebeinkammer:**
  - Sehr dunkel; die Schädelwände sind von oben kaum zu sehen, und in den Beinhauskammern fehlt Licht.
  - Die Knochenhaufen sehen etwas spinnenartig aus.
- **Die fünf neuen Höhlen:**
  - Der Kobold-Stollen ist sehr dunkel.
  - Die Glut- und Giftlachen sind flache, leuchtende Flecken.
  - Der Schacht hat nur wenige Rippen.
  - Die Brut sind Eisäcke aus der Glutkammer.
  - Der Weg durch die fünf neuen Eingänge ist nicht einzeln getestet; die Technik ist dieselbe wie bei der Mine.
