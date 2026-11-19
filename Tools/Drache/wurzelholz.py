# The root wood (Wurzelholz) of the plant dragons (Boris 2026-11-19): one shared woody base the combined plants only colour and
# adorn. Inspired by the wood dragon in his reference pictures: a long head of twisted roots with a hooked upper beak, a hooked
# lower jaw, splinter teeth, shaggy root fibres swept back, moss in the crevices. Built here without Tripo:
#
# - SM_Wurzelholz_Schaedel: the skull with the upper jaw, beak and teeth, in the head frame of AVaelPlantTower (cm / 100;
#   X ahead, Y right, Z up, the crown of the snake at 0)
# - SM_Wurzelholz_Kiefer: the lower jaw with its teeth, origin at the jaw hinge (-6, 0, -6)
# - SM_Wurzelholz_Strang_1..3: braided root strands 1 m along +Z for spline meshes (neck, wings, arms later), radius ~7 cm
# - SM_Wurzelholz_Faser: one thin root 1 m along +Z (fibres, tufts, teeth), radius 1 cm
#
# Every mesh: UV 0 for the bark texture (U round, V along), UV 1 "Verlauf" with the ramp from base to tip (X) like the stalks of
# the plant arms, vertex colour R = the same ramp, G = moss, B = glowing veins (thin roots in the grooves, the inside of the
# mouth). So the material M_Wurzelholz can take over every parameter of M_Halm (Wuchs, Wind, Spin) and later dress the arms.
#
#   blender -b --factory-startup -P wurzelholz.py -- <out folder> [<bark texture for the preview>]

import bpy
import bmesh
import math
import os
import random
import sys
from mathutils import Vector, noise

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = ARGS[0] if ARGS else "C:/Projekte/Vael_Quellen/Wurzelholz"
BARK = ARGS[1] if len(ARGS) > 1 else ""
os.makedirs(OUT, exist_ok=True)

HINGE = Vector((-6.0, 0.0, -6.0))


def log(text):
    print("[wurzelholz] " + text)


# ---------------------------------------------------------------- curves and sweeps

def catmull(points, steps=16, ghosts=None):
    pts = [Vector(p) for p in points]
    # ghosts: the points before the start and after the end of a periodic curve, so both ends get the same direction
    if ghosts is not None:
        ext = [Vector(ghosts[0])] + pts + [Vector(ghosts[1])]
    else:
        ext = [pts[0] * 2 - pts[1]] + pts + [pts[-1] * 2 - pts[-2]]
    out, us = [], []
    for i in range(1, len(ext) - 2):
        p0, p1, p2, p3 = ext[i - 1], ext[i], ext[i + 1], ext[i + 2]
        for s in range(steps):
            t = s / steps
            t2, t3 = t * t, t * t * t
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3))
            us.append(i - 1 + t)
    out.append(pts[-1])
    us.append(len(pts) - 1)
    return out, us


def interp(values, u):
    if isinstance(values, (int, float)):
        return float(values)
    if len(values) == 1:
        return float(values[0])
    i = min(int(u), len(values) - 2)
    f = u - i
    return values[i] + (values[i + 1] - values[i]) * f


def path_frames(points, spacing, side_axis=None, close=False, ghosts=None):
    """Samples along a smooth curve through the points, evenly by length: (point, u, length so far, tangent, normal, binormal)"""
    dense, us = catmull(points, ghosts=ghosts)
    cum = [0.0]
    for a, b in zip(dense, dense[1:]):
        cum.append(cum[-1] + (b - a).length)
    total = cum[-1]
    count = max(3, int(total / spacing) + 1)
    samples = []
    k = 0
    for i in range(count):
        s = total * i / (count - 1)
        while k < len(cum) - 2 and cum[k + 1] < s:
            k += 1
        f = (s - cum[k]) / max(cum[k + 1] - cum[k], 1e-6)
        samples.append([dense[k].lerp(dense[k + 1], f), us[k] + (us[k + 1] - us[k]) * f, s])
    # Tangents, then frames carried along without twisting (or held to a fixed side for the cores)
    for i, sample in enumerate(samples):
        a = samples[max(i - 1, 0)][0]
        b = samples[min(i + 1, count - 1)][0]
        # A periodic curve takes its end directions from the curve itself, so both ends match
        if ghosts is not None and (i == 0 or i == count - 1):
            k = 0 if i == 0 else len(dense) - 1
            d = dense[min(k + 1, len(dense) - 1)] - dense[max(k - 1, 0)]
            sample.append(d.normalized())
            continue
        sample.append((b - a).normalized())
    normal = None
    for sample in samples:
        tangent = sample[3]
        if side_axis is not None:
            binormal = Vector(side_axis)
            binormal = (binormal - tangent * binormal.dot(tangent)).normalized()
            normal = binormal.cross(tangent).normalized()
        else:
            if normal is None:
                helper = Vector((0, 0, 1)) if abs(tangent.z) < 0.9 else Vector((1, 0, 0))
                normal = (helper - tangent * helper.dot(tangent)).normalized()
            else:
                normal = (normal - tangent * normal.dot(tangent)).normalized()
            binormal = tangent.cross(normal).normalized()
        sample.append(normal.copy())
        sample.append(binormal)
    # A closed piece (its end continues its start, one metre up): the frame carried along has turned a little about the tangent
    # by the end; that turn is taken back evenly along the way so the end ring lies exactly on the start ring
    if close and len(samples) > 1:
        first_normal, last_normal, tangent = samples[0][4], samples[-1][4], samples[-1][3]
        turn = math.atan2(tangent.dot(last_normal.cross(first_normal)), last_normal.dot(first_normal))
        for sample in samples:
            angle = turn * sample[2] / max(total, 1e-6)
            t, n = sample[3], sample[4]
            n = n * math.cos(angle) + t.cross(n) * math.sin(angle)
            sample[4] = n
            sample[5] = t.cross(n)
    return samples, total


