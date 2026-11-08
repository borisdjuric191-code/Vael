"""
The tinkerer's workshop of the engineer in the Glutkammer (goal 8.11. evening, step 6), procedural (no Tripo), one design:
dark wood, brass, leather, glowing coals. Exported as FBX for Scripts/BuildWerkstatt.py.

  SM_IW_Esse            a small forge: a stone hearth with glowing coals, a brass hood and a chimney pipe
  SM_IW_Blasebalg       bellows on a wooden stand: two boards, a leather body, a brass nozzle
  SM_IW_Amboss          an anvil on a log stump, horn and heel
  SM_IW_Loeschfass      a quenching barrel with brass hoops, dark water, tongs leaning in it
  SM_IW_Werkzeugwand    a board wall with hammers, tongs, files, saw and gauges on brass hooks
  SM_IW_Schraubstock    a heavy vise on a post
  SM_IW_Schleifstein    a grinding wheel in a wooden frame with a crank and a treadle
  SM_IW_Zeichentisch    a slanted drawing desk with pinned plans, compass and ruler, a brass lamp
  SM_IW_Regal           shelves of jars (ammo fillings), boxes and coils
  SM_IW_Munitionskiste  an open crate of sling stones and bolts with leather straps
  SM_IW_Schrottkiste    a crate of scrap: shell plates, gears, slag bars
  SM_IW_Hammer          a smith's hammer lying on its side (on the anvil)
  SM_IW_Zange           forge tongs lying open, a brass rivet (beside the anvil)

Slots: 0 Holz, 1 Metall (brass), 2 Leder, 3 Glut, 4 Stein, 5 Glas, 6 Papier, 7 Eisen, 8 Fluessigkeit, 9 Faden.
Pivot at the bottom middle, the working side faces +X. A preview of all pieces goes to <out>/vorschau_werkstatt.png.

  blender -b --factory-startup -P vael_werkstatt.py -- C:/Projekte/Vael_Quellen/Ingenieur/export
"""
import math
import os
import random
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Refugium"))
import glutkammer_probe as gk  # noqa: E402
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Refugium"))
from glutkammer_stations import box, lathe  # noqa: E402

SLOTS = ["Holz", "Metall", "Leder", "Glut", "Stein", "Glas", "Papier", "Eisen", "Fluessigkeit", "Faden"]
HOLZ, MESSING, LEDER, GLUT, STEIN, GLAS, PAPIER, EISEN, WASSER, FADEN = range(10)
V = Vector


def j(a):
    return random.uniform(-a, a)


def plank(b, a, c, w, t, material=HOLZ, seed=0):
    """A board from a to c, width w, thickness t (laid flat across +Z unless a-c is vertical)"""
    d = c - a
    length = d.length
    mid = (a + c) * 0.5
    yaw = math.atan2(d.y, d.x)
    pitch = -math.atan2(d.z, math.hypot(d.x, d.y))
    rot = Matrix.Rotation(yaw, 3, "Z") @ Matrix.Rotation(pitch, 3, "Y")
    hx, hy, hz = length / 2, w / 2, t / 2
    corners = [mid + rot @ V((sx * hx, sy * hy, sz * hz)) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]
    idx = {(sx, sy, sz): corners[k] for k, (sx, sy, sz) in enumerate([(sx, sy, sz) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)])}
    faces = [((-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)), ((-1, 1, -1), (1, 1, -1), (1, -1, -1), (-1, -1, -1)),
             ((-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)), ((1, 1, -1), (-1, 1, -1), (-1, 1, 1), (1, 1, 1)),
             ((1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)), ((-1, 1, -1), (-1, -1, -1), (-1, -1, 1), (-1, 1, 1))]
    for q in faces:
        rows = [[idx[q[0]], idx[q[1]]], [idx[q[3]], idx[q[2]]]]
        b.grid(rows, material, uv_of=lambda jj, i: (i * length, jj * w), mask_of=lambda jj, i: 0.0)


