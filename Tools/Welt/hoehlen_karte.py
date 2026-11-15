"""
Zeichnet die Karte einer Höhle des Eisenschlunds aus ihrem Grundriss (Scripts/Data/Hoehlen/<Name>.json) für Minikarte und große
Karte: Fels dunkel, Boden wie altes Papier, Wasser blau, Abgrund schwarz-violett, weiche Ränder. Schreibt
Scripts/Data/Hoehlen/<Name>_Karte.png und trägt in die JSON ein, welches Stück Welt das Bild zeigt ("karte"). Scripts/BuildHoehle.py
legt daraus /Game/Vael/UI/Karten/DA_Karte_<Name> an (die Minikarte findet sie über den Namen des Levels).

  py Tools/Welt/hoehlen_karte.py [Name ...]
"""
import json
import math
import os
import struct
import sys
import zlib

DATEN = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Scripts", "Data", "Hoehlen")
PX = 8                 # Bildpunkte je Zelle
GROESSE = 512          # das Bild ist quadratisch
RAND = 2               # Zellen Rand um die Höhle

FARBE = {"#": (38, 31, 26), ".": (176, 152, 112), "~": (92, 128, 140), "O": (46, 72, 92), "X": (16, 10, 26)}


def png(pfad, breite, hoehe, zeilen):
    def chunk(art, daten):
        return struct.pack(">I", len(daten)) + art + daten + struct.pack(">I", zlib.crc32(art + daten) & 0xFFFFFFFF)
    roh = b"".join(b"\x00" + bytes(z) for z in zeilen)
    with open(pfad, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", breite, hoehe, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(roh, 6)) + chunk(b"IEND", b""))


def rauschen(x, y):
    """Ein wenig Körnung wie Papier, ohne Bibliotheken"""
    v = math.sin(x * 12.9898 + y * 78.233) * 43758.5453
    return v - math.floor(v)


def karte(name):
    pfad = os.path.join(DATEN, name + ".json")
    d = json.load(open(pfad, encoding="utf-8"))
    grid = d["grundriss"]
    H, W = len(grid), max(len(z) for z in grid)
    S = d.get("zelle_m", 4.0) * 100.0

    def zelle(r, c):
        if 0 <= r < H and 0 <= c < len(grid[r]):
            ch = grid[r][c]
            return ch if ch in FARBE else "."
        return "#"

    # Bild: u (Spalte) entlang Welt X, v (Zeile) entlang Welt Y; Zelle (r, c) liegt bei X = (H-1-r)*S, Y = c*S
    welt_min = (-(RAND + 0.5) * S, -(RAND + 0.5) * S)
    welt_groesse = GROESSE / PX * S
    zeilen = []
    for v in range(GROESSE):
        zeile = bytearray()
        for u in range(GROESSE):
            wx = welt_min[0] + (u + 0.5) / PX * S
            wy = welt_min[1] + (v + 0.5) / PX * S
            r = H - 1 - int(math.floor(wx / S + 0.5))
            c = int(math.floor(wy / S + 0.5))
            ch = zelle(r, c)
            farbe = FARBE[ch]
            # weiche Ränder: Boden nahe am Fels dunkler (Abstand in Bildpunkten zur nächsten Zellgrenze mit Fels)
            if ch != "#":
                fu = (wx / S + 0.5) % 1.0
                fv = (wy / S + 0.5) % 1.0
                schatten = 0.0
                for dr, dc, abstand in ((-1, 0, 1.0 - fu), (1, 0, fu), (0, -1, fv), (0, 1, 1.0 - fv)):
                    if zelle(r + dr, c + dc) == "#":
                        schatten = max(schatten, 1.0 - min(1.0, abstand / 0.45))
                k = 1.0 - 0.55 * schatten
                farbe = tuple(int(x * k) for x in farbe)
            korn = 0.9 + 0.2 * rauschen(u, v)
            zeile += bytes(max(0, min(255, int(x * korn))) for x in farbe)
        zeilen.append(zeile)
    # der Ausgang als heller Punkt
    for r in range(H):
        for c in range(len(grid[r])):
            if grid[r][c] == "E":
                cu = int(((H - 1 - r) * S - welt_min[0]) / S * PX)
                cv = int((c * S - welt_min[1]) / S * PX)
                for dv in range(-4, 5):
                    for du in range(-4, 5):
                        if du * du + dv * dv <= 16 and 0 <= cv + dv < GROESSE and 0 <= cu + du < GROESSE:
                            zeilen[cv + dv][(cu + du) * 3:(cu + du) * 3 + 3] = bytes((240, 196, 110))
                d.setdefault("karte_marken", [])
                d["karte_marken"] = [{"name": "Ausgang", "welt": [(H - 1 - r) * S, c * S], "size": 0}]
    png(os.path.join(DATEN, name + "_Karte.png"), GROESSE, GROESSE, zeilen)
    d["karte"] = {"bild": name + "_Karte.png", "welt_min": list(welt_min), "welt_groesse": welt_groesse,
                  "fokus_min": [-S, -S], "fokus_max": [H * S, W * S]}
    json.dump(d, open(pfad, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print("Karte", name, "->", name + "_Karte.png")


if __name__ == "__main__":
    namen = sys.argv[1:] or [f[:-5] for f in os.listdir(DATEN) if f.endswith(".json")]
    for n in namen:
        karte(n)
