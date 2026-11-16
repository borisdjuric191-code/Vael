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

## Verschönern, erster Durchgang (15.11.)

- **Anzeige oben rechts:** zeigt in einer Höhle ihren Namen und „Unter Tage“ statt „Aschenmark / Wetter“. Der Name kommt aus `AVaelIndoorLevel::PlaceName`.
- **Licht:**
  - Keine Stelle liegt weiter als ~16 m vom nächsten Licht. Wo es dunkel war, setzt das Skript eine Lichtquelle der jeweiligen Höhle:
    - Laterne (Mine, grün bei den Kobolden)
    - Kerzen (Gebeinkammer)
    - Kristall (Olmgrotte, Ameisenbau, Schacht)
    - Glutader (Funkenerz)
    - Sporen am Rostschwamm
  - Jede Höhle hat ein Grundlicht in ihrer Farbe. Boden und Belichtung sind heller.
- **Bodenschutt:** Kiesel überall, dazu Gebeine in der Gebeinkammer, Kristallsplitter im Ameisenbau und Glutbrocken in den Funkenerz-Adern.
- **Gebeinkammer:**
  - Die Schädelwände stehen vor der Felsfront (vorher steckten sie im Fels). Sie kamen um 90° verdreht an; das korrigiert `VAEL_HOEHLE_SCHAEDEL_DREHUNG`.
  - Die Wände sind niedriger.
  - Die Gebeine haben ein dunkleres Material.
- **Olmgrotte:** Das Wasser ist matt und dunkel (glattes Wasser spiegelte den hellen Nebel).
- **Tiefster Schacht:** Die Rippenbögen stehen jetzt rundum am Rand des Abgrunds (19 statt 6).

## Verschönern, zweiter Durchgang (15.11.)

- **Minikarte und große Karte für jede Höhle:**
  - `py Tools/Welt/hoehlen_karte.py` zeichnet aus dem Grundriss ein Kartenbild: Fels, Boden, Wasser, Abgrund, Ausgang.
  - `BuildHoehle.py` legt daraus `DA_Karte_<Name>` an; die Minikarte findet es über den Levelnamen.
  - Mit Nebel des Krieges: Die Höhle zeigt sich erst beim Erkunden.
- **Schädelwände:** Schädelreihen auf beiden Seiten. So zeigen sie ihre Schädel, egal wie herum sie stehen; einige zeigten vorher die Rückseite.
- **Knochenhaufen:** Die Knochen liegen kreuz und quer statt sternförmig.
- **Olmgrotte:** Die blauen Platzhalter-Scheiben der Wasserquellen sind ausgeblendet; die Quellen wirken weiter.

**Reihenfolge nach einer Änderung am Grundriss:**
1. `py Tools/Welt/hoehle_grundriss.py <Name>` (nur natürliche Höhlen)
2. `py Tools/Welt/hoehlen_karte.py <Name>`
3. `BuildHoehle.py`

## Verschönern, dritter Durchgang (15.11.)

- **Felsrelief:** `M_HK_FelsRelief` legt die Normal-Map des Felsens auf dieselben drei Projektionen wie die Farbe. Es gibt zwei Größen, und das Relief wird in Weltrichtung zur Flächennormalen addiert. Einen Tangentenraum braucht es nicht, weil die Höhlenteile keine brauchbaren UVs haben.
- **Lachen, die leben:** `M_HK_Fluss_<Farbe>` lässt Flecken langsam über die Lache ziehen und aufglühen. Jede Schleimart bewegt sich anders: Rot schnell und zackig, Violett träge, Glut zackig (`FLUSS` in `BuildHoehle.py`).
  - Das Leuchten ist stark gedämpft, weil die Belichtung der Höhlen (+1,9) alles verstärkt.
  - Das Licht über der Lache hängt höher und ist schwächer. 70 cd auf 1,2 m hatten die Lache weiß angestrahlt.
