"""
The engineer's pieces, built procedurally (no Tripo) with the builder of the Glutkammer stations, exported as FBX:

  SM_Ing_Schleuder      the slingshot he starts with: a forked root branch, a leather band, a pouch
  SM_Ing_Bogen          his master piece, the bow of living wood: scar wood that still grows leaves, spun string, a leather grip,
                        his mark glowing in the riser
  SM_Ing_Armbrust       the crossbow: a stock of root wood, a prod of beetle shell, a spun string, a bone trigger
  SM_Ing_Fangeisen      the trap: a round plate, two toothed jaws held open by a spring
  SM_Ing_Knallmine      the mine: a smoke bladder, laced in leather, with glowing seams and a fuse
  SM_Ing_Schleuderturm  the turret: a tripod of root wood, a turning head with a big fork and a shell shield

Slots in this order on every piece: 0 Holz, 1 Leder, 2 Metall, 3 Faden, 4 Glut, 5 Moos.
Weapons: the grip at the origin, +X along the shot, +Z up. Devices: pivot at the bottom middle.

Run:
  blender -b --factory-startup -P vael_ingenieur_werkstuecke.py -- <output folder>
"""
import bpy, sys, os, math, random
from mathutils import Vector, Matrix

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Refugium"))
import glutkammer_probe as gk  # noqa: E402

random.seed(11)
SLOTS = ["Holz", "Leder", "Metall", "Faden", "Glut", "Moos"]
HOLZ, LEDER, METALL, FADEN, GLUT, MOOS = range(6)


def leaf(b, at, direction, size, seed):
    """A small leaf: a flattened lathe pointing along direction"""
    random.seed(seed)
    d = direction.normalized()
    side = d.cross(Vector((0, 0, 1)))
    if side.length < 0.1:
        side = d.cross(Vector((1, 0, 0)))
    side.normalize()
    rows = []
    for t in (0.0, 0.3, 0.65, 1.0):
        w = size * 0.45 * math.sin(math.pi * t) + 0.0005
        centre = at + d * size * t
        rows.append([centre - side * w, centre, centre + side * w])
    b.grid(rows, MOOS, uv_of=lambda j, i: (i / 2, j / 3), mask_of=lambda j, i: 0.0)
    rows_back = [list(reversed(r)) for r in rows]
    b.grid(rows_back, MOOS, uv_of=lambda j, i: (i / 2, j / 3), mask_of=lambda j, i: 0.0)


def box(b, centre, size, material, yaw=0.0, pitch=0.0):
    """A box as six quads, turned by yaw and pitch"""
    rot = Matrix.Rotation(yaw, 3, "Z") @ Matrix.Rotation(pitch, 3, "Y")
    hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2

    def p(x, y, z):
        return centre + rot @ Vector((x, y, z))

    faces = [
        [[p(-hx, -hy, hz), p(hx, -hy, hz)], [p(-hx, hy, hz), p(hx, hy, hz)]],
        [[p(-hx, hy, -hz), p(hx, hy, -hz)], [p(-hx, -hy, -hz), p(hx, -hy, -hz)]],
        [[p(-hx, -hy, -hz), p(hx, -hy, -hz)], [p(-hx, -hy, hz), p(hx, -hy, hz)]],
        [[p(-hx, hy, hz), p(hx, hy, hz)], [p(-hx, hy, -hz), p(hx, hy, -hz)]],
        [[p(hx, -hy, -hz), p(hx, hy, -hz)], [p(hx, -hy, hz), p(hx, hy, hz)]],
        [[p(-hx, -hy, hz), p(-hx, hy, hz)], [p(-hx, -hy, -hz), p(-hx, hy, -hz)]],
    ]
    for rows in faces:
        b.grid(rows, material, uv_of=lambda j, i: (i, j), mask_of=lambda j, i: 0.0)


