# Where the creature groups of the Aschenmark wait (goal step 12): a group about every 30 to 40 m outside the safe zones,
# fitting the zone, thinner along the roads, stronger the farther from the camp of the survivors.
# Writes Scripts/Data/Welt/AM_Gegner.json for Scripts/PlaceGegner.py.
#
#   blender -b --factory-startup -P Tools/Welt/aschenmark_gegner.py -- C:/Projekte/Vael/Scripts/Data/Welt

import json
import math
import os
import sys

import bpy
import numpy as np

welt = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else "C:/Projekte/Vael/Scripts/Data/Welt"
SIZE_X, SIZE_Y = 2017, 2521
rng = np.random.default_rng(1112)

raw = np.fromfile(os.path.join(welt, "AM_Hoehen.r16"), dtype="<u2")
H = ((raw.astype(np.float32) - 32768.0) / 128.0).reshape(SIZE_Y, SIZE_X).T
img = bpy.data.images.load(os.path.join(welt, "T_AM_Wege.png"))
ww, wh = img.size
MASK = np.array(img.pixels[:], dtype=np.float32).reshape(wh, ww, 4)[::-1]
orte = json.load(open(os.path.join(welt, "AM_Orte.json"), encoding="utf-8"))
siedlungen = json.load(open(os.path.join(welt, "AM_Siedlungen.json"), encoding="utf-8"))["siedlungen"]
slope = np.hypot(*np.gradient(H))

# Kinds (data classes of Source/Vael/Creatures) per zone: (class, weight, count range)
TABLE = {
    "K": [("VaelEmberCrawlerData", 4, (3, 5)), ("VaelVeilMonkeyData", 3, (3, 4)), ("VaelSickleDollData", 3, (3, 4)), ("VaelAshHarpyData", 1, (3, 4))],
    "S": [("VaelAshRollerData", 4, (2, 4)), ("VaelEmberCrawlerData", 3, (4, 6)), ("VaelAshHarpyData", 2, (3, 5))],
    "G": [("VaelEmberCrawlerData", 5, (4, 6)), ("VaelAshHarpyData", 3, (3, 5)), ("VaelAshRollerData", 1, (2, 3)), ("VaelHarpyElderData", 1, (1, 2))],
    "P": [("VaelAshHarpyData", 4, (3, 5)), ("VaelVeilMonkeyData", 2, (3, 5)), ("VaelPreacherData", 1, (2, 3)), ("VaelHarpyElderData", 1, (1, 2))],
}
start = orte["camps"]["Lager der Überlebenden"]

safe = []
for c in orte["camps"].values():
    safe.append((c["x"], c["y"], 35.0 + 30.0))
for s in siedlungen:
    if s["typ"] != "unterwegs":
        safe.append((s["mitte"]["x"], s["mitte"]["y"], s["radius"] + 30.0))
    for p in s["leute"]:
        for spot in p.get("halte", []):
            safe.append((spot["x"], spot["y"], 30.0))
safe = np.array(safe)


def zone_letter(z):
    return "K" if z < 0.17 else "S" if z < 0.5 else "G" if z < 0.83 else "P"


groups = []
counts = {}
for gx in np.arange(10, SIZE_X - 10, 35.0):
    for gy in np.arange(10, SIZE_Y - 10, 35.0):
        x, y = gx + rng.uniform(-8, 8), gy + rng.uniform(-8, 8)
        ix, iy = int(x), int(y)
        if H[ix, iy] < 1.5 or H[ix, iy] > 72 or slope[ix, iy] > 0.45:
            continue
        if (np.hypot(safe[:, 0] - x, safe[:, 1] - y) - safe[:, 2]).min() < 0:
            continue
        way, clear, zone = MASK[min(ix // 2, wh - 1), min(iy // 2, ww - 1)][:3]
        if way > 0.2 and rng.random() < 0.5:
            continue                                      # thinner along the roads
        z = zone_letter(zone)
        table = TABLE[z]
        weights = np.array([t[1] for t in table], dtype=float)
        kind, _, (lo, hi) = table[int(rng.choice(len(table), p=weights / weights.sum()))]
        km = math.hypot(x - start["x"], y - start["y"]) / 1000.0
        strength = round(min(2.2, 1.0 + 0.45 * km), 2)
        groups.append({"art": kind, "x": round(float(x), 1), "y": round(float(y), 1), "z": round(float(H[ix, iy]), 2), "zone": z,
                       "anzahl": int(rng.integers(lo, hi + 1)), "staerke": strength, "elite": 0.06 if km > 0.4 else 0.02})
        counts[kind] = counts.get(kind, 0) + 1

json.dump({"info": "Gegnergruppen der Aschenmark (Tools/Welt/aschenmark_gegner.py), Weltmeter", "gruppen": groups},
          open(os.path.join(welt, "AM_Gegner.json"), "w", encoding="utf-8"), ensure_ascii=False, separators=(",", ":"))
print("[aschenmark_gegner] groups:", len(groups), counts)
