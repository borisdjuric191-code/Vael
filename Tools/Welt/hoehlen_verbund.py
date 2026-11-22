"""
Joins the eight caves of the Eisenschlund into one map (Boris 2026-11-22: "Höhle einheitlicher gestalten und zusammenführen,
zu einer Map"). Every cave keeps its layout (Scripts/Data/Hoehlen/<Name>.json) and becomes a region of the network:

    Funkenerz-Adern   Kernerz-Stollen (Gilde)   Olmgrotte
    Rostschwamm       Gebeinkammer              Ameisenbau
    Tiefster Schacht  (Fels)                    Kobold-Stollen

The guild drove its tunnels between them (region "Verbindung": guild lanterns, the common thread of the network). The
Kobold-Stollen has no way out into the world and no guild tunnel: only a narrow, winding crack from a far corner of the
Ameisenbau leads there (Boris: special, hard to find, a secret).

Writes Scripts/Data/Hoehlen/Eisenschlund.json with the joined grid, a grid of region letters and per region its art, its title,
its world entrance and its box (cells). Then: py Tools/Welt/hoehlen_karte.py Eisenschlund, and Scripts/BuildHoehle.py with
VAEL_HOEHLE=Eisenschlund.

    py Tools/Welt/hoehlen_verbund.py
"""
import json
import math
import os
import random

DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Scripts", "Data", "Hoehlen")

# Region letter, cave name, slot (row, column) in the 3 x 3 network
REGIONEN = [("f", "FunkenerzAdern", (0, 0)), ("g", "Gildenmine", (0, 1)), ("o", "Olmgrotte", (0, 2)),
            ("r", "Rostschwamm", (1, 0)), ("b", "Gebeinkammer", (1, 1)), ("a", "Ameisenbau", (1, 2)),
            ("s", "TiefsterSchacht", (2, 0)), ("k", "KoboldStollen", (2, 2))]
# Guild tunnels (pairs of letters)
GAENGE = [("g", "f"), ("g", "o"), ("g", "b"), ("f", "r"), ("o", "a"), ("b", "r"), ("b", "a"), ("r", "s")]
SLOT_W, SLOT_H, LUECKE, RAND = 48, 50, 12, 3


def lade(name):
    with open(os.path.join(DATA, name + ".json"), encoding="utf-8") as f:
        return json.load(f)


def erreichbar(g, reg, hoehe, breite):
    """Kommt man vom ersten Ausgang aus zu Fuß in jede Region und zum Schatz (Ausgänge selbst zählen nicht als Weg)"""
    start = next((r, c) for r in range(hoehe) for c in range(breite) if g[r][c] == "E")
    gesehen, offen = {start}, [start]
    while offen:
        r, c = offen.pop()
        for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (r + dr, c + dc)
            if n not in gesehen and 0 <= n[0] < hoehe and 0 <= n[1] < breite and g[n[0]][n[1]] not in "#XOE":
                gesehen.add(n)
                offen.append(n)
    regionen = {reg[r][c] for r in range(hoehe) for c in range(breite) if g[r][c] not in "#XOE" and reg[r][c] != " "}
    erreicht = {reg[r][c] for r, c in gesehen}
    schatz = any(g[r][c] == "Q" for r, c in gesehen)
    return regionen <= erreicht and schatz


def main():
    for versuch in range(40):
        if bauen(2211 + versuch, versuch == 39):
            return
    print("kein Verbund gefunden, in dem alles erreichbar ist")


