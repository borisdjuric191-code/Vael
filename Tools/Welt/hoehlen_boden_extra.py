"""
Kleinteile für einen unperfekten Höhlenboden (Boris 2026-11-27: "den Boden ein wenig unregelmäßig mit Steinplatten, verschiedene
kleine Steinchen, wie man sich eine Höhle vorstellt"). Slot "Fels" wie der Baukasten (Material nach Weltposition), in Metern:

  SM_HK_Platte_A..D    Steinplatte, unregelmäßiges Vieleck 0,8 - 2,0 m, 12 - 22 cm dick, Kanten abgeschlagen, oben leicht gewellt;
                       Ursprung oben in der Mitte (wird ein paar Zentimeter versenkt gesetzt)
  SM_HK_Kiesel_A..C    lockere Gruppe kleiner runder Steine (3 - 14 cm), ~1,2 m Fläche, Ursprung auf dem Boden
  SM_HK_Geroell_A/B    Geröllhaufen aus faust- bis kopfgroßen Brocken, ~1,6 m, flach auslaufend
  SM_HK_Felsnase_A/B   Felsbuckel, der aus dem Boden ragt, ~1,6 x 1,1 m, 0,9 m hoch, zur Hälfte im Boden
  SM_HK_Pfuetze_A..C   Wasserfläche einer Pfütze, ~1,4 - 2,6 m, flache Schale: die Mitte 2 cm über 0, der Rand 6 cm darunter,
                       so schneidet der Boden ein unregelmäßiges Ufer heraus (Slot "Wasser")
Run:
  blender -b --factory-startup -P Tools/Welt/hoehlen_boden_extra.py -- C:/Projekte/Vael_Quellen/Hoehlen/export
"""
import bpy, bmesh, sys, os, math, random
from mathutils import Vector, noise

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Refugium"))
import glutkammer_probe as gk  # noqa: E402

FELS = ["Fels"]


def finish(obj, bm, recalc=True):
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.002)
    if recalc:
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(obj.data)
    bm.free()
    for p in obj.data.polygons:
        p.use_smooth = True


def outline(rnd, sides, radius, jag):
    """unregelmäßiges Vieleck: Ecken mit verschiedenen Abständen, dazwischen kleine Abbrüche"""
    pts = []
    base = rnd.uniform(0, 2 * math.pi)
    for s in range(sides):
        a = base + 2 * math.pi * (s + rnd.uniform(-0.3, 0.3)) / sides
        r = radius * rnd.uniform(0.7, 1.15)
        pts.append(Vector((math.cos(a) * r, math.sin(a) * r * rnd.uniform(0.75, 1.0), 0.0)))
    # Kanten ein- oder zweimal abgeschlagen
    out = []
    for i, p in enumerate(pts):
        q = pts[(i + 1) % len(pts)]
        out.append(p)
        for t in (0.33, 0.66):
            if rnd.random() < 0.6:
                m = p.lerp(q, t)
                inward = -m.normalized() * rnd.uniform(0.0, jag)
                out.append(m + inward)
    return out


def slab(name, seed, radius, thick):
    rnd = random.Random(seed)
    obj = gk.new_object(name, FELS)
    bm = bmesh.new()
    ring = outline(rnd, rnd.randint(5, 8), radius, radius * 0.12)
    rings = []
    # oben: Rand leicht gefast, Mitte leicht gewölbt; unten kleiner
    levels = [(0.0, 0.9), (-0.035, 1.0), (-thick * 0.6, 1.0), (-thick, 0.86)]
    for z, k in levels:
        row = []
        for p in ring:
            w = noise.noise(Vector((p.x * 2.0, p.y * 2.0, seed + z * 3)))
            row.append(bm.verts.new(Vector((p.x * k, p.y * k, z + 0.012 * w))))
        rings.append(row)
    n = len(ring)
    for a in range(len(rings) - 1):
        for i in range(n):
            bm.faces.new((rings[a][i], rings[a][(i + 1) % n], rings[a + 1][(i + 1) % n], rings[a + 1][i]))
    # Deckfläche: Fächer um einen leicht erhöhten Mittelpunkt mit Unterteilung für Wellen
    top_c = bm.verts.new(Vector((0, 0, 0.015)))
    mids = []
    for p in ring:
        q = p * 0.45
        mids.append(bm.verts.new(Vector((q.x, q.y, 0.012 + 0.01 * noise.noise(Vector((q.x * 3, q.y * 3, seed + 1)))))))
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((rings[0][j], rings[0][i], mids[i], mids[j]))
        bm.faces.new((mids[j], mids[i], top_c))
    bot = bm.verts.new(Vector((0, 0, -thick)))
    for i in range(n):
        bm.faces.new((rings[-1][i], rings[-1][(i + 1) % n], bot))
    finish(obj, bm)
    gk.export(obj)


def stone(bm, centre, size, rnd, flat=0.7, detail=3):
    """ein runder, abgeschliffener Stein"""
    ico = bmesh.ops.create_icosphere(bm, subdivisions=detail, radius=size)
    seed = rnd.uniform(0, 100)
    sx, sy = rnd.uniform(0.8, 1.25), rnd.uniform(0.8, 1.25)
    for v in ico["verts"]:
        d = v.co.normalized()
        bump = 1.0 + 0.22 * noise.noise(d * 1.6 + Vector((seed, 0, 0))) + 0.06 * noise.noise(d * 4.0 + Vector((0, seed, 0)))
        v.co = Vector((v.co.x * sx * bump, v.co.y * sy * bump, v.co.z * flat * bump)) + centre


