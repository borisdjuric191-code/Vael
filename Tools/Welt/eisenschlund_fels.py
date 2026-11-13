# Felsteile für die Schluchten des Eisenschlunds (Boris 13.11.2026: Höhlenstimmung wie in Diablo-Höhlen als Inspiration:
# hohe Felswände, Stalaktiten, Stalagmiten, Licht). Eigene Formen, nichts nachgebaut.
#
#   blender -b --factory-startup -P Tools/Welt/eisenschlund_fels.py -- C:/Projekte/Vael_Quellen/Eisenschlund/export
#
# Baut in Zentimetern (Ursprung unten in der Mitte, Stalaktiten oben in der Mitte) und exportiert je ein FBX:
# - SM_ES_Wand_A/B/C   Felswandstück, 24 m breit, 7 m tief, 36 m hoch, vorn mit waagerechten Schichten, Klüften und Absätzen
# - SM_ES_Stalagmit_A/B/C  wächst aus dem Boden, 1 m hoch (beim Setzen auf 2-9 m skaliert), unregelmäßig geringelt
# - SM_ES_Stalaktit_A/B    hängt von oben, 1 m lang, spitz und tropfnass
# - SM_ES_Bogen          Felsbogen über eine Schlucht, 34 m Spannweite, 20 m hoch, unten mit Zapfen
# Ohne UVs: das Material legt den Fels nach Weltposition (M_ES_Fels).

import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Vector, noise

OUT = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else "C:/Projekte/Vael_Quellen/Eisenschlund/export"
os.makedirs(OUT, exist_ok=True)


def log(message):
    print("[eisenschlund_fels] " + message)


def clear():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()
    for mesh in list(bpy.data.meshes):
        bpy.data.meshes.remove(mesh)


def new_object(name):
    mesh = bpy.data.meshes.new(name)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def finish(obj, bm):
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.05)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(obj.data)
    bm.free()
    for poly in obj.data.polygons:
        poly.use_smooth = True


