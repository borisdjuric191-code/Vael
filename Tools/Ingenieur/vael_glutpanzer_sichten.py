"""Lists the parts of the Tripo export of the Glutpanzer (C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/02_tripo/Glutpanzer.fbx):
name, triangles, bounding box (in metres, Z up after import), written to 03_blender/teile.json.
  blender -b --factory-startup -P vael_glutpanzer_sichten.py"""
import bpy, json, os
from mathutils import Vector
SRC = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/02_tripo/Glutpanzer.fbx"
OUT = "C:/Projekte/Vael_Quellen/Ingenieur/Ruestung/03_blender"
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC)
parts = []
lo_all, hi_all = Vector((1e9,) * 3), Vector((-1e9,) * 3)
for o in bpy.data.objects:
    if o.type != "MESH":
        continue
    corners = [o.matrix_world @ Vector(c) for c in o.bound_box]
    lo = Vector([min(c[i] for c in corners) for i in range(3)])
    hi = Vector([max(c[i] for c in corners) for i in range(3)])
    lo_all = Vector([min(lo[i], lo_all[i]) for i in range(3)]); hi_all = Vector([max(hi[i], hi_all[i]) for i in range(3)])
    tris = sum(len(p.vertices) - 2 for p in o.data.polygons)
    parts.append({"name": o.name, "tris": tris, "lo": [round(v, 3) for v in lo], "hi": [round(v, 3) for v in hi],
                  "mid": [round((lo[i] + hi[i]) / 2, 3) for i in range(3)], "mats": [m.name for m in o.data.materials if m]})
json.dump({"total_lo": list(lo_all), "total_hi": list(hi_all), "parts": parts}, open(os.path.join(OUT, "teile.json"), "w"), indent=1)
print("SICHTEN total", [round(v, 3) for v in lo_all], [round(v, 3) for v in hi_all], len(parts), "parts", sum(p["tris"] for p in parts), "tris")
for p in sorted(parts, key=lambda p: -p["mid"][2]):
    print("SICHTEN {:<28} {:>7} mid {} size {}".format(p["name"], p["tris"], p["mid"], [round(p["hi"][i] - p["lo"][i], 3) for i in range(3)]))