class Builder:
    def __init__(self):
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.ramp = self.bm.loops.layers.uv.new("Verlauf")
        self.col = self.bm.loops.layers.color.new("Col")
        # The importer drops vertex colours, so moss (X) and veins (Y) also ride on UV channel 2
        self.care = self.bm.loops.layers.uv.new("Moos")
        self.ring_log = []
        self.seed = 0

    def sweep(self, points, ry, rz=None, sides=9, spacing=2.5, rough=0.14, grooves=0.07, vein=0.0, moss=1.0, up_moss=0.6,
              ramp=None, side_axis=None, cap=True, inner=None, vlen=45.0, tip_point=True, dents=1.0, stubs=0.0, dome=True, end_fade=0.0,
              close=False, ghosts=None):
        """A tube along the curve; radii per control point (rz for flattened cores). ramp(point) gives the growth ramp,
        inner(direction) the glow of the inside of the mouth. dents: how uneven it is (squeezed oval, lumps, dents), stubs:
        broken-off side twigs per metre; ends without a point are rounded (dome) so nothing ends flat"""
        samples, total = path_frames(points, spacing, side_axis, close, ghosts)
        # A closed piece repeats its bark a whole number of times, so the texture runs on over the seam into the next piece
        if close:
            vlen = total / max(1, round(total / vlen))
        self.seed += 1
        offset = Vector((self.seed * 13.1, self.seed * 7.7, self.seed * 3.3))
        lump_offset = offset * 1.7 + Vector((5.0, 9.0, 2.0))
        u_shift = (self.seed * 0.37) % 1.0
        rings = []
        # Radii are spread over the whole curve, however many control points it has
        def spread(values, u):
            if isinstance(values, (int, float)) or len(values) == len(points):
                return interp(values, u)
            return interp(values, u / max(len(points) - 1, 1) * (len(values) - 1))
        for p, u, s, tangent, normal, binormal in samples:
            # Ends that must meet the next piece exactly stay smooth: unevenness fades out towards them
            fade = min(1.0, s / end_fade, (total - s) / end_fade) if end_fade > 0.0 else 1.0
            fade = fade * fade * (3.0 - 2.0 * fade)
            # Not a pipe: the cross section squeezed oval and twisting, here and there a knot or a dent
            squeeze = 1.0 + 0.22 * dents * fade * noise.noise(p * 0.045 + offset)
            r1 = spread(ry, u) * squeeze
            r2 = (spread(rz, u) if rz is not None else spread(ry, u)) / max(squeeze, 0.5)
            ring = []
            for j in range(sides):
                a = 2 * math.pi * j / sides
                out = normal * math.cos(a) * r1 + binormal * math.sin(a) * r2
                direction = out.normalized() if out.length > 1e-6 else normal
                q = p + out
                lump = noise.noise(q * 0.07 + lump_offset)
                bump = 1.0 + fade * (rough * noise.noise(q * 0.16 + offset) + 0.5 * rough * noise.noise(q * 0.55 + offset)
                                     + grooves * math.sin(a * 5 + noise.noise(q * 0.08 + offset) * 3.0)
                                     + dents * (0.25 * max(0.0, lump - 0.15) - 0.35 * max(0.0, -lump - 0.3))
                                     + dents * 0.08 * noise.noise(q * 1.3 + offset))
                vert = self.bm.verts.new(p + out * bump)
                mossy = max(0.0, min(1.0, 0.3 + 1.2 * noise.noise(q * 0.15 + offset * 0.5) + 0.5 * noise.noise(q * 0.5 + offset) + up_moss * direction.z)) * moss
                glow = vein
                if inner is not None:
                    glow = max(glow, inner(direction))
                g = ramp(p) if ramp is not None else 1.0
                ring.append((vert, (g, mossy, glow), s))
            rings.append(ring)

        # The end rings, so a strand can check that its pieces meet seamlessly
        self.ring_log.append(([entry[0].co.copy() for entry in rings[0]], [entry[0].co.copy() for entry in rings[-1]]))

        def corner(face, entries):
            for loop, (column, ring_entry) in zip(face.loops, entries):
                colour, s = ring_entry[1], ring_entry[2]
                loop[self.uv].uv = ((column / sides + u_shift), s / vlen)
                loop[self.ramp].uv = (colour[0], 0.0 if ramp is not None else 1.0)
                loop[self.col] = (colour[0], colour[1], colour[2], 1.0)
                loop[self.care].uv = (colour[1], colour[2])

        for i in range(len(rings) - 1):
            for j in range(sides):
                k = j + 1
                a, b = rings[i][j], rings[i][k % sides]
                c, d = rings[i + 1][k % sides], rings[i + 1][j]
                face = self.bm.faces.new((a[0], b[0], c[0], d[0]))
                corner(face, [(j, a), (k, b), (k, c), (j, d)])
        if cap:
            for index, sign in ((0, -1.0), (len(rings) - 1, 1.0)):
                ring = rings[index]
                p, tangent = samples[index][0], samples[index][3]
                # A rounded end: two smaller rings bulging out before the point
                if dome and not tip_point:
                    radius = sum((entry[0].co - p).length for entry in ring) / sides
                    for step, (shrink, push) in enumerate(((0.75, 0.45), (0.38, 0.85))):
                        inner_ring = []
                        for j, entry in enumerate(ring):
                            co = p + (entry[0].co - p) * shrink + tangent * sign * radius * push
                            inner_ring.append((self.bm.verts.new(co), entry[1], entry[2]))
                        for j in range(sides):
                            a, b = ring[j], ring[(j + 1) % sides]
                            c, d = inner_ring[(j + 1) % sides], inner_ring[j]
                            verts = (a[0], b[0], c[0], d[0]) if sign > 0 else (a[0], d[0], c[0], b[0])
                            face = self.bm.faces.new(verts)
                            corner(face, [(j, a), (j + 1, b), (j + 1, c), (j, d)] if sign > 0 else [(j, a), (j, d), (j + 1, c), (j + 1, b)])
                        ring = inner_ring
                    tip = self.bm.verts.new(p + tangent * sign * radius * 1.05)
                else:
                    tip = self.bm.verts.new(p + tangent * sign * (0.6 if tip_point else 0.0))
                entry = (tip, ring[0][1], ring[0][2])
                for j in range(sides):
                    a, b = ring[j], ring[(j + 1) % sides]
                    face = self.bm.faces.new((a[0], b[0], tip))
                    corner(face, [(j, a), (j + 1, b), (j, entry)])

        # Broken-off twigs: short stubs standing out of the strand, ending ragged
        if stubs > 0.0:
            rnd = random.Random(self.seed * 31 + 7)
            for _ in range(int(stubs * total / 100.0 + rnd.random())):
                p, u, s, tangent, normal, binormal = samples[rnd.randrange(1, max(2, len(samples) - 1))]
                r = spread(ry, u)
                if r < 1.2:
                    continue
                a = rnd.uniform(0, 2 * math.pi)
                out = (normal * math.cos(a) + binormal * math.sin(a))
                way = (out + tangent * rnd.uniform(0.6, 1.5) * rnd.choice((1.0, -1.0))).normalized()
                length = r * rnd.uniform(0.8, 1.8)
                base = p + out * r * 0.55
                self.sweep([base, base + way * length * 0.5, base + way * length], [r * 0.38, r * 0.26, r * 0.12], sides=6,
                           spacing=max(1.0, length / 4), rough=0.45, grooves=0.1, moss=moss, up_moss=up_moss, ramp=ramp,
                           vlen=vlen, tip_point=True, dents=0.0, dome=False)
        return samples, total

    def braid(self, points, ry, companions=2, wave=26.0, spread=0.8, thin=0.45, **kwargs):
        kwargs.setdefault("stubs", 0.8)
        """A strand with thinner roots winding round it, so every root is a twisted bundle"""
        samples, total = self.sweep(points, ry, **kwargs)
        if companions <= 0:
            return samples
        for c in range(companions):
            phase = 2 * math.pi * c / companions + self.seed * 0.9
            path = []
            radii = []
            for index in range(0, len(samples), 3):
                p, u, s, tangent, normal, binormal = samples[index]
                r = interp(ry, u) if isinstance(ry, (int, float)) or len(ry) == len(points) else interp(ry, u / max(len(points) - 1, 1) * (len(ry) - 1))
                a = phase + 2 * math.pi * s / wave
                path.append(p + (normal * math.cos(a) + binormal * math.sin(a)) * r * spread)
                radii.append(max(r * thin, 0.15))
            if len(path) >= 3:
                extra = dict(kwargs)
                extra.pop("rz", None)
                extra.pop("side_axis", None)
                extra["sides"] = max(5, kwargs.get("sides", 9) - 3)
                extra["vein"] = kwargs.get("vein", 0.0)
                extra["stubs"] = 0.0
                self.sweep(path, radii, **extra)
        return samples

    def finish(self, name, warp=0.0):
        # A slow warp over the whole piece so no two sides are alike
        if warp > 0.0:
            for vert in self.bm.verts:
                co = vert.co
                vert.co = co + Vector((noise.noise(co * 0.03 + Vector((1, 2, 3))), noise.noise(co * 0.03 + Vector((7, 1, 5))),
                                       noise.noise(co * 0.03 + Vector((4, 9, 2))))) * warp
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        mesh = bpy.data.meshes.new(name)
        self.bm.to_mesh(mesh)
        self.bm.free()
        for poly in mesh.polygons:
            poly.use_smooth = True
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        log(f"{name}: {len(mesh.vertices)} verts, {len(mesh.polygons)} faces")
        return obj


