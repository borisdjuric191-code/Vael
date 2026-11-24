# Geländeplan: das Spielgelände an den neuen Umriss anpassen

Stand 24.11.2026, erzeugt von `Tools/Welt/Umriss/gelaendeplan.ps1`. Vergleich des heutigen Geländes (`welt_klein.png`) mit Umriss v9 (`teile9.ps1`), der Vorlage der gemalten Karte. Bild: `Docs/Karte/vael_gelaendeplan.png` (Norden oben). Noch ist **nichts** am Gelände geändert.

## Bewertung (Claude, Nacht 24./25.11.)

- **Eisenschlund:** verliert mit Umriss v9 rund die Hälfte seines heutigen Geländes (3 von 6 km²), und zwar genau den Teil zum Innenmeer mit Rippenbögen, Knochenhalle, Windschlucht, Tiefstem Schacht, Rostschwamm-Höhle und Grubenlaternen; dazu Rippenlager und Schlackentor-Posten. Das ist kein Verschieben mehr, das wäre ein Neubau der Region.
- **Aschenmark:** verliert gut 40 % (1,8 km²), vor allem den Süden und Südwesten (Teich, Ascheäcker, Glutfelder-Süd) und den Nordosten zum Innenmeer (Gildenspeicher, Schlackengräber). Rund 45 % der Kulisse und viele Gegnergruppen stehen dort.
- **Sturmzinnen:** kleine Verluste im Norden (Eisfeld, Doppeluhu-Horste), dafür viel neues Land an Kiefer und Hörnern.
- **Wurzelforst:** fast nur Gewinn (neues Land im Westen und Norden), Drachensicht und Schwanzgrat rücken etwas.
- **Empfehlung:** Für die endgültige Karte den Umriss beim Eisenschlund und im Nordosten der Aschenmark wieder ans heutige Gelände anlehnen (Innenmeer dort etwas kleiner) und nur Süd-Aschenmark, Sturmzinnen-Nord, Kiefer und Schwanzgrat wirklich umbauen. Die gemalte Karte bleibt dabei fast gleich, nur das Innenmeer wird etwas schmaler. Entscheidung bei Boris.

## Flächen

| Region | bleibt Land | wird Meer | neues Land |
|---|---|---|---|
| Wurzelforst | 1,04 km² | 0,21 km² | 1,70 km² |
| Aschenmark | 2,45 km² | 1,80 km² | 0,64 km² |
| Eisenschlund | 2,99 km² | 2,96 km² | 1,09 km² |
| Sturmzinnen | 2,99 km² | 0,55 km² | 1,83 km² |

Kulissenteile der Aschenmark (`AM_Fuellung.json`) auf künftigem Meer: 53540 von 120129. Die setzt `aschenmark_fuellung.py` beim nächsten Lauf ohnehin neu.

## Betroffene Lager und Orte

Vorschlag = nächste Stelle, die heute Land ist und im neuen Umriss mindestens ~25 m von der Küste entfernt liegt (Weltmeter, X Norden, Y Osten).

