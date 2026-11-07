"""
Refugium Glutkammer: the stations of the hall, built procedurally (no Tripo), exported as FBX next to the look-test pieces.

  SM_GK_Wurzeltisch   the root table of the Hexenküche: a slab of grown root wood on gnarled root legs (0 Holz)
  SM_GK_Tischschmuck  what lies on it: an open book, vials and flasks, mortar and pestle, a small kettle on a tripod over embers,
                      herb bundles, a knife, tongs and candles (0 Holz, 1 Glas, 2 Fluessigkeit, 3 Papier, 4 Metall, 5 Kraut, 6 Wachs, 7 Glut)
  SM_GK_Kraeuterbogen a root arch over the table with herbs and vials hanging from it (0 Holz, 1 Glas, 2 Fluessigkeit, 5 Kraut)
  SM_GK_Werkbank      the engineer's bench: root wood, a vise, bone gears, a tool rack, slag bars, a half-built sling (0 Holz, 4 Metall, 8 Leder)
  SM_GK_Schrein       the prophet's shrine: stacked stones, candles, an incense bowl with embers, hanging charms (9 Stein, 6 Wachs, 10 Stoff, 7 Glut)
  SM_GK_Bau           the tamer's den: a mound of roots and moss with a dark entrance, a straw nest and a trough (0 Holz, 11 Moos, 12 Stroh)
  SM_GK_Glutbrunnen   the mage's glow well: a ring of rune stones round an ember basin, crystals floating over it (9 Stein, 7 Glut)
  SM_GK_Helfernest    the helper nest: a woven basket of roots on a stump, glowing inside (0 Holz, 7 Glut, 12 Stroh)

Every piece has all slots of the list in the same order (unused ones stay empty), so one set of materials fits all.
UV channel 1 U carries a mask: liquids = hue (0..1) for the liquid material, embers and candle flames = glow, stones = rune glow.
Pivots at the bottom middle, +X is the side facing the room centre.

Run:
  blender -b --factory-startup -P glutkammer_stations.py -- <output folder>
"""
import bpy, bmesh, sys, os, math, random
from mathutils import Vector, Matrix, noise

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import glutkammer_probe as gk  # noqa: E402  (Builder, tube, export, helpers)

OUT = gk.OUT
random.seed(7)
SLOTS = ["Holz", "Glas", "Fluessigkeit", "Papier", "Metall", "Kraut", "Wachs", "Glut", "Leder", "Stein", "Stoff", "Moos", "Stroh"]
HOLZ, GLAS, FLUID, PAPIER, METALL, KRAUT, WACHS, GLUT, LEDER, STEIN, STOFF, MOOS, STROH = range(13)


def lathe(b, profile, material, centre=Vector((0, 0, 0)), sides=20, mask=0.0, tilt=None, wobble_amount=0.0, seed=0):
    """Surface of revolution: profile is a list of (radius, height); tilt is a rotation matrix applied around the centre"""
    rows = []
    for r, z in profile:
        row = []
        for s in range(sides):
            a = 2 * math.pi * s / sides
            p = Vector((math.cos(a) * r, math.sin(a) * r, z))
            if wobble_amount > 0:
                p = p + Vector((math.cos(a), math.sin(a), 0)) * r * wobble_amount * noise.noise(p * 6.0 + Vector((seed, 0, 0)))
            if tilt is not None:
                p = tilt @ p
            row.append(centre + p)
        rows.append(row)
    b.grid(rows, material, uv_of=lambda j, i: (i / sides, j / max(len(profile) - 1, 1)), mask_of=lambda j, i: mask, closed_u=True)
    # close the bottom and the top if the profile starts or ends at radius 0 (already a point) or leave open otherwise
    return rows


def disc_cap(b, ring, material, mask=0.0):
    centre = sum(ring, Vector()) / len(ring)
    c = b.bm.verts.new(centre)
    verts = [b.bm.verts.new(p) for p in ring]
    for i in range(len(verts)):
        try:
            f = b.bm.faces.new((verts[i], verts[(i + 1) % len(verts)], c))
        except ValueError:
            continue
        f.material_index = material
        for loop in f.loops:
            loop[b.uv].uv = (0.5 + (loop.vert.co - centre).x, 0.5 + (loop.vert.co - centre).y)
            loop[b.col] = (mask, mask, mask, 1.0)
            loop[b.mask].uv = (mask, mask)


