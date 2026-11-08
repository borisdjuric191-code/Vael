"""
Buildings of the settlements of the Aschenmark (step 4 of Docs/Plan_NPCs_Siedlungen.md), built procedurally (no Tripo)
with the builder of the Glutkammer stations, exported as FBX for the settlement script:

  SM_AM_Huette          a hut of salvaged logs: log walls with a door hung with hide, a sagging hide roof on rafters,
                        ropes over the roof, firewood stacked at the side
  SM_AM_Werkbank        a workbench of planks with a vice, hammer, tongs, iron rods and a coil of rope
  SM_AM_Gehege          a pen of leaning posts and sagging rails, the gate standing open, a trough inside
  SM_AM_Acker           a field patch: furrows of earth with pale sprouts, stakes and string, a scarecrow
  SM_AM_Trockengestell  a hunters' drying rack: two A-frames, hides draped over the pole, strips and glowing
                        Glutkriecher shells hanging on cords

Slots in this order: 0 Holz, 1 Leder, 2 Metall, 3 Faden, 4 Glut, 5 Moos, 6 Erde. Metres, pivot at the bottom middle,
the door / gate faces +X. Imperfect on purpose: logs and posts lean and vary, cloth sags.
After the export all pieces stand side by side and are rendered to <out>/vorschau_huetten.png.

  blender -b --factory-startup -P aschenmark_huetten.py -- C:/Projekte/Vael_Quellen/Aschenmark/export
"""
import math
import os
import random
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Refugium"))
import glutkammer_probe as gk  # noqa: E402

SLOTS = ["Holz", "Leder", "Metall", "Faden", "Glut", "Moos", "Erde"]
HOLZ, LEDER, METALL, FADEN, GLUT, MOOS, ERDE = range(7)
V = Vector


def flat(_j, _i):
    return 0.0


def sheet(b, rows, material, mask=0.0):
    """A cloth surface seen from both sides"""
    n_j, n_i = len(rows), len(rows[0])
    uv = lambda j, i: (i / (n_i - 1), j / (n_j - 1))  # noqa: E731
    b.grid(rows, material, uv_of=uv, mask_of=lambda j, i: mask)
    b.grid([list(reversed(r)) for r in rows], material, uv_of=uv, mask_of=lambda j, i: mask)


def log_(b, a, c, r, seed, material=HOLZ, rough=0.12, sides=7):
    """A log or pole from a to c, a little thinner at the end, capped at both ends"""
    b.tube([a, c], [r, r * 0.92], material, sides=sides, rough=rough, seed=seed)
    b.tube([c, a], [r * 0.92, r], material, sides=sides, rough=rough, seed=seed + 500)


def jitter(amount):
    return random.uniform(-amount, amount)


# ---------------------------------------------------------------- pieces

