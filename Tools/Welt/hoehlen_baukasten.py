"""
Der Höhlenbaukasten des Eisenschlunds (Boris 15.11.2026: "Davon gibt es viele und wir brauchen verschiedene").
Alle Höhlen bauen sich auf einem 4-m-Raster aus diesen Teilen; jede Höhlenart gibt ihnen eigenes Material, Licht und Requisiten
(Scripts/BuildHoehle.py, Grundrisse in Scripts/Data/Hoehlen/*.json).

Fels (ein Slot "Fels", ohne UVs: das Material legt den Fels nach Weltposition), in Metern, Ursprung wie angegeben:
  SM_HK_Wand_A/B/C        Felswand für eine Rasterkante: 4,6 m breit (überlappt), 7 m hoch, Vorderseite -Y zur Höhle,
                          Ursprung unten in der Mitte der Kante; rau, mit Klüften, oben eine gezackte Krone
  SM_HK_Wand_Niedrig_A/B  dieselbe Kante auf der Kameraseite: 1,4 m hoch, damit sie die Figur nicht verdeckt
  SM_HK_Ecke_A/B          Felspfeiler für die Ecken, 2,6 m dick, 7,5 m hoch, Ursprung unten in der Mitte
  SM_HK_Ecke_Niedrig      derselbe, 1,6 m hoch
  SM_HK_Boden_A/B         Bodenplatte 4,2 x 4,2 m, oben bei 0 mit flachen Buckeln, Ursprung in der Mitte
Gildenmine (Slots des Glutkammer-Kits: Holz, Leder, Metall, Faden, Glut, Moos, Messing):
  SM_HK_Verbau / _8 / _12 Türstock aus Holz quer über einen Stollen von 1, 2 oder 3 Zellen (3,4 / 7,4 / 11,2 m): zwei Stempel,
                          eine Kappe, Streben; entlang Y, Ursprung unten in der Mitte
  SM_HK_Schiene           4 m Grubengleis entlang +X: zwei Schienen auf fünf Schwellen
  SM_HK_Lore              eine Lore (Förderwagen), entlang +X, auf dem Gleis
  SM_HK_Kiste             Kiste der Gilde, 1 m
  SM_HK_Balkenstapel      gestapelte Grubenhölzer, 3 m lang
  SM_HK_Laterne           Grubenlaterne an einem Pfahl, 2,6 m; die Glut sitzt oben
Olmgrotte (Slots Kristall und Fels):
  SM_HK_Salzspat          Salzspat-Gruppe, die als Lampe leuchtet, ~2 m
  SM_HK_Salzspat_Klein    kleine Gruppe, Erzstelle
Gebeinkammer (Slots Knochen, Dunkel, Schleim):
  SM_HK_Schaedelwand_A/B  Schädelreihen und Langknochen vor dem Fels, 4,4 x 3,2 m, Vorderseite -Y
  SM_HK_Knochenhaufen     Haufen Gebeine mit Schädeln, ~2 m
  SM_HK_Schleim           Lache Grabschleim mit Ausläufern, ~3 m, leuchtet (auch Glut- und Giftlachen anderer Höhlen)
Rostschwamm-Höhle, Tiefster Schacht:
  SM_HK_Schwamm           Rostschwamm-Wuchs, klumpig und löchrig, ~1,6 m (Slot Fels)
  SM_HK_Kerzen            Kerzen der Totenwache, ~0,8 m (Slots Knochen = Wachs, Glut = Flamme)
  SM_HK_Rippe             Rippenbogen der Urdrachin, 16 m weit, 9 m hoch, entlang X (Slot Knochen)
  SM_HK_Tropfstein_A/B/C  Tropfstein, 1 m hoch (Slot Fels); die Stalagmiten der Schluchten ließen in einem Innenlevel nichts mehr laden

Run:
  blender -b --factory-startup -P Tools/Welt/hoehlen_baukasten.py -- C:/Projekte/Vael_Quellen/Hoehlen/export
"""
import bpy, bmesh, sys, os, math, random
from mathutils import Vector, noise, Matrix

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Refugium"))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Ingenieur"))
import glutkammer_probe as gk  # noqa: E402
from vael_ingenieur_werkstuecke import lathe, disc, box, HOLZ, LEDER, METALL, GLUT, SLOTS as KIT  # noqa: E402

