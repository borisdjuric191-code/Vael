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
MIN = (-10000.0, -48000.0)      # cm, Ecke der Karte (Süden, Westen)
GROESSE = 470000.0              # cm, Kantenlänge
NAHT_X = 201600.0               # cm, nördlicher Rand der Aschenmark


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
            if x < NAHT_X:
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
    fx = [am["focus_min"][0], am["focus_max"][0], vx - wf["focus_min"][1], vx - wf["focus_max"][1]]
    fy = [am["focus_min"][1], am["focus_max"][1], wf["focus_min"][0] + vy, wf["focus_max"][0] + vy]
    out = {"title": "Vael", "world_min": list(MIN), "world_size": GROESSE,
           "focus_min": [min(fx), min(fy)], "focus_max": [max(fx), max(fy)],
           "fog_of_war": True, "camps": am.get("camps", []), "labels": labels}
    json.dump(out, open(os.path.join(WELT, "Vael_Karte.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print("[vael_karte] fertig: {} Namen, Ausschnitt {} bis {}".format(len(labels), out["focus_min"], out["focus_max"]))


main()
