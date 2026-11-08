# Plan: NPCs, Lager und Dörfer in der offenen Welt

Stand 8. November 2026 · von Boris freigegeben (Entscheidungen unten), Schritte 1 bis 5 fertig

## Ziel

Die Aschenmark (und danach jede Region) bekommt bewohnte Orte: Lager, Weiler und Dörfer mit Leuten, die einen Namen, eine Herkunft, eine Rolle und eigenes Gerede haben. Die Namen stammen aus vielen Kulturen der echten Welt. So wirkt Vael wie ein Ort, an dem Menschen von überall gestrandet sind, und nicht wie ein einheitliches Fantasy-Mitteleuropa.

## Was es schon gibt

- `AVaelNpc` (Source/Vael/Story/VaelNpc.h) bietet:
  - Dialog, Laden und Gerede.
  - Den Platzhalter-Körper mit Robenfarbe.
  - Ein „!“ über dem Kopf, wenn die Person etwas Neues zu sagen hat.
- `UVaelGossip` hat Zeilen, die an Story-Flags hängen, und einen `SpeakerName` für Leute ohne eigenen Dialog.
- Bewohnt ist die Welt bisher von fünf Leuten: Edda Krell, Albrun, Maren Holt, zwei Überlebenden und der Gildenwache.
  - Sie stammen aus L_Aschenmark und kamen per PortAschenmarkToWelt.py nach L_Vael.
- Orte in `Scripts/Data/Welt/AM_Orte.json`:
  - 3 Lager.
  - 15 weitere Orte.
  - 13 Lichtungen.
- AusbauAschenmark.py zeigt das Muster für neue Aufbauten:
  - Es lädt alle WP-Actors.
  - Es setzt den Tag `VaelAusbau`.
  - Es ist idempotent.

## Begründung in der Welt: warum so viele Herkünfte?

Vorschlag, die Entscheidung liegt bei Boris: **Vael ist Strandgut der Welt.** Der Urdrachin liegt im Meer, auf halbem Weg zwischen vielen Küsten. Drei Wege bringen Menschen her:

- **Schiffbruch und Sturm:** Viele Familien leben seit Generationen auf Vael, weil ihre Vorfahren hier gestrandet sind. Ihre Namen haben sie behalten.
- **Die Gilde** wirbt Arbeiter von überall an. Im Kontor und in den Minen hört man viele Sprachen.
- **Die Akademie am Auge** zieht Gelehrte aus fernen Ländern an, und der **Orden** zieht Pilger an.

Daraus folgt eine feste Regel: **Keine Fraktion und kein Dorf gehört einer einzigen Kultur.** Herkunft ist Familiengeschichte, kein Stamm und keine Uniform. Das vermeidet Klischees wie „die Wüstenleute sind die Händler“.

## Namen aus aller Welt

### Datenformat

`Scripts/Data/Namen/<Kultur>.json` enthält pro Kulturkreis:

- Vornamen, getrennt in weiblich, männlich und neutral.
- Familiennamen.
- Die **Namensregel** der Kultur:
  - Vorname + Familienname (z. B. Yoruba, Japanisch, Slawisch).
  - Patronym (Nordisch: Halvardsdóttir, Amharisch: Vorname + Name des Vaters).
  - Einzelname (Mongolisch, teils Javanisch).
  - Vorname + zwei Nachnamen (Baskisch/Iberisch).

### Startumfang

18 Kulturkreise mit je etwa 30 bis 40 Vornamen und 20 Familiennamen. Beispiele:

