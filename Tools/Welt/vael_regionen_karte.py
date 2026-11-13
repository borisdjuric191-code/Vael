# Übersichtskarte der vier Regionen der offenen Welt (Aschenmark, Wurzelforst, Sturmzinnen, Eisenschlund) im Stil der
# Zonenprofile (Docs/Karte/*_zonen.svg): dunkles Meer, das Land aus den echten Höhenkarten, weiche Farben je Region,
# Schnee über den Hörnern, Weg der Kampagne, Lager, Übergänge, Namen. Boris 13.11.2026: "eine Karte der vier Gebiete im
# Stil der aktuellen Aschenmark-Karte, für die Lore".
#
#   py Tools/Welt/vael_regionen_karte.py
#
# Schreibt Docs/Karte/vael_regionen.svg (das Land als eingebettetes Bild, alles Übrige als Vektor). Norden oben.

import base64
import json
import math
import os
import struct
import zlib

import numpy as np

HIER = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.join(HIER, "..", "..")
WELT = os.path.join(PROJ, "Scripts", "Data", "Welt")
KIT = os.path.join(PROJ, "Blockout", "Vael_Wurzelforst_Blockout")
OUT = os.path.join(PROJ, "Docs", "Karte", "vael_regionen.svg")

K = 0.215                       # Pixel je Meter
X_TOP, Y_LEFT = 4950.0, -400.0  # Weltmeter oben links (X Norden, Y Osten)
OX, OY = 40.0, 150.0            # Rand des Bildes in der Karte
BW, BH = 1505, 1075             # Bild des Landes in Pixeln
W, H = 1600, 1290


def sx(y):
    return OX + (y - Y_LEFT) * K


def sy(x):
    return OY + (X_TOP - x) * K


def r16(name, nx, ny):
    raw = np.fromfile(os.path.join(WELT, name), dtype="<u2")
    return ((raw.astype(np.float32) - 32768.0) / 128.0).reshape(ny, nx).T