def build_hut():
    obj = gk.new_object("SM_AM_Huette", SLOTS)
    b = gk.Builder()
    random.seed(41)
    D, W, wall_h, ridge, r, step = 1.9, 2.1, 1.95, 3.2, 0.11, 0.21
    seed = 0
    # Long walls at +-X (front +X with the door), gable walls at +-Y; the gables carry on up to the ridge in shorter logs
    k = 0
    while r + k * step < wall_h:
        z = r + k * step
        for side in (-1, 1):
            x = side * D
            ends = (-W - 0.18 + jitter(0.08), W + 0.18 + jitter(0.08))
            rr = r * random.uniform(0.9, 1.08)
            if side == 1 and z < 1.65:
                for a, c in ((ends[0], -0.5), (0.5, ends[1])):
                    log_(b, V((x, a, z)), V((x + jitter(0.02), c, z + jitter(0.02))), rr, seed)
                    seed += 1
            else:
                log_(b, V((x, ends[0], z)), V((x + jitter(0.02), ends[1], z + jitter(0.02))), rr, seed)
                seed += 1
        for side in (-1, 1):
            zz = z + step / 2
            if zz < wall_h:
                log_(b, V((-D - 0.18 + jitter(0.08), side * W, zz)), V((D + 0.18 + jitter(0.08), side * W + jitter(0.02), zz)), r * random.uniform(0.9, 1.08), seed)
                seed += 1
        k += 1
    z = wall_h + step / 2
    while z < ridge - 0.15:
        half = (D + 0.1) * (1 - (z - wall_h) / (ridge - wall_h))
        for side in (-1, 1):
            log_(b, V((-half, side * W, z)), V((half, side * W, z)), r * 0.9, seed)
            seed += 1
        z += step
    # Door posts and the hide hung in the door
    for y in (-0.52, 0.52):
        log_(b, V((D + 0.12, y, 0.0)), V((D + 0.12, y + jitter(0.03), 1.75)), 0.07, seed)
        seed += 1
    rows = []
    for j in range(7):
        t = j / 6
        rows.append([V((D + 0.16 + 0.04 * math.sin(i * 1.7 + t * 3), -0.46 + 0.92 * i / 6, 1.62 - 1.55 * t + (0.08 * t if i in (0, 6) else 0))) for i in range(7)])
    sheet(b, rows, LEDER)
    # Roof: ridge pole, rafters, two sagging hide sheets, a patch, ropes holding it down
    log_(b, V((0, -W - 0.55, ridge + 0.05)), V((0.03, W + 0.55, ridge + 0.03)), 0.09, seed)
    for y in (-W - 0.3, -1.0, 0.0, 1.0, W + 0.3):
        for side in (-1, 1):
            log_(b, V((side * (D + 0.5), y, wall_h - 0.28)), V((0, y + jitter(0.05), ridge + 0.05)), 0.055, seed)
            seed += 1
    for side in (-1, 1):
        rows = []
        n_i = 11
        for j in range(7):
            t = j / 6
            row = []
            for i in range(n_i):
                y = -W - 0.45 + (2 * W + 0.9) * i / (n_i - 1)
                sag = 0.11 * math.sin(math.pi * t) * (0.6 + 0.4 * math.sin(math.pi * i / (n_i - 1)))
                ragged = 0.08 * math.sin(i * 2.3 + side) * t
                row.append(V((side * (0.04 + (D + 0.55 + ragged) * t), y, ridge + 0.14 - (ridge + 0.14 - (wall_h - 0.18)) * t - sag)))
            rows.append(row)
        sheet(b, rows, LEDER)
    patch = [[V((0.6 + 0.7 * t, -0.9 + 0.8 * i / 3, ridge + 0.17 - (ridge - wall_h + 0.3) * (0.6 + 0.7 * t) / (D + 0.55) - 0.03)) for i in range(4)] for t in (0.0, 0.5, 1.0)]
    sheet(b, patch, LEDER)
    for y in (-1.2, 1.3):
        pts = [V((side * (D + 0.5) * t, y + jitter(0.03), ridge + 0.2 - (ridge - wall_h + 0.35) * t)) for side, t in ((-1, 1.0), (-1, 0.5), (1, 0.0), (1, 0.5), (1, 1.0))]
        pts += [V((D + 0.55, y, 0.2))]
        pts = [V((-D - 0.55, y, 0.2))] + pts
        b.tube(pts, [0.018] * len(pts), FADEN, sides=4)
        for x in (-D - 0.55, D + 0.55):
            b.tube([V((x, y, 0.0)), V((x, y, 0.35))], [0.05, 0.03], HOLZ, sides=5)
    # Firewood stacked against the +Y gable
    for row in range(3):
        for n in range(5 - row):
            x = -1.0 + (n + row * 0.5) * 0.24
            log_(b, V((x, W + 0.35, 0.1 + row * 0.2)), V((x + jitter(0.03), W + 1.05, 0.1 + row * 0.2)), 0.1, seed, rough=0.2)
            seed += 1
    b.finish(obj)
    gk.export(obj)
    return obj


