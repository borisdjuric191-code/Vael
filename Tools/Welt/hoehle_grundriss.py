"""
Zeichnet die Grundrisse der natürlichen Höhlen des Eisenschlunds (Docs/Eisenschlund_Hoehlen.md) und schreibt sie nach
Scripts/Data/Hoehlen/<Name>.json. Die Gildenmine ist von Hand gezeichnet; natürliche Höhlen entstehen hier aus runden Kammern und
gewundenen Gängen mit etwas Rauschen, damit nichts nach Raster aussieht. Danach kann man den Grundriss in der JSON von Hand ändern.

  py Tools/Welt/hoehle_grundriss.py Olmgrotte
"""
import json
import math
import os
import random
import sys

ZIEL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Scripts", "Data", "Hoehlen")


class Raster:
    def __init__(self, w, h, seed):
        self.w, self.h = w, h
        self.g = [["#"] * w for _ in range(h)]
        self.rng = random.Random(seed)

    def inside(self, r, c):
        return 1 <= r < self.h - 1 and 1 <= c < self.w - 1

    def put(self, r, c, ch, nur_auf=None):
        if self.inside(r, c) and (nur_auf is None or self.g[r][c] in nur_auf):
            self.g[r][c] = ch

    def kammer(self, r0, c0, rr, rc, ch=".", rauh=0.25):
        """Runde Kammer mit unregelmäßigem Rand"""
        for r in range(int(r0 - rr - 2), int(r0 + rr + 3)):
            for c in range(int(c0 - rc - 2), int(c0 + rc + 3)):
                a = math.atan2(r - r0, c - c0)
                rand = 1.0 + rauh * (math.sin(a * 3 + r0) * 0.6 + math.sin(a * 5 + c0) * 0.4)
                if ((r - r0) / rr) ** 2 + ((c - c0) / rc) ** 2 <= rand * rand:
                    self.put(r, c, ch)

    def gang(self, punkte, breite=1.2, ch="."):
        """Gewundener Gang durch die Punkte (Zeile, Spalte)"""
        for (r0, c0), (r1, c1) in zip(punkte, punkte[1:]):
            n = int(max(abs(r1 - r0), abs(c1 - c0)) * 2) + 1
            for k in range(n + 1):
                t = k / n
                r = r0 + (r1 - r0) * t + math.sin(t * 6.0 + r0) * 0.8
                c = c0 + (c1 - c0) * t + math.cos(t * 5.0 + c0) * 0.8
                b = breite + self.rng.uniform(-0.2, 0.4)
                for dr in range(-2, 3):
                    for dc in range(-2, 3):
                        if dr * dr + dc * dc <= b * b:
                            self.put(int(round(r + dr)), int(round(c + dc)), ch)

    def streuen(self, ch, anzahl, auf=".", abstand=2):
        gesetzt = []
        versuche = 0
        while len(gesetzt) < anzahl and versuche < 5000:
            versuche += 1
            r, c = self.rng.randrange(self.h), self.rng.randrange(self.w)
            if self.g[r][c] in auf and all(abs(r - a) + abs(c - b) >= abstand for a, b in gesetzt):
                self.g[r][c] = ch
                gesetzt.append((r, c))
        return gesetzt

    def zeilen(self):
        return ["".join(z) for z in self.g]