def hammer(b, top, length=0.32, head=0.09, seed=0):
    b.tube([top, top - V((0, 0, length))], [0.013, 0.012], HOLZ, sides=6)
    box(b, top - V((0, 0, length + 0.02)), (0.035, head, 0.04), EISEN)


def tongs(b, top, length=0.4):
    for s in (-1, 1):
        b.tube([top, top + V((0, s * 0.02, -length * 0.6)), top + V((0, s * 0.008, -length))], [0.007, 0.007, 0.006], EISEN, sides=5)


def build_forge():
    obj = gk.new_object("SM_IW_Esse", SLOTS)
    b = gk.Builder()
    random.seed(1)
    # hearth: stone blocks in two courses with a pan of coals on top
    for course in range(3):
        for k in range(8):
            a = 2 * math.pi * k / 8 + course * 0.4
            box(b, V((math.cos(a) * 0.42, math.sin(a) * 0.42, 0.13 + course * 0.22)), (0.28, 0.24, 0.22), STEIN, yaw=a, bevel_noise=0.02, seed=k + course * 9)
    lathe(b, [(0.42, 0.7), (0.4, 0.66), (0.3, 0.62), (0.0, 0.6)], EISEN, V((0, 0, 0)), sides=20)
    lathe(b, [(0.33, 0.66), (0.25, 0.69), (0.12, 0.71), (0.001, 0.72)], GLUT, V((0, 0, 0)), sides=18, mask=1.0, wobble_amount=0.25, seed=3)
    for k in range(14):
        a, r = random.uniform(0, 2 * math.pi), random.uniform(0.05, 0.3)
        box(b, V((math.cos(a) * r, math.sin(a) * r, 0.72)), (0.05, 0.04, 0.03), GLUT, yaw=a, mask=0.8, bevel_noise=0.01, seed=k)
    # brass hood on four posts and the chimney
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.tube([V((sx * 0.4, sy * 0.4, 0.7)), V((sx * 0.36, sy * 0.36, 1.45))], [0.025, 0.022], EISEN, sides=6)
    lathe(b, [(0.62, 1.45), (0.55, 1.5), (0.3, 1.75), (0.14, 1.9)], MESSING, V((0, 0, 0)), sides=20)
    lathe(b, [(0.14, 1.9), (0.13, 2.6), (0.15, 2.62), (0.15, 2.7)], EISEN, V((0, 0, 0)), sides=12)
    gk_finish(b, obj)


def build_bellows():
    obj = gk.new_object("SM_IW_Blasebalg", SLOTS)
    b = gk.Builder()
    random.seed(2)
    for x in (-0.25, 0.25):
        b.tube([V((x, -0.2, 0.0)), V((x, -0.18, 0.55))], [0.03, 0.028], HOLZ, sides=6)
        b.tube([V((x, 0.2, 0.0)), V((x, 0.18, 0.55))], [0.03, 0.028], HOLZ, sides=6)
    plank(b, V((-0.35, 0, 0.58)), V((0.35, 0, 0.58)), 0.45, 0.04)
    # leather body between two boards, pleated
    rows = []
    for k in range(9):
        t = k / 8
        z = 0.62 + t * 0.2
        w = 0.36 * (1 - 0.35 * t) * (1 + 0.06 * math.sin(k * 2.5))
        rows.append([V((-0.3 + 0.55 * u, w * math.sin(math.pi * (u - 0.5)) * 0.0 + (u - 0.5) * 0 , z)) for u in (0,)])
    for k in range(6):
        z = 0.62 + k * 0.035
        w = 0.21 * (1 - 0.1 * k) + (0.02 if k % 2 else 0.0)
        lathe(b, [(w, z), (w, z + 0.035)], LEDER, V((0.0, 0.0, 0.0)), sides=4, tilt=Matrix.Scale(1.6, 3, V((1, 0, 0))))
    plank(b, V((-0.32, 0, 0.84)), V((0.28, 0, 0.84)), 0.38, 0.035)
    b.tube([V((-0.32, 0, 0.86)), V((-0.6, 0, 0.9))], [0.02, 0.018], HOLZ, sides=6)
    lathe(b, [(0.05, 0.0), (0.035, 0.12), (0.015, 0.3)], MESSING, V((0.32, 0, 0.72)), sides=10, tilt=Matrix.Rotation(math.pi / 2, 3, "Y"))
    gk_finish(b, obj)


