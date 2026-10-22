"""
Feuer-Baukasten: the star of burning roots a ground fire comes out of. Used by feuer_adern.py (the picture under the fire)
and feuer_volumen.py (the fuel of the simulated fire), so the flames rise exactly where the roots run.

From the middle, a star of thick uneven arms grows outwards in winding steps, with a few short thick side arms. Coordinates
are in a unit disc (-1..1); every segment is (x0, y0, x1, y1, thickness, way at its start, way at its end), the way being
the length along the roots from the middle.
"""
import math
import numpy as np

STEP = 0.022


def root_segments(seed=41, arms=6):
    rng = np.random.default_rng(seed)
    segments = []

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
    for index in range(arms):
        start_angle = index * math.tau / arms + rng.uniform(-0.35, 0.35)
        grow(0.0, 0.0, start_angle, rng.uniform(0.045, 0.07), 0.0, rng.uniform(0.6, 0.95), 0)
    return np.array(segments, dtype=np.float32)
