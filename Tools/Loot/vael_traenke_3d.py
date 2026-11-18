"""
Vael - Traenke als 3D-Flaschen nach ihren gemalten Bildern (Boris 17.11.)
=========================================================================

Baut einen Trank nach seinem gemalten Bild (Vael_Quellen/Icons/gemalt/T_Icon_<Name>.png): das Profil des Glases wird aus dem
Bild gelesen (je Bildzeile die halbe Breite des Glases, die schmalere Seite, damit Anhaenger nicht mitzaehlen) und gedreht;
eine echte Glaswand mit Dicke, darin die Fluessigkeit als eigener Koerper bis zur Fuellhoehe (eigenes Material, im Spiel
spaeter mit sinkendem Fuellstand), Luftblasen darin, dazu Korken, Siegel, Schnur, Anhaenger und Beschlaege, wie das Bild sie
zeigt. Die Flasche ist 22 cm hoch.

Erste Probe: Heiltrank, Nidtrunk, Manatrank. Ausgabe je Trank SM_Trank_<Name>.fbx und ein Bild neben dem gemalten.

Aufruf:
  blender -b --factory-startup -P vael_traenke_3d.py -- <Zielordner> [Name ...]
"""
import math
import os
import random
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

ICONS = "C:/Projekte/Vael_Quellen/Icons/gemalt"
ARGS = sys.argv[sys.argv.index("--") + 1:]
OUT = ARGS[0]
ONLY = set(ARGS[1:])
HEIGHT = 0.22  # metres of the whole bottle with its cork


# ---------------------------------------------------------------- reading the picture

