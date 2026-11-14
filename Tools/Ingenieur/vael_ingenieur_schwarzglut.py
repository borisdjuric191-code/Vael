"""
The Schwarzglut-Rüstung of the engineer (2026-11-14, Docs/Ingenieur_Aschenmark_Materialien.md 6b), from the real Tripo models.

Look (Boris' reference, 2026-11-14): heavy, smooth black plates laid in layers like shingles, a glowing line along the lower edge
of each plate, big layered pauldrons, a horned helmet with a visor slit, plates over the thighs, spikes. The Glutkriecher's back
plates already overlap exactly like that: each plate is cut out WHOLE between the steps of its shell, given a clean rounded
outline (clipped in the flat, so the creature's UVs and texture stay) and laid on the body almost unbent, with a rim of
Schlackeneisen and the glowing vein. The Aschenroller's belt bands make the joints (belly, forearm, knee).

The shield is a TOWER SHIELD: nearly the whole banded back of an Aschenroller, cut into its five rows of bands. Folded, the rows
slide into each other on the forearm like the creature rolling up; raised, they slide apart into a curved shield as tall as a man.

  SM_Ing_R_Schwarzglut_Kopf / _Brust / _Arm / _Bein    as the other sets (vael_ingenieur_werkstuecke.py, VAEL_ING_RUESTUNG)
  SM_Ing_R_Schwarzglut_Oberschenkel                    the thigh plates (the C++ hangs them on thigh_l / thigh_r)
  SM_Ing_R_Schwarzglut_Schild_Nabe                     the mount on the forearm: brackets and straps, face +X
  SM_Ing_R_Schwarzglut_Schild_Teil_0 .. _4             the five rows of the tower shield, bottom to top, each 20 cm high and
                                                       built round its own middle: face +X, width along Y, up along Z

Slots: the seven of the kit (Holz, Leder, Metall, Faden, Glut, Moos, Messing), then Glutkriecher and Aschenroller (their MI).

Run:
  blender -b --factory-startup -P vael_ingenieur_schwarzglut.py -- <output folder>
"""
import bpy, bmesh, sys, os, math
from mathutils import Vector, Matrix

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Refugium"))
import glutkammer_probe as gk  # noqa: E402
from vael_ingenieur_werkstuecke import lathe, disc, sleeve, LEDER, METALL, GLUT, SLOTS as KIT_SLOTS  # noqa: E402

SLOTS = KIT_SLOTS + ["Glutkriecher", "Aschenroller"]
KRIECHER, ROLLER = 7, 8
QUELLEN = "C:/Projekte/Vael_Quellen"

# The five rows of the shield: equal windows along the Aschenroller's back, 0.124 m each, scaled to 20 cm
SHIELD_ROWS = 5
SHIELD_FROM, SHIELD_ROW = -0.30, 0.124
SHIELD_SCALE = 0.2 / SHIELD_ROW


# ---------------------------------------------------------------- cutting the creatures

def load_creature(name):
    folder = os.path.join(QUELLEN, name, "02_tripo")
    fbx = [f for f in os.listdir(folder) if f.lower().endswith(".fbx")][0]
    gk.clear_scene()
    bpy.ops.import_scene.fbx(filepath=os.path.join(folder, fbx))
    obj = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.transform(obj.matrix_world)
    bm.normal_update()
    uv = bm.loops.layers.uv.active
    data = [([(l.vert.co.copy(), l[uv].uv.copy()) for l in f.loops], f.normal.copy(), f.calc_center_median()) for f in bm.faces]
    bm.free()
    gk.clear_scene()
    return data


class Patch:
    """The outer shell between two cuts, flat: s across (arc length, m), t along (m, 0 in the middle), h relief (m), with UVs"""

    def __init__(self, bm, uv):
        self.bm, self.uv = bm, uv

    def copy(self):
        bm = self.bm.copy()
        return Patch(bm, bm.loops.layers.uv.active)


