"""
Prüfbilder für die Mulden der Schürfameise: rendert die Ameise orthografisch aus sechs Richtungen und schreibt dazu, wo
jede Mulde aus mulden.json im Bild liegt (Pixel, Radius in Pixeln, sichtbar ja/nein). ameise_markieren.py zeichnet daraus
nummerierte Kreise, damit man jede Mulde einzeln prüfen kann.

  blender -b -P Tools/Ameisen/ameise_ansichten.py -- <ameise.fbx> <mulden.json> <Ausgabeordner>
"""
import json
import math
import os
import sys

import bpy
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
ANT, MULDEN, OUT = args[0], args[1], args[2]
RES = 1600
# Name: Blickrichtung (von wo die Kamera schaut)
ANSICHTEN = {
    "oben": Vector((0, 0, 1)),
    "links": Vector((0, 1, 0.15)),
    "rechts": Vector((0, -1, 0.15)),
    "vorne": Vector((1, 0, 0.3)),
    "hinten": Vector((-1, 0, 0.3)),
    "schraeg_links": Vector((0.5, 0.8, 0.8)),
    "schraeg_rechts": Vector((-0.5, -0.8, 0.8)),
    "oben_links": Vector((0, 0.7, 0.7)),
    "oben_rechts": Vector((0, -0.7, 0.7)),
    "oben_hinterleib": Vector((0.7, 0, 0.7)),
    "oben_kopf": Vector((-0.7, 0, 0.7)),
}


def load():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=ANT)
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
    obj.data.transform(Matrix.Scale(1.0 / size, 4) @ Matrix.Translation(-centre))
    obj.data.update()
    return obj


obj = load()
mulden = []
if os.path.exists(MULDEN):
    with open(MULDEN, encoding="utf-8") as f:
        mulden = json.load(f)["mulden"]
scn = bpy.context.scene
scn.render.engine = "BLENDER_EEVEE"
scn.render.resolution_x = scn.render.resolution_y = RES
world = bpy.data.worlds.new("W")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.3, 0.3, 0.33, 1)
scn.world = world
bpy.ops.object.light_add(type="SUN", rotation=(math.radians(40), 0, math.radians(30)))
bpy.context.object.data.energy = 3
bpy.ops.object.light_add(type="SUN", rotation=(math.radians(50), 0, math.radians(200)))
bpy.context.object.data.energy = 1.2
cam = bpy.data.cameras.new("C")
cam.type = "ORTHO"
cam.ortho_scale = 1.15
co = bpy.data.objects.new("C", cam)
scn.collection.objects.link(co)
scn.camera = co
ziel = Vector((0, 0, 0.32))
deps = bpy.context.evaluated_depsgraph_get()
info = {}
os.makedirs(OUT, exist_ok=True)
for name, d in ANSICHTEN.items():
    d = d.normalized()
    co.location = ziel + d * 3
    co.rotation_euler = (-d).to_track_quat("-Z", "Y" if abs(d.z) < 0.99 else "X").to_euler()
    bpy.context.view_layer.update()
    scn.render.filepath = os.path.join(OUT, name + ".png")
    bpy.ops.render.render(write_still=True)
    punkte = []
    for i, m in enumerate(mulden):
        p, n = Vector(m["p"]), Vector(m["n"])
        uv = world_to_camera_view(scn, co, p)
        # sichtbar, wenn die Mulde zur Kamera zeigt und nichts davor liegt
        richtung = (co.location - p)
        hit = scn.ray_cast(deps, p + n * (m.get("tiefe", 0.0) + 0.006), d)   # über dem eigenen Rand starten
        frei = (not hit[0]) and n.dot(d) > -0.1
        punkte.append({"i": i, "x": uv.x * RES, "y": (1 - uv.y) * RES, "r": m["r"] / cam.ortho_scale * RES, "frei": frei})
    info[name] = punkte
with open(os.path.join(OUT, "ansichten.json"), "w", encoding="utf-8") as f:
    json.dump(info, f)
print("ANSICHTEN", len(info), "MULDEN", len(mulden))
