# Plant, wo die Erze der Aschenmark wachsen, und schreibt Scripts/Data/Welt/AM_Erze.json für Scripts/PlaceErze.py.
#
#   py Tools/Welt/aschenmark_erze.py [C:/Projekte/Vael/Scripts/Data/Welt]
#
# Reines Python (ohne Blender, ohne numpy). Weltmeter wie AM_Orte.json: x Norden, y Osten.
#
# - Rußquarz: Felsiger Boden (Hänge), in Schlackenhängen, Aschenpass und am Kraterrand der Glutfelder, dazu am Obsidianbruch.
# - Schlacke-Erz: Vor allem in den Schlackenhängen und an den Schlackenhalden, spärlich in Kesselgrund und Glutfeldern.
# - Niddruse: Nur dicht an der Nid-Quelle (sie wächst nur bei offener Quelle, 7 m). PlaceErze.py setzt sie um die echte
#   Mark-Quelle im Level, hier steht nur die geplante Zahl.
# - Größe, Neigung und Spiegeln wählt AVaelHarvestable selbst je Ort (Boris' Regel: keine gleichen Stücke).
# - Erreichbar: höchstens 220 m vom nächsten Weg und höchstens 40 m über ihm, nicht oben im Gebirge.
# - Erze stehen in kleinen Vorkommen (2 bis 4 Stück dicht beieinander), zwischen den Vorkommen liegt offener Boden.
# - Frei bleiben: Übergangszone zum Wurzelforst (AM_Naht.json), Wege und Pfade, Kampfflächen, Lager, Orte, Siedlungen,
#   die Teile der Füllung (AM_Fuellung.json), das Meer.

import json
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from am_naht import in_naht  # noqa: E402

WELT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Scripts", "Data", "Welt")
SIZE_X, SIZE_Y = 2017, 2521
rng = random.Random(1311)


def lade(name):
    with open(os.path.join(WELT, name), encoding="utf-8") as f:
        return json.load(f)


with open(os.path.join(WELT, "AM_Hoehen.r16"), "rb") as f:
    _raw = f.read()
orte = lade("AM_Orte.json")
pfade = lade("AM_Pfade.json")["wege"]
siedlungen = lade("AM_Siedlungen.json")["siedlungen"]
fuellung = lade("AM_Fuellung.json")["teile"]


def _h(i, j):
    k = (j * SIZE_X + i) * 2
    return (_raw[k] | (_raw[k + 1] << 8)) / 128.0 - 256.0


def ground(x, y):
    x = min(max(x, 0.0), SIZE_X - 1.001)
    y = min(max(y, 0.0), SIZE_Y - 1.001)
    ix, iy = int(x), int(y)
    tx, ty = x - ix, y - iy
    return (_h(ix, iy) * (1 - tx) + _h(ix + 1, iy) * tx) * (1 - ty) + (_h(ix, iy + 1) * (1 - tx) + _h(ix + 1, iy + 1) * tx) * ty


def slope(x, y):
    return max(abs(ground(x + 2, y) - ground(x - 2, y)), abs(ground(x, y + 2) - ground(x, y - 2))) / 4.0


# ---------------------------------------------------------------- was frei bleibt

CELL = 20.0


class Gitter:
    """Punkte mit Halbmesser, schnell nach Nähe abgefragt"""

    def __init__(self):
        self.zellen = {}

    def add(self, x, y, r):
        self.zellen.setdefault((int(x // CELL), int(y // CELL)), []).append((x, y, r))

    def naechster(self, x, y, reichweite=1):
        best = 1e9
        cx, cy = int(x // CELL), int(y // CELL)
        for dx in range(-reichweite, reichweite + 1):
            for dy in range(-reichweite, reichweite + 1):
                for px, py, r in self.zellen.get((cx + dx, cy + dy), []):
                    best = min(best, math.hypot(px - x, py - y) - r)
        return best


wege = Gitter()
BREITE = {"haupt": 4.5, "neben": 3.5, "pfad": 2.5}
for weg in pfade:
    w = BREITE.get(weg["kind"], 3.0)
    for a, b in zip(weg["points"], weg["points"][1:]):
        n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / 2.0))
        for k in range(n + 1):
            t = k / n
            wege.add(a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, w)

# Grobe Wegpunkte (alle 10 m) mit Bodenhöhe: Erze sollen vom Weg aus erreichbar und zu finden sein
wegpunkte = []
for weg in pfade:
    for a, b in zip(weg["points"], weg["points"][1:]):
        n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / 10.0))
        for k in range(n):
            t = k / n
            wegpunkte.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))