def build_anvil():
    obj = gk.new_object("SM_IW_Amboss", SLOTS)
    b = gk.Builder()
    random.seed(3)
    lathe(b, [(0.26, 0.0), (0.24, 0.3), (0.23, 0.5), (0.0, 0.52)], HOLZ, V((0, 0, 0)), sides=16, wobble_amount=0.08, seed=2)
    box(b, V((0, 0, 0.58)), (0.24, 0.18, 0.12), EISEN)
    box(b, V((0, 0, 0.68)), (0.16, 0.14, 0.08), EISEN)
    box(b, V((0.02, 0, 0.77)), (0.46, 0.15, 0.1), EISEN)
    b.tube([V((0.25, 0, 0.78)), V((0.38, 0, 0.77)), V((0.46, 0, 0.75))], [0.07, 0.04, 0.008], EISEN, sides=10)
    box(b, V((-0.27, 0, 0.78)), (0.1, 0.12, 0.07), EISEN)
    hammer(b, V((0.05, 0.03, 0.9)) + V((0, 0, 0)), 0.0, 0.08)
    b.tube([V((-0.05, 0.03, 0.835)), V((0.25, 0.08, 0.835))], [0.012, 0.011], HOLZ, sides=6)
    box(b, V((-0.08, 0.03, 0.85)), (0.05, 0.1, 0.04), EISEN)
    gk_finish(b, obj)


def build_barrel():
    obj = gk.new_object("SM_IW_Loeschfass", SLOTS)
    b = gk.Builder()
    random.seed(4)
    prof = [(0.27, 0.0), (0.31, 0.2), (0.32, 0.35), (0.31, 0.5), (0.28, 0.68)]
    lathe(b, prof, HOLZ, V((0, 0, 0)), sides=22)
    lathe(b, [(0.26, 0.6), (0.0, 0.6)], WASSER, V((0, 0, 0)), sides=22)
    for z in (0.08, 0.34, 0.6):
        r = 0.27 + 0.05 * math.sin(math.pi * z / 0.68)
        lathe(b, [(r + 0.012, z - 0.02), (r + 0.012, z + 0.02)], MESSING, V((0, 0, 0)), sides=22)
    tongs(b, V((0.1, 0.05, 0.95)), 0.55)
    gk_finish(b, obj)


def build_tool_wall():
    obj = gk.new_object("SM_IW_Werkzeugwand", SLOTS)
    b = gk.Builder()
    random.seed(5)
    for k in range(7):
        y = -0.9 + k * 0.3
        plank(b, V((-0.03, y, 0.0)), V((-0.03, y, 1.9)), 0.28, 0.04, seed=k)
    for z in (0.3, 1.6):
        plank(b, V((0.0, -1.0, z)), V((0.0, 1.0, z)), 0.1, 0.04)
    for k in range(9):
        y = -0.8 + k * 0.2
        b.tube([V((0.0, y, 1.45)), V((0.06, y, 1.47))], [0.008, 0.007], MESSING, sides=5)
        kind = k % 4
        if kind == 0:
            hammer(b, V((0.06, y, 1.44)), random.uniform(0.25, 0.35))
        elif kind == 1:
            tongs(b, V((0.06, y, 1.44)), random.uniform(0.35, 0.45))
        elif kind == 2:
            b.tube([V((0.06, y, 1.44)), V((0.06, y, 1.0))], [0.01, 0.008], EISEN, sides=4)
            box(b, V((0.06, y, 1.48)), (0.03, 0.03, 0.08), HOLZ)
        else:
            box(b, V((0.06, y, 1.25)), (0.01, 0.12, 0.35), EISEN)
            box(b, V((0.06, y, 1.45)), (0.04, 0.05, 0.08), HOLZ)
    # gauges and a coil of brass wire low down
    for k in range(3):
        lathe(b, [(0.0, 0.0), (0.06, 0.0), (0.06, 0.02), (0.0, 0.02)], MESSING, V((0.04, -0.6 + k * 0.2, 0.75)), sides=12,
              tilt=Matrix.Rotation(math.pi / 2, 3, "Y"))
    lathe(b, [(0.07, 0.0), (0.1, 0.0), (0.1, 0.05), (0.07, 0.05)], MESSING, V((0.06, 0.5, 0.6)), sides=14, tilt=Matrix.Rotation(math.pi / 2, 3, "Y"))
    gk_finish(b, obj)


