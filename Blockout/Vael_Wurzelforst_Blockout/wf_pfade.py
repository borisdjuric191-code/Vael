"""Small trails and thickets of the Wurzelforst (region II), next to the campaign path.

Finds narrow, winding forest trails from the campaign path to the places off it (fairy caves, fairy ring, egg cave,
moss huts, log bridges, Urstamm, Nid spring ...) over the height map, around the grey box trunks and rocks and
out of the water. Trails that come close join each other, so they form a small network. Writes:

  WF_Pfade.json        trails as polylines in map pixels (1 px = 2 m), for the scatter script; "hauptweg": the campaign path
  T_WF_Pfade.png       4096 x 4096 grey: distance to the nearest trail centre line, 0..PFAD_REICHWEITE_M (as 0..255)
  T_WF_Hauptweg.png    4096 x 4096 grey: the same for the campaign path, which winds gently round its straight route
                       (Boris 2026-10-28: root floor like the trails, imperfect, not too wide, ferns and moss beside it)
  T_WF_Dickicht.png    2048 x 2048 grey: thicket density 0..1 around trunks and along the trail and path edges

Both images cover the landscape like T_WF_Zonenkarte.png. DressWurzelforst.py imports them and lays the root moss
ground on the trails and the dense undergrowth where the thicket map is bright.

    <wf_generator_venv>/python wf_pfade.py      (numpy, scipy, pillow; same seed = same trails)
"""
import heapq
import json
import math
import os

import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
SEED = 2017
GRID = 2                  # the router works on every 2nd map pixel (4 m)
PFAD_REICHWEITE_M = 10.0  # distance range stored in T_WF_Pfade.png
PFAD_SIZE = 4096
WANDER_SPACING = 75.0    # map pixels (150 m) between the ends of the wandering trails
DICKICHT_SIZE = 2048
CM_PER_PX = 200.0
HEIGHT_SCALE = 100.0 / 128.0

ZONE_COLORS = {
    "saum": "6e6448", "moor": "3e6b55", "hain": "3f7f45", "kamm": "3b6670", "grat": "6a6d60", "asche": "7a4a2a",
    "jaw": "4a5f78", "grat_hoch": "9a9c90", "lagune": "2f6f74", "nid": "5a3a7a", "weg": "b9a77a",
}
FOREST = ("hain", "saum", "moor", "kamm", "weg", "nid")

# Grey box parts the trails go around (name parts), and how far around in metres
OBSTACLES = ("_Stamm", "_Stiel", "_Brettwurzel_", "_Stelze_", "Urstamm_Wurzel_", "Felsbrocken", "Felsturm_", "_Huegel",
             "Eihoehle_Fels", "Fang_", "NidQuelle_Kern", "Quellwaechter")


def zone_map():
    rgb = np.asarray(Image.open(os.path.join(HERE, "T_WF_Zonenkarte.png")).convert("RGB"), dtype=np.float32)
    names = list(ZONE_COLORS)
    best = np.full(rgb.shape[:2], 1e12, dtype=np.float32)
    index = np.zeros(rgb.shape[:2], dtype=np.int32)
    for k, name in enumerate(names):
        color = np.array([int(ZONE_COLORS[name][i:i + 2], 16) for i in (0, 2, 4)], dtype=np.float32)
        # The map is hill-shaded, so each colour is compared at its best brightness (as in ScatterWurzelforst.py)
        scale = np.clip((rgb @ color) / float(color @ color), 0.6, 1.3)
        error = ((rgb - scale[..., None] * color) ** 2).sum(-1)
        better = error < best
        best[better] = error[better]
        index[better] = k
    return index, names


def footprint(prim):
    """Corners of a prim's ground footprint in map pixels"""
    x, y = prim["l"][0] / CM_PER_PX, prim["l"][1] / CM_PER_PX
    sx, sy = prim["s"][0] * 50 / CM_PER_PX, prim["s"][1] * 50 / CM_PER_PX
    yaw = math.radians(prim["r"][1])
    c, s = math.cos(yaw), math.sin(yaw)
    return [(x + a * c - b * s, y + a * s + b * c) for a, b in ((-sx, -sy), (sx, -sy), (sx, sy), (-sx, sy))]