def erreichbar(x, y, weit=220.0, hoch=40.0):
    """Nicht weiter als `weit` m vom nächsten Weg und nicht mehr als `hoch` m über ihm (sonst oben im Gebirge)"""
    d, wx, wy = min((math.hypot(px - x, py - y), px, py) for px, py in wegpunkte)
    return d <= weit and ground(x, y) - ground(wx, wy) <= hoch


teile = Gitter()
for t in fuellung:
    teile.add(t[1], t[2], 1.2 * max(1.0, t[8]))

reserviert = [(c["x"], c["y"], 26.0) for c in orte["clearings"]]
reserviert += [(c["x"], c["y"], 40.0) for c in orte["camps"].values()]
reserviert += [(p["x"], p["y"], 32.0) for p in orte["places"].values()]
krater = orte["places"]["Krater der Glutkönigin"]
reserviert += [(krater["x"], krater["y"], 60.0)]          # Bosskampf: Arena frei, die Erze sitzen am Kraterrand
for s in siedlungen:
    if s["typ"] != "unterwegs":
        reserviert.append((s["mitte"]["x"], s["mitte"]["y"], s["radius"] + 12.0))

ZONEN = {name: (p["x"], p["y"]) for name, p in orte["zones"].items()}


def zone_of(x, y):
    return min(ZONEN, key=lambda n: math.hypot(x - ZONEN[n][0], y - ZONEN[n][1]))


def frei(x, y, weg=6.0):
    if not (25 < x < SIZE_X - 40 and 25 < y < SIZE_Y - 40):
        return False
    if in_naht(x, y, rand=10.0):
        return False
    if ground(x, y) < 2.0 or slope(x, y) > 1.1:
        return False
    if wege.naechster(x, y) < weg or teile.naechster(x, y) < 0.6:
        return False
    return not any(math.hypot(x - cx, y - cy) < r for cx, cy, r in reserviert)


# ---------------------------------------------------------------- Vorkommen

vorkommen = []
alle = Gitter()


def setze_vorkommen(art, x, y, stueck, zone, grund):
    """Ein Vorkommen: das erste Stück am Punkt, die übrigen dicht drum herum (1,2 bis 3 m)"""
    stuecke = [(x, y)]
    versuche = 0
    while len(stuecke) < stueck and versuche < 40:
        versuche += 1
        a, d = rng.uniform(0, 2 * math.pi), rng.uniform(1.2, 3.0)
        px, py = x + math.cos(a) * d, y + math.sin(a) * d
        if frei(px, py, 4.5) and all(math.hypot(px - qx, py - qy) > 1.0 for qx, qy in stuecke):
            stuecke.append((px, py))
    for px, py in stuecke:
        alle.add(px, py, 0.0)
    vorkommen.append({"art": art, "zone": zone, "grund": grund, "stuecke": [
        {"x": round(px, 2), "y": round(py, 2), "z": round(ground(px, py), 2), "yaw": round(rng.uniform(0, 360), 1)}
        for px, py in stuecke]})


