"""
Echte Knochen aus dem Fab-Skelett ("Human Skeleton" von Salt Lake Manor, CC-BY: Namensnennung in den Credits; Boris 16.11.2026:
"die Platzhalter ersetzen und beim Magier den Knochensturm und andere Knochenteile verwenden").
Ersetzt die groben Knochen aus Röhren und Drehkörpern des Höhlenbaukastens und die Kegel der Knochenzauber.

Quelle: Vael_Quellen/Skelett_Fab/skeleton.blend (aus dem Fab-Cache), die Texturen packt das Skript nach Skelett_Fab/Textures/Skeleton aus.

Ausgaben (alle Knochen tragen den Slot "Gebein" mit den UVs der Fab-Textur):
  <höhle>/   SM_HK_Schaedelwand_A/B, SM_HK_Knochensaeule, SM_HK_Schaedelbogen, SM_HK_Schaedelbogen_Breit, SM_HK_Knochenhaufen
             wie im Baukasten beschrieben (Maße, Ursprung, Vorderseite), Slots Gebein und Dunkel; Knochen 1,5-fach (aus der
             Draufsicht lesbar), Wände wie in den Pariser Katakomben: Reihen von Schädeln zwischen Bändern aus Knochenenden
  <zauber>/  SM_Knochen_<Teil>: einzelne Knochen in echter Größe, Mitte im Ursprung, lange Achse entlang +Z (Knochensturm,
             Knochendornen, Markkäfig)
  <formen>/  SM_Narbenwall: der Block des Narbenwalls (Würfel von 1 m um die Mitte), rohes Fleisch, aus dem echte Rippenbögen,
             Langknochen und ein Schädel brechen (Slots Fleisch und Knochen)

  blender -b C:/Projekte/Vael_Quellen/Skelett_Fab/skeleton.blend -P Tools/Welt/vael_knochen_fab.py -- \
      C:/Projekte/Vael_Quellen/Hoehlen/export C:/Projekte/Vael_Quellen/Knochen/export C:/Projekte/Vael/Scripts/Data/Zauber
"""
import bpy, bmesh, sys, os, math, random
from mathutils import Vector, Matrix, noise

ARGS = sys.argv[sys.argv.index("--") + 1:]
CAVE_OUT, SPELL_OUT, FORM_OUT = ARGS[0], ARGS[1], ARGS[2]
for d in (CAVE_OUT, SPELL_OUT, FORM_OUT):
    os.makedirs(d, exist_ok=True)

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Refugium"))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Ingenieur"))
_argv = sys.argv
sys.argv = _argv[:_argv.index("--") + 2]       # glutkammer_probe liest nach dem Ausgabeordner einen Zufallswert
import glutkammer_probe as gk  # noqa: E402
from vael_ingenieur_werkstuecke import lathe, box  # noqa: E402
sys.argv = _argv

CAVE_SCALE = 1.5          # Knochen der Höhle: echte Größe ging aus der Draufsicht im Rauschen unter


def log(text):
    print("KNOCHEN " + text, flush=True)


# ---------------------------------------------------------------- Vorlagen aus dem Fab-Skelett

for img in bpy.data.images:
    if img.packed_file is not None:
        img.unpack(method="WRITE_LOCAL")
        log("Textur " + bpy.path.abspath(img.filepath))

SCENE = bpy.context.scene
for o in list(SCENE.objects):
    o.hide_render = False
GEBEIN = bpy.data.materials.new("Gebein")
DUNKEL = bpy.data.materials.new("Dunkel")
FLEISCH = bpy.data.materials.new("Fleisch")
KNOCHEN = bpy.data.materials.new("Knochen")


def tris(me):
    return sum(len(p.vertices) - 2 for p in me.polygons)


def evaluated(names):
    """Die Teile so, wie sie in der Szene stehen (mit Rig und Maßstab), in Weltkoordinaten zu einem Mesh verbunden"""
    dg = bpy.context.evaluated_depsgraph_get()
    bm = bmesh.new()
    for n in names:
        o = bpy.data.objects[n]
        me = bpy.data.meshes.new_from_object(o.evaluated_get(dg))
        me.transform(o.matrix_world)
        bm.from_mesh(me)
        bpy.data.meshes.remove(me)
    me = bpy.data.meshes.new("t")
    bm.to_mesh(me)
    bm.free()
    return me


def decimate(me, target):
    have = tris(me)
    if have <= target:
        return me
    o = bpy.data.objects.new("dec", me)
    SCENE.collection.objects.link(o)
    mod = o.modifiers.new("d", "DECIMATE")
    mod.ratio = target / have
    dg = bpy.context.evaluated_depsgraph_get()
    out = bpy.data.meshes.new_from_object(o.evaluated_get(dg))
    bpy.data.objects.remove(o)
    bpy.data.meshes.remove(me)
    return out