def cut(faces, axis, zc, window, theta_max, z_min, r_min=0.08, relief_max=0.025):
    lo, hi = window
    mid = (lo + hi) / 2

    def polar(p):
        along, side = (p.y, p.x) if axis == "Y" else (p.x, p.y)
        return math.atan2(side, p.z - zc), along, math.hypot(side, p.z - zc)

    picked = []
    for corners, normal, centre in faces:
        theta, along, r = polar(centre)
        if not (lo - 0.03 <= along <= hi + 0.03) or abs(theta) > theta_max or centre.z < z_min or r < r_min:
            continue
        radial = Vector((centre.x, 0.0, centre.z - zc)) if axis == "Y" else Vector((0.0, centre.y, centre.z - zc))
        if normal.dot(radial.normalized()) < -0.3:
            continue
        picked.append([polar(p) + (uv,) for p, uv in corners])
    radii = sorted(c[2] for f in picked for c in f)
    base = radii[len(radii) // 2]
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    for f in picked:
        verts = [bm.verts.new(Vector((t * base, a - mid, max(-relief_max, min(relief_max, r - base))))) for t, a, r, _ in f]
        try:
            face = bm.faces.new(verts)
        except ValueError:
            continue
        for loop, c in zip(face.loops, f):
            loop[uvl].uv = c[3]
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    gk.log("  {} Flaechen zwischen {:.3f} und {:.3f} (Radius {:.3f})".format(len(bm.faces), lo, hi, base))
    return Patch(bm, uvl)


def clip(patch, outline):
    """A copy of the patch inside a convex outline [(s, t)] (counter-clockwise): cut cleanly, the UVs carried along the cuts"""
    p = patch.copy()
    n = len(outline)
    for k in range(n):
        a, b = Vector(outline[k]), Vector(outline[(k + 1) % n])
        edge = b - a
        out = Vector((edge.y, -edge.x)).normalized()
        geom = list(p.bm.verts) + list(p.bm.edges) + list(p.bm.faces)
        bmesh.ops.bisect_plane(p.bm, geom=geom, plane_co=(a.x, a.y, 0.0), plane_no=(out.x, out.y, 0.0), clear_outer=True)
    return p


def ellipse(rs, rt, cs=0.0, ct=0.0, n=24, flat_top=None):
    pts = []
    for k in range(n):
        a = 2 * math.pi * k / n
        s, t = cs + math.cos(a) * rs, ct + math.sin(a) * rt
        if flat_top is not None:
            t = min(t, flat_top)
        pts.append((s, t))
    return pts


def shingle(rs, rt):
    """The outline of a plate in the layers: straight at the top (under the plate above), round at the sides and below"""
    pts = [(-rs, rt), ]
    for k in range(13):
        a = math.pi + math.pi * k / 12
        pts.append((math.cos(a) * rs, math.sin(a) * rt * 1.4 + rt * 0.4))
    pts.append((rs, rt))
    # counter-clockwise: bottom arc from left to right, then the straight top back
    return pts


def place(b, patch, material, mapping, outward_of, thickness=0.006, rim=0.005, vein=True, rim_top=True, rim_skip=None):
    """Lays a clipped patch on the body. mapping(s, t, h) -> point. A thin back, a rim of Schlackeneisen round the outline and
    a glowing vein along its lower edge"""
    bm, uvl = patch.bm, patch.uv
    flips = 0
    for face in bm.faces:
        pts = [mapping(v.co.x, v.co.y, v.co.z) for v in face.verts]
        if len(pts) >= 3:
            normal = (pts[1] - pts[0]).cross(pts[2] - pts[0])
            flips += 1 if normal.dot(outward_of(sum(pts, Vector()) / len(pts))) < 0 else -1
    for face in bm.faces:
        loops = list(face.loops)
        if flips > 0:
            loops = loops[::-1]
        front = [mapping(l.vert.co.x, l.vert.co.y, l.vert.co.z) for l in loops]
        uvs = [l[uvl].uv.copy() for l in loops]
        for side in (0, 1):
            pts, us = (front, uvs) if side == 0 else ([p - outward_of(p) * thickness for p in reversed(front)], list(reversed(uvs)))
            try:
                nf = b.bm.faces.new([b.bm.verts.new(p) for p in pts])
            except ValueError:
                continue
            nf.material_index = material
            nf.tag = True
            for loop, uv in zip(nf.loops, us):
                loop[b.uv].uv = uv
                loop[b.col] = (0.0, 0.0, 0.0, 1.0)
                loop[b.mask].uv = (0.0, 0.0)
    # the outline: boundary edges chained into loops
    boundary = [e for e in bm.edges if e.is_boundary]
    if not boundary:
        return
    nxt = {}
    for e in boundary:
        a, c = e.verts
        nxt.setdefault(a, []).append(c)
        nxt.setdefault(c, []).append(a)
    seen = set()
    lowest = min(v.co.y for v in bm.verts)
    highest = max(v.co.y for v in bm.verts)
    chains = []
    for start in list(nxt):
        if start in seen:
            continue
        chain, prev, cur = [start], None, start
        seen.add(start)
        while True:
            options = [v for v in nxt[cur] if v is not prev and v not in seen]
            if not options:
                break
            prev, cur = cur, options[0]
            seen.add(cur)
            chain.append(cur)
        chains.append(chain)
    # only the outline of the plate: the loop that reaches widest; holes inside the creature's shell get no rim
    chain = max(chains, key=lambda c: max(v.co.x for v in c) - min(v.co.x for v in c))
    chain.append(chain[0])
    # an even line along it, every 1.2 cm of the creature's shell, so the rim is smooth and light
    points, last = [], None
    for v in chain:
        p = Vector((v.co.x, v.co.y))
        if last is None or (p - last).length >= 0.012:
            points.append(p)
            last = p
    skip = rim_skip or ((lambda s, t: t > highest - 0.004) if not rim_top else (lambda s, t: False))

    def lifted(s, t, lift):
        p = mapping(s, t, 0.0)
        return p + outward_of(p) * lift

    if rim:
        run = []
        for p in points + [points[0]]:
            if skip(p.x, p.y):
                if len(run) > 2:
                    b.tube([lifted(q.x, q.y, 0.003) for q in run], [rim] * len(run), METALL, sides=5, cap_end=False)
                run = []
                continue
            run.append(p)
        if len(run) > 2:
            b.tube([lifted(q.x, q.y, 0.003) for q in run], [rim] * len(run), METALL, sides=5, cap_end=False)
    if vein:
        # the glowing line, a little inside the lower part of the outline
        centre_s = sum(p.x for p in points) / len(points)
        centre_t = sum(p.y for p in points) / len(points)
        low = sorted([p for p in points if p.y < centre_t - (centre_t - lowest) * 0.3 and not skip(p.x, p.y)], key=lambda p: p.x)
        if len(low) > 3:
            pts = [lifted(p.x + (centre_s - p.x) * 0.1, p.y + (centre_t - p.y) * 0.22, 0.007) for p in low]
            b.tube(pts, [0.0024] * len(pts), GLUT, sides=4, cap_end=False, mask_of=lambda x: 1.0)


def finish(b, obj):
    """As the kit's finish, but the plates keep the outward turn place() gave them"""
    bmesh.ops.recalc_face_normals(b.bm, faces=[f for f in b.bm.faces if not f.tag])
    b.bm.to_mesh(obj.data)
    b.bm.free()
    for poly in obj.data.polygons:
        poly.use_smooth = True
    # lighter for the game: at most 24 000 triangles a piece, the shape and the UVs kept
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    if tris > 24000:
        mod = obj.modifiers.new("leichter", "DECIMATE")
        mod.ratio = 24000 / tris
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=mod.name)


