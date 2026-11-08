# Goal: Aschenmark lebendig machen (Anschluss an NPC-Plan Schritt 6)

Stand 8.11.2026. Reihenfolge = Arbeitsreihenfolge. Nach jedem Punkt: im Spiel prüfen (Bilder), Lore-Bibel/Aufgaben-Doc, Commit + Push.
Weckzeiten nach dem 5-Stunden-Limit: 19:05, dann alle 5 Stunden (00:05, 05:05, 10:05 …).

## 0. Floromantin zurück auf ihr Modell (schnell, zuerst)
- Ursache der schwarzen Form finden (Verdacht: Schatten-/Nid-Färbung landet auf ihrem ganzen Körper statt nur auf den Pflanzenarmen).
- Sie bekommt wieder ihr bisheriges Aussehen, die Nid-Adern bleiben im HUD und an den Pflanzen.
- Nid-Zustand später über Flügel oder Kleidung zeigen (Licht, Zwielicht, Schatten), dafür nur eine Notiz in der Lore-Bibel.

## 6. Tagesablauf der NPCs (aus dem NPC-Plan)
- Arbeit, Feuer, Schlaf nach Tageszeit, Wechsel außerhalb der Sicht.
- Fuhrleute zwischen ihren Halten.
- Plätze für Feuer und Schlaf mit derselben Freiflächenprüfung wie die Arbeitsplätze.

## 7. Karte und Abschluss NPC-Plan
- Gerade Weglinien aus der Karte entfernen.
- Siedlungsnamen auf die Karte, Predigerlager als Reiselager.
- Lore-Bibel-Reiter „Leute“.

## 8. Boden ohne Wiederholung
- M_AM_Boden neu: mehrere Megascans-Oberflächen (Asche, Schlacke, rissige Erde, verbrannter Grasboden, Fels, Kies).
- Gemischt über Hangneigung, Höhe und große Rauschmasken.
- Kachelung brechen durch zufällig gedrehte und versetzte Kacheln (Hex-Tiling) plus großflächige Farbvariation, die über 50–100 m wechselt.
- Wege als eigene Schicht (festgetretene Asche, Fahrspuren).
- Abdrücke (Decals) für Brandflecken, Glutrisse und Aschewehen.
- Bilder aus gleicher Kamerahöhe vorher/nachher.

## 9. Kulissen-Baukasten Aschenmark
- Aus den Packs:
  - tote Kiefern, umgestürzte Stämme, Stümpfe, Wurzeln;
  - Rock_Collection_04, Isländische Vulkanfelsen;
  - Mauern und Ruinen (Medieval Modular, CastleWall);
  - Gräber, Amboss, Kohle, Fässer;
  - Schilf und Rohr am Teich.
- Eigene Blender-Teile: Zelte, Hütten, Gestelle, Zäune, dazu neu Schlackenbrocken, Glutrisse, verkohlte Pfähle, Knochen und Gerippe großer Tiere, Wegmarken, Grabhügel, eingestürzte Karren.
- Einfärben über neue Material-Instanzen (Asche, Ruß, Glutkante), an den Pack-Assets wird nichts verändert. Blender nur, wo eine Form fehlt.
- Ohne Stylized_Tree_Pack, der ist zu cartoony.
- Vorschau-Blatt für Boris vor dem Verteilen.

## 10. Aschenmark füllen, Zone für Zone
- Jede Zone mit eigenem Charakter:
  - Kesselgrund: Ruinen, Felder, tote Haine;
  - Schlackenhänge: Halden, Schlackenströme, Bruch;
  - Glutfelder: Glutrisse, Rauchsäulen, Nester;
  - Aschenpass: Fels, Wind, Moos zum Forst hin.
- Dichte in Ringen:
  - Wegränder mit Kleinkram;
  - Felder mit Gruppen (Haine, Felsnester, Ruinen) alle 30–60 m;
  - Kampflichtungen von 40 m bleiben frei;
  - Zonenränder sind dicht und hoch.
- Kleine Fundorte („da war etwas“): verlassenes Lager, Wrack eines Karrens, Grab mit Opfergaben, Gerippe, Wachturmrest. Ungefähr alle 100–150 m einer, gern mit Truhe oder Erntegut.
- Atmosphäre: Rauch- und Ascheflocken-Effekte, Glutpartikel in Rissen, Nebel in Senken.
- Skript idempotent (eigener Tag), Leistung mit 4 Spielern messen.

