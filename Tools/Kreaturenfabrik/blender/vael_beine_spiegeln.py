"""Gives a Tripo creature the same legs on both sides by mirroring only the legs of one side (Distelspinne: Tripo made
4 legs on +X and 3 on -X). The body, the head and everything near the middle stay as Tripo made them; only geometry
of the legs region is replaced: below the body, or outside its radius up to the knees, and away from the middle.

Leg region (on one side):  |x| > MIDDLE  and  z < KNEE  and  (z < UNDER_BODY  or  distance from the up axis > BODY_RADIUS)
The legs region of the target side is deleted, the one of the source side copied, mirrored in X (UVs kept, so the
texture fits) and joined. Writes <Ordner>/02_tripo/<name>_beine_gespiegelt.glb next to the source, so the requisite
or creature step can use it, plus orthographic renders in 03_analyse/.

    blender -b --factory-startup -P vael_beine_spiegeln.py -- <tripo.glb> <+x|-x as source> [middle under_body body_radius knee]
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector


def main(path, source, middle=0.10, under_body=0.22, body_radius=0.27, knee=0.48):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    obj = next(o for o in bpy.data.objects if o.type == "MESH")
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    sign = 1.0 if source == "+x" else -1.0

    def in_legs(co, side):
        return side * co.x > middle and co.z < knee and (co.z < under_body or math.hypot(co.x, co.y) > body_radius)

    bm = bmesh.new()
    bm.from_mesh(obj.data)
    uv = bm.loops.layers.uv.active
    target = [f for f in bm.faces if all(in_legs(v.co, -sign) for v in f.verts)]
    source_faces = [f for f in bm.faces if all(in_legs(v.co, sign) for v in f.verts)]
    print("[spiegeln] faces: delete {} on the target side, copy {} from the source side".format(len(target), len(source_faces)))

    # Copy the source legs, mirrored in X; the mirror flips the winding, so the face order is reversed
    new_verts = {}
    copied = 0
    for face in source_faces:
        verts = []
        for v in face.verts:
            if v.index not in new_verts:
                new_verts[v.index] = bm.verts.new(Vector((-v.co.x, v.co.y, v.co.z)))
            verts.append(new_verts[v.index])
        loops_uv = [l[uv].uv.copy() for l in face.loops] if uv else None
        try:
            nf = bm.faces.new(list(reversed(verts)))
        except ValueError:
            continue
        nf.material_index = face.material_index
        if uv:
            for loop, value in zip(nf.loops, reversed(loops_uv)):
                loop[uv].uv = value
        copied += 1
    bmesh.ops.delete(bm, geom=target, context="FACES")
    # Vertices left alone by the deleted faces
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.normal_update()
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    print("[spiegeln] copied {} faces, mesh now {} faces".format(copied, len(obj.data.polygons)))

    base = os.path.splitext(path)[0]
    out = base + "_beine_gespiegelt.glb"
    bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=True)
    print("[spiegeln] written", out)

    # Orthographic renders to check the legs
    folder = os.path.join(os.path.dirname(os.path.dirname(path)), "03_analyse")
    os.makedirs(folder, exist_ok=True)
    pts = [v.co for v in obj.data.vertices]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    centre, size = (lo + hi) / 2, max(hi.x - lo.x, hi.y - lo.y, hi.z - lo.z) * 1.1
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x = scene.render.resolution_y = 900
    views = (("oben", (centre.x, centre.y, hi.z + 5), (0, 0, 0)),
             ("vorne", (centre.x, lo.y - 5, centre.z), (math.radians(90), 0, 0)),
             ("schraeg", (centre.x + 2.2, lo.y - 2.2, hi.z + 2.0), (math.radians(60), 0, math.radians(45))),
             ("unten", (centre.x, centre.y, lo.z - 5), (math.radians(180), 0, 0)))
    for name, loc, rot in views:
        cam = bpy.data.objects.new(name, bpy.data.cameras.new(name))
        cam.data.type = "ORTHO"
        cam.data.ortho_scale = size * (1.3 if name == "schraeg" else 1.0)
        cam.location = loc
        cam.rotation_euler = rot
        scene.collection.objects.link(cam)
        scene.camera = cam
        scene.render.filepath = os.path.join(folder, "gespiegelt_" + name + ".png")
        bpy.ops.render.render(write_still=True)
    print("[spiegeln] renders in", folder)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:]
    main(args[0], args[1], *[float(a) for a in args[2:]])
