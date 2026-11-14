# Ingenieur · Materialien und Baupläne der Aschenmark (Entwurf 14.11.2026)

Alles hier ist ein **Vorschlag zum Absegnen**. Im Spiel ist bisher nur, was mit „im Spiel“ markiert ist.
Grundsatz (Boris): Der Ingenieur bekommt von allem die meisten Materialien (Erze, Kreaturen, Pflanzen), aber keine fertige Ausrüstung. Wie Monster Hunter, nur breiter: Ein Stück entsteht nie aus einem einzigen Material.

---

## 1. Rüstung aus dem Panzer des Glutkriechers: wie gut geht das optisch?

**Gut, und viel besser als die jetzigen Sätze.** Die vier Sätze im Spiel sind prozedurale Grundformen aus Blender (Ringe, Klingen, Federn) und sehen nur entfernt nach ihrer Kreatur aus. Der Glutkriecher liegt als fertiges Tripo-Modell mit 4K-Textur vor (`Vael_Quellen/Glutkriecher`). Daraus geht es so:

1. **Teile aus dem echten Modell schneiden (Blender):** Die Krustenplatten vom Rücken, die Beinsegmente und die Glutdrüse werden ausgeschnitten. Sie bekommen Dicke und saubere Kanten. Die Textur bleibt die echte Kriecher-Textur, deshalb sieht das Stück genau aus wie der Panzer des Tiers, mitsamt den glühenden Rissen.
2. **Zur Rüstung zusammensetzen:** Die Platten werden auf eine Grundform gesetzt, und zwar Leder darunter, Messingnieten und Riemen darüber. So sieht man, dass der Ingenieur es selbst gebaut hat. Brust: drei überlappende Krustenplatten. Schultern: Beinsegmente. Helm: Kopfplatte mit Rissglühen. Der Rücken des Brustteils trägt die Glutdrüse als Lunte.
3. **An den MetaHuman anpassen:** Das läuft über die Kleidungs-Pipeline, die beim Rucksack und bei den Sätzen schon funktioniert, mit Bindung an das MH-Skelett.
4. **Leuchten im Material:** Die Risse glühen über denselben Regler wie beim Tier (GlutStärke). Die Rüstung atmet also mit und glüht beim Ausweichen auf.

**Grenzen:** Tripo-Modelle sind eine einzige verschmolzene Hülle. Feine Teile wie Fühler oder dünne Beine taugen nicht als Rüstung, große Platten sehr gut. Bei Kreaturen mit wenig Fläche (Sichelpuppe, Schleieraffe) mische ich echte Teile (Sichel, Lumpen, Fell-Textur) mit gebauten Formen.
**Alternative, falls ein Satz edler aussehen soll:** Tripo-Bildbearbeitung mit der Kreaturzeichnung als Vorlage („Rüstungsteil aus den Platten dieses Tiers“). Das kostet Credits, deshalb nur für Boss-Sätze.

---

## 2. Das Bausystem (Vorschlag)

Damit viele Materialien sinnvoll zusammenspielen, bekommt jedes Material **eine bis zwei Rollen**. Ein Bauplan verlangt Rollen, nicht immer genau ein Material:

| Rolle | Wofür | Beispiele Aschenmark |
|---|---|---|
| **Kern** | trägt das Stück, bestimmt die Grundwerte | Schlackeneisen, Narbenholz, Rollpanzer, Krustenplatte |
| **Bindung** | hält zusammen, Bänder, Sehnen, Schnur | Silberhalm, Rankenfaser, Kriechersehne, Puppengarn |
| **Fassung** | Metall für Nieten, Gelenke, Ränder | Schlackeneisen, Harpyientand (Messing) |
| **Linse** | Sicht, Zielen, Licht | Rußglas, Glimmauge, Königinnenauge |
| **Füllung** | was in Ladung oder Mine wirkt | Glutstein, Faultau, Rauchstaub, Verdauungssäure |
| **Zünder** | löst aus | Knistersamen, Distelflaum, Glutdrüse |
| **Polster** | Rüstungsinneres, Leben und Widerstand | Schleierpelz, Keimpelzflocken, Ordenstuch |
| **Zusatz** | freier Platz, gibt eine Zusatzwirkung | jedes Element- oder Wegmaterial |