FELS = ["Fels"]


def rock_finish(obj, bm, recalc=True):
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.005)
    if recalc:
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(obj.data)
    bm.free()
    for p in obj.data.polygons:
        p.use_smooth = True


def wall(name, seed, w=4.6, h=7.0, d=1.6, nx=40, nz=56, crown=0.7):
    """Felswand für eine Kante: Gitter vorn (y <= 0), die Krone nach hinten, hinten und an den Seiten geschlossen"""
    obj = gk.new_object(name, FELS)
    bm = bmesh.new()

    def top(x):
        return h + crown * noise.noise(Vector((x * 0.5, seed * 1.3, 3.0))) + 0.25 * noise.noise(Vector((x * 2.0, seed, 7.0)))

    grid = []
    for k in range(nz + 1):
        row = []
        for i in range(nx + 1):
            x = -w / 2 + w * i / nx
            z = top(x) * k / nz
            p = Vector((x * 0.6, z * 0.6, seed * 3.1))
            strata = 0.2 * math.sin(z * 3.2 + noise.noise(Vector((x * 0.4, 0.0, seed))) * 2.5)
            ridges = 0.32 * math.sin(x * 2.6 + noise.noise(Vector((z * 0.35, seed, 2.0))) * 3.0) * (0.5 + 0.5 * noise.noise(Vector((x, z * 0.3, seed + 2))))
            crack = -0.8 * max(0.0, 1.0 - abs(noise.noise(Vector((x * 0.35, z * 0.15, seed + 9.0)))) * 6.0)
            ledge = 0.3 * max(0.0, noise.noise(Vector((x * 0.25, z * 0.9, seed + 5.0))))
            bulge = 0.6 * noise.noise(p * 0.7)
            grain = 0.1 * noise.noise(p * 4.0) + 0.04 * noise.noise(p * 11.0)
            foot = 0.35 * (1.0 - min(1.0, z / 1.2)) ** 2                      # Schutt am Fuß
            y = -(strata + ridges + crack + ledge + bulge + grain + foot) - 0.15 + 0.25 * (z / h)   # nach oben leicht zurück
            row.append(bm.verts.new((x, y, z)))
        grid.append(row)
    for j in range(1, 4):
        t = j / 3
        row = []
        for i in range(nx + 1):
            x = -w / 2 + w * i / nx
            front = grid[nz][i].co
            row.append(bm.verts.new((x, front.y + (d - front.y) * t, top(x) + 0.2 * noise.noise(Vector((x, t * 3, seed + 4))) - 0.2 * t)))
        grid.append(row)
    rows = len(grid) - 1
    for k in range(rows):
        for i in range(nx):
            bm.faces.new((grid[k][i], grid[k][i + 1], grid[k + 1][i + 1], grid[k + 1][i]))
    b0, b1 = bm.verts.new((-w / 2, d, -0.3)), bm.verts.new((w / 2, d, -0.3))
    bm.faces.new([b0, b1] + [grid[rows][i] for i in range(nx, -1, -1)])
    bm.faces.new([b0] + [grid[k][0] for k in range(rows, -1, -1)])
    bm.faces.new([b1] + [grid[k][nx] for k in range(rows + 1)])
    bm.faces.new([b0] + [grid[0][i] for i in range(nx + 1)] + [b1])
    rock_finish(obj, bm)
    gk.export(obj)


def pillar(name, seed, h=7.5, r=1.3):
    """Felspfeiler: unregelmäßiger, geschichteter Block, oben gezackt"""
    obj = gk.new_object(name, FELS)
    bm = bmesh.new()
    rings, sides = 16, 14
    rows = []
    for k in range(rings + 1):
        z = h * k / rings
        row = []
        for s in range(sides):
            a = 2 * math.pi * s / sides
            n = Vector((math.cos(a), math.sin(a), 0))
            rad = r * (1.0 + 0.25 * noise.noise(n * 1.5 + Vector((0, 0, z * 0.3 + seed)))) * (1.15 - 0.15 * k / rings)
            zz = z if k < rings else z + 0.6 * noise.noise(n * 2 + Vector((seed, 0, 0)))
            row.append(bm.verts.new((n.x * rad, n.y * rad, zz)))
        rows.append(row)
    for k in range(rings):
        for s in range(sides):
            bm.faces.new((rows[k][s], rows[k][(s + 1) % sides], rows[k + 1][(s + 1) % sides], rows[k + 1][s]))
    bm.faces.new(rows[-1])
    bm.faces.new(rows[0][::-1])
    rock_finish(obj, bm)
    gk.export(obj)


