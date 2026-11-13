# Gelände des Eisenschlunds (Region IV, Rippen der Urdrachin) für die offene Welt L_Vael, nach dem Zonenprofil
# Docs/Karte/eisenschlund_zonen.svg (Boris 13.11.2026: "sieht ganz gut aus"; erst die grobe Karte, dann Schluchten, die sich
# anfühlen, als wäre man schon tief unten).
#
#   py Tools/Welt/eisenschlund_welt.py
#
# numpy. Weltmeter: X Norden, Y Osten. Der Eisenschlund grenzt an drei Karten und besteht deshalb aus zwei Landschaften:
#   West  X 0..2016,   Y 2507..3767  (zwischen Aschenmark und Wurzelforst, mit dem Ausgang des Schlackentor-Stollens)
#   Ost   X 382..2902, Y 3767..6539  (südlich der Sturmzinnen, Rippen, Grubenstadt, Nackenstieg)
# An allen Rändern zu den Nachbarn haben sie genau deren Höhen und gehen auf 220 m in das eigene Gelände über.
# Die Rippen sind steile Kämme (Wände bis ~70 m) mit tiefen Tälern dazwischen.
#
# Schreibt nach Scripts/Data/Welt: ES_West_Hoehen.r16, ES_Ost_Hoehen.r16, ES_Orte.json, T_ES_Wege.png (Wegemaske über
# das ganze Gebiet), ES_Karte.png (Kartenbild, Zeile 0 = Norden) und ES_Vorschau.png.

import json
import math
import os
import struct
import sys
import zlib

import numpy as np

HIER = os.path.dirname(os.path.abspath(__file__))
WELT = os.path.join(HIER, "..", "..", "Scripts", "Data", "Welt")
S = 2.0
GX0, GY0 = 0.0, 2507.0                 # m, Ecke des ganzen Gebiets
NI, NJ = 1452, 2017                    # Punkte: X 0..2902, Y 2507..6539
LAND = {"West": {"i": (0, 1009), "j": (0, 631)},          # X 0..2016, Y 2507..3767
        "Ost": {"i": (191, 1452), "j": (630, 2017)}}      # X 382..2902, Y 3767..6539
SEA = -12.0
BAND = 220.0
PNG_KOPF = bytes([137, 80, 78, 71, 13, 10, 26, 10])

X = (GX0 + np.arange(NI) * S)[:, None] * np.ones((1, NJ))
Y = (GY0 + np.arange(NJ) * S)[None, :] * np.ones((NI, 1))


def welt(px, py):
    """Zonenprofil (Pixel) -> Weltmeter (X, Y)"""
    return (3000.0 - py / 0.44, 2550.0 + px / 0.44)


def pfad(punkte):
    return [welt(*p) for p in punkte]


KUESTE = pfad([(30, 700), (80, 560), (200, 470), (330, 420), (470, 360), (600, 300), (720, 250), (860, 190), (1000, 150), (1140, 120),
               (1260, 95), (1360, 90), (1430, 60), (1490, 60), (1530, 120), (1560, 220), (1560, 340), (1555, 480), (1530, 620), (1480, 740),
               (1420, 880), (1300, 980), (1140, 1030), (960, 1075), (760, 1078), (580, 1060), (400, 1040), (230, 990), (120, 920),
               (60, 880), (30, 800)])
KUESTE = [(x, y) for x, y in KUESTE] + [(1400.0, 2400.0), (300.0, 2400.0)]       # nach Westen bis an den Fuß des Gebirges
RIPPEN = [pfad([(300, 480), (520, 380), (760, 330), (990, 330)]),
          pfad([(340, 600), (560, 500), (820, 450), (1090, 450)]),
          pfad([(400, 720), (620, 620), (880, 570), (1170, 580)])]
GRUBENSTADT = welt(581, 853)
SCHLACKE = pfad([(520, 860), (600, 900), (700, 960), (760, 1050)])
STOLLEN_AUS = (1150.0, 2690.0)         # Ausgang des Schlackentor-Stollens im Eisenschlund
HAUPTWEG = [(2903.0, 5830.0)] + pfad([(1440, 90), (1400, 170), (1350, 240), (1290, 290), (1220, 340), (1140, 400), (1060, 460), (980, 510),
                                      (900, 540), (820, 570), (740, 600), (660, 640), (610, 700), (580, 780), (580, 850)])