- **Feste Rezepte (Boris, 14.11.):** Jeder Bauplan hat genau eine feste Formel. Die Rollen oben helfen nur beim Ausdenken, damit die Formeln Sinn ergeben; im Spiel gibt es keinen freien Platz. Breiter als Monster Hunter wird es, weil jedes Stück in **Ausbaulinien** wächst (Stufe I → II → III, jede Stufe ein eigenes festes Rezept mit einem Material mehr) und weil ein Material in vielen Rezepten etwas anderes bewirkt (Abschnitt 6, Feuerformen).
- **Veredeln an der Esse:** Manche Rohstoffe werden in der Tüftler-Werkstatt erst zu Werkstoffen (Abschnitt 4). Im Feld (Feldkasten) geht das nicht. Das gibt der Werkstatt einen Grund.
- **Licht und Schatten:** Schattenmaterialien (Faultau, Gallmilch, Narbenholz, Nidkristall …) machen stärkere Stücke, ziehen aber wie die Nidladung an der Waage. Das ist die Versuchung des Tüftlers.

---

## 3. Alle Sammelmaterialien der Aschenmark

### Erze und Steine

| Material | Quelle | Seltenheit | Rollen | Stand |
|---|---|---|---|---|
| Schlacke | Schlacke-Erz (Dorf, Halden) | häufig | Kern, Füllung (Splitter) | im Spiel |
| Glutstein | Glutstein (Krater, Felder) | häufig | Füllung (Feuer), Wärme | im Spiel |
| Rußquarz | Rußquarz (Kraterrand) | gewöhnlich | Linse (nach dem Schleifen) | im Spiel |
| Nidkristall | Niddruse (nur an offener Nid-Quelle) | selten | Zusatz (Nid), Linse | im Spiel |
| **Glutschlacke** *(neu)* | Schlacke-Erz am Kraterrand, selten | ungewöhnlich | Kern, Fassung (hitzefest) | Vorschlag |
| **Klarquarz** *(neu)* | Rußquarz bei Verderbnis unter 20 % | selten | Linse (beste der Aschenmark) | Vorschlag |

*Klarquarz nutzt die vorhandene Lore „wird klarer, je weiter die Verderbnis sinkt“. Wer die Region heilt, bekommt bessere Linsen.*

### Pflanzen (wild)

| Material | Quelle | Rollen | Stand |
|---|---|---|---|
| Glutsamen | Glutdistel | Zünder, Füllung (Feuer) | im Spiel |
| Distelflaum | Glutdistel | Zünder (fängt sofort Feuer) | im Spiel |
| Kelchtau, Tränenglitzer | Tränenkelch | Füllung (Heilung) | im Spiel |
| Rauchstaub | Aschblase | Füllung (Rauch, Sicht) | im Spiel |
| Glockenglut | Laternenglocke | Leuchtmittel, Zünder (sanft) | im Spiel |
| Keimpelzflocken, Pelzwasser, Keimsaat | Keimpelz | Polster, Füllung (Wasser) | im Spiel |
| Kelchfleisch, Kelchschleim, Verdauungssäure | Fleischkelch (hohe Verderbnis) | Kleber, Füllung (Säure) | im Spiel |

### Saatpflanzen (Licht / Schatten)

| Pflanze | Licht | Schatten | Rollen für den Ingenieur |
|---|---|---|---|
| Glimmklee / Schwelklee | Glimmblatt, Glimmstaub | Schwelblatt, Schwelstaub | Zusatz Feuerschutz / Schwelbrand |
| Taublatt / Faulblatt | Blatttau | Faultau | Füllung Wasser / Gift |
| Windhalm / Flüsterhalm | Silberhalm, Halmsamen | Flüsterfaser, Raunsamen | Bindung, Befiederung / Verwirrung |
| Schlotwinde / Würgwinde | Trichterpollen, Rankenfaser | Würgranke, Dornpollen | Bindung, Netz / Fesseln, Bluten |
| Rußrose / Blutrose | Rußblütenblatt, Rosenherz-Nektar | Blutblatt, Blutnektar | Heilfüllung / Lebensraub |
| Milchstern / Gallstern | Sternmilch, Sternpollen | Gallmilch, Gallpollen | **Latex** (Dichtung, Federn) / schwarzer Latex |
| Funkenmohn / Schwarzmohn | Knistersamen, Mohnblütenblatt | Schwarzfunken, Schwarzmohnblatt | Zünder / unlöschbares Feuer |
| Hainschössling / Narbentrieb | Hainblatt, Lichttropfen | Narbenholz, Narbensaft | Licht gegen Markwesen / **Holzkern** |

### Pilze

| Material | Quelle | Rollen | Stand |
|---|---|---|---|
| Pfeifenmorchel-Sporen | Pfeifenmorchel | Pfeife, Lockmittel (pfeift vor Sturm) | im Spiel |
| Aschenstäubling | Aschenstäubling | **Schleifpulver**, Löschmittel | im Spiel |
| Adernflechte | Adernflechte | Nid-Spürer (färbt sich violett) | im Spiel |

