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
SLOTS = ["Holz", "Leder", "Metall", "Faden", "Glut", "Moos", "Messing"]
HOLZ, LEDER, METALL, FADEN, GLUT, MOOS, MESSING = range(7)
EISEN = METALL


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

# The tips of the fork (where the band is tied); the C++ ties the band there: UVaelEngineerComponent::SlingTips
SLING_TIP = Vector((0.0, 0.062, 0.175))


def build_sling():
    """The Zwille: a forked branch of scar wood held upright in the left hand, grip wrapped in leather, the prongs bound
    with thread where the band is tied. The band and its leather pouch are not part of it: the game stretches them."""
    obj = gk.new_object("SM_Ing_Schleuder", SLOTS)
    b = gk.Builder()
    random.seed(21)
    # handle: from the butt up to the crotch of the fork, a little bent like a real branch
    b.tube([Vector((0.0, 0.0, -0.12)), Vector((0.006, 0.002, -0.06)), Vector((0.0, 0.0, 0.0)), Vector((-0.004, 0.0, 0.05))],
           [0.017, 0.019, 0.02, 0.022], HOLZ, sides=10, rough=0.1, seed=1)
    b.tube([Vector((-0.004, 0.0, 0.05)), Vector((0.0, 0.0, -0.125))], [0.022, 0.017], HOLZ, sides=10, rough=0.1, seed=31)
    for side in (-1, 1):
        tip = Vector((SLING_TIP.x, side * SLING_TIP.y, SLING_TIP.z))
        b.tube([Vector((-0.004, side * 0.004, 0.05)), Vector((-0.002, side * 0.03, 0.09)), Vector((0.0, side * 0.052, 0.13)), tip],
               [0.02, 0.017, 0.015, 0.013], HOLZ, sides=9, rough=0.1, seed=2 + side)
        # thread wound round each prong below its tip, a notch for the band
        for k in range(4):
            z = tip.z - 0.012 - k * 0.006
            ring = [tip + Vector((0.016 * math.cos(a), 0.016 * math.sin(a), z - tip.z)) for a in [2 * math.pi * j / 10 for j in range(11)]]
            b.tube(ring, [0.0035] * len(ring), FADEN, sides=4, cap_end=False)
    # leather wrap on the grip in a spiral
    spiral = [Vector((0.021 * math.cos(t * 9.0), 0.021 * math.sin(t * 9.0), -0.11 + t * 0.12)) for t in [i / 40 for i in range(41)]]
    b.tube(spiral, [0.006] * len(spiral), LEDER, sides=5, cap_end=False)
    b.finish(obj)
    gk.export(obj)


def build_sling_pouch():
    """The leather pouch that holds the stone, pivot in its middle, the band ends at +-Y"""
    obj = gk.new_object("SM_Ing_Zwillenschale", SLOTS)
    b = gk.Builder()
    rows = []
    for j in range(7):
        v = j / 6 - 0.5
        row = []
        for i in range(9):
            u = i / 8 - 0.5
            row.append(Vector((-0.012 * (1 - 4 * v * v) - 0.008 * (1 - 4 * u * u), u * 0.05, v * 0.035)))
        rows.append(row)
    b.grid(rows, LEDER, uv_of=lambda j, i: (i / 8, j / 6), mask_of=lambda j, i: 0.0)
    b.grid([list(reversed(r)) for r in rows], LEDER, uv_of=lambda j, i: (i / 8, j / 6), mask_of=lambda j, i: 0.0)
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


if __name__ == "__main__" and not os.environ.get("VAEL_ING_TRAGEN") and not os.environ.get("VAEL_ING_RUESTUNG") and not os.environ.get("VAEL_ING_ZWEIGE") and not os.environ.get("VAEL_ING_FALLEN"):
    os.makedirs(gk.OUT, exist_ok=True)
    for build in (build_sling, build_sling_pouch, build_bow, build_crossbow, build_trap, build_mine, build_turret):
        gk.clear_scene()
        build()
    gk.log("Werkstuecke des Ingenieurs fertig")


# ---------------------------------------------------------------- what he carries (2026-11-08): backpack, pouches, goggles
# Pivots: the backpack at the middle of its back side (it hangs on the upper spine, +X away from the body),
# the pouches at the belt (a ring of pouches around the hip, radius 0.17), the goggles at the middle of the band.