| Kulturkreis | Beispiele |
|---|---|
| Westafrika (Yoruba, Akan, Igbo) | Adeola Adeyemi, Kwame Mensah, Ama Owusu, Chidi Okafor |
| Ostafrika (Swahili, Amharisch) | Amani, Zawadi, Baraka; Abebe Tesfaye, Selam Haile |
| Arabisch | Samira, Karim, Layla Haddad |
| Persisch | Dariush, Shirin, Parisa Karimi |
| Türkisch | Aylin Demir, Emre Kaya, Deniz |
| Südasien (Hindi, Tamil) | Priya, Arjun, Meera Iyer, Kavya, Selvan |
| Japanisch | Haruki Mori, Aiko Sato, Ren |
| Koreanisch | Ji-woo, Min-jun Park, Seo-yeon Han |
| Chinesisch | Mei Lin, Wei Zhang, Lian |
| Vietnamesisch und Philippinisch | Linh Tran, Minh; Dalisay, Bayani |
| Mongolisch | Batu, Altan, Saran (Einzelname + Vatername) |
| Slawisch | Mira Novak, Bogdan, Zora, Radomir |
| Nordisch | Sigrun Halvardsdóttir, Ylva, Halvard |
| Finnisch und Samisch | Aino, Ilmari; Elle, Áilu |
| Keltisch | Niamh, Ciarán, Eilidh, Bran |
| Baskisch und Iberisch | Itziar, Iker, Amaia Etxeberria |
| Andisch und Mesoamerikanisch (Quechua, Nahuatl) | Inti, Killa, Amaru; Citlali, Xóchitl |
| Polynesisch (Māori, Hawaiʻi) | Aroha, Kahu, Moana, Rewi |
| Mitteleuropa (wie bisher) | Edda Krell, Maren Holt, Albrun |

### Regeln für den Umgang

- **Nur echte Alltagsnamen.** Keine Götter- oder Heiligennamen, keine Titel, keine Namen, die eine Kultur als heilig ansieht (z. B. Tāne oder Tonatiuh).
- **Keine Witz- und keine Klangnamen.** Jeder Name wird beim Erstellen der Liste kurz geprüft (Bedeutung und Schreibweise). Die Quellen kommen in `Docs/Quellen.md`.
- **Indigene Kulturen Nordamerikas und Australiens bleiben vorerst außen vor.** Für sie bräuchte es eine Beratung, die wir nicht haben.
- **Feste Namen bleiben fest.** Story-NPCs (Edda, Maren, Albrun, später Ysra und die Moormutter) werden nie gewürfelt.
- **Mischen ist erwünscht.** Eine Familie kann einen Familiennamen aus einem Kulturkreis tragen und Vornamen aus einem anderen (Ehe, Adoption, Generationen auf Vael). Etwa 20 % der Leute mischen so.

### Generator

`Tools/Welt/namen.py` erzeugt die Namen:

- Er würfelt reproduzierbar: Der Seed ist Siedlung + Platznummer.
- Er sorgt dafür, dass jeder Name in der ganzen Welt nur einmal vorkommt.
- Er führt `Scripts/Data/Namen/Vergeben.json`. Wer einmal einen Namen hat, behält ihn über jeden Neuaufbau, damit Spielstände und Gerede stabil bleiben.

## Siedlungen: Typen und Rollen

`Scripts/Data/Welt/AM_Siedlungen.json` beschreibt jede Siedlung mit:

- Typ, Lage und Fraktion.
- Größe und Rollenplätzen.
- Gebäuden.
- Gerede-Thema.

| Typ | Leute | Inhalt |
|---|---|---|
| Lager (Schnellreise) | 6 bis 12 | Feuer, Zelte, Händler, Heiler oder Forscher, Wachen |
| Dorf | 10 bis 20 | Häuser, Brunnen, Werkstätten, Kinder, Alte, Tiere |
| Weiler oder Hof | 3 bis 6 | 1 bis 3 Häuser, eine Familie, ein Handwerk |
| Außenposten | 2 bis 5 | Wache, Pilger, Jäger, Einsiedler |
| Unterwegs | 1 bis 3 | Händlerkarren, Pilgergruppe oder Bote, die einer Straße folgen |

**Rollen** legen Platz, Haltung und Gerede fest:

- Händler, Wache, Handwerker (Schmied, Köhler, Gerber), Bauer, Heiler, Kind, alte Person, Pilger, Prediger, Jäger, Träger und Arbeiter, Gelehrter, Einsiedler.

**Vielfalt über den Namen hinaus**, als Parameter pro Person:

- Alter von Kind bis greis, sichtbar über Größe und Haltung.
- Körperbau.
- Kleidungsfarben nach Fraktion und Handwerk.
- Hilfsmittel wie Krücke, Augenklappe oder Stock.
- Geschlecht, mit etwa gleich vielen Frauen und Männern in allen Rollen. Wachen und Schmiede sind nicht nur Männer.