### Kreaturen: was fällt und was ich dazu vorschlage

| Kreatur | Im Spiel | Neu vorgeschlagen | Begründung |
|---|---|---|---|
| **Glutkriecher** | Glutdrüse | **Krustenplatte** (häufig), **Kriechersehne** (gewöhnlich), Bruch: **Glutkiefer** | Er besteht aus Krustenplatten, sie sind der natürliche Rüstungsstoff. Sehnen halten die Platten beim Graben. |
| **Glutkönigin** (Boss) | Herz der Glut | **Brutkrone** (Kamm aus Krustenplatten), **Königinnenauge** (aus dem Augenkranz), **Legestachel** | Der Augenkranz ist eine perfekte Boss-Linse; Krone und Stachel für den Boss-Satz und die Meisterarmbrust. |
| **Aschharpyie** | Rußfeder | **Greifklaue** (selten, stand schon im Bestiarium) | Haken, Abzug, Kletterhaken |
| **Harpyien-Älteste** | Ältestenschwinge | **Hohlkiel**, **Kreischbalg** (beide standen schon im Bestiarium), **Harpyientand** (neu) | Ihre Krone ist aus gestohlenem Tand, also Messing, Knöpfe und Ringe. Das ist die einzige Messingquelle der Aschenmark und passt zum Messing-Stil der Werkstatt. |
| **Aschenroller** | Rollpanzer | **Rollsehne** | Die Sehne, die ihn einrollt, wird zur Feder für Fangeisen und Armbrust. |
| **Sichelpuppe** | Glimmstroh, Sichelklinge | **Puppengarn** (Erntegarn, das die Garbe bindet) | Bindung; glimmt wie das Stroh, also feuerfeste Schnur |
| **Schleieraffe** | Schleierpelz, Glimmauge | **Klammerfinger** (selten) | Lange Finger als Greifer, zum Beispiel für den Wurfhaken |
| **Prediger der Narbe** | Ordenssiegel | **Ordenstuch**, **Siegelwachs** | Polster und Dichtung; Wachs versiegelt Kapseln (Säure, Heilung) |
| **Kettenkrabbe** (Küste, Idee) | – | **Dornenglied** | Boris' Baukasten-Idee: Kette als Dornenpeitsche |

**Bruchmaterialien:** Heute fällt beim Teilebrechen nur ein allgemeines „Bruchstück“. Nach Monster-Hunter-Art schlage ich ein **eigenes Bruchstück je Kreatur** vor: Glutkiefer, Rollpanzer-Kern, Harpyienschnabel, Sichelherz (der Nid-Knoten in der Puppe), Glutkrone (Königin). Sie ersetzen das Bruchstück in den Meisterstufen.

---

## 4. Veredeln in der Werkstatt (Esse, Schleifstein, Werkbank)

| Werkstoff | Aus | Wo | Wofür |
|---|---|---|---|
| **Schlackeneisen** | 3 Schlacke + 1 Glutstein | Esse | Fassung, Nieten, Bolzenspitzen; das Grundmetall |
| **Rußglas** | 2 Rußquarz + 1 Aschenstäubling (Schleifpulver) | Schleifstein | Linse der Brille und des Fernglases |
| **Klarglas** | 1 Klarquarz + 1 Aschenstäubling | Schleifstein | beste Linse der Aschenmark |
| **Latex** | 2 Sternmilch | Werkbank | Dichtungen, Federn, Schleuderband der 2. Stufe |
| **Schwarzer Latex** | 2 Gallmilch | Werkbank | wie Latex, stärker, zieht zum Schatten |
| **Gehärtete Platte** | 2 Krustenplatte + 1 Glutstein | Esse | Rüstungskern des Krusten-Satzes |
| **Messingblech** | 2 Harpyientand + 1 Schlacke | Esse | edle Fassungen, Linsenringe, Messingkappen der Zwille |

---

## 5. Was der Ingenieur daraus baut

Alle Rezepte sind fest. Ausbauten sind eigene Rezepte auf dem vorigen Stück.

### Werkzeuge