def build_backpack():
    """The carried workshop: a wooden frame (like a pedlar's back rack) holding a leather chest with brass corners and buckles,
    the leather tool roll (Werkrolle) strapped on top, a small anvil and a hammer on one side, ammo jars in a brass rack on the
    other. Pivot at the middle of its back side, +X away from the body."""
    obj = gk.new_object("SM_Ing_Rucksack", SLOTS)
    b = gk.Builder()
    random.seed(77)
    # the frame: two uprights against the back, a crossbar top and bottom, a shelf for the chest
    for side in (-1, 1):
        b.tube([Vector((0.02, side * 0.16, -0.3)), Vector((0.03, side * 0.165, 0.0)), Vector((0.02, side * 0.16, 0.34))], [0.016, 0.016, 0.015], HOLZ, sides=6)
        b.tube([Vector((0.03, side * 0.16, -0.28)), Vector((0.22, side * 0.16, -0.28))], [0.012, 0.012], HOLZ, sides=6)
    for z in (-0.28, 0.3):
        b.tube([Vector((0.02, -0.17, z)), Vector((0.02, 0.17, z))], [0.013, 0.013], HOLZ, sides=6)
    # the leather chest with brass corners and two buckled straps
    box(b, Vector((0.12, 0, -0.06)), (0.18, 0.3, 0.38), LEDER)
    box(b, Vector((0.12, 0, 0.145)), (0.2, 0.32, 0.03), LEDER, pitch=0.06)
    for sy in (-1, 1):
        for sz in (-1, 1):
            box(b, Vector((0.21, sy * 0.145, -0.06 + sz * 0.18)), (0.02, 0.03, 0.03), MESSING)
        b.tube([Vector((0.215, sy * 0.08, 0.15)), Vector((0.215, sy * 0.08, -0.22))], [0.009, 0.009], LEDER, sides=4, flat=0.3)
        box(b, Vector((0.22, sy * 0.08, -0.02)), (0.012, 0.035, 0.03), MESSING)
    # the Werkrolle on top: a rolled leather tool roll with a brass clasp and tool handles peeking out
    b.tube([Vector((0.1, -0.2, 0.36)), Vector((0.1, 0.2, 0.36))], [0.055, 0.055], LEDER, sides=12)
    b.tube([Vector((0.1, -0.03, 0.36)), Vector((0.1, 0.03, 0.36))], [0.06, 0.06], MESSING, sides=12)
    for k in range(4):
        b.tube([Vector((0.1 + 0.02 * (k - 1.5), -0.2, 0.37)), Vector((0.1 + 0.02 * (k - 1.5), -0.25, 0.39))], [0.007, 0.006], HOLZ, sides=5)
    # one side: a small anvil and a hammer on hooks
    box(b, Vector((0.1, -0.205, -0.15)), (0.09, 0.05, 0.05), EISEN)
    b.tube([Vector((0.145, -0.205, -0.15)), Vector((0.19, -0.205, -0.155))], [0.02, 0.005], EISEN, sides=6)
    b.tube([Vector((0.06, -0.2, 0.08)), Vector((0.06, -0.21, -0.08))], [0.008, 0.008], HOLZ, sides=5)
    box(b, Vector((0.06, -0.21, -0.1)), (0.03, 0.025, 0.06), EISEN)
    # the other side: a brass rack of ammo jars with coloured fillings and leather stoppers
    box(b, Vector((0.11, 0.2, -0.02)), (0.12, 0.04, 0.01), MESSING)
    box(b, Vector((0.11, 0.2, -0.16)), (0.12, 0.04, 0.01), MESSING)
    for k, fill in enumerate((GLUT, MOOS, FADEN)):
        x = 0.07 + k * 0.04
        lathe(b, [(0.016, 0.0), (0.018, 0.01), (0.018, 0.11), (0.01, 0.12), (0.0, 0.125)], FADEN if fill != GLUT else GLUT,
              Vector((x, 0.2, -0.155)), sides=8, glow=1.0 if fill == GLUT else 0.0)
        lathe(b, [(0.011, 0.12), (0.011, 0.14), (0.0, 0.14)], LEDER, Vector((x, 0.2, -0.155)), sides=8)
    # shoulder straps
    for side in (-1, 1):
        b.tube([Vector((0.0, side * 0.11, 0.3)), Vector((-0.05, side * 0.12, 0.18)), Vector((-0.04, side * 0.12, -0.05)), Vector((0.0, side * 0.12, -0.26))],
               [0.012] * 4, LEDER, sides=4, flat=0.3)
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