def box(b, centre, size, material, yaw=0.0, mask=0.0, bevel_noise=0.0, seed=0):
    """Box as 6 grids, slightly irregular if bevel_noise > 0"""
    sx, sy, sz = size[0] / 2, size[1] / 2, size[2] / 2
    rot = Matrix.Rotation(yaw, 3, 'Z')

    def corner(x, y, z):
        p = Vector((x * sx, y * sy, z * sz))
        if bevel_noise > 0:
            p = p + noise.noise_vector(p * 3.0 + Vector((seed, seed, seed))) * bevel_noise
        return centre + rot @ p

    faces = [((-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)), ((-1, 1, -1), (1, 1, -1), (1, -1, -1), (-1, -1, -1)),
             ((-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)), ((1, 1, -1), (-1, 1, -1), (-1, 1, 1), (1, 1, 1)),
             ((1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)), ((-1, 1, -1), (-1, -1, -1), (-1, -1, 1), (-1, 1, 1))]
    for quad in faces:
        rows = [[corner(*quad[0]), corner(*quad[1])], [corner(*quad[3]), corner(*quad[2])]]
        b.grid(rows, material, uv_of=lambda j, i: (i * max(size[0], size[1]), j * size[2]), mask_of=lambda j, i: mask)


def root_curve(start, end, sag, seed, steps=14):
    pts = []
    for k in range(steps + 1):
        t = k / steps
        p = start.lerp(end, t) + Vector((0, 0, -sag * math.sin(math.pi * t)))
        pts.append(gk.wobble(p, 1.2, 0.05 * math.sin(math.pi * t) + 0.01, seed))
    return pts


def herb_bundle(b, top, length, seed):
    """Bundle of dried herbs hanging head down from a string: stems as thin tubes, leaves as small flat blades"""
    b.tube([top, top + Vector((0, 0, -0.12))], [0.006, 0.006], LEDER, sides=5, cap_end=False)
    tie = top + Vector((0, 0, -0.12))
    for k in range(9):
        a = 2 * math.pi * k / 9 + random.uniform(-0.2, 0.2)
        spread = random.uniform(0.02, 0.06)
        end = tie + Vector((math.cos(a) * spread, math.sin(a) * spread, -length * random.uniform(0.7, 1.0)))
        pts = [tie.lerp(end, t / 6) + Vector((math.cos(a), math.sin(a), 0)) * 0.02 * math.sin(math.pi * t / 6) for t in range(7)]
        b.tube(pts, [0.004] * 7, KRAUT, sides=4, cap_end=False)
        for leaf in range(3):
            base = pts[2 + leaf * 2]
            d = Vector((math.cos(a + leaf), math.sin(a + leaf), -0.6)).normalized()
            side = d.cross(Vector((0, 0, 1))).normalized() * 0.012
            rows = [[base - side * 0.2, base + side * 0.2], [base + d * 0.035 - side, base + d * 0.035 + side], [base + d * 0.07, base + d * 0.07 + side * 0.1]]
            b.grid(rows, KRAUT, uv_of=lambda j, i: (i, j / 2), mask_of=lambda j, i: 0.0)
    b.tube([tie + Vector((0, 0, 0.01)), tie + Vector((0, 0, -0.02))], [0.025, 0.025], LEDER, sides=8, cap_end=False)


def vial(b, base, height, radius, hue, shape, seed):
    """A glass vessel with liquid inside and a cork: shape 0 round belly, 1 tall bottle, 2 flat flask, 3 thin tube"""
    if shape == 0:
        prof = [(0.001, 0), (radius * 0.7, 0.003), (radius, height * 0.25), (radius * 0.95, height * 0.5), (radius * 0.35, height * 0.72),
                (radius * 0.28, height * 0.95), (radius * 0.33, height)]
    elif shape == 1:
        prof = [(0.001, 0), (radius, 0.003), (radius, height * 0.62), (radius * 0.45, height * 0.78), (radius * 0.35, height * 0.97), (radius * 0.4, height)]
    elif shape == 2:
        prof = [(0.001, 0), (radius * 0.8, 0.003), (radius, height * 0.2), (radius * 0.7, height * 0.55), (radius * 0.3, height * 0.7), (radius * 0.3, height)]
    else:
        prof = [(0.001, 0), (radius * 0.6, 0.004), (radius, height * 0.08), (radius, height * 0.96), (radius * 1.2, height)]
    lathe(b, prof, GLAS, base, sides=18)
    fill = random.uniform(0.35, 0.62)
    inner = [(r * 0.86, z) for r, z in prof if z <= height * fill]
    if len(inner) >= 2:
        inner.append((inner[-1][0] * 0.98, height * fill))
        rows = lathe(b, inner, FLUID, base + Vector((0, 0, 0.002)), sides=18, mask=hue)
        disc_cap(b, rows[-1], FLUID, mask=hue)
    cork_r = prof[-1][0] * 0.9
    lathe(b, [(cork_r, height - 0.01), (cork_r * 1.05, height + 0.02), (cork_r * 0.9, height + 0.035), (0.001, height + 0.037)], HOLZ, base, sides=10)


