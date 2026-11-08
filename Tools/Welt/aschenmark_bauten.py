"""
Buildings of the open Aschenmark that the packs don't have, built procedurally (no Tripo) with the builder of the
Glutkammer stations, exported as FBX for Scripts/AusbauAschenmark.py:

  SM_AM_Zelt           a guard's A-frame tent: two pole pairs, a ridge, sagging canvas, pegged ropes
  SM_AM_Predigerzelt   a preacher's round tent with a tall spire and a scar-violet hem
  SM_AM_Banner         a pole with a hanging cloth (the Passwache's sign)
  SM_AM_Schlackentor   the buried slag gate: a heavy stone arch, half sunk in slag, iron bars across it
  SM_AM_Passtor        the closed gate at the end of the pass: a palisade of sharpened logs, cross beams, moss growing over

Slots in this order: 0 Holz, 1 Leder (canvas), 2 Metall, 3 Faden (rope), 4 Glut, 5 Moos. Metres, pivot at the bottom middle.

  blender -b --factory-startup -P aschenmark_bauten.py -- C:/Projekte/Vael_Quellen/Aschenmark/export
"""
import math
import os
import random
import sys

from mathutils import Vector

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Refugium"))
import glutkammer_probe as gk  # noqa: E402

SLOTS = ["Holz", "Leder", "Metall", "Faden", "Glut", "Moos"]
HOLZ, LEDER, METALL, FADEN, GLUT, MOOS = range(6)


def flat(_j, _i):
    return 0.0


def sheet(b, rows, material):
    """A cloth surface seen from both sides"""
    n_j, n_i = len(rows), len(rows[0])
    b.grid(rows, material, uv_of=lambda j, i: (i / (n_i - 1), j / (n_j - 1)), mask_of=flat)
    b.grid([list(reversed(r)) for r in rows], material, uv_of=lambda j, i: (i / (n_i - 1), j / (n_j - 1)), mask_of=flat)


def build_tent():
    obj = gk.new_object("SM_AM_Zelt", SLOTS)
    b = gk.Builder()
    length, half, ridge = 3.2, 1.5, 2.1
    for x in (-length / 2, length / 2):
        for side in (-1, 1):
            b.tube([Vector((x, side * half, 0)), Vector((x, side * 0.05, ridge + 0.1))], [0.045, 0.04], HOLZ, sides=7, rough=0.1, seed=int(x * 10) + side)
    b.tube([Vector((-length / 2 - 0.2, 0, ridge)), Vector((length / 2 + 0.2, 0, ridge))], [0.05, 0.05], HOLZ, sides=7, rough=0.08, seed=5)
    # canvas: sags between the poles
    for side in (-1, 1):
        rows = []
        for j in range(7):
            t = j / 6
            row = []
            for i in range(9):
                x = -length / 2 + length * i / 8
                sag = 0.12 * math.sin(math.pi * i / 8) * math.sin(math.pi * t)
                y = side * (0.04 + (half + 0.1) * t + sag * 0.5)
                z = ridge * (1 - t) + 0.08 - sag
                row.append(Vector((x, y, z)))
            rows.append(row)
        sheet(b, rows, LEDER)
        # pegged ropes
        for x in (-length / 2, length / 2):
            b.tube([Vector((x, side * 0.1, ridge)), Vector((x * 1.35, side * (half + 0.9), 0.02))], [0.012, 0.012], FADEN, sides=4)
            b.tube([Vector((x * 1.35, side * (half + 0.9), -0.1)), Vector((x * 1.35, side * (half + 0.9), 0.25))], [0.03, 0.02], HOLZ, sides=5)
    b.finish(obj)
    gk.export(obj)


def build_preacher_tent():
    obj = gk.new_object("SM_AM_Predigerzelt", SLOTS)
    b = gk.Builder()
    sides = 14
    rows = []
    for j in range(8):
        t = j / 7
        r = 2.0 * (1 - t) ** 1.25 + 0.04
        z = 3.4 * t
        rows.append([Vector((math.cos(2 * math.pi * s / sides) * r * (1 + 0.04 * math.sin(s * 2.3)), math.sin(2 * math.pi * s / sides) * r, z)) for s in range(sides)])
    b.grid(rows, LEDER, uv_of=lambda j, i: (i / sides, j / 7), mask_of=flat, closed_u=True)
    b.grid([list(reversed(r)) for r in rows], LEDER, uv_of=lambda j, i: (i / sides, j / 7), mask_of=flat, closed_u=True)
    # scar-violet hem: a band of glow-slot cloth at the foot
    hem = [[Vector((math.cos(2 * math.pi * s / sides) * 2.06, math.sin(2 * math.pi * s / sides) * 2.06, z)) for s in range(sides)] for z in (0.0, 0.35)]
    b.grid(hem, GLUT, uv_of=lambda j, i: (i / sides, j), mask_of=lambda j, i: 0.25, closed_u=True)
    b.tube([Vector((0, 0, 0)), Vector((0, 0, 4.6))], [0.06, 0.035], HOLZ, sides=7, seed=9)
    b.tube([Vector((-0.35, 0, 4.1)), Vector((0.35, 0, 4.1))], [0.03, 0.03], METALL, sides=6)
    b.finish(obj)
    gk.export(obj)


