"""Moor water of the Pfuhlmoor (region II): the six lagoons of the sunken forest as a murky, overgrown oasis with a hint of danger.

Works from the height map (the lagoons are the water below 0 inside the lagoon ellipses of WF_Silhouette.json) and writes:

  T_WF_Moor.png     2048 x 2048 RGBA over the moor square MOOR_BOX (about 39 cm per pixel), read by M_WF_Moorwasser:
                    R  depth: 0 at the shore, 1 from about 14 m into the water, wobbly
                    G  duckweed and algae: patches in the bays and along the shore, none in the deep holes
                    B  deep holes: a few near black spots where the nixies will lurk (more ripples there)
                    A  water: the lagoons grown a few metres onto the land (the landscape hides that part), 0 elsewhere
  WF_Moor.json      the places for BuildMoorWasser.py, in world cm: lily pads, reed belts, bank plants, dead wood, mist, the lurking spots
                    and the circles of shallow water (wet, a water source for spells)

Kept clear: the trails and the campaign path at the shore, the log bridges, the Nid spring and the moss huts.

    <wf_generator_venv>/python wf_moor.py      (numpy, scipy, pillow; same seed = same moor)
"""
import json
import math
import os

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
SEED = 1313
CM_PER_PX = 200.0
HEIGHT_SCALE = 100.0 / 128.0
MAP_ORIGIN = -201600.0                 # world cm of the landscape corner
MASK_SIZE = 2048
MOOR_BOX = (330.0, 740.0, 440.0)       # map pixels: left, top, edge length (880 m)
SHORE_GROW_M = 4.0                     # water mask reaches this far onto the land, under the landscape
DEEP_M = 14.0                          # metres from the shore to full depth


def fbm(shape, scales, rng, weights=None):
    """Smooth noise from blurred white noise, sizes in pixels, normalised to 0..1"""
    total = np.zeros(shape, dtype=np.float32)
    weights = weights or [1.0] * len(scales)
    for scale, weight in zip(scales, weights):
        layer = ndimage.gaussian_filter(rng.standard_normal(shape).astype(np.float32), scale, mode="wrap")
        total += weight * layer / (layer.std() + 1e-6)
    total -= total.min()
    return total / (total.max() + 1e-6)


def polyline_distance(points, xs, ys):
    """Distance in the units of the points from each (xs, ys) to the polyline"""
    best = np.full(xs.shape, 1e9, dtype=np.float32)
    for (ax, ay), (bx, by) in zip(points, points[1:]):
        dx, dy = bx - ax, by - ay
        length2 = dx * dx + dy * dy + 1e-9
        t = np.clip(((xs - ax) * dx + (ys - ay) * dy) / length2, 0.0, 1.0)
        best = np.minimum(best, np.hypot(xs - (ax + t * dx), ys - (ay + t * dy)))
    return best