# ---------------------------------------------------------------- the shapes on the body

def on_cylinder(rx, ry, angle_mid, z_mid, scale, relief=0.25, flip_t=False):
    """Round the Z axis (chest, forearm, shin, thigh): s round, t up"""
    r_mean = (rx + ry) / 2

    def mapping(s, t, h):
        a = angle_mid + s * scale / r_mean
        k = 1.0 + h * relief / r_mean
        return Vector((math.cos(a) * rx * k, math.sin(a) * ry * k, z_mid + (-t if flip_t else t) * scale))
    return mapping


def cylinder_out(p):
    v = Vector((p.x, p.y, 0.0))
    return v.normalized() if v.length > 1e-6 else Vector((1, 0, 0))


def on_sphere(centre, radius, ahead, up, scale, relief=0.25):
    """A cap round a point (helmet, pauldron, knee): s to the side, t up, ahead the middle of the plate"""
    f = ahead.normalized()
    side = up.cross(f).normalized()
    u = f.cross(side).normalized()

    def mapping(s, t, h):
        a, c = s * scale / radius, t * scale / radius
        d = f * math.cos(a) * math.cos(c) + side * math.sin(a) * math.cos(c) + u * math.sin(c)
        return centre + d * (radius + h * relief)
    return mapping


def sphere_out(centre):
    return lambda p: (p - centre).normalized()


def spike(b, base, direction, length, radius=0.012):
    d = direction.normalized()
    rot = Vector((0, 0, 1)).rotation_difference(d).to_matrix()
    lathe(b, [(radius, 0.0), (radius * 0.6, length * 0.5), (0.0005, length)], METALL, base, sides=7, axis=rot)