# ---------------------------------------------------------------- the head

def section(table, x):
    """Height of the middle, half width and half height of a core at x"""
    if x <= table[0][0]:
        return table[0][1:]
    for a, b in zip(table, table[1:]):
        if a[0] <= x <= b[0]:
            f = (x - a[0]) / (b[0] - a[0])
            return tuple(a[i] + (b[i] - a[i]) * f for i in range(1, 4))
    return table[-1][1:]


# x, middle z, half width, half height
UPPER = [(-50, 9, 9, 9), (-36, 12, 12, 11), (-26, 13, 17, 14), (-8, 13, 19, 14), (14, 11, 17, 12), (36, 8, 14, 10), (56, 6, 10.5, 8),
         (74, 4, 7.5, 6.5), (88, 2, 5, 5.5)]
LOWER = [(-14, -11, 13, 5), (4, -15, 14.5, 6), (28, -19, 13, 5.5), (50, -19.5, 10, 4.5), (68, -16, 6, 4), (80, -12, 3.5, 3.2)]
EYE = Vector((38.0, 13.0, 13.0))


def on_core(table, x, angle, side, lift=1.0):
    zc, w, h = section(table, x)
    return Vector((x, side * w * math.cos(angle) * lift, zc + h * math.sin(angle) * lift))