def disc(b, centre, radius, normal, material, glow=0.0, sides=16):
    n = normal.normalized()
    side = n.cross(Vector((0, 0, 1)) if abs(n.z) < 0.9 else Vector((1, 0, 0))).normalized()
    up = n.cross(side)
    ring = [centre + (side * math.cos(2 * math.pi * s / sides) + up * math.sin(2 * math.pi * s / sides)) * radius for s in range(sides)]
    b.grid([ring, [centre] * sides], material, uv_of=lambda j, i: (i / sides, j), mask_of=lambda j, i: glow, closed_u=True)


def lathe(b, profile, material, centre, sides=16, glow=0.0, axis=None):
    """Rows of (radius, height) turned round the Z axis (or a given axis)"""
    rot = axis or Matrix.Identity(3)
    rows = []
    for r, z in profile:
        rows.append([centre + rot @ Vector((math.cos(2 * math.pi * s / sides) * r, math.sin(2 * math.pi * s / sides) * r, z)) for s in range(sides)])
    b.grid(rows, material, uv_of=lambda j, i: (i / sides, j / max(len(profile) - 1, 1)), mask_of=lambda j, i: glow, closed_u=True)


# ---------------------------------------------------------------- weapons

def build_sling():
    obj = gk.new_object("SM_Ing_Schleuder", SLOTS)
    b = gk.Builder()
    # handle from the grip down, the fork rising forward and up
    b.tube([Vector((-0.02, 0, -0.12)), Vector((0, 0, 0)), Vector((0.03, 0, 0.1))], [0.022, 0.024, 0.026], HOLZ, sides=9, rough=0.12, seed=1)
    for side in (-1, 1):
        b.tube([Vector((0.03, 0, 0.1)), Vector((0.05, side * 0.05, 0.17)), Vector((0.07, side * 0.075, 0.25))], [0.022, 0.018, 0.015], HOLZ, sides=8, rough=0.12, seed=2 + side)
        # band from each prong back to the pouch
        b.tube([Vector((0.07, side * 0.075, 0.245)), Vector((0.0, side * 0.04, 0.24)), Vector((-0.08, side * 0.015, 0.24))], [0.006, 0.005, 0.005], LEDER, sides=5, cap_end=False)
    # pouch
    b.tube([Vector((-0.09, -0.02, 0.24)), Vector((-0.1, 0, 0.24)), Vector((-0.09, 0.02, 0.24))], [0.012, 0.016, 0.012], LEDER, sides=6)
    # leather wrap on the grip
    for k in range(5):
        b.tube([Vector((-0.012 + k * 0.006, -0.026, -0.08 + k * 0.03)), Vector((-0.012 + k * 0.006, 0.026, -0.07 + k * 0.03))], [0.006, 0.006], LEDER, sides=5)
    b.finish(obj)
    gk.export(obj)


def build_bow():
    obj = gk.new_object("SM_Ing_Bogen", SLOTS)
    b = gk.Builder()
    # the limbs: scar wood bent back, thick at the riser, thin at the tips (the bow stands upright, string behind)
    for side in (-1, 1):
        points = []
        radii = []
        for k in range(9):
            t = k / 8
            z = side * (0.06 + t * 0.66)
            x = 0.12 * math.sin(t * math.pi * 0.55) - 0.1 * t * t
            points.append(Vector((x, 0, z)))
            radii.append(0.026 * (1 - t) + 0.008)
        b.tube(points, radii, HOLZ, sides=8, rough=0.18, seed=10 + side, flat=0.7)
        # the wood still grows: little leaves along the limbs
        for k in range(4):
            t = 0.25 + k * 0.17
            z = side * (0.06 + t * 0.66)
            x = 0.12 * math.sin(t * math.pi * 0.55) - 0.1 * t * t
            leaf(b, Vector((x + 0.01, 0.0, z)), Vector((0.6, random.uniform(-0.6, 0.6), side * 0.5)), 0.05, 20 + k + side)
        # nock and string to the tip
        tip = points[-1]
        b.tube([tip, Vector((-0.12, 0, 0)) if False else Vector((-0.11, 0, side * 0.36)), Vector((-0.1, 0, 0))], [0.0025] * 3, FADEN, sides=4, cap_end=False)
    # riser with the leather grip and his mark
    b.tube([Vector((0.02, 0, -0.08)), Vector((0.0, 0, 0)), Vector((0.02, 0, 0.08))], [0.03, 0.032, 0.03], LEDER, sides=10)
    disc(b, Vector((0.035, 0, 0.045)), 0.012, Vector((1, 0, 0)), GLUT, glow=1.0, sides=10)
    b.finish(obj)
    gk.export(obj)