def horn(b, side):
    pts = [Vector((0.03, side * 0.1, 0.07)), Vector((-0.01, side * 0.15, 0.15)), Vector((-0.08, side * 0.17, 0.22)),
           Vector((-0.17, side * 0.15, 0.27)), Vector((-0.26, side * 0.11, 0.29))]

    b.tube(pts, [0.024, 0.02, 0.014, 0.008, 0.002], METALL, sides=8)


def build_helmet(plates, bands):
    obj = gk.new_object("SM_Ing_R_Schwarzglut_Kopf", SLOTS)
    b = gk.Builder()
    c = Vector((-0.01, 0.0, 0.025))
    out = sphere_out(c)
    up = Vector((0, 0, 1))
    # three layers from the brow back over the crown to the neck, each overlapping the one before it
    for ahead, k, size in ((Vector((1, 0, 0.9)), 0, (0.36, 0.1)), (Vector((0.2, 0, 1)), 1, (0.38, 0.1)), (Vector((-1, 0, 0.6)), 0, (0.34, 0.1))):
        fwd = ahead.normalized()
        plate = clip(plates[k], shingle(*size))
        place(b, plate, KRIECHER, on_sphere(c, 0.142 + 0.006 * k, fwd, up if abs(fwd.z) < 0.9 else Vector((1, 0, 0)), 0.42), out,
              rim=0.004, vein=True)
    # the face guard below the visor slit, the slit glowing like the eyes of the Glutkriecher
    guard = clip(plates[2], ellipse(0.2, 0.06))
    place(b, guard, KRIECHER, on_sphere(c, 0.14, Vector((1, 0, -0.55)), up, 0.4), out, rim=0.004, vein=False)
    for side in (-1, 1):
        disc(b, c + Vector((0.128, side * 0.038, 0.0)), 0.011, Vector((1, side * 0.3, 0)), GLUT, glow=1.0, sides=8)
        horn(b, side)
    for k in range(4):
        spike(b, c + Vector((-0.02 - k * 0.05, 0, 0.15 - k * 0.012)), Vector((-0.4, 0, 1)), 0.05 - k * 0.008, 0.01)
    # Ordenstuch under the rim
    lathe(b, [(0.125, -0.06), (0.128, -0.02)], LEDER, c, sides=20)
    finish(b, obj)
    gk.export(obj)


def build_chest(plates, bands):
    obj = gk.new_object("SM_Ing_R_Schwarzglut_Brust", SLOTS)
    b = gk.Builder()
    ax, ay = 0.15, 0.185
    # three layers over the chest and three over the back, from the top down, each over the one below
    for k, z in enumerate((0.19, 0.105, 0.02)):
        for angle_mid in (0.0, math.pi):
            plate = clip(plates[k % 2], shingle(0.36, 0.075))
            place(b, plate, KRIECHER, on_cylinder(ax * (1.04 - 0.02 * k), ay * (1.04 - 0.02 * k), angle_mid, z, 0.55), cylinder_out, rim_top=False)
    # the Rollpanzer belly: two rolling bands under the plates, then the belt
    for k, z in enumerate((-0.06, -0.12)):
        band = clip(bands[k], [(-0.44, -0.026), (0.44, -0.026), (0.44, 0.026), (-0.44, 0.026)])
        place(b, band, ROLLER, on_cylinder(ax * 0.98, ay * 0.98, 0.0, z, 0.65), cylinder_out, rim=0.0, vein=False)
        place(b, band, ROLLER, on_cylinder(ax * 0.98, ay * 0.98, math.pi, z, 0.65), cylinder_out, rim=0.0, vein=False)
    lathe(b, [(ax * 1.02, -0.19), (ax * 1.05, -0.17), (ax * 1.02, -0.15)], LEDER, Vector((0, 0, 0)), sides=24, axis=Matrix.Diagonal((1, ay / ax, 1)))
    disc(b, Vector((ax * 1.07, 0, -0.17)), 0.022, Vector((1, 0, 0)), METALL, sides=4)
    # the Glutstein in the chest, held in a Schlackeneisen ring
    heart = Vector((ax * 1.13, 0.0, 0.11))
    disc(b, heart + Vector((0.006, 0, 0)), 0.024, Vector((1, 0, 0)), GLUT, glow=1.0, sides=12)
    lathe(b, [(0.024, 0.0), (0.032, 0.006), (0.028, 0.012)], METALL, heart, sides=16, axis=Matrix.Rotation(math.radians(90), 3, "Y"))
    # the pauldrons: three layers each over the shoulder, spikes along the top
    for side in (-1, 1):
        cap = Vector((0.0, side * 0.21, 0.16))
        for k, (tilt, size) in enumerate(((0.35, (0.3, 0.09)), (-0.1, (0.32, 0.09)), (-0.55, (0.3, 0.085)))):
            ahead = Vector((0.0, side * 1.0, tilt + 0.9))
            plate = clip(plates[k], shingle(*size))
            place(b, plate, KRIECHER, on_sphere(cap, 0.13 + 0.014 * k, ahead, Vector((0, 0, 1)) if k else Vector((-1, 0, 0)), 0.62),
                  sphere_out(cap), rim_top=False)
        for k in range(3):
            spike(b, cap + Vector((-0.06 + k * 0.06, side * 0.08, 0.13)), Vector((0, side * 0.6, 1)), 0.09, 0.016)
    finish(b, obj)
    gk.export(obj)


