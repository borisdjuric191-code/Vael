# Offene Welt L_Vael · Übergabe (Stand 13. November 2026)

Die Welt besteht aus vier Regionen im Ring um das Herz, offen verbunden in **L_Vael**. Übersichtskarte: `Docs/Karte/vael_regionen.png` (auch in der Lore-Bibel, Reiter „Weltkarte“). Vorgehen von Boris: erst alle Karten grob, Details (Dörfer, Häuschen, NPC-Plätze, Nähte) später mit Liebe.

## Regionen und wo sie herkommen

| Region | Gelände (Daten) | Bau in L_Vael | Tag (ein neuer Lauf ersetzt nur das) |
| --- | --- | --- | --- |
| I Aschenmark | `Tools/Welt/aschenmark_welt.py` (Blender) | `Scripts/BuildVaelWelt.py` + `PortAschenmarkToWelt.py`, `AusbauAschenmark.py`, `FillAschenmark.py`, `PlaceErze.py` | verschieden, siehe Skriptköpfe |
| II Wurzelforst | Werkstatt `L_Wurzelforst_Blockout`; `Tools/Welt/wurzelforst_welt.py` | `Scripts/PortWurzelforstToWelt.py` (kopiert den Blockout gedreht hinein) | `VaelWurzelforst` |
| III Sturmzinnen | `Tools/Welt/sturmzinnen_welt.py` | `Scripts/BuildSturmzinnen.py` | `VaelSturmzinnen` |
| IV Eisenschlund | `Tools/Welt/eisenschlund_welt.py`, Felsteile `Tools/Welt/eisenschlund_fels.py` (Blender) | `Scripts/BuildEisenschlund.py`, dann `Scripts/BuildEisenschlundSchluchten.py` | `VaelEisenschlund`, `VaelSchluchten` |

Danach immer: `py Tools/Welt/vael_karte.py` + `Scripts/CreateWorldMaps.py` (Minikarte/Weltkarte) und für die Übersicht `py Tools/Welt/vael_regionen_karte.py`.

**Wichtig beim Ausfüllen:** Dinge, die Skripte setzen, verschwinden beim nächsten Lauf des jeweiligen Skripts. Neues von Hand im Editor oder mit eigenem Tag/Ordner setzen, nicht in die Ordner der Skripte. Im Wurzelforst wird im Blockout gearbeitet und danach `PortWurzelforstToWelt.py` laufen gelassen.

## Wichtige Stellen (Weltmeter: X Norden, Y Osten)

- Übergänge: Aschenpass (2016 / 965), Seilbrücke über die Lücke (3802 / 2732), Nackenstieg (2902 / 5830), Schlackentor-Stollen (Aschenmark 1131 / 2288 ⇄ Eisenschlund 1150 / 2690, `AVaelPassage`).
- Testen: `VaelTeleport <X> <Y>` (Meter), `VaelWeather Schnee|Blizzard|...`, Lauftest `-VaelStreamTest` (Route 1–27, 100+, 200+, 300+).
- Das Spiel startet man für Tests mit `UnrealEditor.exe ... -game`, nicht mit `UnrealEditor-Cmd.exe -game` (das startet nicht).

## Offen (für den Feinschliff)

- Sturmzinnen und Eisenschlund: keine Gegner, Orte sind graue Platzhalter (Akademie, Grubenstadt, Schmelzen ...), Nid-Quellen nur Scheiben.
- Eisenschlund-Schluchten: erster Durchgang; Wände zu glatt, Licht/Stimmung noch nicht wie die Diablo-Höhlen-Inspiration. Große Höhlen/Minen sollen eigene Innenlevel werden (wie die Glutkammer). Wetter-Vorschlag Staub/Glutregen/Rostregen/Beben nicht gebaut.
- Nähte: Bodentextur-Kante am Aschenpass, Küstensprung (siehe `Scripts/Data/Welt/AM_Naht.json`).
- Das Herz (Region V) in der Mitte ist noch nicht entworfen.
