# Plans where the scenery of the Aschenmark stands (goal step 10), zone by zone, and fits every piece to the ground
# (goal step 11): writes Scripts/Data/Welt/AM_Fuellung.json for Scripts/FillAschenmark.py.
#
#   blender -b --factory-startup -P Tools/Welt/aschenmark_fuellung.py -- C:/Projekte/Vael/Scripts/Data/Welt C:/Projekte/Vael/Saved/Kulisse_Inventar.json
#
# Layout (Diablo style: things in groups, open ground between):
# - Groups every 30 to 60 m: a dead grove, a rock nest, a ruin, fallen wood, slag, a glowing field, moss, by zone.
# - Road sides: small things, now and then a cairn; the roads themselves stay free (AM_Pfade.json).
# - Finds every 100 to 150 m: an abandoned camp, a cart wreck, a grave with offerings, a skeleton, the rest of a
#   watchtower; many with a chest or something to harvest.
# - The edges of the land (mountains, steep slopes) dense and tall.
# - Free: the 40 m fighting clearings, settlements and camps with a margin, the dressed places, roads, the sea.
# - Atmosphere: smoke columns over slag and glowing fields, thin smoke out of the glowing cracks, fog in hollows.
#
# Ground fit: every piece is measured on nine points of its footprint. Lying things (rocks, logs, slag, wrecks) lean
# with the slope and sink by a share of their height; standing things (walls, posts, cairns, junk) stay upright and are
# set on the lowest point, so no side hangs in the air.

import json
import math
import os
import sys

import bpy  # noqa: F401  (run inside Blender like the other world scripts)
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
welt = args[0] if args else "C:/Projekte/Vael/Scripts/Data/Welt"
inventar_path = args[1] if len(args) > 1 else "C:/Projekte/Vael/Saved/Kulisse_Inventar.json"
SIZE_X, SIZE_Y = 2017, 2521
rng = np.random.default_rng(1110)

raw = np.fromfile(os.path.join(welt, "AM_Hoehen.r16"), dtype="<u2")
H = ((raw.astype(np.float32) - 32768.0) / 128.0).reshape(SIZE_Y, SIZE_X).T            # H[x, y] in m
wege_img = bpy.data.images.load(os.path.join(welt, "T_AM_Wege.png"))
ww, wh = wege_img.size
MASK = np.array(wege_img.pixels[:], dtype=np.float32).reshape(wh, ww, 4)[::-1]        # MASK[x / 2, y / 2]
orte = json.load(open(os.path.join(welt, "AM_Orte.json"), encoding="utf-8"))
siedlungen = json.load(open(os.path.join(welt, "AM_Siedlungen.json"), encoding="utf-8"))["siedlungen"]
pfade = json.load(open(os.path.join(welt, "AM_Pfade.json"), encoding="utf-8"))["wege"]
kit = json.load(open(os.path.join(welt, "AM_Kulisse.json"), encoding="utf-8"))["teile"]
inventar = json.load(open(inventar_path, encoding="utf-8"))

# ---------------------------------------------------------------- the pieces
PIECES = {}
for piece in kit:
    PIECES[piece["name"]] = {"mesh": piece["mesh"], "materials": piece["materials"], "art": piece["art"], "zonen": piece["zonen"],
                             "skala": piece["skala"], "groesse": piece["groesse"]}


def add_raw(mesh, art, zonen, skala):
    data = inventar.get(mesh)
    if data is None:
        print("[aschenmark_fuellung] not in the inventory:", mesh)
        return
    PIECES[mesh.split("/")[-1]] = {"mesh": mesh, "materials": [], "art": art, "zonen": zonen, "skala": skala, "groesse": data["size"]}


for k in range(1, 10):
    add_raw("/Game/Megascans/3D_Plants/DeadBroomBush/SM_DeadBroomBush_V{}".format(k), "Baum", "KGP", (3.6, 6.0))
