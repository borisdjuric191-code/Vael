# Höhenkarte des Wurzelforsts für die offene Welt L_Vael (Region II nördlich des Aschenpasses), für
# Scripts/PortWurzelforstToWelt.py.
#
#   py Tools/Welt/wurzelforst_welt.py
#
# Reines Python. Liest die Höhenkarte des Blockouts (Blockout/Vael_Wurzelforst_Blockout/WF_Hoehenkarte_2017.png, 2 m je
# Punkt, Höhe = (Wert - 32768) / 128 m) und die der Aschenmark (Scripts/Data/Welt/AM_Hoehen.r16) und schreibt nach
# Scripts/Data/Welt:
# - WF_Welt_Hoehen.r16   neue Landschaft, 1261 Punkte nach Norden x 2017 nach Osten, 2 m je Punkt, X (Norden) läuft am schnellsten
# - WF_Welt_Alt.r16      dieselben Punkte mit der alten Höhe des Blockouts (für das Umsetzen der Actors: Unterschied = neu - alt)
# - WF_Welt.json         Lage, Drehung und Maße (auch für das Material und das Umsetzen)
#
# Lage (Boris' Vorgehen 13.11.2026: erst grob zusammenfügen, Details später): Der Blockout wird um 90° gedreht, so dass
# sein Norden der Norden der Welt ist, und so verschoben, dass sein Weg auf das Ende der Hauptstraße am Aschenpass trifft:
#   X_Welt = 250000 - Y_Blockout,  Y_Welt = X_Blockout + 175100  (cm),  Gierwinkel + 90°
# Nur der Teil nördlich der Naht (Kartenpixel y <= 1101) wird zur Landschaft; das Platzhalterstück der Aschenmark im
# Blockout fällt weg. Die ersten beiden Reihen liegen 15 cm unter dem Rand der Aschenmark (keine Risse an der Naht),
# danach geht der Boden auf 240 m weich vom Rand der Aschenmark (Schlucht des Passes, ~54 m) in den Forstsaum (~6 m) über.
# Meer und Wasser liegen in beiden Karten auf 0 m, die Höhen bleiben also, wie sie sind.

import array
import json
import math
import os
import sys

HIER = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HIER)
from png16 import lies_png  # noqa: E402

PROJEKT = os.path.join(HIER, "..", "..")
WELT = os.path.join(PROJEKT, "Scripts", "Data", "Welt")
KIT = os.path.join(PROJEKT, "Blockout", "Vael_Wurzelforst_Blockout")

NX, NY = 1261, 2017                    # Punkte nach Norden und nach Osten (je 126 Quads pro Komponente)
SCHRITT = 200.0                        # cm je Punkt
URSPRUNG = (201400.0, -26500.0)        # cm, Südwestecke der neuen Landschaft
AM_X, AM_Y = 2017, 2521                # Punkte der Aschenmark (1 m)
BAND = 120                             # Reihen (je 2 m) für den weichen Übergang
TIEFER = 0.15                          # m, so weit liegen die beiden Nahtreihen unter der Aschenmark
SEITE = 100.0                          # m, über die der Übergang seitlich neben der Aschenmark ausläuft