def olmgrotte():
    """Die gefluteten Stollen: ein Bach zieht sich von der Eingangsgrotte durch die Höhle zum großen See in der Mitte; daneben
    die Kristallgrotte (Salzspat als Lampen), ganz hinten das Gelege der Drachenbrut am tiefen Wasser"""
    R = Raster(40, 44, 1959)
    R.kammer(38, 20, 4, 5)                                   # Eingangsgrotte
    R.put(42, 20, "E"); R.put(41, 20, "."); R.put(42, 21, ".")
    R.gang([(36, 20), (30, 16), (24, 18)], 1.6)               # Gang nach Norden
    R.kammer(18, 19, 8, 11)                                   # der große See
    R.kammer(18, 19, 3.2, 5.0, "O", 0.3)                      # tiefes Wasser in der Mitte
    R.kammer(17, 25, 1.3, 1.5, ".", 0.2)                      # Insel im See
    R.gang([(14, 9), (10, 5), (7, 6)], 1.4)                   # nach Nordwesten
    R.kammer(6, 8, 4, 6)                                      # Kristallgrotte
    R.gang([(12, 28), (8, 32), (6, 33)], 1.3)                 # nach Nordosten
    R.kammer(5, 33, 4, 5)                                     # Gelege der Drachenbrut
    R.kammer(3, 34, 1.6, 2.4, "O", 0.2)                       # sein tiefes Becken
    R.gang([(30, 24), (28, 31), (22, 34)], 1.2)               # Nebengang im Osten, zurück zum See
    R.kammer(29, 32, 3, 3.5)
    # der Bach: flaches Wasser vom See bis in die Eingangsgrotte, und am Seeufer
    R.gang([(24, 18), (30, 16), (35, 19), (37, 22)], 0.7, "~")
    for r in range(R.h):
        for c in range(R.w):
            if R.g[r][c] == "." and any(R.g[r + dr][c + dc] == "O" for dr in (-1, 0, 1) for dc in (-1, 0, 1)
                                         if 0 <= r + dr < R.h and 0 <= c + dc < R.w):
                R.g[r][c] = "~"
    # Licht, Tropfsteine, Erz, Gegner
    for r, c in [(6, 5), (5, 9), (8, 11), (4, 7), (7, 3), (9, 8)]:
        R.put(r, c, "C", ".")                                 # Salzspat-Lampen in der Kristallgrotte
    for r, c in [(37, 17), (12, 14), (22, 12), (23, 26), (13, 25), (30, 30), (5, 30), (28, 17)]:
        R.put(r, c, "C", ".~")                                # einzelne Lampen am Weg
    R.streuen("M", 26, ".", 3)                                # Tropfsteingruppen
    for r, c in [(5, 7), (8, 6), (3, 10), (16, 13), (28, 33)]:
        R.put(r, c, "Z", ".")                                 # Salzspat-Erz
    for r, c in [(6, 31), (4, 36), (7, 35), (14, 22), (21, 25), (29, 31)]:
        R.put(r, c, "G", ".~")                                # Drachenbrut
    R.put(18, 12, "S", ".~")                                  # Lichtschacht über dem See
    return {
        "name": "Olmgrotte",
        "titel": "Geflutete Stollen",
        "art": "Olmgrotte",
        "zelle_m": 4.0,
        "welt_eingang_m": [818.2, 4959.1, 22.63],
        "welt_eingang_hinweis": "x, y, Bodenhöhe in Metern: Geflutete Stollen aus Scripts/Data/Welt/ES_Orte.json; das Portal liegt davor, der Weg hinein nach -X",
        "legende": {"#": "Fels", ".": "Boden", "E": "Ausgang in die Welt", "~": "flaches Wasser (knietief, Wasserquelle)",
                    "O": "tiefes Wasser (nicht begehbar)", "C": "Salzspat-Lampe (Kristall, der leuchtet)", "M": "Tropfsteingruppe",
                    "S": "Lichtschacht", "Z": "Salzspat-Erz (Platzhalter)", "G": "Platz für Gegner (Drachenbrut, später)"},
        "grundriss": R.zeilen(),
    }


