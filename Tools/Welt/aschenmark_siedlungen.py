"""
The people of the Aschenmark: where they live, what they do, what they think of the Prophet (step 2 of
Docs/Plan_NPCs_Siedlungen.md). Plain Python, no Blender, no Unreal:

  py Tools/Welt/aschenmark_siedlungen.py

Reads Scripts/Data/Welt (AM_Hoehen.r16, AM_Orte.json, T_AM_Karte.png), hands out names with namen.py and writes
  Scripts/Data/Welt/AM_Siedlungen.json   settlements, buildings to add, people with role, age, prophet attitude, places
  Docs/Karte/aschenmark_siedlungen.png   preview: the map with settlements, buildings and people (north up),
                                         rendered by Edge from Saved/Karte/aschenmark_siedlungen.svg

World metres as in AM_Orte.json: X north, Y east, Z ground. Places of a person: "arbeit" by day, "feuer" in the
evening, "schlaf" at night (the game teleports them between these out of sight). People on the road have "halte".
Story people (Edda, Maren, Albrun) and bystanders already in L_Vael keep their actor ("bestehend") and their spot.
Rerunning gives the same result; names come from Scripts/Data/Namen/Vergeben.json once handed out.
"""
import array
import base64
import json
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from namen import Namen, rng  # noqa: E402

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DATA = os.path.join(ROOT, "Scripts", "Data", "Welt")
SIZE_X, SIZE_Y = 2017, 2521
REGION = "Aschenmark"

heights = array.array("H")
with open(os.path.join(DATA, "AM_Hoehen.r16"), "rb") as f:
    heights.frombytes(f.read())
ORTE = json.load(open(os.path.join(DATA, "AM_Orte.json"), encoding="utf-8"))
ROADS = [ORTE["main_road"]] + list(ORTE["side_roads"].values())
CRATER = ORTE["places"]["Krater der Glutkönigin"]


def ground(x, y):
    x = min(max(x, 0.0), SIZE_X - 1.001)
    y = min(max(y, 0.0), SIZE_Y - 1.001)
    ix, iy = int(x), int(y)
    tx, ty = x - ix, y - iy

    def h(i, j):
        return (heights[j * SIZE_X + i] - 32768) / 128.0
    return (h(ix, iy) * (1 - tx) + h(ix + 1, iy) * tx) * (1 - ty) + (h(ix, iy + 1) * (1 - tx) + h(ix + 1, iy + 1) * tx) * ty


def slope(x, y):
    return max(abs(ground(x + 2, y) - ground(x - 2, y)), abs(ground(x, y + 2) - ground(x, y - 2))) / 4.0


def road_point(x, y):
    """Distance to the nearest road and the nearest point on it"""
    best = (1e9, x, y)
    for line in ROADS:
        for a, b in zip(line, line[1:]):
            dx, dy = b["x"] - a["x"], b["y"] - a["y"]
            t = max(0.0, min(1.0, ((x - a["x"]) * dx + (y - a["y"]) * dy) / max(dx * dx + dy * dy, 1e-6)))
            px, py = a["x"] + t * dx, a["y"] + t * dy
            d = math.hypot(x - px, y - py)
            if d < best[0]:
                best = (d, px, py)
    return best