for k in range(1, 4):
    add_raw("/Game/Megascans/3D_Plants/DryPlantSetA/SM_DryPlantSet_V{}".format(k), "Kraut", "KSGP", (0.9, 1.5))
add_raw("/Game/Vael/Maps/Aschenmark/Bauten/SM_AM_Zelt", "Zelt", "KSGP", (2.0, 2.4))

BY_KIND = {}
for name, piece in PIECES.items():
    BY_KIND.setdefault(piece["art"], []).append(name)

LYING = {"Fels", "Stamm", "Ast", "Wurzel", "Kiesel", "Schlacke", "Wrack", "Gerippe", "Boeschung", "Platten", "Glutspalte", "Grab", "Kraut"}
NO_COLLISION = {"Ast", "Kiesel", "Kraut"}
SINK = {"Fels": 0.18, "Schlacke": 0.15, "Kiesel": 0.3, "Boeschung": 0.25, "Stamm": 0.12, "Wurzel": 0.2, "Ast": 0.25, "Gerippe": 0.08,
        "Platten": 0.4, "Glutspalte": 0.0, "Grab": 0.05, "Wrack": 0.05, "Kraut": 0.05}


def zone_letter(z):
    return "K" if z < 0.17 else "S" if z < 0.5 else "G" if z < 0.83 else "P"


def mask_at(x, y):
    m = MASK[int(np.clip(x / 2, 0, wh - 1)), int(np.clip(y / 2, 0, ww - 1))]
    return m[0], m[1], m[2]


def height(x, y):
    x = float(np.clip(x, 0, SIZE_X - 1.001))
    y = float(np.clip(y, 0, SIZE_Y - 1.001))
    ix, iy = int(x), int(y)
    fx, fy = x - ix, y - iy
    return float(H[ix, iy] * (1 - fx) * (1 - fy) + H[ix + 1, iy] * fx * (1 - fy) + H[ix, iy + 1] * (1 - fx) * fy + H[ix + 1, iy + 1] * fx * fy)


# ---------------------------------------------------------------- where nothing may stand
road_pts = []
for road in pfade:
    pts = np.array(road["points"])
    width = {"haupt": 4.5, "neben": 3.5, "pfad": 2.5}[road["kind"]]
    for a, b in zip(pts[:-1], pts[1:]):
        n = max(1, int(np.linalg.norm(b - a) / 1.5))
        for t in np.linspace(0, 1, n, endpoint=False):
            road_pts.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, width))
