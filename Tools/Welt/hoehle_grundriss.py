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


HOEHLEN = {"Olmgrotte": olmgrotte, "Gebeinkammer": gebeinkammer}

if __name__ == "__main__":
    for name in sys.argv[1:] or list(HOEHLEN):
        daten = HOEHLEN[name]()
        for i, z in enumerate(daten["grundriss"]):
            print("%2d %s" % (i, z))
        with open(os.path.join(ZIEL, name + ".json"), "w", encoding="utf-8") as f:
            json.dump(daten, f, ensure_ascii=False, indent=1)
        print("geschrieben:", name)
