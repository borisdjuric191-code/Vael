"""
Feuer-Baukasten: makes the picture of the burning roots a ground fire spreads along (Scripts/Data/Feuer/T_Feuer_Adern.png).
From the middle, a star of thick roots grows outwards in winding steps, with a few short thick side arms.
  R  the veins: 1 on the middle line of a vein, falling off over its width
  G  when the fire gets there: the way along the roots from the middle to the nearest vein, 0 in the middle, 1 at the far tips
  B  the charred halo around the veins
  A  heat: 1 on the thick trunks near the middle, 0 at the thin tips
The material (M_Feuer_Brandadern) lets the veins appear up to its Wachstum, so the fire creeps outwards along them.
  The arms are thick and uneven, a star from the middle outwards; the middle itself is one burning knot.
Run: blender -b -P feuer_adern.py -- <output folder>
"""
import bpy, sys, os, math
import numpy as np

OUT = sys.argv[sys.argv.index("--") + 1]
SIZE = 512
rng = np.random.default_rng(41)

STEP = 0.022
segments = []  # x0, y0, x1, y1, thickness, way at the start, way at the end


def grow(x, y, angle, thickness, way, length, depth):
    """One thick arm of the star: winds a little, tapers from its root to its tip, now and then a short thick side arm"""
    turn = 0.0
    start_way = way
    start_thickness = thickness
    while way - start_way < length and math.hypot(x, y) < 0.95:
        # It keeps turning a little the way it turned before, but stays on its way outwards
        turn = turn * 0.75 + rng.normal(0.0, 0.16)
        angle += turn * 0.4
        outwards = math.atan2(y, x)
        angle += math.atan2(math.sin(outwards - angle), math.cos(outwards - angle)) * 0.12
        nx, ny = x + math.cos(angle) * STEP, y + math.sin(angle) * STEP
        # Thick and uneven: swells and narrows along the way, thins out only towards the tip
        along = (way - start_way) / length
        swell = 1.0 + 0.25 * math.sin(way * 23.0 + depth * 1.7) * rng.uniform(0.6, 1.0)
        thickness = start_thickness * (1.0 - along) ** 0.55 * swell + 0.006
        segments.append((x, y, nx, ny, thickness, way, way + STEP))
        x, y, way = nx, ny, way + STEP
        if depth < 2 and along > 0.12 and rng.random() < 0.08:
            side = 1.0 if rng.random() < 0.5 else -1.0
            remaining = length - (way - start_way)
            grow(x, y, angle + side * rng.uniform(0.5, 0.9), thickness * 0.7, way, remaining * rng.uniform(0.35, 0.7), depth + 1)


# A star of uneven arms: some long, some short, all thick at the middle
arms = 6
for index in range(arms):
    start_angle = index * math.tau / arms + rng.uniform(-0.35, 0.35)
    grow(0.0, 0.0, start_angle, rng.uniform(0.045, 0.07), 0.0, rng.uniform(0.6, 0.95), 0)

seg = np.array(segments, dtype=np.float32)
longest = seg[:, 6].max()
print("FEUER", len(seg), "segments, longest way", longest)

coords = (np.arange(SIZE, dtype=np.float32) + 0.5) / SIZE * 2.0 - 1.0
px, py = np.meshgrid(coords, coords)
best = np.full((SIZE, SIZE), 9.0, dtype=np.float32)      # distance to the edge of the nearest vein, in vein widths
nearest = np.full((SIZE, SIZE), 9.0, dtype=np.float32)   # plain distance to the nearest vein
arrival = np.ones((SIZE, SIZE), dtype=np.float32)
heat = np.zeros((SIZE, SIZE), dtype=np.float32)
first_thickness = seg[:, 4].max()

for x0, y0, x1, y1, thick, w0, w1 in seg:
    reach = thick * 3 + 0.5
    lo_x, hi_x = min(x0, x1) - reach, max(x0, x1) + reach
    lo_y, hi_y = min(y0, y1) - reach, max(y0, y1) + reach
    cols = np.nonzero((coords >= lo_x) & (coords <= hi_x))[0]
    rows = np.nonzero((coords >= lo_y) & (coords <= hi_y))[0]
    if len(cols) == 0 or len(rows) == 0:
        continue
    r0, r1, c0, c1 = rows[0], rows[-1] + 1, cols[0], cols[-1] + 1
    sx, sy = px[r0:r1, c0:c1], py[r0:r1, c0:c1]
    dx, dy = x1 - x0, y1 - y0
    t = np.clip(((sx - x0) * dx + (sy - y0) * dy) / (dx * dx + dy * dy), 0.0, 1.0)
    dist = np.hypot(sx - (x0 + t * dx), sy - (y0 + t * dy))
    rel = dist / thick
    closer = dist < nearest[r0:r1, c0:c1]
    nearest[r0:r1, c0:c1] = np.where(closer, dist, nearest[r0:r1, c0:c1])
    arrival[r0:r1, c0:c1] = np.where(closer, (w0 + t * (w1 - w0)) / longest, arrival[r0:r1, c0:c1])
    heat[r0:r1, c0:c1] = np.where(closer, thick / first_thickness, heat[r0:r1, c0:c1])
    best[r0:r1, c0:c1] = np.minimum(best[r0:r1, c0:c1], rel)

vein = np.clip(1.0 - best, 0.0, 1.0) ** 0.7
halo = np.exp(-nearest / 0.09) * np.clip(1.0 - arrival * 0.5, 0.0, 1.0)
# Far from every vein the fire never arrives
arrival = np.where(nearest > 0.3, 1.0, arrival)

pixels = np.stack([vein, arrival, halo, np.clip(heat, 0.0, 1.0)], axis=-1).astype(np.float32)
# Blender pictures start at the bottom row
pixels = pixels[::-1]
image = bpy.data.images.new("T_Feuer_Adern", SIZE, SIZE, alpha=True, float_buffer=False)
image.colorspace_settings.name = "Non-Color"
image.alpha_mode = "CHANNEL_PACKED"
image.pixels = pixels.ravel()
os.makedirs(OUT, exist_ok=True)
image.filepath_raw = os.path.join(OUT, "T_Feuer_Adern.png")
image.file_format = "PNG"
image.save()
print("FEUER saved", image.filepath_raw)