def build_crossbow():
    obj = gk.new_object("SM_Ing_Armbrust", SLOTS)
    b = gk.Builder()
    # stock along +X, the grip at the origin, the butt behind
    box(b, Vector((0.18, 0, 0.04)), (0.62, 0.05, 0.05), HOLZ)
    box(b, Vector((-0.16, 0, 0.0)), (0.16, 0.045, 0.1), HOLZ, pitch=0.25)
    b.tube([Vector((0, 0, 0.02)), Vector((-0.02, 0, -0.08))], [0.022, 0.02], LEDER, sides=8)
    # prod of beetle shell across the front
    for side in (-1, 1):
        b.tube([Vector((0.46, 0, 0.05)), Vector((0.43, side * 0.18, 0.05)), Vector((0.37, side * 0.3, 0.05))], [0.022, 0.016, 0.01], METALL, sides=7, flat=0.5)
        b.tube([Vector((0.37, side * 0.3, 0.05)), Vector((0.2, side * 0.02, 0.07)), Vector((0.19, 0, 0.07))], [0.003] * 3, FADEN, sides=4, cap_end=False)
    # bolt in the groove, trigger of bone
    b.tube([Vector((0.2, 0, 0.078)), Vector((0.52, 0, 0.078))], [0.006, 0.006], HOLZ, sides=5)
    lathe(b, [(0.0, 0.0), (0.012, 0.01), (0.0, 0.035)], METALL, Vector((0.52, 0, 0.078)), sides=6, axis=Matrix.Rotation(math.pi / 2, 3, "Y"))
    b.tube([Vector((0.06, 0, 0.015)), Vector((0.05, 0, -0.04))], [0.006, 0.005], METALL, sides=5)
    b.finish(obj)
    gk.export(obj)


# ---------------------------------------------------------------- devices

def build_trap():
    obj = gk.new_object("SM_Ing_Fangeisen", SLOTS)
    b = gk.Builder()
    lathe(b, [(0.0, 0.0), (0.32, 0.0), (0.34, 0.02), (0.3, 0.035), (0.0, 0.035)], METALL, Vector((0, 0, 0)), sides=24)
    disc(b, Vector((0, 0, 0.037)), 0.08, Vector((0, 0, 1)), GLUT, glow=0.6)
    # two jaws open to the sides, teeth up
    for side in (-1, 1):
        points = [Vector((math.cos(a) * 0.3, side * math.sin(a) * 0.3 * 0.3 + side * 0.22, 0.04 + 0.14 * math.sin(a))) for a in
                  [math.pi * k / 10 for k in range(11)]]
        b.tube(points, [0.018] * len(points), METALL, sides=6)
        for k in range(1, 10):
            p = points[k]
            lathe(b, [(0.012, 0.0), (0.0, 0.05)], METALL, p, sides=5)
    # spring
    for k in range(6):
        a = k * 1.1
        b.tube([Vector((-0.34 - 0.03 * math.cos(a), 0.03 * math.sin(a), 0.03 + k * 0.012)),
                Vector((-0.34 - 0.03 * math.cos(a + 1.1), 0.03 * math.sin(a + 1.1), 0.03 + (k + 1) * 0.012))], [0.006, 0.006], METALL, sides=4)
    b.finish(obj)
    gk.export(obj)