## Verteilung in der Aschenmark (≈ 60 Leute)

Die Aschenmark liegt nach der Mark-Flut am Boden, deshalb wenig Dörfer und viele Lager. Gebaut wird außerhalb der 40-m-Kampflichtungen und abseits der Straßen, wie bei der Streuung.

| Zone | Siedlung | Typ | Leute | Wer lebt dort |
|---|---|---|---|---|
| Kesselgrund | Lager der Überlebenden | Lager | 10 | Edda, Albrun, Familien aus dem Dorf, Kinder |
| Kesselgrund | Ruinen von Kesselgrund | Dorf (halb verlassen) | 6 | Wer nicht gehen wollte: Brunnenhüterin, eine alte Person, ein Plünderer |
| Kesselgrund | Hof am Ascheäcker | Hof | 4 | Eine Bauernfamilie, die trotz Asche sät |
| Kreuzung | Gildenkontor | Lager | 8 | Maren Holt, Wachen, Schreiber, Träger aus aller Welt |
| Kreuzung | Fuhrleute | Unterwegs | 3 | Karren Kontor ↔ Passwache |
| Schlackenhänge | Schlackengräber am Obsidianbruch | Weiler (neu) | 6 | Gilden-Arbeiter, Vorarbeiterin, Koch |
| Schlackenhänge | Gildenspeicher | verlassen | 0 | Quest-Ort, bleibt leer |
| Glutfelder | Glutjäger am Feldrand | Außenposten (neu) | 4 | Jäger auf Glutkriecher, Fallensteller |
| Glutfelder | Einsiedlerin an den Harpyienfelsen | Außenposten | 1 | Kennt die Königin, gibt Hinweise |
| Kathedrale | Orden der Narbe | Außenposten | 4 | Wächter der versiegelten Nid-Quelle |
| Aschenpass | Passwache | Lager | 6 | Wachen, Hauptfrau, Feldkoch |
| Aschenpass | Predigerlager | Lager | 8 | Prediger, Pilger aus fernen Ländern |
| Aschenpass | Windkante | Außenposten | 2 | Hirte mit Tieren, Windleser |

Zusammen sind das 4 Lager mit Schnellreise und 2 neue Weiler. Neue Schnellreisepunkte (Predigerlager) kommen in `camps` der Karte.

### Ausblick auf die anderen Regionen

Gleiches Muster, mehr Leben, weil diese Gegenden weniger zerstört sind:

- **Wurzelforst:** Waldranddorf am Forstsaum, Ysras Siedlung, Moorhütte der Moormutter, Harzsammler-Weiler.
- **Sturmzinnen:** Akademie am Auge, eine Stadt mit vielen Gelehrten aus aller Welt.
- **Eisenschlund:** Bergarbeiterstadt der Gilde, die vielfältigste Stadt.

## Technik

1. **`FVaelNpcIdentity` in AVaelNpc** mit den Feldern Name (FText), Herkunft (FName des Kulturkreises), Rolle (Enum), Fraktion, Alter und Körperbau.
   - Der Name erscheint über dem Kopf, sobald kein `Dialogue` gesetzt ist. Bisher zeigt sich dort nur der `SpeakerName` des Geredes.
2. **Rollen-Gerede:**
   - `DA_Gossip_<Rolle>_<Fraktion>` als Grundstock.
   - Eigene Zeilen pro Siedlung.
   - Platzhalter `{Name}` oder `{Ort}`, damit Leute einander und ihre Herkunft erwähnen („Meine Großmutter kam mit dem Schiff aus dem Süden …“).
3. **Tagesablauf (leicht):**
   - Jede Rolle hat 2 bis 3 Plätze in der Siedlung: Arbeit, Feuer, Schlafen.
   - Den Wechsel steuert VaelTimeOfDay.
   - Für den Anfang reicht Teleport zwischen den Plätzen außerhalb der Sicht oder einfaches Gehen über NavMesh.
   - Läufer auf Straßen folgen den Wegpunkten aus AM_Orte.json.
