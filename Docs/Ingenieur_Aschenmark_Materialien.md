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
**Gebaut 26.11.:** Die drei Gläser sitzen in der Tripo-Brille (eigener Glas-Slot, eingefärbt), Rauchstaub = Rauchblase, Adernflechte = Myzelprobe. Wärmeglas: 20 % weniger Feuerschaden, eingegrabene Glutkriecher bis 15 m steigen als orange Hitze aus der Asche. Rauchglas: Gegner, die im Rauch taumeln, trifft er immer kritisch. Aderglas: unterirdische Stücke der Nid-Adern bis 12 m dampfen violett aus dem Boden, offene Nidquellen pulsieren. Er trägt das zuletzt gebaute Glas. Test: `VaelIngenieur glaeser`, `VaelIngenieur brille <Glas>`.
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
- **Aussehen** (Boris' Vorlagebild, 14.11.): schwere, glatte schwarze Platten in Lagen übereinander, an der Unterkante jeder Platte eine glühende Linie, große mehrlagige Schulterstücke, ein Helm mit Visierschlitz, glühenden Augen und zwei nach hinten geschwungenen Hörnern, Platten über den Oberschenkeln, Stacheln an Schultern, Unterarmen und Waden. Jede Platte ist eine **ganze** Rückenplatte des Glutkriechers (geschnitten an den Stufen seines Panzers) mit sauber gerundetem Umriss und Schlackeneisen-Rand. Bauch, Unterarm und Knie bekommen Bänder des Aschenrollers. Skript: `Tools/Ingenieur/vael_ingenieur_schwarzglut.py`.

**Fünftes Teil: Klappschild (Turmschild)**

| Teil | Formel |
|---|---|
| Klappschild | 3 Rollpanzer + 1 Rollsehne + 1 Schlackeneisen + 1 Gehärtete Platte |

- Fast der ganze Rückenpanzer eines Aschenrollers, in seine fünf Bandreihen geteilt (je 20 cm, zusammen etwa 1 m hoch und 55 cm breit). Zusammengeschoben liegt er auf dem linken Unterarm; beim Blocken schieben sich die Reihen wie beim Entrollen in 0,18 s zum gewölbten Turmschild aus.
- **Mit dem Glutpanzer (16.11.):** Trägt er die Glutpanzer-Brust, ist ihr ganzer Rückenpanzer der Schild (SKM_Ing_GP_Schild, der ganze Gürteltier-Panzer aus 11 Tripo-Teilen: 0, 14, 41, Nacken 46, 48, 53, 58, obere Seitenreihen 22, 23, Kuppel 27, 51). Beim Blocken löst er sich in 0,32 s als ein Stück vom Rücken, wandert über die linke Schulter nach vorn und steht dort als Turmschild von der Schulter bis zum Knie. Beim Loslassen geht es denselben Weg zurück auf den Rücken. Der Klappschild am Unterarm bleibt dann weg. Damit der Rücken geschlossen bleibt, wenn der Schild vorn ist, trägt die Brust hinten gespiegelte Platten aus ihrer eigenen Vorderseite, und am Nacken liegen Lamellen aus dem Helmkranz (Boris: keine zweite Schildplatte). Test: `VaelIngenieur panzerschild [0..1]` hält den Panzer an einer Stelle des Wegs fest.
- **Bedienung:** L2 bzw. rechte Maustaste **tippen** legt weiter das Gerät, **halten** (ab 0,2 s) hebt den Schild. Ohne Schild legt auch langes Drücken das Gerät.
- **Wirkung:** Was von vorn kommt (breiter Kegel), wird um 70 % gemindert. In den ersten 0,35 s nach dem Anheben kommt gar nichts durch (perfekter Block). Hinter dem Schild geht er mit 55 % Tempo und schießt nicht.
- **Mit vollem Satz:** Ein perfekter Block wirft den Angreifer zurück und setzt ihn in Brand.
- Der Schild zählt nicht zu den 2 und 4 Teilen des Satzes.
- **Später:** die „Rollwall“ als Wurfgerät, die über den Boden rollt und als Deckung aufschnellt (Geräte-Liste).

**Schwer, und die Panzerkugel als Ausgleich (Boris, 17.11.)**

- **Tempo:** Jedes getragene Schwarzglut-Rüstungsteil macht ihn 3 % langsamer (volle Rüstung 88 %, der Schild zählt nicht). Hinter dem gehobenen Schild kommt die 55 % noch dazu, zusammen etwa 48 %.
- **Panzerkugel (4 Teile):** Beim Ausweichen rollt er sich zur Kugel ein. Das ist die eingerollte Aschenroller-Kugel, überzogen mit dem Rückenpanzer des Glutpanzers, einmal oben und einmal unten herum (Tools/Ingenieur/vael_panzerkugel.py, SM_Ing_Panzerkugel, 1,2 m Durchmesser für die 2 m große Glutpanzer-Figur). Nur Schildplatten, zweimal um die Achse gelegt, mit glühenden Fugen, und an jedem Achsenende zwei Gruppen Schulterzacken (Boris: weniger ist mehr; ein Versuch mit der ganzen Rüstung auf der Kugel war zu unruhig). Beim Einrollen und Öffnen verdeckt ein Stoß aus Asche und Glut den Wechsel.
- **Antippen:** die normale Ausweichrolle (0,4 s, in der Zeit unverwundbar).
- **Halten:** Die Kugel rollt weiter, bis zu 3 s insgesamt, mit 8,5 m/s, bergab bis zu doppelt so schnell. Sie lenkt träge (140° pro Sekunde) zum Stick. Nach den ersten 0,4 s ist er nicht mehr unverwundbar, nimmt eingerollt aber 60 % weniger Schaden, und wer ihn trifft, fängt Feuer.
- **Gegner im Weg** fliegen beiseite (Rückstoß 650) und bekommen 10 Feuerschaden, jeder einmal pro Rolle.
- **Gegen eine Wand** bleibt die Kugel stehen und er ist 0,8 s benommen, wie der Aschenroller auf dem Rücken.
- **Test:** `VaelIngenieur kugel [Sekunden gehalten]`, `VaelIngenieur kugelwand [Sekunden]` (rollt zur nächsten Wand), `VaelIngenieur kugelstehen` (zeigt die Kugel still).

### Das Menü des Ingenieurs und sein Werkzeugkasten (Boris, 17.11.)

- **Alles außer „Gestalt“ im Ingenieurstil:** Kompendium und Aufträge stehen in seinem Werkstattfenster aus rotem Werkleder auf dunklem Eisen. Dazu ein Messingrand mit Nieten, eiserne Eckwinkel und oben ein Zahnrad mit Glutstein (`UI/VaelHUDToolbox.cpp`, DrawEngineerWindow). Der erste Reiter heißt bei ihm WERKROLLE statt FORMELN.
- **Seine Tasche ist kein Buch, sondern ein aufgeklappter Werkzeugkasten** (`Tools/HUD/vael_hud_gemalt.py`, inventory_engineer → `T_HUD_Gemalt_Inventar_Ingenieur`):
  - Links der Deckel mit einer Bauzeichnung als Bedienungsanleitung. Darauf sitzen die zehn Ausrüstungsplätze als Messingfassungen auf einem gezeichneten Zahnrad und darunter der Werkzeuggürtel.
  - In der Mitte das Scharnier.
  - Rechts die eingeklemmte Anleitung (dort steht, was gewählt ist), darunter die eisernen Fächer, 7 × 5.
  - Die Plätze liegen genau wie im Grimoire des Magiers.
- **Der Deckel als Bauplan (Boris 17.11., zweite Fassung, „wie bei Iron Man, aber nicht so futuristisch“):**
  - Statt Papier eine rußgeschwärzte Eisenplatte mit Kupferraster.
  - Darauf die **Glutzeichnung**: der echte Glutpanzer von vorn, als Freestyle-Linien in Kupfer, die Glutrisse der Platten glimmen. Kein Blau, das bleibt beim Magier.
  - Fünf Kacheln wie bei Diablo, unterschiedlich groß (Helm, Arme, Beine links; Brust und Schild rechts). Von jeder Kachel läuft eine Linie zu ihrem Teil auf der Zeichnung: glühend, wenn angelegt, sonst matt.
  - Angelegt zeigt die Kachel das Teil selbst, leer seinen dunklen Geist. Darunter vier Gürtelplätze.
  - Die Zeichnung leuchtet mit jedem angelegten Teil etwas heller.
- **Bilder der Teile wie beim Schmied in Monster Hunter** (`Tools/Ingenieur/vael_glutpanzer_bilder.py` → `Vael_Quellen/Ingenieur/Ruestung/08_bilder`, Import `Scripts/ImportRuestungBilder.py` → `/Game/Vael/UI/HUD/Ruestung`):
  - Jedes Teil aus dem Rig, mit Texturen, schräg von vorn gerendert, freigestellt.
  - Die Arme zeigen nur einen Arm.
- **Erster Reiter „Rüstung“** statt des Rucksacks, nur beim Ingenieur:
  - In den Fächern liegen die gebauten Teile, die er nicht trägt.
  - Bestätigen auf einem Fach legt das Teil an, auf einer Kachel legt es ab.
  - Die Anleitung zeigt das gewählte Teil groß mit Platz, Rüstung, Leben, Text und Satz („x von 4 angelegt“). Greifende Boni stehen glutrot.
- **Nur noch die Schwarzglut-Rüstung:** Die vier Kreatur-Sätze (Panzer, Sichel, Ruß, Blutschuppen) und ihre zwei Einfälle sind abgeschaltet, bis sie Modelle haben (`bCreatureSets` in `VaelEngineerGear.cpp`, `bCreatureArmourIdeas` in `VaelEngineerIdeas.cpp`). Alte Spielstände legen solche Teile beim Laden ab.
- **Zum Ausprobieren:** Öffnet der Ingenieur das Menü, liegt der ganze Schwarzglut-Satz (Helm, Brust, Arme, Beine, Klappschild) gebaut im Kasten. Abschalten mit `bTestArmorInToolbox` in `VaelEngineerComponent.cpp`.
- **Test:** `VaelMenue Ruestung<n>` (Kasten auf, Zeile n: 0–4 die Kacheln, ab 5 die Fächer), `VaelIngenieur ablegen <Platz>`.

### Bilder aller Materialien und Farben je Gruppe (Boris, 17.11.)

- **Bilder:** Alle 145 Materialien sind 3D-Renders im Licht der Rüstungsteile: 512 px, groß im Bild, Kantenlicht von hinten (`Tools/Loot/vael_material_bilder.py`, Liste aus `Scripts/ExportMaterialList.py`, Import und Zuweisung mit `Scripts/ImportMaterialBilder.py` nach `/Game/Vael/Items/Icons/Bilder`).
  - Die 14 Erze und Steine kommen aus ihren echten Tripo-Modellen.
  - Alles andere hat eine Form nach seiner Beschreibung: Krustenplatten mit Glutrissen, Rollpanzer-Band, Sehnen, Garnknäuel, Federn, Klauen, Schnabel, Stachel, Kiefer, Kronen, Drüsen und Herzen mit glühenden Adern, Augen, Perlen, Felle, Ordenstuch, Siegel, Barren, Messingblech, Latexrollen, Linse, Phiolen, Nektartöpfe, Pilze, Myzel, Kristalle.
  - Albruns acht Samen sind so gebaut, wie er sie beschreibt.
  - Tränke stehen in Flaschen nach Seltenheit.
- **Hintergrund:** Das Spiel malt ihn selbst (`DrawGroupBackdrop`): ein dunkles Feld, hinter der Mitte ein weicher Schein in der Farbe der Gruppe. Die Seltenheit bleibt am Rahmen.
- **Gruppen** (`Items/VaelItemGroups`, zum späteren Sortieren; die Anleitung nennt die Gruppe statt der alten Kategorie):

| Gruppe | Farbe |
|---|---|
| Erz und Gestein | stahlblau |
| Kristall | violett |
| Kreaturteil | blutrot |
| Werkstoff | messing |
| Pflanze | grün |
| Saft und Tau | wasserblau |
| Pollen und Staub | pollengelb |
| Pilz | erdbraun |
| Saat | junggrün |
| Trank | magenta |
| Rüstung | glutorange |

- **Zweite Fassung, gemalt (Boris 17.11., Referenz: gemalte Itembilder mit viel Eigenheit, schwere Rahmen):** Die 3D-Renders sind ersetzt.
  - **Bildbeschreibung:** Jedes Material hat eine eigene Beschreibung (`Tools/Loot/vael_bildbogen_motive.py`). Tränke bekommen jeweils eine eigene Flaschenform, Glas mit Blasen, Siegel, Anhänger.
  - **Erzeugung:** `vael_bildbogen_prompts.py` schreibt 37 Bögen zu 2 × 2 auf Chroma-Grund: grün, magenta oder blau, je nach den Farben der Dinge. Die Bilder kommen aus Nano Banana in Tripo, freie Bilder, 0 Credits (rund 61 von 100 freien Bildern verbraucht). Ablage: `Vael_Quellen/Icons/boegen`.
  - **Freistellen:** `vael_bildbogen_schneiden.py` misst die Hintergrundfarbe am Rand jedes Viertels und rechnet sie heraus. Glas und Rauch bleiben halbdurchsichtig, Schatten werden weiches Schwarz. Ergebnis 512 px in `Vael_Quellen/Icons/gemalt`, Import mit `Scripts/ImportMaterialBilder.py`.
  - **Rahmen je Seltenheit** (`Tools/HUD/vael_hud_gemalt.py`, `item_frame`, `T_HUD_Gemalt_Itemrahmen_0..4`):

| Seltenheit | Rahmen |
|---|---|
| Gewöhnlich | dunkles Eisen mit Nieten |
| Magisch | Stahl mit blauer Einlage |
| Selten | Gold mit Zierecken |
| Legendär | Schwarzeisen mit Gold und Glutsteinen |
| Einzigartig | Kupfer mit Altgold-Schnörkeln |

  - **Offen:** An einigen glühenden Teilen von grünen Bögen bleibt ein leichter gelbgrüner Saum.
- **Achtung:** `Scripts/CreateLootKit.py` setzt beim nächsten Lauf die alten Bilder seiner fünf Beutestücke wieder; danach `ImportMaterialBilder.py` erneut laufen lassen.
- **Test:** `VaelMaterial alle 3` (jedes Material dreimal in den Beutel), `VaelMenue Beutel<n>` (Reiter n des Beutels).

Nach diesem Muster lassen sich die anderen Sätze mischen, zum Beispiel eine leichte **Aschenjäger-Rüstung** aus Rußfeder, Schleierpelz und Greifklaue (Tempo, Hinterhalt) oder eine **Erntewacht** aus Sichelklinge, Puppengarn und Glimmstroh (Schaden, feuerfest).

---

## 6c. Fallen-Paket (gebaut 25.11.)

Grabmine, Rauchtopf, Lockpfeife und Wurfhaken sind im Spiel. Bauplan, Wirkung und Einfälle stehen in `Docs/Doc-Nachtraege.md`. Abweichungen von den Entwürfen in Abschnitt 5:

- **Rauchtopf** mit 2 Rauchblasen statt Rauchstaub, weil es Rauchstaub als Material nicht gibt.
- **Lockpfeife** mit Pfeifensporen, das ist das Material der Pfeifenmorchel.
- Lockpfeife und Wurfhaken sind **Vorrichtungen, die bleiben** (`FVaelGear::Cooldown`). Dadurch braucht es keine neue Taste.
- Code: `Engineer/VaelGrappleHook.*` (neu), `EVaelDeviceType` BuriedMine/Smoke/Whistle/Hook, `UVaelEngineerComponent::Whistle`, `AVaelCreature::LureTo` mit Tempo.
- Test: `VaelIngenieur fallen` (alle Schritte PASS).
- Modelle der vier Fallen gebaut und importiert (VAEL_ING_FALLEN, VAEL_ING_NUR_FALLEN), Abklingzeit im HUD.
- **Umwelt (25.11.):** Glutknall, Dampfknall, Rauch im Wind, Glutzapfer, Säurefass, Löschpulver (Doc-Nachtraege). Test `VaelIngenieur umwelt` (alle PASS).
- Modelle für Glutzapfer (mit Kieferdüse), Säurefass (mit Deckel) und Löschpulver gebaut und importiert, animiert in `AVaelEngineerDevice::Animate`: Der Zapfer fährt aus dem Boden, atmet, dreht die Düse zum Ziel, hat Rückstoß und lässt die Düse ohne Feuer hängen. Das Fass schaukelt, rüttelt bei nahen Gegnern, schwillt beim Platzen, der Deckel fliegt weg, die Pfütze breitet sich aus und schrumpft am Ende. Der Pulverbeutel fällt, staucht sich und pufft.
- **Fünf weitere Vorrichtungen (25.11.):** Sichelkreisel, Würgnetz, Blendlaterne, Sturmwarner, Rollwall mit Modellen und Bewegung (`AVaelEngineerDevice::TickNewer` und `Animate`). Test `VaelIngenieur geraete` (alle PASS).
- **Quarzbrille (25.11.):** Die Zielhilfebrille hat ein eigenes Modell (`VAEL_ING_BRILLE=1`, `SM_Ing_Quarzbrille`): Augenschalen aus Schlackeneisen mit Latexdichtung und Messingnieten, Gläser aus Rauchquarz, Nasensteg, rechts eine hochgeklappte Zielhilfe-Lupe, ein geflochtenes Silberhalm-Band um den Hinterkopf. Sie ist an die Augenknochen des MetaHuman-Gesichts angepasst und hängt am Kopfknochen des Gesichts (`AttachQuartzGoggles`), mit Farben über die Glutkammer-Materialien. Messen mit `VaelIngenieur gesicht`, Bild mit `-VaelShowcaseHero=Kopf` bzw. `KopfSeite` und `-VaelShowcaseCmd=VaelIngenieur_brille`. Bilder: Docs/Bilder/Ingenieur/Quarzbrille_*.png.
- **Offen:** Rammschild; Werkzeuge (Wärmeglas, Aderglas, Fernglas); weitere ganze Sätze (Aschenjäger, Erntewacht, Glutkronen).
- **Rammschild und Fernglas (25.11.)** gebaut, damit ist die Aschenmark ohne Tripo fertig; dazu die Sprengmeister-Technik und das Verlangen (Doc-Nachtraege).
- **Tripo-Bauliste:** Alle offenen Modelle stehen in `Docs/Ingenieur_Tripo_Bauliste.md` (Aschenmark und Wurzelforst, Rüstungen als Symbiose aus 2–3 Monstern).


## 6d. Aschenjäger (Idee, besprochen 25.11., NICHT gebaut)

Boris' Idee, nur festgehalten: Ein zweiter, leichter Satz als Gegenstück zur schweren Schwarzglut. Er ist schlank, mit Bandagen gewickelt und mit Federn besetzt, dunkel bis aschgrau, für beide Geschlechter. Die Stimmung ist von einem verhüllten Ninja-Kämpfer inspiriert. Form und Symbole kommen aber nur aus unseren Kreaturen, kein Nachbau. Er arbeitet viel mit Rauch. Gebaut wird erst, wenn Boris am PC ist (Tripo-Modelle).

**Drei Wesen (entschieden):**
- **Sichelpuppe:** Puppengarn als Bandagen (glimmt leicht, schwacher Glutfaden an den Kanten), Sichelklingen für die Waffe.
- **Aschharpyie:** Rußfedern an Kragen, Unterarmen und Kapuze, Greifklauen als Knöchelschutz.
- **Schleieraffe:** passt mit seinen Bandagen, der Wendigkeit und den dunklen Elementen. Vorschlag: Schleierpelz für Kapuze und Schal, Klammerfinger in den Handwicklungen (sicheres Fangen), Glimmaugen als Gläser in der Maske (Sicht durch Rauch).

**Entschieden:**
- **Name:** Aschenjäger.
- Wie alle Sätze nur **ganz** tragbar.
- **Waffe Doppelsichel:** zwei Sichelklingen in S-Form an der Nabe vernietet. Sie **ersetzt die Schleuder**, solange der Satz getragen wird, und hat einen eigenen Spielstil.
  - Sie **kehrt immer zurück** („Rauch-Dreh-Sichel-Spiel“).
  - Antippen wirft gerade, Halten wirft im Bogen um den Zielpunkt.
  - Genau gefangen wird der nächste Wurf schneller.
- **Rauchschritt** statt Ausweichrolle: ein kurzer Satz zur Seite in einer Rauchwolke, Gegner verlieren ihn kurz, der erste Wurf aus dem Rauch trifft kritisch.

**Vorschläge, noch nicht entschieden:**
- **Satzbonus:** etwa 10 % schneller, deutlich weniger Rüstung als die Schwarzglut.
- **Umwelt:** durch Feuer kommt die Sichel brennend zurück, durch die Säurepfütze ätzend, durch Rauch mit Rauchspur. Streift sie eine Mine, zündet sie diese.
- **Formeln:** Die Ladungsformeln **bestreichen** die Klingen (Gift, Glut, Rauch), statt verschossen zu werden.
- **Einfall:** Die Sichelpuppe beim Wirbeln besiegen gibt die Doppelsichel. Rußfedern als Stabilisator kommen von der Harpyie.
- **Bauplan-Skizze Waffe:** 2 Sichelklinge + Puppengarn + Schlackeneisen (Niete) + Rußfeder.

**Stufe II (Idee Boris):**
- Die **Harpyien-Älteste** ist kein großer Boss wie die Glutkönigin, sie passt deshalb zum Ausbau.
- **Wirbeltanz:** Er dreht sich mit den Sicheln, die um ihn kreisen, ein Tanz aus Kreisen. Im richtigen Moment wird daraus ein **Aschenteufel** (Staubteufel).
- Mein Vorschlag dazu:
  - Der Aschenteufel wandert eine Weile, zieht kleine Gegner hinein und trägt Rauch.
  - **Umwelt:** Zieht er durch ein Feuer, wird er zum Feuerteufel. Im Sturm wird er größer und hält länger, im Regen zum Aschenregen.
  - Die Älteste verstärkt ihn: Ihr Kreischen könnte Gegner im Teufel betäuben.
- **Materialien (entschieden 25.11.):** Ältestenschwinge plus **beide** Windpflanzen, Silberhalm bzw. Flüsterfaser vom Windhalm **und** Pfeifensporen der Pfeifenmorchel. Monsterteile sind sonst der Kern, hier dürfen mehrere Pflanzenteile dazu. Als Stufe II darf sie schwerer zu bauen sein und mehr Materialien brauchen.

**Gebaut 26.11.:** Das Tripo-Modell ist im Spiel (vier Teile als eigener Körper wie der Glutpanzer, `Tools/Ingenieur/vael_aschenjaeger_rig.py`), mit Rezepten und 10 % mehr Tempo ab zwei Teilen. Mit allen vier Teilen ersetzt die **Doppelsichel** die Schleuder (`AVaelDoubleSickle`):
- Antippen: gerade bis zum Zielpunkt (höchstens 11 m) und zurück. Halten (ab 0,3 s): eine Bumerang-Schleife rechts hinaus, durch den Zielpunkt, links zurück.
- Sie schneidet jeden Gegner auf dem Hin- und auf dem Rückweg je einmal (16 Schaden pro Schnitt, mit den üblichen Schadensboni) und kommt immer zur Hand zurück. Solange sie fliegt, kein zweiter Wurf, und sie fehlt am Gürtel.
- Fangen: Die Angriffstaste höchstens 0,3 s vor der Ankunft gedrückt, dann „Gefangen!“, und der nächste Wurf ist 30 % schneller und härter. Das HUD zeigt „Doppelsichel · gefangen“.
- **Rauchschritt** (26.11.): Ausweichen im ganzen Satz lässt Rauch, wo er absprang und landet (3,8 m), Gegner darin taumeln 1,5 s, der erste Treffer innerhalb von 3 s ist kritisch („Aus dem Rauch!“). Die Sichel zählt wie die Schleuder (Schwachstellen, Krit).- **Ausbau (26.11.):** Eine gewählte Ladung wird auf die Klingen gestrichen statt verschossen (eine pro Wurf, jeder Schnitt trägt ihre Wirkung). Blanke Klingen nehmen im Flug auf: Feuer am Boden → brennend (Glutladung), Rauch von Rauchladung oder Rauchschritt → Rauchspur (Rauchladung), Säurefass-Pfütze → ätzend (Säureladung). Streift sie eine eigene Mine oder ein Säurefass, geht es los, mit der Wirkung der Klingen.
- Tests: `VaelIngenieur sichel`, `sichelumwelt`, `rauchschritt` (alles PASS). Noch offen: „nur ganz tragbar“, Stufe II Wirbeltanz (braucht das Tripo-Modell A5).

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
