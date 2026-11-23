"""Buildings of the Eisenschlund world instead of the grey blockout shapes (Boris 2026-11-23: finish the Eisenschlund).
Every piece fills a 1 m box centred on its origin (-0.5 .. 0.5 on every axis), like the engine shapes it replaces, so
Scripts/EisenschlundBauten.py only swaps the mesh and keeps place, turn and size of each placeholder.

  SM_ES_Haus_A / _B   guild house: stone ground floor, half-timbered upper floor, pitched roof, chimney (box 12 x 9 x 8 m)
  SM_ES_Turm          round stone tower, string courses, arrow slits, corbelled top (cylinder 6 x 6 x 18 m)
  SM_ES_Turmdach      conical roof with flared eaves and an iron finial (cone 7 x 7 x 5 m)
  SM_ES_Halle         smelting hall: stone walls, buttresses, gable roof, glowing forge mouth (box 14 x 10 x 8 m)
  SM_ES_Schlot        tapered chimney with iron bands and a glowing rim (cylinder 2.5 x 2.5 x 18 m)
  SM_ES_Grubenpfosten timber post of the headframe with iron brackets (box 0.8 x 0.8 x 26 m)
  SM_ES_Plattform     plank deck with beams and a railing (box 10 x 10 x 1 m)
  SM_ES_Seilrad       spoked rope wheel, axis along Z (cylinder 6 x 6 x 0.6 m)
  SM_ES_Wachturm      square stone tower with a wooden lookout and a roof (box 6 x 6 x 16 m)
  SM_ES_Feuerstelle   ring of stones, crossed logs, embers (cylinder 1.6 x 1.6 x 0.4 m)
  SM_ES_Zelt          six-sided tent with poles through the top and a door flap (cone 4 x 4 x 3 m)
  SM_ES_NidSiegel     sealed Nid spring: stone ring, iron grate, violet glow under it (cylinder 5 x 5 x 0.6 m)
  SM_ES_Bohrer        probing drill: pipe, gear housing, clamps (cylinder 1.2 x 1.2 x 9 m)

Slots (material names): Stein, Holz, Dach, Metall, Gilde, Glut, Zelt, Nid, Dunkel.

    blender -b --factory-startup -P Tools/Welt/eisenschlund_bauten.py -- C:/Projekte/Vael_Quellen/Hoehlen/export
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

OUT = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else "C:/Projekte/Vael_Quellen/Hoehlen/export"
SLOTS = ["Stein", "Holz", "Dach", "Metall", "Gilde", "Glut", "Zelt", "Nid", "Dunkel"]


class Bau:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()

    def _fertig(self, geom, slot):
        for f in [g for g in geom if isinstance(g, bmesh.types.BMFace)]:
            f.material_index = SLOTS.index(slot)
            f.smooth = False

    def kiste(self, mitte, groesse, slot, dreh=0.0, kipp=(0.0, 0.0)):
        """A box: centre, size (x, y, z), turned about Z (degrees), tilted about X and Y"""
        r = bmesh.ops.create_cube(self.bm, size=1.0)
        m = (Matrix.Translation(Vector(mitte)) @ Matrix.Rotation(math.radians(dreh), 4, "Z") @ Matrix.Rotation(math.radians(kipp[0]), 4, "X")
             @ Matrix.Rotation(math.radians(kipp[1]), 4, "Y") @ Matrix.Diagonal(Vector((*groesse, 1.0))))
        bmesh.ops.transform(self.bm, matrix=m, verts=r["verts"])
        self._fertig(set(f for v in r["verts"] for f in v.link_faces), slot)

    def balken(self, a, b, dicke, slot):
        """A beam from a to b with a square section"""
        a, b = Vector(a), Vector(b)
        d = b - a
        r = bmesh.ops.create_cube(self.bm, size=1.0)
        q = Vector((0, 0, 1)).rotation_difference(d.normalized())
        m = Matrix.Translation((a + b) / 2) @ q.to_matrix().to_4x4() @ Matrix.Diagonal(Vector((dicke, dicke, d.length, 1.0)))
        bmesh.ops.transform(self.bm, matrix=m, verts=r["verts"])
        self._fertig(set(f for v in r["verts"] for f in v.link_faces), slot)

    def drehkoerper(self, profil, slot, mitte=(0, 0, 0), seiten=16, deckel=True, glatt=True):
        """A lathe: profil = [(radius, z), ...] from bottom to top"""
        mitte = Vector(mitte)
        ringe = []
        for r, z in profil:
            ringe.append([self.bm.verts.new(mitte + Vector((math.cos(2 * math.pi * k / seiten) * r, math.sin(2 * math.pi * k / seiten) * r, z)))
                          for k in range(seiten)])
        fl = []
        for i in range(len(ringe) - 1):
            for k in range(seiten):
                n = (k + 1) % seiten
                fl.append(self.bm.faces.new((ringe[i][k], ringe[i][n], ringe[i + 1][n], ringe[i + 1][k])))
        if deckel:
            fl.append(self.bm.faces.new(ringe[0][::-1]))
            fl.append(self.bm.faces.new(ringe[-1]))
        self._fertig(fl, slot)
        for f in fl:
            f.smooth = glatt

    def zylinder(self, mitte, r, h, slot, seiten=12):
        self.drehkoerper([(r, 0.0), (r, h)], slot, mitte, seiten, glatt=True)

    def satteldach(self, mitte, laenge, breite, hoehe, slot, ueberstand=0.3):
        """A gable roof along X, its eaves at mitte.z"""
        x0, x1 = mitte[0] - laenge / 2 - ueberstand, mitte[0] + laenge / 2 + ueberstand
        y0, y1 = mitte[1] - breite / 2 - ueberstand, mitte[1] + breite / 2 + ueberstand
        z0, z1 = mitte[2], mitte[2] + hoehe
        d = 0.18
        v = [self.bm.verts.new(p) for p in (
            (x0, y0, z0), (x1, y0, z0), (x1, mitte[1], z1), (x0, mitte[1], z1), (x0, y1, z0), (x1, y1, z0),
            (x0, y0, z0 + d), (x1, y0, z0 + d), (x1, mitte[1], z1 + d), (x0, mitte[1], z1 + d), (x0, y1, z0 + d), (x1, y1, z0 + d))]
        fl = [self.bm.faces.new((v[6], v[7], v[8], v[9])), self.bm.faces.new((v[9], v[8], v[11], v[10])),
              self.bm.faces.new((v[0], v[3], v[2], v[1])), self.bm.faces.new((v[3], v[4], v[5], v[2])),
              self.bm.faces.new((v[0], v[1], v[7], v[6])), self.bm.faces.new((v[4], v[10], v[11], v[5])),
              self.bm.faces.new((v[0], v[6], v[9], v[3])), self.bm.faces.new((v[3], v[9], v[10], v[4])),
              self.bm.faces.new((v[1], v[2], v[8], v[7])), self.bm.faces.new((v[2], v[5], v[11], v[8]))]
        self._fertig(fl, slot)
        # gable ends (the triangle under the roof)
        g0 = [self.bm.verts.new(p) for p in ((x0 + ueberstand, y0 + ueberstand, z0), (x0 + ueberstand, y1 - ueberstand, z0),
                                             (x0 + ueberstand, mitte[1], z1 - ueberstand * 0.5))]
        g1 = [self.bm.verts.new(p) for p in ((x1 - ueberstand, y0 + ueberstand, z0), (x1 - ueberstand, mitte[1], z1 - ueberstand * 0.5),
                                             (x1 - ueberstand, y1 - ueberstand, z0))]
        self._fertig([self.bm.faces.new(g0), self.bm.faces.new(g1)], "Gilde")

    def fertig(self):
        """Into a 1 m box centred on the origin, then export"""
        bm = self.bm
        lo = Vector((min(v.co.x for v in bm.verts), min(v.co.y for v in bm.verts), min(v.co.z for v in bm.verts)))
        hi = Vector((max(v.co.x for v in bm.verts), max(v.co.y for v in bm.verts), max(v.co.z for v in bm.verts)))
        g = hi - lo
        m = Matrix.Diagonal(Vector((1 / g.x, 1 / g.y, 1 / g.z, 1.0))) @ Matrix.Translation(-(lo + hi) / 2)
        bmesh.ops.transform(bm, matrix=m, verts=bm.verts)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        me = bpy.data.meshes.new(self.name)
        bm.to_mesh(me)
        bm.free()
        obj = bpy.data.objects.new(self.name, me)
        bpy.context.scene.collection.objects.link(obj)
        for s in SLOTS:
            me.materials.append(bpy.data.materials.get(s) or bpy.data.materials.new(s))
        # drop unused slots so Unreal gets only the ones in use
        benutzt = sorted(set(p.material_index for p in me.polygons))
        for p in me.polygons:
            p.material_index = benutzt.index(p.material_index)
        for i in reversed(range(len(SLOTS))):
            if i not in benutzt:
                me.materials.pop(index=i)
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        pfad = os.path.join(OUT, self.name + ".fbx")
        bpy.ops.export_scene.fbx(filepath=pfad, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                                 apply_scale_options="FBX_SCALE_ALL", bake_anim=False, mesh_smooth_type="FACE")
        print("[Bauten] %s: %d Dreiecke, Slots %s -> %s" % (self.name, sum(len(p.vertices) - 2 for p in me.polygons),
                                                            [m.name for m in me.materials], pfad))
        bpy.data.objects.remove(obj)


def fachwerk(b, x0, x1, y, z0, z1, aussen, rnd):
    """Timber frame on a facade (y = wall plane, aussen = which way is out)"""
    t = 0.22
    yy = y + aussen * 0.08
    for z in (z0 + 0.1, z1 - 0.1):
        b.balken((x0, yy, z), (x1, yy, z), t, "Holz")
    n = max(2, int((x1 - x0) / 2.2))
    for k in range(n + 1):
        x = x0 + (x1 - x0) * k / n
        b.balken((x, yy, z0), (x, yy, z1), t, "Holz")
        if k < n and rnd.random() < 0.7:
            xn = x0 + (x1 - x0) * (k + 1) / n
            if rnd.random() < 0.5:
                b.balken((x, yy, z0), (xn, yy, z1), t * 0.8, "Holz")
            else:
                b.balken((x, yy, z1), (xn, yy, z0), t * 0.8, "Holz")


def haus(name, saat):
    rnd = random.Random(saat)
    b = Bau(name)
    L, B = 12.0, 9.0
    unten, oben = 3.2, 2.8
    b.kiste((0, 0, unten / 2), (L, B, unten), "Stein")
    b.kiste((0, 0, unten + oben / 2), (L + 0.5, B + 0.5, oben), "Gilde")      # the upper floor overhangs
    for aussen in (-1, 1):
        fachwerk(b, -L / 2 - 0.25, L / 2 + 0.25, aussen * (B / 2 + 0.25), unten, unten + oben, aussen, rnd)
        # windows below and above, a door on one side
        for k in range(3):
            x = -L / 2 + L * (k + 0.5) / 3 + rnd.uniform(-0.6, 0.6)
            b.kiste((x, aussen * (B / 2 + 0.3), unten + 1.4), (1.0, 0.12, 1.2), "Dunkel")
            if aussen < 0 and k == 1:
                b.kiste((x, aussen * (B / 2 + 0.05), 1.2), (1.4, 0.12, 2.4), "Dunkel")
                b.kiste((x, aussen * (B / 2 + 0.1), 2.5), (1.8, 0.25, 0.25), "Holz")
            elif rnd.random() < 0.6:
                b.kiste((x, aussen * (B / 2 + 0.05), 1.6), (0.7, 0.12, 0.8), "Dunkel")
    # corner stones
    for sx in (-1, 1):
        for sy in (-1, 1):
            for z in range(4):
                b.kiste((sx * (L / 2 - 0.1), sy * (B / 2 - 0.1), 0.35 + z * 0.8), (0.7, 0.7, 0.55), "Stein", dreh=rnd.uniform(-4, 4))
    if saat % 2:
        b.satteldach((0, 0, unten + oben), L + 0.5, B + 0.5, 3.4, "Dach", 0.5)
    else:
        # roof along Y and a dormer
        b2 = Bau("tmp")
        b.satteldach((0, 0, unten + oben), L + 0.5, B + 0.5, 3.0, "Dach", 0.5)
        b.kiste((L * 0.15, -B * 0.25, unten + oben + 1.0), (1.6, 1.6, 1.6), "Gilde")
        b.kiste((L * 0.15, -B * 0.25 - 0.82, unten + oben + 1.1), (0.8, 0.1, 0.8), "Dunkel")
        b2.bm.free()
    # chimney
    cx = rnd.choice((-1, 1)) * (L / 2 - 1.5)
    b.kiste((cx, B * 0.18, unten + oben + 2.2), (0.9, 0.9, 4.6), "Stein")
    b.kiste((cx, B * 0.18, unten + oben + 4.6), (1.1, 1.1, 0.3), "Stein")
    b.fertig()


def turm():
    rnd = random.Random(3)
    b = Bau("SM_ES_Turm")
    H = 18.0
    profil = [(3.15, 0.0), (3.1, 1.0), (2.85, H * 0.95), (3.3, H * 0.97), (3.3, H)]
    b.drehkoerper(profil, "Stein", seiten=20, glatt=False)
    for z in (H * 0.33, H * 0.66):
        r = 3.1 - 0.25 * z / H
        b.drehkoerper([(r + 0.12, z), (r + 0.12, z + 0.35)], "Stein", seiten=20)
    for k in range(9):
        a = 2 * math.pi * k / 9 + rnd.uniform(-0.1, 0.1)
        z = rnd.uniform(3.0, H - 3.0)
        r = 3.1 - 0.25 * z / H
        b.kiste((math.cos(a) * r, math.sin(a) * r, z), (0.3, 0.3, 1.3), "Dunkel", dreh=math.degrees(a))
    # crenellations
    for k in range(12):
        a = 2 * math.pi * k / 12
        b.kiste((math.cos(a) * 3.1, math.sin(a) * 3.1, H + 0.5), (0.8, 0.5, 1.0), "Stein", dreh=math.degrees(a) + 90)
    b.kiste((0, -3.05, 1.3), (1.4, 0.4, 2.6), "Dunkel")
    b.fertig()


def turmdach():
    b = Bau("SM_ES_Turmdach")
    b.drehkoerper([(3.5, 0.0), (3.1, 0.4), (2.4, 1.4), (1.2, 3.2), (0.25, 4.6), (0.05, 5.0)], "Dach", seiten=16, glatt=True)
    b.drehkoerper([(0.08, 4.6), (0.08, 5.9), (0.25, 6.0), (0.0, 6.3)], "Metall", seiten=6)
    b.fertig()


def halle():
    rnd = random.Random(7)
    b = Bau("SM_ES_Halle")
    L, B, W = 14.0, 10.0, 4.8
    for aussen in (-1, 1):
        b.kiste((0, aussen * (B / 2 - 0.4), W / 2), (L, 0.8, W), "Stein")
        for k in range(5):
            x = -L / 2 + 1.0 + k * (L - 2.0) / 4
            b.kiste((x, aussen * (B / 2 + 0.2), 1.6), (0.9, 1.2, 3.2), "Stein", kipp=(aussen * 6.0, 0.0))
    b.kiste((L / 2 - 0.4, 0, W / 2), (0.8, B, W), "Stein")
    # front: two piers, a lintel and the glowing forge mouth
    for sy in (-1, 1):
        b.kiste((-L / 2 + 0.4, sy * (B / 2 - 1.6), W / 2), (0.8, 3.2, W), "Stein")
    b.kiste((-L / 2 + 0.4, 0, W - 0.6), (0.9, B - 2.0, 1.2), "Stein")
    b.kiste((-L / 2 + 1.0, 0, 1.6), (0.3, B - 3.6, 3.2), "Dunkel")
    b.kiste((-L / 2 + 2.5, 0, 0.5), (2.2, 2.6, 1.0), "Stein")
    b.kiste((-L / 2 + 2.5, 0, 1.05), (1.8, 2.2, 0.15), "Glut")
    b.satteldach((0, 0, W), L, B, 3.6, "Dach", 0.6)
    b.kiste((L * 0.2, 0, W + 3.7), (3.0, 1.4, 0.8), "Dach")       # vent on the ridge
    for k in range(4):
        b.kiste((rnd.uniform(-L / 2, L / 2), rnd.choice((-1, 1)) * (B / 2 + 1.2), 0.3), (rnd.uniform(0.6, 1.4),) * 2 + (0.6,), "Gilde",
                dreh=rnd.uniform(0, 90))
    b.fertig()


def schlot():
    b = Bau("SM_ES_Schlot")
    H = 18.0
    b.drehkoerper([(1.3, 0.0), (1.25, 2.0), (0.85, H - 1.0), (1.0, H - 0.7), (1.0, H)], "Stein", seiten=12, glatt=False)
    for z in (3.0, 7.0, 11.0, 15.0):
        r = 1.25 - 0.4 * (z - 2.0) / (H - 3.0)
        b.drehkoerper([(r + 0.06, z), (r + 0.06, z + 0.25)], "Metall", seiten=12)
    b.drehkoerper([(0.82, H - 0.05), (0.82, H + 0.05)], "Glut", seiten=12)
    b.fertig()


def grubenpfosten():
    b = Bau("SM_ES_Grubenpfosten")
    H = 26.0
    b.kiste((0, 0, H / 2), (0.8, 0.8, H), "Holz")
    for z in (0.6, H * 0.33, H * 0.66, H - 0.6):
        b.kiste((0, 0, z), (0.92, 0.92, 0.3), "Metall")
    b.fertig()


def plattform():
    rnd = random.Random(11)
    b = Bau("SM_ES_Plattform")
    for k in range(16):
        y = -4.7 + k * 0.625
        b.kiste((rnd.uniform(-0.15, 0.15), y, 0.85), (10.0, 0.56, 0.15), "Holz", dreh=rnd.uniform(-0.8, 0.8))
    for x in (-4.5, 0.0, 4.5):
        b.kiste((x, 0, 0.45), (0.45, 10.0, 0.6), "Holz")
    for sx in (-1, 1):
        for k in range(6):
            p = -4.8 + k * 1.92
            b.balken((sx * 4.85, p, 0.9), (sx * 4.85, p, 2.0), 0.15, "Holz")
            b.balken((p, sx * 4.85, 0.9), (p, sx * 4.85, 2.0), 0.15, "Holz")
        b.balken((sx * 4.85, -4.85, 1.9), (sx * 4.85, 4.85, 1.9), 0.14, "Holz")
        b.balken((-4.85, sx * 4.85, 1.9), (4.85, sx * 4.85, 1.9), 0.14, "Holz")
    b.kiste((0, 0, 0.05), (10.0, 10.0, 0.1), "Holz")            # keeps the 1 m box: the bottom of the beams
    b.fertig()


def seilrad():
    b = Bau("SM_ES_Seilrad")
    R = 3.0
    b.drehkoerper([(R - 0.25, -0.3), (R, -0.3), (R, -0.12), (R - 0.12, 0.0), (R, 0.12), (R, 0.3), (R - 0.25, 0.3), (R - 0.25, -0.3)],
                  "Metall", seiten=32, deckel=False)
    for k in range(8):
        a = 2 * math.pi * k / 8
        b.balken((0, 0, 0), (math.cos(a) * (R - 0.2), math.sin(a) * (R - 0.2), 0), 0.16, "Metall")
    b.drehkoerper([(0.45, -0.3), (0.45, 0.3)], "Metall", seiten=12)
    b.fertig()


def wachturm():
    rnd = random.Random(5)
    b = Bau("SM_ES_Wachturm")
    H = 11.0
    b.kiste((0, 0, H / 2), (5.0, 5.0, H), "Stein")
    for z in range(6):
        for sx in (-1, 1):
            for sy in (-1, 1):
                b.kiste((sx * 2.45, sy * 2.45, 0.6 + z * 1.9), (0.8, 0.8, 0.7), "Stein", dreh=rnd.uniform(-4, 4))
    # wooden lookout on beams, overhanging
    for sx in (-1, 1):
        b.balken((sx * 2.2, -3.2, H - 0.8), (sx * 2.2, 3.2, H - 0.8), 0.35, "Holz")
    b.kiste((0, 0, H + 0.1), (6.0, 6.0, 0.3), "Holz")
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.balken((sx * 2.8, sy * 2.8, H + 0.2), (sx * 2.8, sy * 2.8, H + 2.8), 0.3, "Holz")
    for aussen in (-1, 1):
        b.kiste((0, aussen * 2.85, H + 0.7), (5.6, 0.15, 1.0), "Holz")
        b.kiste((aussen * 2.85, 0, H + 0.7), (0.15, 5.6, 1.0), "Holz")
    b.drehkoerper([(4.3, H + 2.8), (0.2, H + 5.0), (0.0, H + 5.1)], "Dach", seiten=4, glatt=False)
    b.kiste((0, -2.55, 1.3), (1.3, 0.2, 2.6), "Dunkel")
    for k in range(4):
        b.kiste((rnd.uniform(-1.5, 1.5), -2.55, rnd.uniform(4, 9)), (0.3, 0.2, 1.1), "Dunkel")
    b.fertig()


def feuerstelle():
    rnd = random.Random(13)
    b = Bau("SM_ES_Feuerstelle")
    for k in range(11):
        a = 2 * math.pi * k / 11
        s = rnd.uniform(0.25, 0.38)
        b.kiste((math.cos(a) * 0.68, math.sin(a) * 0.68, s * 0.4), (s, s * 0.9, s * 0.8), "Stein", dreh=rnd.uniform(0, 90))
    for k in range(4):
        a = math.pi * k / 4 + rnd.uniform(-0.2, 0.2)
        b.balken((math.cos(a) * 0.55, math.sin(a) * 0.55, 0.08), (-math.cos(a) * 0.55, -math.sin(a) * 0.55, 0.32), 0.1, "Holz")
    b.drehkoerper([(0.45, 0.0), (0.3, 0.12), (0.0, 0.16)], "Glut", seiten=10)
    b.fertig()


def zelt():
    rnd = random.Random(17)
    b = Bau("SM_ES_Zelt")
    R, H = 2.0, 2.6
    seiten = 6
    spitze = Vector((0, 0, H))
    rand = [Vector((math.cos(2 * math.pi * k / seiten) * R, math.sin(2 * math.pi * k / seiten) * R, 0.0)) for k in range(seiten)]
    for k in range(seiten):
        a, c = rand[k], rand[(k + 1) % seiten]
        mitte = (a + c) / 2 * 0.93 + Vector((0, 0, 0.0))
        bauch = (a + spitze) / 2 * 0.5 + (c + spitze) / 2 * 0.5
        bauch = bauch * 0.94
        va, vc, vs, vm, vb = (b.bm.verts.new(p) for p in (a, c, spitze, mitte, bauch))
        fl = [b.bm.faces.new((va, vm, vb)), b.bm.faces.new((vm, vc, vb)), b.bm.faces.new((va, vb, vs)), b.bm.faces.new((vb, vc, vs))]
        b._fertig(fl, "Zelt")
    for k in range(3):
        a = 2 * math.pi * k / 3 + 0.3
        b.balken((math.cos(a) * 0.1, math.sin(a) * 0.1, H - 0.4), (math.cos(a) * 0.35, math.sin(a) * 0.35, H + 0.5), 0.07, "Holz")
    b.kiste((R * 0.78, 0, 0.6), (0.1, 0.9, 1.2), "Dunkel", dreh=0.0)
    b.fertig()


def nidsiegel():
    rnd = random.Random(19)
    b = Bau("SM_ES_NidSiegel")
    for k in range(16):
        a = 2 * math.pi * k / 16
        b.kiste((math.cos(a) * 2.3, math.sin(a) * 2.3, 0.3), (1.0, 0.55, 0.6 + rnd.uniform(-0.05, 0.1)), "Stein", dreh=math.degrees(a) + 90)
    b.drehkoerper([(2.0, 0.0), (2.0, 0.08)], "Nid", seiten=24)
    for k in range(-4, 5):
        b.balken((k * 0.45, -1.95, 0.45), (k * 0.45, 1.95, 0.45), 0.1, "Metall")
        b.balken((-1.95, k * 0.45, 0.5), (1.95, k * 0.45, 0.5), 0.1, "Metall")
    for k in range(4):
        a = 2 * math.pi * k / 4 + 0.4
        b.balken((math.cos(a) * 1.9, math.sin(a) * 1.9, 0.55), (0, 0, 0.55), 0.08, "Metall")
    b.kiste((0, 0, 0.58), (0.7, 0.7, 0.12), "Metall", dreh=45.0)
    b.fertig()


def bohrer():
    b = Bau("SM_ES_Bohrer")
    H = 9.0
    b.drehkoerper([(0.6, 0.0), (0.6, 0.6), (0.3, 0.7)], "Stein", seiten=10)
    b.drehkoerper([(0.16, 0.0), (0.16, H - 2.0)], "Metall", seiten=10)
    for z in (1.5, 3.5, 5.5):
        b.drehkoerper([(0.24, z), (0.24, z + 0.3)], "Metall", seiten=10)
    b.kiste((0, 0, H - 1.4), (1.1, 0.9, 1.6), "Metall")
    b.drehkoerper([(0.45, H - 0.7), (0.45, H - 0.5)], "Metall", seiten=16)
    b.drehkoerper([(0.06, H - 0.6), (0.06, H)], "Metall", seiten=6)
    b.kiste((0, 0.5, H - 1.4), (0.5, 0.15, 0.5), "Glut")
    b.fertig()


if __name__ == "__main__":
    bpy.ops.wm.read_factory_settings(use_empty=True)
    haus("SM_ES_Haus_A", 1)
    haus("SM_ES_Haus_B", 2)
    turm()
    turmdach()
    halle()
    schlot()
    grubenpfosten()
    plattform()
    seilrad()
    wachturm()
    feuerstelle()
    zelt()
    nidsiegel()
    bohrer()
    print("[Bauten] fertig")