def build_vise():
    obj = gk.new_object("SM_IW_Schraubstock", SLOTS)
    b = gk.Builder()
    random.seed(6)
    lathe(b, [(0.12, 0.0), (0.1, 0.05), (0.08, 0.05), (0.08, 0.8), (0.0, 0.8)], HOLZ, V((0, 0, 0)), sides=10)
    box(b, V((0, 0, 0.86)), (0.2, 0.16, 0.12), EISEN)
    box(b, V((0.09, 0, 0.96)), (0.04, 0.18, 0.12), EISEN)
    box(b, V((-0.09, 0, 0.96)), (0.04, 0.18, 0.12), EISEN)
    b.tube([V((0.11, 0, 0.92)), V((0.3, 0, 0.92))], [0.016, 0.016], EISEN, sides=8)
    b.tube([V((0.3, -0.12, 0.92)), V((0.3, 0.12, 0.92))], [0.012, 0.012], MESSING, sides=6)
    gk_finish(b, obj)


def build_grinder():
    obj = gk.new_object("SM_IW_Schleifstein", SLOTS)
    b = gk.Builder()
    random.seed(7)
    for y in (-0.18, 0.18):
        plank(b, V((-0.35, y, 0.0)), V((0.0, y, 0.75)), 0.06, 0.06)
        plank(b, V((0.35, y, 0.0)), V((0.0, y, 0.75)), 0.06, 0.06)
    lathe(b, [(0.0, -0.06), (0.32, -0.06), (0.33, 0.0), (0.32, 0.06), (0.0, 0.06)], STEIN, V((0, 0, 0.75)), sides=28,
          tilt=Matrix.Rotation(math.pi / 2, 3, "X"))
    b.tube([V((0, -0.3, 0.75)), V((0, 0.26, 0.75))], [0.02, 0.02], EISEN, sides=8)
    b.tube([V((0, -0.3, 0.75)), V((0.12, -0.3, 0.75)), V((0.12, -0.38, 0.75))], [0.015] * 3, EISEN, sides=6)
    box(b, V((0.12, -0.4, 0.75)), (0.04, 0.08, 0.04), HOLZ)
    lathe(b, [(0.16, 0.42), (0.15, 0.5), (0.0, 0.5)], HOLZ, V((0, 0, 0)), sides=14)
    plank(b, V((-0.3, 0.0, 0.05)), V((0.3, 0.0, 0.12)), 0.2, 0.03)
    gk_finish(b, obj)


