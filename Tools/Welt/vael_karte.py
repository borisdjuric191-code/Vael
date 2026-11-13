# Setzt die Karte der offenen Welt L_Vael aus den Karten der Regionen zusammen (für die Minikarte und die große Karte):
# südlich der Naht die Aschenmark (T_AM_Karte.png, Tools/Welt/aschenmark_karte.py), nördlich der Wurzelforst
# (T_WF_Karte.png des Blockouts, wf_karte.py), gedreht wie in Scripts/PortWurzelforstToWelt.py (Lage aus WF_Welt.json).
#
#   py Tools/Welt/vael_karte.py
#
# Reines Python. Schreibt Scripts/Data/Welt/T_Vael_Karte.png (quadratisch; Bild x = Welt X (Norden), Bild y = Welt Y
# (Osten), wie die Karte der Aschenmark) und Vael_Karte.json (Ausschnitt, Lager, Namen), die Scripts/CreateWorldMaps.py
# zu DA_Karte_Vael macht. Grob zusammengesetzt (Boris 13.11.2026): die beiden Regionen sind verschieden gemalt.

import json
import os
import struct
import sys
import zlib

HIER = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HIER)
from png16 import lies_png  # noqa: E402

PROJEKT = os.path.join(HIER, "..", "..")
WELT = os.path.join(PROJEKT, "Scripts", "Data", "Welt")
KIT = os.path.join(PROJEKT, "Blockout", "Vael_Wurzelforst_Blockout")
PIXEL = 4096
MIN = (-110000.0, -48000.0)     # cm, Ecke der Karte (Süden, Westen)
GROESSE = 710000.0              # cm, Kantenlänge (Aschenmark, Wurzelforst, Sturmzinnen)
NAHT_X = 201600.0               # cm, nördlicher Rand der Aschenmark
SZ_MEER = bytes([40, 66, 86])   # Wasserfarbe in SZ_Karte.png (sturmzinnen_welt.py)


