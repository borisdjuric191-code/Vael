"""
Soft shapes for the Eisenschlund world instead of blockout primitives (Boris 2026-11-22, point 5: "Simple Formen wie das Wasser
zu richtigem Wasser ausbauen statt der viereckigen Form").

Two unit pieces that drop into the transforms of the old blockout actors, so the world layout stays as it is:
- SM_ES_Suhle: an uneven mud puddle with a lumpy, rounded shore. Replaces the flat cylinder (100 cm across, 100 cm tall,
  pivot in the middle; the old actor scale of x 5-9 m, z 0.15 carries over).
- SM_ES_Glutstrom: one stretch of the slag river with a wavy bank and tapered, rounded ends. Replaces the stretched cube of each
  segment (100 x 100 x 100 cm, pivot in the middle); the segments overlap by 4 m, so the tapered ends run into each other.

Exported in centimetres like hoehlen_wasser.py to C:/Projekte/Vael_Quellen/Hoehlen/export/.

    blender -b --factory-startup -P Tools/Welt/eisenschlund_formen.py
"""
import math
import os
import random

import bmesh
import bpy

EXPORT = "C:/Projekte/Vael_Quellen/Hoehlen/export"


def wobble(rng, n_waves):
    """A smooth closed noise around the circle: a few sine waves with random phase"""
    waves = [(k, rng.uniform(0.04, 0.12) / k ** 0.6, rng.uniform(0, 2 * math.pi)) for k in range(2, 2 + n_waves)]
    return lambda a: 1.0 + sum(amp * math.sin(k * a + ph) for k, amp, ph in waves)


def disc(bm, outline, rings, top, bottom, dome):
    """Filled shape from an outline function (angle -> (x, y)), with a domed top that falls off softly to the rim.
    top/bottom are the z of the middle and the rim; rings are concentric loops from the middle to the rim."""
    segs = 64
    centre = bm.verts.new((0.0, 0.0, top))
    loops = []
    for i in range(1, rings + 1):
        t = i / rings
        z = bottom + (top - bottom) * (1.0 - t ** dome)
        loop = []
        for s in range(segs):
            x, y = outline(2 * math.pi * s / segs)
            loop.append(bm.verts.new((x * t, y * t, z)))
        loops.append(loop)
    for s in range(segs):
        bm.faces.new((centre, loops[0][s], loops[0][(s + 1) % segs]))
    for a, b in zip(loops, loops[1:]):
        for s in range(segs):
            bm.faces.new((a[s], b[s], b[(s + 1) % segs], a[(s + 1) % segs]))
    # the underside closes the piece (flat at the bottom), so it reads as solid from low angles too
    under = bm.verts.new((0.0, 0.0, -50.0))
    rim = loops[-1]
    for s in range(segs):
        bm.faces.new((under, rim[(s + 1) % segs], rim[s]))


def export(name, bm, tiles_u, tiles_v):
    """tiles: texture repeats across the 100 cm piece along X and Y (the actor scale stretches the piece, so the river,
    stretched ~4x more along X, gets ~4x more repeats there and the texture stays square)"""
    uv = bm.loops.layers.uv.new("UV")
    for face in bm.faces:
        for loop in face.loops:
            co = loop.vert.co
            loop[uv].uv = ((co.x / 100.0 + 0.5) * tiles_u, (co.y / 100.0 + 0.5) * tiles_v)
    mesh = bpy.data.meshes.new(name)
    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()
    for poly in mesh.polygons:
        poly.use_smooth = True
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    os.makedirs(EXPORT, exist_ok=True)
    path = os.path.join(EXPORT, name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_ALL", bake_anim=False, mesh_smooth_type="FACE")
    print("[Formen] {} -> {} ({} faces)".format(name, path, len(mesh.polygons)))


bpy.ops.wm.read_factory_settings(use_empty=True)
rng = random.Random(2311)

# the puddle: lumpy round shore, nearly flat, a thin rounded lip; 100 cm across, top at +50 like the cylinder it replaces
w = wobble(rng, 5)
bm = bmesh.new()
disc(bm, lambda a: (50.0 * w(a) * math.cos(a), 50.0 * w(a) * math.sin(a)), rings=6, top=20.0, bottom=10.0, dome=6.0)   # nearly flat: a wallow lies in the ground, not on it
export("SM_ES_Suhle", bm, 2.0, 2.0)

# a stretch of slag river: long along X (-50..50), the banks wave, the ends taper to a round point
w1, w2 = wobble(rng, 4), wobble(rng, 4)


def strom(a):
    x = 50.0 * math.cos(a)
    # half width: full in the middle, narrowing over the last fifth toward both ends
    end = min(1.0, (50.0 - abs(x)) / 12.0 + 0.25)
    half = 50.0 * (w1(a) if math.sin(a) >= 0 else w2(a)) * end
    return x, half * (1.0 if math.sin(a) >= 0 else -1.0) * abs(math.sin(a)) ** 0.35


bm = bmesh.new()
disc(bm, strom, rings=5, top=50.0, bottom=20.0, dome=5.0)
export("SM_ES_Glutstrom", bm, 8.0, 2.0)
