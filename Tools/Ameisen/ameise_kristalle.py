"""
Setzt Erzkristalle in die leeren Fassungen der Schürfameise (Probebilder, Boris 26.11.: "selbst eine Kristallversion daraus
machen"). Die Mulden kommen aus ameise_mulden.py (mulden.json, gleiche Normalisierung: Länge 1 m, Füße auf Z = 0).

Als Kristall dient das echte Erz aus dem Spiel (Vael_Quellen/<Erz>/04_export/SM_<Erz>.fbx): Nur die Spitzen werden
behalten (obere Hälfte, wie ErzWand "A"), die Schnittfläche steckt in der Mulde.

  blender -b -P Tools/Ameisen/ameise_kristalle.py -- <ameise.fbx> <mulden.json> <Erz> <Anteil 0..1> <Größe> <Ausgabe-Präfix> [Saat]

Anteil: wie viele Mulden einen Kristall bekommen (junge Arbeiterin wenig, Ahnenpanzer alle).
Größe: 1 = Kristall füllt die Fassung, >1 = wächst über den Rand hinaus (älter).
Ausgabe: <Präfix>_0..2.png (drei Ansichten) und <Präfix>.blend.
"""
import json
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Matrix, Quaternion, Vector

args = sys.argv[sys.argv.index("--") + 1:]
ANT, MULDEN, ERZ, ANTEIL, GROESSE, OUT = args[0], args[1], args[2], float(args[3]), float(args[4]), args[5]
SAAT = int(args[6]) if len(args) > 6 else 1
QUELLEN = "C:/Projekte/Vael_Quellen"
SPITZEN_ANTEIL = 0.5      # welcher Teil der Höhe vom Erz wegfällt (Fels unten)
EINSENKEN = 0.3           # wie tief der Kristall in der Mulde sitzt (Anteil des Muldenradius)


def import_joined(path):
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    meshes = [o for o in bpy.context.scene.objects if o not in before and o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for o in list(bpy.context.scene.objects):
        if o in before or o == obj:
            continue
        if o.type != "MESH":
            bpy.data.objects.remove(o)
    return obj


def bounds(me):
    pts = [v.co for v in me.vertices]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


def ameise():
    obj = import_joined(ANT)
    lo, hi = bounds(obj.data)
    size = max(hi - lo)
    centre = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    obj.data.transform(Matrix.Scale(1.0 / size, 4) @ Matrix.Translation(-centre))
    obj.name = "Schuerfameise"
    return obj


def kristall():
    obj = import_joined("{}/{}/04_export/SM_{}.fbx".format(QUELLEN, ERZ, ERZ))
    me = obj.data
    lo, hi = bounds(me)
    me.transform(Matrix.Scale(1.0 / max(hi - lo), 4))
    lo, hi = bounds(me)
    z0 = lo.z + (hi.z - lo.z) * SPITZEN_ANTEIL
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(0, 0, z0), plane_no=(0, 0, 1),
                           clear_inner=True)
    bm.to_mesh(me)
    bm.free()
    lo, hi = bounds(me)
    me.transform(Matrix.Translation((-(lo.x + hi.x) / 2, -(lo.y + hi.y) / 2, -z0)))
    lo, hi = bounds(me)
    obj.name = "Kristall_" + ERZ
    # Breite am dichten Kern messen (75 % der Punkte liegen innerhalb), nicht an einzelnen weit abstehenden Spitzen
    ab = sorted(math.hypot(v.co.x, v.co.y) for v in me.vertices)
    breite = 2 * ab[int(len(ab) * 0.75)]
    hoehen = sorted(v.co.z for v in me.vertices)
    return obj, breite, hoehen[-1]


def setzen(vorlage, ort, n, breite_soll, hoehe_soll, breite, hoehe, rnd):
    k = vorlage.copy()
    bpy.context.scene.collection.objects.link(k)
    s = breite_soll / breite
    # Form des Erzes erhalten: höchstens leicht strecken oder stauchen
    z = s * min(1.4, max(1.0, hoehe_soll / (hoehe * s)))
    dreh = n.to_track_quat("Z", "Y") @ Quaternion((0, 0, 1), rnd.uniform(0, 2 * math.pi))
    neig = Quaternion(n.orthogonal().normalized(), math.radians(rnd.uniform(-10, 10)))
    k.matrix_world = Matrix.Translation(ort) @ (neig @ dreh).to_matrix().to_4x4() @ Matrix.Diagonal((s, s, z, 1))