| Stück | Bauplan | Wirkung |
|---|---|---|
| **Zielhilfebrille** | 2 Rußglas + 1 Schlackeneisen (Fassung) + 2 Silberhalm (Band) + 1 Latex (Dichtung) | Schwachstellen ×1,5, Krit 15 % (wie heute) |
| ↳ **Wärmeglas-Brille** | Brille + 2 Glimmblatt + 1 Mohnblütenblatt | zusätzlich +20 % Feuerwiderstand, sieht Hitze (Glutkriecher im Boden) |
| ↳ **Rauchglas-Brille** | Brille + 2 Rauchstaub + 1 Siegelwachs | zusätzlich Sicht durch Rauch, auch die eigene Rauchladung |
| ↳ **Aderglas-Brille** | Brille + 2 Adernflechte + 1 Nidkristall | zusätzlich leuchten Nid-Adern und Niddrusen in der Nähe auf |
| **Linsen** (wie heute) | Glimmauge / Nidkristall / Sternmilch, jeweils + 1 Rußglas + 1 Messingblech | wie heute |
| **Königinnenlinse** *(neu)* | 1 Königinnenauge + 1 Klarglas + 1 Messingblech | sieht Feinde hinter Fels als Wärmeschatten |
| **Fernglas** *(neu)* | 2 Rußglas + 1 Narbenholz + 1 Messingblech | weit spähen, Kreaturen aus der Ferne ins Kompendium eintragen |
| **Wurfhaken** *(neu)* | 1 Greifklaue + 1 Klammerfinger + 3 Rankenfaser (Seil) | auf Felsvorsprünge ziehen, kleine Kreaturen heranreißen; Vorstufe zum Enterhaken der Peitschenunke |
| **Lockpfeife** *(neu)* | 1 Kreischbalg + 1 Hohlkiel + 1 Pfeifenmorchel-Sporen | lockt eine Gruppe zu einer gelegten Falle |
| **Blendlaterne** *(neu)* | 1 Glockenglut + 1 Rußglas + 1 Schlackeneisen + [Glimmauge] | Lichtkegel; blendet Schleieraffen, sie lassen Beute fallen |
| **Tarnplane** | heute Rankenkammblatt (Wurzelforst); Aschenmark-Version: 2 Ordenstuch + 2 Rauchstaub | Fallen unsichtbar, in der Aschenmark aschgrau |

### Waffen und Ausbauten

| Stück | Bauplan | Hinweis |
|---|---|---|
| **Zwille** (Start) | Narbenholz, Silberhalm-Band, Lederschale | heute schon Narbenholz + Spannfaden; mit Silberhalm geht sie **ohne den Wurzelforst** |
| **Glutschleuder** | 3 Glutstein + 1 Glutdrüse + 1 Puppengarn (feuerfest) | Puppengarn neu dazu, sonst brennt das Band durch |
| **Spannschleuder** | 2 Latex + 1 Rollsehne + 1 Schlackeneisen | Latex statt Spannfaden, damit sie in der Aschenmark baubar ist |
| **Krustenschleuder** *(neu, Zweig 3)* | 2 Krustenplatte + 1 Kriechersehne + 1 Glutkiefer | schwere Steine, jeder vierte Schuss betäubt |
| **Bogen aus lebendem Holz** | 4 Narbenholz + 3 Spannfaden + 2 Rankenfaser | bleibt das Meisterstück und braucht den Wurzelforst; so lockt er weiter |
| **Armbrust** | 3 Rollpanzer + 2 Sichelklinge + 2 Rollsehne + 3 Schlackeneisen | Rollsehne als Spannfeder |
| **Meisterarmbrust** | 1 Legestachel + 1 Glutkrone + 2 Messingblech | Boss-Material statt allgemeinem Bruchstück |

### Munition (heute 14 Ladungen; neue Vorschläge aus der Aschenmark)

*Ersetzt durch Abschnitt 6 „Munition als Formel“. Die Liste hier bleibt nur als Ideensammlung stehen (Heulpfeil, Tandladung, Löschladung kommen in den Formeln noch nicht vor).*

| Ladung | Bauplan | Wirkung |
|---|---|---|
| **Krustenbolzen** | 1 Krustenplatte + 1 Schlackeneisen | durchschlägt Panzer, ×1,2 gegen gepanzerte Flanken |
| **Glutnadel** | 1 Hohlkiel + 1 Glutdrüse | sitzt im Ziel, explodiert nach 2 s (kleine Kriecher-Explosion) |
| **Heulpfeil** | 1 Hohlkiel + 1 Rußfeder + 1 Kreischbalg-Splitter | heult im Flug, Gegner im Umkreis drehen sich zum Einschlag |
| **Löschladung** | 1 Aschenstäubling + 1 Siegelwachs | löscht Brände, kühlt Glutkriecher (verzögert ihre Explosion) |
| **Schwelladung** (Schatten) | 1 Schwelstaub + 1 Distelflaum | Schwelbrand 10 s, schwach, aber kaum zu löschen; zieht 0,5 zum Schatten |
| **Würgladung** (Schatten) | 1 Würgranke + 1 Kelchschleim | hält 2 s fest, das Ziel blutet beim Losreißen |
| **Gesangsladung** (Licht) | 1 Hainblatt + 1 Silberhalm | trifft Markwesen (Sichelpuppe, Prediger) doppelt |
| **Tandladung** | 1 Harpyientand + 1 Glockenglut | glitzert im Flug; Harpyien und Schleieraffen jagen ihr nach, ein Köder |