def dist(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


# ---------------------------------------------------------------- what lives where

def P(rolle, geschlecht=None, alter="erwachsen", haushalt=None, kind=False, fest=None, bestehend=None, prophet=None, notiz=None):
    return dict(rolle=rolle, geschlecht=geschlecht, alter=alter, haushalt=haushalt, kind=kind, fest=fest, bestehend=bestehend, prophet=prophet, notiz=notiz)


# Prophet attitude mix per settlement (weights). Fixed story people get "Story": their dialogue decides.
SIEDLUNGEN = [
    dict(id="lager", name="Lager der Überlebenden", typ="lager", anker=("camps", "Lager der Überlebenden"), radius=14.0,
         fraktion="Ueberlebende", schnellreise=True, gerede="Lager",
         prophet={"Anhaenger": 3, "Gegner": 2, "Zweifler": 3, "Gleichgueltig": 2},
         leute=[P("Anfuehrer", "f", "alt", fest="Edda Krell", prophet="Story"),
                P("Gelehrter", "f", fest="Albrun", prophet="Story"),
                P("Bauer", bestehend="Ueberlebende 1"), P("Bauer", bestehend="Ueberlebende 2"),
                P("Heiler", "f", haushalt="A"), P("Handwerker", "m", haushalt="A"), P("Kind", alter="kind", haushalt="A", kind=True),
                P("Koch", haushalt="B"), P("Kind", alter="kind", haushalt="B", kind=True), P("Alte", alter="alt", haushalt="B")]),
    dict(id="dorf", name="Ruinen von Kesselgrund", typ="dorf", anker=("places", "Ruinen von Kesselgrund"), radius=22.0,
         fraktion="Ueberlebende", schnellreise=False, gerede="Dorf",
         prophet={"Anhaenger": 2, "Gegner": 3, "Zweifler": 2, "Gleichgueltig": 1},
         leute=[P("Brunnenhueterin", "f", notiz="kühlte Brunnen und Keller mit der Frostlanze, lernte an der Akademie"),
                P("Alte", alter="alt"), P("Pluenderer", "m", "jung"),
                P("Handwerker", "m", haushalt="A"), P("Bauer", "f", haushalt="A"), P("Kind", alter="kind", haushalt="A", kind=True)]),
    dict(id="hof", name="Hof am Ascheacker", typ="hof", anker=("places", "Ascheäcker"), suche=(45, 90, (8, 40)), radius=10.0,
         fraktion="Ueberlebende", schnellreise=False, gerede="Hof",
         prophet={"Anhaenger": 1, "Gegner": 1, "Zweifler": 2, "Gleichgueltig": 1},
         gebaeude=["Huette", "Huette", "Feuer", "Gehege", "Acker"],
         leute=[P("Bauer", "f", haushalt="A"), P("Bauer", "m", haushalt="A"), P("Kind", alter="kind", haushalt="A", kind=True),
                P("Alte", "m", "alt", haushalt="A")]),
    dict(id="kontor", name="Gildenkontor", typ="lager", anker=("camps", "Gildenkontor"), radius=12.0,
         fraktion="Gilde", schnellreise=True, gerede="Kontor",
         prophet={"Anhaenger": 1, "Gegner": 2, "Zweifler": 1, "Gleichgueltig": 4},
         leute=[P("Haendler", "f", fest="Maren Holt", prophet="Story"), P("Wache", bestehend="Gildenwache"),
                P("Schreiber"), P("Traeger"), P("Traeger"), P("Wache"), P("Koch"), P("Handwerker")]),
    dict(id="fuhrleute", name="Fuhrleute der Gilde", typ="unterwegs", strasse=("Gildenkontor", "Passwache"), radius=4.0,
         fraktion="Gilde", schnellreise=False, gerede="Strasse",
         prophet={"Anhaenger": 1, "Gegner": 1, "Zweifler": 1, "Gleichgueltig": 1},
         leute=[P("Fuhrmann", "f"), P("Traeger"), P("Wache")]),
    dict(id="schlackengraeber", name="Schlackengräber", typ="weiler", anker=("places", "Obsidianbruch"), suche=(50, 110, (8, 40)), radius=14.0,
         fraktion="Gilde", schnellreise=False, gerede="Schlacke",
         prophet={"Anhaenger": 2, "Gegner": 2, "Zweifler": 1, "Gleichgueltig": 1},
         gebaeude=["Huette", "Huette", "Zelt", "Zelt", "Werkbank", "Feuer"],
         leute=[P("Vorarbeiter", "f"), P("Arbeiter"), P("Arbeiter"), P("Arbeiter"), P("Koch"), P("Schmied")]),
    dict(id="glutjaeger", name="Glutjäger", typ="aussenposten", anker=("places", "Glutkriecher-Nester"), suche=(60, 130, (22, 60)), radius=8.0,
         fraktion="Freie", schnellreise=False, gerede="Jagd",
         prophet={"Anhaenger": 1, "Gegner": 2, "Zweifler": 1, "Gleichgueltig": 1},
         gebaeude=["Zelt", "Zelt", "Feuer", "Trockengestell"],
         leute=[P("Jaeger"), P("Jaeger"), P("Fallensteller", "f"), P("Gerber")]),
    dict(id="einsiedlerin", name="Einsiedlerin der Harpyienfelsen", typ="aussenposten", anker=("places", "Harpyienfelsen"), suche=(55, 110, (25, 80)), radius=5.0,
         fraktion="Freie", schnellreise=False, gerede="Einsiedlerin",
         prophet={"Gegner": 1},
         gebaeude=["Huette", "Feuer"],
         leute=[P("Einsiedler", "f", "alt", notiz="kennt die Glutkönigin, gibt Hinweise auf den Krater")]),
    dict(id="orden", name="Orden der Narbe", typ="aussenposten", anker=("places", "Kathedrale des Ordens"), radius=12.0,
         fraktion="Orden", schnellreise=False, gerede="Orden",
         prophet={"Anhaenger": 1, "Gegner": 2, "Zweifler": 1},
         leute=[P("Wache"), P("Wache"), P("Prediger"), P("Pilger")]),
    dict(id="passwache", name="Passwache", typ="lager", anker=("camps", "Passwache"), radius=13.0,
         fraktion="Passwache", schnellreise=True, gerede="Passwache",
         prophet={"Anhaenger": 1, "Gegner": 2, "Zweifler": 2, "Gleichgueltig": 1},
         zelte=[(9.0, 35 + 90 * k) for k in range(4)],
         leute=[P("Anfuehrer", "f", notiz="Hauptfrau der Passwache"), P("Wache"), P("Wache"), P("Wache"), P("Koch"), P("Bote", alter="jung")]),
    dict(id="predigerlager", name="Predigerlager", typ="lager", anker=("places", "Predigerlager"), radius=14.0,
         fraktion="Orden", schnellreise=True, gerede="Predigerlager",
         prophet={"Anhaenger": 4, "Gegner": 3, "Zweifler": 1},
         zelte=[(9.0, 10 + 120 * k) for k in range(3)],
         leute=[P("Prediger"), P("Prediger"), P("Pilger"), P("Pilger"), P("Pilger", alter="alt"), P("Pilger", alter="jung"),
                P("Pilger"), P("Heiler")]),
    dict(id="windkante", name="Windkante", typ="aussenposten", anker=("places", "Windkante"), radius=8.0,
         fraktion="Freie", schnellreise=False, gerede="Windkante",
         prophet={"Anhaenger": 1, "Zweifler": 1},
         gebaeude=["Zelt", "Gehege"],
         leute=[P("Hirte"), P("Windleser", "f", "alt")]),
]

# Where each role works, as (radius share of the settlement, towards): towards "mitte", "strasse" or "aussen"
ROLLEN = {
    "Anfuehrer": (0.25, "mitte"), "Gelehrter": (0.5, "aussen"), "Heiler": (0.4, "mitte"), "Handwerker": (0.6, "aussen"),
    "Schmied": (0.6, "aussen"), "Koch": (0.18, "mitte"), "Kind": (0.5, "mitte"), "Alte": (0.3, "mitte"),
    "Bauer": (1.6, "aussen"), "Hirte": (1.8, "aussen"), "Haendler": (0.35, "strasse"), "Wache": (0.95, "strasse"),
    "Schreiber": (0.4, "mitte"), "Traeger": (0.7, "strasse"), "Fuhrmann": (0.0, "strasse"), "Vorarbeiter": (0.6, "aussen"),
    "Arbeiter": (1.4, "aussen"), "Jaeger": (0.7, "aussen"), "Fallensteller": (1.5, "aussen"), "Gerber": (0.6, "aussen"),
    "Einsiedler": (0.3, "mitte"), "Prediger": (0.2, "mitte"), "Pilger": (0.55, "mitte"), "Bote": (0.8, "strasse"),
    "Brunnenhueterin": (0.15, "mitte"), "Pluenderer": (0.9, "aussen"), "Windleser": (0.6, "aussen"),
}
KOERPER = ["schmal", "mittel", "mittel", "kraeftig"]
HILFSMITTEL = ["Stock", "Kruecke", "Augenklappe", "Armschlinge"]


# ---------------------------------------------------------------- free ground

class Platz:
    """Free ground inside one settlement: not water, not steep, not on a road, not in someone else's clearing, not on a taken spot"""

    def __init__(self, s, centre):
        self.centre = centre
        self.taken = [(centre[0], centre[1], 2.0)]  # the fire
        for r, deg in s.get("zelte", []):
            a = math.radians(deg)
            self.taken.append((centre[0] + math.cos(a) * r, centre[1] + math.sin(a) * r, 2.8))
        self.others = [(c["x"], c["y"]) for c in ORTE["clearings"] if dist((c["x"], c["y"]), centre) > 8]

    def free(self, x, y, room, on_road=False):
        if not (25 < x < SIZE_X - 60 and 25 < y < SIZE_Y - 60) or ground(x, y) < 2.0 or slope(x, y) > 0.7:
            return False
        if road_point(x, y)[0] < (1.2 if on_road else 3.0):
            return False
        if any(dist((x, y), c) < 28 for c in self.others) or dist((x, y), (CRATER["x"], CRATER["y"])) < 95:
            return False
        return all(math.hypot(x - tx, y - ty) >= room + tr for tx, ty, tr in self.taken)

    def find(self, angle, radius, r, room=0.8, on_road=False):
        """The free spot nearest to (angle, radius) round the centre; it is taken afterwards"""
        for k in range(160):
            spread = 1 + k / 12
            a = angle + (r.gauss(0, 0.3 * spread) if k else 0)
            d = max(0.0, radius + (r.uniform(-1.5, 1.5) * spread if k else 0))
            x, y = self.centre[0] + math.cos(a) * d, self.centre[1] + math.sin(a) * d
            if self.free(x, y, room, on_road):
                self.taken.append((x, y, room))
                return x, y
        raise RuntimeError("no free spot near {} r={:.0f}".format(self.centre, radius))


def spot(x, y, face):
    yaw = math.degrees(math.atan2(face[1] - y, face[0] - x))
    return {"x": round(x, 2), "y": round(y, 2), "z": round(ground(x, y), 2), "yaw": round(yaw, 1)}


def find_centre(s):
    """A new settlement near its anchor place: flat, near a road (but not on it), away from clearings, camps and places"""
    kind, key = s["anker"]
    anchor = ORTE[kind][key]
    a0 = (anchor["x"], anchor["y"])
    near, far, (road_min, road_max) = s["suche"]
    camps = [(p["x"], p["y"]) for p in ORTE["camps"].values()]
    places = [(p["x"], p["y"]) for n, p in ORTE["places"].items() if n != key]
    clearings = [(c["x"], c["y"]) for c in ORTE["clearings"]]
    best = None
    for d in range(near, far + 1, 5):
        for deg in range(0, 360, 10):
            x, y = a0[0] + math.cos(math.radians(deg)) * d, a0[1] + math.sin(math.radians(deg)) * d
            if not (40 < x < SIZE_X - 80 and 40 < y < SIZE_Y - 80) or ground(x, y) < 3.0:
                continue
            ring = [slope(x + math.cos(t) * s["radius"] * 0.8, y + math.sin(t) * s["radius"] * 0.8) for t in (0, 1.6, 3.1, 4.7)] + [slope(x, y)]
            if max(ring) > 0.45:
                continue
            road = road_point(x, y)[0]
            if not (road_min <= road <= road_max):
                continue
            if min(dist((x, y), c) for c in clearings) < 45 or min(dist((x, y), c) for c in camps) < 70 \
                    or min(dist((x, y), p) for p in places) < 55 or dist((x, y), (CRATER["x"], CRATER["y"])) < 120:
                continue
            score = sum(ring) * 4 + abs(road - (road_min + road_max) / 2) / 25 + d / 150
            if best is None or score < best[0]:
                best = (score, x, y)
    if best is None:
        raise RuntimeError("no place for " + s["name"])
    return best[1], best[2]


# ---------------------------------------------------------------- prophet attitudes

def attitudes(s, people, r):
    """Largest remainder over the weights, shuffled; at least one believer and one enemy where four or more can choose;
    children say what their household says"""
    free_people = [p for p in people if not p["prophet"] and not p["kind"]]
    weights = s["prophet"]
    total = sum(weights.values())
    shares = {k: v * len(free_people) / total for k, v in weights.items()}
    counts = {k: int(v) for k, v in shares.items()}
    for k in sorted(shares, key=lambda k: shares[k] - counts[k], reverse=True)[:len(free_people) - sum(counts.values())]:
        counts[k] += 1
    if len(free_people) >= 4:
        for need in ("Anhaenger", "Gegner"):
            if counts.get(need, 0) == 0:
                donor = max(counts, key=counts.get)
                counts[donor] -= 1
                counts[need] = 1
    pool = [k for k, n in sorted(counts.items()) for _ in range(n)]
    r.shuffle(pool)
    for p, a in zip(free_people, pool):
        p["prophet"] = a
    for p in people:
        if p["kind"]:
            elder = next((q for q in people if q["haushalt"] == p["haushalt"] and not q["kind"] and q["prophet"] not in (None, "Story")), None)
            p["prophet"] = elder["prophet"] if elder else "Gleichgueltig"


# ---------------------------------------------------------------- building the data

def road_route(a_name, b_name):
    road = ORTE["main_road"]
    a, b = ORTE["camps"][a_name], ORTE["camps"][b_name]
    ia = min(range(len(road)), key=lambda i: math.hypot(road[i]["x"] - a["x"], road[i]["y"] - a["y"]))
    ib = min(range(len(road)), key=lambda i: math.hypot(road[i]["x"] - b["x"], road[i]["y"] - b["y"]))
    lo, hi = sorted((ia, ib))
    return road[lo:hi + 1]


def build():
    namen = Namen()
    out = []
    for s in SIEDLUNGEN:
        r = rng("siedlung", s["id"])
        key = "{}/{}".format(REGION, s["id"])
        if s["typ"] == "unterwegs":
            route = road_route(*s["strasse"])
            halts = [route[int(len(route) * f)] for f in (0.25, 0.5, 0.75)]
            centre = (halts[1]["x"], halts[1]["y"])
        elif "suche" in s:
            centre = find_centre(s)
        else:
            kind, name = s["anker"]
            centre = (ORTE[kind][name]["x"], ORTE[kind][name]["y"])
        platz = Platz(s, centre)
        rd, rx, ry = road_point(*centre)
        to_road = math.atan2(ry - centre[1], rx - centre[0]) if rd > 0.5 else r.uniform(0, 2 * math.pi)

        buildings = []
        for i, art in enumerate(s.get("gebaeude", [])):
            if art == "Feuer":
                buildings.append(dict(art=art, **spot(centre[0], centre[1], (centre[0] + 1, centre[1]))))
                continue
            far = {"Acker": 1.7, "Gehege": 1.3}.get(art, 0.75)
            room = {"Huette": 3.2, "Zelt": 2.4, "Acker": 6.0, "Gehege": 3.5}.get(art, 1.6)
            angle = to_road + math.pi + (i - len(s["gebaeude"]) / 2) * 0.9
            x, y = platz.find(angle, s["radius"] * far, r, room=room)
            buildings.append(dict(art=art, **spot(x, y, centre)))

        people = [dict(p) for p in s["leute"]]
        attitudes(s, people, r)
        n_fire = len(people)
        for i, p in enumerate(people):
            pkey = "{}/{}".format(key, i + 1)
            if p["fest"]:
                p.update(name=p["fest"], kultur="mitteleuropa", geschlecht=p["geschlecht"])
            else:
                hh = "{}/{}".format(key, p["haushalt"]) if p["haushalt"] else None
                given = namen.vergib(pkey, geschlecht=p["geschlecht"], haushalt=hh, kind=p["kind"])
                p.update(name=given["name"], kultur=given["kultur"], familie_kultur=given["familie_kultur"], geschlecht=given["geschlecht"])
            p["schluessel"] = pkey
            p["koerper"] = "klein" if p["alter"] == "kind" else r.choice(KOERPER)
            p["hilfsmittel"] = r.choice(HILFSMITTEL) if p["alter"] != "kind" and r.random() < 0.12 else None
            if p["fest"] or p["bestehend"]:
                p["plaetze"] = None  # keeps the spot of its actor in L_Vael
                continue
            share, towards = ROLLEN[p["rolle"]]
            if s["typ"] == "unterwegs":
                p["halte"] = []
                for h in halts:
                    x, y = platz.find(r.uniform(0, 6.3), 1.6, r, on_road=True)
                    x, y = x - centre[0] + h["x"], y - centre[1] + h["y"]
                    p["halte"].append(spot(x, y, (h["x"], h["y"])))
                camp = ORTE["camps"][s["strasse"][1]]
                p["plaetze"] = {"schlaf": spot(camp["x"] + r.uniform(-14, -10), camp["y"] + r.uniform(-6, 6), (camp["x"], camp["y"]))}
                continue
            angle = {"mitte": r.uniform(0, 2 * math.pi), "strasse": to_road + r.uniform(-0.5, 0.5),
                     "aussen": to_road + math.pi + r.uniform(-1.4, 1.4)}[towards]
            work = platz.find(angle, max(1.5, s["radius"] * share), r, on_road=towards == "strasse")
            fire = platz.find(2 * math.pi * i / n_fire + 0.3, 3.2 + 0.6 * (i % 2), r, room=0.6)
            sleep = platz.find(to_road + math.pi + (ord(p["haushalt"][-1]) % 7 if p["haushalt"] else i) * 0.55, s["radius"] * 0.85, r, room=0.9)
            p["plaetze"] = {"arbeit": spot(work[0], work[1], centre if towards != "aussen" else (2 * work[0] - centre[0], 2 * work[1] - centre[1])),
                            "feuer": spot(fire[0], fire[1], centre),
                            "schlaf": spot(sleep[0], sleep[1], centre)}
        for p in people:
            for k in ("fest", "kind") + (("haushalt",) if not p["haushalt"] else ()):
                p.pop(k, None)
            if p.get("haushalt"):
                p["haushalt"] = "{}/{}".format(key, p["haushalt"])
            for k in [k for k, v in p.items() if v is None]:
                p.pop(k)
        out.append({k: v for k, v in dict(
            id=s["id"], name=s["name"], typ=s["typ"], fraktion=s["fraktion"], schnellreise=s["schnellreise"], gerede=s["gerede"],
            mitte=spot(centre[0], centre[1], (centre[0] + 1, centre[1])), radius=s["radius"],
            neu="suche" in s, gebaeude=buildings, leute=people,
            route=[{"x": p["x"], "y": p["y"], "z": p["z"]} for p in road_route(*s["strasse"])] if s["typ"] == "unterwegs" else None).items() if v is not None})
    namen.speichern()
    return out


# ---------------------------------------------------------------- preview

FARBEN = {"Anhaenger": "#f2c94c", "Gegner": "#d64545", "Zweifler": "#9b8cd9", "Gleichgueltig": "#cfcfcf", "Story": "#ffffff"}


def preview(siedlungen, path):
    """The map with north up: settlements as circles with name and head count, buildings as squares,
    people as dots coloured by their attitude to the Prophet"""
    karte = json.load(open(os.path.join(DATA, "AM_Karte.json"), encoding="utf-8"))
    min_x, min_y = karte["world_min"][0] / 100.0, karte["world_min"][1] / 100.0
    size = karte["world_size"] / 100.0
    px = 2048

    def pos(x, y):  # svg x = east, svg y = down = south
        return (y - min_y) / size * px, px - (x - min_x) / size * px

    picture = base64.b64encode(open(os.path.join(DATA, "T_AM_Karte.png"), "rb").read()).decode("ascii")
    svg = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {0} {0}" width="{0}" height="{0}" font-family="Georgia, serif">'.format(px),
           '<image width="{0}" height="{0}" transform="matrix(0,-1,1,0,0,{0})" href="data:image/png;base64,{1}"/>'.format(px, picture)]
    total = 0
    labels = []
    for s in siedlungen:
        cx, cy = pos(s["mitte"]["x"], s["mitte"]["y"])
        rr = s["radius"] / size * px
        colour = "#7fd1ff" if s["schnellreise"] else ("#8fe08a" if s["neu"] else "#ffffff")
        if "route" in s:
            pts = " ".join("{:.1f},{:.1f}".format(*pos(p["x"], p["y"])) for p in s["route"])
            svg.append('<polyline points="{}" fill="none" stroke="#e0a050" stroke-width="5" stroke-dasharray="10 8"/>'.format(pts))
        svg.append('<circle cx="{:.1f}" cy="{:.1f}" r="{:.1f}" fill="none" stroke="{}" stroke-width="3"/>'.format(cx, cy, max(rr, 10), colour))
        for b in s["gebaeude"]:
            bx, by = pos(b["x"], b["y"])
            svg.append('<rect x="{:.1f}" y="{:.1f}" width="7" height="7" fill="#8a6a45" stroke="#000" stroke-width="1"/>'.format(bx - 3.5, by - 3.5))
        for p in s["leute"]:
            spots = [p["plaetze"]["arbeit"]] if p.get("plaetze") and "arbeit" in p["plaetze"] else p.get("halte", [])[:1]
            for q in spots:
                qx, qy = pos(q["x"], q["y"])
                svg.append('<circle cx="{:.1f}" cy="{:.1f}" r="3.2" fill="{}" stroke="#000" stroke-width="0.8"/>'.format(qx, qy, FARBEN[p["prophet"]]))
        total += len(s["leute"])
        label = "{} ({})".format(s["name"], len(s["leute"]))
        # above the circle, or below it where another name already stands
        w = len(label) * 13
        ly = cy - max(rr, 10) - 8
        if any(abs(cx - ox) < (w + ow) / 2 and abs(ly - oy) < 30 for ox, oy, ow in labels):
            ly = cy + max(rr, 10) + 30
        labels.append((cx, ly, w))
        svg.append('<text x="{:.1f}" y="{:.1f}" font-size="26" fill="#fff" stroke="#000" stroke-width="5" paint-order="stroke" text-anchor="middle">{}</text>'.format(cx, ly, label))
    legend = [("Schnellreise-Lager", "#7fd1ff"), ("neue Siedlung", "#8fe08a"), ("bestehender Ort", "#ffffff")] + \
             [("Prophet: " + k, v) for k, v in FARBEN.items()]
    svg.append('<rect x="20" y="20" width="420" height="{}" fill="#000" fill-opacity="0.6" rx="10"/>'.format(70 + 34 * len(legend)))
    svg.append('<text x="40" y="62" font-size="30" fill="#fff">Leute der Aschenmark: {}</text>'.format(total))
    for i, (text, colour) in enumerate(legend):
        y = 100 + 34 * i
        svg.append('<circle cx="52" cy="{}" r="10" fill="{}"/><text x="74" y="{}" font-size="24" fill="#fff">{}</text>'.format(y - 8, colour, y, text))
    svg.append("</svg>")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, "w", encoding="utf-8").write("\n".join(svg))