def smooth(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def main():
    breite, hoehe, kanaele, zeilen = lies_png(os.path.join(KIT, "WF_Hoehenkarte_2017.png"))
    wf = [[(z[k * kanaele] - 32768) / 128.0 for k in range(breite)] for z in zeilen]
    am = array.array("H")
    with open(os.path.join(WELT, "AM_Hoehen.r16"), "rb") as f:
        am.frombytes(f.read())

    def am_h(x, y):
        """Aschenmark in Metern (x Norden, y Osten), bilinear"""
        x = min(max(x, 0.0), AM_X - 1.001)
        y = min(max(y, 0.0), AM_Y - 1.001)
        ix, iy = int(x), int(y)
        tx, ty = x - ix, y - iy

        def h(i, j):
            return (am[j * AM_X + i] - 32768) / 128.0
        return (h(ix, iy) * (1 - tx) + h(ix + 1, iy) * tx) * (1 - ty) + (h(ix, iy + 1) * (1 - tx) + h(ix + 1, iy + 1) * tx) * ty

    neu = array.array("H", [0]) * (NX * NY)
    alt = array.array("H", [0]) * (NX * NY)
    for iy in range(NY):
        y_m = (URSPRUNG[1] + iy * SCHRITT) / 100.0
        spalte = min(max(iy, 0), breite - 1)                    # Kartenpixel x = iy - 200, Gitterspalte = x + 200
        # Wie stark die Aschenmark hier mitredet: voll über ihrer Breite, seitlich auslaufend
        seitlich = 1.0 - smooth(max(-y_m, y_m - (AM_Y - 1), 0.0) / SEITE)
        rand = am_h(AM_X - 1, y_m)
        for ix in range(NX):
            zeile = min(max(1251 - ix, 0), hoehe - 1)          # Kartenpixel y = 1101 - ix, Gitterzeile = y + 150
            h_wf = wf[zeile][spalte]
            x_m = (URSPRUNG[0] + ix * SCHRITT) / 100.0
            if ix <= 1 and seitlich > 0.0:
                h = (am_h(x_m, y_m) - TIEFER) * seitlich + h_wf * (1.0 - seitlich)
            else:
                s = smooth((ix - 1) / BAND)
                ziel = rand * seitlich + h_wf * (1.0 - seitlich)
                h = ziel * (1.0 - s) + h_wf * s
            k = iy * NX + ix
            neu[k] = int(min(max(round(32768 + h * 128.0), 0), 65535))
            alt[k] = int(min(max(round(32768 + h_wf * 128.0), 0), 65535))
    with open(os.path.join(WELT, "WF_Welt_Hoehen.r16"), "wb") as f:
        neu.tofile(f)
    with open(os.path.join(WELT, "WF_Welt_Alt.r16"), "wb") as f:
        alt.tofile(f)

    info = {
        "info": "Wurzelforst in L_Vael (Tools/Welt/wurzelforst_welt.py). cm. Blockout -> Welt: X = 250000 - Y_b, Y = X_b + 175100, Gier + 90. "
                "Welt -> Blockout (für Materialien mit Weltkoordinaten): X_b = Y - 175100, Y_b = 250000 - X.",
        "punkte": [NX, NY], "schritt_cm": SCHRITT, "ursprung_cm": list(URSPRUNG), "skala": [SCHRITT, SCHRITT, 100.0],
        "drehung_gier": 90.0, "versatz_cm": [250000.0, 175100.0],
        "material_achsen": {"KarteAchseU": [0.0, 1.0], "KarteAchseV": [-1.0, 0.0], "KarteVersatz": [-175100.0, 250000.0]},
        "ende_nord_cm": URSPRUNG[0] + (NX - 1) * SCHRITT, "ende_ost_cm": URSPRUNG[1] + (NY - 1) * SCHRITT,
        "uebergang_m": BAND * SCHRITT / 100.0,
    }
    with open(os.path.join(WELT, "WF_Welt.json"), "w", encoding="utf-8") as f:
        json.dump(info, f, ensure_ascii=False, indent=1)

    # Prüfung an der Naht: Ende der Hauptstraße (Aschenmark 2016 / 965 m) und der Start des Wurzelforsts
    def lies(werte, x_cm, y_cm):
        ix = int(round((x_cm - URSPRUNG[0]) / SCHRITT))
        iy = int(round((y_cm - URSPRUNG[1]) / SCHRITT))
        return (werte[iy * NX + ix] - 32768) / 128.0
    print("[wurzelforst_welt] Naht bei Y 965 m: Aschenmark {:.1f} m, neu {:.1f} m (Reihe 1), {:.1f} m nach 60 m, {:.1f} m nach 240 m".format(
        am_h(2016, 965), lies(neu, 201600, 96500), lies(neu, 207400, 96500), lies(neu, 225400, 96500)))
    start = (250000 - 38000, -78600 + 175100)
    print("[wurzelforst_welt] Start des Blockouts in der Welt: {:.0f} / {:.0f} cm, Boden alt {:.1f} m, neu {:.1f} m".format(
        start[0], start[1], lies(alt, *start), lies(neu, *start)))


main()
