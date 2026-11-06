"""
Refugium Glutkammer: builds the look-test pieces of the brood chamber procedurally (no Tripo) and exports them as FBX.

  SM_GK_Rippenwand    four ribs of petrified tissue rising from the floor and leaning into the room (+X), with
                      membranes stretched between them (material slots: 0 Rippe, 1 Membran)
  SM_GK_Eisack        glowing sac full of small eggs (the skin bulges over each), thin veins, soft strands into the wall (0 Sack, 1 Ader)
  SM_GK_Gelegebeet    torn-open clutch sac of the queen as a bed: leathery skin bulging over eggs, small insect eggs
                      in clusters on the inner wall (some burst), soil in the middle (0 Haut, 1 Erde, 2 Eier)
  SM_GK_Wurzelvorhang roots hanging from the crack in the ceiling down to the floor, some spreading on it (0 Rinde)

Vertex colour R and UV channel 1 carry a mask for the materials: membrane = distance from the ribs (glow in the middle), sac = glow,
bed = egg bulges and eggs, roots = 0 at the top to 1 at the tips. Pivots sit at the bottom middle, +X is the room side.

Run:
  blender -b --factory-startup -P glutkammer_probe.py -- <output folder> [seed]
"""
import bpy, bmesh, sys, os, math, random
from mathutils import Vector, Matrix, noise

args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = args[0] if args else "C:/Projekte/Vael_Quellen/Glutkammer/04_export"
SEED = int(args[1]) if len(args) > 1 else 11
random.seed(SEED)
os.makedirs(OUT, exist_ok=True)


def log(text):
    print("GLUTKAMMER " + text, flush=True)


def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def new_object(name, slots):
    mesh = bpy.data.meshes.new(name)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    for slot in slots:
        mat = bpy.data.materials.get(slot) or bpy.data.materials.new(slot)
        obj.data.materials.append(mat)
    return obj


class Builder:
    """Collects faces with UVs, vertex colour and a material index in one bmesh"""

    def __init__(self):
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.col = self.bm.loops.layers.color.new("Col")
        self.mask = self.bm.loops.layers.uv.new("Maske")      # the same mask in UV channel 1, which every importer keeps

    def grid(self, rows, material, uv_of, mask_of, closed_u=False):
        """rows: list of lists of points (v along the rows, u along each row)"""
        verts = [[self.bm.verts.new(p) for p in row] for row in rows]
        nu = len(rows[0])
        last_u = nu if closed_u else nu - 1
        for j in range(len(rows) - 1):
            for i in range(last_u):
                i2 = (i + 1) % nu
                quad = (verts[j][i], verts[j][i2], verts[j + 1][i2], verts[j + 1][i])
                try:
                    face = self.bm.faces.new(quad)
                except ValueError:
                    continue
                face.material_index = material
                for loop, (jj, ii, wrap) in zip(face.loops, ((j, i, False), (j, i + 1, True), (j + 1, i + 1, True), (j + 1, i, False))):
                    loop[self.uv].uv = uv_of(jj, ii)
                    m = mask_of(jj, ii % nu)
                    loop[self.col] = (m, m, m, 1.0)
                    loop[self.mask].uv = (m, m)
        return verts

    def tube(self, points, radii, material, sides=10, v_scale=1.0, mask_of=None, cap_end=True, flat=1.0, rough=0.0, seed=0):
        """A tube along points with a radius per point; u around, v along (metres * v_scale).
        flat < 1 squashes the cross-section (bone-like), rough pushes the surface in and out with noise (share of the radius)"""
        rows = []
        prev_normal = None
        lengths = [0.0]
        for a, b in zip(points, points[1:]):
            lengths.append(lengths[-1] + (b - a).length)
        for k, p in enumerate(points):
            tangent = (points[min(k + 1, len(points) - 1)] - points[max(k - 1, 0)]).normalized()
            if prev_normal is None:
                helper = Vector((0, 0, 1)) if abs(tangent.z) < 0.9 else Vector((1, 0, 0))
                prev_normal = tangent.cross(helper).normalized()
            normal = (prev_normal - tangent * prev_normal.dot(tangent)).normalized()
            prev_normal = normal
            binormal = tangent.cross(normal)
            row = []
            for s in range(sides):
                ang = 2 * math.pi * s / sides
                direction = normal * math.cos(ang) + binormal * math.sin(ang) * flat
                q = p + direction * radii[k]
                if rough > 0.0:
                    q = q + direction.normalized() * radii[k] * rough * noise.noise(q * 4.0 + Vector((seed, seed * 0.5, 0)))
                row.append(q)
            rows.append(row)
        total = max(lengths[-1], 1e-4)
        verts = self.grid(rows, material,
                          uv_of=lambda j, i: (i / sides, lengths[j] * v_scale),
                          mask_of=(lambda j, i: mask_of(lengths[j] / total)) if mask_of else (lambda j, i: 0.0),
                          closed_u=True)
        if cap_end:
            tip = self.bm.verts.new(points[-1] + (points[-1] - points[-2]).normalized() * radii[-1] * 0.6)
            ring = verts[-1]
            for s in range(sides):
                try:
                    face = self.bm.faces.new((ring[s], ring[(s + 1) % sides], tip))
                    face.material_index = material
                    for loop in face.loops:
                        loop[self.uv].uv = (s / sides, total * v_scale)
                        m = mask_of(1.0) if mask_of else 0.0
                        loop[self.col] = (m, m, m, 1.0)
                        loop[self.mask].uv = (m, m)
                except ValueError:
                    pass

    def finish(self, obj):
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=0.0005)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        self.bm.to_mesh(obj.data)
        self.bm.free()
        for poly in obj.data.polygons:
            poly.use_smooth = True