4. **Platzierung:** `Scripts/PlaceSiedlungen.py` nach dem Muster von AusbauAschenmark:
   - Es ist idempotent über den Tag `VaelSiedlung`.
   - Gebäude kommen aus den Packs und aus `aschenmark_bauten.py` (Zelte, Hütten, Werkbänke, prozedural, ohne Tripo).
   - NPCs stehen auf den Rollenplätzen und schauen zum Feuer.
   - Die Namen kommen aus `Vergeben.json`.
5. **Aussehen in Stufen:**
   - Erst die Platzhalter-Roben mit Fraktionsfarben, Hut- und Tuchteilen sowie Größe und Haltung nach Alter.
   - Später MetaHuman + Mutable. Dafür steht der Probelauf noch aus, und Boris muss im Editor sein.
   - Der Kulturkreis steuert dann Hauttöne, Haare und Gesichter; die Fraktion steuert die Kleidung.
6. **Karte:** Siedlungen werden Kartenbeschriftungen, die der Nebel erst freigibt. Lager werden Schnellreisepunkte.
7. **Leistung mit 4 Spielern:**
   - NPCs sind WP-gestreamt.
   - Tick und Gerede laufen nur in Spielernähe.
   - Budget: höchstens 25 Leute in einer Streaming-Zelle.
   - Der Stream-Test läuft danach mit allen Siedlungen.
8. **Kompendium (optional):** Eine Seite „Leute von Vael“ führt Getroffene mit Name, Herkunft und Rolle auf.

## Schritte

| Nr. | Schritt | Ergebnis | Aufwand |
|---|---|---|---|
| 1 | Namenslisten (18 Kulturkreise) + Generator + Prüfliste | `Scripts/Data/Namen/*`, `Tools/Welt/namen.py` | 1 Sitzung |
| 2 | `AM_Siedlungen.json` mit Lage, Rollenplätzen und Gebäuden | Datei + Vorschaubild auf der Karte | 0,5 Sitzung |
| 3 | `FVaelNpcIdentity`, Name über dem Kopf, Rollen-Gerede mit Platzhaltern | C++ + Story-Assets per Skript | 1 Sitzung |
| 4 | Hütten und Werkstätten prozedural (Blender) | `SM_AM_Huette`, `SM_AM_Werkbank` … | 1 Sitzung |
| 5 | `PlaceSiedlungen.py`: die Aschenmark wird bevölkert | ≈ 60 Leute in L_Vael | 1 Sitzung |
| 6 | Tagesablauf + Läufer auf Straßen | Leute wechseln Plätze, Karren fährt | 1 Sitzung |
| 7 | Karte und Schnellreise ergänzen, Stream-Test, Lore-Bibel (Tab „Leute“) | Test 0 Fehler, Commit | 0,5 Sitzung |
| 8 | Später: MetaHuman-Aussehen nach Herkunft | wartet auf den Probelauf mit Boris | offen |

## Entscheidungen von Boris (8. November 2026)

1. **Begründung:** „Strandgut der Welt“ passt.
2. **Namen:** echte Namen, aber leicht abgewandelt. Man hört, woher jemand kommt, aber kein Name ist 1:1 echt. Das macht der Generator mit Regeln pro Kulturkreis: typische Endungen tauschen, Vokale, Silben neu mischen.
3. **Dichte:** rund 60 Leute, gebündelt in den Dörfern und Lagern, ein paar verteilt dazwischen.
4. **Edda und Maren** bleiben, wie sie sind.
5. **Tagesablauf:** Teleport außerhalb der Sicht reicht.
6. **Prophet (neu):** Der Prophet braucht Menschen, die ihn lieben oder hassen. Jede Person bekommt deshalb eine **Haltung zum Propheten**: Anhänger, Gegner, Zweifler oder gleichgültig.
   - Die Haltung ist pro Siedlung gemischt. Im Predigerlager gibt es mehr Anhänger und Gegner, im Kontor mehr Gleichgültige.
   - Sie bestimmt Gerede und Reaktion („Seht nur! Er ist es!“ oder „Geh weiter, Falscher.“).
   - Sie kann sich später durch Taten der Gruppe ändern (Licht/Schatten, Gemeinden aus Akt I).
   - Das Feld kommt in Schritt 3 mit in `FVaelNpcIdentity`.