- **Erze als Wandschmuck (Boris: zerschneiden, in Wänden und Decken verteilen, nicht erntebar):**
  - **Zerschneiden:** `Tools/Welt/hoehlen_erzwand.py` (Blender) schneidet jedes Erz in drei Stücke: A Spitzen, B Hälfte auf der Seite, C schräge Ecke. Die Stücke landen in `Vael_Quellen/Hoehlen/export/erze/SM_HK_ErzWand_<Erz>_<A|B|C>.fbx`.
  - **Verteilen:** `BuildHoehle.py` schießt von den Zellmitten Strahlen gegen den Fels und von oben auf die Felskronen. Jedes Stück wächst entlang der getroffenen Fläche heraus.
  - **Was zählt:** Nur Wände, Ecken und Brocken zählen als Treffer. Treffer von innen (der Strahl begann im Fels) werden verworfen.
  - **Welche Erze wo:** Welche Erze in welcher Höhle wachsen, steht in `HOEHLEN_ERZE`. Wie oft sie vorkommen, richtet sich nach `ERZ_SELTENHEIT`.
  - **Ohne Stücke:** Gibt es für eine Höhle noch keine Stücke, bleibt sie ohne Wanderze.
- **Erntebare Erze:** Die Erzstellen (Z) bekommen ein `VaelHarvestable` mit `DA_Harvest_<Erz>`, dazu Spitzen am Boden.
  - Je weiter vom Ausgang (Wegstrecke), desto seltener das Erz.
  - Sehr Seltenes liegt nur in der hinteren Hälfte, Legendäres nur an der tiefsten Stelle.
  - Fehlen die Erntedaten noch, bleibt der alte Platzhalter.
- **Ausprobieren ohne die echten Erze:** `VAEL_HOEHLE_ERZ_TEST=Niddruse` ersetzt jedes Erz durch ein vorhandenes Tripo-Modell.

## Felswände und Tropfsteine (15.11., Boris: Wände zu glatt)

- **Hauptursache:** Die Wände standen um 180° verdreht. Was Blender als -Y (raue Vorderseite) baut, kommt in Unreal als +Y an. Darum zeigte die glatte Rückseite in den Gang. `WALL_TURN` (180) dreht sie um.
- **Neue Wandform (`hoehlen_baukasten.py`):**
  - Der Fuß ist breit und läuft buckelig in den Boden aus. Die Wand beginnt 70 cm unter dem Boden.
  - Oben wölbt sie sich zu einem Überhang vor.
  - Vorsprünge, Nischen und schräge Felsbänke brechen die Fläche auf.
  - Wie weit die Wand vorragt, ist weich begrenzt (hohe Wand 1,9 m, niedrige 1,1 m).
  - Die Krone reicht 2,7 m nach hinten, damit zwei Wände in einer Felszelle überlappen.
- **Enge Gänge** (Fels auf beiden Seiten der Zelle): Die Wand wird 70 cm zurückgesetzt und auf 0,6 abgeflacht. Dort stehen keine Stalagmiten und keine Brocken. Der Lauftest im Kobold-Irrgarten geht wieder durch.
- **Tropfsteine (`tropfsteine()` in `BuildHoehle.py`):**
  - Stalagmiten stehen in Gruppen am Wandfuß.
  - Stalaktiten hängen unter den Überhängen. Ein Strahl vom Boden nach oben findet den Fels.
  - Natürliche Höhlen sind dicht besetzt (0,4), Stollen und Katakomben spärlich (0,12).

## Katakombe und Schleimzapfen (15.11., ohne Tripo)

- **Gebeinkammer als Katakombe (`katakombe()`):**
  - Breite Schädelbögen (`SM_HK_Schaedelbogen_Breit`, 7,4 m) stehen quer über den zweispurigen Gängen, jede dritte Reihe.
  - Schmale Bögen (`SM_HK_Schaedelbogen`) stehen über einspurigen Gängen.
  - Knochensäulen (`SM_HK_Knochensaeule`) stehen in den Hallen.
  - Die Bögen haben genaue Kollision, man geht darunter durch. Der Lauftest durch einen Gang mit Bögen geht durch.
  - `VaelTeleport` setzt die Figur unter einem Bogen nicht ab.