def profile_from_picture(name, top_cut, bottom_cut, rows=60):
    """Half widths of the glass from top to bottom, from the cut-out picture. top_cut/bottom_cut: the part of the picture's
    height (0 top .. 1 bottom of the thing) where the glass starts and ends; returns [(radius, height 0..1)] bottom first,
    in units of the glass's height"""
    img = bpy.data.images.load(os.path.join(ICONS, "T_Icon_" + name + ".png"))
    w, h = img.size
    a = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1, :, 3]
    bpy.data.images.remove(img)
    solid = a > 0.5
    ys = np.nonzero(solid.any(axis=1))[0]
    y0, y1 = ys.min(), ys.max()
    g0 = int(y0 + (y1 - y0) * top_cut)
    g1 = int(y0 + (y1 - y0) * bottom_cut)
    # the middle of the glass: the middle of the widest rows of its lower half
    lower = [y for y in range(g0 + (g1 - g0) // 2, g1)]
    mids = []
    for y in lower:
        xs = np.nonzero(solid[y])[0]
        if len(xs):
            mids.append((xs.min() + xs.max()) / 2)
    cx = float(np.median(mids))
    out = []
    span = g1 - g0
    for k in range(rows + 1):
        y = int(g1 - span * k / rows)
        row = solid[y]
        # walk out from the middle on both sides while there is glass, take the shorter side
        left = right = 0
        x = int(cx)
        while x - left > 0 and row[x - left]:
            left += 1
        while x + right < w - 1 and row[x + right]:
            right += 1
        out.append((min(left, right) / span, k / rows))
    return out


def flask(body_r, body_z, neck_r, neck_top, lip=0.025, foot=0.5, shoulder=58.0):
    """A round flask: a ball body standing on a flat foot, a neck, a lip; (radius, z) bottom first, z in units of the glass"""
    pts = [(body_r * foot, 0.0)]
    a0 = -math.degrees(math.acos(foot))
    for a in np.linspace(a0, shoulder, 28):
        r = body_r * math.cos(math.radians(a))
        pts.append((r, body_z + body_r * math.sin(math.radians(a))))
    z_sh = body_z + body_r * math.sin(math.radians(shoulder))
    pts += [(neck_r * 1.15, z_sh + 0.04), (neck_r, z_sh + 0.09), (neck_r, neck_top - lip * 1.6), (neck_r + lip, neck_top - lip), (neck_r + lip, neck_top),
            (neck_r + lip * 0.4, neck_top + lip * 0.3)]
    return pts


def smooth(profile, passes=2):
    p = [list(v) for v in profile]
    for _ in range(passes):
        q = [p[0]] + [[(p[i - 1][0] + 2 * p[i][0] + p[i + 1][0]) / 4, p[i][1]] for i in range(1, len(p) - 1)] + [p[-1]]
        p = q
    return [tuple(v) for v in p]


# ---------------------------------------------------------------- scene and materials

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    s = bpy.context.scene
    s.render.engine = "CYCLES"
    s.cycles.samples = 96
    s.cycles.use_denoising = True
    s.render.film_transparent = True
    s.render.resolution_x = s.render.resolution_y = 512
    s.view_settings.view_transform = "Standard"
    s.view_settings.exposure = -0.2
    world = bpy.data.worlds.new("W")
    s.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.08, 0.07, 0.065, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.45


def mat(name, color, rough=0.5, metal=0.0, transmission=0.0, emit=0.0, emit_color=None, bump=0.0, bump_scale=60.0, ior=1.45, coat=0.0, alpha=1.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    b.inputs["Transmission Weight"].default_value = transmission
    b.inputs["IOR"].default_value = ior
    b.inputs["Coat Weight"].default_value = coat
    b.inputs["Alpha"].default_value = alpha
    if emit:
        b.inputs["Emission Color"].default_value = (*(emit_color or color), 1)
        b.inputs["Emission Strength"].default_value = emit
    if bump:
        tex = nt.nodes.new("ShaderNodeTexNoise")
        tex.inputs["Scale"].default_value = bump_scale
        tex.inputs["Detail"].default_value = 10.0
        bn = nt.nodes.new("ShaderNodeBump")
        bn.inputs["Strength"].default_value = bump
        bn.inputs["Distance"].default_value = 0.004
        nt.links.new(tex.outputs["Fac"], bn.inputs["Height"])
        nt.links.new(bn.outputs["Normal"], b.inputs["Normal"])
        mix = nt.nodes.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.inputs["A"].default_value = (*[c * 0.65 for c in color], 1)
        mix.inputs["B"].default_value = (*[min(1, c * 1.3) for c in color], 1)
        nt.links.new(tex.outputs["Fac"], mix.inputs["Factor"])
        nt.links.new(mix.outputs["Result"], b.inputs["Base Color"])
    return m


def glass(tint=(0.92, 0.95, 0.96), rough=0.03):
    return mat("Glas", tint, rough=rough, transmission=1.0, ior=1.45)


def liquid(color, glow, density=6.0):
    """A liquid with depth: clear surface, the colour soaks in with the thickness (volume absorption), a little glow"""
    m = mat("Fluessigkeit", color, rough=0.04, transmission=1.0, emit=glow, ior=1.33)
    nt = m.node_tree
    out = next(n for n in nt.nodes if n.type == "OUTPUT_MATERIAL")
    absorb = nt.nodes.new("ShaderNodeVolumeAbsorption")
    absorb.inputs["Color"].default_value = (*color, 1)
    absorb.inputs["Density"].default_value = density
    nt.links.new(absorb.outputs[0], out.inputs["Volume"])
    return m


# ---------------------------------------------------------------- shapes

def link(name, me, material, smooth_=True):
    o = bpy.data.objects.new(name, me)
    o.data.materials.append(material)
    bpy.context.scene.collection.objects.link(o)
    if smooth_:
        for p in me.polygons:
            p.use_smooth = True
    return o


def lathe(name, profile, material, sides=64, cap_top=False, cap_bottom=True, twist=0.0):
    """Turns (radius, z) points bottom to top round Z"""
    bm = bmesh.new()
    rings = []
    for r, z in profile:
        rings.append([bm.verts.new((math.cos(2 * math.pi * i / sides + twist) * r, math.sin(2 * math.pi * i / sides + twist) * r, z)) for i in range(sides)])
    for a, b in zip(rings, rings[1:]):
        for i in range(sides):
            bm.faces.new((a[i], a[(i + 1) % sides], b[(i + 1) % sides], b[i]))
    if cap_bottom:
        c = bm.verts.new((0, 0, profile[0][1]))
        for i in range(sides):
            bm.faces.new((rings[0][(i + 1) % sides], rings[0][i], c))
    if cap_top:
        c = bm.verts.new((0, 0, profile[-1][1]))
        for i in range(sides):
            bm.faces.new((rings[-1][i], rings[-1][(i + 1) % sides], c))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    return link(name, me, material, smooth_=sides > 8)


def glass_body(profile, material, wall=0.012, sides=64, twist=0.0):
    """The glass as a shell: the outer surface, the lip, the inner surface; open at the top for the cork"""
    outer = profile
    inner = [(max(r - wall, 0.002), z) for r, z in profile]
    inner[0] = (inner[0][0], profile[0][1] + wall * 1.6)
    shell = outer + [(inner[-1][0], outer[-1][1])] + list(reversed(inner))
    bm = bmesh.new()
    rings = [[bm.verts.new((math.cos(2 * math.pi * i / sides + twist) * r, math.sin(2 * math.pi * i / sides + twist) * r, z)) for i in range(sides)] for r, z in shell]
    for a, b in zip(rings, rings[1:]):
        for i in range(sides):
            bm.faces.new((a[i], a[(i + 1) % sides], b[(i + 1) % sides], b[i]))
    for ring, up in ((rings[0], False), (rings[-1], True)):
        c = bm.verts.new((0, 0, ring[0].co.z))
        for i in range(sides):
            bm.faces.new((ring[i], ring[(i + 1) % sides], c) if up else (ring[(i + 1) % sides], ring[i], c))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new("Glas")
    bm.to_mesh(me)
    bm.free()
    return link("Glas", me, material, smooth_=sides > 8), inner


def fill(inner, level, material, sides=64, shrink=0.002, twist=0.0):
    """The liquid: the inner surface up to the level (0..1 of the inner height), a flat top"""
    z0, z1 = inner[0][1], inner[-1][1]
    top = z0 + (z1 - z0) * level
    pts = [(max(r - shrink, 0.001), z) for r, z in inner if z <= top]
    # the radius at the level
    for (ra, za), (rb, zb) in zip(inner, inner[1:]):
        if za <= top <= zb:
            t = (top - za) / max(zb - za, 1e-6)
            pts.append((max(ra + (rb - ra) * t - shrink, 0.001), top))
            break
    return lathe("Fluessigkeit", pts, material, sides=sides, cap_top=True, twist=twist), top


def bubbles(inner, top, material, count, seed=1, radius=0.006):
    random.seed(seed)
    z0 = inner[0][1]
    for k in range(count):
        z = random.uniform(z0 + 0.02, top - 0.01)
        r_here = float(np.interp(z, [zz for _, zz in inner], [r for r, _ in inner])) * 0.75
        a = random.uniform(0, 2 * math.pi)
        d = random.uniform(0, r_here)
        bm = bmesh.new()
        rr = radius * random.uniform(0.4, 1.3)
        bmesh.ops.create_uvsphere(bm, u_segments=12, v_segments=8, radius=rr)
        bmesh.ops.translate(bm, vec=(math.cos(a) * d, math.sin(a) * d, z), verts=bm.verts)
        me = bpy.data.meshes.new("Blase")
        bm.to_mesh(me)
        bm.free()
        link("Blase%d" % k, me, material)


def tube(name, points, radius, material, radii=None, cyclic=False):
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = radius
    cu.bevel_resolution = 4
    cu.resolution_u = 10
    sp = cu.splines.new("NURBS")
    sp.points.add(len(points) - 1)
    for p, co in zip(sp.points, points):
        p.co = (*co, 1.0)
    if radii:
        for p, r in zip(sp.points, radii):
            p.radius = r
    sp.use_endpoint_u = not cyclic
    sp.use_cyclic_u = cyclic
    sp.order_u = 3
    o = bpy.data.objects.new(name, cu)
    o.data.materials.append(material)
    bpy.context.scene.collection.objects.link(o)
    return o


def cord(z, radius, material, turns=1.0, thick=0.004, strands=2, hang=None):
    """A twisted cord round the neck: strands winding round each other along a ring; hang = (x, z) of a loose end"""
    for s in range(strands):
        pts = []
        n = 90
        for k in range(n + 1):
            t = k / n * 2 * math.pi * turns
            w = t * 9 + s * math.pi
            r = radius + thick * 0.9 * math.cos(w)
            pts.append((math.cos(t) * r, math.sin(t) * r, z + thick * 0.9 * math.sin(w)))
        tube("Schnur%d" % s, pts, thick, material, cyclic=turns >= 1.0)
    if hang:
        x0, y0 = radius * math.cos(-0.5), radius * math.sin(-0.5)
        for s in range(strands):
            pts = []
            for k in range(12):
                t = k / 11
                w = t * 14 + s * math.pi
                px = x0 + (hang[0] - x0) * t
                py = y0 + (hang[2] - y0) * t
                pz = z + (hang[1] - z) * t
                pts.append((px + thick * 0.9 * math.cos(w), py, pz + thick * 0.9 * math.sin(w)))
            tube("Schnurende%d" % s, pts, thick, material)


def cork(z0, r_bottom, r_top, height, material, crown=0.0):
    prof = [(r_bottom, z0 - height * 0.45), (r_bottom * 1.02, z0), (r_top, z0 + height * 0.55), (r_top * 0.96, z0 + height * 0.55 + 0.002)]
    o = lathe("Korken", prof, material, sides=40, cap_top=True)
    return o


def plate(name, outline, depth, material, bend=0.0, at=(0, 0, 0), rot=(0, 0, 0)):
    """A flat plate of any outline in its own XZ plane, depth thick along -Y, bent round Z by bend"""
    bm = bmesh.new()
    face = bm.faces.new([bm.verts.new((x, 0, z)) for x, z in outline])
    bmesh.ops.recalc_face_normals(bm, faces=[face])
    ext = bmesh.ops.extrude_face_region(bm, geom=[face])
    bmesh.ops.translate(bm, vec=(0, depth, 0), verts=[g for g in ext["geom"] if isinstance(g, bmesh.types.BMVert)])
    if bend:
        for v in bm.verts:
            v.co.y -= bend * v.co.x * v.co.x
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = link(name, me, material, smooth_=False)
    mod = o.modifiers.new("B", "BEVEL")
    mod.width = depth * 0.3
    mod.segments = 2
    mod.limit_method = "ANGLE"
    o.location = at
    o.rotation_euler = [math.radians(v) for v in rot]
    return o


def disc(name, radius, depth, material, at, rot, sides=32, rim=None):
    prof = [(radius, 0.0), (radius, depth), (radius * 0.92, depth * 1.2)]
    o = lathe(name, prof, material, sides=sides, cap_top=True)
    o.location = at
    o.rotation_euler = [math.radians(v) for v in rot]
    if rim:
        r = bpy.data.objects.new(name + "_Rand", None)
    return o


def gear(name, radius, teeth, depth, material, at, rot, hole=0.45):
    outline = []
    for k in range(teeth * 4):
        a = k / (teeth * 4) * 2 * math.pi
        rr = radius * (1.0 if k % 4 in (1, 2) else 0.8)
        outline.append((math.cos(a) * rr, math.sin(a) * rr))
    o = plate(name, outline, depth, material, at=at, rot=rot)
    # the hole: a darker disc on both faces is enough at this size
    return o


# ---------------------------------------------------------------- the three potions

def heiltrank():
    prof = flask(0.31, 0.31, 0.085, 0.86)
    body, inner = glass_body(prof, glass())
    liq, top = fill(inner, 0.42, liquid((0.8, 0.07, 0.02), 0.15, density=22.0))
    bubbles(inner, top, glass((0.98, 0.9, 0.85), 0.01), 18, seed=2)
    neck_r = prof[-1][0]
    cork(prof[-2][1], neck_r * 0.85, neck_r * 1.0, 0.1, mat("Kork", (0.42, 0.27, 0.14), rough=0.9, bump=1.2, bump_scale=90.0))
    rope = mat("Schnur", (0.32, 0.12, 0.06), rough=0.8, bump=0.5, bump_scale=200.0)
    cord(prof[-1][1] - 0.1, neck_r * 1.12, rope, thick=0.008, hang=(0.1, prof[-1][1] - 0.32, -0.21))
    wax = mat("Siegelwachs", (0.2, 0.025, 0.015), rough=0.45, coat=0.2, bump=0.3, bump_scale=40.0)
    seal = disc("Siegel", 0.065, 0.022, wax, at=(0.06, -0.2, prof[-1][1] - 0.32), rot=(90, 0, 15))
    tube("Praegung", [(0.06 + 0.04 * math.cos(a), -0.225, prof[-1][1] - 0.32 + 0.04 * math.sin(a)) for a in np.linspace(0, 2 * math.pi, 25)], 0.004,
         mat("Praegung", (0.25, 0.03, 0.02), rough=0.4))
    paper = mat("Papier", (0.42, 0.34, 0.22), rough=0.9, bump=0.8, bump_scale=120.0)
    plate("Anhaenger", [(-0.05, 0.03), (0.06, 0.04), (0.08, -0.01), (0.065, -0.07), (-0.045, -0.075)], -0.004, paper, bend=0.6,
          at=(0.24, -0.12, prof[-1][1] - 0.36), rot=(5, 0, -35))


def nidtrunk():
    """Boris 17.11.: a rectangular bottle, metal straps running down the middle of every side and round its middle, crossing
    in the middle of each side; a cork on top. Dark violet glass, a skull tag on a chain."""
    half = 0.2                       # half the width of a side
    corner = half / math.cos(math.pi / 4)
    body_prof = [(corner * 0.9, 0.0), (corner, 0.035), (corner, 0.5), (corner * 0.86, 0.555), (0.13, 0.615), (0.1, 0.64)]
    neck_prof = [(0.1, 0.63), (0.085, 0.67), (0.085, 0.8), (0.1, 0.82), (0.1, 0.85)]
    dark = glass((0.14, 0.08, 0.17), 0.07)
    body, inner = glass_body(body_prof, dark, sides=4, twist=math.pi / 4)
    body.data.polygons.foreach_set("use_smooth", [False] * len(body.data.polygons))
    bev = body.modifiers.new("Kanten", "BEVEL")
    bev.width = 0.02
    bev.segments = 4
    bev.limit_method = "ANGLE"
    neck, _ = glass_body(neck_prof, dark, sides=40)
    liq, top = fill(inner, 0.86, liquid((0.32, 0.04, 0.48), 0.3, density=40.0), sides=4, twist=math.pi / 4)
    liq.data.polygons.foreach_set("use_smooth", [False] * len(liq.data.polygons))
    bubbles(inner, top, glass((0.8, 0.6, 1.0), 0.01), 8, seed=5, radius=0.004)
    iron = mat("Eisen", (0.018, 0.017, 0.02), rough=0.55, metal=0.85, bump=0.9, bump_scale=80.0)
    rivet_mat = mat("Niete", (0.12, 0.11, 0.1), rough=0.35, metal=1.0)
    z_lo, z_hi, z_mid = 0.03, 0.53, 0.28
    strap_w, strap_t = 0.055, 0.014
    out = half + strap_t * 0.5 + 0.004

    def box(name, size, at, material):
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:
            v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2])) + Vector(at)
        me = bpy.data.meshes.new(name)
        bm.to_mesh(me)
        bm.free()
        o = link(name, me, material, smooth_=False)
        mod = o.modifiers.new("B", "BEVEL")
        mod.width = 0.004
        mod.segments = 2
        return o

    # the four sides: +X, -X, +Y, -Y; on each a strap down the middle, and the band round the middle crosses it
    for nx, ny in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        cx, cy = nx * out, ny * out
        thick = (strap_t if nx else strap_w, strap_w if nx else strap_t, z_hi - z_lo)
        box("Band_senkrecht", thick, (cx, cy, (z_lo + z_hi) / 2), iron)
        across = (strap_t if nx else 2 * half + 0.03, 2 * half + 0.03 if nx else strap_t, strap_w)
        box("Band_rund", across, (cx, cy, z_mid), iron)
        # where they cross: a boss with a spike, four rivets round it
        n = Vector((nx, ny, 0))
        side = Vector((-ny, nx, 0))
        centre = Vector((cx, cy, z_mid)) + n * strap_t * 0.5
        tube("Stachel", [tuple(centre), tuple(centre + n * 0.05), tuple(centre + n * 0.1)], 0.026, iron, radii=[1.0, 0.5, 0.04])
        for du, dz in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
            r = bmesh.new()
            bmesh.ops.create_uvsphere(r, u_segments=10, v_segments=6, radius=0.009)
            bmesh.ops.translate(r, vec=centre + side * du * 0.017 + Vector((0, 0, dz * 0.017)), verts=r.verts)
            me = bpy.data.meshes.new("Niete")
            r.to_mesh(me)
            r.free()
            link("Niete", me, rivet_mat)
        # rivets at the ends of each strap
        for z in (z_lo + 0.02, z_hi - 0.02):
            r = bmesh.new()
            bmesh.ops.create_uvsphere(r, u_segments=10, v_segments=6, radius=0.009)
            bmesh.ops.translate(r, vec=Vector((cx, cy, z)) + n * strap_t * 0.5, verts=r.verts)
            me = bpy.data.meshes.new("Niete")
            r.to_mesh(me)
            r.free()
            link("Niete", me, rivet_mat)
    # the cork, a little black wax round its foot
    neck_r = neck_prof[-1][0]
    top_z = neck_prof[-2][1]
    cork(top_z, neck_r * 0.85, neck_r * 1.0, 0.12, mat("Kork", (0.36, 0.23, 0.12), rough=0.9, bump=1.2, bump_scale=90.0))
    wax = mat("Schwarzwachs", (0.02, 0.018, 0.02), rough=0.3, coat=0.6, bump=0.4, bump_scale=30.0)
    lathe("Wachsring", [(neck_r * 1.18, top_z - 0.05), (neck_r * 1.22, top_z - 0.01), (neck_r * 0.98, top_z + 0.015)], wax, sides=40)
    # the skull tag on a chain from the band on the right
    rust = mat("Rost", (0.32, 0.14, 0.07), rough=0.8, metal=0.5, bump=1.0, bump_scale=60.0)
    tag_at = (out + 0.06, -0.09, z_mid - 0.13)
    plate("Marke", [(-0.04, 0.045), (0.04, 0.045), (0.045, -0.045), (-0.045, -0.05)], -0.008, rust, at=tag_at, rot=(0, 0, -25))
    bone = mat("Knochen", (0.55, 0.45, 0.35), rough=0.6, bump=0.5)
    disc("Schaedel", 0.026, 0.012, bone, at=(tag_at[0] - 0.005, tag_at[1] - 0.012, tag_at[2] + 0.005), rot=(90, 0, -25))
    for k in range(5):
        t = k / 4
        x = out + 0.01 + 0.05 * t
        z = z_mid - 0.03 - 0.06 * t
        o = lathe("Glied%d" % k, [(0.008, -0.003), (0.012, 0.0), (0.008, 0.003)], iron, sides=12)
        o.location = (x, -0.09 + 0.01 * (1 - t), z)
        o.rotation_euler = (math.radians(90 * (k % 2)), 0, 0)