def streue(art, zonen_anteil, ziel, abstand, bedingung, stueck=(2, 4), grund="Hang"):
    """Verteilt `ziel` Vorkommen auf die Zonen (Anteile), mit Mindestabstand zu allen anderen Erzen"""
    gesetzt = {}
    for zone, anteil in zonen_anteil.items():
        soll = max(1, round(ziel * anteil))
        n, versuche = 0, 0
        while n < soll and versuche < 20000:
            versuche += 1
            x, y = rng.uniform(25, SIZE_X - 40), rng.uniform(25, SIZE_Y - 40)
            if zone_of(x, y) != zone or not bedingung(x, y) or not frei(x, y):
                continue
            if alle.naechster(x, y, reichweite=int(abstand // CELL) + 1) < abstand:
                continue
            setze_vorkommen(art, x, y, rng.randint(*stueck), zone, grund)
            n += 1
        gesetzt[zone] = n
    return gesetzt


def felsig(x, y):
    return 0.25 < slope(x, y) < 0.75 and erreichbar(x, y)


def eben_bis_hang(x, y):
    return slope(x, y) < 0.6 and erreichbar(x, y)


bericht = {}

# Obsidianbruch und Schlackenhalden: feste Fundstellen am Rand der Orte (außerhalb ihrer 32 m)
def am_ort(art, ort, anzahl, stueck, grund):
    o = orte["places"][ort]
    n, versuche = 0, 0
    while n < anzahl and versuche < 3000:
        versuche += 1
        a, d = rng.uniform(0, 2 * math.pi), rng.uniform(34, 55)
        x, y = o["x"] + math.cos(a) * d, o["y"] + math.sin(a) * d
        if frei(x, y) and alle.naechster(x, y, 2) > 12.0:
            setze_vorkommen(art, x, y, rng.randint(*stueck), zone_of(x, y), grund)
            n += 1
    return n


bericht["Russquarz am Obsidianbruch"] = am_ort("Russquarz", "Obsidianbruch", 3, (2, 4), "Obsidianbruch")
bericht["Schlacke an den Schlackenhalden"] = am_ort("Schlacke", "Schlackenhalden", 4, (2, 3), "Schlackenhalden")

# Kraterrand der Glutfelder: Rußquarz in der Kraterwand (Ring 62 bis 95 m um die Mitte)
n, versuche = 0, 0
while n < 4 and versuche < 4000:
    versuche += 1
    a, d = rng.uniform(0, 2 * math.pi), rng.uniform(62, 95)
    x, y = krater["x"] + math.cos(a) * d, krater["y"] + math.sin(a) * d
    if frei(x, y) and alle.naechster(x, y, 3) > 30.0:
        setze_vorkommen("Russquarz", x, y, rng.randint(2, 3), "Glutfelder", "Kraterrand")
        n += 1
bericht["Russquarz am Kraterrand"] = n

bericht["Russquarz verstreut"] = streue("Russquarz", {"Schlackenhänge": 0.45, "Aschenpass": 0.35, "Glutfelder": 0.2}, 22, 55.0, felsig)
bericht["Schlacke verstreut"] = streue("Schlacke", {"Schlackenhänge": 0.6, "Kesselgrund": 0.2, "Glutfelder": 0.2}, 22, 50.0, eben_bis_hang, stueck=(1, 3), grund="Boden")

NIDDRUSEN = 5                      # um die Mark-Quelle, 2,5 bis 6 m (Wuchsradius 7 m)

stuecke = sum(len(v["stuecke"]) for v in vorkommen)
out = {
    "info": "Erze der Aschenmark (Tools/Welt/aschenmark_erze.py) für Scripts/PlaceErze.py. Weltmeter, x Norden, y Osten, z Boden. "
            "art = Name von DA_Harvest_<art> (Schlacke = Schlacke-Erz). Niddrusen setzt PlaceErze.py um die echte Mark-Quelle.",
    "niddrusen": {"anzahl": NIDDRUSEN, "abstand_min": 2.5, "abstand_max": 6.0},
    "vorkommen": vorkommen,
}
with open(os.path.join(WELT, "AM_Erze.json"), "w", encoding="utf-8") as f:
    json.dump(out, f, ensure_ascii=False, indent=1)

for k, v in bericht.items():
    print("[aschenmark_erze]", k, v)
nach_art = {}
for v in vorkommen:
    nach_art.setdefault(v["art"], [0, 0])
    nach_art[v["art"]][0] += 1
    nach_art[v["art"]][1] += len(v["stuecke"])
print("[aschenmark_erze] Vorkommen/Stücke je Art:", nach_art, "gesamt Stücke:", stuecke, "+", NIDDRUSEN, "Niddrusen")
print("[aschenmark_erze] in der Naht:", sum(1 for v in vorkommen for s in v["stuecke"] if in_naht(s["x"], s["y"])))