Statt einer Datei pro Kulturkreis liegt alles in einer Datei, `Scripts/Data/Namen/Kulturen.json` (20 Kulturkreise). Die vergebenen Namen stehen in `Scripts/Data/Namen/Vergeben.json`.

## Stand nach Schritt 2 (8. November 2026)

`py Tools/Welt/aschenmark_siedlungen.py` schreibt `Scripts/Data/Welt/AM_Siedlungen.json` und das Vorschaubild `Docs/Karte/aschenmark_siedlungen.png`.

- **Umfang:** 62 Leute in 12 Siedlungen. Alle 20 Kulturkreise sind vertreten.
- **Haltung zum Propheten:** 16 Anhänger, 18 Gegner, 16 Zweifler, 9 Gleichgültige. Bei 3 Story-Figuren (Edda, Albrun, Maren) entscheidet ihr Dialog.
- **Neue Siedlungen:** Der Generator sucht ihre Plätze selbst. Bedingungen:
  - Flaches Gelände.
  - Erreichbar von der Straße, aber nicht auf ihr.
  - Weg von Kampflichtungen, Lagern und Orten.
  - Ergebnis: Hof am Ascheacker, Schlackengräber, Glutjäger und Einsiedlerin der Harpyienfelsen.
- **Pro Person:** Rolle, Alter, Körperbau, gelegentlich ein Hilfsmittel, ein Haushalt und Plätze:
  - Arbeit am Tag, Feuer am Abend, Schlaf in der Nacht.
  - Die Fuhrleute haben stattdessen drei Halte auf der Straße zwischen Kontor und Passwache.
- **Haushalte:** Familien teilen ihren Familiennamen. Bei der Vater-Regel tragen Kinder den Vornamen des Vaters, etwa Mekdas Fikemu, Kind von Fikemu Beniam. Kinder übernehmen die Haltung zum Propheten von ihrer Familie.
- **Bestehende Actors:** Edda, Albrun, Maren, die beiden Überlebenden und die Gildenwache behalten ihren Actor und ihren Platz in L_Vael. Die drei Bystander bekommen jetzt Namen.
- **Neue Gebäude:** Hütten, Zelte, Werkbank, Gehege, Acker und Trockengestell sind als Vorschlag mit Lage eingetragen. Die Modelle kommen in Schritt 4.
- **Offen für Schritt 5:** In den portierten Orten (Lager, Kontor, Dorf, Kathedrale) kennt der Generator die Häuser nicht. Die Plätze dort werden beim Setzen in Unreal per Trace auf freien Boden geschoben.

## Stand nach Schritt 3 (8. November 2026)

- **C++:**
  - `FVaelNpcIdentity` in `Source/Vael/Story/VaelNpcIdentity.h` mit Name, Kultur, Rolle, Fraktion, Siedlung, Siedlungsname, Alter, Körperbau und Haltung zum Propheten (`EVaelProphetAttitude`).
  - `AVaelNpc` zeigt den Namen über dem Kopf, sobald es keinen Dialog gibt.
  - Der Platzhalter-Körper folgt Alter und Körperbau: Kinder klein, Alte leicht gebeugt, Kräftige breiter.
- **Gerede:**
  - Zeilen können an eine Rolle und an eine Haltung zum Propheten gebunden sein.
  - Ein Gerede-Asset kann andere einbinden (`Includes`).
  - Platzhalter: `{Name}`, `{Ort}`, `{Nachbar}` (eine andere Person derselben Siedlung).
- **Assets:** `Scripts/CreateSettlementGossip.py` schreibt `/Game/Vael/Story/Gerede`:
  - `DA_Gossip_Leute` mit 83 Zeilen: 22 zur Haltung zum Propheten, 6 zur Herkunft (Strandgut), dazu Zeilen pro Rolle.
  - 12 Assets `DA_Gossip_Siedlung_*`. Lager und Kontor binden ihr altes Gerede ein, das unverändert bleibt.