### Vorrichtungen

| Vorrichtung | Bauplan | Wirkung |
|---|---|---|
| **Fangeisen** | 1 Sichelklinge + 1 Rollsehne (Feder) + 1 Schlackeneisen | wie heute; die Rollsehne ersetzt den Spannfaden |
| **Grabmine** *(neu, Glutkriecher-Idee)* | 1 Glutdrüse + 1 Krustenplatte + 1 Distelflaum | gräbt sich ein und ist unsichtbar, explodiert bei Berührung |
| **Sichelkreisel** (stand im Bestiarium) | 2 Sichelklinge + 1 Puppengarn + 1 Glimmstroh | dreht sich 4 s auf der Stelle, Wirbel wie die Sichelpuppe |
| **Rauchtopf** | 2 Rauchstaub + 1 Siegelwachs | Rauchwand für 8 s, Feinde verlieren das Ziel |
| **Rammschild** (stand im Bestiarium) | 3 Rollpanzer + 1 Rollsehne + 2 Schlackeneisen | wird hingestellt, rollende Gegner (Aschenroller) prallen ab und liegen betäubt |
| **Sturmwarner** | 1 Pfeifenmorchel-Sporen + 1 Messingblech | pfeift, bevor eine Aschenböe kommt (die Pfeifenmorchel-Lore) |

### Rüstungssätze der Aschenmark

Vier Plätze (Kopf, Brust, Arme, Beine); 2 und 4 Teile geben den Satzbonus. Jedes Teil = **Kern der Kreatur + Bindung + Polster + Fassung**.

| Satz | Kern | 2 Teile | 4 Teile | Aussehen |
|---|---|---|---|---|
| **Krusten-Satz** (Glutkriecher, neu) | Gehärtete Platte, Kriechersehne | +15 % Feuerwiderstand | Nach einem Treffer glühen die Risse auf; der nächste eigene Schuss setzt in Brand | echte Krustenplatten mit Glutrissen, Messingnieten |
| **Panzer-Satz** (Aschenroller, im Spiel) | Rollpanzer, Rollsehne | wie heute | wie heute | Ringsegmente aus dem echten Roller-Modell |
| **Sichel-Satz** (Sichelpuppe, im Spiel) | Sichelklinge, Puppengarn, Glimmstroh | wie heute | wie heute | Strohpolster, Sichelklingen als Schulterkanten |
| **Ruß-Satz** (Aschharpyie, im Spiel) | Rußfeder, Greifklaue | wie heute | wie heute | Federkragen, Klauen an den Armschienen |
| **Schleier-Satz** (Schleieraffe, neu) | Schleierpelz, Klammerfinger | Gegner bemerken ihn 30 % später | Aus dem Hinterhalt: der erste Schuss ist immer kritisch | Lumpenkapuze, rußschwarzes Fell |
| **Glutkronen-Satz** (Glutkönigin, Boss) | Brutkrone, Herz der Glut, Königinnenauge | Minen +20 % Schaden | Gelegte Minen glimmen und zünden sich nach 6 s selbst; jede tote Mine gebiert einen Funkenkriecher, der zum nächsten Feind läuft | Kamm aus Krustenplatten, Augenkranz am Helm, violette Nid-Adern |

*Polster: jeder Satz hat sein festes Polster (Krusten-Satz Ordenstuch, Schleier-Satz Schleierpelz, Glutkronen-Satz Keimpelzflocken; die drei Sätze im Spiel behalten ihre Rezepte).*

---

## 6. Munition als Formel (Boris, 14.11.)

Munition entsteht nicht aus fertigen Arten, sondern aus Bauteilen, die sich verstärken. Jede Formel ist ein festes Rezept; jede Stufe hat genau ein Teil mehr.

**Formel = Hülle + Kern + bis zu zwei Verstärker** (höchstens vier Teile).

- **Hülle:** wie das Geschoss wirkt (hart einschlagen, platzen, stecken bleiben).
- **Kern:** das Element.
- **Verstärker:** verändert die Form (Stichflamme, Explosion, Wolke …) oder macht sie stärker (länger, größer, heißer).
- **Regel:** Die Hülle muss zum Inhalt passen. Feuer verbrennt Latex, Säure frisst Latex, Gas braucht eine weiche Hülle.