if __name__ == "__main__" and os.environ.get("VAEL_ING_TRAGEN") and not os.environ.get("VAEL_ING_RUESTUNG") and not os.environ.get("VAEL_ING_ZWEIGE"):
    for build in (build_backpack, lambda: build_pouches("SM_Ing_Beutel_Leder", LEDER, 0.07, 3),
                  lambda: build_pouches("SM_Ing_Beutel_Panzer", METALL, 0.08, 4), lambda: build_pouches("SM_Ing_Beutel_Seide", FADEN, 0.09, 5), build_goggles):
        gk.clear_scene()
        build()
    gk.log("Traggut des Ingenieurs fertig")


# ---------------------------------------------------------------- armour sets (2026-11-08): the look of the creature it is made from
# Every piece is built around its own origin: the helmet around the middle of the head, the chest plate around the middle of the
# chest (+X ahead), bracers and greaves as a sleeve along +Z (the C++ turns them along the forearm and the shin).
SETS = ["Rollpanzer", "Sichelklinge", "Russfeder", "Blutschuppen"]


def sleeve(b, length, radius, material, rings=1, flare=1.0):
    """A shell around a limb along Z, open at both ends"""
    for k in range(rings):
        z0 = -length / 2 + k * length / rings
        z1 = z0 + length / rings * 1.08
        r0 = radius * (1.0 + 0.06 * k)
        lathe(b, [(r0 * flare, z0), (r0 * 1.08, (z0 + z1) / 2), (r0, z1)], material, Vector((0, 0, 0)), sides=14)
        lathe(b, [(r0 * 0.94, z1), (r0 * 0.94 * flare, z0)], LEDER, Vector((0, 0, 0)), sides=14)


def set_detail(b, set_name, centre, outward, up, scale):
    """What makes the set: blades, feathers or scales on a point of the piece"""
    if set_name == "Sichelklinge":
        lathe(b, [(0.012 * scale, 0.0), (0.0, 0.07 * scale)], METALL, centre, sides=5,
              axis=Matrix.Rotation(Vector((0, 0, 1)).rotation_difference(outward + up * 0.4).angle, 3, Vector((0, 0, 1)).cross(outward + up * 0.4).normalized()))
    elif set_name == "Russfeder":
        for k in range(3):
            leaf(b, centre + up * 0.01 * k, outward * 0.5 + up * (0.8 + 0.1 * k), 0.08 * scale, int(centre.x * 100 + k))
    elif set_name == "Blutschuppen":
        disc(b, centre + outward * 0.004, 0.018 * scale, outward, GLUT, glow=0.1, sides=6)
    else:
        disc(b, centre + outward * 0.004, 0.012 * scale, outward, METALL, sides=8)


