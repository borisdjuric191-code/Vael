"""Shapes of the Tropfseuche, shared by the Schleimgaenger script and the clump assets (Boris 2026-11-21: thick slime in
patches, lumps on the body, a glowing heart between the ribs held by a web of slime threads).

All shapes are plain mesh data in metres; the callers place them, give them weights and materials.
"""
import math
import random

import bmesh
import bpy
from mathutils import Vector, noise


def klumpen(name, radius=1.0, flach=0.6, saat=0, unterteilung=3, beulen=0.35):
    """A lumpy blob of slime: an icosphere pushed in and out by noise, flattened along Z (Z = away from the body),
    sagging a little downwards like something heavy and wet. Returns a new mesh object (not linked)."""
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=unterteilung, radius=radius)
    versatz = Vector((saat * 3.7, saat * 1.3, saat * 5.1))
    for v in bm.verts:
        n = v.co.normalized()
        beule = noise.noise(n * 1.8 + versatz) * beulen + noise.noise(n * 4.5 + versatz) * beulen * 0.35
        v.co = n * radius * (1.0 + beule)
        v.co.z *= flach
        # heavy: the lower half hangs a little, like a drop about to fall
        if v.co.y < 0:
            v.co.y *= 1.0 + 0.25 * min(1.0, -v.co.y / radius)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    return bpy.data.objects.new(name, me)


def faden(name, a, b, dicke_a, dicke_b, durchhang=0.0, seiten=6, stuecke=10, zittern=0.0, saat=0):
    """A slime thread from a to b: a tube that tapers, sags by durchhang metres in the middle and wanders a little.
    Returns a new mesh object (not linked)."""
    a, b = Vector(a), Vector(b)
    bm = bmesh.new()
    ringe = []
    rng = random.Random(saat)
    quer_zufall = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1)))
    for i in range(stuecke + 1):
        t = i / stuecke
        p = a.lerp(b, t)
        p.z -= durchhang * 4 * t * (1 - t)
        p += quer_zufall * zittern * math.sin(math.pi * t)
        # thicker at both ends where it sticks on, thin in the middle
        r = (dicke_a * (1 - t) + dicke_b * t) * (0.65 + 0.35 * abs(2 * t - 1) ** 0.7)
        richtung = (b - a).normalized()
        u = richtung.cross(Vector((0, 0, 1)))
        if u.length < 1e-3:
            u = richtung.cross(Vector((1, 0, 0)))
        u.normalize()
        w = richtung.cross(u)
        ringe.append([bm.verts.new(p + (u * math.cos(2 * math.pi * k / seiten) + w * math.sin(2 * math.pi * k / seiten)) * r)
                      for k in range(seiten)])
    for i in range(stuecke):
        for k in range(seiten):
            bm.faces.new((ringe[i][k], ringe[i][(k + 1) % seiten], ringe[i + 1][(k + 1) % seiten], ringe[i + 1][k]))
    bm.faces.new(ringe[0][::-1])
    bm.faces.new(ringe[-1])
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    return bpy.data.objects.new(name, me)