Die Formen aus der früheren Fassung (Stichflamme, Schwelbrand, Dampf, Blendglut …) sind jetzt **Ergebnisse** von Formeln, keine eigenen Arten. Die 14 Ladungen im Spiel gehen alle in diesem System auf (in den Tabellen mit „im Spiel“ markiert).

### Hüllen der Aschenmark

| Hülle | Woraus | Wirkt so | Passt für | Nicht für |
|---|---|---|---|---|
| **Schlackenhülle** | 1 Schlacke | hart, schlägt ein, ein Ziel | Feuer, Splitter, Wucht | Flüssiges, Gas |
| **Blasenhaut** | leere Aschblase | platzt weich zu einer Wolke (3 m) | Rauch, Licht, Gas | Feuer |
| **Latexkapsel** | Latex (Sternmilch) | platzt mit Spritzer (2 m), wie ein Wasserballon | Wasser, Gift, Heilung, Kleber | Feuer, Säure |
| **Schlackenglas** | Schlacke + Glutstein (Esse) | zerspringt: Splitter plus Inhalt als Fläche | Feuer, Säure | – |
| **Hohlkiel** | Harpyien-Älteste | Nadel, bleibt stecken, wirkt nach 2 s oder über Zeit | Feuer, Gift, Säure in kleiner Menge | Wolken, Flächen |

Später: Bernstein (Saftperle, Wurzelforst) und Kelchkapsel.

### Kerne der Aschenmark (8)

Feuer (Glutstein) · Wasser (Blatttau) · Rauch (Rauchstaub) · Gift (Faultau, Schatten) · Säure (Verdauungssäure, Schatten) · Heilung (Kelchtau) · Licht (Glockenglut) · Wucht und Splitter (Sichelklinge, Rollpanzer).
Nid ist kein eigener Kern: Ein Nidkristall als Verstärker verdoppelt jede Formel und zieht 1,5 zum Schatten (Bauplan nötig).

### Verstärker der Aschenmark

| Verstärker | Aus | Macht |
|---|---|---|
| Knistersamen | Funkenmohn | **Form:** Stichflamme, ein Stoß im Kegel |
| Glutdrüse | Glutkriecher | **Form:** Explosion mit Rückstoß |
| Glutstein (als Zweitteil) | Erz | **Form:** Wasser wird zu Dampf |
| Schwarzmohnblatt | Schwarzmohn | **Form:** Rauch wird zu Schlafwolke |
| Raunsamen | Flüsterhalm | **Form:** Verwirrung |
| Kelchschleim | Fleischkelch | **Form:** klebt, verlangsamt |
| Dornpollen | Würgwinde | **Form:** Bluten |
| Glimmstroh | Sichelpuppe | **Form:** Lichtblitz, blendet |
| Schwarzfunken | Schwarzmohn | **Form:** Schwarzfeuer, übergeht Feuerwiderstand (Schatten) |
| Distelflaum | Glutdistel | **Stärke:** brennbar, heißer und weiter |
| Glutsamen | Glutdistel | **Stärke:** brennt länger |
| Pelzwasser | Keimpelz | **Stärke:** größere Fläche |
| Gallpollen / Sternpollen | Gallstern / Milchstern | **Stärke:** Schatten- / Lichtwirkung länger |
| Lichttropfen | Hainschössling | **Stärke:** Licht und Heilung stärker, trifft Markwesen |
| Rollpanzer / Krustenplatte | Aschenroller / Glutkriecher | **Stärke:** Wucht, durchschlägt Panzer |

### Formelbäume

**Feuer**

| Formel | Ergebnis |
|---|---|
| Schlackenhülle + Glutstein | **Glutladung**: brennt 6 s (im Spiel als Brandladung) |
| … + Knistersamen | **Stichflamme**: Feuerstoß im Kegel, 4 m |
| … + Knistersamen + Distelflaum | **Große Stichflamme**: 6 m, heißer |
| … + Knistersamen + Glutdrüse | **Feuerstoß**: Stichflamme, die explodiert und zurückwirft |
| Schlackenglas + Glutstein + Glutsamen | **Brandsatz**: Brandfläche 3 m, 5 s |
| Schlackenglas + Glutstein + Schwarzfunken | **Schwarzfeuer**: übergeht Feuerwiderstand (Schatten) |
| Hohlkiel + Glutdrüse | **Glutnadel**: sitzt im Ziel, explodiert nach 2 s |

**Wasser**

