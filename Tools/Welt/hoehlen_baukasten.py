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


if __name__ == "__main__":
    random.seed(15)
    builds = [lambda: wall("SM_HK_Wand_A", 1), lambda: wall("SM_HK_Wand_B", 2), lambda: wall("SM_HK_Wand_C", 3),
              lambda: wall("SM_HK_Wand_Niedrig_A", 4, h=1.4, nz=10, crown=0.25), lambda: wall("SM_HK_Wand_Niedrig_B", 5, h=1.4, nz=10, crown=0.25),
              lambda: pillar("SM_HK_Ecke_A", 6), lambda: pillar("SM_HK_Ecke_B", 7), lambda: pillar("SM_HK_Ecke_Niedrig", 8, h=1.6),
              lambda: floor("SM_HK_Boden_A", 9), lambda: floor("SM_HK_Boden_B", 10),
              lambda: support("SM_HK_Verbau", 3.4), lambda: support("SM_HK_Verbau_8", 7.4), lambda: support("SM_HK_Verbau_12", 11.2),
              rails, cart, crate, beams, lantern]
    for build in builds:
        gk.clear_scene()
        build()
    gk.log("Hoehlenbaukasten fertig")