def gebeinkammer():
    """Die Gebeinkammer (Vorlage: Katakomben von Paris): gerade, schmale Gänge im Raster, an den Wänden die Schädelreihen der
    Gilde, zwei Beinhauskammern; im Norden die bunte Knochenhöhle, in der der Grabschleim in vielen Farben leuchtet"""
    R = Raster(40, 44, 1786)

    def rect(r0, c0, r1, c1, ch="."):
        for r in range(r0, r1 + 1):
            for c in range(c0, c1 + 1):
                R.put(r, c, ch)
    rect(37, 15, 41, 24)                                     # Eingangshalle
    R.put(42, 19, "E"); R.put(42, 20, ".")
    rect(14, 18, 36, 19)                                     # Hauptgang nach Norden
    rect(30, 5, 31, 34)                                      # Quergänge
    rect(22, 5, 23, 34)
    rect(14, 5, 31, 6)                                       # Seitengänge
    rect(14, 33, 31, 34)
    rect(25, 9, 28, 14)                                      # Beinhauskammern
    rect(25, 25, 28, 30)
    rect(33, 26, 35, 31); rect(32, 28, 32, 29)
    rect(26, 7, 27, 8); rect(26, 15, 27, 17)                 # Türen der Beinhauskammern
    rect(26, 20, 27, 24); rect(26, 31, 27, 32)
    R.kammer(8, 20, 6, 13, ".", 0.3)                         # die Knochenhöhle
    rect(13, 5, 14, 9); rect(12, 8, 13, 10)                  # Zugänge zur Höhle
    rect(13, 30, 14, 34); rect(12, 29, 13, 31)
    for r, c, ch in [(39, 16, "B"), (39, 23, "B"), (38, 19, "L"), (31, 18, "L"), (23, 19, "L"), (30, 6, "L"), (22, 33, "L"),
                     (26, 10, "B"), (27, 13, "B"), (26, 27, "B"), (28, 29, "B"), (34, 30, "B"),
                     (27, 6, "P"), (18, 34, "P"), (30, 24, "P"),
                     (24, 12, "G"), (26, 28, "G"), (17, 6, "G"), (33, 28, "G")]:
        R.put(r, c, ch, ".")
    # in der Knochenhöhle: Schleimlachen in vielen Farben, Knochenhaufen, das Nest des Mutterschleims
    for r, c in [(5, 12), (4, 20), (6, 27), (9, 9), (10, 16), (8, 23), (11, 29), (3, 15), (12, 21), (7, 32), (10, 11)]:
        R.put(r, c, "P", ".")
    for r, c in [(6, 16), (9, 26), (4, 24), (11, 14), (7, 10), (12, 25), (3, 19)]:
        R.put(r, c, "B", ".")
    for r, c in [(7, 20), (9, 13), (5, 28)]:
        R.put(r, c, "G", ".")
    return {
        "name": "Gebeinkammer",
        "titel": "Gebeinkammer",
        "art": "Gebeinkammer",
        "zelle_m": 4.0,
        "welt_eingang_m": [2045.5, 4004.5, 20.01],
        "welt_eingang_hinweis": "x, y, Bodenhöhe in Metern: die Knochenhalle aus Scripts/Data/Welt/ES_Orte.json",
        "legende": {"#": "Fels (in den Gängen mit Schädelreihen davor)", ".": "Boden", "E": "Ausgang in die Welt", "B": "Knochenhaufen",
                    "P": "Lache Grabschleim (Farbe wechselt: grün, violett, rot, bernstein, türkis)", "L": "Grubenlaterne der Gilde",
                    "G": "Platz für Gegner (Grabschleim, Madenwirt, Knochenwesen, später)"},
        "grundriss": R.zeilen(),
    }


def _rect(R, r0, c0, r1, c1, ch="."):
    for r in range(r0, r1 + 1):
        for c in range(c0, c1 + 1):
            R.put(r, c, ch)


def _eingang(R, r, c):
    """Eingangsgrotte unten in der Mitte, der Ausgang in der untersten Zeile"""
    R.kammer(r - 3, c, 3, 4)
    R.put(R.h - 2, c, "E"); R.put(R.h - 3, c, "."); R.put(R.h - 2, c + 1, ".")


def funkenerz():
    """Funkenerz-Adern: schmale, gewundene Spalten im schwarzen Fels, am Boden glühende Adern, Feuerquellen in den Weitungen"""
    R = Raster(36, 44, 1136)
    _eingang(R, 41, 18)
    R.gang([(37, 18), (31, 13), (25, 16), (19, 11), (12, 14), (6, 10)], 0.9)
    R.gang([(25, 16), (21, 23), (15, 27), (8, 25), (4, 19)], 0.9)
    R.gang([(31, 13), (30, 25), (34, 29)], 0.8)
    for r, c, rr, rc in [(25, 16, 2.5, 3), (12, 14, 2.5, 3.5), (15, 27, 3, 3), (5, 15, 3, 6), (34, 29, 2.5, 3)]:
        R.kammer(r, c, rr, rc)
    R.streuen("P", 16, ".", 3)                               # glühende Adern
    R.streuen("F", 7, ".", 5)                                # Feuerquellen
    R.streuen("Z", 9, ".", 4)                                # Funkenerz
    R.streuen("G", 5, ".", 6)
    return R, {"name": "FunkenerzAdern", "titel": "Funkenerz-Adern", "art": "FunkenerzAdern", "welt_eingang_m": [1136.4, 5595.5, 20.75],
               "portal_bauen": True,
               "legende": {"P": "glühende Ader (Glutlache)", "F": "Feuerquelle", "Z": "Funkenerz (Platzhalter)", "G": "Platz für Gegner"}}