- **Bestehende Leute:** `Scripts/PlaceSiedlungen.py` gibt den 6 Leuten in L_Vael ihre Identität und den 3 Bystandern das Siedlungs-Gerede. Schritt 5 erweitert das Skript um das Setzen der übrigen 56.
- **Test im Spiel** (`Docs/Karte/npc_gerede_test.png`): Am Lager stehen „Cutlali Condora“ und „Yow Okunkwo“ über den Überlebenden. Cutlali, eine Anhängerin, sagt: „Die Asche wird weichen. Ich spüre es, seit sie da sind.“

## Stand nach Schritt 4 (8. November 2026)

`Tools/Welt/aschenmark_huetten.py` baut die Gebäude der Siedlungen prozedural in Blender, ohne Tripo. Die Vorschau liegt in `Docs/Karte/siedlungsbauten_vorschau.png`.

| Modell | Inhalt | Maße | Kollision |
|---|---|---|---|
| `SM_AM_Huette` | Blockhütte aus Rundhölzern, Tür mit Fellvorhang, durchhängendes Lederdach mit Flicken und Seilen, Feuerholzstapel | 5,1 × 5,9 × 3,5 m | einfache Kollision |
| `SM_AM_Werkbank` | Werkbank mit Schraubstock, Hammer, Zange, Eisenstäben und Seilrolle | – | einfache Kollision |
| `SM_AM_Gehege` | Gehege mit schiefen Pfosten, offenem Tor, Trog und Stroh | 7,5 × 5 m | wie es aussieht |
| `SM_AM_Acker` | Erdfurchen mit blassen Trieben, Pflöcke mit Schnur, Vogelscheuche | 10,7 × 7 m | wie es aussieht |
| `SM_AM_Trockengestell` | Trockengestell der Jäger mit Häuten und glühenden Glutkriecher-Panzern an Schnüren | – | wie es aussieht |

- **Import:** `Scripts/ImportSiedlungsBauten.py` legt die Modelle nach `/Game/Vael/Maps/Aschenmark/Bauten`, mit den Materialien des Glutkammer-Kits.
- **Material:** Neu ist der Slot Erde, belegt mit `M_GK_Erde`.
- **Feuer:** Das Feuer der Siedlungen kommt in Schritt 5 als Feuerstelle wie in den Lagern, nicht als Modell.

## Stand nach Schritt 5 (8. November 2026)

`Scripts/PlaceSiedlungen.py` bevölkert die Aschenmark in L_Vael und ist wiederholbar: Alles Gesetzte trägt den Tag `VaelSiedlung` und wird beim nächsten Lauf ersetzt.

- **Gesetzt:** 56 neue Leute auf ihrem Arbeitsplatz. Name, Identität und Siedlungs-Gerede sind gesetzt, die Robenfarbe richtet sich nach der Fraktion:
  - Überlebende: erdbraun
  - Gilde: blau
  - Orden: dunkel violett
  - Passwache: graugrün
  - Freie: lederfarben
- **Gebäude:** Hof, Schlackengräber, Glutjäger, Einsiedlerin und Windkante haben ihre Gebäude und Feuerstellen (`VaelFireSpot` + `VaelFireLight`). Zusammen mit den Leuten sind das 79 Actors.
- **Bestehende Leute:** Die 6 Leute, die schon in L_Vael standen, behalten ihren Platz und werden nur aktualisiert.
- **Freier Boden:** Geprüft wird über die Bounds aller sichtbaren Actors mit Collision, 12.065 Hindernisse.
  - Traces funktionieren im Commandlet nicht, weil Meshes dort keine Physik haben. Ein Trace durch eine gesetzte Hütte traf nur die Landschaft.
  - 11 Leute sind 0,8 bis 3 m ausgewichen, vor allem in Lager, Dorf und Kontor.
- **Im Spiel gesehen:** Schlackengräber, Hof, Predigerlager, Glutjäger, Lager und Kontor. Die Leute stehen frei, die Namen sind sichtbar, Kinder sind kleiner.
- **Hinweis zum Cheat:** `VaelTeleport` genau auf die Dorfmitte landet auf einer Ruinenmauer. Die Figur erscheint dann wieder im Lager. Das betrifft den Test-Cheat, nicht die NPCs.