# ---------------------------------------------------------------- the root table

def build_table():
    obj = gk.new_object("SM_GK_Wurzeltisch", SLOTS)
    b = gk.Builder()
    # slab: a grown, irregular oval top with a rounded edge, 2.4 x 1.7 m, top at 0.9 m
    rings, sides = 6, 64
    rows = []
    for j in range(rings + 1):
        t = j / rings
        row = []
        for i in range(sides):
            a = 2 * math.pi * i / sides
            r = 1.0 + 0.07 * noise.noise(Vector((math.cos(a) * 1.5, math.sin(a) * 1.5, 3.0))) + 0.03 * math.sin(a * 7)
            edge = [(1.0, -0.09), (1.02, -0.05), (1.03, -0.01), (1.0, 0.0), (0.92, 0.008), (0.5, 0.012), (0.0, 0.014)][j]
            x, y = math.cos(a) * 1.2 * r * edge[0], math.sin(a) * 0.85 * r * edge[0]
            row.append(Vector((x, y, 0.9 + edge[1] + noise.noise(Vector((x * 2, y * 2, 1))) * 0.006)))
        rows.append(row)
    b.grid(rows, HOLZ, uv_of=lambda j, i: (i / sides * 6, j / rings), mask_of=lambda j, i: 0.0, closed_u=True)
    # under side
    under = [[Vector((p.x * f, p.y * f, 0.8)) for p in rows[0]] for f in (1.0, 0.6, 0.01)]
    b.grid([rows[0]] + under, HOLZ, uv_of=lambda j, i: (i / sides * 6, j / 3), mask_of=lambda j, i: 0.0, closed_u=True)
    # legs: gnarled roots from under the top down into the floor, spreading
    for k in range(6):
        a = 2 * math.pi * k / 6 + 0.3
        top = Vector((math.cos(a) * 0.75, math.sin(a) * 0.5, 0.82))
        foot = Vector((math.cos(a) * 1.25, math.sin(a) * 0.95, -0.05))
        pts = []
        for s in range(16):
            t = s / 15
            p = top.lerp(foot, t ** 1.4)
            p.z = top.z + (foot.z - top.z) * t
            pts.append(gk.wobble(p, 1.5, 0.06 * math.sin(math.pi * t), k))
        b.tube(pts, [0.11 * (1 - 0.4 * t) + 0.05 * (s == 15) for s, t in ((s, s / 15) for s in range(16))], HOLZ, sides=10, v_scale=0.8, rough=0.2, seed=k)
        # small side roots at the foot
        for r in range(2):
            d = Vector((math.cos(a + r - 0.5), math.sin(a + r - 0.5), 0))
            b.tube([foot + Vector((0, 0, 0.05)), foot + d * 0.25 + Vector((0, 0, -0.02)), foot + d * 0.45 + Vector((0, 0, -0.05))], [0.05, 0.035, 0.015], HOLZ, sides=7, rough=0.2, seed=k * 3 + r)
    b.finish(obj)
    gk.export(obj)


