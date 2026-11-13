# Gelände der Sturmzinnen (Region III, Kopf der Urdrachin) für die offene Welt L_Vael, nach dem Zonenprofil
# Docs/Karte/sturmzinnen_zonen.svg (von Boris freigegeben am 13.11.2026: "Passt, bau los").
#
#   py Tools/Welt/sturmzinnen_welt.py
#
# numpy, kein Blender. Weltmeter wie überall: X Norden, Y Osten. Die Landschaft schließt östlich an die des Wurzelforsts an
# (dessen Ostrand Y 3767 m, dort endet der Kiefer aus dem Blockout auf ~36 m); ihre erste Spalte hat genau dessen Höhen.
# Schreibt nach Scripts/Data/Welt:
# - SZ_Hoehen.r16   1009 Punkte nach Norden x 1387 nach Osten, 2 m je Punkt, X läuft am schnellsten, Höhe = (Wert - 32768) / 128 m
# - SZ_Orte.json    Lager, Orte, Kampfflächen, Quellen, Wege (mit Bodenhöhe), Küste zum offenen Meer, Seilbrücke über die Lücke
# - SZ_Vorschau.png Schummerung zum Ansehen
#
# Wetter nach Verderbnis (Boris 13.11.2026): je höher, desto härter bis zum Blizzard, die Hörner immer eine Stufe kälter.

import json
import math
import os
import struct
import zlib

import numpy as np

HIER = os.path.dirname(os.path.abspath(__file__))
WELT = os.path.join(HIER, "..", "..", "Scripts", "Data", "Welt")
NX, NY = 1009, 1387
SCHRITT = 2.0
X0, Y0 = 2902.0, 3767.0          # m, Südwestecke (X0 auf dem Raster des Wurzelforsts: 2014 + 2k)
SEA = -12.0
PNG_KOPF = bytes([137, 80, 78, 71, 13, 10, 26, 10])
WF_NX, WF_NY = 1261, 2017
rng = np.random.default_rng(1311)

X = (X0 + np.arange(NX) * SCHRITT)[:, None] * np.ones((1, NY))
Y = (Y0 + np.arange(NY) * SCHRITT)[None, :] * np.ones((NX, 1))

# ---------------------------------------------------------------- Inhalt des Zonenprofils (Weltmeter)
KUESTE = [(4134, 3767), (4300, 3950), (4560, 4250), (4740, 4600), (4790, 4950), (4740, 5300), (4560, 5650), (4280, 5900),
          (3960, 6020), (3600, 5960), (3250, 6000), (2902, 5960), (2902, 5700), (3150, 5640), (3110, 5450), (3040, 5050), (3080, 4650), (3220, 4300), (3400, 4000), (3556, 3767)]
AUGE = (3960.0, 4990.0)          # Kratersee: Mitte
AUGE_R, RAND_R = 85.0, 150.0
SEE_SPIEGEL = 24.0
HOERNER = [[(4320, 4930), (4520, 5060), (4760, 5230)],          # nördliches Horn
           [(4140, 5470), (4360, 5640), (4600, 5800)]]          # südöstliches Horn
GIPFEL = (4560.0, 5480.0)        # Sattel zwischen den Hörnern: Bossarena
HAUPTWEG = [(3810, 3767), (3820, 3900), (3790, 4060), (3640, 4230), (3480, 4380), (3400, 4500), (3440, 4660), (3560, 4800),
            (3700, 4900), (3820, 4960), (3850, 5100), (3960, 5160), (4080, 5140), (4180, 5180), (4270, 5260), (4370, 5330),
            (4470, 5410), (4560, 5480)]