def build_desk():
    obj = gk.new_object("SM_IW_Zeichentisch", SLOTS)
    b = gk.Builder()
    random.seed(8)
    for x in (-0.3, 0.3):
        for y in (-0.55, 0.55):
            b.tube([V((x, y, 0.0)), V((x, y, 0.85 + (0.12 if x < 0 else 0.0)))], [0.03, 0.028], HOLZ, sides=6)
    # the slanted top with plans pinned on it
    top = Matrix.Rotation(0.25, 3, "Y")
    plank(b, V((-0.35, 0, 0.98)) , V((0.35, 0, 0.82)), 1.25, 0.04)
    for k in range(4):
        a = V((random.uniform(-0.25, 0.2), random.uniform(-0.45, 0.4), 0.0))
        z = 0.9 - a.x * 0.23 + 0.025 + k * 0.002
        rows = [[V((a.x - 0.14, a.y - 0.2, z + 0.032)), V((a.x - 0.14, a.y + 0.2, z + 0.032))],
                [V((a.x + 0.14, a.y - 0.2, z - 0.032)), V((a.x + 0.14, a.y + 0.2, z - 0.032))]]
        b.grid(rows, PAPIER, uv_of=lambda jj, i: (i, jj), mask_of=lambda jj, i: 0.0)
        b.tube([V((a.x - 0.12, a.y - 0.18, z + 0.04)), V((a.x - 0.12, a.y - 0.18, z + 0.06))], [0.006, 0.006], MESSING, sides=5)
    # ruler, compass, a brass lamp with a glowing ember inside
    box(b, V((0.0, 0.5, 0.94)), (0.5, 0.04, 0.01), HOLZ)
    for s in (-1, 1):
        b.tube([V((0.1, -0.3, 1.0)), V((0.1 + s * 0.05, -0.3, 0.92))], [0.004, 0.003], MESSING, sides=4)
    lathe(b, [(0.06, 0.0), (0.05, 0.03), (0.045, 0.03), (0.045, 0.16), (0.07, 0.18), (0.0, 0.2)], MESSING, V((-0.25, 0.5, 0.99)), sides=12)
    lathe(b, [(0.035, 0.03), (0.03, 0.15)], GLUT, V((-0.25, 0.5, 0.99)), sides=10, mask=1.0)
    gk_finish(b, obj)


def build_shelf():
    obj = gk.new_object("SM_IW_Regal", SLOTS)
    b = gk.Builder()
    random.seed(9)
    for y in (-0.6, 0.6):
        for x in (-0.15, 0.15):
            b.tube([V((x, y, 0.0)), V((x, y, 1.9))], [0.025, 0.025], HOLZ, sides=6)
    for k, z in enumerate((0.3, 0.75, 1.2, 1.65)):
        plank(b, V((0.0, -0.65, z)), V((0.0, 0.65, z)), 0.36, 0.035, seed=k)
        y = -0.55
        while y < 0.5:
            kind = random.random()
            if kind < 0.55:
                r = random.uniform(0.05, 0.08)
                h = random.uniform(0.12, 0.22)
                lathe(b, [(r * 0.9, 0.0), (r, 0.02), (r, h), (r * 0.6, h + 0.02), (r * 0.6, h + 0.04)], GLAS, V((0.0, y + r, z + 0.02)), sides=12)
                fill = random.choice([GLUT, WASSER, STEIN, FADEN])
                lathe(b, [(r * 0.85, 0.02), (r * 0.85, h * random.uniform(0.3, 0.8))], fill, V((0.0, y + r, z + 0.02)), sides=10,
                      mask=1.0 if fill == GLUT else 0.0)
                lathe(b, [(r * 0.65, h + 0.04), (r * 0.65, h + 0.07), (0.0, h + 0.07)], LEDER, V((0.0, y + r, z + 0.02)), sides=10)
                y += r * 2 + 0.03
            elif kind < 0.8:
                box(b, V((0.0, y + 0.08, z + 0.07)), (0.24, 0.16, 0.1), HOLZ, bevel_noise=0.005, seed=int(y * 100))
                y += 0.19
            else:
                lathe(b, [(0.04, 0.0), (0.07, 0.0), (0.07, 0.06), (0.04, 0.06)], FADEN, V((0.0, y + 0.07, z + 0.02)), sides=12)
                y += 0.16
    gk_finish(b, obj)