def pebbles(name, seed, count, spread, smin, smax):
    rnd = random.Random(seed)
    obj = gk.new_object(name, FELS)
    bm = bmesh.new()
    for k in range(count):
        a = rnd.uniform(0, 2 * math.pi)
        r = spread * math.sqrt(rnd.random())
        s = rnd.uniform(smin, smax) * (1.3 - 0.6 * r / spread)
        stone(bm, Vector((math.cos(a) * r, math.sin(a) * r, s * 0.25)), s, rnd, flat=rnd.uniform(0.5, 0.8), detail=2 if s < 0.06 else 3)
    finish(obj, bm, recalc=False)
    gk.export(obj)


def rubble(name, seed):
    rnd = random.Random(seed)
    obj = gk.new_object(name, FELS)
    bm = bmesh.new()
    for k in range(34):
        a = rnd.uniform(0, 2 * math.pi)
        r = 0.8 * math.sqrt(rnd.random())
        s = rnd.uniform(0.05, 0.2) * (1.35 - r)
        h = (0.8 - r) * 0.35
        stone(bm, Vector((math.cos(a) * r, math.sin(a) * r * 0.8, h * rnd.uniform(0.3, 1.0))), s, rnd, flat=rnd.uniform(0.55, 0.9))
    finish(obj, bm, recalc=False)
    gk.export(obj)


def outcrop(name, seed):
    rnd = random.Random(seed)
    obj = gk.new_object(name, FELS)
    bm = bmesh.new()
    ico = bmesh.ops.create_icosphere(bm, subdivisions=4, radius=1.0)
    for v in ico["verts"]:
        d = v.co.normalized()
        p = d * 1.3 + Vector((seed, seed * 0.5, 0))
        bump = 1.0 + 0.28 * noise.noise(p) + 0.12 * noise.noise(p * 2.7) + 0.05 * noise.noise(p * 7.0)
        # Schichtung: flache Bänke wie beim Wandfels
        strata = 0.06 * math.sin(d.z * 9.0 + noise.noise(p * 0.5) * 2.0)
        v.co = Vector((d.x * 0.85, d.y * 0.6, d.z * 0.55 + 0.1)) * (bump + strata)
        if v.co.z < -0.35:
            v.co.z = -0.35
    finish(obj, bm)
    gk.export(obj)


def puddle(name, seed, radius):
    rnd = random.Random(seed)
    obj = gk.new_object(name, ["Wasser"])
    bm = bmesh.new()
    rings, sides = 6, 28
    centre = bm.verts.new(Vector((0, 0, 0.02)))
    rows = []
    shape = [radius * (1.0 + 0.32 * noise.noise(Vector((math.cos(2 * math.pi * s / sides) * 1.5, math.sin(2 * math.pi * s / sides) * 1.5, seed))))
             * rnd.uniform(0.92, 1.05) for s in range(sides)]
    for k in range(1, rings + 1):
        t = k / rings
        row = []
        for s in range(sides):
            a = 2 * math.pi * s / sides
            r = shape[s] * t
            z = 0.02 - 0.08 * t ** 2.2
            row.append(bm.verts.new(Vector((math.cos(a) * r, math.sin(a) * r * 0.8, z))))
        rows.append(row)
    for s in range(sides):
        bm.faces.new((centre, rows[0][s], rows[0][(s + 1) % sides]))
    for k in range(rings - 1):
        for s in range(sides):
            bm.faces.new((rows[k][s], rows[k + 1][s], rows[k + 1][(s + 1) % sides], rows[k][(s + 1) % sides]))
    finish(obj, bm, recalc=False)
    for p in obj.data.polygons:
        if p.normal.z < 0:
            p.flip()
    gk.export(obj)


if __name__ == "__main__":
    builds = [lambda: slab("SM_HK_Platte_A", 101, 0.55, 0.14), lambda: slab("SM_HK_Platte_B", 102, 0.8, 0.18),
              lambda: slab("SM_HK_Platte_C", 103, 1.0, 0.22), lambda: slab("SM_HK_Platte_D", 104, 0.42, 0.12),
              lambda: pebbles("SM_HK_Kiesel_A", 111, 14, 0.6, 0.03, 0.09), lambda: pebbles("SM_HK_Kiesel_B", 112, 22, 0.75, 0.025, 0.07),
              lambda: pebbles("SM_HK_Kiesel_C", 113, 8, 0.45, 0.05, 0.14),
              lambda: rubble("SM_HK_Geroell_A", 121), lambda: rubble("SM_HK_Geroell_B", 122),
              lambda: outcrop("SM_HK_Felsnase_A", 131), lambda: outcrop("SM_HK_Felsnase_B", 132),
              lambda: puddle("SM_HK_Pfuetze_A", 141, 0.75), lambda: puddle("SM_HK_Pfuetze_B", 142, 1.1), lambda: puddle("SM_HK_Pfuetze_C", 143, 1.35)]
    for build in builds:
        gk.clear_scene()
        build()
    gk.log("Bodenteile fertig")