def build_table_dressing():
    """Everything on the table top (z = 0.9 m in the table's frame)"""
    obj = gk.new_object("SM_GK_Tischschmuck", SLOTS)
    b = gk.Builder()
    top = 0.905

    # the open book at the front (+X, towards the room), pages fanning up, a ribbon
    bc = Vector((0.55, -0.1, top))
    rot = Matrix.Rotation(0.35, 3, 'Z')
    box(b, bc + Vector((0, 0, 0.008)), (0.36, 0.5, 0.016), LEDER, yaw=0.35)
    for side in (-1, 1):
        for layer in range(3):
            rows = []
            for j in range(9):
                v = j / 8
                row = []
                for i in range(7):
                    u = i / 6
                    x = (u - 0.5) * 0.3
                    y = side * (0.012 + v * 0.22)
                    z = 0.018 + layer * 0.004 + 0.05 * math.sin(math.pi * v * 0.95) * (1 - 0.3 * v) - 0.01 * v
                    row.append(bc + rot @ Vector((x, y, z)))
                rows.append(row)
            b.grid(rows, PAPIER, uv_of=lambda j, i, s=side: (i / 6, j / 8 * 1.4), mask_of=lambda j, i: 0.0)
    b.tube([bc + rot @ Vector((0.12, 0.0, 0.03)), bc + rot @ Vector((0.17, 0.02, 0.0)), bc + rot @ Vector((0.21, 0.01, -0.06))], [0.006, 0.006, 0.006], STOFF, sides=4, cap_end=False)

    # mortar with pestle
    mc = Vector((0.15, 0.48, top))
    lathe(b, [(0.07, 0.0), (0.085, 0.01), (0.09, 0.07), (0.1, 0.09), (0.085, 0.095), (0.07, 0.04), (0.001, 0.03)], STEIN, mc, sides=20)
    b.tube([mc + Vector((0.0, 0.0, 0.04)), mc + Vector((0.06, 0.03, 0.18))], [0.022, 0.016], STEIN, sides=10)
    for k in range(6):
        lathe(b, [(0.001, 0), (0.012, 0.004), (0.001, 0.009)], KRAUT, mc + Vector((random.uniform(-0.05, 0.05), random.uniform(-0.05, 0.05), 0.035)), sides=6)

    # kettle on a tripod over embers, at the back (-X)
    kc = Vector((-0.5, 0.05, top))
    for k in range(3):
        a = 2 * math.pi * k / 3
        b.tube([kc + Vector((math.cos(a) * 0.24, math.sin(a) * 0.24, 0.0)), kc + Vector((math.cos(a) * 0.1, math.sin(a) * 0.1, 0.36))], [0.012, 0.01], METALL, sides=6)
    rows = lathe(b, [(0.001, 0.14), (0.12, 0.15), (0.18, 0.22), (0.19, 0.3), (0.16, 0.37), (0.165, 0.39), (0.14, 0.385)], METALL, kc, sides=24)
    brew = lathe(b, [(0.14, 0.36), (0.001, 0.362)], FLUID, kc, sides=24, mask=0.3)
    for k in range(14):
        a = random.uniform(0, 2 * math.pi)
        r = random.uniform(0, 0.15)
        box(b, kc + Vector((math.cos(a) * r, math.sin(a) * r, 0.02)), (0.05, 0.035, 0.03), GLUT, yaw=random.uniform(0, 3), mask=random.uniform(0.6, 1.0), bevel_noise=0.008, seed=k)
    lathe(b, [(0.2, 0.0), (0.21, 0.02), (0.18, 0.03), (0.001, 0.02)], STEIN, kc, sides=16)

    # vials and flasks along the left and right edge, a little rack of tubes
    spots = [(-0.1, -0.55), (0.0, -0.6), (0.1, -0.52), (-0.25, -0.48), (0.25, 0.62), (0.35, 0.55), (-0.15, 0.6), (0.85, 0.15), (0.9, 0.3)]
    for k, (x, y) in enumerate(spots):
        vial(b, Vector((x, y, top)), random.uniform(0.12, 0.26), random.uniform(0.035, 0.06), (k * 0.13 + 0.05) % 1.0, k % 3, k)
    rack = Vector((-0.6, -0.45, top))
    box(b, rack + Vector((0, 0, 0.09)), (0.08, 0.3, 0.015), HOLZ)
    box(b, rack + Vector((0, 0, 0.02)), (0.1, 0.32, 0.02), HOLZ)
    for k in range(5):
        vial(b, rack + Vector((0, -0.12 + k * 0.06, 0.012)), 0.15, 0.012, (0.6 + k * 0.09) % 1.0, 3, k)

    # herb bundles lying on the table, a knife and tongs, a pile of seeds
    for k in range(3):
        start = Vector((0.1 + k * 0.12, 0.25 - k * 0.05, top + 0.015))
        d = Vector((math.cos(1.2 + k * 0.4), math.sin(1.2 + k * 0.4), 0))
        for s in range(8):
            off = d.cross(Vector((0, 0, 1))) * random.uniform(-0.03, 0.03)
            b.tube([start + off, start + off + d * 0.28 + Vector((0, 0, 0.01))], [0.004, 0.003], KRAUT, sides=4, cap_end=False)
        b.tube([start + d * 0.06 + Vector((0, 0, 0.0)), start + d * 0.07], [0.022, 0.022], LEDER, sides=8, cap_end=False)
    box(b, Vector((0.62, 0.36, top + 0.006)), (0.03, 0.16, 0.008), METALL, yaw=-0.4)
    box(b, Vector((0.65, 0.46, top + 0.01)), (0.035, 0.09, 0.02), HOLZ, yaw=-0.4)
    b.tube([Vector((0.45, -0.5, top + 0.01)), Vector((0.62, -0.42, top + 0.02)), Vector((0.45, -0.38, top + 0.01))], [0.006, 0.006, 0.006], METALL, sides=5, cap_end=False)
    for k in range(25):
        lathe(b, [(0.001, 0), (0.006, 0.003), (0.001, 0.006)], KRAUT, Vector((-0.2 + random.gauss(0, 0.04), 0.3 + random.gauss(0, 0.04), top)), sides=5)

    # candles: three on the back corner, one by the book, with flames (glow mask in the wax material)
    for k, (x, y, h) in enumerate(((-0.8, -0.2, 0.16), (-0.78, -0.08, 0.1), (-0.7, -0.15, 0.22), (0.8, -0.35, 0.12))):
        c = Vector((x, y, top))
        lathe(b, [(0.028, 0.0), (0.03, h * 0.8), (0.026, h), (0.001, h + 0.004)], WACHS, c, sides=12, wobble_amount=0.08, seed=k)
        lathe(b, [(0.001, h), (0.009, h + 0.012), (0.006, h + 0.03), (0.001, h + 0.05)], WACHS, c, sides=8, mask=1.0)
        lathe(b, [(0.034, 0.0), (0.04, 0.008), (0.001, 0.009)], WACHS, c, sides=12)
    b.finish(obj)
    gk.export(obj)