def wobble(p, scale, amount, seed):
    n = noise.noise_vector(p * scale + Vector((seed * 7.1, seed * 3.3, seed * 1.7)))
    return p + n * amount


def export(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={"MESH"},
        apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
        bake_anim=False, mesh_smooth_type="FACE", colors_type="LINEAR",
    )
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    log(f"{obj.name}: {tris} Dreiecke -> {path}")


# ---------------------------------------------------------------- pieces

def smooth(a, b, x):
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


def egg_cells(p, scale):
    """1 in the middle of an egg-sized cell, 0 on the seams between them (eggs pressing through a skin)"""
    d, _ = noise.voronoi(p * scale, distance_metric="DISTANCE", exponent=2.5)
    return smooth(0.55, 0.12, d[0]) ** 0.7               # round dome around each egg, gaps between them


def rib_curve(y, seed, height, lean, samples=44):
    """One rib: from the floor up, leaning into the room (+X) like the inside of a ribcage"""
    pts = []
    bend = random.uniform(-0.25, 0.25)
    for k in range(samples + 1):
        t = k / samples
        x = lean * t ** 2.1 - 0.35 * math.sin(math.pi * t)
        z = -0.4 + (height + 0.4) * math.sin(t * math.pi / 2)
        yy = y * (1.0 - 0.18 * t) + bend * math.sin(math.pi * t)
        pts.append(wobble(Vector((x, yy, z)), 0.35, 0.18, seed))
    return pts


def rib_radii(points, seed):
    radii = []
    n = len(points) - 1
    for k in range(n + 1):
        t = k / n
        r = 0.42 * (1 - t) + 0.11 * t
        r *= 1.0 + 0.28 * max(0.0, math.sin(t * math.pi * 5.5 + seed)) ** 4        # knuckles
        r *= 1.0 + 0.12 * noise.noise(Vector((t * 9.0, seed, 0.0)))                  # uneven growth
        if t < 0.09:
            r *= 1.0 + (0.09 - t) * 7.0                                              # flared foot
        radii.append(r)
    return radii