def floor(name, seed, size=4.2, n=14):
    """Bodenplatte: oben bei 0 mit flachen Buckeln und Rinnen, 0,6 m dick"""
    obj = gk.new_object(name, FELS)
    bm = bmesh.new()
    top = []
    for j in range(n + 1):
        row = []
        for i in range(n + 1):
            x, y = -size / 2 + size * i / n, -size / 2 + size * j / n
            edge = min(i, n - i, j, n - j) / 2.0
            z = (0.07 * noise.noise(Vector((x * 0.7, y * 0.7, seed))) + 0.025 * noise.noise(Vector((x * 3, y * 3, seed + 2)))) * min(1.0, edge)
            row.append(bm.verts.new((x, y, z)))
        top.append(row)
    for j in range(n):
        for i in range(n):
            bm.faces.new((top[j][i], top[j][i + 1], top[j + 1][i + 1], top[j + 1][i]))
    # eine offene Platte: das Umdrehen der Normalen raten lassen kehrte sie manchmal nach unten (man fiel hindurch)
    rock_finish(obj, bm, recalc=False)
    gk.export(obj)


# ---------------------------------------------------------------- Gildenmine

def timber(b, a, c, thick, seed=0):
    """Ein Grubenholz von a nach c: vierkantig mit gebrochenen Kanten"""
    b.tube([a, (a + c) / 2, c], [thick, thick * 1.02, thick], HOLZ, sides=6, rough=0.12, seed=seed)


def support(name, span):
    """Türstock quer über einen Stollen der Breite span (m): Stempel an beiden Wänden, eine Kappe darüber, Streben in den Ecken"""
    obj = gk.new_object(name, KIT)
    b = gk.Builder()
    half = span / 2
    height = 4.3 + 0.12 * span
    for side in (-1, 1):
        timber(b, Vector((0, side * half, 0)), Vector((0, side * (half - 0.15), height)), 0.18, seed=side)        # Stempel, leicht nach innen
        timber(b, Vector((0, side * (half - 0.25), height - 0.9)), Vector((0, side * (half - 1.0), height)), 0.09, seed=side + 4)  # Strebe
        box(b, Vector((0, side * (half - 0.15), height - 0.1)), (0.28, 0.28, 0.06), METALL)                        # Eisenschuh
    timber(b, Vector((0, -half - 0.25, height + 0.12)), Vector((0, half + 0.25, height + 0.12 + 0.05 * span * 0.1)), 0.2, seed=3)  # Kappe
    b.finish(obj)
    gk.export(obj)


def rails():
    obj = gk.new_object("SM_HK_Schiene", KIT)
    b = gk.Builder()
    for k in range(5):
        box(b, Vector((-1.6 + k * 0.8, 0, 0.05)), (0.22, 1.3, 0.1), HOLZ, yaw=random.uniform(-0.05, 0.05))
    for side in (-1, 1):
        box(b, Vector((0, side * 0.45, 0.14)), (4.0, 0.07, 0.08), METALL)
    b.finish(obj)
    gk.export(obj)


def cart():
    obj = gk.new_object("SM_HK_Lore", KIT)
    b = gk.Builder()
    box(b, Vector((0, 0, 0.75)), (1.5, 1.0, 0.7), HOLZ)
    for x in (-0.76, 0.76):
        box(b, Vector((x, 0, 0.75)), (0.05, 1.04, 0.74), METALL)
    for y in (-0.51, 0.51):
        box(b, Vector((0, y, 1.08)), (1.56, 0.05, 0.06), METALL)
    turn = Matrix.Rotation(math.radians(90), 3, "X")
    for x in (-0.5, 0.5):
        for y in (-0.45, 0.45):
            lathe(b, [(0.0, -0.04), (0.2, -0.04), (0.2, 0.04), (0.0, 0.04)], METALL, Vector((x, y, 0.24)), sides=12, axis=turn)
    # ein paar Brocken Erz in der Lore
    for k in range(6):
        box(b, Vector((random.uniform(-0.5, 0.5), random.uniform(-0.3, 0.3), 1.1)), (0.3, 0.25, 0.2), METALL,
            yaw=random.uniform(0, 3), pitch=random.uniform(-0.4, 0.4))
    b.finish(obj)
    gk.export(obj)