- **Schleimzapfen (`SM_HK_Schleimzapfen_A/B`):** Stalaktiten mit leuchtendem Schleimmantel, Tropfen an der Spitze und einem fallenden Tropfen darunter. Das sind die lockenden Säure-Bakterien aus der Lore.
  - In der Gebeinkammer werden 70 % der Stalaktiten zu Schleimzapfen, in allen Schleimfarben. Im Rostschwamm sind es 35 %, als Gift.
  - Jeder dritte Zapfen hat einen schwachen Schein in seiner Farbe.
- **Stream-Test:** Das Zeitlimit richtet sich jetzt nach der Länge der Strecke. Zuvor brach der Test 14 m vor dem Ende ab.

## Was beim Bauen schiefging (für die nächsten Höhlen)

- **Stalagmiten der Schluchten:** In einem Innenlevel ließen sie gar nichts mehr laden: kein Bild, keine Kollision, die Figur fiel endlos. Höhlen nehmen deshalb ihre eigenen Tropfsteine (`SM_HK_Tropfstein_*`).
- **Materialien:** Werden sie bei jedem Lauf neu angelegt, fehlen beim nächsten Spielstart die Teile, bis die Shader übersetzt sind. Darum bleiben sie jetzt stehen. Neu anlegen nur mit `VAEL_HOEHLE_MATERIAL_NEU=1`.
- **Fehler eingrenzen:** `VAEL_HOEHLE_TEST_OHNE=<Zeichen>` behandelt diese Zeichen als Boden, `VAEL_HOEHLE_TEST_GERADE=1` schaltet die unregelmäßigen Wände ab.
- **Drehung beim Import:** Teile aus Blender kommen in Unreal an Y gespiegelt an. Eine Vorderseite -Y zeigt dort nach +Y.
- **Prüfen:** Mit `-VaelShowcaseWalkTo` im Log nachsehen, ob die Figur steht (Z ≈ 100) oder fällt.

## Echte Knochen aus dem Fab-Skelett (16.11.)

- **Quelle:** „Human Skeleton“ von Salt Lake Manor (Fab, CC BY: Namensnennung, steht in `Docs/Credits.md`). Liegt als `Vael_Quellen/Skelett_Fab/skeleton.blend` mit Texturen. Modular, mit UE-Mannequin-Rig (67 Knochen).
- **Blender:** `Tools/Welt/vael_knochen_fab.py` baut aus den echten Knochen:
  - **Schädelwand A/B:** wie in den Pariser Katakomben. Reihen von Schädeln (ein Viertel mit Kiefer) zwischen dichten Bändern aus Gelenkenden von Oberschenkel, Oberarm und Schienbein, auf beiden Seiten, lose Knochen oben auf der Krone.
  - **Knochensäule und Schädelbögen:** Ringe aus Schädeln und Bändern aus Knochenenden um den dunklen Kern. Auf dem Bogen schauen Schädel in beide Gangrichtungen, oben sitzt ein großer Schlussstein-Schädel.
  - **Knochenhaufen:** Langknochen kreuz und quer, Becken, Brustkorb, Schulterblätter, Wirbel und Kiefer, Schädel obenauf.
  - Die Höhlenknochen sind 1,5-mal so groß wie echte, damit man sie von oben erkennt. Ein Teil hat bis zu 250 000 Dreiecke; `BuildHoehle.py` schaltet dafür Nanite ein.
  - Die groben Fassungen in `hoehlen_baukasten.py` bleiben als Rückfall, werden aber nicht mehr gebaut.
- **Unreal:** `Scripts/ImportKnochenFab.py`:
  - Texturen und `M_Vael_Gebein` (Farbe × Tönung, ORM, Normal, Leuchten).
  - `MI_Gebein_Hoehle` ist gealtert und dunkler, `MI_Gebein_Zauber` heller.
  - Einzelknochen `SM_Knochen_*` und der neue Narbenwall, alles unter `/Game/Vael/Requisiten/Knochen`.
  - In der Höhle bekommt der Slot `Gebein` das Höhlenmaterial.