def build_rib_wall():
    obj = new_object("SM_GK_Rippenwand", ["Rippe", "Membran"])
    b = Builder()
    spacing = 1.8
    ys = [(-1.5 + i) * spacing for i in range(4)]
    curves = [rib_curve(y, i + SEED, random.uniform(6.0, 7.3), random.uniform(2.2, 3.0)) for i, y in enumerate(ys)]
    for i, pts in enumerate(curves):
        b.tube(pts, rib_radii(pts, i), 0, sides=14, v_scale=0.5, flat=0.7, rough=0.22, seed=i * 3.7)
        for s in range(3):
            base = pts[2 + s] + Vector((0, 0, -0.1))
            d = Vector((random.uniform(-1, 0.6), random.uniform(-0.8, 0.8), 0.0)).normalized()
            spur = [base + d * (0.22 * k) + Vector((0, 0, -0.03 * k * k)) for k in range(6)]
            b.tube(spur, [0.2 * (1 - k / 6) + 0.03 for k in range(6)], 0, sides=8, v_scale=0.5, rough=0.2, seed=s)
    rows_n, cols_n = 40, 16
    for a, c in zip(curves, curves[1:]):
        rows = []
        for j in range(rows_n + 1):
            t = 0.04 + 0.9 * j / rows_n
            ia = t * (len(a) - 1)
            pa = a[int(ia)].lerp(a[min(int(ia) + 1, len(a) - 1)], ia - int(ia))
            pc = c[int(ia)].lerp(c[min(int(ia) + 1, len(c) - 1)], ia - int(ia))
            row = []
            for i in range(cols_n + 1):
                s = i / cols_n
                p = pa.lerp(pc, s)
                sag = math.sin(math.pi * s)
                p = p + Vector((-0.32 * sag, 0, -0.12 * sag))
                p = p + Vector((noise.noise(p * 1.7) * 0.09 * sag, 0, noise.noise(p * 2.3 + Vector((5, 5, 5))) * 0.06 * sag))
                row.append(p)
            rows.append(row)
        b.grid(rows, 1,
               uv_of=lambda j, i: (i / cols_n, j / rows_n * 3.0),
               mask_of=lambda j, i: math.sin(math.pi * i / cols_n) * (0.35 + 0.65 * math.sin(math.pi * j / rows_n)))
    b.finish(obj)
    export(obj)


def build_egg_sac():
    """A sac full of small eggs: the skin bulges over every egg inside, the eggs glow, the seams stay dark"""
    obj = new_object("SM_GK_Eisack", ["Sack", "Ader"])
    b = Builder()
    rings, sides = 60, 52
    height, radius = 1.45, 0.52

    def shape(t, ang):
        z = height * t
        r = radius * math.sin(math.pi * t) ** 0.8 * (1.0 + 0.12 * math.sin(math.pi * t * 0.9))
        return Vector((math.cos(ang) * r, math.sin(ang) * r, z)), r

    def surface(t, ang):
        p, r = shape(t, ang)
        if r < 1e-4:
            return p, 0.0
        egg = egg_cells(p, 7.0) * smooth(0.02, 0.12, t) * smooth(0.98, 0.86, t)
        out = Vector((math.cos(ang), math.sin(ang), 0))
        return p + out * (0.045 * egg + noise.noise(p * 3.0) * 0.02), egg

    grid = [[surface(j / rings, 2 * math.pi * i / sides) for i in range(sides)] for j in range(rings + 1)]
    b.grid([[q[0] for q in row] for row in grid], 0, uv_of=lambda j, i: (i / sides, j / rings),
           mask_of=lambda j, i: 0.25 + 0.75 * grid[j][i % sides][1], closed_u=True)
    # a few thin veins over the seams
    for v in range(5):
        ang0 = 2 * math.pi * v / 5 + random.uniform(-0.3, 0.3)
        pts = []
        for k in range(23):
            t = 0.08 + 0.85 * k / 22
            ang = ang0 + 0.4 * math.sin(t * 5 + v)
            p, _ = surface(t, ang)
            pts.append(p + Vector((math.cos(ang), math.sin(ang), 0)) * 0.012)
        b.tube(pts, [0.012 + 0.008 * math.sin(math.pi * k / 22) for k in range(23)], 1, sides=6)
    # soft strands from the top that curve back into the membrane behind it (-X) and droop
    for v in range(7):
        side = (v - 3) * 0.09
        start, _ = shape(0.95, math.pi + side * 3)
        pts = []
        for k in range(12):
            u = k / 11
            pts.append(wobble(start + Vector((-0.42 * u, side * 2.2 * u, 0.22 * math.sin(u * math.pi * 0.8) - 0.1 * u * u)), 2.5, 0.02, v))
        b.tube(pts, [0.03 * (1 - k / 11) + 0.008 for k in range(12)], 1, sides=6)
    b.finish(obj)
    export(obj)