def manatrank():
    prof = flask(0.34, 0.34, 0.08, 0.9, shoulder=62.0)
    body, inner = glass_body(prof, glass((0.9, 0.95, 1.0)))
    liq, top = fill(inner, 0.48, liquid((0.05, 0.16, 0.9), 0.2, density=22.0))
    bubbles(inner, top, glass((0.85, 0.9, 1.0), 0.01), 22, seed=7)
    neck_r = prof[-1][0]
    cork_mat = mat("Kork", (0.45, 0.3, 0.16), rough=0.9, bump=1.2, bump_scale=90.0)
    cork(prof[-2][1], neck_r * 0.85, neck_r * 1.02, 0.13, cork_mat)
    cork_top = prof[-2][1] + 0.13 * 0.55
    # red wax round the foot of the cork, running down over the neck
    wax = mat("Rotwachs", (0.42, 0.02, 0.02), rough=0.25, coat=0.7, bump=0.2, bump_scale=30.0)
    zt = prof[-1][1]
    lathe("Wachsring", [(neck_r * 1.2, zt - 0.05), (neck_r * 1.26, zt - 0.015), (neck_r * 1.02, zt + 0.012)], wax, sides=40)
    random.seed(3)
    for k in range(7):
        a = k / 7 * 2 * math.pi + random.uniform(-0.2, 0.2)
        length = random.uniform(0.03, 0.09)
        r = neck_r * 1.1
        tube("Tropfen%d" % k, [(math.cos(a) * r, math.sin(a) * r, zt - 0.04), (math.cos(a) * r * 1.02, math.sin(a) * r * 1.02, zt - 0.04 - length * 0.6),
                               (math.cos(a) * r, math.sin(a) * r, zt - 0.04 - length)], 0.012, wax, radii=[1.0, 0.8, 1.25])
    rope = mat("Schnur", (0.35, 0.16, 0.08), rough=0.8, bump=0.5, bump_scale=200.0)
    # the cord: round the cork, then hanging down to the hole of the tag
    cord(cork_top - 0.04, neck_r * 1.02, rope, thick=0.006)
    tag_at = Vector((-0.24, -0.2, zt - 0.36))
    turn = Matrix.Rotation(math.radians(35), 3, "Z")
    hole = tag_at + turn @ Vector((-0.01, 0.0, 0.115))
    start = Vector((-neck_r * 1.0, -0.02, cork_top - 0.04))
    for k in range(2):
        pts = []
        for i in range(14):
            t = i / 13
            p = start.lerp(hole, t) + Vector((0, 0, -0.06 * math.sin(math.pi * t)))
            w = t * 16 + k * math.pi
            pts.append(tuple(p + Vector((0.004 * math.cos(w), 0.004 * math.sin(w), 0))))
        tube("Anhaengerschnur%d" % k, pts, 0.0045, rope)
    # the leather tag with a rune on the left, the gear on the right
    leather = mat("Leder", (0.16, 0.2, 0.12), rough=0.75, bump=0.8, bump_scale=70.0)
    tag = plate("Lederanhaenger", [(-0.09, 0.13), (0.07, 0.14), (0.1, -0.11), (0.0, -0.17), (-0.1, -0.1)], -0.008, leather, bend=0.9,
                at=tuple(tag_at), rot=(0, 0, 35))
    rune = mat("Rune", (0.25, 0.04, 0.03), rough=0.6)
    for pts in ([(-0.02, -0.007, 0.06), (-0.02, -0.007, -0.07)], [(-0.05, -0.007, 0.02), (0.01, -0.007, 0.03)], [(-0.045, -0.007, -0.03), (0.0, -0.007, -0.05)]):
        t = tube("Rune", pts, 0.004, rune)
        t.parent = tag
    bronze = mat("Bronze", (0.42, 0.2, 0.08), rough=0.45, metal=1.0, bump=0.6, bump_scale=60.0)
    gear("Zahnrad", 0.06, 10, -0.014, bronze, at=(0.2, -0.3, prof[len(prof) // 3][1]), rot=(0, 0, -15))


BUILDERS = {"Heiltrank": heiltrank, "Nidtrunk": nidtrunk, "Trank_Manatrank": manatrank}


# ---------------------------------------------------------------- render and export

def bounds():
    deps = bpy.context.evaluated_depsgraph_get()
    pts = []
    for o in bpy.context.scene.objects:
        if o.type in ("MESH", "CURVE"):
            ev = o.evaluated_get(deps)
            me = ev.to_mesh()
            pts += [ev.matrix_world @ v.co for v in me.vertices]
            ev.to_mesh_clear()
    lo = Vector([min(p[i] for p in pts) for i in range(3)])
    hi = Vector([max(p[i] for p in pts) for i in range(3)])
    return lo, hi


def light(name, direction, energy, color, size):
    d = bpy.data.lights.new(name, "AREA")
    d.energy = energy
    d.color = color
    d.size = size
    o = bpy.data.objects.new(name, d)
    o.location = Vector(direction).normalized() * 4.0
    o.rotation_euler = (-o.location).to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.collection.objects.link(o)


def render(path):
    bpy.context.view_layer.update()
    lo, hi = bounds()
    centre = (lo + hi) / 2
    s = bpy.context.scene
    cd = bpy.data.cameras.new("C")
    cd.type = "ORTHO"
    cam = bpy.data.objects.new("C", cd)
    s.collection.objects.link(cam)
    s.camera = cam
    view = Vector((0.0, -1.0, 0.12)).normalized()
    cam.location = centre + view * 10
    cam.rotation_euler = (-view).to_track_quat("-Z", "Y").to_euler()
    cd.ortho_scale = max(hi.x - lo.x, hi.z - lo.z) * 1.12
    light("Key", (-1.3, -1.6, 1.6), 260, (1.0, 0.88, 0.75), 1.2)
    light("Rim", (1.2, 1.5, 1.0), 320, (0.8, 0.88, 1.0), 1.0)
    light("Fill", (1.8, -1.2, 0.0), 60, (1.0, 0.95, 0.9), 2.0)
    strip = bpy.data.lights.new("Streif", "AREA")
    strip.shape = "RECTANGLE"
    strip.size, strip.size_y = 0.15, 2.0
    strip.energy = 220
    so = bpy.data.objects.new("Streif", strip)
    so.location = centre + Vector((-1.2, -2.2, 0.4))
    so.rotation_euler = (centre - so.location).to_track_quat("-Z", "Y").to_euler()
    s.collection.objects.link(so)
    s.render.filepath = path
    bpy.ops.render.render(write_still=True)


def export(path):
    # metres to the game: the whole thing HEIGHT high, standing on its bottom at the origin
    bpy.context.view_layer.update()
    lo, hi = bounds()
    scale = HEIGHT / (hi.z - lo.z)
    for o in bpy.context.scene.objects:
        if o.type == "CURVE":
            bpy.context.view_layer.objects.active = o
            o.select_set(True)
    bpy.ops.object.select_all(action="DESELECT")
    for o in [o for o in bpy.context.scene.objects if o.type == "CURVE"]:
        o.select_set(True)
        bpy.context.view_layer.objects.active = o
    if bpy.context.selected_objects:
        bpy.ops.object.convert(target="MESH")
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active
    for v in o.data.vertices:
        v.co = (v.co - Vector((0, 0, lo.z))) * scale
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                             mesh_smooth_type="FACE", axis_forward="X", axis_up="Z")


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, build in BUILDERS.items():
        if ONLY and name not in ONLY:
            continue
        reset()
        build()
        render(os.path.join(OUT, "3D_" + name + ".png"))
        export(os.path.join(OUT, "SM_Trank_" + name + ".fbx"))
        print("TRANK", name, flush=True)


main()