| Formel | Ergebnis |
|---|---|
| Latexkapsel + Blatttau | **Wasserkapsel**: nass 8 s, Feuer geht aus, Blitz trifft doppelt (im Spiel als Tauladung) |
| … + Pelzwasser | **Große Wasserkapsel**: 4 m |
| Schlackenglas + Blatttau + Glutstein | **Dampfkapsel**: verbrüht und blendet 3 s; wirkt auch gegen Feuerfeste |
| … + Pelzwasser | **Dampfwolke**: 4 m, 5 s |

**Rauch**

| Formel | Ergebnis |
|---|---|
| Blasenhaut + Rauchstaub | **Rauchladung**: Gegner verlieren das Ziel (im Spiel) |
| … + Schwarzmohnblatt | **Schlafwolke**: Schwache schlafen, Starke werden träge (im Spiel als Schlafladung) |
| … + Raunsamen | **Wirrwolke**: Gegner darin greifen 3 s einander an |
| … + Glutsamen + Distelflaum | **Glutrauch**: die Wolke glüht innen und brennt |

**Gift** (Schatten)

| Formel | Ergebnis |
|---|---|
| Latexkapsel + Faultau | **Giftladung**: 6 s Gift, das Ziel taumelt (im Spiel) |
| … + Gallpollen | **Zehrgift**: 10 s |
| … + Dornpollen | **Dorngift**: Gift und Bluten (im Spiel als Dornladung) |
| Hohlkiel + Faultau + Gallpollen | **Giftnadel**: Gift, das langsam stärker wird |

**Säure** (Schatten)

| Formel | Ergebnis |
|---|---|
| Schlackenglas + Verdauungssäure | **Säureladung**: frisst Panzer, mehr Schaden von allem (im Spiel) |
| … + Kelchschleim | **Klebsäure**: haftet und frisst doppelt so lange |
| Hohlkiel + Verdauungssäure | **Säurenadel**: frisst genau eine Stelle; das Teil bricht schneller |

**Heilung und Licht**

| Formel | Ergebnis |
|---|---|
| Latexkapsel + Kelchtau | **Heilkapsel**: heilt die Gefährten im Spritzer |
| … + Lichttropfen | **Starke Heilkapsel** (im Spiel als Heilladung) |
| … + Rosenherz-Nektar | **Heiltau**: heilt 8 s lang |
| Blasenhaut + Glockenglut | **Leuchtladung**: markiert, mehr Schaden von allen (im Spiel) |
| … + Glimmstroh | **Blendladung**: Lichtblitz, blendet 2 s, Schleieraffen lassen Beute fallen |
| … + Lichttropfen | **Sonnenladung**: blendet und trifft Markwesen doppelt |

**Wucht und Splitter**

| Formel | Ergebnis |
|---|---|
| Schlackenhülle + Sichelklinge | **Splitterladung**: zerspringt, trifft alles rundherum (im Spiel) |
| Schlackenhülle + Rollpanzer | **Schwerer Stein**: wirft zurück, betäubt (im Spiel) |
| Schlackenhülle + Krustenplatte + Schlackeneisen | **Krustenbolzen**: durchschlägt gepanzerte Flanken |
| Latexkapsel + Kelchschleim | **Klebekapsel**: halb so schnell, 6 s (im Spiel als Klebeladung) |

### Wie viele Munitionsarten?

- **Aschenmark:** 32 Formeln aus 5 Hüllen, 8 Kernen und rund 15 Verstärkern, dazu die Nid-Fassung (doppelte Kraft mit Nidkristall).
- **Jede weitere Region** bringt 1 bis 2 neue Hüllen (Bernstein, Kelchkapsel …), 2 bis 3 neue Kerne (Blitz, Eis, Erze) und Verstärker, also rund 15 bis 20 neue Formeln. Am Ende sind es etwa **90 bis 100**.
- Das überfordert nicht, weil man keine 100 Arten lernt, sondern 5 Hüllen, 8 Kerne und die Verstärker. Wer weiß, dass Knistersamen eine Stichflamme macht, versteht jede Formel damit sofort.

### Der Rahmen (wie die Armplätze der Floromantin)

- **Munitionstaschen der Werkrolle:** 4 am Anfang, 6 mit dem Panzerbeutel, 8 mit dem Seidenbeutel. Nur was in einer Tasche steckt, kann er im Kampf laden.
- **Steine** hat er immer, außerhalb der Taschen.
- **Vorrichtungen:** wie heute 2 bis 4 Plätze.
- **Nachfüllen** im Feld aus dem Materialbeutel (gibt es schon); **neu einteilen**, welche Formeln in den Taschen stecken, nur am Lagerfeuer (Feldkasten) oder in der Werkstatt.
- So ist er der Freieste in der Auswahl (rund 100 Formeln), aber im Kampf an 4 bis 8 gebunden. Vor jedem Gebiet entscheidet er, was er mitnimmt, wie ein Jäger in Monster Hunter.