def build_mine():
    obj = gk.new_object("SM_Ing_Knallmine", SLOTS)
    b = gk.Builder()
    profile = [(0.001, 0.0), (0.16, 0.03), (0.24, 0.12), (0.25, 0.2), (0.2, 0.3), (0.1, 0.36), (0.001, 0.38)]
    lathe(b, profile, LEDER, Vector((0, 0, 0)), sides=18)
    # glowing seams where the bladder bulges between the laces
    for k in range(6):
        a = 2 * math.pi * k / 6
        b.tube([Vector((math.cos(a) * r * 1.02, math.sin(a) * r * 1.02, z)) for r, z in profile[1:-1]], [0.012] * 5, GLUT, sides=5, mask_of=lambda t: 1.0)
    # fuse with an ember at its end
    b.tube([Vector((0, 0, 0.37)), Vector((0.03, 0, 0.45)), Vector((0.08, 0.02, 0.5))], [0.01, 0.008, 0.007], FADEN, sides=5)
    lathe(b, [(0.0, 0.0), (0.018, 0.01), (0.0, 0.03)], GLUT, Vector((0.08, 0.02, 0.5)), sides=6, glow=1.0)
    b.finish(obj)
    gk.export(obj)


def build_turret():
    obj = gk.new_object("SM_Ing_Schleuderturm", SLOTS)
    b = gk.Builder()
    # tripod
    for k in range(3):
        a = 2 * math.pi * k / 3 + 0.4
        b.tube([Vector((0, 0, 0.85)), Vector((math.cos(a) * 0.45, math.sin(a) * 0.45, 0.0))], [0.04, 0.035], HOLZ, sides=8, rough=0.15, seed=30 + k)
    lathe(b, [(0.0, 0.82), (0.12, 0.82), (0.13, 0.9), (0.0, 0.92)], METALL, Vector((0, 0, 0)), sides=14)
    # the turning head: a big fork pointing forward (+X) with its band
    b.tube([Vector((0, 0, 0.92)), Vector((0.1, 0, 1.0)), Vector((0.2, 0, 1.05))], [0.045, 0.04, 0.035], HOLZ, sides=8, rough=0.12, seed=40)
    for side in (-1, 1):
        b.tube([Vector((0.2, 0, 1.05)), Vector((0.3, side * 0.14, 1.15)), Vector((0.36, side * 0.2, 1.3))], [0.035, 0.028, 0.022], HOLZ, sides=8, rough=0.12, seed=41 + side)
        b.tube([Vector((0.36, side * 0.2, 1.29)), Vector((0.1, side * 0.08, 1.25)), Vector((-0.1, side * 0.03, 1.22))], [0.01, 0.009, 0.008], LEDER, sides=5, cap_end=False)
    b.tube([Vector((-0.12, -0.04, 1.22)), Vector((-0.14, 0, 1.22)), Vector((-0.12, 0.04, 1.22))], [0.02, 0.028, 0.02], LEDER, sides=6)
    # shell shield in front of the head
    rows = []
    for v in range(5):
        z = 0.95 + v * 0.1
        rows.append([Vector((0.28 + 0.06 * math.cos(u * math.pi / 6 - math.pi / 2) * 0, -0.32 + u * 0.107, z + 0.03 * math.sin(u * 0.9))) for u in range(7)])
    rows = [[Vector((0.3 - 0.07 * (abs(p.y) / 0.32) ** 2, p.y, p.z)) for p in row] for row in rows]
    b.grid(rows, METALL, uv_of=lambda j, i: (i / 6, j / 4), mask_of=lambda j, i: 0.0)
    b.grid([list(reversed(r)) for r in rows], METALL, uv_of=lambda j, i: (i / 6, j / 4), mask_of=lambda j, i: 0.0)
    b.finish(obj)
    gk.export(obj)


if __name__ == "__main__" and not os.environ.get("VAEL_ING_TRAGEN"):
    os.makedirs(gk.OUT, exist_ok=True)
    for build in (build_sling, build_bow, build_crossbow, build_trap, build_mine, build_turret):
        gk.clear_scene()
        build()
    gk.log("Werkstuecke des Ingenieurs fertig")


# ---------------------------------------------------------------- what he carries (2026-11-08): backpack, pouches, goggles
# Pivots: the backpack at the middle of its back side (it hangs on the upper spine, +X away from the body),
# the pouches at the belt (a ring of pouches around the hip, radius 0.17), the goggles at the middle of the band.