def export(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    # Gebaut in Zentimetern, Blender rechnet in Metern: auf ein Hundertstel, sonst kommt alles 100-mal zu groß in Unreal an
    obj.scale = (0.01, 0.01, 0.01)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    path = os.path.join(OUT, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_ALL", bake_anim=False, mesh_smooth_type="FACE")
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    log("{}: {} Dreiecke".format(obj.name, tris))


def wall(name, seed):
    """Ein Wandstück: Gitter vorn (y = 0) mit Schichten und Klüften, nach hinten geschlossen"""
    rnd = random.Random(seed)
    w, d, h = 2400.0, 700.0, 3600.0
    nx, nz = 36, 54
    obj = new_object(name)
    bm = bmesh.new()
    grid = []
    for k in range(nz + 1):
        row = []
        z = h * k / nz
        for i in range(nx + 1):
            x = -w / 2 + w * i / nx
            p = Vector((x * 0.004, z * 0.004, seed * 3.1))
            strata = 45.0 * math.sin(z * 0.012 + noise.noise(Vector((x * 0.002, 0.0, seed))) * 2.0)      # Schichten
            ledges = 90.0 * max(0.0, noise.noise(Vector((x * 0.001, z * 0.0025, seed + 5.0))))           # Absätze
            crack = -160.0 * max(0.0, 1.0 - abs(noise.noise(Vector((x * 0.0012, z * 0.0006, seed + 9.0)))) * 9.0)  # Klüfte
            bulge = 180.0 * noise.noise(p * 0.5)
            y = -(strata + ledges + crack + bulge) - 60.0 * (1.0 - z / h)          # unten etwas vorstehend (Schutt)
            edge = min(i, nx - i) / 3.0
            y *= min(1.0, edge)                                                       # Seiten flach, damit Stücke aneinander passen
            row.append(bm.verts.new((x, y, z)))
        grid.append(row)
    back = [[bm.verts.new((v.co.x, d, v.co.z)) for v in (grid[0][0], grid[0][nx], grid[nz][nx], grid[nz][0])]]
    for k in range(nz):
        for i in range(nx):
            bm.faces.new((grid[k][i], grid[k][i + 1], grid[k + 1][i + 1], grid[k + 1][i]))
    b0, b1, b2, b3 = back[0]
    bm.faces.new((b0, b3, b2, b1))
    # Seiten, Boden und Deckel zur Rückseite schließen
    left = [grid[k][0] for k in range(nz + 1)]
    right = [grid[k][nx] for k in range(nz + 1)]
    bm.faces.new(left[::-1] + [b0, b3][::-1] if False else [b0] + left + [b3])
    bm.faces.new([b1, b2] + right[::-1])
    bm.faces.new([grid[0][i] for i in range(nx + 1)][::-1] + [b0, b1][::-1] if False else [b0, b1] + [grid[0][i] for i in range(nx, -1, -1)])
    bm.faces.new([grid[nz][i] for i in range(nx + 1)] + [b2, b3])
    finish(obj, bm)
    return obj


def spike(name, seed, hanging=False):
    """Ein Tropfstein: geringelter Kegel, oben (oder unten) spitz, mit Wülsten"""
    rnd = random.Random(seed)
    obj = new_object(name)
    bm = bmesh.new()
    rings, sides = 18, 14
    base = rnd.uniform(16.0, 24.0)
    rows = []
    for k in range(rings + 1):
        t = k / rings
        radius = base * (1.0 - t) ** rnd.uniform(0.9, 1.3) + 0.5
        radius *= 1.0 + 0.12 * math.sin(t * rnd.uniform(18, 26) + seed)                # Ringe vom Tropfen
        lean = Vector((math.sin(seed) * 6.0 * t, math.cos(seed * 1.3) * 6.0 * t, 0.0))
        z = 100.0 * t
        row = []
        for s in range(sides):
            a = 2 * math.pi * s / sides
            wob = 1.0 + 0.18 * noise.noise(Vector((math.cos(a) * 2.0, math.sin(a) * 2.0, t * 4.0 + seed)))
            p = Vector((math.cos(a) * radius * wob, math.sin(a) * radius * wob, z)) + lean
            if hanging:
                p.z = -p.z
            row.append(bm.verts.new(p))
        rows.append(row)
    for k in range(rings):
        for s in range(sides):
            a, b = rows[k][s], rows[k][(s + 1) % sides]
            c, e = rows[k + 1][(s + 1) % sides], rows[k + 1][s]
            bm.faces.new((a, b, c, e) if not hanging else (e, c, b, a))
    tip = bm.verts.new((rows[-1][0].co.x * 0.2, rows[-1][0].co.y * 0.2, (-104.0 if hanging else 104.0)))
    for s in range(sides):
        f = (rows[-1][s], rows[-1][(s + 1) % sides], tip)
        bm.faces.new(f if not hanging else f[::-1])
    cap = [rows[0][s] for s in range(sides)]
    bm.faces.new(cap[::-1] if not hanging else cap)
    finish(obj, bm)
    return obj


def arch(name, seed):
    """Felsbogen: ein dickes, verwittertes Band über 34 m, die Füße breiter; unten hängen kleine Zapfen"""
    obj = new_object(name)
    bm = bmesh.new()
    span, height, thick, depth = 3400.0, 2000.0, 420.0, 700.0
    steps, sides = 40, 10
    rows = []
    for k in range(steps + 1):
        t = k / steps
        a = math.pi * t
        cx = -span / 2 * math.cos(a)
        cz = height * math.sin(a) ** 0.8
        foot = 1.0 + 0.9 * (1.0 - math.sin(a)) ** 2
        row = []
        for s in range(sides):
            b = 2 * math.pi * s / sides
            off = Vector((-math.sin(a) * math.cos(b) * thick * foot * 0.5, math.sin(b) * depth * foot * 0.5, math.cos(a) * math.cos(b) * thick * foot * 0.5))
            p = Vector((cx, 0.0, cz)) + off
            p += noise.noise_vector(p * 0.002 + Vector((seed, 0, 0))) * 90.0
            row.append(bm.verts.new(p))
        rows.append(row)
    for k in range(steps):
        for s in range(sides):
            bm.faces.new((rows[k][s], rows[k][(s + 1) % sides], rows[k + 1][(s + 1) % sides], rows[k + 1][s]))
    bm.faces.new([rows[0][s] for s in range(sides)][::-1])
    bm.faces.new([rows[-1][s] for s in range(sides)])
    finish(obj, bm)
    return obj


def main():
    clear()
    pieces = [wall("SM_ES_Wand_A", 1), wall("SM_ES_Wand_B", 2), wall("SM_ES_Wand_C", 3),
              spike("SM_ES_Stalagmit_A", 11), spike("SM_ES_Stalagmit_B", 12), spike("SM_ES_Stalagmit_C", 13),
              spike("SM_ES_Stalaktit_A", 21, hanging=True), spike("SM_ES_Stalaktit_B", 22, hanging=True),
              arch("SM_ES_Bogen", 31)]
    for obj in pieces:
        export(obj)
    log("fertig: {} Teile nach {}".format(len(pieces), OUT))


main()