NEBENWEGE = {
    "Zahnweg": [(3400, 4500), (3280, 4600), (3200, 4760), (3170, 4950)],
    "Blitzfeldweg": [(3560, 4800), (3380, 4920), (3200, 5060)],
    "Seherpfad": [(3820, 4960), (3980, 4800), (4150, 4700), (4250, 4680)],
    "Archivweg": [(3960, 5160), (3880, 5320), (3800, 5450)],
    "Nackenweg": [(3800, 5450), (3640, 5600), (3460, 5760), (3260, 5820), (3040, 5830), (2903, 5830)],
    "Eispfad": [(4270, 5260), (4420, 5180), (4600, 5130), (4700, 5100)],
    "Horstpfad": [(4470, 5410), (4520, 5560), (4560, 5660)],
}
LAGER = {"Lückenwacht": (3820, 3960), "Kliffhütte": (3420, 4450), "Akademie am Auge": (3860, 5110), "Hornbiwak": (4270, 5250)}
ORTE = {
    "Zahnreihe": (3190, 4700), "Wetterwarte": (3560, 4830), "Blitzfeld": (3200, 5080), "Seherturm des Ordens": (4260, 4700),
    "Das Auge": AUGE, "Tränenkanal": (4040, 5150), "Archiv im Fels": (3790, 5460), "Sturmglocken": (3650, 5600),
    "Frostspat-Bruch": (3470, 5820), "Horngletscher": (4560, 4980), "Eisfeld": (4700, 5110), "Doppeluhu-Horste": (4560, 5660),
    "Gipfel": GIPFEL, "Zum Nackenstieg": (3000, 5830),
}
KAMPF = [(3700, 4120), (3560, 4300), (3330, 4560), (3330, 4840), (3640, 4720), (3720, 5010), (4120, 4840), (4080, 5260),
         (3580, 5520), (4400, 5150), (4380, 5470), (4610, 5600)]
QUELLEN = {
    "Wasser": [(3880, 3800), (3150, 4500), AUGE], "Luft": [(3860, 3880), (4240, 4600), (4700, 5150), (4700, 5650)],
    "Erde": [(3200, 4800), (3600, 5600), (4500, 5000)], "Feuer": [LAGER["Akademie am Auge"], LAGER["Hornbiwak"]],
}
NID_QUELLE = ORTE["Tränenkanal"]