WESTWEG = [STOLLEN_AUS] + pfad([(120, 800), (220, 820), (320, 840), (420, 858), (500, 860), (580, 850)])
NEBENWEGE = {
    "Kernerzweg": pfad([(580, 850), (700, 840), (820, 860), (960, 900), (1080, 930), (1200, 900), (1300, 840)]),
    "Knochenweg": pfad([(820, 570), (760, 500), (700, 440), (640, 400)]),
    "Suhlenweg": pfad([(1060, 460), (1160, 520), (1260, 560), (1360, 600)]),
    "Laternenweg": pfad([(610, 700), (520, 650), (440, 600), (380, 560)]),
    "Sondierweg": pfad([(380, 560), (340, 520), (300, 490)]),
}
LAGER = {"Schlackentor-Posten": welt(110, 802), "Grubenstadt der Gilde": GRUBENSTADT, "Rippenlager": welt(870, 556), "Nackenhütte": welt(1320, 240)}
ORTE = {
    "Schmelzhütten": welt(470, 940), "Förderturm": welt(820, 900), "Geflutete Stollen": welt(1060, 960), "Kernerz-Stollen": welt(1240, 880),
    "Funkenerz-Adern": welt(1340, 820), "Knochenhalle": welt(700, 390), "Windschlucht": welt(900, 390), "Rippenbrücke": welt(1060, 470),
    "Salvenschwein-Suhlen": welt(1360, 600), "Rostschwamm-Höhle": welt(380, 560), "Grubenlaternen": welt(470, 650),
    "Nackenwirbel": welt(1220, 175), "Wachturm der Gilde": welt(1430, 300), "Tiefster Schacht": welt(290, 500),
}
KAMPF = [welt(*p) for p in [(230, 830), (430, 860), (720, 620), (660, 520), (960, 520), (1000, 400), (1160, 400), (1260, 300), (1420, 200),
                            (1080, 900), (1300, 620), (460, 560)]]
QUELLEN = {"Feuer": [welt(560, 900), welt(700, 980), welt(1240, 860)], "Erde": [welt(760, 400), welt(1000, 560), welt(1360, 600), welt(820, 900)],
           "Luft": [welt(880, 345), welt(1000, 465), welt(1400, 150)], "Wasser": [welt(1060, 960), welt(380, 560)]}
NID_QUELLE = ORTE["Tiefster Schacht"]