def main():
    rng = np.random.default_rng(SEED)
    data = json.load(open(os.path.join(HERE, "WF_Platzhalter.json"), encoding="utf-8"))
    silhouette = json.load(open(os.path.join(HERE, "WF_Silhouette.json"), encoding="utf-8"))
    ox, oy = data["OX"], data["OY"]
    heights = (np.asarray(Image.open(os.path.join(HERE, "WF_Hoehenkarte_2017.png")), dtype=np.float64) - 32768.0) * HEIGHT_SCALE

    left, top, edge = MOOR_BOX
    m_per_px = edge * CM_PER_PX / 100.0 / MASK_SIZE
    # Map pixel coordinates of every mask pixel
    ys, xs = np.mgrid[0:MASK_SIZE, 0:MASK_SIZE].astype(np.float32)
    map_x = left + (xs + 0.5) * edge / MASK_SIZE
    map_y = top + (ys + 0.5) * edge / MASK_SIZE
    h = ndimage.map_coordinates(heights.astype(np.float32), [map_y, map_x], order=1)

    # The lagoons: below 0, inside their ellipses grown a little; the shore made ragged by noise
    lagoons = [(cx + ox, cy + oy, rx, ry) for cx, cy, rx, ry in silhouette["lagunen_pfuhlmoor"]]
    near = np.zeros(h.shape, bool)
    for cx, cy, rx, ry in lagoons:
        near |= ((map_x - cx) / (rx * 1.35)) ** 2 + ((map_y - cy) / (ry * 1.35)) ** 2 < 1.0
    ragged = (fbm(h.shape, [4, 14], rng, [0.6, 1.0]) - 0.5) * 30.0
    wet = near & (h + ragged < 0.0)
    wet = ndimage.binary_opening(wet, iterations=2)
    inside_m = ndimage.distance_transform_edt(wet) * m_per_px
    cover = ndimage.binary_dilation(wet, iterations=int(SHORE_GROW_M / m_per_px))

    depth = np.clip(inside_m / DEEP_M, 0, 1)
    depth = np.clip(depth * (0.75 + 0.5 * fbm(h.shape, [25, 60], rng)), 0, 1)

    # Keep clear: the Nid spring (its own violet pool), the log bridges, trails and the campaign path
    nid = next(p for p in data["prims"] if p["n"] == "NidQuelle_Becken")
    nid_x, nid_y = nid["l"][0] / CM_PER_PX, nid["l"][1] / CM_PER_PX
    nid_m = np.hypot(map_x - nid_x, map_y - nid_y) * 2.0
    bridges = [p for p in data["prims"] if p["n"].startswith("Stammbruecke_")]
    huts = [p for p in data["prims"] if p["n"].startswith("Moos_Huette_") and "_Stelze_" not in p["n"]]
    trails = json.load(open(os.path.join(HERE, "WF_Pfade.json"), encoding="utf-8"))["pfade"]
    road = [(px + ox, py + oy) for px, py in silhouette["kampagnenweg"]]
    # On a coarse grid (about 3 m), then smoothly scaled up: plenty for keeping reeds off the ways
    coarse = MASK_SIZE // 8
    cy_, cx_ = np.mgrid[0:coarse, 0:coarse].astype(np.float32)
    cmx, cmy = left + (cx_ + 0.5) * edge / coarse, top + (cy_ + 0.5) * edge / coarse
    way_coarse = polyline_distance(road, cmx, cmy)
    for trail in trails:
        way_coarse = np.minimum(way_coarse, polyline_distance(trail["punkte"], cmx, cmy))
    way_m = ndimage.zoom(way_coarse, 8, order=1) * 2.0

    # Deep holes: one or two per lagoon where it is deepest, ragged blobs
    holes = np.zeros(h.shape, np.float32)
    hole_spots = []
    labels, count = ndimage.label(wet)
    for k in range(1, count + 1):
        part = labels == k
        area_m2 = part.sum() * m_per_px * m_per_px
        if area_m2 < 600:
            continue
        candidates = np.argwhere(part & (inside_m > 9) & (nid_m > 45))
        if len(candidates) == 0:
            continue
        for _ in range(1 if area_m2 < 5000 else 2):
            y, x = candidates[rng.integers(len(candidates))]
            if any(math.hypot(x - sx, y - sy) * m_per_px < 30 for sx, sy in hole_spots):
                continue
            hole_spots.append((x, y))
            radius_px = rng.uniform(5, 9) / m_per_px
            blob = np.exp(-(((xs - x) ** 2 + (ys - y) ** 2) / (radius_px ** 2)))
            holes = np.maximum(holes, blob)
    holes *= 0.6 + 0.8 * fbm(h.shape, [6], rng)
    holes = np.clip((holes - 0.25) / 0.5, 0, 1) * wet

    # Duckweed and algae: patchy, gathering in the shallows and bays, never on the deep holes or the Nid pool
    weed = fbm(h.shape, [5, 18, 45], rng, [0.5, 1.0, 0.8])
    # The shore gathers more, but only in stretches, so it never runs round as a ring
    shore_bias = np.clip(1.0 - inside_m / 10.0, 0, 1) * np.clip((fbm(h.shape, [35], rng) - 0.4) / 0.25, 0, 1)
    weed = np.clip((weed + 0.4 * shore_bias - 0.66) / 0.16, 0, 1)
    weed *= (1 - holes) * np.clip((nid_m - 30) / 15, 0, 1) * wet

    rgba = np.stack([depth, weed, holes, cover.astype(np.float32)], -1)
    Image.fromarray((np.clip(rgba, 0, 1) * 255).astype(np.uint8), "RGBA").save(os.path.join(HERE, "T_WF_Moor.png"))

    def world(x, y):
        """Mask pixel -> world cm, with the height of the ground there (under water: the lagoon floor)"""
        mx = left + (x + 0.5) * edge / MASK_SIZE
        my = top + (y + 0.5) * edge / MASK_SIZE
        z = float(h[min(int(y), MASK_SIZE - 1), min(int(x), MASK_SIZE - 1)])
        return [round(mx * CM_PER_PX + MAP_ORIGIN, 1), round(my * CM_PER_PX + MAP_ORIGIN, 1), round(z, 1)]

    def clear_of_things(x, y, metres):
        """Far enough from bridges and huts (world check in map pixels)"""
        mx = left + (x + 0.5) * edge / MASK_SIZE
        my = top + (y + 0.5) * edge / MASK_SIZE
        for p in bridges:
            bx, by = p["l"][0] / CM_PER_PX, p["l"][1] / CM_PER_PX
            yaw = math.radians(p["r"][1])
            # along the log (26 m long) and across it
            along = (mx - bx) * math.cos(yaw) + (my - by) * math.sin(yaw)
            across = -(mx - bx) * math.sin(yaw) + (my - by) * math.cos(yaw)
            if abs(along) * 2 < 15 + metres and abs(across) * 2 < 2 + metres:
                return False
        for p in huts:
            if math.hypot(mx - p["l"][0] / CM_PER_PX, my - p["l"][1] / CM_PER_PX) * 2 < 10 + metres:
                return False
        return True

    py_rng = np.random.default_rng(SEED + 1)

    # Lily pads in loose rafts: open water between 1.5 m and the deep middle, not on the holes or near the Nid pool
    lilies = []
    lily_ok = wet & (inside_m > 1.5) & (holes < 0.1) & (nid_m > 40) & (weed < 0.7)
    spots = np.argwhere(lily_ok)
    rafts = 0
    for _ in range(140):
        y, x = spots[py_rng.integers(len(spots))]
        if not clear_of_things(x, y, 4):
            continue
        rafts += 1
        raft_r = py_rng.uniform(2.5, 8.0) / m_per_px
        for _ in range(int(py_rng.integers(4, 22))):
            a = py_rng.uniform(0, 2 * math.pi)
            r = math.sqrt(py_rng.random()) * raft_r
            px, py = int(x + math.cos(a) * r), int(y + math.sin(a) * r)
            if not (0 <= px < MASK_SIZE and 0 <= py < MASK_SIZE) or not lily_ok[py, px]:
                continue
            lilies.append(world(px, py) + [round(float(py_rng.uniform(1.0, 2.0)), 2), round(float(py_rng.uniform(0, 360)), 1),
                                           int(py_rng.integers(0, 1000))])

    # Reed belts along the shore with gaps, out of the way of trails and the path
    shore = wet & (inside_m < 2.5) | (cover & ~wet & (ndimage.distance_transform_edt(~wet) * m_per_px < 1.5))
    belt = fbm(h.shape, [30, 80], rng) > 0.45
    reeds = []
    reed_spots = np.argwhere(shore & belt & (way_m > 5.0) & (nid_m > 25))
    py_rng.shuffle(reed_spots)
    for y, x in reed_spots[:2600]:
        if not clear_of_things(x, y, 2):
            continue
        tall = py_rng.random() < 0.06
        reeds.append(world(x, y) + [round(float(py_rng.uniform(0.8, 1.4) if tall else py_rng.uniform(150, 260)), 2),
                                    round(float(py_rng.uniform(0, 360)), 1), "megaplant" if tall else "halme"])

    # Banks: Matted Pratia creeping over the wet ground right at the water, burdock clumps a little further up, both in patches
    outside_m = ndimage.distance_transform_edt(~wet) * m_per_px
    patches = fbm(h.shape, [12, 40], rng)
    bank = []
    pratia_spots = np.argwhere(~wet & (outside_m > 0.3) & (outside_m < 6) & (patches > 0.35) & (way_m > 2.5) & (nid_m > 20) & near)
    py_rng.shuffle(pratia_spots)
    for y, x in pratia_spots[:4000]:
        if clear_of_things(x, y, 0):
            bank.append(world(x, y) + [round(float(py_rng.uniform(2.0, 4.5)), 2), round(float(py_rng.uniform(0, 360)), 1), "pratia"])
    burdock_spots = np.argwhere(~wet & (outside_m > 2) & (outside_m < 10) & (patches > 0.55) & (way_m > 3.5) & (nid_m > 20) & near)
    py_rng.shuffle(burdock_spots)
    for y, x in burdock_spots[:700]:
        if clear_of_things(x, y, 1):
            bank.append(world(x, y) + [round(float(py_rng.uniform(0.9, 1.8)), 2), round(float(py_rng.uniform(0, 360)), 1), "klette"])

    # Dead wood: dead pines standing in the water, fallen trunks half sunk across the shore, stumps and roots
    deadwood = []
    for k in range(1, count + 1):
        part = labels == k
        area_m2 = part.sum() * m_per_px * m_per_px
        if area_m2 < 600:
            continue
        standing = np.argwhere(part & (inside_m > 4) & (inside_m < 16) & (holes < 0.1) & (nid_m > 30))
        across = np.argwhere(part & (inside_m < 2) & (way_m > 8))
        middle = np.argwhere(part & (inside_m > 2) & (holes < 0.2) & (nid_m > 30))
        plan = (("toter_baum", standing, max(1, int(area_m2 / 6000))), ("liegender_stamm", across, max(1, int(area_m2 / 5000))),
                ("stumpf", middle, max(2, int(area_m2 / 2500))), ("wurzeln", across, max(1, int(area_m2 / 4000))))
        for kind, pool, number in plan:
            for _ in range(number):
                if len(pool) == 0:
                    break
                y, x = pool[py_rng.integers(len(pool))]
                if not clear_of_things(x, y, 4 if kind in ("toter_baum", "liegender_stamm") else 1):
                    continue
                # Fallen trunks point from the land into the water
                gy, gx = np.gradient(inside_m[max(y - 3, 0):y + 4, max(x - 3, 0):x + 4])
                into_water = math.degrees(math.atan2(gy.mean(), gx.mean())) if kind == "liegender_stamm" else float(py_rng.uniform(0, 360))
                deadwood.append({"art": kind, "ort": world(x, y), "drehung": round(into_water + float(py_rng.uniform(-25, 25)), 1),
                                 "zufall": round(float(py_rng.random()), 3)})

    # Lurking spots: the holes, plus a few lone spots in deeper water
    lurk = [world(x, y) for x, y in hole_spots]
    deep_spots = np.argwhere(wet & (inside_m > 6) & (nid_m > 40))
    for _ in range(10):
        y, x = deep_spots[py_rng.integers(len(deep_spots))]
        lurk.append(world(x, y))

    # Mist over every lagoon, as flat as a lying sheet
    mist = []
    for k in range(1, count + 1):
        part = np.argwhere(labels == k)
        if len(part) * m_per_px * m_per_px < 600:
            continue
        y0, x0 = part.min(0)
        y1, x1 = part.max(0)
        mist.append({"ort": world((x0 + x1) / 2, (y0 + y1) / 2), "groesse_cm": [round((x1 - x0) * m_per_px * 100, 0), round((y1 - y0) * m_per_px * 100, 0)]})

    # Shallow water: circles on a hex grid inside the water (radius 18 m, 26 m apart)
    wet_circles = []
    step = 26.0 / m_per_px
    for row, y in enumerate(np.arange(step / 2, MASK_SIZE, step * 0.866)):
        for x in np.arange(step / 2 + (row % 2) * step / 2, MASK_SIZE, step):
            iy, ix = int(y), int(x)
            window = wet[max(iy - 20, 0):iy + 21, max(ix - 20, 0):ix + 21]
            if window.mean() > 0.35 and nid_m[iy, ix] > 25:
                wet_circles.append(world(ix, iy))

    out = {
        "_hinweis": "Moorwasser im Pfuhlmoor, Orte in Welt-cm (x, y, Bodenhoehe). Erzeugt von wf_moor.py, gelesen von Scripts/BuildMoorWasser.py",
        "maske": {"ursprung_cm": [left * CM_PER_PX + MAP_ORIGIN, top * CM_PER_PX + MAP_ORIGIN], "groesse_cm": edge * CM_PER_PX},
        "wasserspiegel_cm": 0.0,
        "seerosen": lilies, "schilf": reeds, "ufer": bank, "totholz": deadwood, "lauerstellen": lurk, "nebel": mist,
        "seichtwasser_radius_cm": 1800.0, "seichtwasser": wet_circles,
    }
    with open(os.path.join(HERE, "WF_Moor.json"), "w", encoding="utf-8") as f:
        json.dump(out, f, indent=0, ensure_ascii=False)

    # Preview for checking by eye
    preview = np.zeros((MASK_SIZE, MASK_SIZE, 3), np.float32)
    preview[:] = (90, 80, 60)
    water = np.array([40, 70, 60]) * (1 - depth[..., None]) + np.array([8, 22, 22]) * depth[..., None]
    preview[wet] = water[wet]
    preview = preview * (1 - weed[..., None]) + np.array([90, 140, 30]) * weed[..., None]
    preview = preview * (1 - holes[..., None]) + np.array([0, 0, 0]) * holes[..., None]
    image = Image.fromarray(preview.astype(np.uint8)).resize((1024, 1024))
    draw = ImageDraw.Draw(image)
    def to_px(w):
        return ((w[0] - MAP_ORIGIN) / CM_PER_PX - left) / edge * 1024, ((w[1] - MAP_ORIGIN) / CM_PER_PX - top) / edge * 1024
    for lily in lilies:
        x, y = to_px(lily)
        draw.point((x, y), fill=(120, 200, 80))
    for reed in reeds:
        x, y = to_px(reed)
        draw.point((x, y), fill=(200, 170, 100))
    for wood in deadwood:
        x, y = to_px(wood["ort"])
        draw.rectangle((x - 2, y - 2, x + 2, y + 2), fill=(60, 30, 10))
    for spot in lurk:
        x, y = to_px(spot)
        draw.ellipse((x - 3, y - 3, x + 3, y + 3), outline=(230, 240, 80))
    image.save(os.path.join(HERE, "..", "..", "Saved", "WF_Moor_Vorschau.png"))
    print("moor: {} lagoon parts, {} lily pads in {} rafts, {} reeds, {} bank plants, {} dead wood, {} lurking spots, {} shallow circles".format(
        count, len(lilies), rafts, len(reeds), len(bank), len(deadwood), len(lurk), len(wet_circles)))


if __name__ == "__main__":
    main()