def main():
    siedlungen = build()
    data = {"info": "Settlements and people of the Aschenmark (Tools/Welt/aschenmark_siedlungen.py). World metres: X north, Y east, Z ground; "
                    "yaw in degrees in world XY (0 = north, 90 = east). Places: arbeit (day), feuer (evening), schlaf (night); "
                    "halte for people on the road; no places = keeps its actor in L_Vael (bestehend or story person).",
            "region": REGION, "siedlungen": siedlungen}
    with open(os.path.join(DATA, "AM_Siedlungen.json"), "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
    svg = os.path.join(ROOT, "Saved", "Karte", "aschenmark_siedlungen.svg")
    preview(siedlungen, svg)
    # the picture for the docs, rendered by Edge if it is there (the svg carries the whole map and is too big for git)
    edge = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
    if os.path.isfile(edge):
        import subprocess
        subprocess.run([edge, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--window-size=2048,2048",
                        "--screenshot=" + os.path.join(ROOT, "Docs", "Karte", "aschenmark_siedlungen.png"), "file:///" + svg.replace(os.sep, "/")],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=120)
    total = 0
    tally = {}
    for s in siedlungen:
        total += len(s["leute"])
        for p in s["leute"]:
            tally[p["prophet"]] = tally.get(p["prophet"], 0) + 1
        print("{:32s} {:12s} {:2d} Leute  Mitte {:7.1f} {:7.1f}{}".format(s["name"], s["typ"], len(s["leute"]), s["mitte"]["x"], s["mitte"]["y"], "  neu" if s["neu"] else ""))
    cultures = {p.get("familie_kultur", p.get("kultur")) for s in siedlungen for p in s["leute"]}
    print("{} Leute, {} Kulturkreise, Prophet: {}".format(total, len(cultures), tally))


if __name__ == "__main__":
    main()