def bauen(saat, letzter):
    rng = random.Random(saat)
    breite = 3 * SLOT_W + 2 * LUECKE + 2 * RAND
    hoehe = 3 * SLOT_H + 2 * LUECKE + 2 * RAND
    g = [["#"] * breite for _ in range(hoehe)]
    reg = [[" "] * breite for _ in range(hoehe)]
    info = {}
    for buchstabe, name, (sr, sc) in REGIONEN:
        d = lade(name)
        raster = d["grundriss"]
        h, w = len(raster), max(len(z) for z in raster)
        r0 = RAND + sr * (SLOT_H + LUECKE) + (SLOT_H - h) // 2
        c0 = RAND + sc * (SLOT_W + LUECKE) + (SLOT_W - w) // 2
        for r in range(h):
            for c in range(w):
                ch = raster[r][c] if c < len(raster[r]) else "#"
                if buchstabe == "k" and ch == "E":
                    ch = "."                    # the Kobold-Stollen has no way out into the world
                g[r0 + r][c0 + c] = ch
                reg[r0 + r][c0 + c] = buchstabe
        info[buchstabe] = {"name": name, "art": d.get("art", name), "titel": d.get("titel", name),
                           "box": [r0, c0, r0 + h - 1, c0 + w - 1], "eingang_m": d.get("welt_eingang_m") if buchstabe != "k" else None,
                           "portal_bauen": bool(d.get("portal_bauen")) and buchstabe != "k"}

    def boden(r, c):
        return 0 <= r < hoehe and 0 <= c < breite and g[r][c] != "#"

    ausgaenge = [(r, c) for r in range(hoehe) for c in range(breite) if g[r][c] == "E"]

    def zellen(buchstabe):
        """Boden der Höhle, auf dem ein Stollen beginnen darf: weit weg von jedem Ausgang (ein Stollen durch den Ausgang der Mine
        schickte jeden, der zur Gebeinkammer wollte, zurück in die Welt)"""
        r0, c0, r1, c1 = info[buchstabe]["box"]
        return [(r, c) for r in range(r0, r1 + 1) for c in range(c0, c1 + 1) if g[r][c] in ".GLCMZTNB" and reg[r][c] == buchstabe
                and all(max(abs(r - a), abs(c - b)) >= 7 for a, b in ausgaenge)]

    def mitte(buchstabe):
        r0, c0, r1, c1 = info[buchstabe]["box"]
        return (r0 + r1) / 2.0, (c0 + c1) / 2.0

    def graben(punkte, dicke, zeichen_region, laternen=0):
        """A winding tunnel through the points; cells outside every cave become the given region"""
        gesetzt = []
        vorher = [None]

        def frei(rr, cc):
            # nicht neben einem Ausgang: wer durch den Stollen ging, streifte sonst den Mund und stand wieder in der Welt
            if any(max(abs(rr - a), abs(cc - b)) <= 2 for a, b in ausgaenge):
                return
            if 1 <= rr < hoehe - 1 and 1 <= cc < breite - 1 and g[rr][cc] == "#":
                g[rr][cc] = "."
                if reg[rr][cc] == " ":
                    reg[rr][cc] = zeichen_region
                gesetzt.append((rr, cc))

        for (ra, ca), (rb, cb) in zip(punkte, punkte[1:]):
            n = int(max(abs(rb - ra), abs(cb - ca)) * 2) + 1
            for k in range(n + 1):
                t = k / n
                fade = math.sin(math.pi * t)
                r = ra + (rb - ra) * t + math.sin(t * 6.0 + ra) * 0.8 * fade
                c = ca + (cb - ca) * t + math.cos(t * 5.0 + ca) * 0.8 * fade
                # the centre line stays connected side by side (a diagonal step left a one-cell crack closed)
                mr, mc = int(round(r)), int(round(c))
                if vorher[0] is not None and vorher[0] != (mr, mc):
                    pr, pc = vorher[0]
                    while (pr, pc) != (mr, mc):
                        if pr != mr:
                            pr += 1 if mr > pr else -1
                        else:
                            pc += 1 if mc > pc else -1
                        frei(pr, pc)
                vorher[0] = (mr, mc)
                frei(mr, mc)
                b = dicke + rng.uniform(-0.15, 0.3)
                for dr in range(-2, 3):
                    for dc in range(-2, 3):
                        if dr * dr + dc * dc <= b * b:
                            frei(int(round(r + dr)), int(round(c + dc)))
        # guild lanterns along the tunnel
        if laternen:
            for i, (rr, cc) in enumerate(gesetzt):
                # nur am Rand des Stollens, an der Felswand (in der Mitte stand die Laterne im Weg)
                am_fels = any(g[rr + dr][cc + dc] == "#" for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                if i % laternen == laternen // 2 and am_fels and g[rr][cc] == "." and reg[rr][cc] == zeichen_region:
                    g[rr][cc] = "L"
        return gesetzt

    for a, b in GAENGE:
        ma, mb = mitte(a), mitte(b)
        # from the floor of each cave that lies nearest the other one
        pa = min(zellen(a), key=lambda p: (p[0] - mb[0]) ** 2 + (p[1] - mb[1]) ** 2)
        pb = min(zellen(b), key=lambda p: (p[0] - ma[0]) ** 2 + (p[1] - ma[1]) ** 2)
        m = ((pa[0] + pb[0]) / 2 + rng.uniform(-4, 4), (pa[1] + pb[1]) / 2 + rng.uniform(-4, 4))
        graben([pa, m, pb], 1.0, "v", laternen=14)

    # The secret way into the Kobold-Stollen: a crack from the far east corner of the Ameisenbau, along the east edge of the
    # network down to the entrance of the maze (its top right corner); one cell wide, crooked, no light
    kr0, kc0, kr1, kc1 = info["k"]["box"]
    eingang = min(((r, c) for r in range(kr0, kr0 + 4) for c in range(kc0, kc1 + 1) if boden(r, c)), key=lambda p: -p[1])
    ameise = max(zellen("a"), key=lambda p: p[1] * 2 + p[0])
    rand_c = breite - RAND - 1
    weg = [ameise, (ameise[0] + 2, rand_c - 2), ((ameise[0] + kr0) / 2 + 3, rand_c), (kr0 - 4, rand_c - 1), (kr0 - 2, eingang[1] + 2), eingang]
    graben(weg, 0.35, "x")
    info["x"] = {"name": "Spalt", "art": "Spalt", "titel": "Enger Spalt", "box": None, "eingang_m": None, "portal_bauen": False}
    info["v"] = {"name": "Verbindung", "art": "Verbindung", "titel": "Gildenstollen", "box": None, "eingang_m": None, "portal_bauen": False}

    # Each cave built out to its theme (Boris 2026-11-22: "die einzelnen Höhlen sinngemäß ausbauen"): more of what makes it what it is
    AUSBAU = {"g": [("K", 5), ("B", 5)], "v": [("B", 7), ("K", 5)], "f": [("P", 10), ("F", 3)], "a": [("N", 5), ("C", 10)],
              "r": [("M", 16), ("P", 5)], "o": [("C", 5)], "s": [("C", 5)], "b": [("B", 4)]}
    for buchstabe, zeichen in AUSBAU.items():
        frei = [(r, c) for r in range(hoehe) for c in range(breite) if g[r][c] == "." and reg[r][c] == buchstabe
                and all(g[r + dr][c + dc] not in "EL" for dr in (-2, -1, 0, 1, 2) for dc in (-2, -1, 0, 1, 2)
                        if 0 <= r + dr < hoehe and 0 <= c + dc < breite)]
        rng.shuffle(frei)
        gesetzt = []
        for ch, anzahl in zeichen:
            for r, c in frei:
                if anzahl <= 0:
                    break
                if g[r][c] == "." and all(abs(r - a) + abs(c - b) >= 4 for a, b in gesetzt):
                    g[r][c] = ch
                    gesetzt.append((r, c))
                    anzahl -= 1

    if not erreichbar(g, reg, hoehe, breite) and not letzter:
        return False
    daten = {
        "name": "Eisenschlund",
        "titel": "Eisenschlund – Tiefen",
        "art": "Eisenschlund",
        "zelle_m": 4.0,
        "verbund": True,
        "legende": {"#": "Fels", ".": "Boden", "E": "Ausgang in die Welt (der Region)", "Q": "der Schatz des Kobold-Horts",
                    "sonst": "die Zeichen der einzelnen Höhlen"},
        "regionen_legende": {k: v["titel"] for k, v in info.items()},
        "region_info": info,
        "grundriss": ["".join(z) for z in g],
        "regionen": ["".join(z) for z in reg],
    }
    with open(os.path.join(DATA, "Eisenschlund.json"), "w", encoding="utf-8") as f:
        json.dump(daten, f, ensure_ascii=False, indent=1)
    for z in g[::2]:
        print("".join(z[::2]))
    print("geschrieben: Eisenschlund.json, {} x {} Zellen (Saat {})".format(breite, hoehe, saat))
    return True


if __name__ == "__main__":
    main()