def build_herb_arch():
    """A root arch over the back of the table, herbs and a few vials hanging from it on strings"""
    obj = gk.new_object("SM_GK_Kraeuterbogen", SLOTS)
    b = gk.Builder()
    left, right = Vector((-0.95, -1.15, 0.0)), Vector((-0.95, 1.15, 0.0))
    pts = []
    for k in range(25):
        t = k / 24
        p = left.lerp(right, t)
        p.z = 2.55 * math.sin(math.pi * t) ** 0.6
        p.x += 0.15 * math.sin(math.pi * t)
        pts.append(gk.wobble(p, 0.8, 0.07, 3))
    b.tube(pts, [0.13 - 0.06 * math.sin(math.pi * k / 24) for k in range(25)], HOLZ, sides=12, v_scale=0.8, rough=0.22, seed=2)
    second = [gk.wobble(p + Vector((0.05, 0, -0.1)), 1.3, 0.08, 9) for p in pts[2:-2]]
    b.tube(second, [0.05] * len(second), HOLZ, sides=8, rough=0.25, seed=5)
    for k in range(4, 21, 2):
        hang = pts[k]
        if hang.z < 1.6:
            continue
        drop = random.uniform(0.25, 0.6)
        if k % 6 == 0:
            b.tube([hang, hang + Vector((0, 0, -drop))], [0.004, 0.004], LEDER, sides=4, cap_end=False)
            vial(b, hang + Vector((0, 0, -drop - 0.16)), 0.15, 0.045, (k * 0.17) % 1.0, 0, k)
        else:
            herb_bundle(b, hang + Vector((0, 0, -0.05)), drop + 0.15, k)
    b.finish(obj)
    gk.export(obj)


# ---------------------------------------------------------------- the class corners