def build_armor_piece(set_name, place):
    obj = gk.new_object("SM_Ing_R_{}_{}".format(set_name, place), SLOTS)
    b = gk.Builder()
    rings = 3 if set_name == "Rollpanzer" else 1
    if place == "Kopf":
        # a cap over the skull with a brim at the brow and the neck
        lathe(b, [(0.0, 0.16), (0.07, 0.15), (0.11, 0.1), (0.12, 0.04), (0.125, 0.0)], METALL if set_name != "Russfeder" else LEDER, Vector((-0.01, 0, 0)), sides=18)
        for k in range(7):
            a = math.radians(-60 + k * 20)
            p = Vector((math.cos(a) * 0.11 - 0.01, math.sin(a) * 0.11, 0.1))
            set_detail(b, set_name, p, Vector((math.cos(a) * 0.3, math.sin(a), 0.6)).normalized(), Vector((0, 0, 1)), 1.0)
    elif place == "Brust":
        # a curved plate over the chest and one over the back, joined by straps over the shoulders
        for side, depth in ((1, 0.13), (-1, 0.12)):
            rows = []
            for v in range(5):
                z = -0.18 + v * 0.09
                rows.append([Vector((side * (depth - 0.06 * (abs(u - 3) / 3) ** 2), -0.17 + u * 0.057, z)) for u in range(7)])
            if side < 0:
                rows = [list(reversed(r)) for r in rows]
            b.grid(rows, METALL if set_name != "Russfeder" else LEDER, uv_of=lambda j, i: (i / 6, j / 4), mask_of=lambda j, i: 0.0)
            for v in range(1, 5, 1 if set_name == "Blutschuppen" else 2):
                for u in range(1, 6, 2):
                    p = rows[v][u]
                    set_detail(b, set_name, p, Vector((side, 0, 0)), Vector((0, 0, 1)), 1.0)
        for y in (-0.12, 0.12):
            b.tube([Vector((0.12, y, 0.18)), Vector((0.0, y * 1.05, 0.24)), Vector((-0.11, y, 0.18))], [0.012] * 3, LEDER, sides=5)
        if rings > 1:
            for z in (-0.12, -0.03, 0.06):
                b.tube([Vector((0.135, -0.16, z)), Vector((0.145, 0, z)), Vector((0.135, 0.16, z))], [0.01] * 3, METALL, sides=5)
    else:
        length, radius = (0.2, 0.048) if place == "Arm" else (0.26, 0.06)
        sleeve(b, length, radius, METALL if set_name != "Russfeder" else LEDER, rings=rings, flare=1.1)
        for k in range(4 if set_name != "Blutschuppen" else 10):
            a = math.radians(k * (360 / (4 if set_name != "Blutschuppen" else 10)))
            z = -length * 0.25 + (k % 3) * length * 0.2
            p = Vector((math.cos(a) * radius * 1.12, math.sin(a) * radius * 1.12, z))
            set_detail(b, set_name, p, Vector((math.cos(a), math.sin(a), 0)), Vector((0, 0, 1)), 0.8)
    b.finish(obj)
    gk.export(obj)


if __name__ == "__main__" and os.environ.get("VAEL_ING_RUESTUNG") and not os.environ.get("VAEL_ING_ZWEIGE"):
    for set_name in SETS:
        for place in ("Kopf", "Brust", "Arm", "Bein"):
            gk.clear_scene()
            build_armor_piece(set_name, place)
    gk.log("Ruestungen des Ingenieurs fertig")


# ---------------------------------------------------------------- weapon branches (2026-11-08): the base weapon with what the branch adds
_EXTRA = None
_NAME = None
_orig_finish = gk.Builder.finish
_orig_new = gk.new_object


def _finish(self, obj):
    if _EXTRA is not None:
        _EXTRA(self)
    _orig_finish(self, obj)


def _new(name, slots):
    return _orig_new(_NAME or name, slots)


def embers(b):
    # a glowing gland bound into the crotch of the fork, embers along the prongs
    lathe(b, [(0.0, -0.018), (0.016, -0.008), (0.016, 0.008), (0.0, 0.018)], GLUT, Vector((0.012, 0.0, 0.06)), sides=8, glow=1.0)
    for side in (-1, 1):
        for k in range(3):
            disc(b, Vector((0.012, side * (0.02 + 0.014 * k), 0.09 + 0.025 * k)), 0.006, Vector((1, 0, 0.2)), GLUT, glow=1.0, sides=6)


def extra_strings(b):
    # brass caps on the prong tips and a brace across the fork: it takes a doubled band
    for side in (-1, 1):
        lathe(b, [(0.0, 0.0), (0.016, 0.01), (0.012, 0.03), (0.0, 0.034)], METALL, Vector((SLING_TIP.x, side * SLING_TIP.y, SLING_TIP.z - 0.01)), sides=8)
    b.tube([Vector((0.0, -0.035, 0.1)), Vector((0.0, 0.035, 0.1))], [0.006, 0.006], METALL, sides=6)


def thorns(b):
    for side in (-1, 1):
        for k in range(5):
            t = 0.2 + k * 0.15
            z = side * (0.06 + t * 0.66)
            x = 0.12 * math.sin(t * math.pi * 0.55) - 0.1 * t * t
            lathe(b, [(0.008, 0.0), (0.0, 0.04)], HOLZ, Vector((x + 0.015, 0, z)), sides=5, axis=Matrix.Rotation(math.pi / 2, 3, "Y"))


def feathers(b):
    for side in (-1, 1):
        for k in range(3):
            leaf(b, Vector((-0.02, 0, side * (0.1 + 0.03 * k))), Vector((-0.8, (k - 1) * 0.4, side * 0.5)), 0.07, 40 + k + side)