def build_ammo_crate():
    obj = gk.new_object("SM_IW_Munitionskiste", SLOTS)
    b = gk.Builder()
    random.seed(10)
    for side in (-1, 1):
        plank(b, V((-0.35, side * 0.25, 0.18)), V((0.35, side * 0.25, 0.18)), 0.04, 0.36)
        plank(b, V((side * 0.35, -0.25, 0.18)), V((side * 0.35, 0.25, 0.18)), 0.04, 0.36)
    plank(b, V((-0.35, 0, 0.02)), V((0.35, 0, 0.02)), 0.5, 0.04)
    for k in range(40):
        lathe(b, [(0.0, 0.0), (0.035, 0.02), (0.03, 0.045), (0.0, 0.055)], STEIN, V((j(0.28), j(0.2), 0.04 + random.uniform(0.0, 0.25))), sides=7)
    for k in range(10):
        a = V((j(0.25), j(0.18), 0.3))
        b.tube([a, a + V((0.0, 0.0, 0.18)) + V((j(0.05), j(0.05), 0))], [0.008, 0.007], HOLZ, sides=5)
    for x in (-0.2, 0.2):
        b.tube([V((x, -0.27, 0.36)), V((x, -0.29, 0.2)), V((x, -0.27, 0.0))], [0.012] * 3, LEDER, sides=4, flat=0.3, cap_end=False)
    # the lid leaning open behind it
    plank(b, V((-0.37, 0.3, 0.36)), V((0.37, 0.42, 0.75)), 0.04, 0.5)
    gk_finish(b, obj)