def build_arm(plates, bands):
    obj = gk.new_object("SM_Ing_R_Schwarzglut_Arm", SLOTS)
    b = gk.Builder()
    r = 0.05
    sleeve(b, 0.2, r * 0.92, LEDER, rings=1, flare=1.05)
    # the gauntlet: rolling bands round the wrist half, two plates over the outside, a row of spikes
    for k, z in enumerate((-0.075, -0.035)):
        band = clip(bands[k], [(-0.44, -0.026), (0.44, -0.026), (0.44, 0.026), (-0.44, 0.026)])
        place(b, band, ROLLER, on_cylinder(r * 1.05, r * 1.05, 0.0, z, 0.38, 0.1), cylinder_out, rim=0.0, vein=False)
    for k, z in enumerate((0.07, 0.02)):
        plate = clip(plates[k], shingle(0.3, 0.075))
        place(b, plate, KRIECHER, on_cylinder(r * 1.2, r * 1.2, 0.0, z, 0.33, 0.12), cylinder_out, rim=0.004, rim_top=False)
    for k in range(3):
        spike(b, Vector((r * 1.32, 0, -0.05 + k * 0.05)), Vector((1, 0, -0.5)), 0.05, 0.01)
    finish(b, obj)
    gk.export(obj)


def build_leg(plates, bands):
    obj = gk.new_object("SM_Ing_R_Schwarzglut_Bein", SLOTS)
    b = gk.Builder()
    r = 0.062
    sleeve(b, 0.26, r * 0.92, LEDER, rings=1, flare=1.05)
    # the greave: three layers down the shin, a knee cop above, a band at the ankle, spikes at the calf
    for k, z in enumerate((0.07, 0.01, -0.05)):
        plate = clip(plates[k % 2], shingle(0.3, 0.075))
        place(b, plate, KRIECHER, on_cylinder(r * (1.22 - 0.02 * k), r * (1.22 - 0.02 * k), 0.0, z, 0.4, 0.12), cylinder_out, rim_top=False)
    knee = Vector((0.0, 0.0, 0.15))
    plate = clip(plates[2], ellipse(0.17, 0.1))
    place(b, plate, KRIECHER, on_sphere(knee, 0.07, Vector((1, 0, 0.2)), Vector((0, 0, 1)), 0.4), sphere_out(knee), rim=0.004)
    band = clip(bands[1], [(-0.44, -0.026), (0.44, -0.026), (0.44, 0.026), (-0.44, 0.026)])
    place(b, band, ROLLER, on_cylinder(r * 1.04, r * 1.04, 0.0, -0.11, 0.46, 0.1), cylinder_out, rim=0.0, vein=False)
    for k in range(2):
        spike(b, Vector((-r * 1.1, 0, -0.06 + k * 0.08)), Vector((-1, 0, -0.6)), 0.06, 0.011)
    finish(b, obj)
    gk.export(obj)


def build_thigh(plates, bands):
    obj = gk.new_object("SM_Ing_R_Schwarzglut_Oberschenkel", SLOTS)
    b = gk.Builder()
    r = 0.085
    # three big layers over the front and outside of the thigh, as in the picture
    for k, z in enumerate((0.1, 0.02, -0.06)):
        plate = clip(plates[k % 2], shingle(0.36, 0.075))
        place(b, plate, KRIECHER, on_cylinder(r * (1.16 - 0.02 * k), r * (1.16 - 0.02 * k), 0.0, z, 0.55, 0.14), cylinder_out, rim_top=False)
    for z in (0.14, -0.12):
        b.tube([Vector((math.cos(a) * r, math.sin(a) * r, z)) for a in [math.radians(d) for d in range(0, 361, 30)]], [0.006] * 13, LEDER, sides=4, cap_end=False)
    finish(b, obj)
    gk.export(obj)