def ameisenbau():
    """Ameisenbau: runde, gefressene Kammern an gewundenen Gängen, Kristallreste in den Wänden, Brutkammern; ganz unten die
    große Kammer der Erzmutter"""
    R = Raster(44, 46, 77)
    _eingang(R, 43, 22)
    kammern = [(36, 22, 3, 4), (30, 12, 3.5, 4), (29, 32, 3.5, 4.5), (21, 21, 4, 5), (14, 9, 3, 4), (13, 34, 3, 4), (7, 22, 5, 9)]
    for r, c, rr, rc in kammern:
        R.kammer(r, c, rr, rc)
    for a, b in [(0, 1), (0, 2), (1, 3), (2, 3), (1, 4), (2, 5), (3, 6), (4, 6), (5, 6)]:
        (r0, c0, _, _), (r1, c1, _, _) = kammern[a], kammern[b]
        R.gang([(r0, c0), ((r0 + r1) / 2 + 1, (c0 + c1) / 2 - 2), (r1, c1)], 1.1)
    R.gang([(40, 22), (36, 22)], 1.1)
    R.streuen("C", 22, ".", 3)                               # Kristallreste
    R.streuen("Z", 10, ".", 3)
    for r, c in [(30, 10), (31, 14), (29, 30), (28, 34), (14, 8), (13, 35)]:
        R.put(r, c, "N", ".")                                # Brut
    for r, c in [(7, 22), (21, 21), (30, 12), (29, 32), (14, 9), (36, 22)]:
        R.put(r + 1, c + 1, "G", ".")
    return R, {"name": "Ameisenbau", "titel": "Ameisenbau", "art": "Ameisenbau", "welt_eingang_m": [954.5, 4413.6, 18.51], "portal_bauen": True,
               "legende": {"C": "Kristallreste in vielen Farben", "Z": "Erz (Platzhalter)", "N": "Brut der Schürfameisen",
                           "G": "Platz für Gegner (Schürfameisen, Ahnenpanzer; Erzmutter in der großen Kammer)"}}