def finish_template(me, name, material=None):
    me.materials.clear()
    me.materials.append(material or GEBEIN)
    for p in me.polygons:
        p.material_index = 0
        p.use_smooth = True
    me.name = name
    return me


def centred(me, axis_to_z=None):
    """Mitte der Hülle in den Ursprung; die lange Achse nach +Z"""
    if axis_to_z == "X":
        me.transform(Matrix.Rotation(math.radians(90), 4, "Y"))
    elif axis_to_z == "Y":
        me.transform(Matrix.Rotation(math.radians(-90), 4, "X"))
    lo = Vector([min(v.co[i] for v in me.vertices) for i in range(3)])
    hi = Vector([max(v.co[i] for v in me.vertices) for i in range(3)])
    me.transform(Matrix.Translation(-(lo + hi) / 2))
    return me


def bone(src, target, axis=None):
    return finish_template(decimate(centred(evaluated([src]), axis), target), src + "_t")


def bone_end(src, keep, top, target):
    """Das Ende eines Langknochens (die Gelenkknolle), wie es aus den Katakombenwänden schaut: Ursprung am Schnitt, Knolle bei +Z"""
    me = centred(evaluated([src]))
    zs = [v.co.z for v in me.vertices]
    bm = bmesh.new()
    bm.from_mesh(me)
    cut = (max(zs) - keep) if top else (min(zs) + keep)
    geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
    res = bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0, 0, cut), plane_no=(0, 0, 1), clear_inner=top, clear_outer=not top)
    bm.to_mesh(me)
    bm.free()
    me.transform(Matrix.Translation((0, 0, -cut)))
    if not top:
        me.transform(Matrix.Rotation(math.pi, 4, "X"))
    return finish_template(decimate(me, target), src + ("_oben" if top else "_unten"))


def skull(with_jaw, target):
    parts = ["SK_Head_Merged"] + (["SK_Jaw_Merged"] if with_jaw else [])
    return finish_template(decimate(centred(evaluated(parts)), target), "Schaedel" + ("_Kiefer" if with_jaw else ""))


def half_ribcage(target):
    """Eine Hälfte des Brustkorbs: Rippenbögen, die sich wie Klauen krümmen"""
    me = evaluated(["SM_RibCage"])
    lo = Vector([min(v.co[i] for v in me.vertices) for i in range(3)])
    hi = Vector([max(v.co[i] for v in me.vertices) for i in range(3)])
    mid = (lo + hi) / 2
    bm = bmesh.new()
    bm.from_mesh(me)
    geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
    bmesh.ops.bisect_plane(bm, geom=geom, plane_co=mid, plane_no=(1, 0, 0), clear_inner=True)
    bm.to_mesh(me)
    bm.free()
    me.transform(Matrix.Translation(-mid))
    return finish_template(decimate(me, target), "Rippen_Haelfte")


# Höhle: sparsam (Nanite übernimmt die Menge), Zauber: feiner (wenige, nah)
T = {
    "schaedel": skull(False, 900), "schaedel_k": skull(True, 1200),
    "femur": bone("SM_Femur", 400), "tibia": bone("SM_Tibia", 350), "humerus": bone("SM_Humerus", 300),
    "ulna": bone("SM_Ulna", 220), "radius": bone("SM_Radius", 220), "fibula": bone("SM_Fibula", 220),
    "becken": bone("SM_Pelvis", 1200), "korb": bone("SM_RibCage", 1500), "wirbel": bone("SM_Spine_L03", 150),
    "kiefer": bone("SM_Jaw", 300), "schulter": bone("SM_Scapula", 300),
    "ende": [bone_end("SM_Femur", 0.11, True, 80), bone_end("SM_Femur", 0.11, False, 80),
             bone_end("SM_Humerus", 0.1, True, 70), bone_end("SM_Tibia", 0.1, True, 80), bone_end("SM_Tibia", 0.1, False, 70)],
}
LONG = ["femur", "tibia", "humerus", "ulna", "radius", "fibula"]


# ---------------------------------------------------------------- Zusammensetzen