road_pts = np.array(road_pts)
CELL = 20.0
road_grid = {}
for x, y, w in road_pts:
    road_grid.setdefault((int(x // CELL), int(y // CELL)), []).append((x, y, w))


def road_distance(x, y):
    """Distance to the nearest road edge (negative on the road)"""
    best = 99.0
    cx, cy = int(x // CELL), int(y // CELL)
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for px, py, w in road_grid.get((cx + dx, cy + dy), []):
                best = min(best, math.hypot(px - x, py - y) - w)
    return best


keep_free = []
for c in orte["clearings"]:
    keep_free.append((c["x"], c["y"], 22.0))
for c in orte["camps"].values():
    keep_free.append((c["x"], c["y"], 38.0))
for c in orte["places"].values():
    keep_free.append((c["x"], c["y"], 30.0))
for s in siedlungen:
    if s["typ"] != "unterwegs":
        keep_free.append((s["mitte"]["x"], s["mitte"]["y"], s["radius"] + 12.0))
        for b in s.get("gebaeude", []):
            keep_free.append((b["x"], b["y"], 7.0))
    for p in s["leute"]:
        for spot in list(p.get("plaetze", {}).values()) + p.get("halte", []):
            keep_free.append((spot["x"], spot["y"], 4.0))
keep_free = np.array(keep_free)


def free_ground(x, y, margin=0.0):
    if x < 3 or y < 3 or x > SIZE_X - 4 or y > SIZE_Y - 4:
        return False
    if height(x, y) < 1.5:
        return False
    d = np.hypot(keep_free[:, 0] - x, keep_free[:, 1] - y) - keep_free[:, 2]
    if d.min() < margin:
        return False
    return road_distance(x, y) > 1.0 + margin


# Placed footprints, so pieces do not grow into each other
taken = {}


def overlaps(x, y, r):
    cx, cy = int(x // 10), int(y // 10)
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for px, py, pr in taken.get((cx + dx, cy + dy), []):
                if math.hypot(px - x, py - y) < (pr + r) * 0.8:
                    return True
    return False


def take(x, y, r):
    taken.setdefault((int(x // 10), int(y // 10)), []).append((x, y, r))


# ---------------------------------------------------------------- fit to the ground
placed = []
stats = {}


def fit(name, x, y, scale=None, yaw=None, check_free=True, margin=0.0):
    """Places one piece if the ground allows; returns True if placed"""
    piece = PIECES[name]
    art = piece["art"]
    scale = scale if scale is not None else float(rng.uniform(*piece["skala"]))
    yaw = yaw if yaw is not None else float(rng.uniform(0, 360))
    sx, sy, sz = (v / 100.0 * scale for v in piece["groesse"])
    radius = max(0.4, math.hypot(sx, sy) * 0.45)
    if check_free and (not free_ground(x, y, margin + radius * 0.6) or overlaps(x, y, radius)):
        return False
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    samples = []
    for u in (-0.45, 0.0, 0.45):
        for v in (-0.45, 0.0, 0.45):
            px, py = x + c * u * sx - s * v * sy, y + s * u * sx + c * v * sy
            samples.append((px, py, height(px, py)))
    zs = np.array([p[2] for p in samples])
    spread = zs.max() - zs.min()
    normal = (0.0, 0.0, 1.0)
    if art in LYING:
        # plane through the samples in world metres: z = a*(px - x) + b*(py - y) + c; the piece leans with it (at most 22 degrees)
        A = np.array([[p[0] - x, p[1] - y, 1.0] for p in samples])
        coef, *_ = np.linalg.lstsq(A, zs, rcond=None)
        n = np.array([-coef[0], -coef[1], 1.0])
        n /= np.linalg.norm(n)
        if n[2] < math.cos(math.radians(22)):
            flat = math.sqrt(max(1e-6, n[0] ** 2 + n[1] ** 2))
            k = math.sin(math.radians(22)) / flat
            n = np.array([n[0] * k, n[1] * k, math.cos(math.radians(22))])
        normal = (round(float(n[0]), 4), round(float(n[1]), 4), round(float(n[2]), 4))
        z = float(coef[2]) - SINK.get(art, 0.1) * sz
        if spread > max(1.5, sz * 1.2):
            return False
    else:
        if spread > max(0.8, sz * 0.5) and art not in ("Baum", "Stumpf"):
            return False                                   # an upright thing on a steep slope would show its foot
        z = float(zs.min()) - 0.04 - (0.08 * sz if art in ("Baum", "Stumpf", "Pfaehle") else 0.0)
    placed.append([name, round(x, 2), round(y, 2), round(z, 3), round(yaw, 1), normal[0], normal[1], normal[2], round(scale, 3)])
    take(x, y, radius)
    stats[art] = stats.get(art, 0) + 1
    return True


def pick(kind, zone):
    names = [n for n in BY_KIND.get(kind, []) if zone in PIECES[n]["zonen"]] or BY_KIND.get(kind, [])
    return names[int(rng.integers(len(names)))] if names else None


def scatter_group(cx, cy, zone, recipe, radius):
    """A group: (kind, min, max) counts placed round a centre"""
    count = 0
    for kind, lo, hi in recipe:
        for _ in range(int(rng.integers(lo, hi + 1))):
            name = pick(kind, zone)
            if name is None:
                continue
            for _try in range(6):
                a = rng.uniform(0, 2 * math.pi)
                d = radius * math.sqrt(rng.uniform(0.0, 1.0))
                if fit(name, cx + math.cos(a) * d, cy + math.sin(a) * d):
                    count += 1
                    break
    return count


GROUPS = {
    "K": [("Hain", 4, [("Baum", 2, 4), ("Stumpf", 1, 2), ("Ast", 2, 4), ("Wurzel", 0, 1), ("Kraut", 2, 4)]),
          ("Felsnest", 3, [("Fels", 2, 4), ("Kiesel", 1, 3), ("Kraut", 1, 3)]),
          ("Ruine", 2, [("Ruine", 1, 3), ("Platten", 0, 1), ("Fels", 0, 2), ("Kiesel", 1, 2), ("Kram", 0, 1)]),
          ("Holzfall", 3, [("Stamm", 1, 2), ("Ast", 2, 3), ("Stumpf", 0, 2), ("Kraut", 1, 2)]),
          ("Brand", 2, [("Pfaehle", 1, 2), ("Stamm", 0, 1), ("Ast", 1, 3), ("Gerippe", 0, 1)])],
    "S": [("Schlacke", 5, [("Schlacke", 2, 4), ("Fels", 1, 2), ("Kiesel", 1, 2)]),
          ("Felsnest", 3, [("Fels", 2, 5), ("Kiesel", 1, 3)]),
          ("Kram", 1, [("Kram", 2, 3), ("Schlacke", 1, 2), ("Pfaehle", 0, 1)]),
          ("Ruine", 1, [("Ruine", 1, 2), ("Schlacke", 1, 2)])],
    "G": [("Glutfeld", 4, [("Glutspalte", 1, 1), ("Schlacke", 1, 2), ("Pfaehle", 0, 1), ("Kraut", 0, 2)]),
          ("Hain", 3, [("Baum", 1, 3), ("Stumpf", 1, 2), ("Ast", 1, 3), ("Pfaehle", 0, 1)]),
          ("Felsnest", 3, [("Fels", 2, 3), ("Schlacke", 1, 2), ("Kiesel", 1, 2)]),
          ("Brand", 2, [("Pfaehle", 1, 2), ("Gerippe", 0, 1), ("Ast", 1, 2)])],
    "P": [("Felsnest", 5, [("Fels", 3, 5), ("Kiesel", 1, 3), ("Kraut", 1, 2)]),
          ("Moos", 3, [("Fels", 1, 2), ("Stamm", 0, 1), ("Boeschung", 0, 1), ("Stumpf", 1, 2), ("Kraut", 1, 3)]),
          ("Hain", 2, [("Baum", 1, 3), ("Stumpf", 1, 2), ("Ast", 1, 2)]),
          ("Ruine", 1, [("Ruine", 1, 2), ("Fels", 1, 2)])],
}


def choose(options):
    weights = np.array([o[1] for o in options], dtype=float)
    return options[int(rng.choice(len(options), p=weights / weights.sum()))]


# ---------------------------------------------------------------- 1. groups every 30 to 60 m
group_centres = []
step = 34.0
for gx in np.arange(10, SIZE_X - 10, step):
    for gy in np.arange(10, SIZE_Y - 10, step):
        x, y = gx + rng.uniform(-14, 14), gy + rng.uniform(-14, 14)
        if not free_ground(x, y, 5.0):
            continue
        way, clear, zone = mask_at(x, y)
        z = zone_letter(zone)
        kind, _, recipe = choose(GROUPS[z])
        if scatter_group(x, y, z, recipe, rng.uniform(6.0, 10.0)) > 0:
            group_centres.append((x, y, z, kind))
print("[aschenmark_fuellung] groups:", len(group_centres))

# ---------------------------------------------------------------- 2. the edges of the land, dense and tall
slope = np.hypot(*np.gradient(H))
edge_count = 0
for gx in np.arange(6, SIZE_X - 6, 16.0):
    for gy in np.arange(6, SIZE_Y - 6, 16.0):
        x, y = gx + rng.uniform(-6, 6), gy + rng.uniform(-6, 6)
        ix, iy = int(x), int(y)
        if not (H[ix, iy] > 72 or slope[ix, iy] > 0.55):
            continue
        z = zone_letter(mask_at(x, y)[2])
        name = pick("Fels", z) if rng.random() < 0.7 else (pick("Baum", z) if z in "KGP" else pick("Schlacke", z))
        if name and fit(name, x, y, scale=float(rng.uniform(1.4, 2.6)) if PIECES[name]["art"] == "Fels" else None):
            edge_count += 1
print("[aschenmark_fuellung] edge pieces:", edge_count)

# ---------------------------------------------------------------- 3. road sides
side_count = 0
for road in pfade:
    pts = np.array(road["points"])
    if road["kind"] == "pfad":
        continue
    total = 0.0
    for a, b in zip(pts[:-1], pts[1:]):
        seg = np.linalg.norm(b - a)
        total += seg
        if rng.random() > seg / 9.0:
            continue
        d = (b - a) / max(seg, 1e-3)
        side = np.array([-d[1], d[0]]) * (1 if rng.random() < 0.5 else -1)
        off = {"haupt": 6.0, "neben": 5.0}[road["kind"]] + rng.uniform(0, 3)
        x, y = a + side * off
        z = zone_letter(mask_at(x, y)[2])
        roll_ = rng.random()
        kind = "Wegmarke" if roll_ < 0.06 else "Kiesel" if roll_ < 0.45 else "Ast" if roll_ < 0.7 else "Kraut" if roll_ < 0.88 else "Kram"
        name = pick(kind, z)
        if name and fit(name, float(x), float(y)):
            side_count += 1
print("[aschenmark_fuellung] road side pieces:", side_count)

# ---------------------------------------------------------------- 3b. fine scatter: dry weeds, pebbles, twigs about every 7 m
fine = 0
FINE = {"K": [("Kraut", 0.5), ("Kiesel", 0.25), ("Ast", 0.25)], "S": [("Kiesel", 0.6), ("Kraut", 0.2), ("Schlacke", 0.2)],
        "G": [("Kraut", 0.35), ("Kiesel", 0.35), ("Ast", 0.3)], "P": [("Kiesel", 0.45), ("Kraut", 0.45), ("Ast", 0.1)]}
for gx in np.arange(4, SIZE_X - 4, 5.0):
    for gy in np.arange(4, SIZE_Y - 4, 5.0):
        if rng.random() > 0.75:
            continue
        x, y = gx + rng.uniform(-3, 3), gy + rng.uniform(-3, 3)
        z = zone_letter(mask_at(x, y)[2])
        options = FINE[z]
        kind = options[int(rng.choice(len(options), p=np.array([o[1] for o in options])))][0]
        name = pick(kind, z)
        if name is None:
            continue
        lo, hi = PIECES[name]["skala"]
        scale = float(rng.uniform(0.25, 0.45)) if kind == "Schlacke" else float(rng.uniform(lo, hi)) * (1.6 if kind in ("Kraut", "Ast") else 1.0)
        if kind == "Kiesel" and max(PIECES[name]["groesse"]) < 60:
            scale *= 3.0
        if fit(name, x, y, scale=scale):
            fine += 1
print("[aschenmark_fuellung] fine scatter:", fine)

# ---------------------------------------------------------------- 4. finds every 100 to 150 m
HARVEST = {"K": ["Aschmoos", "Russbeere", "Glimmklee"], "S": ["Schlackenglas", "Russquarz", "Eisenkraut"],
           "G": ["Glutdistel", "Glutstein", "Aschblase"], "P": ["Russquarz", "Pfeifenmorchel", "Adernflechte"]}
FINDS = [("Lager", [("Zelt", 1, 1), ("Pfaehle", 1, 1), ("Kram", 3, 5)]),
         ("Wrack", [("Wrack", 1, 1), ("Kram", 1, 2), ("Wegmarke", 0, 1)]),
         ("Grab", [("Grab", 2, 3), ("Kram", 1, 1), ("Pfaehle", 0, 1)]),
         ("Gerippe", [("Gerippe", 1, 1), ("Pfaehle", 0, 1), ("Ast", 1, 2)]),
         ("Turmrest", [("Ruine", 3, 5), ("Platten", 0, 1), ("Kiesel", 1, 2)])]
finds = []
for gx in np.arange(30, SIZE_X - 30, 125.0):
    for gy in np.arange(30, SIZE_Y - 30, 125.0):
        x, y = gx + rng.uniform(-30, 30), gy + rng.uniform(-30, 30)
        if not free_ground(x, y, 8.0):
            continue
        z = zone_letter(mask_at(x, y)[2])
        kind, recipe = FINDS[int(rng.integers(len(FINDS)))]
        if kind == "Lager" and z == "G":
            kind, recipe = FINDS[3]
        n = scatter_group(x, y, z, recipe, 5.0)
        if n == 0:
            continue
        reward = rng.random()
        finds.append({"art": kind, "x": round(float(x), 2), "y": round(float(y), 2), "z": round(height(x, y), 2), "zone": z,
                      "truhe": bool(reward < 0.35), "ernte": HARVEST[z][int(rng.integers(3))] if reward > 0.55 else None,
                      "yaw": round(float(rng.uniform(0, 360)), 1)})
print("[aschenmark_fuellung] finds:", len(finds))

# ---------------------------------------------------------------- 5. atmosphere
smoke = []
for x, y, z, kind in group_centres:
    if kind in ("Glutfeld", "Schlacke") and rng.random() < (0.35 if kind == "Glutfeld" else 0.18) and len(smoke) < 30:
        smoke.append({"art": "Rauchsaeule", "x": round(float(x), 2), "y": round(float(y), 2), "z": round(height(x, y), 2)})
for p in placed:
    if PIECES[p[0]]["art"] == "Glutspalte" and rng.random() < 0.6 and len(smoke) < 90:
        smoke.append({"art": "Glutrauch", "x": p[1], "y": p[2], "z": round(p[3] + 0.1, 2)})
# Fog in hollows: ground clearly lower than its surroundings
hb = H.copy()
for _ in range(3):
    hb = (np.roll(hb, 12, 0) + np.roll(hb, -12, 0) + np.roll(hb, 12, 1) + np.roll(hb, -12, 1) + hb) / 5
hollow = hb - H
fog = []
for gx in np.arange(20, SIZE_X - 20, 30.0):
    for gy in np.arange(20, SIZE_Y - 20, 30.0):
        ix, iy = int(gx), int(gy)
        if hollow[ix, iy] > 1.6 and H[ix, iy] > 1.5 and len(fog) < 60:
            fog.append({"x": float(gx), "y": float(gy), "z": round(float(H[ix, iy]), 2), "groesse": round(float(min(30, 10 + hollow[ix, iy] * 4)), 1)})
print("[aschenmark_fuellung] smoke:", len(smoke), "fog:", len(fog))

out = {
    "info": "Füllung der Aschenmark (Tools/Welt/aschenmark_fuellung.py): teile = [name, x, y, z (Boden, m), yaw, Bodennormale x, y, z, skala]; "
            "z ist der Boden unter dem Teil, der Drehpunkt wird in Unreal nach den Mesh-Grenzen ausgeglichen",
    "meshes": {name: {"mesh": p["mesh"], "materials": p["materials"], "art": p["art"], "kollision": p["art"] not in NO_COLLISION}
               for name, p in PIECES.items()},
    "teile": placed, "fundorte": finds, "rauch": smoke, "nebel": fog,
}
with open(os.path.join(welt, "AM_Fuellung.json"), "w", encoding="utf-8") as f:
    json.dump(out, f, ensure_ascii=False, separators=(",", ":"))
print("[aschenmark_fuellung] pieces:", len(placed), stats)