def plates(b):
    for x in (0.0, 0.15, 0.3):
        box(b, Vector((x, 0, 0.075)), (0.08, 0.075, 0.03), METALL)
    box(b, Vector((0.43, 0, 0.05)), (0.05, 0.12, 0.08), METALL)


if __name__ == "__main__" and os.environ.get("VAEL_ING_ZWEIGE"):
    gk.Builder.finish = _finish
    gk.new_object = _new
    for name, base, extra in (("SM_Ing_Glutschleuder", build_sling, embers), ("SM_Ing_Spannschleuder", build_sling, extra_strings),
                              ("SM_Ing_Dornenbogen", build_bow, thorns), ("SM_Ing_Sehnenbogen", build_bow, feathers),
                              ("SM_Ing_SchwereArmbrust", build_crossbow, plates)):
        _NAME, _EXTRA = name, extra
        gk.clear_scene()
        base()
    gk.log("Zweige des Ingenieurs fertig")


# ---------------------------------------------------------------- the Fallen-Paket (2026-11-25): VAEL_ING_FALLEN=1
# Devices: pivot at the bottom middle. The hook: pivot at its hub, +X the way it flies. The whistle: pivot in the middle,
# +X towards the mouthpiece.

def build_buried_mine():
    """Grabmine: a domed crust plate with glowing cracks, the gland peeking out in the middle, thistle down at the rim"""
    obj = gk.new_object("SM_Ing_Grabmine", SLOTS)
    b = gk.Builder()
    random.seed(71)
    sides = 22
    jag = [1.0 + random.uniform(-0.12, 0.1) for _ in range(sides)]
    rows = []
    for r, z in ((0.0, 0.075), (0.12, 0.07), (0.22, 0.055), (0.3, 0.03), (0.34, 0.008), (0.33, 0.0)):
        rows.append([Vector((math.cos(2 * math.pi * s / sides) * r * jag[s], math.sin(2 * math.pi * s / sides) * r * jag[s], z)) for s in range(sides)])
    b.grid(rows, METALL, uv_of=lambda j, i: (i / sides, j / 5), mask_of=lambda j, i: 0.0, closed_u=True)
    # cracks from the gland to the rim, glowing
    for k in range(7):
        a = 2 * math.pi * k / 7 + random.uniform(-0.2, 0.2)
        pts = []
        for t in (0.07, 0.15, 0.23, 0.31):
            wob = random.uniform(-0.12, 0.12)
            r = t
            z = 0.075 - 0.07 * (t / 0.34) ** 2 + 0.004
            pts.append(Vector((math.cos(a + wob) * r, math.sin(a + wob) * r, z)))
        b.tube(pts, [0.012, 0.01, 0.008, 0.005], GLUT, sides=5, mask_of=lambda t: 1.0)
    # the gland in the middle
    lathe(b, [(0.0, 0.06), (0.05, 0.075), (0.055, 0.1), (0.035, 0.12), (0.0, 0.125)], GLUT, Vector((0, 0, 0)), sides=10, glow=1.0)
    # tufts of thistle down round the rim
    for k in range(9):
        a = 2 * math.pi * k / 9 + 0.2
        base = Vector((math.cos(a) * 0.31, math.sin(a) * 0.31, 0.02))
        for m in range(4):
            tip = base + Vector((math.cos(a + m * 0.5) * 0.06, math.sin(a + m * 0.5) * 0.06, 0.05 + 0.02 * m))
            b.tube([base, tip], [0.004, 0.001], FADEN, sides=3)
    b.finish(obj)
    gk.export(obj)


def build_smoke_pot():
    """Rauchtopf: a squat pot of sealing wax, two smoke bladders bulging out of its mouth, a cord and a red seal"""
    obj = gk.new_object("SM_Ing_Rauchtopf", SLOTS)
    b = gk.Builder()
    lathe(b, [(0.0, 0.0), (0.11, 0.0), (0.15, 0.05), (0.16, 0.12), (0.12, 0.2), (0.1, 0.22), (0.0, 0.22)], LEDER, Vector((0, 0, 0)), sides=16)
    # two bladders, grey-brown, pushing out of the neck
    for k, (dx, dy, s) in enumerate(((-0.04, 0.02, 1.0), (0.05, -0.03, 0.85))):
        c = Vector((dx, dy, 0.27))
        lathe(b, [(0.0, -0.06 * s), (0.07 * s, -0.03 * s), (0.08 * s, 0.02 * s), (0.05 * s, 0.07 * s), (0.0, 0.09 * s)], MOOS, c, sides=12)
    # cord round the neck and the wax seal with its glowing mark
    ring = [Vector((math.cos(2 * math.pi * s / 14) * 0.112, math.sin(2 * math.pi * s / 14) * 0.112, 0.205)) for s in range(15)]
    b.tube(ring, [0.01] * len(ring), FADEN, sides=5)
    disc(b, Vector((0.158, 0.0, 0.11)), 0.045, Vector((1, 0, 0)), GLUT, glow=0.35)
    b.tube([Vector((0.11, 0, 0.205)), Vector((0.16, 0.02, 0.16)), Vector((0.165, 0.0, 0.11))], [0.006, 0.006, 0.006], FADEN, sides=4)
    b.finish(obj)
    gk.export(obj)