def build_scrap_crate():
    obj = gk.new_object("SM_IW_Schrottkiste", SLOTS)
    b = gk.Builder()
    random.seed(11)
    for side in (-1, 1):
        plank(b, V((-0.4, side * 0.3, 0.2)), V((0.4, side * 0.3, 0.2)), 0.04, 0.4)
        plank(b, V((side * 0.4, -0.3, 0.2)), V((side * 0.4, 0.3, 0.2)), 0.04, 0.4)
    for k in range(10):
        box(b, V((j(0.3), j(0.2), 0.3 + random.uniform(0, 0.15))), (0.2, 0.14, 0.02), EISEN, yaw=random.uniform(0, 3), bevel_noise=0.01, seed=k)
    for k in range(4):
        teeth = 10
        r = random.uniform(0.06, 0.1)
        c = V((j(0.25), j(0.15), 0.42 + k * 0.03))
        rows = []
        for ring, (rr, z) in enumerate(((r * 0.3, 0.0), (r, 0.0), (r, 0.02), (r * 0.3, 0.02))):
            rows.append([c + V((math.cos(2 * math.pi * s / (teeth * 4)) * rr * (1.0 + (0.2 if (s // 2) % 2 == 0 and ring in (1, 2) else 0.0)),
                                math.sin(2 * math.pi * s / (teeth * 4)) * rr * (1.0 + (0.2 if (s // 2) % 2 == 0 and ring in (1, 2) else 0.0)), z))
                         for s in range(teeth * 4)])
        b.grid(rows, MESSING, uv_of=lambda jj, i: (i / 40, jj / 3), mask_of=lambda jj, i: 0.0, closed_u=True)
    gk_finish(b, obj)


def build_hammer():
    obj = gk.new_object("SM_IW_Hammer", SLOTS)
    b = gk.Builder()
    # lying along +X: the handle, a brass ferrule and the iron head with a face and a peen
    b.tube([V((-0.24, 0, 0.016)), V((0.08, 0, 0.018)), V((0.13, 0, 0.02))], [0.014, 0.016, 0.017], HOLZ, sides=8)
    b.tube([V((0.1, 0, 0.02)), V((0.125, 0, 0.02))], [0.02, 0.02], MESSING, sides=8)
    box(b, V((0.155, 0, 0.035)), (0.055, 0.1, 0.07), EISEN)
    b.tube([V((0.155, 0.05, 0.035)), V((0.155, 0.075, 0.035))], [0.03, 0.026], EISEN, sides=8)
    box(b, V((0.155, -0.065, 0.035)), (0.03, 0.03, 0.05), EISEN)
    gk_finish(b, obj)


def build_tongs():
    obj = gk.new_object("SM_IW_Zange", SLOTS)
    b = gk.Builder()
    # two iron arms crossing at a brass rivet, the jaws ahead (+X), the reins open behind
    for s in (-1, 1):
        b.tube([V((-0.32, s * 0.05, 0.01)), V((-0.1, s * 0.02, 0.01)), V((0.0, -s * 0.004, 0.012)), V((0.1, -s * 0.018, 0.012)),
                V((0.16, -s * 0.008, 0.012))], [0.008, 0.009, 0.012, 0.01, 0.007], EISEN, sides=6)
    b.tube([V((0, 0, 0.0)), V((0, 0, 0.026))], [0.012, 0.012], MESSING, sides=8)
    gk_finish(b, obj)


def gk_finish(b, obj):
    b.finish(obj)
    gk.export(obj)


COLOURS = {"Holz": (0.12, 0.07, 0.04), "Metall": (0.7, 0.5, 0.2), "Leder": (0.28, 0.15, 0.07), "Glut": (1.0, 0.35, 0.05),
           "Stein": (0.25, 0.23, 0.21), "Glas": (0.5, 0.6, 0.55), "Papier": (0.75, 0.66, 0.48), "Eisen": (0.08, 0.08, 0.09),
           "Fluessigkeit": (0.05, 0.07, 0.08), "Faden": (0.6, 0.52, 0.36)}


def render_preview(path):
    for name, colour in COLOURS.items():
        mat = bpy.data.materials.get(name)
        if mat is None:
            continue
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.4 if name in ("Metall", "Glas") else 0.8
        bsdf.inputs["Metallic"].default_value = 1.0 if name in ("Metall", "Eisen") else 0.0
        if name == "Glut":
            bsdf.inputs["Emission Color"].default_value = (*colour, 1.0)
            bsdf.inputs["Emission Strength"].default_value = 4.0
    objs = [o for o in bpy.data.objects if o.type == "MESH"]
    for k, obj in enumerate(objs):
        obj.location = ((k // 4) * -2.6, (k % 4) * 2.6, 0.0)
    centre = V((-2.6, 3.9, 0.0))
    bpy.ops.mesh.primitive_plane_add(size=40, location=centre)
    ground = bpy.context.active_object
    gmat = bpy.data.materials.new("Boden")
    gmat.use_nodes = True
    gmat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.15, 0.12, 0.1, 1)
    ground.data.materials.append(gmat)
    bpy.ops.object.light_add(type="SUN", rotation=(math.radians(45), math.radians(15), math.radians(200)))
    bpy.context.active_object.data.energy = 3.5
    scene = bpy.context.scene
    scene.world = bpy.data.worlds.new("Welt")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.4, 0.36, 0.33, 1)
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.7
    direction = V((-1.0, -0.8, -0.9)).normalized()
    bpy.ops.object.camera_add(location=centre - direction * 30.0)
    cam = bpy.context.active_object
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 12.0
    scene.camera = cam
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 40
    scene.render.resolution_x, scene.render.resolution_y = 1800, 1100
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


if __name__ == "__main__":
    os.makedirs(gk.OUT, exist_ok=True)
    gk.clear_scene()
    for build in (build_forge, build_bellows, build_anvil, build_barrel, build_tool_wall, build_vise, build_grinder, build_desk,
                  build_shelf, build_ammo_crate, build_scrap_crate, build_hammer, build_tongs):
        build()
    render_preview(os.path.join(gk.OUT, "vorschau_werkstatt.png"))
    gk.log("Werkstatt des Ingenieurs fertig")