class Piece:
    """Sammelt Knochen als Objekte (verknüpfte Kopien der Vorlagen) und einen dunklen Kern; join() macht ein Mesh daraus"""

    def __init__(self, name, slots=(GEBEIN, DUNKEL)):
        self.name = name
        self.slots = slots
        self.objs = []
        self.core = None

    def add(self, me, matrix):
        o = bpy.data.objects.new("k", me)
        o.matrix_world = matrix
        SCENE.collection.objects.link(o)
        self.objs.append(o)

    def set_core(self, builder_fn):
        obj = gk.new_object(self.name, [s.name for s in self.slots])
        obj.data.materials.clear()
        for s in self.slots:
            obj.data.materials.append(s)
        b = gk.Builder()
        builder_fn(b)
        b.finish(obj)
        self.core = obj

    def join(self):
        if self.core is None:
            self.set_core(lambda b: None)
        bpy.ops.object.select_all(action="DESELECT")
        for o in self.objs:
            o.data = o.data.copy()          # eigene Daten: join verändert sonst die Vorlage
            o.select_set(True)
        self.core.select_set(True)
        bpy.context.view_layer.objects.active = self.core
        bpy.ops.object.join()
        obj = bpy.context.view_layer.objects.active
        obj.name = self.name
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
        for p in obj.data.polygons:
            p.use_smooth = True
        return obj


def facing_matrix(at, facing, size, roll=0.0):
    """Ein Schädel (Gesicht in der Vorlage nach -Y, oben +Z) schaut in Richtung facing"""
    y = -facing.normalized()
    x = y.cross(Vector((0, 0, 1)))
    if x.length < 1e-4:
        x = Vector((1, 0, 0))
    x.normalize()
    z = x.cross(y)
    rot = Matrix((x, y, z)).transposed().to_4x4() @ Matrix.Rotation(roll, 4, "Y")
    return Matrix.Translation(at) @ rot @ Matrix.Scale(size, 4)


def along_matrix(at, direction, size, spin=0.0):
    """Ein Knochen (lange Achse +Z) zeigt in Richtung direction, um sich selbst um spin gedreht"""
    z = direction.normalized()
    helper = Vector((0, 0, 1)) if abs(z.z) < 0.9 else Vector((1, 0, 0))
    x = helper.cross(z).normalized()
    y = z.cross(x)
    rot = Matrix((x, y, z)).transposed().to_4x4() @ Matrix.Rotation(spin, 4, "Z")
    return Matrix.Translation(at) @ rot @ Matrix.Scale(size, 4)


