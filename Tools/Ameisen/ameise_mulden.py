"""
Findet die leeren Fassungen (Mulden) im Panzer der Schürfameise (Tripo-Modell) und schreibt sie als JSON.

Eine Mulde ist eine Stelle, an der die Oberfläche ringsum in ALLE Richtungen höher liegt (entlang der Normalen).
Rillen (Taille, Gelenke) fallen raus, weil sie nur in eine Richtung ansteigen.

  blender -b -P Tools/Ameisen/ameise_mulden.py -- <ameise.fbx> <mulden.json> [vorschau.png]

JSON: Liste von {p: [x,y,z], n: [x,y,z], r: radius} in Metern, Modell normalisiert: Länge (größte Ausdehnung) = 1 m,
Füße auf Z = 0, Mitte auf X/Y = 0. Die Normalisierung steht als "norm" mit im JSON.
"""
import json
import math
import sys

import bpy
from mathutils import Matrix, Vector
from mathutils.kdtree import KDTree

args = sys.argv[sys.argv.index("--") + 1:]
SRC, OUT = args[0], args[1]
PREVIEW = args[2] if len(args) > 2 else None

SKALEN = (0.015, 0.022, 0.030, 0.040, 0.052)   # Suchradien (Anteil der Länge)
MIN_DICKE = 0.09                         # Körper muss unter der Mulde so dick sein (Beine/Fühler fallen raus)
MIN_Z = 0.15                             # nichts an Füßen und Unterschenkeln
MIN_NZ = -0.2                            # keine Mulden auf der Unterseite (sieht man aus der Iso-Kamera nicht)
SEKTOREN = 6
MIN_HUB = 0.10                           # jede Richtung muss im Mittel um mind. 10 % des Radius höher liegen


def load():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=SRC)
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    pts = [v.co for v in obj.data.vertices]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    size = max(hi - lo)
    centre = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    m = Matrix.Scale(1.0 / size, 4) @ Matrix.Translation(-centre)
    obj.data.transform(m)
    obj.data.update()
    print("DIMS", [round(x, 3) for x in (hi - lo)], "size", round(size, 3))
    return obj, {"scale": 1.0 / size, "centre": list(centre)}


def finden(obj):
    me = obj.data
    vs = me.vertices
    kd = KDTree(len(vs))
    for i, v in enumerate(vs):
        kd.insert(v.co, i)
    kd.balance()
    from mathutils.bvhtree import BVHTree
    bvh = BVHTree.FromPolygons([v.co.copy() for v in vs], [tuple(p.vertices) for p in me.polygons])
    kandidaten = []
    for i, v in enumerate(vs):
        if i % 2:          # jede zweite Ecke reicht als Mittelpunkt
            continue
        n = v.normal
        if n.length < 0.5 or v.co.z < MIN_Z or n.z < MIN_NZ:
            continue
        t1 = n.orthogonal().normalized()
        t2 = n.cross(t1)
        best = None
        for r in SKALEN:
            hood = kd.find_range(v.co, r)
            if len(hood) < 12:
                continue
            sek = [[] for _ in range(SEKTOREN)]
            for co, j, d in hood:
                if d < r * 0.45:
                    continue
                dv = co - v.co
                # nur dieselbe Seite der Oberfläche (keine Punkte von einem anderen Körperteil)
                if vs[j].normal.dot(n) < 0.2:
                    continue
                a = math.atan2(dv.dot(t2), dv.dot(t1))
                if a != a:
                    continue
                sek[int((a + math.pi) / (2 * math.pi) * SEKTOREN) % SEKTOREN].append(dv.dot(n))
            if any(len(s) < 2 for s in sek):
                continue
            hub = min(sum(s) / len(s) for s in sek) / r
            if hub > MIN_HUB:
                best = (hub, r)          # größter Radius, der noch eine Mulde ergibt
        if best:
            loc, nor, idx, dist = bvh.ray_cast(v.co - n * 0.002, -n)
            dicke = (loc - v.co).length if loc is not None else 0.0
            if dicke >= MIN_DICKE:
                kandidaten.append((best[1], best[0], i))
    kandidaten.sort(reverse=True)      # große Mulden zuerst
    mulden = []
    for r, hub, i in kandidaten:
        p = vs[i].co
        if any((p - Vector(m["p"])).length < max(r, m["r"]) * 1.5 for m in mulden):
            continue
        mulden.append({"p": list(p), "n": list(vs[i].normal), "r": r, "hub": round(hub, 3)})
    print("KANDIDATEN", len(kandidaten), "MULDEN", len(mulden))
    return mulden


def vorschau(obj, mulden, path):
    import bmesh
    mat = bpy.data.materials.new("Marker")
    mat.diffuse_color = (1, 0, 0, 1)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (1, 0, 0, 1)
    bsdf.inputs["Emission Color"].default_value = (1, 0, 0, 1)
    bsdf.inputs["Emission Strength"].default_value = 3.0
    for m in mulden:
        bpy.ops.mesh.primitive_uv_sphere_add(radius=m["r"] * 0.5, location=m["p"], segments=8, ring_count=6)
        bpy.context.object.data.materials.append(mat)
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_EEVEE"
    scn.render.resolution_x, scn.render.resolution_y = 1400, 1000
    world = bpy.data.worlds.new("W")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.35, 0.35, 0.38, 1)
    scn.world = world
    bpy.ops.object.light_add(type="SUN", rotation=(math.radians(50), 0, math.radians(30)))
    bpy.context.object.data.energy = 3
    cam = bpy.data.cameras.new("C")
    co = bpy.data.objects.new("C", cam)
    scn.collection.objects.link(co)
    scn.camera = co
    cam.lens = 50
    for k, (az, el) in enumerate(((35, 30), (215, 30), (90, 70))):
        d = 2.2
        a, e = math.radians(az), math.radians(el)
        co.location = Vector((d * math.cos(a) * math.cos(e), d * math.sin(a) * math.cos(e), 0.2 + d * math.sin(e)))
        look = Vector((0, 0, 0.22)) - co.location
        co.rotation_euler = look.to_track_quat("-Z", "Y").to_euler()
        scn.render.filepath = path.replace(".png", f"_{k}.png")
        bpy.ops.render.render(write_still=True)


obj, norm = load()
mulden = finden(obj)
with open(OUT, "w", encoding="utf-8") as f:
    json.dump({"norm": norm, "mulden": mulden}, f, indent=1)
if PREVIEW:
    vorschau(obj, mulden, PREVIEW)