def core(builder, table, inner_sign):
    points = [(x, 0, zc) for x, zc, w, h in table]
    widths = [w for x, zc, w, h in table]
    heights = [h for x, zc, w, h in table]
    # The inside of the mouth glows faintly in the colour of the plant (the palate above, the tongue bed below)
    builder.sweep(points, widths, rz=heights, sides=16, spacing=3.0, rough=0.08, grooves=0.04, moss=0.5, side_axis=(0, 1, 0),
                  inner=lambda d: max(0.0, min(0.22, (d.z * inner_sign - 0.4) * 0.5)), tip_point=False)


def plates(b, rnd, table, side, angles, xs, count, width, length, lift=1.07):
    """Flat bark plates lying on the core like scales, overlapping, so the head is not only a stack of round roots"""
    for _ in range(count):
        angle = math.radians(rnd.uniform(*angles))
        x0 = rnd.uniform(*xs)
        span = rnd.uniform(*length)
        points = [on_core(table, x0 + span * t, angle + rnd.uniform(-0.05, 0.05), side, lift) for t in (0.0, 0.33, 0.66, 1.0)]
        zc = section(table, x0 + span * 0.5)[0]
        normal = Vector((0.0, side * math.cos(angle), math.sin(angle))).normalized()
        w = rnd.uniform(*width)
        b.sweep(points, [w * 0.5, w, w * 0.9, w * 0.35], rz=[0.8, 1.4, 1.3, 0.6], side_axis=tuple(normal), sides=10, spacing=2.5,
                rough=0.2, grooves=0.12, dents=0.6, moss=1.2, up_moss=0.8)