def smooth(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def noise(scale, octaves, seed):
    g = np.random.default_rng(seed)
    out = np.zeros((NI, NJ), np.float32)
    amp, total = 1.0, 0.0
    for o in range(octaves):
        ci = max(2, int(NI * S / (scale / 2 ** o)) + 2)
        cj = max(2, int(NJ * S / (scale / 2 ** o)) + 2)
        grid = g.standard_normal((ci, cj)).astype(np.float32)
        xi, yi = np.linspace(0, ci - 1.001, NI), np.linspace(0, cj - 1.001, NJ)
        x0, y0 = xi.astype(int), yi.astype(int)
        tx, ty = smooth(xi - x0)[:, None], smooth(yi - y0)[None, :]
        out += amp * ((grid[x0][:, y0] * (1 - tx) + grid[x0 + 1][:, y0] * tx) * (1 - ty) + (grid[x0][:, y0 + 1] * (1 - tx) + grid[x0 + 1][:, y0 + 1] * tx) * ty)
        total += amp
        amp *= 0.5
    return out / total


def inside(poly):
    res = np.zeros((NI, NJ), bool)
    for (ax, ay), (bx, by) in zip(poly, poly[1:] + poly[:1]):
        res ^= ((ay > Y) != (by > Y)) & (X < (bx - ax) * (Y - ay) / (by - ay + 1e-9) + ax)
    return res


def box_blur(a, r):
    r = int(r)
    k = 2 * r + 1
    c = np.cumsum(np.pad(a, ((r + 1, r), (0, 0)), mode="edge"), axis=0)
    a = (c[k:] - c[:-k]) / k
    c = np.cumsum(np.pad(a, ((0, 0), (r + 1, r)), mode="edge"), axis=1)
    return (c[:, k:] - c[:, :-k]) / k


def polyline_distance(points):
    d = np.full((NI, NJ), 1e9, np.float32)
    for (ax, ay), (bx, by) in zip(points, points[1:]):
        dx, dy = bx - ax, by - ay
        t = np.clip(((X - ax) * dx + (Y - ay) * dy) / max(dx * dx + dy * dy, 1e-6), 0, 1)
        d = np.minimum(d, np.hypot(X - ax - t * dx, Y - ay - t * dy))
    return d


def lies_r16(name, nx, ny):
    raw = np.fromfile(os.path.join(WELT, name), dtype="<u2")
    return ((raw.astype(np.float32) - 32768.0) / 128.0).reshape(ny, nx).T


def main():
    am = lies_r16("AM_Hoehen.r16", 2017, 2521)                    # 1 m, X 0..2016, Y 0..2520
    wf = lies_r16("WF_Welt_Hoehen.r16", 1261, 2017)               # 2 m, X 2014.., Y -265..
    sz = lies_r16("SZ_Hoehen.r16", 1009, 1387)                    # 2 m, X 2902.., Y 3767..

    def am_at(x, y):
        ix = np.clip(np.round(x).astype(int), 0, 2016)
        iy = np.clip(np.round(y).astype(int), 0, 2520)
        return am[ix, iy]

    def wf_at(x, y):
        ix = np.clip(np.round((x - 2014.0) / 2.0).astype(int), 0, 1260)
        iy = np.clip(np.round((y + 265.0) / 2.0).astype(int), 0, 2016)
        return wf[ix, iy]

    def sz_at(y):
        iy = np.clip(np.round((y - 3767.0) / 2.0).astype(int), 0, 1386)
        return sz[0, iy]

    # ---------------------------------------------------------------- das eigene Gelände
    land = inside(KUESTE)
    land_f = box_blur(land.astype(np.float32), 18)
    coast = smooth((land_f - 0.3) / 0.35)
    h = 26.0 + noise(450, 4, 1) * 7.0 + noise(70, 3, 2) * 2.0
    # Felsiger Rand: das Land steigt zur Küste hin zu Klippen an
    rand = smooth((0.85 - land_f) / 0.4) * land_f
    h += 45.0 * rand * (0.7 + 0.3 * noise(200, 2, 3))
    # Die Rippen: steile Kämme (fast senkrechte Wände), dazwischen tiefe Täler
    kamm = np.zeros((NI, NJ), np.float32)
    for k, rippe in enumerate(RIPPEN):
        d = polyline_distance(rippe)
        kamm = np.maximum(kamm, np.exp(-(d / 34.0) ** 4) * (0.8 + 0.2 * noise(160, 3, 10 + k)))
    h += 70.0 * kamm
    tal = np.zeros((NI, NJ), np.float32)
    for a, b in zip(RIPPEN, RIPPEN[1:]):
        mitte = [((ax + bx) * 0.5, (ay + by) * 0.5) for (ax, ay), (bx, by) in zip(a, b)]
        tal = np.maximum(tal, np.exp(-(polyline_distance(mitte) / 60.0) ** 2))
    h -= 12.0 * tal
    # Grubenstadt im Kessel, Schlackenfluss in einer Rinne zum Meer
    h -= 10.0 * np.exp(-(np.hypot(X - GRUBENSTADT[0], Y - GRUBENSTADT[1]) / 160.0) ** 2)
    h -= 5.0 * np.exp(-(polyline_distance(SCHLACKE) / 9.0) ** 2)
    h = h * coast + SEA * (1.0 - coast)

    # ---------------------------------------------------------------- Ränder zu den Nachbarn
    # Aschenmark (Westrand Y 2520, X 0..2016): genau ihre Höhen, dann Übergang; im Überlapp 15 cm darunter
    am_rand = am_at(X, np.full_like(Y, 2520.0))
    w = 1.0 - smooth((Y - 2520.0) / BAND)
    w = np.where(X <= 2016.0, w, 0.0)
    h = h * (1.0 - w) + am_rand * w
    h = np.where((Y <= 2520.0) & (X <= 2016.0), am_at(X, Y) - 0.15, h)
    # Wurzelforst (Block X >= 2014, Y <= 3767): an seinem Rand genau seine Höhen
    bx = np.clip(X, 2014.0, None)
    by = np.clip(Y, None, 3767.0)
    d_wf = np.hypot(X - bx, Y - by)
    d_wf = np.where((X >= 2014.0) & (Y <= 3767.0), 0.0, d_wf)
    near_x = np.where(X < 2014.0, 2016.0, X)
    near_y = np.where(Y > 3767.0, 3767.0, Y)
    wf_rand = wf_at(near_x, near_y)
    w = 1.0 - smooth(d_wf / BAND)
    h = h * (1.0 - w) + wf_rand * w
    # Sturmzinnen (Nordrand X 2902): genau ihre Höhen
    w = 1.0 - smooth((2902.0 - X) / BAND)
    w = np.where(Y >= 3767.0, w, 0.0)
    h = h * (1.0 - w) + sz_at(Y) * w

    # ---------------------------------------------------------------- Wege, Kampfflächen, Lager
    gewicht = np.zeros((NI, NJ), np.float32)
    ziel = np.zeros((NI, NJ), np.float32)
    wege_aus = []
    alle = [("Hauptweg", HAUPTWEG), ("Westweg", WESTWEG)] + list(NEBENWEGE.items())
    for name, punkte in alle:
        pts = []
        for (ax, ay), (bx2, by2) in zip(punkte, punkte[1:]):
            n = max(1, int(math.hypot(bx2 - ax, by2 - ay) / 4.0))
            pts += [(ax + (bx2 - ax) * k / n, ay + (by2 - ay) * k / n) for k in range(n)]
        pts.append(punkte[-1])
        hs = []
        for x, y in pts:
            ix = int(np.clip((x - GX0) / S, 0, NI - 1))
            iy = int(np.clip((y - GY0) / S, 0, NJ - 1))
            hs.append(float(h[max(0, ix - 6):ix + 7, max(0, iy - 6):iy + 7].mean()))
        hs = np.array(hs)
        if name == "Hauptweg":
            hs[0] = float(sz_at(np.array([punkte[0][1]]))[0])
        for _ in range(40):
            hs[1:-1] = (hs[:-2] + hs[1:-1] * 2 + hs[2:]) / 4.0
        for i in range(1, len(hs)):
            hs[i] = np.clip(hs[i], hs[i - 1] - 0.88, hs[i - 1] + 0.88)
        breite = 4.0 if name in ("Hauptweg", "Westweg") else 2.5
        for (x, y), z in zip(pts[::2], hs[::2]):
            ix0, ix1 = int(max(0, (x - 30 - GX0) / S)), int(min(NI, (x + 30 - GX0) / S + 1))
            iy0, iy1 = int(max(0, (y - 30 - GY0) / S)), int(min(NJ, (y + 30 - GY0) / S + 1))
            d = np.hypot(X[ix0:ix1, iy0:iy1] - x, Y[ix0:ix1, iy0:iy1] - y)
            wgt = smooth(1.0 - (d - breite) / 18.0)
            neu = wgt > gewicht[ix0:ix1, iy0:iy1]
            gewicht[ix0:ix1, iy0:iy1] = np.where(neu, wgt, gewicht[ix0:ix1, iy0:iy1])
            ziel[ix0:ix1, iy0:iy1] = np.where(neu, z, ziel[ix0:ix1, iy0:iy1])
        wege_aus.append({"name": name, "kind": "haupt" if name in ("Hauptweg", "Westweg") else "neben",
                         "points": [[round(x, 1), round(y, 1), round(float(z), 2)] for (x, y), z in zip(pts[::5], hs[::5])]})
    # Wege nicht in die Nachbarkarten hinein schneiden
    frei = ~(((Y <= 2520.0) & (X <= 2016.0)) | ((X >= 2014.0) & (Y <= 3767.0)))
    gewicht = np.where(frei, gewicht, 0.0)
    h = h * (1.0 - gewicht) + ziel * gewicht

    def eben(px, py, radius):
        nonlocal h
        ix = int(np.clip((px - GX0) / S, 0, NI - 1))
        iy = int(np.clip((py - GY0) / S, 0, NJ - 1))
        level = float(h[ix, iy])
        k = smooth(1.0 - (np.hypot(X - px, Y - py) - radius) / 14.0) * frei
        h = h * (1.0 - k) + level * k
    for p in KAMPF:
        eben(*p, 20.0)
    for p in LAGER.values():
        eben(*p, 18.0)
    eben(*GRUBENSTADT, 70.0)
    eben(*STOLLEN_AUS, 10.0)
    # Die Ränder zu den Nachbarn bleiben exakt
    h = np.where((Y <= 2520.0) & (X <= 2016.0), am_at(X, Y) - 0.15, h)
    nord = (X >= 2901.0) & (Y >= 3767.0)
    h = np.where(nord, sz_at(Y), h)
    h = np.where((X >= 2014.0) & (Y <= 3767.0), wf_at(X, Y), h)

    # ---------------------------------------------------------------- Ausgabe
    for name, teil in LAND.items():
        i0, i1 = teil["i"]
        j0, j1 = teil["j"]
        stueck = h[i0:i1, j0:j1]
        np.clip(32768.0 + stueck.T * 128.0, 0, 65535).astype("<u2").tofile(os.path.join(WELT, "ES_{}_Hoehen.r16".format(name)))

    def boden(x, y):
        ix = int(np.clip(round((x - GX0) / S), 0, NI - 1))
        iy = int(np.clip(round((y - GY0) / S), 0, NJ - 1))
        return round(float(h[ix, iy]), 2)

    def ort(p):
        return {"x": round(float(p[0]), 1), "y": round(float(p[1]), 1), "z": boden(*p)}

    # Küste zum offenen Meer (ab 2,5 m Tiefe), nur auf eigenem Boden
    raster = 3
    tief = (h[::raster, ::raster] <= -2.5)
    eigen = frei[::raster, ::raster]
    na, nb = tief.shape
    meer = np.zeros_like(tief)
    offen = [(a, b) for a in (0, na - 1) for b in range(nb)] + [(a, b) for a in range(na) for b in (0, nb - 1)]
    while offen:
        a, b = offen.pop()
        if not (0 <= a < na and 0 <= b < nb) or meer[a, b] or not tief[a, b]:
            continue
        meer[a, b] = True
        offen += [(a + 1, b), (a - 1, b), (a, b + 1), (a, b - 1)]
    kanten = []
    for a in range(na):
        for b in range(nb):
            if not meer[a, b] or not eigen[a, b]:
                continue
            for da, db in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                c, d = a + da, b + db
                if 0 <= c < na and 0 <= d < nb and not meer[c, d] and eigen[c, d]:
                    mx = GX0 + (a + c) * raster * S / 2
                    my = GY0 + (b + d) * raster * S / 2
                    q = raster * S / 2
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
    sys.setrecursionlimit(20000)

    def vereinfache(pkt, tol=8.0):
        if len(pkt) < 3:
            return pkt
        (ax, ay), (bx2, by2) = pkt[0], pkt[-1]
        lang = max(math.hypot(bx2 - ax, by2 - ay), 1e-6)
        dist = [abs((bx2 - ax) * (ay - py) - (ax - px) * (by2 - ay)) / lang if lang > 1e-5 else math.hypot(px - ax, py - ay) for px, py in pkt[1:-1]]
        i = int(np.argmax(dist)) + 1
        if dist[i - 1] <= tol:
            return [pkt[0], pkt[-1]]
        return vereinfache(pkt[:i + 1], tol)[:-1] + vereinfache(pkt[i:], tol)
    kueste = [[{"x": x, "y": y, "z": boden(x, y)} for x, y in vereinfache(l)] for l in linien if len(l) > 3]

    out = {
        "info": "Eisenschlund in L_Vael (Tools/Welt/eisenschlund_welt.py): Weltmeter, X Norden, Y Osten, Z Boden",
        "landschaften": {name: {"ursprung_m": [GX0 + t["i"][0] * S, GY0 + t["j"][0] * S], "punkte": [t["i"][1] - t["i"][0], t["j"][1] - t["j"][0]],
                                "schritt_m": S} for name, t in LAND.items()},
        "gebiet": {"ursprung_m": [GX0, GY0], "punkte": [NI, NJ], "schritt_m": S},
        "lager": {k: ort(v) for k, v in LAGER.items()},
        "orte": {k: ort(v) for k, v in ORTE.items()},
        "kampf": [ort(p) for p in KAMPF],
        "quellen": {k: [ort(p) for p in v] for k, v in QUELLEN.items()},
        "nid_quelle": ort(NID_QUELLE),
        "rippen": [[ort(p) for p in r] for r in RIPPEN],
        "schlackenfluss": [ort(p) for p in SCHLACKE],
        "stollen": {"aschenmark": {"x": 1131.0, "y": 2288.0}, "eisenschlund": ort(STOLLEN_AUS)},
        "wege": wege_aus,
        "kueste": kueste,
    }
    json.dump(out, open(os.path.join(WELT, "ES_Orte.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)

    def chunk(art, daten):
        return struct.pack(">I", len(daten)) + art + daten + struct.pack(">I", zlib.crc32(art + daten) & 0xFFFFFFFF)

    def png(name, bild, farbtyp):
        zeilen = b"".join(bytes(1) + bild[i].tobytes() for i in range(bild.shape[0]))
        with open(os.path.join(WELT, name), "wb") as f:
            f.write(PNG_KOPF + chunk(b"IHDR", struct.pack(">IIBBBBB", bild.shape[1], bild.shape[0], 8, farbtyp, 0, 0, 0))
                    + chunk(b"IDAT", zlib.compress(zeilen, 6)) + chunk(b"IEND", b""))
    # Wegemaske (Bild x = X, Bild y = Y), Kartenbild und Vorschau (Zeile 0 = Norden)
    png("T_ES_Wege.png", (np.clip(gewicht, 0, 1) * 255).astype(np.uint8).T.copy(), 0)
    gx, gy = np.gradient(h)
    licht = np.clip(0.75 - (gx * 0.6 + gy * 0.4) * 0.3, 0.3, 1.15)
    farbe = np.stack([0.42 + 0 * h, 0.36 + 0 * h, 0.30 + 0 * h], -1)
    farbe = farbe * (1 - kamm[..., None] * 0.3) + np.array([0.75, 0.72, 0.66]) * kamm[..., None] * 0.3
    farbe = farbe * licht[..., None]
    farbe[h < 0.3] = [0.16, 0.26, 0.34]
    farbe[(polyline_distance(SCHLACKE) < 8.0) & (h > 0.3)] = [0.95, 0.42, 0.12]
    farbe[gewicht > 0.6] = farbe[gewicht > 0.6] * 0.5 + np.array([0.85, 0.78, 0.6]) * 0.5
    nachbar = ~frei
    farbe[nachbar] = [0.16, 0.26, 0.34]
    voll = (np.clip(farbe, 0, 1) * 255).astype(np.uint8)[::-1, :, :].copy()
    png("ES_Karte.png", voll, 2)
    png("ES_Vorschau.png", voll[::3, ::3].copy(), 2)

    steil = []
    for w_ in wege_aus:
        p = w_["points"]
        for a, b in zip(p, p[1:]):
            d = math.hypot(b[0] - a[0], b[1] - a[1])
            if d > 0:
                steil.append(abs(b[2] - a[2]) / d)
    print("[eisenschlund_welt] Höhen {:.0f} bis {:.0f} m; Lager {}; Küste {} Linien, {} Wände; steilster Weg {:.0f} %".format(
        float(h[frei].min()), float(h[frei].max()), {k: v["z"] for k, v in out["lager"].items()}, len(kueste),
        sum(len(l) - 1 for l in kueste), max(steil) * 100))


main()
