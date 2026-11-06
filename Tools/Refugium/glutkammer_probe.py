"""
Refugium Glutkammer: builds the look-test pieces of the brood chamber procedurally (no Tripo) and exports them as FBX.

  SM_GK_Rippenwand    four ribs of petrified tissue rising from the floor and leaning into the room (+X), with
                      membranes stretched between them (material slots: 0 Rippe, 1 Membran)
  SM_GK_Eisack        glowing egg sac with raised veins and tendrils on top (0 Sack, 1 Ader)
  SM_GK_Eierschale    broken, empty egg shell as a bed, filled with soil (0 Schale, 1 Erde)
  SM_GK_Wurzelvorhang roots hanging from the crack in the ceiling down to the floor, some spreading on it (0 Rinde)

Vertex colour R and UV channel 1 carry a mask for the materials: membrane = distance from the ribs (glow in the middle), sac = glow,
shell = 1 inside / 0 outside, roots = 0 at the top to 1 at the tips. Pivots sit at the bottom middle, +X is the room side.

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

    def tube(self, points, radii, material, sides=10, v_scale=1.0, mask_of=None, cap_end=True):
        """A tube along points with a radius per point; u around, v along (metres * v_scale)"""
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
                row.append(p + (normal * math.cos(ang) + binormal * math.sin(ang)) * radii[k])
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

def rib_curve(y, seed, height=6.8, lean=2.6, samples=40):
    """One rib: from the floor up, leaning into the room (+X) like the inside of a ribcage"""
    pts = []
    for k in range(samples + 1):
        t = k / samples
        x = lean * t ** 2.1 - 0.35 * math.sin(math.pi * t)
        z = -0.4 + (height + 0.4) * math.sin(t * math.pi / 2)
        yy = y * (1.0 - 0.18 * t)                        # ribs converge a little towards the top
        pts.append(wobble(Vector((x, yy, z)), 0.35, 0.16, seed))
    return pts


def rib_radii(points, seed):
    radii = []
    n = len(points) - 1
    for k in range(n + 1):
        t = k / n
        r = 0.44 * (1 - t) + 0.13 * t
        r *= 1.0 + 0.16 * max(0.0, math.sin(t * math.pi * 6 + seed)) ** 3   # knuckles
        if t < 0.08:
            r *= 1.0 + (0.08 - t) * 6.0                                     # flared foot
        radii.append(r)
    return radii


def build_rib_wall():
    obj = new_object("SM_GK_Rippenwand", ["Rippe", "Membran"])
    b = Builder()
    spacing = 1.8
    ys = [(-1.5 + i) * spacing for i in range(4)]
    curves = [rib_curve(y, i + SEED) for i, y in enumerate(ys)]
    for i, pts in enumerate(curves):
        b.tube(pts, rib_radii(pts, i), 0, sides=12, v_scale=0.5)
        # a few short side spurs at the foot, like roots of the rib
        for s in range(3):
            base = pts[2 + s] + Vector((0, 0, -0.1))
            d = Vector((random.uniform(-1, 0.6), random.uniform(-0.8, 0.8), 0.0)).normalized()
            spur = [base + d * (0.22 * k) + Vector((0, 0, -0.03 * k * k)) for k in range(6)]
            b.tube(spur, [0.2 * (1 - k / 6) + 0.03 for k in range(6)], 0, sides=8, v_scale=0.5)
    # membranes between neighbouring ribs, sagging back into the wall
    rows_n, cols_n = 34, 14
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
                p = p + Vector((noise.noise(p * 1.7) * 0.07 * sag, 0, noise.noise(p * 2.3 + Vector((5, 5, 5))) * 0.05 * sag))
                row.append(p)
            rows.append(row)
        b.grid(rows, 1,
               uv_of=lambda j, i: (i / cols_n, j / rows_n * 3.0),
               mask_of=lambda j, i: math.sin(math.pi * i / cols_n) * (0.35 + 0.65 * math.sin(math.pi * j / rows_n)))
    b.finish(obj)
    export(obj)


def build_egg_sac():
    obj = new_object("SM_GK_Eisack", ["Sack", "Ader"])
    b = Builder()
    rings, sides = 28, 24
    height, radius = 1.45, 0.52

    def surface(t, ang):
        # egg profile, wider below the middle
        z = height * t
        r = radius * math.sin(math.pi * t) ** 0.8 * (1.0 + 0.12 * math.sin(math.pi * t * 0.9))
        p = Vector((math.cos(ang) * r, math.sin(ang) * r, z))
        bump = noise.noise(p * 3.2 + Vector((SEED, 0, 0))) * 0.035 + noise.noise(p * 9.0) * 0.01
        if r > 1e-4:
            p = p + Vector((math.cos(ang), math.sin(ang), 0)) * bump
        return p

    rows = [[surface(j / rings, 2 * math.pi * i / sides) for i in range(sides)] for j in range(rings + 1)]
    b.grid(rows, 0, uv_of=lambda j, i: (i / sides, j / rings),
           mask_of=lambda j, i: 0.55 + 0.45 * math.sin(math.pi * j / rings) ** 2, closed_u=True)
    # raised veins along the surface
    for v in range(7):
        ang0 = 2 * math.pi * v / 7 + random.uniform(-0.2, 0.2)
        pts = []
        for k in range(23):
            t = 0.06 + 0.9 * k / 22
            ang = ang0 + 0.35 * math.sin(t * 5 + v)
            p = surface(t, ang)
            outward = Vector((p.x, p.y, 0)).normalized() if Vector((p.x, p.y)).length > 1e-3 else Vector((0, 0, 1))
            pts.append(p + outward * 0.012)
        b.tube(pts, [0.018 + 0.012 * math.sin(math.pi * k / 22) for k in range(23)], 1, sides=6, v_scale=1.0)
    # tendrils on top that hold it in the membrane
    for v in range(4):
        start = surface(0.97, v * math.pi / 2)
        d = Vector((math.cos(v * math.pi / 2) * 0.5, math.sin(v * math.pi / 2) * 0.5, 1.0)).normalized()
        pts = [wobble(start + d * (0.09 * k), 2.0, 0.03 * k / 8, v) for k in range(9)]
        b.tube(pts, [0.045 * (1 - k / 9) + 0.01 for k in range(9)], 1, sides=6)
    b.finish(obj)
    export(obj)


def build_egg_shell():
    obj = new_object("SM_GK_Eierschale", ["Schale", "Erde"])
    b = Builder()
    rings, sides = 18, 40
    rx, rz, thick = 0.62, 0.86, 0.035

    def rim_height(ang):
        # jagged break line around the top: zigzag teeth plus noise
        teeth = abs(((ang / (2 * math.pi) * 11) % 1.0) - 0.5) * 2.0
        return 0.50 + 0.14 * teeth + 0.08 * noise.noise(Vector((math.cos(ang) * 2, math.sin(ang) * 2, SEED)))

    def point(t, ang, r_off):
        # t from bottom (0) to the rim (1) of a lower egg half
        h = rim_height(ang) * t
        phi = -math.pi / 2 + math.pi * h
        r = (rx + r_off) * math.cos(phi)
        z = (rz + r_off) * (math.sin(phi) + 1.0)
        p = Vector((math.cos(ang) * r, math.sin(ang) * r, z))
        return p + p.normalized() * noise.noise(p * 5.0) * 0.01

    outer = [[point(j / rings, 2 * math.pi * i / sides, 0.0) for i in range(sides)] for j in range(rings + 1)]
    inner = [[point(j / rings, 2 * math.pi * i / sides, -thick) for i in range(sides)] for j in range(rings + 1)]
    b.grid(outer, 0, uv_of=lambda j, i: (i / sides * 2, j / rings), mask_of=lambda j, i: 0.0, closed_u=True)
    b.grid(list(reversed(inner)), 0, uv_of=lambda j, i: (i / sides * 2, 1 - j / rings), mask_of=lambda j, i: 1.0, closed_u=True)
    b.grid([outer[-1], inner[-1]], 0, uv_of=lambda j, i: (i / sides * 2, 0), mask_of=lambda j, i: 0.5, closed_u=True)
    # soil inside, a little below the lowest tooth, slightly heaped
    soil_z = (rz - thick) * (math.sin(-math.pi / 2 + math.pi * 0.42) + 1.0)
    soil_r = (rx - thick) * math.cos(-math.pi / 2 + math.pi * 0.42)
    soil = []
    for j in range(7):
        rr = soil_r * (1 - j / 6) * 0.98
        soil.append([Vector((math.cos(2 * math.pi * i / sides) * rr, math.sin(2 * math.pi * i / sides) * rr,
                             soil_z + 0.06 * (j / 6) ** 0.5 + noise.noise(Vector((i * 0.3, j * 0.5, 2))) * 0.015))
                     for i in range(sides)])
    b.grid(soil, 1, uv_of=lambda j, i: (i / sides, j / 6), mask_of=lambda j, i: 0.0, closed_u=True)
    b.finish(obj)
    export(obj)


def build_root_curtain():
    obj = new_object("SM_GK_Wurzelvorhang", ["Rinde"])
    b = Builder()
    top = 7.5

    def root(start, end_floor, r0, seed, spread=False, steps=48):
        pts = []
        for k in range(steps + 1):
            t = k / steps
            p = start.lerp(end_floor, t)
            p.z = start.z + (end_floor.z - start.z) * (t ** 0.85)
            p = wobble(p, 0.45, 0.35 * math.sin(math.pi * t) + 0.05, seed)
            pts.append(p)
        if spread:
            d = (Vector((end_floor.x, end_floor.y, 0)) - Vector((start.x, start.y, 0)))
            d = d.normalized() if d.length > 1e-3 else Vector((1, 0, 0))
            last = pts[-1]
            for k in range(1, 12):
                pts.append(wobble(last + d * (0.22 * k) + Vector((0, 0, -0.01 * k)), 1.0, 0.08, seed + 1))
        n = len(pts) - 1
        radii = [max(0.025, r0 * (1 - 0.82 * (k / n) ** 0.9)) * (1 + 0.08 * math.sin(k * 0.9 + seed)) for k in range(n + 1)]
        b.tube(pts, radii, 0, sides=10, v_scale=0.8, mask_of=lambda t: t)
        return pts

    mains = []
    for i in range(7):
        ang = 2 * math.pi * i / 7 + random.uniform(-0.3, 0.3)
        start = Vector((math.cos(ang) * 0.5, math.sin(ang) * 0.5, top + random.uniform(0, 0.6)))
        reaches = i % 3 != 2
        dist = random.uniform(0.6, 2.2)
        end = Vector((math.cos(ang) * dist, math.sin(ang) * dist, 0.0 if reaches else random.uniform(1.0, 2.6)))
        mains.append(root(start, end, random.uniform(0.2, 0.3), i * 13 + SEED, spread=reaches))
    # thinner rootlets branching off the main roots and hanging down
    for i, pts in enumerate(mains):
        for s in range(4):
            k = random.randint(6, len(pts) // 2)
            base = pts[k]
            end = base + Vector((random.uniform(-0.9, 0.9), random.uniform(-0.9, 0.9), -random.uniform(1.2, base.z - 0.3 if base.z > 1.6 else 1.0)))
            end.z = max(end.z, 0.25)
            root(base, end, 0.06 + random.uniform(0, 0.04), 100 + i * 7 + s, steps=20)
    b.finish(obj)
    export(obj)


if __name__ == "__main__":
    for build in (build_rib_wall, build_egg_sac, build_egg_shell, build_root_curtain):
        clear_scene()
        build()
    log("fertig")