def build_whistle():
    """Lockpfeife: a hollow quill, a leather screech bladder at its back, brass bands, a tongue of spores at the mouth"""
    obj = gk.new_object("SM_Ing_Lockpfeife", SLOTS)
    b = gk.Builder()
    quill = [Vector((x, 0, 0.004 * math.sin(x * 20))) for x in (-0.09, -0.04, 0.0, 0.05, 0.1, 0.14)]
    b.tube(quill, [0.022, 0.021, 0.019, 0.016, 0.013, 0.01], FADEN, sides=10)
    for x in (-0.02, 0.06):
        lathe(b, [(0.024, -0.008), (0.026, 0.0), (0.024, 0.008)], MESSING, Vector((x, 0, 0)), sides=12,
              axis=Matrix.Rotation(math.pi / 2, 3, "Y"))
    # the screech bladder, wrinkled leather, at the back
    lathe(b, [(0.0, 0.0), (0.04, 0.01), (0.055, 0.04), (0.05, 0.07), (0.025, 0.09), (0.0, 0.095)], LEDER, Vector((-0.085, 0, 0)), sides=12,
          axis=Matrix.Rotation(-math.pi / 2, 3, "Y"))
    # spore tongue at the mouthpiece, a faint glow
    lathe(b, [(0.0, 0.0), (0.012, 0.005), (0.006, 0.02), (0.0, 0.025)], MOOS, Vector((0.145, 0, 0.0)), sides=8,
          axis=Matrix.Rotation(math.pi / 2, 3, "Y"), glow=0.4)
    # a cord to hang it by
    b.tube([Vector((-0.02, 0, 0.024)), Vector((0.0, 0, 0.06)), Vector((0.06, 0, 0.024))], [0.004, 0.004, 0.004], FADEN, sides=4)
    b.finish(obj)
    gk.export(obj)


def build_hook():
    """Wurfhaken: a harpy claw as the hub, three long monkey fingers curling back as hooks, a knot of fibre rope behind"""
    obj = gk.new_object("SM_Ing_Wurfhaken", SLOTS)
    b = gk.Builder()
    # hub: the claw's knuckle, bound with brass
    b.tube([Vector((-0.06, 0, 0)), Vector((0.0, 0, 0)), Vector((0.05, 0, 0))], [0.028, 0.034, 0.026], LEDER, sides=10)
    lathe(b, [(0.036, -0.01), (0.038, 0.0), (0.036, 0.01)], MESSING, Vector((-0.01, 0, 0)), sides=12, axis=Matrix.Rotation(math.pi / 2, 3, "Y"))
    # three fingers: forward, then curling back, dark horn tipped with a claw
    for k in range(3):
        a = 2 * math.pi * k / 3 + 0.3
        out = Vector((0, math.cos(a), math.sin(a)))
        pts = [Vector((0.04, 0, 0)) + out * 0.02, Vector((0.12, 0, 0)) + out * 0.06, Vector((0.17, 0, 0)) + out * 0.12,
               Vector((0.14, 0, 0)) + out * 0.18, Vector((0.08, 0, 0)) + out * 0.19]
        b.tube(pts, [0.016, 0.014, 0.012, 0.009, 0.004], METALL, sides=7, rough=0.08, seed=80 + k)
        for j in (1, 2):
            p = pts[j]
            lathe(b, [(0.017, -0.006), (0.019, 0.0), (0.017, 0.006)], FADEN, p, sides=8)
    # rope knot and a short stub of the rope
    lathe(b, [(0.0, 0.0), (0.03, 0.01), (0.032, 0.03), (0.0, 0.045)], FADEN, Vector((-0.06, 0, 0)), sides=10, axis=Matrix.Rotation(-math.pi / 2, 3, "Y"))
    b.tube([Vector((-0.1, 0, 0)), Vector((-0.16, 0.01, -0.01)), Vector((-0.22, 0.0, -0.02))], [0.012, 0.011, 0.011], FADEN, sides=6)
    b.finish(obj)
    gk.export(obj)