def smooth(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def blur(a, r):
    k = 2 * r + 1
    c = np.cumsum(np.pad(a, ((r + 1, r), (0, 0)) + ((0, 0),) * (a.ndim - 2), mode="edge"), axis=0)
    a = (c[k:] - c[:-k]) / k
    c = np.cumsum(np.pad(a, ((0, 0), (r + 1, r)) + ((0, 0),) * (a.ndim - 2), mode="edge"), axis=1)
    return (c[:, k:] - c[:, :-k]) / k


def land_image():
    """Höhe, Region und Farbe für jedes Pixel"""
    px = np.arange(BW)
    py = np.arange(BH)
    Y = Y_LEFT + (px + 0.5) / K
    X = X_TOP - (py + 0.5) / K
    XX, YY = np.meshgrid(X, Y, indexing="ij")          # [py, px]
    h = np.full(XX.shape, -20.0, np.float32)
    reg = np.zeros(XX.shape, np.int8)

    def lay(grid, x0, y0, step, rid):
        nx, ny = grid.shape
        fx = (XX - x0) / step
        fy = (YY - y0) / step
        inside = (fx >= 0) & (fx <= nx - 1) & (fy >= 0) & (fy <= ny - 1)
        ix = np.clip(np.round(fx).astype(int), 0, nx - 1)
        iy = np.clip(np.round(fy).astype(int), 0, ny - 1)
        val = grid[ix, iy]
        nonlocal h, reg
        h = np.where(inside, val, h)
        reg = np.where(inside & (val > 0.3), rid, np.where(inside, 0, reg))

    lay(r16("AM_Hoehen.r16", 2017, 2521), 0.0, 0.0, 1.0, 1)
    lay(r16("WF_Welt_Hoehen.r16", 1261, 2017), 2014.0, -265.0, 2.0, 2)
    lay(r16("SZ_Hoehen.r16", 1009, 1387), 2902.0, 3767.0, 2.0, 3)
    es = json.load(open(os.path.join(WELT, "ES_Orte.json"), encoding="utf-8"))
    for name, land in es["landschaften"].items():
        nx, ny = land["punkte"]
        lay(r16("ES_{}_Hoehen.r16".format(name), nx, ny), land["ursprung_m"][0], land["ursprung_m"][1], 2.0, 4)

    land = (h > 0.3).astype(np.float32)
    farben = {1: (0.36, 0.28, 0.21), 2: (0.20, 0.29, 0.22), 3: (0.29, 0.32, 0.37), 4: (0.34, 0.26, 0.20)}
    col = np.zeros(XX.shape + (3,), np.float32)
    for rid, c in farben.items():
        col[reg == rid] = c
    col = blur(col, 22) * 0.55 + col * 0.45                 # weiche Übergänge wie die Farbflecken des Profils
    # Höhenzüge heller, Schnee auf den Hörnern
    col = col * (1.0 + 0.35 * smooth((h - 40.0) / 120.0))[..., None]
    schnee = (smooth((h - 95.0) / 60.0) * (reg == 3))[..., None]
    col = col * (1 - schnee) + np.array([0.82, 0.86, 0.92]) * schnee
    gx, gy = np.gradient(np.clip(h, 0, None))
    licht = np.clip(1.0 - (gx * 0.06 - gy * 0.06), 0.75, 1.2)
    col = col * licht[..., None]
    # Küste: ein dunkler Saum, davor flaches Wasser etwas heller
    rand = blur(land, 2)
    saum = ((rand > 0.05) & (rand < 0.95)).astype(np.float32)
    meer_hell = smooth((h + 4.0) / 4.0) * (1 - land)
    sea = np.array([0.10, 0.13, 0.16])
    flach = np.array([0.16, 0.22, 0.26])
    bild = col * land[..., None] + (sea * (1 - meer_hell[..., None]) + flach * meer_hell[..., None]) * (1 - land[..., None])
    bild = bild * (1 - 0.6 * saum[..., None])
    alpha = np.clip(land + meer_hell * 0.9, 0, 1)
    rgba = np.concatenate([np.clip(bild, 0, 1), alpha[..., None]], -1)
    return (rgba * 255).astype(np.uint8)


def png_bytes(rgba):
    def chunk(art, daten):
        return struct.pack(">I", len(daten)) + art + daten + struct.pack(">I", zlib.crc32(art + daten) & 0xFFFFFFFF)
    zeilen = b"".join(bytes(1) + rgba[i].tobytes() for i in range(rgba.shape[0]))
    return (bytes([137, 80, 78, 71, 13, 10, 26, 10]) + chunk(b"IHDR", struct.pack(">IIBBBBB", rgba.shape[1], rgba.shape[0], 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(zeilen, 9)) + chunk(b"IEND", b""))


def poly(points, step=1):
    return " ".join("{:.1f},{:.1f}".format(sx(y), sy(x)) for x, y in points[::step] + [points[-1]])


def main():
    am = json.load(open(os.path.join(WELT, "AM_Orte.json"), encoding="utf-8"))
    amk = json.load(open(os.path.join(WELT, "AM_Karte.json"), encoding="utf-8"))
    wf_pfade = json.load(open(os.path.join(KIT, "WF_Pfade.json"), encoding="utf-8"))["hauptweg"]
    wfk = json.load(open(os.path.join(KIT, "WF_Karte.json"), encoding="utf-8"))
    sz = json.load(open(os.path.join(WELT, "SZ_Orte.json"), encoding="utf-8"))
    es = json.load(open(os.path.join(WELT, "ES_Orte.json"), encoding="utf-8"))

    def wf_welt(x_b, y_b):
        return ((250000.0 - y_b) / 100.0, (x_b + 175100.0) / 100.0)

    svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="{}" height="{}" viewBox="0 0 {} {}" font-family="Georgia, \'Times New Roman\', serif">'.format(W, H, W, H),
           '<defs><radialGradient id="sea" cx="45%" cy="55%" r="80%"><stop offset="0" stop-color="#232c33"/><stop offset="1" stop-color="#0c1014"/></radialGradient>'
           '<radialGradient id="heart" cx="50%" cy="50%" r="50%"><stop offset="0" stop-color="#ff3b5c" stop-opacity="0.7"/><stop offset="0.6" stop-color="#7a1a3a" stop-opacity="0.35"/>'
           '<stop offset="1" stop-color="#2a0f1a" stop-opacity="0"/></radialGradient>'
           '<filter id="glow"><feGaussianBlur stdDeviation="5"/></filter></defs>',
           '<rect width="{}" height="{}" fill="url(#sea)"/>'.format(W, H)]
    rgba = land_image()
    svg.append('<image x="{}" y="{}" width="{}" height="{}" href="data:image/png;base64,{}"/>'.format(OX, OY, BW, BH, base64.b64encode(png_bytes(rgba)).decode()))

    # Das Herz in der Mitte des Rings: noch nicht gebaut
    hx, hy = sx(2700.0), sy(2550.0)
    svg.append('<circle cx="{:.0f}" cy="{:.0f}" r="120" fill="url(#heart)"/>'.format(hx, hy))
    svg.append('<text x="{:.0f}" y="{:.0f}" font-size="20" fill="#ffd2c8" text-anchor="middle" font-style="italic">V · Das Herz</text>'.format(hx, hy))
    svg.append('<text x="{:.0f}" y="{:.0f}" font-size="13" fill="#cfa9a0" text-anchor="middle">die Wunde · noch nicht gebaut</text>'.format(hx, hy + 20))

    # Weg der Kampagne
    weg = []
    weg.append([(p["x"], p["y"]) for p in am["main_road"]])
    wf = [wf_welt(gx * 200.0 - 201600.0, gy * 200.0 - 201600.0) for gx, gy in wf_pfade]
    weg.append([p for p in wf if p[0] >= 2016.0])
    b = sz["bruecke"]
    weg.append([(b["von"]["x"], b["von"]["y"]), (b["nach"]["x"], b["nach"]["y"]), (3810.0, 3300.0), (3810.0, 3767.0)])
    for w in sz["wege"]:
        if w["name"] in ("Hauptweg", "Nackenweg"):
            weg.append([(p[0], p[1]) for p in w["points"]])
    for w in es["wege"]:
        if w["name"] in ("Hauptweg", "Westweg"):
            weg.append([(p[0], p[1]) for p in w["points"]])
    for linie in weg:
        if len(linie) > 1:
            svg.append('<polyline points="{}" fill="none" stroke="#efe6d2" stroke-width="3.5" stroke-opacity="0.85" stroke-linejoin="round"/>'.format(poly(linie, 2)))
    # Nebenwege
    for w in sz["wege"] + es["wege"]:
        if w["kind"] == "neben" and w["name"] not in ("Nackenweg",):
            svg.append('<polyline points="{}" fill="none" stroke="#efe6d2" stroke-width="1.6" stroke-dasharray="6 5" stroke-opacity="0.5"/>'.format(
                poly([(p[0], p[1]) for p in w["points"]])))
    for name, linie in am["side_roads"].items():
        svg.append('<polyline points="{}" fill="none" stroke="#efe6d2" stroke-width="1.6" stroke-dasharray="6 5" stroke-opacity="0.5"/>'.format(
            poly([(p["x"], p["y"]) for p in linie])))

    # Übergänge: Seilbrücke über die Lücke, der Schlackentor-Stollen durch den Berg
    st = es["stollen"]
    svg.append('<line x1="{:.0f}" y1="{:.0f}" x2="{:.0f}" y2="{:.0f}" stroke="#ffb066" stroke-width="2.5" stroke-dasharray="3 4"/>'.format(
        sx(st["aschenmark"]["y"]), sy(st["aschenmark"]["x"]), sx(st["eisenschlund"]["y"]), sy(st["eisenschlund"]["x"])))
    uebergaenge = [("Aschenpass", 2016.0, 965.0, 12, -8), ("Seilbrücke über die Lücke", 3802.0, 2732.0, 10, -10),
                   ("Schlackentor-Stollen", 1140.0, 2490.0, 10, 18), ("Nackenstieg", 2902.0, 5830.0, 12, 4)]
    for name, x, y, dx, dy in uebergaenge:
        svg.append('<circle cx="{:.0f}" cy="{:.0f}" r="5" fill="#ffb066" stroke="#14171b" stroke-width="1.5"/>'.format(sx(y), sy(x)))
        svg.append('<text x="{:.0f}" y="{:.0f}" font-size="12" fill="#ffd9a8" stroke="#14171b" stroke-width="3" paint-order="stroke">{}</text>'.format(
            sx(y) + dx, sy(x) + dy, name))

    # Lager
    lager = [(c["world"][0] / 100.0, c["world"][1] / 100.0, c["name"]) for c in amk.get("camps", [])]
    lager += [(p["x"], p["y"], n) for n, p in sz["lager"].items()] + [(p["x"], p["y"], n) for n, p in es["lager"].items()]
    for x, y, name in lager:
        cx, cy = sx(y), sy(x)
        svg.append('<rect x="{:.1f}" y="{:.1f}" width="11" height="11" transform="rotate(45 {:.1f} {:.1f})" fill="#ffb066" stroke="#14171b" stroke-width="1.5"/>'.format(
            cx - 5.5, cy - 5.5, cx, cy))
        links = name in ("Passwache",)
        svg.append('<text x="{:.0f}" y="{:.0f}" font-size="11.5" fill="#efe6d2" stroke="#14171b" stroke-width="3" paint-order="stroke" text-anchor="{}">{}</text>'.format(
            cx - 9 if links else cx + 9, cy + 4, "end" if links else "start", name))

    # Nid-Quellen (versiegelt)
    nid = [(am["places"]["Nid-Quelle"]["x"], am["places"]["Nid-Quelle"]["y"]), (sz["nid_quelle"]["x"], sz["nid_quelle"]["y"]),
           (es["nid_quelle"]["x"], es["nid_quelle"]["y"])]
    for lab in wfk["labels"]:
        if lab["name"] == "Nid-Quelle":
            nid.append(wf_welt(*lab["world"]))
    for x, y in nid:
        svg.append('<circle cx="{:.0f}" cy="{:.0f}" r="5.5" fill="#a24dff" stroke="#14171b" stroke-width="1.5"/>'.format(sx(y), sy(x)))

    # Orte (eine Auswahl)
    orte = [(am["places"][n]["x"], am["places"][n]["y"], n) for n in ("Krater der Glutkönigin", "Kathedrale des Ordens", "Ruinen von Kesselgrund", "Schlackentor")
            if n in am["places"]]
    for lab in wfk["labels"]:
        if lab["name"] in ("Eihöhle", "Drachensicht"):
            x, y = wf_welt(*lab["world"])
            orte.append((x, y, lab["name"]))
    orte += [(sz["orte"][n]["x"], sz["orte"][n]["y"], n) for n in ("Das Auge", "Zahnreihe", "Gipfel", "Seherturm des Ordens")]
    orte += [(es["orte"][n]["x"], es["orte"][n]["y"], n) for n in ("Förderturm", "Kernerz-Stollen", "Knochenhalle", "Tiefster Schacht")]
    for x, y, name in orte:
        svg.append('<circle cx="{:.0f}" cy="{:.0f}" r="2.5" fill="#cdbfa3"/>'.format(sx(y), sy(x)))
        svg.append('<text x="{:.0f}" y="{:.0f}" font-size="10.5" fill="#cdbfa3" stroke="#14171b" stroke-width="3" paint-order="stroke">{}</text>'.format(
            sx(y) + 5, sy(x) - 4, "Gipfel (Bossarena)" if name == "Gipfel" else name))

    # Zonen
    zonen = [(p["x"], p["y"], n) for n, p in am["zones"].items()]
    for lab in wfk["labels"]:
        if lab["size"] >= 1:
            x, y = wf_welt(*lab["world"])
            if x >= 2016.0:
                zonen.append((x, y, lab["name"]))
    zonen += [(3820.0, 3700.0, "Die Lücke"), (3300.0, 4400.0, "Kieferklippen"), (4060.0, 4840.0, "Akademie am Auge"), (4560.0, 5250.0, "Hörner")]
    zonen += [(2650.0, 5450.0, "Nackenstieg"), (1980.0, 4080.0, "Rippenbögen"), (700.0, 3900.0, "Minen der Gilde")]
    for x, y, name in zonen:
        svg.append('<text x="{:.0f}" y="{:.0f}" font-size="16" font-weight="bold" fill="#ffffff" text-anchor="middle" stroke="#14171b" stroke-width="4" '
                   'paint-order="stroke" opacity="0.92">{}</text>'.format(sx(y), sy(x), name))

    # Regionen
    regionen = [(1430.0, 1720.0, "I · Aschenmark", "Flanke · Asche, Glut, Schlacke", "#ffb066"),
                (3250.0, 1650.0, "II · Wurzelforst", "Rücken und Schwanz · Wald, Moor", "#9fd08e"),
                (4780.0, 4250.0, "III · Sturmzinnen", "Kopf · Sturm, Schnee, Akademie", "#a9c4ee"),
                (1380.0, 5300.0, "IV · Eisenschlund", "Rippen · Schluchten, Minen, Gilde", "#e0a070")]
    for x, y, name, sub, col in regionen:
        svg.append('<text x="{:.0f}" y="{:.0f}" font-size="30" font-weight="bold" fill="{}" text-anchor="middle" stroke="#14171b" stroke-width="5" paint-order="stroke">{}</text>'.format(
            sx(y), sy(x), col, name))
        svg.append('<text x="{:.0f}" y="{:.0f}" font-size="14" fill="#cdbfa3" text-anchor="middle" stroke="#14171b" stroke-width="3" paint-order="stroke">{}</text>'.format(
            sx(y), sy(x) + 22, sub))

    # Titel, Legende, Maßstab
    svg.append('<text x="40" y="62" font-size="40" font-weight="bold" fill="#efe6d2">Vael · Die vier Regionen</text>')
    svg.append('<text x="40" y="92" font-size="17" fill="#cdbfa3">Der Körper der Urdrachin · offene Welt, Stand 13. November 2026 · Namen sind Arbeitstitel</text>')
    svg.append('<text x="40" y="116" font-size="15" fill="#9d927c">etwa 7 km × 5 km · vier Regionen im Ring, in der Mitte das Herz · Übergänge ohne Ladebildschirm</text>')
    leg = OY + BH + 28
    svg.append('<g transform="translate(40,{:.0f})" font-size="13" fill="#cdbfa3">'.format(leg)
               + '<line x1="0" y1="0" x2="40" y2="0" stroke="#efe6d2" stroke-width="4"/><text x="50" y="5">Weg der Kampagne</text>'
               + '<line x1="210" y1="0" x2="250" y2="0" stroke="#efe6d2" stroke-width="1.6" stroke-dasharray="6 5"/><text x="260" y="5">Nebenweg</text>'
               + '<rect x="360" y="-6" width="11" height="11" transform="rotate(45 365.5 -0.5)" fill="#ffb066"/><text x="380" y="5">Lager: sicher, Schnellreise</text>'
               + '<circle cx="590" cy="0" r="5" fill="#ffb066"/><text x="602" y="5">Übergang zwischen Regionen</text>'
               + '<circle cx="810" cy="0" r="5.5" fill="#a24dff"/><text x="822" y="5">Nid-Quelle (versiegelt)</text>'
               + '<line x1="1000" y1="0" x2="{:.0f}" y2="0" stroke="#efe6d2" stroke-width="3"/><text x="1000" y="22" font-size="12">1 km · knapp 3 Minuten Laufen</text>'.format(1000 + 1000 * K)
               + '</g>')
    svg.append('</svg>')
    open(OUT, "w", encoding="utf-8").write("\n".join(svg))
    print("[vael_regionen_karte] geschrieben:", OUT, "{:.0f} kB".format(os.path.getsize(OUT) / 1024))


main()