def skull(rnd):
    b = Builder()
    core(b, UPPER, -1.0)
    # The nape: a thick knot of wood at the back of the skull that the strands of the neck run into
    b.sweep([(-16, 0, 9), (-34, 0, 8), (-50, 0, 6), (-62, 0, 3)], [14, 13, 11.5, 10], rz=[13, 12, 11, 9.5], side_axis=(0, 1, 0),
            sides=14, spacing=3.0, rough=0.16, grooves=0.1, dents=1.2, moss=1.0, tip_point=False)

    # The ridge down the middle of the skull into the hooked beak, and the hook itself
    b.braid([(-40, 0, 20), (-20, 0, 27), (10, 0, 25), (40, 0, 18), (66, 0, 12), (84, 0, 7), (95, 0, 0), (99, 0, -9), (96, 0, -19), (91, 0, -25)],
            [2.5, 6, 5.2, 4.5, 4, 3.6, 3.2, 2.4, 1.4, 0.3], companions=2)
    b.braid([(80, 0, 5), (92, 0, 2), (98, 0, -6), (97, 0, -15), (93, 0, -22)], [6, 5.5, 4.2, 2.6, 0.6],
            rz=[5.5, 5, 3.6, 2.2, 0.5], side_axis=(0, 1, 0), companions=0, sides=12)

    for side in (1.0, -1.0):
        # Heavy brows over the sunken eyes, running down the snout into the beak
        b.braid([(-30, side * 11, 18), (-6, side * 16, 23), (20, side * 16, 21), (36, side * 17, 20), (50, side * 13, 15), (72, side * 7, 10), (90, side * 3, 2), (95, side * 1.5, -10), (92, side * 0.5, -21)],
                [2.0, 5.5, 4.6, 4.4, 3.6, 3, 2.4, 1.6, 0.4], companions=2)
        # The upper lip: the edge the teeth hang from
        b.braid([(-28, side * 17, 2), (-8, side * 20, 0), (16, side * 18.5, -2), (40, side * 15, -3.5), (64, side * 10, -3), (84, side * 5.5, -4), (94, side * 2.5, -12), (92, side * 1, -21)],
                [2.4, 5.5, 5, 4.4, 3.6, 2.8, 1.8, 0.4], companions=2)
        # The cheek under the eye
        b.braid([(-32, side * 16, 9), (-8, side * 19.5, 8), (18, side * 18, 6), (36, side * 15, 4), (58, side * 10.5, 3), (82, side * 5.5, 0)],
                [2.0, 5, 4.4, 3.6, 3, 1.6], companions=2)
        # Nostril knots
        b.sweep([(76, side * 4, 8), (82, side * 6.5, 7), (86, side * 5.5, 4)], [2.2, 2.6, 1.4], sides=8)

        # Roots woven round the skull from the back to the snout, parting round the eye
        for angle_deg in range(84, -42, -9):
            angle = math.radians(angle_deg + rnd.uniform(-3, 3))
            points = []
            for x in range(-38, 90, 9):
                x = x + rnd.uniform(-2, 2)
                p = on_core(UPPER, x, angle + rnd.uniform(-0.06, 0.06), side, 1.04)
                to_eye = Vector((p.x - EYE.x, p.y - side * EYE.y, p.z - EYE.z))
                if to_eye.length < 9.0:
                    p.z += (1 if to_eye.z >= 0 else -1) * (9.0 - to_eye.length) * 0.9
                points.append(p)
            # They run on into the hook of the beak
            points.append(Vector((95, side * 1.5, -4 + 6 * math.sin(angle))))
            radius = rnd.uniform(1.2, 3.0)
            # They grow out of the nape thin, swell and thin out into the beak
            b.braid(points, [radius * 0.3, radius, radius * 1.1, radius * 0.9, radius * 0.6, 0.25], companions=2, spread=0.75, thin=0.4)
        # Thin glowing roots in the grooves between them
        for angle_deg in (54, 22, -10):
            angle = math.radians(angle_deg)
            points = []
            for x in range(-32, 76, 12):
                p = on_core(UPPER, x, angle, side, 1.02)
                to_eye = Vector((p.x - EYE.x, p.y - side * EYE.y, p.z - EYE.z))
                if to_eye.length < 8.0:
                    p.z += (1 if to_eye.z >= 0 else -1) * (8.0 - to_eye.length)
                points.append(p)
            b.sweep(points, [1.1, 1.0, 0.9, 0.5], sides=6, vein=1.0, moss=0.0, rough=0.25)

        # Flat bark plates on the cheeks, the snout and the skull, between the roots
        plates(b, rnd, UPPER, side, (-35, 5), (-28, 50), 9, (5.0, 8.0), (16.0, 28.0))
        plates(b, rnd, UPPER, side, (40, 80), (-34, 60), 6, (4.0, 7.0), (14.0, 24.0))
        plates(b, rnd, UPPER, side, (-10, 30), (50, 78), 3, (3.0, 5.0), (10.0, 16.0))

        # Gnarled horns swept back
        b.braid([(-14, side * 13, 25), (-30, side * 19, 35), (-46, side * 24, 38), (-60, side * 27, 33), (-68, side * 27, 26)],
                [4.2, 3.6, 2.8, 1.6, 0.3], companions=2, wave=18)
        b.braid([(-24, side * 8, 25), (-38, side * 11, 31), (-52, side * 12, 29), (-60, side * 11, 24)], [3, 2.4, 1.4, 0.3],
                companions=1, wave=16)

        # Roots from the back of the skull curving down into the neck
        for k in range(4):
            angle = math.radians(-30 + k * 30)
            start = on_core(UPPER, -30, angle, side, 1.0)
            b.braid([start, start + Vector((-14, -side * 2, -3)), start + Vector((-28, -side * 4, -6)), start + Vector((-40, -side * 5, -8))],
                    [3.8, 3.4, 2.4, 0.8], companions=1)

        # Teeth: splinters hanging from the lip, the front ones longest
        for x in list(range(-18, 88, 4)) + list(range(-10, 60, 7)):
            inner_row = x % 4 != 2 and x < 60 and x % 7 == 4 % 7
            x = x + rnd.uniform(-1.2, 1.2)
            zc, w, h = section(UPPER, x)
            root = Vector((x, side * w * rnd.uniform(0.7, 0.9) * (0.75 if inner_row else 1.0), zc - h * 0.62))
            length = rnd.uniform(5.5, 11.0) + max(0.0, x - 56) * 0.35
            way = Vector((0.12, -side * 0.15, -1.0)).normalized()
            back = Vector((-1.0, 0.0, 0.0))
            b.sweep([root, root + way * length * 0.5 + back * length * 0.1, root + way * length + back * length * 0.22],
                    [rnd.uniform(1.5, 2.2), 1.0, 0.12], sides=6, spacing=1.2, moss=0.0, rough=0.3, dents=0.3)

    # Shaggy root fibres swept back off the skull, longer towards the back
    for index in range(90):
        x = rnd.uniform(-36, 52)
        angle = rnd.uniform(math.radians(25), math.radians(155))
        side = 1.0 if angle < math.pi / 2 else -1.0
        root = on_core(UPPER, x, angle if side > 0 else math.pi - angle, side, 1.08)
        outward = Vector((0, root.y, root.z - section(UPPER, x)[0])).normalized()
        way = (Vector((-1.0, 0.0, 0.3)) + outward * 0.55 + Vector((rnd.uniform(-0.2, 0.2), rnd.uniform(-0.3, 0.3), rnd.uniform(-0.2, 0.3)))).normalized()
        length = rnd.uniform(14, 28) + (52 - x) * 0.3
        bend = Vector((0, 0, -0.35)) + Vector((rnd.uniform(-0.25, 0.25), rnd.uniform(-0.25, 0.25), 0))
        wave = Vector((rnd.uniform(-1, 1), rnd.uniform(-1, 1), rnd.uniform(-1, 1))) * length * 0.08
        b.sweep([root, root + way * length * 0.35 + bend * length * 0.06 + wave, root + way * length * 0.7 + bend * length * 0.25 - wave,
                 root + way * length + bend * length * 0.5],
                [rnd.uniform(0.6, 1.0), 0.5, 0.35, 0.06], sides=5, spacing=2.0, rough=0.25, moss=0.4)
    return b.finish("SM_Wurzelholz_Schaedel", warp=2.2)