# ---------------------------------------------------------------- Umwelt devices (2026-11-25), also under VAEL_ING_FALLEN
# Moving parts are pieces of their own so the game can animate them: the nozzle of the Glutzapfer (pivot where it sits on the
# chimney, +X the way it spits), the lid of the Säurefass (pivot at its middle, it pops off).

def build_tapper():
    """Glutzapfer: a squat chimney of slag iron on three clawed feet, riveted bands, a glowing grate at the bottom, sinew stays"""
    obj = gk.new_object("SM_Ing_Glutzapfer", SLOTS)
    b = gk.Builder()
    lathe(b, [(0.0, 0.0), (0.2, 0.0), (0.22, 0.06), (0.17, 0.3), (0.13, 0.62), (0.15, 0.66), (0.0, 0.66)], METALL, Vector((0, 0, 0)), sides=14)
    for z in (0.12, 0.36, 0.58):
        r = 0.22 - z * 0.14
        lathe(b, [(r, z - 0.015), (r + 0.015, z), (r, z + 0.015)], MESSING, Vector((0, 0, 0)), sides=14)
    # glowing grate where it takes the fire in
    for k in range(4):
        a = 2 * math.pi * k / 4 + 0.4
        disc(b, Vector((math.cos(a) * 0.205, math.sin(a) * 0.205, 0.08)), 0.045, Vector((math.cos(a), math.sin(a), 0.1)), GLUT, glow=1.0, sides=8)
    # three feet like claws gripping the ground
    for k in range(3):
        a = 2 * math.pi * k / 3
        out = Vector((math.cos(a), math.sin(a), 0))
        b.tube([out * 0.18 + Vector((0, 0, 0.08)), out * 0.3 + Vector((0, 0, 0.04)), out * 0.36 + Vector((0, 0, -0.01))], [0.03, 0.022, 0.01], METALL, sides=6)
    # sinew stays from the top to the feet
    for k in range(3):
        a = 2 * math.pi * k / 3 + math.pi / 3
        out = Vector((math.cos(a), math.sin(a), 0))
        b.tube([out * 0.12 + Vector((0, 0, 0.6)), out * 0.26 + Vector((0, 0, 0.25)), out * 0.32 + Vector((0, 0, 0.02))], [0.008, 0.008, 0.008], FADEN, sides=4)
    b.finish(obj)
    gk.export(obj)


def build_tapper_nozzle():
    """The nozzle: the Glutkiefer, jaws open forward, glowing inside; sits on the chimney (pivot at its base)"""
    obj = gk.new_object("SM_Ing_Glutzapfer_Duese", SLOTS)
    b = gk.Builder()
    lathe(b, [(0.0, 0.0), (0.14, 0.0), (0.15, 0.05), (0.0, 0.07)], METALL, Vector((0, 0, 0)), sides=14)
    for side in (-1, 1):
        pts = [Vector((0.0, 0, 0.06 + side * 0.03)), Vector((0.12, 0, 0.1 + side * 0.07)), Vector((0.26, 0, 0.12 + side * 0.1)), Vector((0.32, 0, 0.11 + side * 0.08))]
        b.tube(pts, [0.07, 0.06, 0.04, 0.02], METALL, sides=8, flat=0.55, rough=0.1, seed=60 + side)
        for k in range(1, 4):
            p = pts[k]
            lathe(b, [(0.012, 0.0), (0.0, -side * 0.045)], FADEN, p + Vector((0, 0, -side * 0.02)), sides=5)
    # the glow inside the jaw
    lathe(b, [(0.0, 0.0), (0.05, 0.02), (0.04, 0.12), (0.0, 0.16)], GLUT, Vector((0.02, 0, 0.12)), sides=8, glow=1.0, axis=Matrix.Rotation(math.pi / 2, 3, "Y"))
    b.finish(obj)
    gk.export(obj)