def build_clutch_bed():
    """Bed: a torn-open clutch sac of the queen, leathery, with small insect eggs lining its inner wall and soil in the middle"""
    obj = new_object("SM_GK_Gelegebeet", ["Haut", "Erde", "Eier"])
    b = Builder()
    rings, sides = 26, 64
    R, H, thick = 0.62, 0.72, 0.03

    def rim(ang):
        # torn, not broken: smooth irregular edge with two deep tears
        tear = sum(math.exp(-(math.atan2(math.sin(ang - a0), math.cos(ang - a0)) / 0.18) ** 2) for a0 in (0.9, 3.6))
        return 0.82 + 0.16 * noise.noise(Vector((math.cos(ang) * 1.5, math.sin(ang) * 1.5, SEED))) - 0.35 * tear

    def point(t, ang, inset):
        h = rim(ang)
        tt = t * h
        r = R * math.sin(min(tt, 1.0) * math.pi * 0.62 + 0.25) / math.sin(math.pi * 0.62 + 0.25) - inset
        z = H * tt
        if tt > 0.7:                                          # torn skin flaps peel outward
            peel = (tt - 0.7) / 0.3
            r += 0.22 * peel * peel
            z -= 0.12 * peel * peel
        r *= 1.0 + 0.08 * noise.noise(Vector((math.cos(ang) * 2, math.sin(ang) * 2, tt * 2)))
        p = Vector((math.cos(ang) * r, math.sin(ang) * r, z))
        egg = egg_cells(p, 9.0) * smooth(0.05, 0.25, tt) * smooth(0.95, 0.7, tt)
        out = Vector((math.cos(ang), math.sin(ang), 0))
        return p + out * (0.03 * egg if inset == 0 else -0.01 * egg), egg

    outer = [[point(j / rings, 2 * math.pi * i / sides, 0.0) for i in range(sides)] for j in range(rings + 1)]
    inner = [[point(j / rings, 2 * math.pi * i / sides, thick) for i in range(sides)] for j in range(rings + 1)]
    b.grid([[q[0] for q in row] for row in outer], 0, uv_of=lambda j, i: (i / sides * 3, j / rings),
           mask_of=lambda j, i: 0.15 + 0.55 * outer[j][i % sides][1], closed_u=True)
    inner_rows = list(reversed(inner))
    b.grid([[q[0] for q in row] for row in inner_rows], 0, uv_of=lambda j, i: (i / sides * 3, 1 - j / rings),
           mask_of=lambda j, i: 0.3 + 0.5 * inner_rows[j][i % sides][1], closed_u=True)
    b.grid([[q[0] for q in outer[-1]], [q[0] for q in inner[-1]]], 0, uv_of=lambda j, i: (i / sides * 3, 0),
           mask_of=lambda j, i: 0.2, closed_u=True)
    # soil, slightly heaped
    soil_z, soil_r = 0.26, 0.36
    soil = [[Vector((math.cos(2 * math.pi * i / sides) * soil_r * (1 - j / 6), math.sin(2 * math.pi * i / sides) * soil_r * (1 - j / 6),
                     soil_z + 0.07 * (j / 6) ** 0.5 + noise.noise(Vector((i * 0.3, j * 0.5, 2))) * 0.015)) for i in range(sides)] for j in range(7)]
    b.grid(soil, 1, uv_of=lambda j, i: (i / sides, j / 6), mask_of=lambda j, i: 0.0, closed_u=True)

    # small elongated eggs in clusters on the inner wall, between soil and rim; some burst and empty
    eggs = 0
    for cluster in range(11):
        ang_c = 2 * math.pi * cluster / 9 + random.uniform(-0.2, 0.2)
        for e in range(random.randint(6, 9)):
            ang = ang_c + random.uniform(-0.28, 0.28)
            t = random.uniform(0.42, 0.85)
            wall, _ = point(t, ang, thick)
            inward = Vector((-math.cos(ang), -math.sin(ang), 0.35)).normalized()
            centre = wall + inward * 0.03
            length, width = random.uniform(0.07, 0.1), random.uniform(0.035, 0.045)
            burst = random.random() < 0.3
            axis = (inward + Vector((0, 0, 0.8))).normalized()
            helper = Vector((1, 0, 0)) if abs(axis.x) < 0.9 else Vector((0, 1, 0))
            u_dir = axis.cross(helper).normalized()
            v_dir = axis.cross(u_dir)
            rows_e = []
            top = 0.55 if burst else 1.0
            for j in range(10):
                phi = -math.pi / 2 + math.pi * top * j / 9
                ring_r = max(math.cos(phi) * width, 0.002)
                rows_e.append([centre + axis * (math.sin(phi) * length) + (u_dir * math.cos(2 * math.pi * s / 12) + v_dir * math.sin(2 * math.pi * s / 12)) * ring_r
                               for s in range(12)])
            glow = 0.15 if burst else random.uniform(0.6, 1.0)
            b.grid(rows_e, 2, uv_of=lambda j, i: (i / 12, j / 9), mask_of=lambda j, i, g=glow: g * math.sin(math.pi * j / 9) ** 1.5 + 0.05,
                   closed_u=True)
            eggs += 1
    log(f"Gelegebeet: {eggs} Eier")
    b.finish(obj)
    export(obj)