def build_workbench():
    obj = gk.new_object("SM_AM_Werkbank", SLOTS)
    b = gk.Builder()
    random.seed(52)
    top = 0.88
    for k, x in enumerate((-0.3, -0.1, 0.1, 0.3)):
        b.tube([V((x, -1.02 + jitter(0.03), top)), V((x, 1.02 + jitter(0.03), top + jitter(0.01)))], [0.105, 0.105], HOLZ, sides=6, flat=0.32, rough=0.05, seed=k)
    for x in (-0.32, 0.32):
        for y in (-0.85, 0.85):
            log_(b, V((x + jitter(0.02), y, 0.0)), V((x, y, top - 0.03)), 0.05, int(x * 10 + y * 10) + 20)
        log_(b, V((x, -0.85, 0.24)), V((x, 0.85, 0.24)), 0.035, 30 + int(x * 10))
    for k, x in enumerate((-0.18, 0.0, 0.18)):
        b.tube([V((x, -0.8, 0.27)), V((x, 0.8, 0.27))], [0.09, 0.09], HOLZ, sides=6, flat=0.3, seed=40 + k)
    # Vice at the +Y end
    b.tube([V((0.36, 0.86, top - 0.1)), V((0.36, 0.86, top + 0.14))], [0.06, 0.06], METALL, sides=6)
    b.tube([V((0.47, 0.86, top - 0.05)), V((0.47, 0.86, top + 0.14))], [0.05, 0.05], METALL, sides=6)
    b.tube([V((0.3, 0.86, top + 0.03)), V((0.62, 0.86, top + 0.03))], [0.015, 0.015], METALL, sides=5)
    # Hammer, tongs, rods
    b.tube([V((-0.1, 0.1, top + 0.06)), V((0.12, 0.45, top + 0.06))], [0.018, 0.016], HOLZ, sides=5)
    b.tube([V((0.06, 0.48, top + 0.07)), V((0.18, 0.4, top + 0.07))], [0.035, 0.035], METALL, sides=6)
    for dy in (-0.02, 0.02):
        b.tube([V((-0.25, -0.2 + dy, top + 0.07)), V((0.05, -0.05 + dy * 3, top + 0.07))], [0.01, 0.008], METALL, sides=4)
    for k in range(4):
        b.tube([V((-0.35 + k * 0.03, -0.95, top + 0.07 + k * 0.012)), V((-0.3 + k * 0.03, -0.35, top + 0.07))], [0.012, 0.012], METALL, sides=4)
    # A coil of rope
    pts = [V((0.18 + 0.12 * math.cos(a), -0.6 + 0.12 * math.sin(a), top + 0.08 + a * 0.004)) for a in [t * 0.4 for t in range(40)]]
    b.tube(pts, [0.018] * len(pts), FADEN, sides=5)
    b.finish(obj)
    gk.export(obj)
    return obj


def build_pen():
    obj = gk.new_object("SM_AM_Gehege", SLOTS)
    b = gk.Builder()
    random.seed(63)
    a, c = 3.0, 2.3  # half size along X and Y
    corners = [(-a, -c), (a, -c), (a, c), (-a, c)]
    posts = []
    for (x0, y0), (x1, y1) in zip(corners, corners[1:] + corners[:1]):
        n = max(2, int(math.hypot(x1 - x0, y1 - y0) / 1.5))
        for k in range(n):
            t = k / n
            posts.append(V((x0 + (x1 - x0) * t + jitter(0.12), y0 + (y1 - y0) * t + jitter(0.12), 0.0)))
    gate = min(range(len(posts)), key=lambda i: (posts[i] - V((a, 0, 0))).length)
    tops = []
    for k, p in enumerate(posts):
        top = p + V((jitter(0.12), jitter(0.12), random.uniform(1.15, 1.4)))
        log_(b, p + V((0, 0, -0.2)), top, 0.065 * random.uniform(0.85, 1.15), 70 + k, rough=0.18)
        tops.append(top)
    for k in range(len(posts)):
        if k == gate:
            continue
        p, q = posts[k], posts[(k + 1) % len(posts)]
        for h, sag in ((0.45, 0.04), (0.98, 0.07)):
            mid = (p + q) / 2 + V((0, 0, h - sag))
            b.tube([p + V((0, 0, h + jitter(0.04))), mid, q + V((0, 0, h + jitter(0.04)))], [0.045, 0.045, 0.045], HOLZ, sides=6, rough=0.15, seed=100 + k)
    # The gate stands open, swung outwards
    hinge = posts[gate]
    end = hinge + V((1.25, 0.6, 0))
    for h in (0.3, 0.9):
        log_(b, hinge + V((0.1, 0, h)), end + V((0, 0, h - 0.05)), 0.04, 130 + int(h * 10))
    log_(b, end + V((0, 0, 0.1)), end + V((0, 0, 1.05)), 0.045, 140)
    log_(b, hinge + V((0.1, 0, 0.3)), end + V((0, 0, 0.9)), 0.035, 141)
    # Trough of a split log, a heap of straw beside it
    b.tube([V((-1.6, -0.6, 0.18)), V((-0.2, -0.5, 0.2))], [0.24, 0.23], HOLZ, sides=8, flat=0.55, rough=0.1, seed=150)
    for k in range(40):
        x, y = -1.2 + jitter(0.5), 1.0 + jitter(0.4)
        a = random.uniform(0, 2 * math.pi)
        length = random.uniform(0.4, 0.8)
        z = random.uniform(0.03, 0.18)
        b.tube([V((x, y, z)), V((x + math.cos(a) * length, y + math.sin(a) * length, z + jitter(0.06)))], [0.025, 0.015], FADEN, sides=4, seed=160 + k)
    b.finish(obj)
    gk.export(obj)
    return obj