def jaw(rnd):
    b = Builder()
    core(b, LOWER, 1.0)
    # The chin down the middle into the hook that curls up
    b.braid([(-10, 0, -18), (20, 0, -25.5), (50, 0, -24.5), (72, 0, -19), (84, 0, -12), (89, 0, -5), (89, 0, 1)],
            [5, 5, 4.4, 3.6, 2.6, 1.6, 0.3], companions=2)
    b.braid([(66, 0, -16), (78, 0, -14), (86, 0, -9), (89, 0, -2)], [4.6, 3.8, 2.4, 0.5], rz=[3.8, 3.2, 2.2, 0.4],
            side_axis=(0, 1, 0), companions=0, sides=10)
    for side in (1.0, -1.0):
        # The jaw line, the edge the teeth stand on
        b.braid([(-14, side * 15, -8), (4, side * 16.5, -12), (30, side * 14.5, -15.5), (54, side * 10, -16), (74, side * 5, -12.5), (86, side * 2, -7), (88, side * 0.5, -1)],
                [5, 4.8, 4.2, 3.4, 2.4, 1.4, 0.3], companions=2)
        b.braid([(-12, side * 13, -15), (18, side * 14.5, -22), (48, side * 10, -22), (72, side * 4.5, -17)], [4.2, 4, 3.2, 1.4], companions=1)
        for angle_deg in (-70, -40, -10, 20):
            angle = math.radians(angle_deg)
            points = [on_core(LOWER, x + rnd.uniform(-2, 2), angle, side, 1.06) for x in range(-14, 76, 10)]
            radius = rnd.uniform(1.8, 2.8)
            b.braid(points, [radius, radius * 0.9, radius * 0.7, 0.4], companions=1, spread=0.7)
        b.sweep([on_core(LOWER, x, math.radians(-25), side, 1.03) for x in range(-10, 70, 12)], [1.0, 0.9, 0.5], sides=6, vein=1.0,
                moss=0.0, rough=0.25)
        plates(b, rnd, LOWER, side, (-60, 10), (-12, 60), 7, (4.0, 6.5), (14.0, 24.0))
        # Teeth standing up from the jaw line
        for x in range(-12, 84, 4):
            x = x + rnd.uniform(-1.2, 1.2)
            zc, w, h = section(LOWER, x)
            root = Vector((x, side * w * rnd.uniform(0.7, 0.88), zc + h * 0.7))
            length = rnd.uniform(4.5, 9.0) + max(0.0, x - 58) * 0.35
            way = Vector((-0.12, -side * 0.15, 1.0)).normalized()
            back = Vector((-1.0, 0.0, 0.0))
            b.sweep([root, root + way * length * 0.5 + back * length * 0.08, root + way * length + back * length * 0.18],
                    [rnd.uniform(1.4, 2.0), 0.9, 0.1], sides=6, spacing=1.2, moss=0.0, rough=0.3, dents=0.3)
    # A beard of roots hanging from the chin
    for index in range(9):
        x = rnd.uniform(4, 56)
        root = on_core(LOWER, x, math.radians(-90 + rnd.uniform(-35, 35)), 1.0, 1.05)
        way = Vector((-0.45 + rnd.uniform(-0.2, 0.2), rnd.uniform(-0.3, 0.3), -1.0)).normalized()
        length = rnd.uniform(10, 26)
        b.sweep([root, root + way * length * 0.5 + Vector((-length * 0.1, 0, 0)), root + way * length + Vector((-length * 0.25, 0, 0))],
                [rnd.uniform(0.8, 1.2), 0.6, 0.08], sides=5, spacing=2.0, rough=0.25, moss=0.4)
    obj = b.finish("SM_Wurzelholz_Kiefer", warp=1.6)
    # Its origin is the hinge
    for vert in obj.data.vertices:
        vert.co -= HINGE
    return obj