def export(obj, folder):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(folder, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_ALL", bake_anim=False, mesh_smooth_type="FACE", colors_type="NONE")
    log("{}: {} Dreiecke -> {}".format(obj.name, tris(obj.data), path))
    bpy.data.objects.remove(obj)


S = CAVE_SCALE
SKULL_H = 0.25 * S           # Schädel mit Abstand in der Reihe (ohne Kiefer ~0,19 m hoch, 0,15 m breit)
SKULL_W = 0.17 * S
END_STEP = 0.07 * S          # Abstand der Knochenenden in einem Band: dicht, der Kern soll nicht durchscheinen
END_SIZE = 1.25 * S


def skull_row(p, rnd, centre_line, out, length, z, depth, count=None):
    """Eine Reihe Schädel entlang centre_line (Richtung entlang), Gesichter nach out; depth: wie weit vor der Wand"""
    n = count or int(length / SKULL_W)
    for k in range(n):
        t = (k + 0.5) / n - 0.5
        at = centre_line(t) + out * depth + Vector((0, 0, z + rnd.uniform(-0.015, 0.015)))
        face = out + Vector((rnd.uniform(-0.1, 0.1), 0, rnd.uniform(-0.12, 0.05)))
        key = "schaedel_k" if rnd.random() < 0.25 else "schaedel"
        p.add(T[key], facing_matrix(at, face, S * rnd.uniform(0.95, 1.05), rnd.uniform(-0.12, 0.12)))


def end_band(p, rnd, centre_line, out, length, z, rows=3):
    """Ein Band aus Knochenenden, die dicht gestapelt aus der Wand schauen"""
    n = int(length / END_STEP)
    for row in range(rows):
        for k in range(n):
            t = (k + 0.5 + (row % 2) * 0.5) / n - 0.5
            if t > 0.5:
                continue
            at = centre_line(t) + out * (-0.03) + Vector((0, 0, z + row * END_STEP * 0.9 + rnd.uniform(-0.008, 0.008)))
            d = out + Vector((rnd.uniform(-0.04, 0.04), rnd.uniform(-0.04, 0.04), rnd.uniform(-0.03, 0.03)))
            p.add(rnd.choice(T["ende"]), along_matrix(at, d, END_SIZE * rnd.uniform(0.92, 1.06), rnd.uniform(0, 6.28)))


def layers(p, rnd, centre_line, out, length, height, top_open=True):
    """Von unten: Band, Schädel, Band, Schädel ... bis zur Höhe"""
    z = 0.06
    band = True
    while z < height - 0.15:
        if band:
            end_band(p, rnd, centre_line, out, length, z)
            z += END_STEP * 2.7 + 0.03
        else:
            skull_row(p, rnd, centre_line, out, length, z + SKULL_H * 0.42, 0.11 * S)
            z += SKULL_H + 0.03
        band = not band
    return z


def skull_wall(name, seed):
    """4,4 m breit, 3,2 m hoch, Vorderseite -Y; auf beiden Seiten Schädel (die Wand steht mal so, mal so im Gang)"""
    rnd = random.Random(seed)
    p = Piece(name)
    p.set_core(lambda b: box(b, Vector((0, 0.15, 1.6)), (4.4, 0.3, 3.2), 1))
    for out, y0 in ((Vector((0, -1, 0)), 0.0), (Vector((0, 1, 0)), 0.3)):
        top = layers(p, rnd, lambda t, y0=y0: Vector((t * 4.3, y0, 0)), out, 4.3, 3.15)
    # oben aufgeschichtet: ein paar lose Schädel und Langknochen auf der Krone
    for k in range(9):
        at = Vector((rnd.uniform(-2.0, 2.0), 0.15 + rnd.uniform(-0.1, 0.1), 3.2 + 0.08))
        if rnd.random() < 0.5:
            p.add(T["schaedel"], facing_matrix(at + Vector((0, 0, 0.1)), Vector((rnd.uniform(-1, 1), rnd.uniform(-1, 1), 0.2)), S))
        else:
            p.add(T[rnd.choice(LONG)], along_matrix(at, Vector((1, rnd.uniform(-0.3, 0.3), rnd.uniform(-0.1, 0.1))), S, rnd.uniform(0, 6)))
    export(p.join(), CAVE_OUT)


def column(p, rnd, at, h, r):
    """Säule der Katakomben: dunkler Kern, Ringe nach außen schauender Schädel zwischen Bändern aus Knochenenden"""
    def ring(t):
        a = t * 2 * math.pi
        return at + Vector((math.cos(a), math.sin(a), 0)) * r
    z = 0.06
    band = True
    circ = 2 * math.pi * r
    while z < h - 0.2:
        if band:
            n = int(circ / END_STEP)
            for row in range(3):
                for k in range(n):
                    a = (k + 0.5 * (row % 2)) / n * 2 * math.pi
                    out = Vector((math.cos(a), math.sin(a), 0))
                    d = out + Vector((rnd.uniform(-0.04, 0.04), rnd.uniform(-0.04, 0.04), rnd.uniform(-0.03, 0.03)))
                    p.add(rnd.choice(T["ende"]), along_matrix(at + out * (r - 0.03) + Vector((0, 0, z + row * END_STEP * 0.9)), d, END_SIZE, rnd.uniform(0, 6)))
            z += END_STEP * 2.7 + 0.03
        else:
            n = int((circ + 0.9) / SKULL_W)
            off = rnd.uniform(0, 1)
            for k in range(n):
                a = (k + off) / n * 2 * math.pi
                out = Vector((math.cos(a), math.sin(a), 0))
                key = "schaedel_k" if rnd.random() < 0.2 else "schaedel"
                p.add(T[key], facing_matrix(at + out * (r + 0.1 * S) + Vector((0, 0, z + SKULL_H * 0.42)), out + Vector((0, 0, rnd.uniform(-0.1, 0.05))), S))
            z += SKULL_H + 0.03
        band = not band
    # Krone: Langknochen schräg nach außen, ein Schädel obenauf
    for k in range(10):
        a = 2 * math.pi * k / 10 + rnd.uniform(-0.1, 0.1)
        out = Vector((math.cos(a), math.sin(a), 0))
        p.add(T[rnd.choice(["femur", "tibia", "humerus"])], along_matrix(at + Vector((0, 0, h + 0.1)) + out * (r * 0.6), out + Vector((0, 0, 0.7)), S, rnd.uniform(0, 6)))
    p.add(T["schaedel_k"], facing_matrix(at + Vector((0, 0, h + 0.22)), Vector((rnd.uniform(-1, 1), rnd.uniform(-1, 1), 0.1)), S * 1.1))


def bone_pillar(name, seed):
    """3,6 m hoch, Ursprung unten in der Mitte"""
    rnd = random.Random(seed)
    p = Piece(name)
    h, r = 3.6, 0.5
    p.set_core(lambda b: lathe(b, [(0.0, 0.0), (r * 0.98, 0.0), (r * 0.95, h + 0.05), (0.0, h + 0.05)], 1, Vector((0, 0, 0)), sides=14))
    column(p, rnd, Vector((0, 0, 0)), h, r)
    export(p.join(), CAVE_OUT)


def skull_arch(name, seed, span=3.6):
    """Zwei Säulen bei Y = ±span/2, darüber ein Bogen aus Schädeln, die in beide Richtungen des Gangs (±X) schauen"""
    rnd = random.Random(seed)
    p = Piece(name)
    half = span / 2
    rise = 1.1 if span < 5 else 1.8
    h, r = 3.0, 0.42
    steps = 15 if span < 5 else 25
    pts = [Vector((0.0, -math.cos(math.pi * k / (steps - 1)) * half, h + math.sin(math.pi * k / (steps - 1)) * rise)) for k in range(steps)]

    def core(b):
        for sy in (-1, 1):
            lathe(b, [(0.0, 0.0), (r * 0.98, 0.0), (r * 0.95, h), (0.0, h)], 1, Vector((0, sy * half, 0)), sides=12)
        b.tube(pts, [0.3] * len(pts), 1, sides=10)
    p.set_core(core)
    for sy in (-1, 1):
        column(p, rnd, Vector((0, sy * half, 0)), h - 0.1, r)
    # Bogen: auf beiden Seiten eine Reihe Schädel, darüber und darunter Knochenenden
    length = sum((b - a).length for a, b in zip(pts, pts[1:]))
    n = int(length / SKULL_W)
    for sx in (-1, 1):
        out = Vector((sx, 0, 0))
        for k in range(n):
            t = (k + 0.5) / n * (steps - 1)
            i = min(int(t), steps - 2)
            c = pts[i].lerp(pts[i + 1], t - i)
            key = "schaedel_k" if rnd.random() < 0.3 else "schaedel"
            p.add(T[key], facing_matrix(c + out * 0.34, out + Vector((0, 0, rnd.uniform(-0.15, 0.05))), S * 0.95))
        for k in range(int(length / END_STEP)):
            t = (k + 0.5) / int(length / END_STEP) * (steps - 1)
            i = min(int(t), steps - 2)
            c = pts[i].lerp(pts[i + 1], t - i)
            tang = (pts[i + 1] - pts[i]).normalized()
            normal = Vector((0, -tang.z, tang.y))      # nach außen (oben) am Bogen
            for side in (-1, 1):
                at = c + normal * side * 0.27 + out * 0.2
                p.add(rnd.choice(T["ende"]), along_matrix(at, out + normal * side * 0.3, END_SIZE * 0.95, rnd.uniform(0, 6)))
    # der Schlussstein: ein großer Schädel mit Kiefer auf jeder Seite
    top = pts[steps // 2]
    for sx in (-1, 1):
        p.add(T["schaedel_k"], facing_matrix(top + Vector((sx * 0.38, 0, 0.3)), Vector((sx, 0, 0.1)), S * 1.6))
    export(p.join(), CAVE_OUT)


def bone_pile(name, seed):
    """Ein Haufen Gebeine, ~2 m: Langknochen kreuz und quer, Becken, ein Brustkorb, Schädel obenauf"""
    rnd = random.Random(seed)
    p = Piece(name)

    def mound(r):
        return 0.55 * max(0.0, 1.0 - (r / 1.0) ** 2) ** 0.8

    for k in range(48):
        a = rnd.uniform(0, 2 * math.pi)
        r = rnd.uniform(0, 1.0) ** 0.7
        at = Vector((math.cos(a) * r, math.sin(a) * r, 0.04 + mound(r) * rnd.uniform(0.3, 1.0)))
        b_ = rnd.uniform(0, math.pi)
        d = Vector((math.cos(b_), math.sin(b_), rnd.uniform(-0.35, 0.35)))
        p.add(T[rnd.choice(LONG + ["femur", "tibia"])], along_matrix(at, d, S * rnd.uniform(0.9, 1.05), rnd.uniform(0, 6.28)))
    for key, count in (("becken", 2), ("korb", 1), ("schulter", 3), ("wirbel", 8), ("kiefer", 3)):
        for k in range(count):
            a = rnd.uniform(0, 2 * math.pi)
            r = rnd.uniform(0, 0.8)
            at = Vector((math.cos(a) * r, math.sin(a) * r, 0.06 + mound(r) * rnd.uniform(0.5, 1.0)))
            rot = Matrix.Rotation(rnd.uniform(-0.6, 0.6), 4, "X") @ Matrix.Rotation(rnd.uniform(0, 6.28), 4, "Z")
            p.add(T[key], Matrix.Translation(at) @ rot @ Matrix.Scale(S, 4))
    for k in range(6):
        a = rnd.uniform(0, 2 * math.pi)
        r = rnd.uniform(0.2, 0.85)
        at = Vector((math.cos(a) * r, math.sin(a) * r, 0.1 + mound(r) * 0.8))
        p.add(T["schaedel" if k % 2 else "schaedel_k"], facing_matrix(at, Vector((math.cos(a), math.sin(a), rnd.uniform(0.0, 0.5))), S, rnd.uniform(-0.4, 0.4)))
    export(p.join(), CAVE_OUT)


# ---------------------------------------------------------------- Zauberknochen

def spell_bones():
    """Einzelne Knochen in echter Größe, feiner aufgelöst als in der Höhle"""
    parts = {"Oberschenkel": ("SM_Femur", 900, None), "Schienbein": ("SM_Tibia", 800, None), "Oberarm": ("SM_Humerus", 600, None),
             "Elle": ("SM_Ulna", 450, None), "Speiche": ("SM_Radius", 450, None), "Wadenbein": ("SM_Fibula", 450, None),
             "Schluesselbein": ("SM_Clavicle", 350, "X"), "Becken": ("SM_Pelvis", 1800, None), "Wirbel": ("SM_Spine_T07", 300, None),
             "Kiefer": ("SM_Jaw", 600, None), "Schulterblatt": ("SM_Scapula", 600, None)}
    for name, (src, target, axis) in parts.items():
        me = finish_template(decimate(centred(evaluated([src]), axis), target), "SM_Knochen_" + name)
        o = bpy.data.objects.new("SM_Knochen_" + name, me)
        SCENE.collection.objects.link(o)
        export(o, SPELL_OUT)
    me = finish_template(decimate(centred(evaluated(["SK_Head_Merged", "SK_Jaw_Merged"])), 2400), "SM_Knochen_Schaedel")
    o = bpy.data.objects.new("SM_Knochen_Schaedel", me)
    SCENE.collection.objects.link(o)
    export(o, SPELL_OUT)
    rib = half_ribcage(1400)
    o = bpy.data.objects.new("SM_Knochen_Rippen", rib)
    SCENE.collection.objects.link(o)
    export(o, SPELL_OUT)


# ---------------------------------------------------------------- gebrochene Knochen (Boris 16.11.: "abbrechen und spitze
# zerbrochene Knochen wirbeln lassen")

def boundary_loops(bm):
    """Die offenen Ränder nach einem Schnitt als geordnete Ringe von Ecken"""
    edges = [e for e in bm.edges if e.is_boundary]
    nxt = {}
    for e in edges:
        a, b = e.verts
        nxt.setdefault(a, []).append(b)
        nxt.setdefault(b, []).append(a)
    seen, loops = set(), []
    for start in nxt:
        if start in seen:
            continue
        ring, prev, cur = [start], None, start
        seen.add(start)
        while True:
            options = [v for v in nxt[cur] if v is not prev and v not in seen]
            if not options:
                break
            prev, cur = cur, options[0]
            ring.append(cur)
            seen.add(cur)
        if len(ring) >= 4:
            loops.append(ring)
    return loops


def jagged_cap(bm, normal, rnd, length_share):
    """Schließt jeden Bruch mit Splittern: der Ring zerfällt in 3-4 Bögen, jeder läuft in eine eigene Zacke aus, eine davon lang
    und scharf, die anderen kürzer und schief; die Zacken stehen zur Seite versetzt wie bei einem echten Splitterbruch"""
    uv = bm.loops.layers.uv.active
    for ring in boundary_loops(bm):
        n_r = len(ring)
        c = sum((v.co for v in ring), Vector()) / n_r
        r = sum((v.co - c).length for v in ring) / n_r
        uvs = {v: (v.link_loops[0][uv].uv.copy() if (uv and v.link_loops) else None) for v in ring}
        known = [u for u in uvs.values() if u is not None]
        mid_uv = (sum(known, Vector((0, 0))) / len(known)) if known else Vector((0.5, 0.5))
        # der Rand bricht unregelmäßig: kleine Zacken
        for v in ring:
            v.co += normal * r * rnd.uniform(0.0, 0.45)
        m = rnd.choice((3, 3, 4)) if n_r >= 8 else 1
        first = rnd.randrange(n_r)
        bounds = sorted(rnd.sample(range(1, n_r), m - 1)) if m > 1 else []
        cuts = [first] + [(first + b) % n_r for b in bounds]
        main = rnd.randrange(m)
        tips = []
        for k in range(m):
            a_, b_ = cuts[k], cuts[(k + 1) % m]
            arc = [ring[(a_ + i) % n_r] for i in range(((b_ - a_) % n_r or n_r) + 1)]
            ac = sum((v.co for v in arc), Vector()) / len(arc)
            out = (ac - c)
            length = r * (length_share * rnd.uniform(0.85, 1.2) if k == main else rnd.uniform(0.8, 2.0))
            tip = bm.verts.new(ac + normal * length + out * rnd.uniform(-0.2, 0.5) - (ac - c) * (0.6 if k == main else 0.1))
            tips.append(tip)
            for i in range(len(arc) - 1):
                try:
                    f = bm.faces.new((arc[i], arc[i + 1], tip))
                except ValueError:
                    continue
                if uv:
                    for loop in f.loops:
                        loop[uv].uv = uvs.get(loop.vert) if uvs.get(loop.vert) is not None else mid_uv
        if m > 1:
            # zwischen zwei Zacken: die Kerbe; in der Mitte der Boden des Bruchs
            for k in range(m):
                shared = ring[cuts[(k + 1) % m]]
                for verts in ((shared, tips[(k + 1) % m], tips[k]),):
                    try:
                        f = bm.faces.new(verts)
                    except ValueError:
                        continue
                    if uv:
                        for loop in f.loops:
                            loop[uv].uv = uvs.get(loop.vert) if uvs.get(loop.vert) is not None else mid_uv
            try:
                f = bm.faces.new(tips)
                if uv:
                    for loop in f.loops:
                        loop[uv].uv = mid_uv
            except ValueError:
                pass
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)


def broken(src, lo, hi, seed, target, axis=None, spike=4.0):
    """Ein gebrochener Knochen: von lo bis hi seiner Länge behalten (0 = unten, 1 = oben), jeder Schnitt schräg und mit
    gezackter Spitze; Mitte im Ursprung, lange Achse +Z"""
    rnd = random.Random(seed)
    me = centred(evaluated([src]), axis)
    zs = [v.co.z for v in me.vertices]
    z0, length = min(zs), max(zs) - min(zs)
    bm = bmesh.new()
    bm.from_mesh(me)
    for frac, sign in ((hi, 1.0), (lo, -1.0)):
        if (sign > 0 and frac >= 1.0) or (sign < 0 and frac <= 0.0):
            continue
        a = rnd.uniform(0, 2 * math.pi)
        tilt = rnd.uniform(0.35, 0.8)
        n = Vector((math.cos(a) * tilt, math.sin(a) * tilt, 1.0)).normalized() * sign
        geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
        bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0, 0, z0 + length * frac), plane_no=n, clear_outer=True)
        jagged_cap(bm, n, rnd, spike)
    bm.to_mesh(me)
    bm.free()
    return finish_template(decimate(centred(me), target), src + "_gebrochen")