def build_field():
    obj = gk.new_object("SM_AM_Acker", SLOTS)
    b = gk.Builder()
    random.seed(74)
    L, rows = 5.0, 9
    for k in range(rows):
        y = -2.8 + 5.6 * k / (rows - 1)
        pts = [V((-L + 2 * L * i / 9, y + jitter(0.06), 0.02 + jitter(0.03))) for i in range(10)]
        b.tube(pts, [0.24] * 10, ERDE, sides=7, flat=0.45, rough=0.2, seed=200 + k)
        x = -L + 0.3
        while x < L - 0.2:
            if random.random() < 0.65:
                base = V((x, y + jitter(0.05), 0.08))
                for leaf in range(3):
                    tip = base + V((jitter(0.08), jitter(0.08), random.uniform(0.12, 0.3)))
                    b.tube([base, tip], [0.025, 0.004], MOOS, sides=4)
            x += random.uniform(0.25, 0.4)
    # Stakes at the corners with a string round the field
    stakes = [V((sx * (L + 0.3), sy * 3.2, 0)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    for k, s in enumerate(stakes):
        log_(b, s + V((0, 0, -0.1)), s + V((jitter(0.05), jitter(0.05), 0.6)), 0.04, 260 + k)
    ring = [s + V((0, 0, 0.45)) for s in stakes] + [stakes[0] + V((0, 0, 0.45))]
    for p, q in zip(ring, ring[1:]):
        b.tube([p, (p + q) / 2 - V((0, 0, 0.08)), q], [0.01] * 3, FADEN, sides=4)
    # Scarecrow: post, cross bar, a sack for a head, a ragged coat
    s = V((-L + 0.8, 3.0, 0))
    log_(b, s + V((0, 0, -0.2)), s + V((0, 0, 2.1)), 0.05, 280)
    log_(b, s + V((0, -0.75, 1.55)), s + V((0, 0.75, 1.6)), 0.04, 281)
    b.tube([s + V((0, 0, 1.85)), s + V((0, 0, 2.05)), s + V((0, 0, 2.2))], [0.15, 0.17, 0.1], LEDER, sides=8, rough=0.15, seed=282)
    rows_ = []
    for j in range(6):
        t = j / 5
        rows_.append([s + V((0.06 + 0.05 * math.sin(i + t * 4), -0.7 + 1.4 * i / 6, 1.6 - 0.95 * t - (0.15 * t if i in (0, 6) else 0))) for i in range(7)])
    sheet(b, rows_, LEDER)
    b.finish(obj)
    gk.export(obj)
    return obj


def build_rack():
    obj = gk.new_object("SM_AM_Trockengestell", SLOTS)
    b = gk.Builder()
    random.seed(85)
    top = 1.95
    for k, y in enumerate((-1.45, 1.45)):
        for side in (-1, 1):
            log_(b, V((side * 0.75, y + jitter(0.05), -0.1)), V((side * 0.05, y, top + 0.15)), 0.05, 300 + k * 2 + side)
    log_(b, V((0, -1.7, top)), V((0, 1.7, top + jitter(0.03))), 0.05, 310)
    log_(b, V((0.55, -1.5, 1.35)), V((0.55, 1.5, 1.35)), 0.035, 311)
    # Hides draped over the top pole, each a different size, ragged at the edges
    for k, (y0, width, front, back) in enumerate(((-1.15, 0.8, 0.95, 0.7), (-0.1, 0.95, 1.2, 0.9), (0.95, 0.7, 0.8, 0.6))):
        rows = []
        for j in range(9):
            t = j / 8
            # over the pole: t 0 = low at the back (-X), t 1 = low at the front (+X)
            a = math.pi * t
            reach = back if t < 0.5 else front
            x = -math.cos(a) * 0.12 + (t - 0.5) * 0.25
            z = top + 0.06 - (1 - math.sin(a)) * reach
            row = []
            for i in range(6):
                u = i / 5
                ragged = 0.07 * math.sin(i * 2.1 + k + j)
                row.append(V((x + 0.02 * math.sin(u * 5 + k), y0 - width / 2 + width * u, z + (ragged if j in (0, 8) else 0))))
            rows.append(row)
        sheet(b, rows, LEDER)
    # Strips and glowing shells hanging from the lower pole on cords
    for k in range(7):
        y = -1.35 + k * 0.42 + jitter(0.06)
        length = random.uniform(0.35, 0.65)
        if k % 2:
            b.tube([V((0.55, y, 1.33)), V((0.56, y, 1.33 - length))], [0.006, 0.006], FADEN, sides=4)
            b.tube([V((0.55, y, 1.33 - length - 0.02)), V((0.55, y, 1.33 - length - 0.2))], [0.09, 0.03], GLUT, sides=6, flat=0.4, seed=330 + k, mask_of=lambda t: 0.3)
        else:
            rows = [[V((0.56 + 0.02 * math.sin(j + i), y - 0.05 + 0.1 * i, 1.33 - length * j / 4)) for i in range(2)] for j in range(5)]
            sheet(b, rows, LEDER)
    b.finish(obj)
    gk.export(obj)
    return obj


# ---------------------------------------------------------------- preview

COLOURS = {"Holz": (0.24, 0.14, 0.07), "Leder": (0.42, 0.3, 0.18), "Metall": (0.12, 0.12, 0.13), "Faden": (0.6, 0.52, 0.36),
           "Glut": (1.0, 0.35, 0.05), "Moos": (0.35, 0.45, 0.18), "Erde": (0.11, 0.08, 0.06)}


def render_preview(objects, path):
    for name, colour in COLOURS.items():
        mat = bpy.data.materials.get(name)
        if mat is None:
            continue
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.85
        if name == "Glut":
            bsdf.inputs["Emission Color"].default_value = (*colour, 1.0)
            bsdf.inputs["Emission Strength"].default_value = 3.0
    # Two rows, seen from the iso angle of the game through an orthographic camera
    layout = [(0.0, 0.0), (0.0, 7.0), (0.0, 12.5), (-9.0, 0.5), (-9.0, 10.5)]
    for obj, (x, y) in zip(objects, layout):
        obj.location = (x, y, 0.0)
    centre = V((-4.5, 6.5, 0.0))
    bpy.ops.mesh.primitive_plane_add(size=80, location=centre)
    ground = bpy.context.active_object
    gmat = bpy.data.materials.new("Boden")
    gmat.use_nodes = True
    gmat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.2, 0.18, 0.16, 1)
    ground.data.materials.append(gmat)
    bpy.ops.object.light_add(type="SUN", rotation=(math.radians(40), math.radians(15), math.radians(200)))
    bpy.context.active_object.data.energy = 3.5
    scene = bpy.context.scene
    scene.world = bpy.data.worlds.new("Welt")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.45, 0.42, 0.4, 1)
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.8
    direction = V((-1.0, -1.0, -1.15)).normalized()
    bpy.ops.object.camera_add(location=centre - direction * 40.0)
    cam = bpy.context.active_object
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 24.0
    scene.camera = cam
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 48
    scene.render.resolution_x, scene.render.resolution_y = 1800, 1000
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    gk.log("Vorschau -> " + path)


if __name__ == "__main__":
    os.makedirs(gk.OUT, exist_ok=True)
    gk.clear_scene()
    built = [build() for build in (build_hut, build_workbench, build_pen, build_field, build_rack)]
    render_preview(built, os.path.join(gk.OUT, "vorschau_huetten.png"))
    gk.log("Hütten der Aschenmark fertig")