def build_workbench():
    obj = gk.new_object("SM_GK_Werkbank", SLOTS)
    b = gk.Builder()
    box(b, Vector((0, 0, 0.82)), (0.8, 1.8, 0.1), HOLZ, bevel_noise=0.01)
    for x in (-0.3, 0.3):
        for y in (-0.78, 0.78):
            b.tube([Vector((x, y, 0.78)), Vector((x * 1.15, y * 1.05, 0.0))], [0.07, 0.08], HOLZ, sides=8, rough=0.15, seed=int(x * 10 + y * 10))
    box(b, Vector((0, 0, 0.25)), (0.65, 1.6, 0.04), HOLZ)
    # tool wall at the back with hooks and tools
    box(b, Vector((-0.42, 0, 1.45)), (0.06, 1.7, 1.15), HOLZ, bevel_noise=0.01)
    for k in range(7):
        y = -0.7 + k * 0.23
        b.tube([Vector((-0.38, y, 1.75)), Vector((-0.33, y, 1.78))], [0.01, 0.008], METALL, sides=5)
        length = random.uniform(0.3, 0.5)
        b.tube([Vector((-0.36, y, 1.74)), Vector((-0.36, y, 1.74 - length))], [0.015, 0.012], HOLZ if k % 2 else METALL, sides=6)
        box(b, Vector((-0.36, y, 1.74 - length - 0.04)), (0.03, 0.09 if k % 3 else 0.05, 0.08), METALL)
    # vise
    box(b, Vector((0.25, 0.65, 0.95)), (0.18, 0.2, 0.16), METALL)
    box(b, Vector((0.25, 0.54, 0.98)), (0.16, 0.04, 0.12), METALL)
    b.tube([Vector((0.25, 0.4, 0.98)), Vector((0.25, 0.75, 0.98))], [0.012, 0.012], METALL, sides=6)
    # bone gears lying around
    for k, (x, y, r) in enumerate(((0.1, -0.3, 0.12), (0.2, -0.55, 0.08), (-0.1, -0.1, 0.06))):
        teeth = 12
        prof_rows = []
        for ring, (rr, z) in enumerate(((r * 0.3, 0.88), (r, 0.88), (r, 0.9), (r * 0.3, 0.9))):
            row = []
            for s in range(teeth * 4):
                a = 2 * math.pi * s / (teeth * 4)
                tooth = 1.0 + (0.18 if (s // 2) % 2 == 0 and ring in (1, 2) else 0.0)
                row.append(Vector((x + math.cos(a) * rr * tooth, y + math.sin(a) * rr * tooth, z)))
            prof_rows.append(row)
        b.grid(prof_rows, STEIN, uv_of=lambda j, i: (i / 48, j / 3), mask_of=lambda j, i: 0.0, closed_u=True)
    # slag bars and a half-built sling
    for k in range(4):
        box(b, Vector((0.1 + k * 0.05, 0.2, 0.89 + k * 0.012)), (0.08, 0.22, 0.03), METALL, yaw=0.1 * k, bevel_noise=0.006, seed=k)
    sling = [Vector((0.15, -0.8, 0.88)), Vector((0.2, -0.75, 1.0)), Vector((0.25, -0.65, 1.08))]
    b.tube(sling, [0.02, 0.018, 0.015], HOLZ, sides=8)
    b.tube([Vector((0.2, -0.75, 1.0)), Vector((0.12, -0.6, 1.05)), Vector((0.1, -0.5, 1.07))], [0.018, 0.015, 0.012], HOLZ, sides=8)
    b.tube([Vector((0.25, -0.65, 1.08)), Vector((0.2, -0.55, 0.95)), Vector((0.1, -0.5, 1.07))], [0.004] * 3, LEDER, sides=4, cap_end=False)
    # a leather apron hanging over the corner
    rows = [[Vector((0.4, 0.9 + u * 0.3, 0.86 - v * 0.6 + 0.04 * math.sin(u * 6))) for u in (0, 0.33, 0.66, 1.0)] for v in (0, 0.33, 0.66, 1.0)]
    b.grid(rows, LEDER, uv_of=lambda j, i: (i / 3, j / 3), mask_of=lambda j, i: 0.0)
    b.finish(obj)
    gk.export(obj)


def build_shrine():
    obj = gk.new_object("SM_GK_Schrein", SLOTS)
    b = gk.Builder()
    # three stacked, uneven stones
    z = 0.0
    for k, (w, h) in enumerate(((1.3, 0.45), (0.95, 0.4), (0.6, 0.35))):
        box(b, Vector((0, 0, z + h / 2)), (w * 0.8, w, h), STEIN, yaw=random.uniform(-0.15, 0.15), bevel_noise=0.04, seed=k)
        z += h
    # a standing stone slab behind with a carved sun / moon disc (glow mask)
    box(b, Vector((-0.38, 0, 1.25)), (0.18, 0.7, 1.4), STEIN, bevel_noise=0.04, seed=8)
    lathe(b, [(0.001, 0.0), (0.2, 0.0), (0.2, 0.02), (0.001, 0.025)], STEIN, Vector((-0.28, 0, 1.55)), sides=24, mask=0.8,
          tilt=Matrix.Rotation(math.pi / 2, 3, 'Y'))
    # incense bowl with embers and candles on the steps
    bowl = Vector((0.0, 0.0, z))
    lathe(b, [(0.001, 0.0), (0.15, 0.01), (0.2, 0.08), (0.18, 0.09), (0.001, 0.05)], METALL, bowl, sides=20)
    for k in range(10):
        a = random.uniform(0, 6.3)
        r = random.uniform(0, 0.12)
        box(b, bowl + Vector((math.cos(a) * r, math.sin(a) * r, 0.06)), (0.04, 0.03, 0.025), GLUT, yaw=a, mask=random.uniform(0.5, 1.0), bevel_noise=0.006, seed=k)
    for k, (x, y, zz, h) in enumerate(((0.35, -0.4, 0.45, 0.2), (0.35, 0.42, 0.45, 0.15), (0.25, -0.25, 0.85, 0.12), (0.25, 0.3, 0.85, 0.18), (0.55, 0.0, 0.0, 0.1))):
        c = Vector((x, y, zz))
        lathe(b, [(0.035, 0.0), (0.036, h * 0.8), (0.03, h), (0.001, h + 0.004)], WACHS, c, sides=12, wobble_amount=0.1, seed=k)
        lathe(b, [(0.001, h), (0.01, h + 0.014), (0.007, h + 0.034), (0.001, h + 0.055)], WACHS, c, sides=8, mask=1.0)
        lathe(b, [(0.06, 0.0), (0.07, 0.01), (0.001, 0.012)], WACHS, c, sides=12)
    # a prayer cloth hanging over the top stone, charms on strings from a crooked branch
    rows = [[Vector((0.15 + v * 0.05, -0.35 + u * 0.7, 1.2 - v * 0.55 + 0.03 * math.sin(u * 9 + v * 3))) for u in (0, 0.25, 0.5, 0.75, 1.0)] for v in (0, 0.33, 0.66, 1.0)]
    b.grid(rows, STOFF, uv_of=lambda j, i: (i / 4, j / 3), mask_of=lambda j, i: 0.0)
    branch = [Vector((-0.4, -0.6, 2.0)), Vector((-0.2, -0.2, 2.15)), Vector((-0.1, 0.25, 2.1)), Vector((0.0, 0.6, 2.2))]
    b.tube(branch, [0.04, 0.035, 0.03, 0.02], HOLZ, sides=8, rough=0.2)
    for k in range(5):
        p = branch[0].lerp(branch[-1], 0.15 + k * 0.17) + Vector((0, 0, 0.05))
        drop = random.uniform(0.25, 0.5)
        b.tube([p, p + Vector((0, 0, -drop))], [0.003, 0.003], STOFF, sides=4, cap_end=False)
        lathe(b, [(0.001, 0.0), (0.03, 0.02), (0.035, 0.05), (0.001, 0.08)], STEIN if k % 2 else HOLZ, p + Vector((0, 0, -drop - 0.08)), sides=8, mask=0.5 if k % 2 else 0.0)
    b.finish(obj)
    gk.export(obj)


def build_den():
    obj = gk.new_object("SM_GK_Bau", SLOTS)
    b = gk.Builder()
    # mound of roots: arched roots over a hollow, the entrance facing +X
    for k in range(14):
        a = 2 * math.pi * k / 14
        if abs(math.atan2(math.sin(a), math.cos(a))) < 0.5:
            continue
        foot = Vector((math.cos(a) * 1.6, math.sin(a) * 1.5, -0.05))
        other = Vector((math.cos(a + math.pi * 0.7) * 1.4, math.sin(a + math.pi * 0.7) * 1.3, -0.05))
        pts = []
        for s in range(18):
            t = s / 17
            p = foot.lerp(other, t)
            p.z = 1.35 * math.sin(math.pi * t) ** 0.7
            pts.append(gk.wobble(p, 0.9, 0.12, k))
        b.tube(pts, [0.16 * (1 - 0.5 * math.sin(math.pi * s / 17)) + 0.04 for s in range(18)], HOLZ, sides=9, rough=0.25, seed=k)
    # moss skin over the back of the mound
    rows = []
    for j in range(13):
        v = j / 12
        row = []
        for i in range(25):
            u = i / 24
            a = math.pi * 0.35 + u * math.pi * 1.3
            r = 1.55 * math.cos(v * math.pi / 2) + 0.05
            p = Vector((math.cos(a) * r, math.sin(a) * r * 0.95, 1.3 * math.sin(v * math.pi / 2)))
            row.append(p + Vector((math.cos(a), math.sin(a), 0.3)) * 0.08 * noise.noise(p * 2.5))
        rows.append(row)
    b.grid(rows, MOOS, uv_of=lambda j, i: (i / 24 * 4, j / 12 * 2), mask_of=lambda j, i: 0.0)
    # straw nest inside and a stone trough by the entrance
    for k in range(60):
        a = random.uniform(0, 2 * math.pi)
        r = random.uniform(0.2, 0.55)
        p = Vector((math.cos(a) * r - 0.1, math.sin(a) * r, 0.05 + 0.08 * (r - 0.2)))
        d = Vector((-math.sin(a), math.cos(a), random.uniform(-0.2, 0.2))).normalized()
        b.tube([p - d * 0.15, p + d * 0.15], [0.006, 0.005], STROH, sides=4, cap_end=False)
    lathe(b, [(0.001, 0.0), (0.25, 0.0), (0.27, 0.15), (0.22, 0.16), (0.2, 0.06), (0.001, 0.06)], STEIN, Vector((1.6, -0.6, 0.0)), sides=8, wobble_amount=0.1)
    b.finish(obj)
    gk.export(obj)


def build_glow_well():
    obj = gk.new_object("SM_GK_Glutbrunnen", SLOTS)
    b = gk.Builder()
    # basin ring of stone with ember in it
    lathe(b, [(0.95, 0.0), (1.0, 0.25), (0.9, 0.42), (0.75, 0.42), (0.7, 0.2), (0.001, 0.15)], STEIN, Vector((0, 0, 0)), sides=40, wobble_amount=0.05)
    rows = lathe(b, [(0.72, 0.3), (0.4, 0.32), (0.001, 0.33)], GLUT, Vector((0, 0, 0)), sides=40, mask=1.0)
    # rune stones standing round it, runes as glowing grooves (mask)
    for k in range(7):
        a = 2 * math.pi * k / 7
        c = Vector((math.cos(a) * 1.45, math.sin(a) * 1.45, 0.0))
        h = random.uniform(0.8, 1.4)
        box(b, c + Vector((0, 0, h / 2)), (0.22, 0.34, h), STEIN, yaw=a, bevel_noise=0.03, seed=k)
        box(b, c + Vector((-math.cos(a) * 0.115, -math.sin(a) * 0.115, h * 0.6)), (0.01, 0.08, 0.22), GLUT, yaw=a, mask=0.9)
    # crystals floating over the basin
    for k in range(5):
        a = 2 * math.pi * k / 5
        c = Vector((math.cos(a) * 0.35, math.sin(a) * 0.35, 1.0 + 0.25 * math.sin(k * 1.7)))
        lathe(b, [(0.001, -0.12), (0.06, 0.0), (0.001, 0.18)], GLUT, c, sides=5, mask=0.8, tilt=Matrix.Rotation(0.3 * k, 3, 'X'))
    b.finish(obj)
    gk.export(obj)


def build_helper_nest():
    obj = gk.new_object("SM_GK_Helfernest", SLOTS)
    b = gk.Builder()
    lathe(b, [(0.45, 0.0), (0.5, 0.3), (0.42, 0.75), (0.001, 0.8)], HOLZ, Vector((0, 0, 0)), sides=16, wobble_amount=0.15)
    # woven basket: crossing root strands round an egg shape, glow inside
    for k in range(16):
        pts = []
        for s in range(20):
            t = s / 19
            a = 2 * math.pi * (k / 16) + t * math.pi * (1 if k % 2 else -1)
            r = 0.5 * math.sin(math.pi * t) + 0.02
            pts.append(Vector((math.cos(a) * r, math.sin(a) * r, 0.8 + 0.9 * t)))
        b.tube(pts, [0.018] * 20, STROH if k % 3 else HOLZ, sides=5, cap_end=False)
    rows = lathe(b, [(0.001, 1.0), (0.25, 1.15), (0.3, 1.3), (0.2, 1.45), (0.001, 1.5)], GLUT, Vector((0, 0, 0)), sides=14, mask=0.7)
    b.finish(obj)
    gk.export(obj)


if __name__ == "__main__":
    for build in (build_table, build_table_dressing, build_herb_arch, build_workbench, build_shrine, build_den, build_glow_well, build_helper_nest):
        gk.clear_scene()
        build()
    gk.log("Stationen fertig")