# ---------------------------------------------------------------- strands for spline meshes

def strand(name, subs, turns, seed, vein_sub=True):
    rnd = random.Random(seed)
    b = Builder()
    b.seed = seed * 10
    ramp = lambda p: max(0.0, min(1.0, p.z / 100.0))
    for k, (ro, rr) in enumerate(subs):
        phase = 2 * math.pi * k / len(subs) + rnd.uniform(-0.3, 0.3)
        points = []
        for i in range(0, 21):
            z = i * 5.0
            a = phase + turns * 2 * math.pi * z / 100.0
            # Periodic over the metre, so the end meets the start of the next piece
            wobble = 0.6 * math.sin(2 * math.pi * 2 * z / 100.0 + k * 2.0)
            points.append(((ro + wobble) * math.cos(a), (ro + wobble) * math.sin(a), z))
        radii = [rr * rnd.uniform(0.75, 1.2) for _ in points]
        # The ends a little fuller, as thick as the knotty middle looks, so no waist shows where two pieces meet
        radii[0] = radii[1] = radii[-1] = radii[-2] = rr * 1.08
        logged = len(b.ring_log)
        lift = Vector((0.0, 0.0, 100.0))
        b.sweep(points, radii, sides=9, spacing=3.5, ramp=ramp, up_moss=0.0, vlen=40.0, stubs=2.0, end_fade=14.0,
                tip_point=False, close=True, ghosts=(Vector(points[-2]) - lift, Vector(points[1]) + lift), cap=False)
        # The end ring must lie on the start ring one metre up, so the next piece of the vine continues it seamlessly
        first, last = b.ring_log[logged]
        gap = max(min((a + Vector((0.0, 0.0, 100.0)) - c).length for c in last) for a in first)
        log(f"{name} root {k}: seam gap {gap:.2f} cm")
    if vein_sub:
        # A thin glowing root lying in the groove between two of them
        phase = math.pi / len(subs)
        ro = subs[0][0] + subs[0][1] * 0.55
        points = []
        for i in range(0, 21):
            z = i * 5.0
            a = phase + turns * 2 * math.pi * z / 100.0
            points.append((ro * math.cos(a), ro * math.sin(a), z))
        lift = Vector((0.0, 0.0, 100.0))
        logged = len(b.ring_log)
        b.sweep(points, [1.0] * len(points), sides=5, spacing=4.0, ramp=ramp, vein=1.0, moss=0.0, rough=0.3, tip_point=False, end_fade=14.0,
                close=True, ghosts=(Vector(points[-2]) - lift, Vector(points[1]) + lift), cap=False)
        first, last = b.ring_log[logged]
        gap = max(min((a + lift - c).length for c in last) for a in first)
        log(f"{name} vein: seam gap {gap:.2f} cm")
    return b.finish(name)


def fibre(name):
    b = Builder()
    b.seed = 77
    ramp = lambda p: max(0.0, min(1.0, p.z / 100.0))
    points = [(1.2 * math.sin(z * 0.07), 1.0 * math.cos(z * 0.05), z) for z in range(0, 101, 10)]
    b.sweep(points, [1.0] * len(points), sides=6, spacing=4.0, ramp=ramp, up_moss=0.0, rough=0.2, tip_point=False)
    return b.finish(name)


# ---------------------------------------------------------------- export and preview