- **Zauber des Magiers:**
  - **Knochensturm:** 60 gebrochene, spitze Knochensplitter wirbeln und taumeln um den Magier (Boris 16.11.). Es sind zwölf Bruchstücke von Oberschenkel, Schienbein, Oberarm, Elle, Speiche, Wadenbein und Schlüsselbein, dazu Wirbel. Sie sind 1,7-mal so groß wie echte, damit man sie von oben erkennt.
  - **Bruchstellen:** Jede Bruchstelle ist schräg und splittert in drei bis vier Zacken, eine davon lang und scharf (`broken()` und `jagged_cap()` in `vael_knochen_fab.py`). `VAEL_NUR_SPLITTER=1` baut nur die Splitter neu.
  - **Knochendornen:** Oberschenkel, Schienbein, Oberarm und Wadenbein brechen schräg aus dem Boden und glimmen violett.
  - **Markkäfig:** Je Stab wächst ein Oberschenkel oder Schienbein aus dem Boden, das Gelenk sitzt unten, oben ist er spitz abgebrochen. Darauf biegt sich ein abgebrochenes Unterarm- oder Wadenbein mit Splitterklaue über das Ziel.
  - **Narbenwall:** rohes Fleisch mit echten Rippenbögen, Langknochen und einem halb versunkenen Schädel.
  - Fehlen die Knochen, nehmen die Zauber wieder die alten Kegel.

## Offen

- **Aussehen:** Mehr Requisiten. Die Mine soll noch nicht abgenommen werden, Boris schaut zuerst.
- **Erze (15.11.):** Alle 11 sind im Spiel: Modelle in `Requisiten/Eisenschlund`, Erntedaten, Materialien, Wandstücke. Erneut einspielen mit `Scripts/ImportEisenschlundErze.py`, danach `hoehlen_erzwand.py` je Erz und die Höhlen neu bauen.
  - Tripo-Download: Die Browser-Leiste benennt den Download nicht um, die fertige ZIP bleibt als `.tmp` in Downloads liegen. Das FBX-Format nehmen, der GLB-Export brach ab.
  - **Kompendium:** elf Einträge im Buch der Gesteine (`CreateCompendiumAssets.py`). Das ist ein Entwurf, Boris hat ihn noch nicht abgenommen, und in der Lore-Bibel steht er noch nicht.
  - **Icons:** `Tools/Loot/vael_icons_erze.py` rendert das echte Tripo-Modell, danach kommt `ImportAutoIcons.py`.
  - **Leuchten und Aura:** `Scripts/BuildKristallAura.py` legt `M_Kristall_Master` an, eine Kopie des Kreatur-Masters mit Aura. Der Kreatur-Master selbst bleibt unberührt.
    - Was farbig oder hell ist, leuchtet an den Kanten.
    - Die Farbe wandert wie eine Lavalampe zwischen zwei Farben, jedes Stück im eigenen Takt.
    - Je seltener das Erz, desto stärker leuchtet es. Das gilt für Wandstücke und Erntestellen gleich.
  - **Schein:** Jede Erntestelle bekommt dazu einen weichen Schein in ihrer Farbe.
  - **Dichte:** Die Wanderze stehen dichter als zuvor (0,5 statt 0,32). Der Kobold-Stollen bleibt bei 0,16, weil er sehr viele Wände hat.
- **Gegner:** Die Plätze sind markiert (`VaelHoehleGegner`). Die befallenen Bergleute gibt es noch nicht.
- **Die fünf neuen Höhlen:**
  - Der Kobold-Stollen ist sehr dunkel.
  - Die Glut- und Giftlachen sind flache, leuchtende Flecken.
  - Der Schacht hat nur wenige Rippen.
  - Die Brut sind Eisäcke aus der Glutkammer.
  - Der Weg durch die fünf neuen Eingänge ist nicht einzeln getestet; die Technik ist dieselbe wie bei der Mine.