def schreibe_png(pfad, breite, hoehe, zeilen):
    def chunk(art, daten):
        return struct.pack(">I", len(daten)) + art + daten + struct.pack(">I", zlib.crc32(art + daten) & 0xFFFFFFFF)
    roh = b"".join(b"\x00" + z for z in zeilen)
    with open(pfad, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", breite, hoehe, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(roh, 6)) + chunk(b"IEND", b""))


def main():
    am = json.load(open(os.path.join(WELT, "AM_Karte.json"), encoding="utf-8"))
    wf = json.load(open(os.path.join(KIT, "WF_Karte.json"), encoding="utf-8"))
    lage = json.load(open(os.path.join(WELT, "WF_Welt.json"), encoding="utf-8"))
    vx, vy = lage["versatz_cm"]

    aw, ah, ac, am_bild = lies_png(os.path.join(WELT, "T_AM_Karte.png"))
    ww, wh, wc, wf_bild = lies_png(os.path.join(KIT, "T_WF_Karte.png"))
    print("[vael_karte] Aschenmark {}x{} ({} Kanäle), Wurzelforst {}x{} ({} Kanäle)".format(aw, ah, ac, ww, wh, wc))

    sz = json.load(open(os.path.join(WELT, "SZ_Orte.json"), encoding="utf-8"))
    sw, sh, sc, sz_bild = lies_png(os.path.join(WELT, "SZ_Karte.png"))
    sx0, sy0 = (v * 100.0 for v in sz["landschaft"]["ursprung_m"])
    snord = sx0 + (sz["landschaft"]["punkte"][0] - 1) * 200.0

    def pixel(bild, kanaele, i, j):
        return bytes(bild[j][i * kanaele:i * kanaele + 3])
    meer = pixel(wf_bild, wc, 4, 4)            # tiefes Meer des Wurzelforsts für alles außerhalb der Bilder
    schritt = GROESSE / PIXEL
    zeilen = []
    for py in range(PIXEL):                       # Bild y = Welt Y (Osten)
        y = MIN[1] + (py + 0.5) * schritt
        zeile = bytearray()
        # Aschenmark-Spalte (Bild y = Welt Y) und Wurzelforst-Zeile (Blockout X = Y - vy) hängen nur an y
        am_j = int((y - am["world_min"][1]) / am["world_size"] * ah)
        wf_i = int(((y - vy) - wf["world_min"][0]) / wf["world_size"] * ww)
        for px in range(PIXEL):                   # Bild x = Welt X (Norden)
            x = MIN[0] + (px + 0.5) * schritt
            farbe = meer
            if y >= sy0 and sx0 <= x <= snord:
                # Sturmzinnen: Zeile 0 = Norden, Spalte = Osten, 2 m je Pixel
                si, sj = int((y - sy0) / 200.0), int((snord - x) / 200.0)
                if 0 <= si < sw and 0 <= sj < sh:
                    farbe = pixel(sz_bild, sc, si, sj)
                    if farbe == SZ_MEER:                      # das Meer der Sturmzinnen-Vorschau: wie das übrige Kartenmeer
                        farbe = meer
            elif x < NAHT_X:
                am_i = int((x - am["world_min"][0]) / am["world_size"] * aw)
                if 0 <= am_i < aw and 0 <= am_j < ah:
                    farbe = pixel(am_bild, ac, am_i, am_j)
            else:
                wf_j = int(((vx - x) - wf["world_min"][1]) / wf["world_size"] * wh)
                if 0 <= wf_i < ww and 0 <= wf_j < wh:
                    farbe = pixel(wf_bild, wc, wf_i, wf_j)
            zeile += farbe
        zeilen.append(bytes(zeile))
    # PNG-Zeilen laufen von oben nach unten = Bild y; Bild x läuft in der Zeile
    schreibe_png(os.path.join(WELT, "T_Vael_Karte.png"), PIXEL, PIXEL, zeilen)

    def welt(p):
        return [vx - p[1], p[0] + vy]
    labels = list(am["labels"]) + [{"name": l["name"], "world": welt(l["world"]), "size": l["size"]} for l in wf["labels"]]
    labels.append({"name": "Der Wurzelforst", "world": [300000.0, 30000.0], "size": 2})
    labels.append({"name": "Die Sturmzinnen", "world": [480000.0, 420000.0], "size": 2})
    for name, (gross, punkte) in {"Die Lücke": (1, [3820, 3700]), "Kieferklippen": (1, [3300, 4400]), "Akademie am Auge": (1, [4060, 4900]),
                                  "Hörner": (1, [4560, 5200])}.items():
        labels.append({"name": name, "world": [punkte[0] * 100.0, punkte[1] * 100.0], "size": gross})
    for name, p in sz["orte"].items():
        if name not in ("Gipfel",):
            labels.append({"name": name, "world": [p["x"] * 100.0, p["y"] * 100.0], "size": 0})
    camps = list(am.get("camps", [])) + [{"name": name, "world": [p["x"] * 100.0, p["y"] * 100.0]} for name, p in sz["lager"].items()]
    fx = [am["focus_min"][0], am["focus_max"][0], vx - wf["focus_min"][1], vx - wf["focus_max"][1], sx0, snord]
    fy = [am["focus_min"][1], am["focus_max"][1], wf["focus_min"][0] + vy, wf["focus_max"][0] + vy, sy0,
          sy0 + (sz["landschaft"]["punkte"][1] - 1) * 200.0]
    out = {"title": "Vael", "world_min": list(MIN), "world_size": GROESSE,
           "focus_min": [min(fx), min(fy)], "focus_max": [max(fx), max(fy)],
           "fog_of_war": True, "camps": camps, "labels": labels}
    json.dump(out, open(os.path.join(WELT, "Vael_Karte.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print("[vael_karte] fertig: {} Namen, Ausschnitt {} bis {}".format(len(labels), out["focus_min"], out["focus_max"]))


main()