def crate():
    obj = gk.new_object("SM_HK_Kiste", KIT)
    b = gk.Builder()
    box(b, Vector((0, 0, 0.5)), (1.0, 1.0, 1.0), HOLZ)
    for z in (0.08, 0.92):
        for y in (-0.51, 0.51):
            box(b, Vector((0, y, z)), (1.02, 0.03, 0.08), METALL)
    b.finish(obj)
    gk.export(obj)


def beams():
    obj = gk.new_object("SM_HK_Balkenstapel", KIT)
    b = gk.Builder()
    for layer in range(3):
        for k in range(4 - layer):
            y = -0.45 + 0.3 * k + 0.15 * layer
            timber(b, Vector((-1.5, y, 0.14 + layer * 0.26)), Vector((1.5 + random.uniform(-0.2, 0.2), y, 0.14 + layer * 0.26)), 0.13, seed=layer * 5 + k)
    b.finish(obj)
    gk.export(obj)


def lantern():
    obj = gk.new_object("SM_HK_Laterne", KIT)
    b = gk.Builder()
    timber(b, Vector((0, 0, 0)), Vector((0, 0, 2.5)), 0.08)
    timber(b, Vector((0, 0, 2.45)), Vector((0.45, 0, 2.5)), 0.04, seed=2)
    lathe(b, [(0.0, -0.32), (0.11, -0.3), (0.13, -0.12), (0.11, 0.0), (0.05, 0.06), (0.0, 0.08)], METALL, Vector((0.45, 0, 2.2)), sides=8)
    lathe(b, [(0.0, -0.28), (0.09, -0.26), (0.1, -0.12), (0.0, -0.04)], GLUT, Vector((0.45, 0, 2.2)), sides=8, glow=1.0)
    b.finish(obj)
    gk.export(obj)


# ---------------------------------------------------------------- Olmgrotte

def stalagmite(name, seed):
    """Tropfstein, 1 m hoch (beim Setzen skaliert): geringelter Kegel mit Wülsten, oben stumpf-spitz; Slot Fels"""
    rnd = random.Random(seed)
    obj = gk.new_object(name, FELS)
    bm = bmesh.new()
    rings, sides = 14, 10
    base = rnd.uniform(0.17, 0.24)
    rows = []
    for k in range(rings + 1):
        t = k / rings
        radius = base * (1.0 - t) ** rnd.uniform(0.9, 1.2) + 0.006
        radius *= 1.0 + 0.12 * math.sin(t * rnd.uniform(18, 26) + seed)
        lean = Vector((math.sin(seed) * 0.06 * t, math.cos(seed * 1.3) * 0.06 * t, 0.0))
        row = []
        for sd in range(sides):
            a = 2 * math.pi * sd / sides
            wob = 1.0 + 0.18 * noise.noise(Vector((math.cos(a) * 2.0, math.sin(a) * 2.0, t * 4.0 + seed)))
            row.append(bm.verts.new(Vector((math.cos(a) * radius * wob, math.sin(a) * radius * wob, t)) + lean))
        rows.append(row)
    for k in range(rings):
        for sd in range(sides):
            bm.faces.new((rows[k][sd], rows[k][(sd + 1) % sides], rows[k + 1][(sd + 1) % sides], rows[k + 1][sd]))
    bm.faces.new(rows[-1])
    bm.faces.new(rows[0][::-1])
    rock_finish(obj, bm)
    gk.export(obj)