def smooth(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def noise(scale, octaves, seed):
    g = np.random.default_rng(seed)
    out = np.zeros((NX, NY), np.float32)
    amp, total = 1.0, 0.0
    for o in range(octaves):
        cells = max(2, int(NX * SCHRITT / (scale / 2 ** o)) + 2)
        cy = max(2, int(NY * SCHRITT / (scale / 2 ** o)) + 2)
        grid = g.standard_normal((cells, cy)).astype(np.float32)
        xi = np.linspace(0, cells - 1.001, NX)
        yi = np.linspace(0, cy - 1.001, NY)
        x0, y0 = xi.astype(int), yi.astype(int)
        tx, ty = smooth(xi - x0)[:, None], smooth(yi - y0)[None, :]
        a = grid[x0][:, y0]
        b = grid[x0 + 1][:, y0]
        c = grid[x0][:, y0 + 1]
        d = grid[x0 + 1][:, y0 + 1]
        out += amp * ((a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty)
        total += amp
        amp *= 0.5
    return out / total


def inside(poly):
    res = np.zeros((NX, NY), bool)
    for (ax, ay), (bx, by) in zip(poly, poly[1:] + poly[:1]):
        cond = ((ay > Y) != (by > Y)) & (X < (bx - ax) * (Y - ay) / (by - ay + 1e-9) + ax)
        res ^= cond
    return res


def box_blur(a, r):
    r = int(r)
    if r < 1:
        return a
    k = 2 * r + 1
    c = np.cumsum(np.pad(a, ((r + 1, r), (0, 0)), mode="edge"), axis=0)
    a = (c[k:] - c[:-k]) / k
    c = np.cumsum(np.pad(a, ((0, 0), (r + 1, r)), mode="edge"), axis=1)
    return (c[:, k:] - c[:, :-k]) / k


def polyline_distance(points):
    d = np.full((NX, NY), 1e9, np.float32)
    for (ax, ay), (bx, by) in zip(points, points[1:]):
        dx, dy = bx - ax, by - ay
        t = np.clip(((X - ax) * dx + (Y - ay) * dy) / max(dx * dx + dy * dy, 1e-6), 0, 1)
        d = np.minimum(d, np.hypot(X - ax - t * dx, Y - ay - t * dy))
    return d


def bump(cx, cy, r):
    return np.exp(-(np.hypot(X - cx, Y - cy) / r) ** 2)


def wf_ostrand():
    """Höhen der letzten Spalte des Wurzelforsts (Y 3767 m) für die ersten Spalten hier"""
    raw = np.fromfile(os.path.join(WELT, "WF_Welt_Hoehen.r16"), dtype="<u2")
    h = ((raw.astype(np.float32) - 32768.0) / 128.0).reshape(WF_NY, WF_NX).T
    rand = h[:, -1]                                   # nach X (Norden), ab 2014 m in 2-m-Schritten
    xs = 2014.0 + np.arange(WF_NX) * 2.0
    return np.interp(X0 + np.arange(NX) * SCHRITT, xs, rand, left=SEA, right=SEA)


def main():
    land = inside(KUESTE)
    land_f = box_blur(land.astype(np.float32), 20)
    # Südküste (Kieferklippen) fällt steil ab, sonst weicher
    sued = smooth((3500.0 - X) / 250.0)
    coast = smooth((land_f - 0.3) / (0.35 - 0.2 * sued))

    h = 22.0 + noise(500, 4, 1) * 8.0 + noise(80, 3, 2) * 2.0
    # 1 Lücke: niedrig, Gischt
    h -= 8.0 * bump(3820, 3950, 300)
    # 2 Kieferklippen: Hochufer, Brandung darunter
    h += 18.0 * smooth((3600.0 - X) / 300.0) * land_f
    # 3 Auge: Kraterrand und See
    r = np.hypot(X - AUGE[0], Y - AUGE[1])
    h += 26.0 * np.exp(-((r - (AUGE_R + 40.0)) / 35.0) ** 2) * (0.85 + 0.15 * noise(60, 2, 3))
    h = np.where(r < AUGE_R + 10.0, np.minimum(h, SEE_SPIEGEL - 6.0 + (r / AUGE_R) ** 2 * 5.0), h)
    # 4 Hörner: der Nordosten steigt, zwei Hornkämme mit Gipfeln, ein Sattel dazwischen
    nordost = smooth((X - 4100.0) / 450.0) * smooth((Y - 4700.0) / 500.0)
    h += 70.0 * nordost
    for kamm in HOERNER:
        d = polyline_distance(kamm)
        (ax, ay), (bx, by) = kamm[0], kamm[-1]
        t = np.clip(((X - ax) * (bx - ax) + (Y - ay) * (by - ay)) / ((bx - ax) ** 2 + (by - ay) ** 2), 0, 1)
        h += (110.0 + 190.0 * t) * np.exp(-(d / (95.0 - 30.0 * t)) ** 2) * (0.85 + 0.15 * noise(120, 3, 4))
    h += 40.0 * bump(GIPFEL[0], GIPFEL[1], 160)

    # Küste: zum Meer hin
    h = h * coast + SEA * (1.0 - coast)

    # Seam: die ersten 150 m gehen von den Höhen des Kiefers aus dem Wurzelforst aus
    rand = wf_ostrand()[:, None]
    s = smooth((Y - Y0) / 150.0)
    h = rand * (1.0 - s) + h * s
    h[:, 0] = rand[:, 0]

    # Wege: ein gleichmäßig steigendes Band (höchstens 22 % Steigung), 8 m eben, 18 m Übergang
    def weg_profil(points, start_h=None):
        pts = []
        for (ax, ay), (bx, by) in zip(points, points[1:]):
            n = max(1, int(math.hypot(bx - ax, by - ay) / 4.0))
            pts += [(ax + (bx - ax) * k / n, ay + (by - ay) * k / n) for k in range(n)]
        pts.append(points[-1])
        hs = []
        for x, y in pts:
            ix = int(np.clip((x - X0) / SCHRITT, 0, NX - 1))
            iy = int(np.clip((y - Y0) / SCHRITT, 0, NY - 1))
            hs.append(float(box_blur(h[max(0, ix - 6):ix + 7, max(0, iy - 6):iy + 7], 0).mean()) if True else 0.0)
        hs = np.array(hs)
        if start_h is not None:
            hs[0] = start_h
        for _ in range(40):
            hs[1:-1] = (hs[:-2] + hs[1:-1] * 2 + hs[2:]) / 4.0
        for i in range(1, len(hs)):                          # Steigung begrenzen (4 m Abstand)
            hs[i] = np.clip(hs[i], hs[i - 1] - 0.88, hs[i - 1] + 0.88)
        return pts, hs

    alle_wege = [("Hauptweg", HAUPTWEG)] + list(NEBENWEGE.items())
    gewicht = np.zeros((NX, NY), np.float32)
    ziel = np.zeros((NX, NY), np.float32)
    wege_aus = []
    for name, punkte in alle_wege:
        pts, hs = weg_profil(punkte, float(rand[int((punkte[0][0] - X0) / SCHRITT), 0]) if name == "Hauptweg" else None)
        breite = 4.0 if name == "Hauptweg" else 2.5
        for (x, y), z in zip(pts[::2], hs[::2]):
            ix0, ix1 = int(max(0, (x - 30 - X0) / SCHRITT)), int(min(NX, (x + 30 - X0) / SCHRITT + 1))
            iy0, iy1 = int(max(0, (y - 30 - Y0) / SCHRITT)), int(min(NY, (y + 30 - Y0) / SCHRITT + 1))
            d = np.hypot(X[ix0:ix1, iy0:iy1] - x, Y[ix0:ix1, iy0:iy1] - y)
            w = smooth(1.0 - (d - breite) / 18.0)
            neu = w > gewicht[ix0:ix1, iy0:iy1]
            gewicht[ix0:ix1, iy0:iy1] = np.where(neu, w, gewicht[ix0:ix1, iy0:iy1])
            ziel[ix0:ix1, iy0:iy1] = np.where(neu, z, ziel[ix0:ix1, iy0:iy1])
        wege_aus.append({"name": name, "kind": "haupt" if name == "Hauptweg" else "neben",
                         "points": [[round(x, 1), round(y, 1), round(float(z), 2)] for (x, y), z in zip(pts[::5], hs[::5])]})
    h = h * (1.0 - gewicht) + ziel * gewicht

    # Ebene Flächen: Kampfflächen 40 m, Lager 36 m, Gipfelarena 50 m
    def eben(px, py, radius):
        nonlocal h
        ix = int(np.clip((px - X0) / SCHRITT, 0, NX - 1))
        iy = int(np.clip((py - Y0) / SCHRITT, 0, NY - 1))
        level = float(h[ix, iy])
        d = np.hypot(X - px, Y - py)
        k = smooth(1.0 - (d - radius) / 14.0)
        h = h * (1.0 - k) + level * k
    for p in KAMPF:
        eben(*p, 20.0)
    for p in LAGER.values():
        eben(*p, 18.0)
    eben(*GIPFEL, 25.0)
    h[:, 0] = rand[:, 0]

    raw = np.clip(32768.0 + h.T * 128.0, 0, 65535).astype("<u2")       # X läuft am schnellsten
    raw.tofile(os.path.join(WELT, "SZ_Hoehen.r16"))

    def boden(x, y):
        ix = int(np.clip(round((x - X0) / SCHRITT), 0, NX - 1))
        iy = int(np.clip(round((y - Y0) / SCHRITT), 0, NY - 1))
        return round(float(h[ix, iy]), 2)

    def ort(p):
        return {"x": float(p[0]), "y": float(p[1]), "z": boden(*p)}

    # Küste zum offenen Meer (Wände ab 2,5 m Tiefe, wie im Wurzelforst): Rand der Meeresfläche, vom Kartenrand aus gefüllt
    raster = 3
    tief = h[::raster, ::raster] <= -2.5
    na, nb = tief.shape
    meer = np.zeros_like(tief)
    offen = [(a, b) for a in range(na) for b in (nb - 1,)] + [(a, b) for a in (0, na - 1) for b in range(nb)]
    while offen:
        a, b = offen.pop()
        if not (0 <= a < na and 0 <= b < nb) or meer[a, b] or not tief[a, b]:
            continue
        meer[a, b] = True
        offen += [(a + 1, b), (a - 1, b), (a, b + 1), (a, b - 1)]
    kanten = []
    for a in range(na):
        for b in range(nb):
            if not meer[a, b]:
                continue
            for da, db in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                c, d = a + da, b + db
                if 0 <= c < na and 0 <= d < nb and not meer[c, d]:
                    mx = X0 + (a + c) * raster * SCHRITT / 2
                    my = Y0 + (b + d) * raster * SCHRITT / 2
                    q = raster * SCHRITT / 2
                    kanten.append(((round(mx - q * abs(db), 1), round(my - q * abs(da), 1)), (round(mx + q * abs(db), 1), round(my + q * abs(da), 1))))
    nachbarn = {}
    for p, q in kanten:
        nachbarn.setdefault(p, []).append(q)
        nachbarn.setdefault(q, []).append(p)
    benutzt, linien = set(), []
    for start in list(nachbarn):
        for weiter in nachbarn[start]:
            if (start, weiter) in benutzt:
                continue
            linie, p, q = [start], start, weiter
            while True:
                benutzt.add((p, q)); benutzt.add((q, p))
                linie.append(q)
                nxt = [r for r in nachbarn[q] if (q, r) not in benutzt]
                if not nxt or q == start:
                    break
                p, q = q, nxt[0]
            linien.append(linie)

    def vereinfache(pkt, tol=8.0):
        if len(pkt) < 3:
            return pkt
        (ax, ay), (bx, by) = pkt[0], pkt[-1]
        lang = max(math.hypot(bx - ax, by - ay), 1e-6)
        dist = [abs((bx - ax) * (ay - py) - (ax - px) * (by - ay)) / lang if lang > 1e-5 else math.hypot(px - ax, py - ay) for px, py in pkt[1:-1]]
        i = int(np.argmax(dist)) + 1
        if dist[i - 1] <= tol:
            return [pkt[0], pkt[-1]]
        return vereinfache(pkt[:i + 1], tol)[:-1] + vereinfache(pkt[i:], tol)
    import sys
    sys.setrecursionlimit(20000)
    kueste = [[{"x": x, "y": y, "z": boden(x, y)} for x, y in vereinfache(l)] for l in linien if len(l) > 3]

    out = {
        "info": "Sturmzinnen in L_Vael (Tools/Welt/sturmzinnen_welt.py): Weltmeter, X Norden, Y Osten, Z Boden. Landschaft ab {} / {} m, {} x {} Punkte, 2 m".format(X0, Y0, NX, NY),
        "landschaft": {"ursprung_m": [X0, Y0], "punkte": [NX, NY], "schritt_m": SCHRITT},
        "lager": {k: ort(v) for k, v in LAGER.items()},
        "orte": {k: ort(v) for k, v in ORTE.items()},
        "kampf": [ort(p) for p in KAMPF],
        "quellen": {k: [ort(p) for p in v] for k, v in QUELLEN.items()},
        "nid_quelle": ort(NID_QUELLE),
        "auge": {"x": AUGE[0], "y": AUGE[1], "radius": AUGE_R, "spiegel": SEE_SPIEGEL},
        "hoerner": [[ort(p) for p in kamm] for kamm in HOERNER],
        "wege": wege_aus,
        "kueste": kueste,
        "bruecke": {"von": {"x": 3800.0, "y": 2622.0}, "nach": {"x": 3805.0, "y": 2842.0}},
    }
    json.dump(out, open(os.path.join(WELT, "SZ_Orte.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)

    # Vorschau: Schummerung, Wasser blau, Schnee über 110 m
    gx, gy = np.gradient(h)
    licht = np.clip(0.75 - (gx * 0.6 + gy * 0.4) * 0.35, 0.3, 1.15)
    farbe = np.stack([0.42 + h * 0 , 0.44 + h * 0, 0.40 + h * 0], -1)
    schnee = smooth((h - 90.0) / 50.0)[..., None]
    farbe = farbe * (1 - schnee) + np.array([0.92, 0.95, 1.0]) * schnee
    farbe = farbe * licht[..., None]
    wasser = (h < 0.3) | (np.hypot(X - AUGE[0], Y - AUGE[1]) < AUGE_R) & (h < SEE_SPIEGEL)
    farbe[wasser] = [0.16, 0.26, 0.34]
    weg = gewicht > 0.6
    farbe[weg] = farbe[weg] * 0.5 + np.array([0.85, 0.78, 0.6]) * 0.5
    voll = (np.clip(farbe, 0, 1) * 255).astype(np.uint8)[::-1, :, :]                   # Norden oben, Osten rechts, 2 m je Pixel
    bild = voll[::2, ::2]                                                                 # Vorschau: halbe Größe
    hgt, wid = bild.shape[:2]
    rohdaten = b"".join(b"\x00" + bild[i].tobytes() for i in range(hgt))

    def chunk(art, daten):
        return struct.pack(">I", len(daten)) + art + daten + struct.pack(">I", zlib.crc32(art + daten) & 0xFFFFFFFF)
    # Kartenbild für die Weltkarte (Tools/Welt/vael_karte.py): volle Auflösung, Zeile 0 = Norden
    vh, vw = voll.shape[:2]
    voll_roh = b"".join(bytes(1) + voll[i].tobytes() for i in range(vh))
    with open(os.path.join(WELT, "SZ_Karte.png"), "wb") as f:
        f.write(PNG_KOPF + chunk(b"IHDR", struct.pack(">IIBBBBB", vw, vh, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(voll_roh, 6)) + chunk(b"IEND", b""))
    with open(os.path.join(WELT, "SZ_Vorschau.png"), "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", wid, hgt, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(rohdaten, 6)) + chunk(b"IEND", b""))

    # Wegemaske für den Boden (R = Weg, 0..255), ein Pixel je Punkt: Bild x = Welt X (Norden), Bild y = Welt Y (Osten)
    maske = (np.clip(gewicht, 0, 1) * 255).astype(np.uint8).T                         # Zeilen = Y
    roh = b"".join(b"\x00" + maske[i].tobytes() for i in range(maske.shape[0]))
    with open(os.path.join(WELT, "T_SZ_Wege.png"), "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", maske.shape[1], maske.shape[0], 8, 0, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(roh, 6)) + chunk(b"IEND", b""))

    print("[sturmzinnen_welt] Höhen {:.0f} bis {:.0f} m; Gipfel {:.0f} m; Lager {}; Küste {} Linien, {} Wände".format(
        h.min(), h.max(), boden(*GIPFEL), {k: v["z"] for k, v in out["lager"].items()}, len(kueste), sum(len(l) - 1 for l in kueste)))
    steil = []
    for w in wege_aus:
        p = w["points"]
        for a, b in zip(p, p[1:]):
            d = math.hypot(b[0] - a[0], b[1] - a[1])
            if d > 0:
                steil.append(abs(b[2] - a[2]) / d)
    print("[sturmzinnen_welt] steilste Wegstelle {:.0f} %".format(max(steil) * 100))


main()