| Nr | Region | Art | Name | Lage | heute (X / Y m) | Vorschlag | neu (X / Y m) |
|---|---|---|---|---|---|---|---|
| 1 | Aschenmark | Ort | Ascheäcker | wird Meer | 400 / 1 352 | 24 m nach Nordwesten | 415 / 1 333 |
| 2 | Aschenmark | Ort | Gildenspeicher | wird Meer | 1 054 / 1 871 | 25 m nach Westen | 1 053 / 1 846 |
| 3 | Aschenmark | Ort | Schlackengräber | wird Meer | 926 / 1 968 | 31 m nach Südwesten | 907 / 1 943 |
| 4 | Aschenmark | Ort | Teich | wird Meer | 439 / 765 | 52 m nach Nordosten | 470 / 806 |
| 5 | Aschenmark | Siedlung | Schlackengräber | an neuer Küste | 926 / 1 943 | 14 m nach Südwesten | 914 / 1 936 |
| 6 | Eisenschlund | Lager | Rippenlager | wird Meer | 1 836 / 4 482 | 48 m nach Nordosten | 1 871 / 4 516 |
| 7 | Eisenschlund | Lager | Schlackentor-Posten | wird Meer | 1 177 / 2 800 | 77 m nach Norden | 1 254 / 2 803 |
| 8 | Eisenschlund | Ort | Funkenerz-Adern | wird Meer | 1 136 / 5 596 | 53 m nach Nordosten | 1 185 / 5 618 |
| 9 | Eisenschlund | Ort | Geflutete Stollen | wird Meer | 818 / 4 959 | 70 m nach Nordwesten | 880 / 4 925 |
| 10 | Eisenschlund | Ort | Grubenlaternen | wird Meer | 1 648 / 3 686 | 169 m nach Westen | 1 601 / 3 524 |
| 11 | Eisenschlund | Ort | Kernerz-Stollen | wird Meer | 1 000 / 5 368 | 107 m nach Norden | 1 101 / 5 334 |
| 12 | Eisenschlund | Ort | Knochenhalle | wird Meer | 2 046 / 4 005 | 167 m nach Norden | 2 211 / 3 982 |
| 13 | Eisenschlund | Ort | Nackenwirbel | wird Meer | 2 602 / 5 323 | 95 m nach Osten | 2 613 / 5 417 |
| 14 | Eisenschlund | Ort | Rippenbögen | wird Meer | 1 850 / 4 300 | 37 m nach Südosten | 1 816 / 4 315 |
| 15 | Eisenschlund | Ort | Rippenbrücke | wird Meer | 1 836 / 4 823 | 118 m nach Norden | 1 947 / 4 862 |
| 16 | Eisenschlund | Ort | Rostschwamm-Höhle | wird Meer | 1 636 / 3 300 | 176 m nach Osten | 1 635 / 3 476 |
| 17 | Eisenschlund | Ort | Tiefster Schacht | wird Meer | 1 864 / 3 209 | 239 m nach Osten | 1 857 / 3 448 |
| 18 | Eisenschlund | Ort | Windschlucht | wird Meer | 2 109 / 4 596 | 96 m nach Nordosten | 2 197 / 4 634 |
| 19 | Sturmzinnen | Ort | Doppeluhu-Horste | wird Meer | 4 560 / 5 660 | 94 m nach Nordwesten | 4 603 / 5 576 |
| 20 | Sturmzinnen | Ort | Eisfeld | wird Meer | 4 700 / 5 110 | 80 m nach Südosten | 4 638 / 5 160 |
| 21 | Sturmzinnen | Ort | Horngletscher | an neuer Küste | 4 560 / 4 980 | 14 m nach Südosten | 4 547 / 4 987 |
| 22 | Sturmzinnen | Ort | Kieferklippen | an neuer Küste | 3 300 / 4 400 | 22 m nach Süden | 3 279 / 4 398 |
| 23 | Wurzelforst | Ort | Drachensicht | wird Meer | 3 912 / 2 155 | 65 m nach Norden | 3 972 / 2 179 |
| 24 | Wurzelforst | Ort | Schwanzgrat | an neuer Küste | 3 916 / 1 835 | 8 m nach Südosten | 3 910 / 1 839 |

## Gegner (Gruppen, zusammengefasst)

Gegnergruppen setzen `aschenmark_gegner.py` und `wf_kreaturen.py` nach Zonen neu; auf künftigem Meer fallen sie einfach weg oder rücken nach. Anzahl der Gruppen:

| Region | Art | wird Meer | an neuer Küste |
|---|---|---|---|
| Aschenmark | AshHarpy | 199 | 11 |
| Aschenmark | AshRoller | 164 | 8 |
| Aschenmark | EmberCrawler | 373 | 28 |
| Aschenmark | HarpyElder | 58 | 2 |
| Aschenmark | Rankenluchs | 1 | 0 |
| Aschenmark | Schleieraffe | 1 | 0 |
| Aschenmark | SickleDoll | 17 | 4 |
| Aschenmark | VeilMonkey | 39 | 10 |
| Wurzelforst | Rankenluchs | 8 | 2 |
| Wurzelforst | Schleieraffe | 10 | 0 |
| Wurzelforst | Spannhornkaefer | 1 | 0 |
| Wurzelforst | Tauraupe | 0 | 1 |

## Erze