def fill_polygon(mask, corners, grow):
    """Sets the cells (router grid) inside the polygon, grown by `grow` cells"""
    pts = [(px / GRID, py / GRID) for px, py in corners]
    lo_x, hi_x = int(min(p[0] for p in pts) - grow - 1), int(max(p[0] for p in pts) + grow + 2)
    lo_y, hi_y = int(min(p[1] for p in pts) - grow - 1), int(max(p[1] for p in pts) + grow + 2)
    cx, cy = sum(p[0] for p in pts) / 4, sum(p[1] for p in pts) / 4
    for j in range(max(lo_x, 0), min(hi_x, mask.shape[1])):
        for i in range(max(lo_y, 0), min(hi_y, mask.shape[0])):
            # inside the polygon grown outward from its centre
            px, py = j - cx, i - cy
            dist = math.hypot(px, py)
            q = (cx + px * max(0.0, dist - grow) / max(dist, 1e-6), cy + py * max(0.0, dist - grow) / max(dist, 1e-6))
            inside = True
            for (ax, ay), (bx, by) in zip(pts, pts[1:] + pts[:1]):
                if (bx - ax) * (q[1] - ay) - (by - ay) * (q[0] - ax) < 0:
                    inside = False
                    break
            if inside:
                mask[i, j] = True


def chaikin(points, rounds=3):
    for _ in range(rounds):
        out = [points[0]]
        for a, b in zip(points, points[1:]):
            out.append((a[0] * 0.75 + b[0] * 0.25, a[1] * 0.75 + b[1] * 0.25))
            out.append((a[0] * 0.25 + b[0] * 0.75, a[1] * 0.25 + b[1] * 0.75))
        out.append(points[-1])
        points = out
    return points


def main_path(route, rng):
    """The campaign path as a gently winding line: the straight route of the silhouette, rounded, swinging up to
    about 7 m to either side on long and short waves (the landscape is flattened 28 m round the route, so it stays
    on level ground), in map pixels"""
    dense = []
    for (ax, ay), (bx, by) in zip(route, route[1:]):
        steps = max(1, int(math.hypot(bx - ax, by - ay) / 4.0))
        dense += [(ax + (bx - ax) * k / steps, ay + (by - ay) * k / steps) for k in range(steps)]
    dense.append(route[-1])
    points = chaikin(dense, 3)
    phases = rng.uniform(0, 6.28, 3)
    run = 0.0
    out = [points[0]]
    for a, b in zip(points, points[1:]):
        dx, dy = b[0] - a[0], b[1] - a[1]
        length = math.hypot(dx, dy) or 1e-6
        run += length
        # Waves of ~70 m, ~28 m and ~11 m (in px of 2 m), faded in and out at both ends so it meets the region's gates
        fade = min(1.0, run / 20.0)
        amp = fade * (2.4 * math.sin(run / 5.6 + phases[0]) + 0.9 * math.sin(run / 2.2 + phases[1]) + 0.3 * math.sin(run / 0.9 + phases[2]))
        out.append((b[0] - dy / length * amp, b[1] + dx / length * amp))
    total = run
    run = 0.0
    for k in range(len(out) - 1, 0, -1):
        run += math.hypot(out[k][0] - out[k - 1][0], out[k][1] - out[k - 1][1])
        if run > 20.0:
            break
        # Back onto the route at the far end
        t = run / 20.0
        out[k] = (points[k][0] + (out[k][0] - points[k][0]) * t, points[k][1] + (out[k][1] - points[k][1]) * t)
    print("campaign path: {:.0f} m, winding".format(total * 2))
    return out