def build_banner():
    obj = gk.new_object("SM_AM_Banner", SLOTS)
    b = gk.Builder()
    b.tube([Vector((0, 0, -0.3)), Vector((0, 0, 4.2))], [0.06, 0.045], HOLZ, sides=7, rough=0.1, seed=3)
    b.tube([Vector((0, -0.7, 3.9)), Vector((0, 0.7, 3.9))], [0.035, 0.035], HOLZ, sides=6, seed=4)
    rows = []
    for j in range(6):
        t = j / 5
        rows.append([Vector((0.03 + 0.08 * math.sin(i * 1.3 + t * 2), -0.65 + 1.3 * i / 6, 3.85 - 1.9 * t + (0.25 * t * t if i in (0, 6) else 0))) for i in range(7)])
    sheet(b, rows, LEDER)
    b.finish(obj)
    gk.export(obj)


def build_slag_gate():
    obj = gk.new_object("SM_AM_Schlackentor", SLOTS)
    b = gk.Builder()
    random.seed(21)
    # two pillars of rough blocks, an arch of wedge stones over them, all in the metal slot (dark stone)
    for side in (-1, 1):
        for k in range(5):
            b.tube([Vector((0, side * 3.4, -1.0 + k * 1.4)), Vector((0, side * 3.4, -1.0 + (k + 1) * 1.4 - 0.08))],
                   [1.0 + random.uniform(-0.1, 0.1), 0.95], METALL, sides=6, rough=0.18, seed=30 + k * 2 + side)
    for k in range(13):
        a = math.pi * k / 12
        p0 = Vector((0, -math.cos(a) * 3.4, 6.0 + math.sin(a) * 2.6))
        a1 = math.pi * (k + 0.85) / 12
        p1 = Vector((0, -math.cos(a1) * 3.4, 6.0 + math.sin(a1) * 2.6))
        b.tube([p0, p1], [0.85, 0.85], METALL, sides=6, rough=0.2, seed=60 + k)
    # iron bars across the opening and a dark back
    for k in range(7):
        y = -2.4 + k * 0.8
        b.tube([Vector((0.3, y, -0.6)), Vector((0.3, y, 6.0 + 2.0 * math.cos(y / 3.4 * math.pi / 2)))], [0.06, 0.06], METALL, sides=5)
    for z in (1.5, 4.2):
        b.tube([Vector((0.35, -2.8, z)), Vector((0.35, 2.8, z))], [0.08, 0.08], METALL, sides=5)
    rows = [[Vector((-0.6, -2.7 + 5.4 * i / 6, z)) for i in range(7)] for z in (-1.0, 7.4)]
    b.grid(rows, GLUT, uv_of=lambda j, i: (i / 6, j), mask_of=lambda j, i: 0.08)
    # slag heaped against the gate
    for k in range(9):
        y = random.uniform(-4.5, 4.5)
        b.tube([Vector((1.2, y, -0.8)), Vector((0.6, y + random.uniform(-0.5, 0.5), random.uniform(0.6, 2.2)))],
               [random.uniform(1.0, 1.6), 0.3], METALL, sides=6, rough=0.3, seed=80 + k)
    b.finish(obj)
    gk.export(obj)


def build_pass_gate():
    obj = gk.new_object("SM_AM_Passtor", SLOTS)
    b = gk.Builder()
    random.seed(33)
    width = 14.0
    n = 22
    for k in range(n):
        y = -width / 2 + width * k / (n - 1)
        h = random.uniform(4.6, 5.8)
        lean = random.uniform(-0.12, 0.12)
        b.tube([Vector((0, y, -0.6)), Vector((lean, y, h - 0.6)), Vector((lean * 1.1, y, h))], [0.22, 0.2, 0.02], HOLZ, sides=7, rough=0.12, seed=100 + k)
        # moss creeping up from the north side
        if random.random() < 0.7:
            b.tube([Vector((-0.2, y, 0.0)), Vector((-0.2, y + random.uniform(-0.1, 0.1), random.uniform(0.8, 2.6)))], [0.12, 0.04], MOOS, sides=6, rough=0.3, seed=200 + k)
    for z in (1.2, 3.4):
        b.tube([Vector((0.3, -width / 2 - 0.3, z)), Vector((0.3, width / 2 + 0.3, z + 0.1))], [0.14, 0.14], HOLZ, sides=7, rough=0.08, seed=300 + int(z))
        for k in range(8):
            y = -width / 2 + width * (k + 0.5) / 8
            b.tube([Vector((0.42, y - 0.1, z - 0.1)), Vector((0.42, y + 0.1, z + 0.12))], [0.05, 0.05], FADEN, sides=4)
    # a cross brace and a mossy mound at the foot
    b.tube([Vector((0.45, -3.0, 0.3)), Vector((0.45, 3.0, 3.8))], [0.13, 0.13], HOLZ, sides=7, seed=310)
    b.tube([Vector((0.45, 3.0, 0.3)), Vector((0.45, -3.0, 3.8))], [0.13, 0.13], HOLZ, sides=7, seed=311)
    for k in range(10):
        y = random.uniform(-width / 2, width / 2)
        b.tube([Vector((-0.6, y, -0.3)), Vector((-0.5, y + 0.3, 0.35))], [random.uniform(0.6, 1.1), 0.2], MOOS, sides=7, rough=0.3, seed=400 + k)
    b.finish(obj)
    gk.export(obj)


if __name__ == "__main__":
    os.makedirs(gk.OUT, exist_ok=True)
    for build in (build_tent, build_preacher_tent, build_banner, build_slag_gate, build_pass_gate):
        gk.clear_scene()
        build()
    gk.log("Bauten der Aschenmark fertig")