## 11. Nichts schwebt, nichts steckt fest
- Setzen über mehrere Bodenpunkte. Traces treffen im Headless-Lauf die Landschaft, das reicht hier.
- Felsen werden an der Hangneigung ausgerichtet und versenkt.
- Zäune: jeder Pfosten einzeln am Boden, Bretter folgen.
- Gebäude bekommen einen Sockel oder werden eingegraben.
- Prüflauf über alle bestehenden Actors in L_Vael (auch Port- und Ausbau-Teile): Liste der schwebenden und vergrabenen Teile, dann automatisch korrigieren.
- Boden-IK für Füße (Spieler, NPCs, Gegner am Hang) als eigener kleiner Punkt. Gegen schwebende Requisiten hilft sie nicht.

## 12. Gegner überall außerhalb der Schutzzonen
- Spawnpunkte alle 40–60 m an Wegen und Feldern (~150 Punkte), Gruppen von 3–6 aus den Aschenmark-Kreaturen:
  - Aschenroller, Glutkriecher, Aschharpyie, Schleieraffe, Sichelpuppe;
  - selten Harpyien-Älteste oder Elite.
- Jeweils passend zur Zone.
- Spawnen erst in Spielernähe, verschwinden weit weg, Respawn-Zeit. Damit bleibt es mit 4 Spielern flüssig.
- Schutzzonen (Lager, Dorf, Kontor, Siedlungen) plus 30 m Rand bleiben frei. Wege werden etwas dünner besetzt als die Felder.
- Stufen nach Entfernung vom Startlager.

## 13. Ingenieur sauber
- Rucksack sitzt gerade auf dem Rücken (Sockel und Versatz pro Statur prüfen).
- Schleuder-Animation: Ausholen, Kreisen, Wurf. Gebaut aus vorhandenen Wurfgesten, die Schleuder schwingt mit.
- Geschoss: kleiner Stein aus Rock_Collection oder die Steinform des Magiers, verkleinert, mit leichter Staubspur. Spezialmunition wird eingefärbt (Brand, Gift, Säure …).
- Bilder: Stand, Lauf, Schuss in Zeitlupe.

## 14. Gesamttest
- Streamtest der ganzen Strecke.
- Bilder je Zone, Leistung mit 4 Spielern.
- Lore-Bibel, Aufgaben-Doc mit Testzeilen, Commit + Push.

## Offen für Boris
- Karte: Wege ganz weg, oder später als echte, geschwungene Pfade nach dem Gelände zeichnen?
- Gegnerdichte: mein Vorschlag ist alle 40–60 m eine Gruppe. Dichter geht, kostet aber Ruhe zum Erkunden.

## Stand 8.11. abends: alle Punkte erledigt
- 0 Floromantin: Waage-Zeichen nicht mehr auf ihrem Körper (53d9bd5d)
- 6 Tagesablauf (fddadb35), 7 Pergamentkarte mit echten Wegen, Leute-Reiter (2316d18b)
- 8 Boden mit acht Schichten, Hex-Tiling, 840 Abdrücken (14e697e1), 9 Baukasten 81 Teile (4a3b8548)
- 10 Füllung 120.000 Teile in 282 Zellen, 237 Fundorte, Rauch, Nebel (9f331cb7), 11 Bodenprüfung 2.099 Teile abgesenkt (6c3fd655)
- 12 Gegner 2.073 Gruppen in Spielernähe mit Respawn und Stärke (574a3630), 13 Ingenieur Schleuderwurf, Stein, Rucksack (0a0328a9)
- 14 Streamtest 0 Fehler, 4 Spieler 71 FPS, Lore-Bibel und Aufgaben-Doc (Gruppe BP)
- Nicht gemacht: Fuß-IK für Spieler/NPCs am Hang (Extrapunkt). Kreaturen haben schon VaelLegIKComponent; für die Helden bräuchte es einen eigenen Anim-Knoten, bei flachen Wegen und Iso-Kamera bisher kaum sichtbar.
- Neu bauen: blender aschenmark_pfade.py → aschenmark_karte.py → CreateWorldMaps; aschenmark_boden.py + aschenmark_abdruecke.py → BuildAschenmarkBoden; aschenmark_kulisse.py → BuildKulisse → aschenmark_fuellung.py → FillAschenmark; aschenmark_gegner.py → PlaceGegner; FitGroundAll.