def main():
    rng = np.random.default_rng(SEED)
    data = json.load(open(os.path.join(HERE, "WF_Platzhalter.json"), encoding="utf-8"))
    silhouette = json.load(open(os.path.join(HERE, "WF_Silhouette.json"), encoding="utf-8"))
    n = data["N"]

    heights = (np.asarray(Image.open(os.path.join(HERE, "WF_Hoehenkarte_2017.png")), dtype=np.float64) - 32768.0) * HEIGHT_SCALE
    zones, zone_names = zone_map()
    forest = np.isin(zones, [zone_names.index(z) for z in FOREST])

    # Router grid
    h = heights[::GRID, ::GRID]
    gn = h.shape[0]
    gy, gx = np.gradient(h, GRID * CM_PER_PX)
    slope = np.hypot(gx, gy)
    cost = 1.0 + 30.0 * np.clip(slope - 0.12, 0, None) ** 1.5
    cost[slope > 0.55] += 400.0
    cost[h < 40.0] += 400.0                       # no trails through water
    cost[~forest[::GRID, ::GRID]] += 6.0
    # Smooth random hills in the cost: the trails wander instead of running straight
    wander = ndimage.gaussian_filter(rng.random((gn, gn)), 4.0) + 0.6 * ndimage.gaussian_filter(rng.random((gn, gn)), 10.0)
    wander = (wander - wander.min()) / max(1e-9, wander.max() - wander.min())
    cost += 6.0 * wander ** 2

    blocked = np.zeros((gn, gn), dtype=bool)
    for prim in data["prims"]:
        if any(part in prim["n"] for part in OBSTACLES):
            fill_polygon(blocked, footprint(prim), 0.6)
    cost[blocked] += 1000.0
    near_block = ndimage.binary_dilation(blocked, iterations=1) & ~blocked
    cost[near_block] += 1.5

    # The campaign path is where trails start
    route = [(px + data["OX"], py + data["OY"]) for px, py in silhouette["kampagnenweg"]]
    goal = np.zeros((gn, gn), dtype=bool)
    for (ax, ay), (bx, by) in zip(route, route[1:]):
        steps = int(math.hypot(bx - ax, by - ay) / GRID * 2) + 1
        for t in np.linspace(0, 1, steps):
            goal[int((ay + (by - ay) * t) / GRID), int((ax + (bx - ax) * t) / GRID)] = True
    goal = ndimage.binary_dilation(goal, iterations=2)

    # Places off the path, in map pixels; the trail ends where it reaches `stop` pixels from the place
    by_name = {p["n"]: p for p in data["prims"]}

    def at(name):
        p = by_name[name]
        return p["l"][0] / CM_PER_PX, p["l"][1] / CM_PER_PX

    places = [(at("Urstamm_Stamm"), 15.0, "Urstamm")]
    places += [(at("Feenhoehle_{}_Eingang".format(i)), 2.0, "Feenhoehle") for i in range(1, 6) if "Feenhoehle_{}_Eingang".format(i) in by_name]
    places += [(at("Moos_Huette_{}".format(i)), 3.0, "Moos-Huette") for i in range(1, 5) if "Moos_Huette_{}".format(i) in by_name]
    for name, stop, label in (("Feenkreis_Boden", 6.0, "Feenkreis"), ("Eihoehle_Teich", 7.0, "Eihoehle"),
                              ("Pilzhoehle_Eingang", 2.0, "Pilzhoehle"),
                              ("Drachensicht_Plattform", 3.0, "Drachensicht")):
        if name in by_name:
            places.append((at(name), stop, label))
    # Both ends of every log bridge across the moor
    for name, p in by_name.items():
        if name.startswith("Stammbruecke_"):
            x, y = p["l"][0] / CM_PER_PX, p["l"][1] / CM_PER_PX
            half = p["s"][0] * 50 / CM_PER_PX
            yaw = math.radians(p["r"][1])
            for side in (-1, 1):
                places.append(((x + math.cos(yaw) * half * side, y + math.sin(yaw) * half * side), 1.0, "Stammbruecke"))
    for i in range(1, 13):
        if "Farnfeld_{}".format(i) in by_name and i % 3 == 0:
            places.append((at("Farnfeld_{}".format(i)), 4.0, "Farnfeld"))
    # Sort: nearest to the path first, so the far ones can join the trails already laid
    gpts = np.argwhere(goal)

    def path_gap(point):
        return float(np.min(np.hypot(gpts[:, 1] * GRID - point[0], gpts[:, 0] * GRID - point[1])))

    places.sort(key=lambda item: path_gap(item[0]))

    # Wandering trails deep into the forest: spots spread over the dry, flat forest, each led to the nearest trail,
    # so the network branches out like game trails. Laid after the places, farthest from the path last.
    walkable = (forest[::GRID, ::GRID] & (h > 60.0) & (slope < 0.3) & ~ndimage.binary_dilation(blocked, iterations=3)
                & (zones[::GRID, ::GRID] != zone_names.index("weg")) & (zones[::GRID, ::GRID] != zone_names.index("nid")))
    candidates = np.argwhere(walkable)
    rng.shuffle(candidates)
    spots = []
    for i, j in candidates:
        x, y = j * GRID, i * GRID
        if all(math.hypot(x - a, y - b) > WANDER_SPACING for a, b in spots) and                 all(math.hypot(x - p[0][0], y - p[0][1]) > WANDER_SPACING * 0.6 for p in places):
            spots.append((x, y))
    spots.sort(key=path_gap)
    places += [((x, y), 0.0, "Wildpfad") for x, y in spots]

    trail_cells = np.zeros((gn, gn), dtype=bool)
    trails = []
    # 16 directions (with the knight's moves), so the trails don't run only along the grid and its diagonals
    steps8 = [(dx, dy, math.hypot(dx, dy)) for dx in (-2, -1, 0, 1, 2) for dy in (-2, -1, 0, 1, 2)
              if (dx, dy) != (0, 0) and math.gcd(abs(dx), abs(dy)) == 1]
    for (px, py), stop, label in places:
        sx, sy = int(px / GRID), int(py / GRID)
        if not (0 <= sx < gn and 0 <= sy < gn):
            continue
        # Start just outside the place (blocked cells around it are free for the first metres)
        free = np.zeros((gn, gn), dtype=bool)
        yy, xx = np.ogrid[:gn, :gn]
        free |= np.hypot(xx - sx, yy - sy) <= (max(stop, 10.0) / GRID + 1)
        local = cost.copy()
        local[free & blocked] = 1.0
        local[trail_cells] = np.minimum(local[trail_cells], 0.45)  # joining an existing trail is cheap
        targets = goal | trail_cells
        # Dijkstra from the place to the nearest path or trail cell
        dist = {(sx, sy): 0.0}
        back = {}
        heap = [(0.0, sx, sy)]
        end = None
        while heap:
            d, x, y = heapq.heappop(heap)
            if d > dist.get((x, y), 1e18):
                continue
            if targets[y, x] and (x, y) != (sx, sy):
                end = (x, y)
                break
            if d > 6000:
                break
            for dx, dy, length in steps8:
                nx, ny = x + dx, y + dy
                if not (0 <= nx < gn and 0 <= ny < gn):
                    continue
                mid = local[(y + ny) // 2, (x + nx) // 2]
                nd = d + length * (local[y, x] + local[ny, nx] + 2 * mid) * 0.25
                if nd < dist.get((nx, ny), 1e18):
                    dist[(nx, ny)] = nd
                    back[(nx, ny)] = (x, y)
                    heapq.heappush(heap, (nd, nx, ny))
        if end is None:
            print("no trail to", label, px, py)
            continue
        cells = [end]
        while cells[-1] != (sx, sy):
            a, b = cells[-1], back[cells[-1]]
            if abs(a[0] - b[0]) == 2 or abs(a[1] - b[1]) == 2:
                cells.append(((a[0] + b[0]) // 2 if abs(a[0] - b[0]) == 2 else a[0] if abs(a[1] - b[1]) != 2 else b[0],
                              (a[1] + b[1]) // 2 if abs(a[1] - b[1]) == 2 else a[1] if abs(a[0] - b[0]) != 2 else b[1]))
            cells.append(b)
        # cut the part inside the place's stop radius, keep the end a little past the join
        cells = [c for c in cells if math.hypot(c[0] - sx, c[1] - sy) * GRID >= stop] or cells[:1]
        for x, y in cells:
            trail_cells[y, x] = True
        # In map pixels, smoothed, with a small wobble across (seen up close the trail is never straight)
        points = chaikin([(x * GRID + 0.5, y * GRID + 0.5) for x, y in cells[::2] + [cells[-1]]], 3)
        phase, wave = rng.uniform(0, 6.28), rng.uniform(3.0, 5.0)
        run = 0.0
        wobbled = [points[0]]
        for a, b in zip(points, points[1:]):
            dx, dy = b[0] - a[0], b[1] - a[1]
            length = math.hypot(dx, dy) or 1e-6
            run += length
            amp = 0.7 * math.sin(run / wave + phase) + 0.25 * math.sin(run / (wave * 0.37) + phase * 2.1)
            wobbled.append((b[0] - dy / length * amp, b[1] + dx / length * amp))
        trails.append({"ziel": label, "punkte": [[round(x, 2), round(y, 2)] for x, y in wobbled],
                       "breite_m": round(float(rng.uniform(1.1, 1.6)), 2)})

    # Distance to the nearest centre line on the 4096 image (1 px = MAP / 4096)
    scale = PFAD_SIZE / float(n)
    px_m = n * CM_PER_PX / 100.0 / PFAD_SIZE

    def distance_image(lines, file_name):
        canvas = np.ones((PFAD_SIZE, PFAD_SIZE), dtype=bool)
        for pts in lines:
            for (ax, ay), (bx, by) in zip(pts, pts[1:]):
                steps = int(math.hypot(bx - ax, by - ay) * scale * 3) + 1
                for t in np.linspace(0, 1, steps):
                    canvas[int((ay + (by - ay) * t) * scale), int((ax + (bx - ax) * t) * scale)] = False
        result = ndimage.distance_transform_edt(canvas) * px_m
        Image.fromarray(np.clip(result / PFAD_REICHWEITE_M * 255.0 + 0.5, 0, 255).astype(np.uint8), "L").save(os.path.join(HERE, file_name))
        return result

    distance_m = distance_image([trail["punkte"] for trail in trails], "T_WF_Pfade.png")
    main_line = main_path(route, np.random.default_rng(SEED + 1))
    main_m = distance_image([main_line], "T_WF_Hauptweg.png")

    # Thicket: rings of undergrowth around trunks, mushroom stems and the Urstamm, plus a band along the trail edges,
    # broken into patches; never on the trails themselves
    tscale = DICKICHT_SIZE / float(n)
    trunks = np.zeros((DICKICHT_SIZE, DICKICHT_SIZE), dtype=bool)
    for prim in data["prims"]:
        name = prim["n"]
        if name.endswith(("_Stamm", "_Stiel")) or "_Brettwurzel_" in name or "Urstamm_Wurzel_" in name or name.startswith("Wurzelbogen_"):
            for x, y in footprint(prim) + [(prim["l"][0] / CM_PER_PX, prim["l"][1] / CM_PER_PX)]:
                i, j = int(y * tscale), int(x * tscale)
                if 0 <= i < DICKICHT_SIZE and 0 <= j < DICKICHT_SIZE:
                    trunks[i, j] = True
    tpx_m = n * CM_PER_PX / 100.0 / DICKICHT_SIZE
    trunk_m = ndimage.distance_transform_edt(~trunks) * tpx_m
    ring = np.clip(1.0 - (trunk_m - 4.0) / 10.0, 0, 1)           # full up to 4 m out, gone at 14 m
    # Around the Urstamm far wider: its Tripo roots reach 25 m out
    ux, uy = at("Urstamm_Stamm")
    yy, xx = np.mgrid[:DICKICHT_SIZE, :DICKICHT_SIZE]
    urstamm_m = np.hypot(xx / tscale - ux, yy / tscale - uy) * CM_PER_PX / 100.0
    ring = np.maximum(ring, np.clip(1.0 - (urstamm_m - 32.0) / 14.0, 0, 1))
    trail_small = np.asarray(Image.fromarray(distance_m.astype(np.float32)).resize((DICKICHT_SIZE, DICKICHT_SIZE), Image.BILINEAR))
    edge = np.clip(1.0 - np.abs(trail_small - 5.0) / 1.8, 0, 1) * 0.7  # 3..7 m beside the trail centre
    # Ferns crowd the campaign path more: a band 3.5..8 m beside its centre, never on it
    main_small = np.asarray(Image.fromarray(main_m.astype(np.float32)).resize((DICKICHT_SIZE, DICKICHT_SIZE), Image.BILINEAR))
    edge = np.maximum(edge, np.clip(1.0 - np.abs(main_small - 5.5) / 2.5, 0, 1) * 0.9)
    patches = ndimage.gaussian_filter(rng.random((DICKICHT_SIZE, DICKICHT_SIZE)), 3.0)
    patches = np.clip((patches - patches.mean()) / (patches.std() * 2.0) + 0.75, 0, 1)
    thicket = np.clip(np.maximum(ring, edge) * patches, 0, 1)
    thicket[trail_small < 3.0] = 0.0
    thicket[main_small < 3.5] = 0.0
    zones_small = np.asarray(Image.fromarray(forest.astype(np.uint8) * 255).resize((DICKICHT_SIZE, DICKICHT_SIZE), Image.BILINEAR)) / 255.0
    thicket *= zones_small
    Image.fromarray((thicket * 255.0 + 0.5).astype(np.uint8), "L").save(os.path.join(HERE, "T_WF_Dickicht.png"))

    with open(os.path.join(HERE, "WF_Pfade.json"), "w", encoding="utf-8") as f:
        json.dump({"_hinweis": "Kleine Pfade, Punkte in Kartenpixeln (1 px = 2 m), wie WF_Silhouette.json plus OX/OY schon eingerechnet",
                   "pfade": trails, "hauptweg": [[round(x, 2), round(y, 2)] for x, y in main_line]}, f, indent=1)
    total = sum(sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(t["punkte"], t["punkte"][1:])) for t in trails) * 2
    print("{} trails, {:.1f} km; thicket covers {:.1f} % of the map".format(len(trails), total / 1000, (thicket > 0.3).mean() * 100))

    # Preview over the zone map for a quick look
    preview = Image.open(os.path.join(HERE, "T_WF_Zonenkarte.png")).convert("RGB").resize((PFAD_SIZE // 2, PFAD_SIZE // 2))
    pv = np.asarray(preview).astype(np.float32)
    on_trail = np.asarray(Image.fromarray(np.minimum(distance_m, main_m).astype(np.float32)).resize((PFAD_SIZE // 2, PFAD_SIZE // 2))) < 1.6
    th = np.asarray(Image.fromarray(thicket.astype(np.float32)).resize((PFAD_SIZE // 2, PFAD_SIZE // 2)))
    pv = pv * (1 - th[..., None] * 0.6) + np.array([30, 140, 40]) * th[..., None] * 0.6
    pv[on_trail] = [255, 230, 60]
    Image.fromarray(pv.astype(np.uint8)).save(os.path.join(HERE, "..", "..", "Saved", "WF_Pfade_Vorschau.png") if os.path.isdir(
        os.path.join(HERE, "..", "..", "Saved")) else os.path.join(HERE, "WF_Pfade_Vorschau.png"))


if __name__ == "__main__":
    main()