def kobold():
    """Kobold-Stollen: ein Irrgarten aus engen Gängen mit Sackgassen und falschem Licht; ganz hinten der Hort"""
    R = Raster(41, 45, 4242)
    # Irrgarten auf einem Raster von 2 Zellen (Gang, Wand), Tiefensuche von unten
    zellen_r, zellen_c = range(5, 38, 2), range(3, 38, 2)
    besucht = set()
    stapel = [(37, 19)]
    besucht.add((37, 19))
    R.put(37, 19, ".")
    while stapel:
        r, c = stapel[-1]
        nachbarn = [(r + dr, c + dc) for dr, dc in ((-2, 0), (2, 0), (0, -2), (0, 2))
                    if (r + dr) in zellen_r and (c + dc) in zellen_c and (r + dr, c + dc) not in besucht]
        if not nachbarn:
            stapel.pop()
            continue
        nr, nc = R.rng.choice(nachbarn)
        R.put((r + nr) // 2, (c + nc) // 2, ".")
        R.put(nr, nc, ".")
        besucht.add((nr, nc))
        stapel.append((nr, nc))
    # ein paar Schleifen, damit es nicht nur einen Weg gibt
    for k in range(18):
        r, c = R.rng.randrange(6, 37), R.rng.randrange(4, 37)
        if R.g[r][c] == "#" and ((R.g[r - 1][c] == "." and R.g[r + 1][c] == ".") or (R.g[r][c - 1] == "." and R.g[r][c + 1] == ".")):
            R.put(r, c, ".")
    _rect(R, 38, 17, 42, 22)                                  # Eingang
    R.put(43, 19, "E")
    R.kammer(3, 20, 2, 6)                                    # der Hort im Norden
    R.put(5, 19, "."); R.put(5, 21, ".")
    for r, c in [(2, 16), (2, 23), (3, 25), (3, 15), (2, 20)]:
        R.put(r, c, "Z", ".")
    for r, c in [(4, 17), (4, 23), (3, 21)]:
        R.put(r, c, "K", ".")
    R.streuen("L", 12, ".", 5)                               # falsches Licht
    R.streuen("T", 5, ".", 6)                                # eingestürzt
    R.streuen("G", 7, ".", 6)
    return R, {"name": "KoboldStollen", "titel": "Kobold-Stollen", "art": "KoboldStollen", "welt_eingang_m": [1647.7, 3686.4, 18.54],
               "portal_bauen": True,
               "legende": {"L": "Laterne der Kobolde (falsches, grünes Licht)", "T": "Geröll", "Z": "Kobaltblüte (Hort)", "K": "Beute der Kobolde",
                           "G": "Platz für Gegner (Kobolde)"}}


def rostschwamm():
    """Rostschwamm-Höhle: weite, niedrige Grotten voller Schwammwucherungen, Giftlachen, kränkliches grünes Licht"""
    R = Raster(42, 42, 1636)
    _eingang(R, 39, 20)
    R.gang([(35, 20), (28, 14), (20, 17), (12, 12), (6, 18)], 1.6)
    R.gang([(28, 14), (24, 27), (14, 30), (6, 18)], 1.5)
    for r, c, rr, rc in [(27, 15, 4, 5), (19, 20, 4, 6), (12, 12, 3.5, 5), (14, 30, 4, 5), (6, 19, 4, 8), (24, 28, 3, 4)]:
        R.kammer(r, c, rr, rc)
    R.streuen("M", 34, ".", 2)                               # Rostschwamm
    R.streuen("P", 12, ".", 4)                               # Giftlachen
    R.streuen("Z", 9, ".", 4)                                # Siechglas, Grünspan
    R.streuen("G", 5, ".", 6)
    return R, {"name": "Rostschwamm", "titel": "Rostschwamm-Höhle", "art": "Rostschwamm", "welt_eingang_m": [1636.4, 3300.0, 65.29],
               "legende": {"M": "Rostschwamm", "P": "Giftlache", "Z": "Siechglas / Grünspan (Platzhalter)", "G": "Platz für Gegner"}}


def schacht():
    """Tiefster Schacht: ein Ringweg um einen bodenlosen Abgrund, über den sich die Rippen der Urdrachin spannen; im Fels
    der Rippenspat. Hier lauert das Sternmaul"""
    R = Raster(44, 44, 1863)
    _eingang(R, 41, 21)
    R.gang([(37, 21), (31, 21)], 1.2)
    R.kammer(20, 21, 13, 13, ".", 0.12)                      # der Schacht
    R.kammer(20, 21, 8.5, 8.5, "X", 0.15)                    # der Abgrund
    R.kammer(7, 21, 3, 5)                                    # Nische des Sternmauls hinten
    for r, c in [(20, 9), (20, 33), (9, 21), (31, 21), (12, 12), (12, 30), (28, 12), (28, 30)]:
        R.put(r, c, "R", ".")                                # Rippen
    for r, c in [(19, 8), (21, 34), (10, 13), (30, 29), (27, 11), (11, 29)]:
        R.put(r, c, "Z", ".")                                # Rippenspat
    for r, c in [(16, 8), (25, 34), (8, 17), (32, 25)]:
        R.put(r, c, "C", ".")
    for r, c in [(7, 21), (24, 9), (14, 33)]:
        R.put(r, c, "G", ".")
    return R, {"name": "TiefsterSchacht", "titel": "Tiefster Schacht", "art": "TiefsterSchacht", "welt_eingang_m": [1863.6, 3209.1, 28.43],
               "portal_bauen": True,
               "legende": {"X": "Abgrund (nicht begehbar)", "R": "Rippe der Urdrachin", "Z": "Rippenspat (Platzhalter)", "C": "Kristall-Licht",
                           "G": "Platz für Gegner (Sternmaul)"}}


def _daten(fabrik):
    R, d = fabrik()
    d.setdefault("zelle_m", 4.0)
    d["legende"] = dict({"#": "Fels", ".": "Boden", "E": "Ausgang in die Welt"}, **d["legende"])
    d["grundriss"] = R.zeilen()
    return d


HOEHLEN = {"Olmgrotte": olmgrotte, "Gebeinkammer": gebeinkammer,
           "FunkenerzAdern": lambda: _daten(funkenerz), "Ameisenbau": lambda: _daten(ameisenbau), "KoboldStollen": lambda: _daten(kobold),
           "Rostschwamm": lambda: _daten(rostschwamm), "TiefsterSchacht": lambda: _daten(schacht)}

if __name__ == "__main__":
    for name in sys.argv[1:] or list(HOEHLEN):
        daten = HOEHLEN[name]()
        for i, z in enumerate(daten["grundriss"]):
            print("%2d %s" % (i, z))
        with open(os.path.join(ZIEL, name + ".json"), "w", encoding="utf-8") as f:
            json.dump(daten, f, ensure_ascii=False, indent=1)
        print("geschrieben:", name)