def broken_bones():
    """Splitter für den Knochensturm, gebrochene Stäbe für den Markkäfig (unten das Gelenk, oben die Bruchspitze)"""
    parts = {
        # Knochensturm: Schaftstücke mit zwei Bruchspitzen, Gelenkenden mit einer
        "Splitter_01": ("SM_Femur", 0.22, 0.72, 11, 900, None, 4.5), "Splitter_02": ("SM_Femur", 0.55, 1.0, 12, 800, None, 5.0),
        "Splitter_03": ("SM_Femur", 0.0, 0.42, 13, 800, None, 4.0), "Splitter_04": ("SM_Tibia", 0.12, 0.62, 14, 700, None, 5.0),
        "Splitter_05": ("SM_Tibia", 0.5, 1.0, 15, 700, None, 4.0), "Splitter_06": ("SM_Humerus", 0.3, 1.0, 16, 550, None, 5.0),
        "Splitter_07": ("SM_Ulna", 0.08, 0.6, 17, 450, None, 6.0), "Splitter_08": ("SM_Radius", 0.35, 0.95, 18, 450, None, 6.0),
        "Splitter_09": ("SM_Fibula", 0.2, 0.75, 19, 450, None, 7.0), "Splitter_10": ("SM_Clavicle", 0.0, 0.62, 20, 350, "X", 5.0),
        "Splitter_11": ("SM_Humerus", 0.05, 0.5, 21, 500, None, 5.0), "Splitter_12": ("SM_Femur", 0.35, 0.62, 22, 700, None, 6.0),
        # Markkäfig: unten das ganze Gelenk im Boden, oben abgebrochen und spitz; die gebogenen Stücke mit Klauenspitze
        "Kaefig_Unten_01": ("SM_Femur", 0.0, 0.82, 31, 900, None, 5.0), "Kaefig_Unten_02": ("SM_Tibia", 0.0, 0.8, 32, 800, None, 5.0),
        "Kaefig_Oben_01": ("SM_Ulna", 0.0, 0.8, 33, 450, None, 7.0), "Kaefig_Oben_02": ("SM_Radius", 0.0, 0.82, 34, 450, None, 7.0),
        "Kaefig_Oben_03": ("SM_Fibula", 0.0, 0.75, 35, 450, None, 8.0),
    }
    for name, (src, lo, hi, seed, target, axis, spike) in parts.items():
        me = broken(src, lo, hi, seed, target, axis, spike)
        o = bpy.data.objects.new("SM_Knochen_" + name, me)
        SCENE.collection.objects.link(o)
        export(o, SPELL_OUT)