def crystals(name, seed, size=1.0, count=9):
    """Salzspat: eine Gruppe sechseckiger Säulen aus einem Felsfuß, schräg auseinander, oben spitz; Slot Kristall (leuchtet)
    und Fels (der Fuß)"""
    obj = gk.new_object(name, ["Kristall", "Fels"])
    b = gk.Builder()
    rnd = random.Random(seed)
    lathe(b, [(0.0, 0.0), (0.42 * size, 0.0), (0.36 * size, 0.12 * size), (0.18 * size, 0.2 * size), (0.0, 0.22 * size)], 1,
          Vector((0, 0, -0.05)), sides=9)
    for k in range(count):
        length = size * rnd.uniform(0.45, 1.5) * (1.0 if k else 1.4)
        radius = size * rnd.uniform(0.05, 0.11) * (1.0 if k else 1.5)
        lean = Vector((rnd.uniform(-0.55, 0.55), rnd.uniform(-0.55, 0.55), 1.0)).normalized() if k else Vector((0.05, 0.02, 1.0)).normalized()
        base = Vector((rnd.uniform(-0.2, 0.2) * size, rnd.uniform(-0.2, 0.2) * size, 0.05 * size))
        turn = Vector((0, 0, 1)).rotation_difference(lean).to_matrix()
        lathe(b, [(radius, 0.0), (radius, length * 0.78), (radius * 0.35, length * 0.95), (0.0, length)], 0, base, sides=6, axis=turn)
    b.finish(obj)
    gk.export(obj)


# ---------------------------------------------------------------- Gebeinkammer

def skull(b, at, facing, size, rnd):
    """Ein Schädel aus groben Formen: Hirnschale, zwei dunkle Augenhöhlen, Kiefer; schaut in Richtung facing"""
    f = facing.normalized()
    side = Vector((0, 0, 1)).cross(f).normalized()
    up = f.cross(side).normalized()
    turn = Matrix((side, up, f)).transposed()
    lathe(b, [(0.0, -0.5), (0.42, -0.42), (0.5, -0.05), (0.45, 0.25), (0.3, 0.45), (0.0, 0.5)], 0, at, sides=9,
          axis=turn @ Matrix.Diagonal((size, size, size * 0.9)))
    for sx in (-0.2, 0.2):
        disc(b, at + f * size * 0.47 + side * sx * size + up * 0.05 * size, 0.13 * size, f, 1, sides=6)
    box(b, at + f * size * 0.32 - up * size * 0.42, (size * 0.5, size * 0.3, size * 0.22), 0)


def skull_wall(name, seed):
    """Eine Wand aus geordneten Schädelreihen vor dem Fels (Katakomben): 4,4 m breit, 3,2 m hoch, Vorderseite -Y; dazwischen
    gestapelte Langknochen; Slots Knochen und Dunkel"""
    obj = gk.new_object(name, ["Knochen", "Dunkel"])
    b = gk.Builder()
    rnd = random.Random(seed)
    for row in range(6):
        z = 0.3 + row * 0.52
        # auf beiden Seiten: so zeigt die Wand ihre Schädel, wie herum sie auch im Gang steht
        for side, y0 in ((-1.0, -0.2), (1.0, 0.5)):
            if row % 3 == 2:
                # eine Lage Langknochen quer
                y = y0 * 0.6 + rnd.uniform(-0.03, 0.03)
                zz = z + rnd.uniform(-0.05, 0.05)
                b.tube([Vector((-2.2, y, zz)), Vector((2.2, y, zz + rnd.uniform(-0.04, 0.04)))], [0.09, 0.08], 0, sides=6)
                b.tube([Vector((-2.2, y, zz + 0.16)), Vector((2.2, y, zz + 0.16))], [0.09, 0.08], 0, sides=6)
                continue
            for k in range(10):
                x = -1.98 + k * 0.44 + rnd.uniform(-0.02, 0.02)
                skull(b, Vector((x, y0, z)), Vector((rnd.uniform(-0.12, 0.12), side, rnd.uniform(-0.1, 0.1))), rnd.uniform(0.38, 0.42), rnd)
    box(b, Vector((0, 0.15, 1.6)), (4.4, 0.3, 3.2), 1)
    b.finish(obj)
    gk.export(obj)