def shield_outline():
    """The tower shield in the Aschenroller's back (s across, t along from its middle): straight sides, the top round, the foot
    narrowing to a blunt point. Convex, counter-clockwise."""
    half = SHIELD_ROWS * SHIELD_ROW / 2
    w = 0.17
    pts = [(-w * 0.35, -half), (w * 0.35, -half), (w, -half + 0.12)]
    for k in range(9):
        a = math.radians(k * 180 / 8)
        pts.append((math.cos(a) * w, half - 0.07 + math.sin(a) * 0.07))
    pts.append((-w, -half + 0.12))
    return pts


def build_shield(bands_all):
    """The five rows: the whole back cut once, then each row clipped from it and built round its own middle"""
    outline = shield_outline()
    whole = clip(bands_all, outline)
    half = SHIELD_ROWS * SHIELD_ROW / 2
    for row in range(SHIELD_ROWS):
        t0 = -half + row * SHIELD_ROW
        t1 = t0 + SHIELD_ROW
        piece = clip(whole, [(-1, t0 - 0.004), (1, t0 - 0.004), (1, t1 + 0.004), (-1, t1 + 0.004)])
        mid = (t0 + t1) / 2
        obj = gk.new_object("SM_Ing_R_Schwarzglut_Schild_Teil_{}".format(row), SLOTS)
        b = gk.Builder()
        k = SHIELD_SCALE

        def mapping(s, t, h, mid=mid):
            # gently curved round a tall axis behind it, the rows overlapping a little outward from the bottom up
            y = s * k
            return Vector((h * 0.5 - (y * y) * 0.55 + 0.004 * (t - mid) / SHIELD_ROW, y, (t - mid) * k))
        inner = lambda s, t, t0=t0, t1=t1, row=row: (row > 0 and t < t0 + 0.006) or (row < SHIELD_ROWS - 1 and t > t1 - 0.006)
        place(b, piece, ROLLER, mapping, lambda p: Vector((1, 0, 0)), thickness=0.01, rim=0.008, vein=row == 0, rim_skip=inner)
        finish(b, obj)
        gk.export(obj)


def build_shield_mount():
    """What holds the folded rows on the forearm and the shield in the hand: two brackets of Schlackeneisen, straps, a grip"""
    obj = gk.new_object("SM_Ing_R_Schwarzglut_Schild_Nabe", SLOTS)
    b = gk.Builder()
    for z in (-0.06, 0.06):
        b.tube([Vector((-0.01, -0.07, z)), Vector((-0.03, 0.0, z)), Vector((-0.01, 0.07, z))], [0.008] * 3, METALL, sides=5)
        b.tube([Vector((-0.03, -0.05, z)), Vector((-0.07, 0.0, z)), Vector((-0.03, 0.05, z))], [0.009] * 3, LEDER, sides=5)
    b.tube([Vector((-0.03, 0, -0.07)), Vector((-0.03, 0, 0.07))], [0.012, 0.012], METALL, sides=6)
    finish(b, obj)
    gk.export(obj)


if __name__ == "__main__":
    kriecher = load_creature("Glutkriecher")
    # the back plates between the steps of the shell (height profile along the back): a long middle plate and two behind it
    plates = [cut(kriecher, "Y", 0.18, window, math.radians(70), 0.2) for window in ((-0.035, 0.195), (0.195, 0.355), (0.355, 0.445))]
    roller = load_creature("Aschenroller")
    bands = [cut(roller, "X", 0.22, window, math.radians(100), 0.19) for window in ((-0.03, 0.025), (0.025, 0.08), (-0.085, -0.03))]
    back = cut(roller, "X", 0.22, (SHIELD_FROM, SHIELD_FROM + SHIELD_ROWS * SHIELD_ROW), math.radians(70), 0.19)
    for build in (lambda: build_helmet(plates, bands), lambda: build_chest(plates, bands), lambda: build_arm(plates, bands),
                  lambda: build_leg(plates, bands), lambda: build_thigh(plates, bands), lambda: build_shield(back), build_shield_mount):
        gk.clear_scene()
        build()
    gk.log("Schwarzglut-Ruestung fertig")