# ---------------------------------------------------------------- Narbenwall

def lumpy_block(radius, amount, scale, seed):
    """Fleisch, das einen Würfel füllt (wie in Tools/Zauber/vael_zauber_formen.py), in Metern"""
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=5, radius=1.0)
    offset = Vector((seed, seed * 1.7, seed * 0.3))
    for vert in bm.verts:
        q = vert.co.normalized()
        biggest = max(abs(q.x), abs(q.y), abs(q.z))
        q = (q / biggest).lerp(q, 0.25)
        push = noise.fractal(q * scale + offset, 1.0, 2.0, 4) * amount
        push += max(0.0, noise.ridged_multi_fractal(q * scale * 0.7 + offset, 1.0, 2.0, 3, 0.0, 2.0) - 0.75) * amount * 0.8
        vert.co = q * radius * (1.0 + push)
    me = bpy.data.meshes.new("SM_Narbenwall")
    bm.to_mesh(me)
    bm.free()
    return me


def scar_wall():
    """Rohes Fleisch, aus dem auf beiden langen Seiten (±X) Rippenbögen brechen und sich nach oben krümmen, dazu Langknochen
    und ein halb versunkener Schädel"""
    rnd = random.Random(7)
    me = lumpy_block(0.52, 0.2, 2.6, 3.0)
    me.materials.append(FLEISCH)
    core = bpy.data.objects.new("SM_Narbenwall", me)
    SCENE.collection.objects.link(core)
    rib = finish_template(half_ribcage(1200), "Rippen_Wall", KNOCHEN)
    objs = []

    def add(mesh, matrix):
        o = bpy.data.objects.new("k", mesh.copy())
        o.matrix_world = matrix
        SCENE.collection.objects.link(o)
        objs.append(o)

    for side in (-1, 1):
        for y in (-0.22, 0.22):
            # die Rippen der Hälfte krümmen sich in der Vorlage nach +X; auf der -X-Seite gespiegelt
            m = Matrix.Translation((side * 0.38, y + rnd.uniform(-0.04, 0.04), 0.05)) @ Matrix.Rotation(math.radians(rnd.uniform(-12, 12)), 4, "Z") \
                @ Matrix.Diagonal((side * 1.9, 1.9, 1.9, 1.0))
            add(rib, m)
    for k in range(3):
        src = finish_template(T[rnd.choice(["femur", "tibia", "humerus"])].copy(), "lang", KNOCHEN)
        d = Vector((rnd.uniform(-0.5, 0.5), rnd.uniform(-0.6, 0.6), 1.0))
        add(src, along_matrix(Vector((rnd.uniform(-0.25, 0.25), (k - 1) * 0.3, 0.42)), d, 1.6, rnd.uniform(0, 6)))
    sk = finish_template(T["schaedel_k"].copy(), "kopf", KNOCHEN)
    add(sk, facing_matrix(Vector((0.3, -0.36, 0.36)), Vector((0.6, -1, 0.3)), 1.5))
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    core.select_set(True)
    bpy.context.view_layer.objects.active = core
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = "SM_Narbenwall"
    for poly in obj.data.polygons:
        poly.use_smooth = True
    export(obj, FORM_OUT)


# Die Fab-Szene bleibt stehen (die Vorlagen kommen aus ihr); jedes Teil wird für sich ausgewählt und exportiert
broken_bones()
if os.environ.get("VAEL_NUR_SPLITTER"):          # nur die gebrochenen Knochen neu (schnell)
    log("fertig (nur Splitter)")
    sys.exit(0)
spell_bones()
skull_wall("SM_HK_Schaedelwand_A", 41)
skull_wall("SM_HK_Schaedelwand_B", 42)
bone_pillar("SM_HK_Knochensaeule", 61)
skull_arch("SM_HK_Schaedelbogen", 62)
skull_arch("SM_HK_Schaedelbogen_Breit", 63, span=7.4)
bone_pile("SM_HK_Knochenhaufen", 43)
scar_wall()
log("fertig")