def build_root_curtain():
    obj = new_object("SM_GK_Wurzelvorhang", ["Rinde"])
    b = Builder()
    top = 7.5

    def root(start, end_floor, r0, seed, spread=False, steps=52):
        pts = []
        sway = Vector((random.uniform(-1, 1), random.uniform(-1, 1), 0)).normalized() * random.uniform(0.3, 0.8)
        for k in range(steps + 1):
            t = k / steps
            p = start.lerp(end_floor, t) + sway * math.sin(math.pi * t)
            p.z = start.z + (end_floor.z - start.z) * (t ** 0.8)
            p = wobble(p, 0.5, 0.45 * math.sin(math.pi * t) + 0.06, seed)
            pts.append(p)
        if spread:
            d = (Vector((end_floor.x, end_floor.y, 0)) - Vector((start.x, start.y, 0)))
            d = d.normalized() if d.length > 1e-3 else Vector((1, 0, 0))
            last = pts[-1]
            for k in range(1, 12):
                pts.append(wobble(last + d * (0.22 * k) + Vector((0, 0, -0.01 * k)), 1.0, 0.08, seed + 1))
        n = len(pts) - 1
        # thin where it comes out of the crack, thickest a little below, then tapering to the tip
        radii = [max(0.02, r0 * (0.55 + 0.9 * math.sin(math.pi * min(1.0, (k / n) * 1.6)) ** 0.7) * (1 - 0.8 * (k / n) ** 1.2))
                 * (1 + 0.1 * math.sin(k * 0.9 + seed)) for k in range(n + 1)]
        b.tube(pts, radii, 0, sides=10, v_scale=0.8, mask_of=lambda t: t, rough=0.18, seed=seed)
        return pts

    mains = []
    for i in range(6):
        # they come out of a crack line, not a single point
        along = -1.1 + 2.2 * i / 5 + random.uniform(-0.15, 0.15)
        start = Vector((along, random.uniform(-0.25, 0.25), top + random.uniform(-0.3, 0.4)))
        reaches = i % 3 != 1
        ang = random.uniform(0, 2 * math.pi)
        dist = random.uniform(0.5, 1.8)
        end = Vector((along * 0.6 + math.cos(ang) * dist, math.sin(ang) * dist, 0.0 if reaches else random.uniform(1.2, 3.0)))
        mains.append(root(start, end, random.uniform(0.12, 0.18), i * 13 + SEED, spread=reaches))
    for i, pts in enumerate(mains):
        for s in range(5):
            k = random.randint(8, len(pts) // 2)
            base = pts[k]
            end = base + Vector((random.uniform(-1.0, 1.0), random.uniform(-1.0, 1.0), -random.uniform(1.0, max(1.1, base.z - 0.3))))
            end.z = max(end.z, 0.2)
            root(base, end, 0.035 + random.uniform(0, 0.03), 100 + i * 7 + s, steps=22)
    b.finish(obj)
    export(obj)


if __name__ == "__main__":
    for build in (build_rib_wall, build_egg_sac, build_clutch_bed, build_root_curtain):
        clear_scene()
        build()
    log("fertig")