def export(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    saved = obj.scale.copy()
    obj.scale = (0.01, 0.01, 0.01)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    path = os.path.join(OUT, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                             mesh_smooth_type="FACE", colors_type="SRGB", add_leaf_bones=False)
    # Back to cm for the preview
    obj.scale = (100.0, 100.0, 100.0)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.scale = saved
    log("exported " + path)


def preview_material(glow):
    mat = bpy.data.materials.new("Wurzelholz")
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    attr = nodes.new("ShaderNodeAttribute")
    attr.attribute_name = "Col"
    split = nodes.new("ShaderNodeSeparateColor")
    links.new(attr.outputs["Color"], split.inputs["Color"])
    bark = nodes.new("ShaderNodeRGB")
    bark.outputs[0].default_value = (0.16, 0.13, 0.09, 1)
    if BARK and os.path.exists(BARK):
        uv = nodes.new("ShaderNodeUVMap")
        uv.uv_map = "UVMap"
        tex = nodes.new("ShaderNodeTexImage")
        tex.image = bpy.data.images.load(BARK, check_existing=True)
        links.new(uv.outputs["UV"], tex.inputs["Vector"])
        tint = nodes.new("ShaderNodeMix")
        tint.data_type = "RGBA"
        tint.blend_type = "MULTIPLY"
        tint.inputs["Factor"].default_value = 1.0
        links.new(tex.outputs["Color"], tint.inputs[6])
        tint.inputs[7].default_value = (0.42, 0.38, 0.3, 1)
        bark = tint
    moss_mask = nodes.new("ShaderNodeMapRange")
    moss_mask.inputs["From Min"].default_value = 0.62
    moss_mask.inputs["From Max"].default_value = 0.8
    links.new(split.outputs["Green"], moss_mask.inputs["Value"])
    mix = nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    links.new(moss_mask.outputs["Result"], mix.inputs["Factor"])
    links.new(bark.outputs[0] if bark.type == "RGB" else bark.outputs[2], mix.inputs[6])
    mix.inputs[7].default_value = (0.05, 0.075, 0.025, 1)
    links.new(mix.outputs[2], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.85
    emit = nodes.new("ShaderNodeMath")
    emit.operation = "MULTIPLY"
    links.new(split.outputs["Blue"], emit.inputs[0])
    emit.inputs[1].default_value = 4.0
    bsdf.inputs["Emission Color"].default_value = glow
    links.new(emit.outputs[0], bsdf.inputs["Emission Strength"])
    return mat


def preview(head, lower, strand_obj):
    scene = bpy.context.scene
    mat = preview_material((1.0, 0.35, 0.06, 1.0))
    for obj in (head, lower, strand_obj):
        obj.data.materials.append(mat)
    # The jaw a little open on its hinge
    lower.location = HINGE
    lower.rotation_euler = (0.0, math.radians(26), 0.0)
    # A neck of strands rising from below behind the head
    neck = Builder()
    neck.seed = 500
    for k in range(5):
        phase = 2 * math.pi * k / 5
        points = []
        for i in range(0, 13):
            t = i / 12.0
            centre = Vector((-150 + 125 * math.sin(t * math.pi / 2), 0, -230 + 222 * t))
            a = phase + t * 5.0
            ro = 13 - 4 * t
            points.append(centre + Vector((ro * math.cos(a) * 0.6, ro * math.sin(a), ro * math.cos(a) * 0.4)))
        neck.braid(points, [9 - 3 * (i / 12.0) for i in range(13)], companions=1)
    neck_obj = neck.finish("Hals")
    neck_obj.data.materials.append(mat)
    strand_obj.location = (-200, 80, -230)

    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
    scene.render.resolution_x = 900
    scene.render.resolution_y = 620
    world = bpy.data.worlds.new("W")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.42, 0.48, 0.55, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.8
    scene.world = world
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(50), math.radians(-20), math.radians(-60))
    scene.collection.objects.link(sun)
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    target = bpy.data.objects.new("Target", None)
    scene.collection.objects.link(target)
    track = cam.constraints.new("TRACK_TO")
    track.target = target
    track.track_axis = "TRACK_NEGATIVE_Z"
    track.up_axis = "UP_Y"
    scene.camera = cam
    for name, place, look, lens in (("seite", (60, -260, 10), (20, 0, -20), 40), ("schraeg", (210, -170, 40), (20, 0, -10), 38),
                                     ("front", (250, -30, 0), (30, 0, -8), 45)):
        cam.location = place
        target.location = look
        cam.data.lens = lens
        scene.render.filepath = os.path.join(OUT, "Wurzelholz_" + name + ".png")
        bpy.ops.render.render(write_still=True)


bpy.ops.wm.read_factory_settings(use_empty=True)
random_stream = random.Random(19)
head_obj = skull(random_stream)
jaw_obj = jaw(random_stream)
strands = [strand("SM_Wurzelholz_Strang_1", [(3.6, 3.4), (3.6, 3.0), (3.6, 3.2), (3.6, 2.8)], 1.0, 1),
           strand("SM_Wurzelholz_Strang_2", [(3.2, 3.8), (3.2, 3.2), (3.2, 3.4)], 1.0, 2),
           strand("SM_Wurzelholz_Strang_3", [(4.2, 2.6), (4.2, 2.4), (4.2, 2.8), (4.2, 2.2), (4.2, 2.5)], 2.0, 3)]
fibre_obj = fibre("SM_Wurzelholz_Faser")
for obj in [head_obj, jaw_obj, fibre_obj] + strands:
    export(obj)
for obj in strands[1:] + [fibre_obj]:
    obj.hide_render = True
preview(head_obj, jaw_obj, strands[0])
log("done")