def build_backpack():
    obj = gk.new_object("SM_Ing_Rucksack", SLOTS)
    b = gk.Builder()
    box(b, Vector((0.12, 0, -0.05)), (0.2, 0.34, 0.42), LEDER)
    box(b, Vector((0.12, 0, 0.17)), (0.22, 0.36, 0.04), LEDER, pitch=0.12)          # flap
    for side in (-1, 1):
        b.tube([Vector((0.0, side * 0.12, 0.15)), Vector((-0.04, side * 0.13, 0.05)), Vector((-0.02, side * 0.12, -0.2))], [0.012] * 3, LEDER, sides=5)
        b.tube([Vector((0.23, side * 0.1, 0.1)), Vector((0.235, side * 0.1, -0.15))], [0.008, 0.008], METALL, sides=4)   # buckles
    # a rolled blanket on top, a coil of thread and a hammer hanging at the side
    b.tube([Vector((0.12, -0.2, 0.24)), Vector((0.12, 0.2, 0.24))], [0.06, 0.06], FADEN, sides=10)
    lathe(b, [(0.03, 0.0), (0.06, 0.0), (0.06, 0.04), (0.03, 0.04)], FADEN, Vector((0.2, 0.19, -0.12)), sides=10, axis=Matrix.Rotation(math.pi / 2, 3, "X"))
    b.tube([Vector((0.18, -0.2, 0.0)), Vector((0.18, -0.21, -0.22))], [0.01, 0.01], HOLZ, sides=5)
    box(b, Vector((0.18, -0.21, -0.24)), (0.04, 0.03, 0.08), METALL)
    b.finish(obj)
    gk.export(obj)


def build_pouches(name, material, size, count):
    obj = gk.new_object(name, SLOTS)
    b = gk.Builder()
    # the belt
    ring = [Vector((math.cos(a) * 0.17, math.sin(a) * 0.15, 0.0)) for a in [2 * math.pi * k / 16 for k in range(17)]]
    b.tube(ring, [0.015] * len(ring), LEDER, sides=4, flat=0.3, cap_end=False)
    for k in range(count):
        a = math.radians(-50 + k * (100 / max(count - 1, 1)) + 180)       # on the back and the sides of the hip
        centre = Vector((math.cos(a) * 0.19, math.sin(a) * 0.17, -0.04))
        box(b, centre, (size * 0.6, size, size * 1.2), material, yaw=a)
        box(b, centre + Vector((0, 0, size * 0.62)), (size * 0.66, size * 1.05, size * 0.12), LEDER, yaw=a)
        disc(b, centre + Vector((math.cos(a) * size * 0.31, math.sin(a) * size * 0.31, size * 0.35)), size * 0.08, Vector((math.cos(a), math.sin(a), 0)), METALL)
    b.finish(obj)
    gk.export(obj)


def build_goggles():
    obj = gk.new_object("SM_Ing_Brille", SLOTS)
    b = gk.Builder()
    for side in (-1, 1):
        centre = Vector((0.09, side * 0.035, 0.0))
        lathe(b, [(0.024, -0.01), (0.026, 0.0), (0.026, 0.012), (0.022, 0.014)], METALL, centre, sides=14, axis=Matrix.Rotation(math.pi / 2, 3, "Y"))
        disc(b, centre + Vector((0.014, 0, 0)), 0.022, Vector((1, 0, 0)), GLUT, glow=0.25, sides=14)
    # band round the head
    band = [Vector((0.09 * math.cos(a) - 0.0, 0.08 * math.sin(a), 0.0)) for a in [math.radians(-70 + k * 14) for k in range(-10, 11)]]
    b.tube(band, [0.008] * len(band), LEDER, sides=4, flat=0.4, cap_end=False)
    b.finish(obj)
    gk.export(obj)


if __name__ == "__main__" and os.environ.get("VAEL_ING_TRAGEN"):
    for build in (build_backpack, lambda: build_pouches("SM_Ing_Beutel_Leder", LEDER, 0.07, 3),
                  lambda: build_pouches("SM_Ing_Beutel_Panzer", METALL, 0.08, 4), lambda: build_pouches("SM_Ing_Beutel_Seide", FADEN, 0.09, 5), build_goggles):
        gk.clear_scene()
        build()
    gk.log("Traggut des Ingenieurs fertig")