def bone_pile(name, seed):
    """Ein Haufen Gebeine: Langknochen kreuz und quer, ein paar Schädel obenauf; Slots Knochen und Dunkel"""
    obj = gk.new_object(name, ["Knochen", "Dunkel"])
    b = gk.Builder()
    rnd = random.Random(seed)
    for k in range(34):
        a = rnd.uniform(0, 2 * math.pi)
        r = rnd.uniform(0, 0.9) ** 0.7
        c = Vector((math.cos(a) * r, math.sin(a) * r, 0.06 + (0.95 - r) * 0.4 * rnd.random()))
        # kreuz und quer, nicht sternförmig um die Mitte (sah nach Spinne aus)
        b_ = rnd.uniform(0, math.pi)
        d = Vector((math.cos(b_), math.sin(b_), rnd.uniform(-0.15, 0.15))).normalized() * rnd.uniform(0.18, 0.42)
        b.tube([c - d, c + d], [0.05, 0.05], 0, sides=6)
        for e in (c - d, c + d):
            lathe(b, [(0.0, -0.07), (0.07, -0.04), (0.07, 0.04), (0.0, 0.07)], 0, e, sides=6)
    for k in range(5):
        a = rnd.uniform(0, 2 * math.pi)
        r = rnd.uniform(0, 0.55)
        skull(b, Vector((math.cos(a) * r, math.sin(a) * r, 0.5 - r * 0.4)), Vector((math.cos(a), math.sin(a), 0.3)), rnd.uniform(0.34, 0.4), rnd)
    b.finish(obj)
    gk.export(obj)


def slime(name, seed):
    """Eine Lache Grabschleim: flacher, zähflüssiger Fleck mit Rinnsalen, 3 m; Slot Schleim (leuchtet in seiner Farbe)"""
    obj = gk.new_object(name, ["Schleim"])
    b = gk.Builder()
    rnd = random.Random(seed)
    rows = []
    for k in range(6):
        t = k / 5
        ring = []
        for sd in range(20):
            a = 2 * math.pi * sd / 20
            rad = 1.4 * (1.0 - t) * (1.0 + 0.35 * noise.noise(Vector((math.cos(a) * 1.5, math.sin(a) * 1.5, seed))))
            ring.append(Vector((math.cos(a) * rad, math.sin(a) * rad, 0.02 + 0.06 * math.sin(t * math.pi / 2))))
        rows.append(ring)
    b.grid(rows, 0, uv_of=lambda j, i: (i / 20, j / 5), mask_of=lambda j, i: 1.0, closed_u=True)
    # flache Ausläufer: kleinere Lachen am Rand
    for k in range(5):
        a = rnd.uniform(0, 2 * math.pi)
        c = Vector((math.cos(a) * rnd.uniform(1.3, 2.0), math.sin(a) * rnd.uniform(1.3, 2.0), 0.0))
        r = rnd.uniform(0.25, 0.5)
        ring = [c + Vector((math.cos(2 * math.pi * i / 10) * r, math.sin(2 * math.pi * i / 10) * r, 0.02)) for i in range(10)]
        b.grid([ring, [c + Vector((0, 0, 0.05))] * 10], 0, uv_of=lambda j, i: (i / 10, j), mask_of=lambda j, i: 1.0, closed_u=True)
    b.finish(obj)
    gk.export(obj)


# ---------------------------------------------------------------- Rostschwamm-Höhle, Tiefster Schacht

def sponge(name, seed):
    """Rostschwamm: ein klumpiger, löchriger Wuchs aus mehreren Knollen, ~1,6 m; Slot Fels (bekommt das Schwamm-Material)"""
    obj = gk.new_object(name, FELS)
    b = gk.Builder()
    rnd = random.Random(seed)
    for k in range(rnd.randint(5, 8)):
        c = Vector((rnd.uniform(-0.6, 0.6), rnd.uniform(-0.6, 0.6), 0.0))
        h = rnd.uniform(0.5, 1.6)
        r = rnd.uniform(0.18, 0.4)
        profile = [(0.0, 0.0), (r * 0.8, 0.0), (r, h * 0.3), (r * 1.25, h * 0.75), (r * 1.1, h * 0.95), (r * 0.5, h), (0.0, h * 0.92)]
        lathe(b, profile, 0, c, sides=10)
        for j in range(3):
            a = rnd.uniform(0, 2 * math.pi)
            z = rnd.uniform(0.3, 0.9) * h
            disc(b, c + Vector((math.cos(a) * r * 1.15, math.sin(a) * r * 1.15, z)), r * 0.25, Vector((math.cos(a), math.sin(a), 0.2)), 0, sides=6)
    b.finish(obj)
    gk.export(obj)


