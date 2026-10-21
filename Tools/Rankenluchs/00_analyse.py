"""Shows what the Tripo GLB contains: objects, materials, textures, size and where the mass sits along each axis."""
import bpy, sys, os
from mathutils import Vector
ORDNER = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=os.path.join(ORDNER, "Rankenluchs_tripo.glb"))
for o in bpy.data.objects:
    print("OBJ", o.name, o.type, tuple(round(v, 3) for v in o.dimensions), [m.name for m in getattr(o.data, "materials", [])] if o.type == "MESH" else "")
for i in bpy.data.images:
    print("IMG", i.name, tuple(i.size))
for m in bpy.data.materials:
    print("MAT", m.name, [n.type for n in m.node_tree.nodes] if m.use_nodes else "")
o = next(o for o in bpy.data.objects if o.type == "MESH")
pts = [o.matrix_world @ v.co for v in o.data.vertices]
lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
print("BOX", tuple(round(v, 3) for v in lo), tuple(round(v, 3) for v in hi), "verts", len(pts), "faces", len(o.data.polygons))
# Height profile along X and Y: max Z per slice shows where the head (high) and the tail (low, thin) are
for axis in (0, 1):
    n = 12
    a, b = lo[axis], hi[axis]
    row = []
    for k in range(n):
        s = [p for p in pts if a + (b - a) * k / n <= p[axis] < a + (b - a) * (k + 1) / n]
        row.append((round(max(p.z for p in s), 2) if s else None, len(s)))
    print("AXIS", "XY"[axis], row)