def build_acid_barrel():
    """Säurefass: a small bellied barrel of dark wood, sealed with wax, green acid weeping from the seams"""
    obj = gk.new_object("SM_Ing_Saeurefass", SLOTS)
    b = gk.Builder()
    prof = [(0.0, 0.0), (0.15, 0.0), (0.18, 0.08), (0.2, 0.22), (0.18, 0.36), (0.15, 0.44), (0.0, 0.44)]
    lathe(b, prof, HOLZ, Vector((0, 0, 0)), sides=16)
    for z in (0.06, 0.38):
        r = 0.17 + (0.01 if z > 0.2 else 0.0)
        lathe(b, [(r, z - 0.02), (r + 0.012, z), (r, z + 0.02)], METALL, Vector((0, 0, 0)), sides=16)
    # wax drips at the top rim and green streaks weeping down
    for k in range(7):
        a = 2 * math.pi * k / 7 + 0.3
        top = Vector((math.cos(a) * 0.15, math.sin(a) * 0.15, 0.44))
        b.tube([top, top + Vector((math.cos(a) * 0.02, math.sin(a) * 0.02, -0.06))], [0.014, 0.008], LEDER, sides=5)
    for k in range(5):
        a = 2 * math.pi * k / 5 + 1.0
        pts = [Vector((math.cos(a) * r * 1.03, math.sin(a) * r * 1.03, z)) for r, z in ((0.19, 0.3), (0.2, 0.2), (0.185, 0.1))]
        b.tube(pts, [0.008, 0.01, 0.006], MOOS, sides=4, mask_of=lambda t: 0.8)
    b.finish(obj)
    gk.export(obj)


def build_acid_lid():
    """The lid with its wax seal and bung (pivot at its middle); it pops off when the barrel bursts"""
    obj = gk.new_object("SM_Ing_Saeurefass_Deckel", SLOTS)
    b = gk.Builder()
    lathe(b, [(0.0, 0.0), (0.15, 0.0), (0.15, 0.02), (0.0, 0.025)], HOLZ, Vector((0, 0, 0)), sides=16)
    lathe(b, [(0.0, 0.02), (0.05, 0.02), (0.045, 0.045), (0.0, 0.05)], LEDER, Vector((0.05, 0.0, 0)), sides=10)
    disc(b, Vector((0.05, 0.0, 0.051)), 0.03, Vector((0, 0, 1)), GLUT, glow=0.3, sides=10)
    b.finish(obj)
    gk.export(obj)


def build_douse():
    """Löschpulver: a pale wax pouch bulging with powder, tied at the neck, a little powder dusting out"""
    obj = gk.new_object("SM_Ing_Loeschpulver", SLOTS)
    b = gk.Builder()
    random.seed(91)
    sides = 14
    rows = []
    for r, z in ((0.001, 0.0), (0.1, 0.02), (0.14, 0.08), (0.13, 0.15), (0.07, 0.2), (0.035, 0.22)):
        rows.append([Vector((math.cos(2 * math.pi * s / sides) * r * (1 + 0.08 * math.sin(3 * s)), math.sin(2 * math.pi * s / sides) * r * (1 + 0.08 * math.sin(3 * s)), z)) for s in range(sides)])
    b.grid(rows, FADEN, uv_of=lambda j, i: (i / sides, j / 5), mask_of=lambda j, i: 0.0, closed_u=True)
    # the tied neck and its tuft
    ring = [Vector((math.cos(2 * math.pi * s / 10) * 0.04, math.sin(2 * math.pi * s / 10) * 0.04, 0.215)) for s in range(11)]
    b.tube(ring, [0.009] * 11, LEDER, sides=4)
    for k in range(6):
        a = 2 * math.pi * k / 6
        b.tube([Vector((0, 0, 0.22)), Vector((math.cos(a) * 0.05, math.sin(a) * 0.05, 0.28))], [0.012, 0.004], FADEN, sides=4)
    b.finish(obj)
    gk.export(obj)


if __name__ == "__main__" and os.environ.get("VAEL_ING_FALLEN"):
    os.makedirs(gk.OUT, exist_ok=True)
    for build in (build_buried_mine, build_smoke_pot, build_whistle, build_hook, build_tapper, build_tapper_nozzle, build_acid_barrel, build_acid_lid, build_douse):
        gk.clear_scene()
        build()
    gk.log("Fallen-Paket des Ingenieurs fertig")
