"""Moderglocke: the small meshes of its spores and its poison, built here and exported for Unreal.

  SM_Moderglocke_Spore         a burr: a small ball with uneven barbs, drifts out of the cap by the dozen
  SM_Moderglocke_Gifttropfen   a drop of the cap's ichor with a thread drawn out behind it (falls along -Z)
  SM_Moderglocke_Giftlache     what a drop leaves on the ground: a flat blot with a ragged edge

Every mesh is about one metre large (100 cm in Unreal), the game scales the pieces. Written to
Vael_Quellen/Moderglocke/export/. The game colours them (M_Moderglocke_Effekt, made by 07_unreal_nachtrag.py).

    blender -b --factory-startup -P Tools/Nachtwesen/moderglocke_effekte.py
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Vector, noise

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import gemein  # noqa: E402

NAME = "Moderglocke"


def neu(name, bm):
    mesh = bpy.data.meshes.new(name)
    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()
    for p in mesh.polygons:
        p.use_smooth = True
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def spore():
    """A ball of 0.6 m with barbs: some corners pulled far out, the rest dented by noise"""
    random.seed(7)
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=2, radius=0.3)
    for v in bm.verts:
        n = v.co.normalized()
        dorn = 0.2 * random.random() if random.random() < 0.3 else 0.0
        v.co = n * (0.3 + dorn + 0.05 * noise.noise(n * 4.0))
    return neu("SM_%s_Spore" % NAME, bm)


def tropfen():
    """A drop: round and heavy below, drawn out to a thread above; 1 m long, the heavy end at z = 0"""
    bm = bmesh.new()
    ringe, ecken = 9, 10
    profil = [(0.0, 0.0), (0.09, 0.02), (0.15, 0.08), (0.16, 0.16), (0.13, 0.27), (0.08, 0.42), (0.04, 0.6), (0.018, 0.8), (0.0, 1.0)]
    reihen = []
    for r, z in profil:
        if r == 0.0:
            reihen.append([bm.verts.new((0.0, 0.0, z))])
        else:
            reihen.append([bm.verts.new((r * math.cos(2 * math.pi * e / ecken) * (1 + 0.12 * math.sin(3 * e + z * 9)),
                                         r * math.sin(2 * math.pi * e / ecken), z)) for e in range(ecken)])
    for a, b in zip(reihen, reihen[1:]):
        for e in range(ecken):
            e2 = (e + 1) % ecken
            if len(a) == 1:
                bm.faces.new((a[0], b[e2], b[e]))
            elif len(b) == 1:
                bm.faces.new((a[e], a[e2], b[0]))
            else:
                bm.faces.new((a[e], a[e2], b[e2], b[e]))
    assert len(reihen) == ringe
    return neu("SM_%s_Gifttropfen" % NAME, bm)


def lache():
    """A blot of 1 m: a low dome with a ragged, lobed edge and a few fingers running out"""
    random.seed(3)
    bm = bmesh.new()
    ecken = 48
    mitte = bm.verts.new((0.0, 0.0, 0.035))
    innen, rand = [], []
    # a round blot whose edge bulges softly in a few uneven places and runs out in two short tongues: round lobes of
    # different width at uneven angles, nothing pointed (points read as a leaf or a star)
    lappen = ((0.4, 0.1, 0.5), (1.9, 0.07, 0.35), (2.6, 0.17, 0.22), (4.1, 0.09, 0.6), (5.3, 0.14, 0.26))
    for e in range(ecken):
        w = 2 * math.pi * e / ecken
        r = 0.3 + 0.012 * math.sin(7 * w + 2.0) + 0.01 * random.random()
        for wo, hoch, breit in lappen:
            d = (w - wo + math.pi) % (2 * math.pi) - math.pi
            r += hoch * math.exp(-(d / breit) ** 2)
        innen.append(bm.verts.new((0.6 * r * math.cos(w), 0.6 * r * math.sin(w), 0.03)))
        rand.append(bm.verts.new((r * math.cos(w), r * math.sin(w), 0.0)))
    for e in range(ecken):
        e2 = (e + 1) % ecken
        bm.faces.new((mitte, innen[e], innen[e2]))
        bm.faces.new((innen[e], rand[e], rand[e2], innen[e2]))
    return neu("SM_%s_Giftlache" % NAME, bm)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    os.makedirs(gemein.ordner(NAME, "export"), exist_ok=True)
    for bauen in (spore, tropfen, lache):
        obj = bauen()
        for o in bpy.context.scene.objects:
            o.select_set(o is obj)
        bpy.context.view_layer.objects.active = obj
        ziel = gemein.ordner(NAME, "export", obj.name + ".fbx")
        bpy.ops.export_scene.fbx(filepath=ziel, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                                 apply_scale_options="FBX_SCALE_ALL", mesh_smooth_type="FACE", bake_anim=False)
        gemein.log(NAME, "Effekt-Mesh:", ziel, "Dreiecke:", sum(len(p.vertices) - 2 for p in obj.data.polygons))


main()
