# Übergangszone der Aschenmark zum Wurzelforst (Scripts/Data/Welt/AM_Naht.json).
# Bis zum Zusammenfügen der Karten wird dort nichts Neues verteilt:
#
#   from am_naht import in_naht
#   if in_naht(x, y): continue        # x Norden, y Osten in Weltmetern

import json
import os

_PFAD = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Scripts", "Data", "Welt", "AM_Naht.json")
with open(_PFAD, encoding="utf-8") as _f:
    NAHT = json.load(_f)
_Z = NAHT["uebergangszone"]


def in_naht(x, y, rand=0.0):
    """True, wenn der Punkt (Weltmeter) in der Übergangszone liegt oder ihr näher als `rand` m kommt"""
    return _Z["x_min"] - rand <= x <= _Z["x_max"] + rand and _Z["y_min"] - rand <= y <= _Z["y_max"] + rand