def rib(name, seed):
    """Eine Rippe der Urdrachin: ein Knochenbogen, 16 m weit, 9 m hoch, flach und rau; Slot Knochen"""
    obj = gk.new_object(name, ["Knochen"])
    b = gk.Builder()
    pts, radii = [], []
    for k in range(13):
        t = k / 12
        a = math.pi * t
        pts.append(Vector((-8.0 * math.cos(a), 0.0, 9.0 * math.sin(a) ** 0.8)))
        radii.append(0.55 + 0.35 * (1.0 - math.sin(a)))
    b.tube(pts, radii, 0, sides=8, flat=0.6, rough=0.15, seed=seed)
    b.finish(obj)
    gk.export(obj)


def candles(name, seed):
    """Kerzen der Totenwache: ein Haufen Wachsstümpfe verschiedener Höhe, zerlaufen, mit kleinen Flammen; Slots Knochen (Wachs)
    und Glut (Flamme)"""
    obj = gk.new_object(name, ["Knochen", "Glut"])
    b = gk.Builder()
    rnd = random.Random(seed)
    lathe(b, [(0.0, 0.0), (0.38, 0.0), (0.34, 0.04), (0.0, 0.05)], 0, Vector((0, 0, 0)), sides=12)          # zerlaufenes Wachs
    for k in range(rnd.randint(6, 9)):
        a = rnd.uniform(0, 2 * math.pi)
        r = rnd.uniform(0.0, 0.28)
        c = Vector((math.cos(a) * r, math.sin(a) * r, 0.03))
        h = rnd.uniform(0.08, 0.45)
        rad = rnd.uniform(0.025, 0.05)
        lathe(b, [(rad * 1.3, 0.0), (rad, 0.02), (rad, h), (rad * 0.6, h + 0.01), (0.0, h + 0.012)], 0, c, sides=8)
        lathe(b, [(0.0, 0.0), (0.012, 0.015), (0.0, 0.05)], 1, c + Vector((0, 0, h + 0.01)), sides=5, glow=1.0)
    b.finish(obj)
    gk.export(obj)


if __name__ == "__main__":
    random.seed(15)
    builds = [lambda: wall("SM_HK_Wand_A", 1), lambda: wall("SM_HK_Wand_B", 2), lambda: wall("SM_HK_Wand_C", 3),
              lambda: wall("SM_HK_Wand_Niedrig_A", 4, h=1.4, nz=10, crown=0.25), lambda: wall("SM_HK_Wand_Niedrig_B", 5, h=1.4, nz=10, crown=0.25),
              lambda: pillar("SM_HK_Ecke_A", 6), lambda: pillar("SM_HK_Ecke_B", 7), lambda: pillar("SM_HK_Ecke_Niedrig", 8, h=1.6),
              lambda: floor("SM_HK_Boden_A", 9), lambda: floor("SM_HK_Boden_B", 10),
              lambda: support("SM_HK_Verbau", 3.4), lambda: support("SM_HK_Verbau_8", 7.4), lambda: support("SM_HK_Verbau_12", 11.2),
              rails, cart, crate, beams, lantern,
              lambda: crystals("SM_HK_Salzspat", 21, 1.6, 11), lambda: crystals("SM_HK_Salzspat_Klein", 22, 0.7, 6),
              lambda: stalagmite("SM_HK_Tropfstein_A", 31), lambda: stalagmite("SM_HK_Tropfstein_B", 32), lambda: stalagmite("SM_HK_Tropfstein_C", 33),
              lambda: skull_wall("SM_HK_Schaedelwand_A", 41), lambda: skull_wall("SM_HK_Schaedelwand_B", 42),
              lambda: bone_pile("SM_HK_Knochenhaufen", 43), lambda: slime("SM_HK_Schleim", 44),
              lambda: sponge("SM_HK_Schwamm", 51), lambda: rib("SM_HK_Rippe", 52), lambda: candles("SM_HK_Kerzen", 53)]
    for build in builds:
        gk.clear_scene()
        build()
    gk.log("Hoehlenbaukasten fertig")