| Region | Erz (Fundort) | Lage | heute (X / Y m) | Vorschlag |
|---|---|---|---|---|
| Aschenmark | Russquarz (Hang) | wird Meer | 1 078 / 2 326 | 67 m nach Norden |
| Aschenmark | Russquarz (Hang) | wird Meer | 1 077 / 2 326 | 68 m nach Norden |
| Aschenmark | Russquarz (Hang) | an neuer Küste | 1 229 / 2 322 | 30 m nach Südwesten |
| Aschenmark | Russquarz (Hang) | wird Meer | 1 073 / 1 727 | 55 m nach Südosten |
| Aschenmark | Russquarz (Hang) | an neuer Küste | 1 230 / 2 321 | 30 m nach Südwesten |
| Aschenmark | Russquarz (Hang) | wird Meer | 958 / 2 347 | 123 m nach Südwesten |
| Aschenmark | Russquarz (Hang) | wird Meer | 961 / 2 346 | 126 m nach Südwesten |
| Aschenmark | Russquarz (Hang) | wird Meer | 959 / 2 347 | 125 m nach Südwesten |
| Aschenmark | Russquarz (Hang) | wird Meer | 1 079 / 2 327 | 67 m nach Norden |
| Aschenmark | Russquarz (Hang) | wird Meer | 960 / 2 347 | 126 m nach Südwesten |
| Aschenmark | Russquarz (Hang) | wird Meer | 1 009 / 2 320 | 100 m nach Westen |
| Aschenmark | Russquarz (Hang) | wird Meer | 1 008 / 2 320 | 99 m nach Westen |
| Aschenmark | Russquarz (Hang) | wird Meer | 1 011 / 2 320 | 99 m nach Westen |
| Aschenmark | Russquarz (Hang) | wird Meer | 1 045 / 1 900 | 54 m nach Westen |
| Aschenmark | Russquarz (Hang) | wird Meer | 1 045 / 1 899 | 53 m nach Westen |
| Aschenmark | Russquarz (Hang) | an neuer Küste | 888 / 2 167 | 22 m nach Süden |
| Aschenmark | Russquarz (Hang) | wird Meer | 1 071 / 1 727 | 53 m nach Südosten |
| Aschenmark | Russquarz (Hang) | an neuer Küste | 889 / 2 169 | 24 m nach Süden |
| Aschenmark | Russquarz (Hang) | an neuer Küste | 889 / 2 168 | 23 m nach Süden |
| Aschenmark | Russquarz (Hang) | an neuer Küste | 890 / 2 166 | 24 m nach Süden |
| Aschenmark | Schlacke (Boden) | wird Meer | 1 050 / 2 119 | 55 m nach Südosten |
| Aschenmark | Schlacke (Boden) | wird Meer | 1 050 / 2 122 | 53 m nach Südosten |
| Aschenmark | Schlacke (Boden) | wird Meer | 937 / 2 004 | 54 m nach Süden |
| Aschenmark | Schlacke (Boden) | wird Meer | 1 310 / 2 256 | 70 m nach Norden |
| Aschenmark | Schlacke (Boden) | wird Meer | 964 / 2 252 | 39 m nach Westen |
| Aschenmark | Schlacke (Boden) | wird Meer | 964 / 2 255 | 41 m nach Westen |
| Aschenmark | Schlacke (Boden) | wird Meer | 1 302 / 2 127 | 88 m nach Südosten |
| Aschenmark | Schlacke (Boden) | wird Meer | 289 / 1 204 | 51 m nach Norden |
| Aschenmark | Schlacke (Boden) | wird Meer | 291 / 1 205 | 49 m nach Norden |
| Aschenmark | Schlacke (Boden) | wird Meer | 938 / 2 003 | 55 m nach Süden |
| Aschenmark | Schlacke (Boden) | wird Meer | 939 / 2 004 | 56 m nach Süden |
| Aschenmark | Schlacke (Boden) | wird Meer | 1 304 / 2 127 | 88 m nach Südosten |
| Aschenmark | Schlacke (Boden) | wird Meer | 1 064 / 2 012 | 155 m nach Südosten |
| Aschenmark | Schlacke (Boden) | wird Meer | 1 311 / 2 408 | 79 m nach Süden |
| Aschenmark | Schlacke (Boden) | wird Meer | 1 309 / 2 410 | 77 m nach Süden |
| Aschenmark | Schlacke (Boden) | wird Meer | 1 145 / 2 125 | 79 m nach Osten |
| Aschenmark | Schlacke (Boden) | wird Meer | 1 146 / 2 124 | 80 m nach Osten |
| Aschenmark | Schlacke (Boden) | wird Meer | 1 064 / 2 013 | 154 m nach Südosten |
| Aschenmark | Schlacke (Boden) | wird Meer | 998 / 1 848 | 45 m nach Nordwesten |
| Aschenmark | Schlacke (Boden) | wird Meer | 997 / 1 848 | 46 m nach Nordwesten |
| Aschenmark | Schlacke (Boden) | wird Meer | 1 248 / 1 978 | 222 m nach Osten |
| Aschenmark | Schlacke (Boden) | wird Meer | 1 218 / 2 033 | 168 m nach Osten |
| Aschenmark | Schlacke (Boden) | wird Meer | 1 216 / 2 032 | 169 m nach Osten |
| Aschenmark | Schlacke (Boden) | wird Meer | 998 / 1 849 | 46 m nach Nordwesten |