---

## 6b. Rüstung als Formel (Boris, 14.11.: gemischte Sätze statt reiner Kreatur-Sätze)

Wie bei der Munition: Ein Rüstungsteil besteht aus **Platte + Gelenk + Fassung + Kern**, und die Teile dürfen von verschiedenen Kreaturen kommen. Ein Satz ist dann kein „Glutkriecher-Satz“, sondern eine Rüstung mit eigenem Charakter, die aus mehreren Tieren und Steinen zusammengebaut ist. Jeder Satz wächst in Stufen (I → II), wie die Waffen.

**Erster Satz: Schwarzglut-Rüstung** (schwere schwarze Platten)

| Rolle | Material | Warum |
|---|---|---|
| Platte | Krustenplatte (Glutkriecher), als Gehärtete Platte an der Esse | schwarze, überlappende Platten mit Glutrissen |
| Gelenk | Rollpanzer (Aschenroller) | Schulter- und Kniebänder, die sich wie beim Roller einrollen |
| Fassung | Schlackeneisen | Rahmen und Nieten |
| Kern | Glutstein (Brust) | speichert Wärme, die Risse glühen |
| Polster | Ordenstuch | darunter |

| Teil | Formel |
|---|---|
| Helm | 2 Gehärtete Platte + 1 Schlackeneisen + 1 Ordenstuch |
| Brust | 3 Gehärtete Platte + 1 Rollpanzer + 1 Glutstein + 2 Schlackeneisen + 1 Ordenstuch |
| Arme | 1 Gehärtete Platte + 2 Rollpanzer + 1 Schlackeneisen |
| Beine | 2 Gehärtete Platte + 1 Rollpanzer + 1 Schlackeneisen + 1 Kriechersehne |

- **2 Teile:** +15 % Feuerwiderstand, 10 % weniger Schaden.
- **4 Teile:** Nach dem Ausweichen rollt er sich eine Sekunde ein (60 % weniger Schaden, wie heute beim Panzer-Satz); wer ihn in dieser Zeit trifft, verbrennt sich an den Glutrissen.
- **Stufe II, Glutkronen-Rüstung** (nach der Glutkönigin): Brutkrone auf den Helm, Herz der Glut in die Brust. Die Risse glühen violett-orange; zusätzlich der Minen-Bonus des Glutkronen-Satzes.
- **Aussehen:** Die Platten werden aus dem echten Glutkriecher-Modell geschnitten, die Gelenkbänder aus dem Aschenroller-Modell, dazu Schlackeneisen-Nieten. Schwarz mit glühenden Rissen.

Nach diesem Muster lassen sich die anderen Sätze mischen, zum Beispiel eine leichte **Aschenjäger-Rüstung** aus Rußfeder, Schleierpelz und Greifklaue (Tempo, Hinterhalt) oder eine **Erntewacht** aus Sichelklinge, Puppengarn und Glimmstroh (Schaden, feuerfest).

---

## 7. Was in der Aschenmark fehlt (und wie ich es löse)

- **Metall:** Die Aschenmark hat kein echtes Erz. Lösung: Schlackeneisen (veredelte Schlacke) und Harpyientand als Messing. Echte Metalle kommen erst im Eisenschlund (Rosteisen, Kupferkristall, Silberlocke) als nächste Stufe.
- **Sehne und Feder:** Heute braucht fast alles den Spannfaden aus dem Wurzelforst. Mit Silberhalm, Rollsehne, Kriechersehne und Latex hat die Aschenmark eigene Bindungen. Der Bogen bleibt bewusst beim Wurzelforst.
- **Kleber:** Kelchschleim und Siegelwachs (Rotnoppe kommt erst im Wurzelforst).

---

## 8. Fragen an Boris

1. ~~Rollen-System oder feste Rezepte?~~ Entschieden 14.11.: feste Formeln aus Hülle, Kern und Verstärkern.
1b. ~~Bekannt oder entdeckt?~~ Entschieden 14.11.: Formeln lernt man durch Ausprobieren, erst sammeln, dann kombinieren.
2. Veredeln nur in der Werkstatt, oder auch am Feldkasten?
3. Die neuen Kreaturmaterialien (Krustenplatte, Kriechersehne, Harpyientand, Rollsehne, Puppengarn, Klammerfinger, Ordenstuch, Siegelwachs, Königinnenauge, Brutkrone, Legestachel): alle so übernehmen?
4. Eigene Bruchstücke je Kreatur statt des allgemeinen Bruchstücks?
5. Sollen Schattenmaterialien grundsätzlich an der Waage ziehen, wie die Nidladung?