def besetzen(vorlage, breite, hoehe, mulden):
    """Jede Fassung wird ausgefüllt: der Kristall steht auf dem Muldenboden, füllt den Ring innen ganz aus und wächst
    über den Rand hinaus (je älter, desto weiter). Große Fassungen bekommen zwei kleinere Nebenkristalle dazu."""
    rnd = random.Random(SAAT)
    # größere Mulden zuerst besetzen, dann zufällig: auch junge Ameisen tragen ihre Kristalle zuerst in den großen Fassungen
    reihe = sorted(mulden, key=lambda m: -m["r"] - rnd.uniform(0, 0.02))
    anzahl = max(1, round(len(reihe) * ANTEIL))
    alter = GROESSE - 1.0
    for m in reihe[:anzahl]:
        p, n, r = Vector(m["p"]), Vector(m["n"]).normalized(), m["r"]
        tiefe = m.get("tiefe", r * 0.3)
        t1 = n.orthogonal().normalized()
        t2 = n.cross(t1)
        fuss = p - n * r * 0.08
        # Druse: ein Mittelkristall und ein Kranz, die Zahl wächst mit der Fläche der Fassung
        kranz = max(0, min(7, round((r / 0.011) ** 2 * 0.6)))
        teile = [(Vector(), 0.0, 1.0)]
        start = rnd.uniform(0, 2 * math.pi)
        for i in range(kranz):
            a = start + 2 * math.pi * i / kranz + rnd.uniform(-0.3, 0.3)
            e = t1 * math.cos(a) + t2 * math.sin(a)
            teile.append((e * r * rnd.uniform(0.45, 0.65), rnd.uniform(15, 30), rnd.uniform(0.55, 0.8)))
        for off, kipp, groesse in teile:
            richtung = n if off.length == 0 else (n + off.normalized() * math.tan(math.radians(kipp))).normalized()
            weit = r * (1.9 if kranz else 2.4) * groesse * (1 + alter * 0.5) * rnd.uniform(0.85, 1.1)
            hoch = (tiefe + r * (0.9 + alter)) * groesse * rnd.uniform(0.8, 1.25)
            setzen(vorlage, fuss + off, richtung, weit, hoch, breite, hoehe, rnd)
    bpy.data.objects.remove(vorlage)
    print("KRISTALLE", anzahl, "von", len(mulden))


def bilder():
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_EEVEE"
    scn.render.resolution_x, scn.render.resolution_y = 1400, 1000
    world = bpy.data.worlds.new("W")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.32, 0.32, 0.35, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.8
    scn.world = world
    bpy.ops.object.light_add(type="SUN", rotation=(math.radians(50), 0, math.radians(30)))
    bpy.context.object.data.energy = 3
    bpy.ops.object.light_add(type="SUN", rotation=(math.radians(60), 0, math.radians(210)))
    bpy.context.object.data.energy = 1
    cam = bpy.data.cameras.new("C")
    co = bpy.data.objects.new("C", cam)
    scn.collection.objects.link(co)
    scn.camera = co
    cam.lens = 50
    for k, (az, el) in enumerate(((35, 30), (215, 30), (120, 55))):
        d = 2.2
        a, e = math.radians(az), math.radians(el)
        co.location = Vector((d * math.cos(a) * math.cos(e), d * math.sin(a) * math.cos(e), 0.2 + d * math.sin(e)))
        look = Vector((0, 0, 0.22)) - co.location
        co.rotation_euler = look.to_track_quat("-Z", "Y").to_euler()
        scn.render.filepath = "{}_{}.png".format(OUT, k)
        bpy.ops.render.render(write_still=True)


bpy.ops.wm.read_factory_settings(use_empty=True)
ameise()
with open(MULDEN, encoding="utf-8") as f:
    mulden = json.load(f)["mulden"]
vorlage, breite, hoehe = kristall()
besetzen(vorlage, breite, hoehe, mulden)
bilder()
bpy.ops.wm.save_as_mainfile(filepath=OUT + ".blend")
