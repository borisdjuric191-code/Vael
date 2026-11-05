# Entwurf D des Keimgeflechts (Boris 2026-11-05): "noch sehr viele sehr einfache Formen: der Schmetterling hat einen Zylinder als
# Körper, Wurzeln, Lichter und Blätter sehen einfach aus, das Papier billig". Baut auf Entwurf C auf und ersetzt dessen
# einfache Formen durch ausgearbeitete:
#   Blätter     gesägter Rand, Mittelrippe und Seitenadern, zur Rippe gefaltet, durchscheinend, zur Spitze heller
#   Motte       pelziger Brustkorb aus echten Haaren, geringelter Hinterleib aus Segmenten, Kopf mit Facettenaugen,
#               gefiederte Fühler, sechs Beine
#   Wurzeln     knorrig mit wechselnder Dicke, Rinde mit Rissen, Moos auf den Oberseiten, feine Wurzelhaare
#   Licht       echte Lichtstrahlen durch Dunst (Volumen), sechseckiges Bokeh mit hellem Rand, Glühwürmchen mit Leuchtspur
#   Papier      handgeschöpftes Pergament: Fasern, Stockflecken, Wasserränder, Knitter, dunkle Ränder
#
# blender.exe -b --factory-startup -P Tools/HUD/vael_keimgeflecht_d.py -- <Ausgabe.png>

import math
import os
import random
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vael_keimgeflecht_c as C  # noqa: E402

K = C.K
rnd = random.Random(1105)
_leaf_mats = {}


# ---------------------------------------------------------------- Materialien

def node_tree(name):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    return m, m.node_tree, m.node_tree.nodes["Principled BSDF"]


def leaf_mat(name, dark, light, glow=0.15):
    """Blatt mit Mittelrippe und Seitenadern (über die UVs des Blattes: u längs, v quer), zur Spitze heller, durchscheinend"""
    m, nt, b = node_tree(name)
    n, l = nt.nodes, nt.links
    uv = n.new("ShaderNodeUVMap")
    sep = n.new("ShaderNodeSeparateXYZ")
    l.new(uv.outputs["UV"], sep.inputs[0])

    def math_node(op, a=None, b_=None, va=None, vb=None):
        node = n.new("ShaderNodeMath")
        node.operation = op
        if a is not None:
            l.new(a, node.inputs[0])
        elif va is not None:
            node.inputs[0].default_value = va
        if b_ is not None:
            l.new(b_, node.inputs[1])
        elif vb is not None:
            node.inputs[1].default_value = vb
        return node.outputs[0]

    across = math_node("ABSOLUTE", math_node("SUBTRACT", sep.outputs["Y"], vb=0.5))       # 0 auf der Rippe, 0,5 am Rand
    midrib = math_node("SUBTRACT", va=1.0, b_=math_node("MINIMUM", math_node("DIVIDE", across, vb=0.035), vb=1.0))
    # Seitenadern: schräg zur Spitze laufende Linien
    wave = math_node("SINE", math_node("SUBTRACT", math_node("MULTIPLY", sep.outputs["X"], vb=34.0), math_node("MULTIPLY", across, vb=26.0)))
    side = math_node("POWER", math_node("MAXIMUM", wave, vb=0.0), vb=14.0)
    veins = math_node("MAXIMUM", midrib, math_node("MULTIPLY", side, vb=0.7))
    # Farbe: Grund zur Spitze heller, Adern heller, Rand dunkler
    ramp = n.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = (*dark, 1)
    ramp.color_ramp.elements[1].color = (*light, 1)
    l.new(sep.outputs["X"], ramp.inputs["Fac"])
    mix = n.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    l.new(veins, mix.inputs[0])
    l.new(ramp.outputs["Color"], mix.inputs[6])
    vein_col = n.new("ShaderNodeHueSaturation")
    vein_col.inputs["Value"].default_value = 1.6
    vein_col.inputs["Saturation"].default_value = 0.8
    l.new(ramp.outputs["Color"], vein_col.inputs["Color"])
    l.new(vein_col.outputs["Color"], mix.inputs[7])
    edge = n.new("ShaderNodeMix")
    edge.data_type = "RGBA"
    l.new(math_node("POWER", math_node("MULTIPLY", across, vb=2.0), vb=6.0), edge.inputs[0])
    l.new(mix.outputs[2], edge.inputs[6])
    edge.inputs[7].default_value = (dark[0] * 0.5, dark[1] * 0.5, dark[2] * 0.5, 1)
    l.new(edge.outputs[2], b.inputs["Base Color"])
    l.new(edge.outputs[2], b.inputs["Emission Color"])
    b.inputs["Emission Strength"].default_value = glow
    bump = n.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.35
    l.new(veins, bump.inputs["Height"])
    l.new(bump.outputs["Normal"], b.inputs["Normal"])
    b.inputs["Roughness"].default_value = 0.45
    b.inputs["Subsurface Weight"].default_value = 0.25
    b.inputs["Subsurface Radius"].default_value = (0.4, 1.0, 0.3)
    b.inputs["Coat Weight"].default_value = 0.15
    return m


def bark_mat(name):
    """Rinde mit Längsrissen (Voronoi-Kanten gestreckt), tiefen Furchen und Moos auf allem, was nach oben schaut"""
    m, nt, b = node_tree(name)
    n, l = nt.nodes, nt.links
    coord = n.new("ShaderNodeTexCoord")
    mapping = n.new("ShaderNodeMapping")
    mapping.inputs["Scale"].default_value = (0.25, 0.25, 0.06)
    l.new(coord.outputs["Object"], mapping.inputs["Vector"])
    cracks = n.new("ShaderNodeTexVoronoi")
    cracks.feature = "DISTANCE_TO_EDGE"
    cracks.inputs["Scale"].default_value = 1.6
    l.new(mapping.outputs["Vector"], cracks.inputs["Vector"])
    grain = n.new("ShaderNodeTexNoise")
    grain.inputs["Scale"].default_value = 0.6
    grain.inputs["Detail"].default_value = 12
    l.new(coord.outputs["Object"], grain.inputs["Vector"])
    ramp = n.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.0
    ramp.color_ramp.elements[0].color = (0.012, 0.008, 0.006, 1)
    ramp.color_ramp.elements[1].position = 0.12
    ramp.color_ramp.elements[1].color = (0.11, 0.075, 0.05, 1)
    l.new(cracks.outputs["Distance"], ramp.inputs["Fac"])
    tone = n.new("ShaderNodeMix")
    tone.data_type = "RGBA"
    tone.blend_type = "MULTIPLY"
    tone.inputs[0].default_value = 0.6
    l.new(ramp.outputs["Color"], tone.inputs[6])
    l.new(grain.outputs["Color"], tone.inputs[7])
    # Moos oben: grüne, flaumige Flecken auf den nach oben zeigenden Seiten
    geo = n.new("ShaderNodeNewGeometry")
    up = n.new("ShaderNodeSeparateXYZ")
    l.new(geo.outputs["Normal"], up.inputs[0])
    patch = n.new("ShaderNodeTexNoise")
    patch.inputs["Scale"].default_value = 0.08
    patch.inputs["Detail"].default_value = 6
    l.new(coord.outputs["Object"], patch.inputs["Vector"])
    share = n.new("ShaderNodeMath")
    share.operation = "MULTIPLY"
    l.new(up.outputs["Z"], share.inputs[0])
    l.new(patch.outputs["Fac"], share.inputs[1])
    moss_ramp = n.new("ShaderNodeValToRGB")
    moss_ramp.color_ramp.elements[0].position = 0.28
    moss_ramp.color_ramp.elements[0].color = (0, 0, 0, 1)
    moss_ramp.color_ramp.elements[1].position = 0.36
    moss_ramp.color_ramp.elements[1].color = (1, 1, 1, 1)
    l.new(share.outputs[0], moss_ramp.inputs["Fac"])
    moss_col = n.new("ShaderNodeMix")
    moss_col.data_type = "RGBA"
    l.new(moss_ramp.outputs["Color"], moss_col.inputs[0])
    l.new(tone.outputs[2], moss_col.inputs[6])
    moss_col.inputs[7].default_value = (0.05, 0.16, 0.025, 1)
    l.new(moss_col.outputs[2], b.inputs["Base Color"])
    bump = n.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.9
    height = n.new("ShaderNodeMath")
    height.operation = "ADD"
    l.new(cracks.outputs["Distance"], height.inputs[0])
    l.new(grain.outputs["Fac"], height.inputs[1])
    l.new(height.outputs[0], bump.inputs["Height"])
    l.new(bump.outputs["Normal"], b.inputs["Normal"])
    b.inputs["Roughness"].default_value = 0.85
    return m


def parchment_mat(name):
    """Handgeschöpftes Pergament: warme Grundfarbe, Fasern, Stockflecken, Wasserränder, Knitter, Ränder dunkler"""
    m, nt, b = node_tree(name)
    n, l = nt.nodes, nt.links
    coord = n.new("ShaderNodeTexCoord")

    def noise(scale, detail, stretch=(1, 1, 1)):
        mp = n.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = stretch
        l.new(coord.outputs["Generated"], mp.inputs["Vector"])
        tx = n.new("ShaderNodeTexNoise")
        tx.inputs["Scale"].default_value = scale
        tx.inputs["Detail"].default_value = detail
        l.new(mp.outputs["Vector"], tx.inputs["Vector"])
        return tx.outputs["Fac"]

    def ramp(fac, stops):
        r = n.new("ShaderNodeValToRGB")
        r.color_ramp.elements[0].position, r.color_ramp.elements[0].color = stops[0][0], (*stops[0][1], 1)
        r.color_ramp.elements[1].position, r.color_ramp.elements[1].color = stops[-1][0], (*stops[-1][1], 1)
        for pos, col in stops[1:-1]:
            e = r.color_ramp.elements.new(pos)
            e.color = (*col, 1)
        l.new(fac, r.inputs["Fac"])
        return r.outputs["Color"]

    def mult(a, b_, fac=1.0, kind="MULTIPLY"):
        mx = n.new("ShaderNodeMix")
        mx.data_type = "RGBA"
        mx.blend_type = kind
        mx.inputs[0].default_value = fac
        l.new(a, mx.inputs[6])
        l.new(b_, mx.inputs[7])
        return mx.outputs[2]

    base = ramp(noise(2.5, 6), [(0.3, (0.62, 0.5, 0.33)), (0.75, (0.84, 0.74, 0.55))])
    fibres = ramp(noise(180.0, 3, (1, 1, 6)), [(0.4, (0.86, 0.84, 0.8)), (0.6, (1, 1, 1))])
    color = mult(base, fibres, 0.8)
    # Stockflecken: kleine braune Punkte
    fox = n.new("ShaderNodeTexVoronoi")
    fox.inputs["Scale"].default_value = 26.0
    fox.inputs["Randomness"].default_value = 1.0
    l.new(coord.outputs["Generated"], fox.inputs["Vector"])
    spots = ramp(fox.outputs["Distance"], [(0.03, (0.45, 0.3, 0.16)), (0.07, (1, 1, 1))])
    color = mult(color, spots, 0.55)
    # Wasserränder: dünne dunklere Ringe aus großem Rauschen
    stain = ramp(noise(1.6, 2), [(0.48, (1, 1, 1)), (0.5, (0.72, 0.58, 0.4)), (0.53, (1, 1, 1))])
    color = mult(color, stain, 0.7)
    # Ränder dunkler und fleckiger
    sep = n.new("ShaderNodeSeparateXYZ")
    l.new(coord.outputs["Generated"], sep.inputs[0])
    edge_x = n.new("ShaderNodeMath")
    edge_x.operation = "PINGPONG"
    edge_x.inputs[1].default_value = 0.5
    l.new(sep.outputs["X"], edge_x.inputs[0])
    edge_z = n.new("ShaderNodeMath")
    edge_z.operation = "PINGPONG"
    edge_z.inputs[1].default_value = 0.5
    l.new(sep.outputs["Z"], edge_z.inputs[0])
    edge = n.new("ShaderNodeMath")
    edge.operation = "MINIMUM"
    l.new(edge_x.outputs[0], edge.inputs[0])
    l.new(edge_z.outputs[0], edge.inputs[1])
    burn = ramp(edge.outputs[0], [(0.0, (0.3, 0.18, 0.08)), (0.06, (0.75, 0.6, 0.42)), (0.14, (1, 1, 1))])
    color = mult(color, burn)
    l.new(color, b.inputs["Base Color"])
    l.new(color, b.inputs["Emission Color"])
    b.inputs["Emission Strength"].default_value = 0.06
    bump = n.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.35
    wrinkle = n.new("ShaderNodeMath")
    wrinkle.operation = "ADD"
    l.new(noise(4.0, 4), wrinkle.inputs[0])
    l.new(noise(180.0, 3, (1, 1, 6)), wrinkle.inputs[1])
    l.new(wrinkle.outputs[0], bump.inputs["Height"])
    l.new(bump.outputs["Normal"], b.inputs["Normal"])
    b.inputs["Roughness"].default_value = 0.9
    b.inputs["Subsurface Weight"].default_value = 0.08
    return m


# ---------------------------------------------------------------- Formen

_plain_leaf = K.leaf


def detailed_leaf(x, y, angle, length, width, material, depth=-12.0, bend=0.25):
    """Blatt mit gesägtem Rand, zur Mittelrippe gefaltet, UVs für die Adern"""
    name = material.name if material is not None else ""
    if name.startswith("Blatt") or name in ("Vorderblatt",):
        dark, light = {"Blatt": ((0.03, 0.12, 0.03), (0.2, 0.5, 0.14)), "BlattDunkel": ((0.01, 0.04, 0.01), (0.05, 0.14, 0.04)),
                       "Vorderblatt": ((0.003, 0.008, 0.003), (0.012, 0.03, 0.01))}.get(name.split(".")[0], ((0.03, 0.12, 0.03), (0.2, 0.5, 0.14)))
        key = name.split(".")[0]
        if key not in _leaf_mats:
            _leaf_mats[key] = leaf_mat("Ader" + key, dark, light, 0.02 if key == "Vorderblatt" else 0.15)
        material = _leaf_mats[key]
    nl, nw = 18, 6
    a = math.radians(angle)
    ax, ay = math.cos(a), math.sin(a)
    verts, faces, uvs = [], [], []
    seed = rnd.uniform(0, 10)
    for i in range(nl + 1):
        t = i / nl
        half = width * 0.5 * math.sin(math.pi * min(t * 1.06, 1.0)) ** 0.75
        if 0.12 < t < 0.95:
            half *= 1.0 + 0.07 * max(0.0, math.sin(t * 46.0 + seed))
        cx, cy = x + ax * length * t, y + ay * length * t
        cx += -ay * bend * length * t * t
        cy += ax * bend * length * t * t
        for j in range(nw + 1):
            v = j / nw * 2.0 - 1.0
            px, py = cx - ay * half * v, cy + ax * half * v
            fold = abs(v) * width * 0.14 + math.sin(t * math.pi) * width * 0.04
            verts.append(K.P(px, py, depth - fold))
            uvs.append((t, (v + 1) * 0.5))
    for i in range(nl):
        for j in range(nw):
            a0 = i * (nw + 1) + j
            faces.append((a0, a0 + 1, a0 + nw + 2, a0 + nw + 1))
    mesh = bpy.data.meshes.new("Blatt")
    mesh.from_pydata(verts, [], faces)
    mesh.uv_layers.new()
    for poly in mesh.polygons:
        poly.use_smooth = True
        for li in poly.loop_indices:
            mesh.uv_layers[0].data[li].uv = uvs[mesh.loops[li].vertex_index]
    obj = bpy.data.objects.new("Blatt", mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material)
    return obj


def hair(name, roots, dirs, lengths, radius, material):
    """Kurze Haare als Haarkurven: roots und dirs in Szenenkoordinaten"""
    curves = bpy.data.hair_curves.new(name)
    points = 4
    curves.add_curves([points] * len(roots))
    pos = []
    for r, d, ln in zip(roots, dirs, lengths):
        for k in range(points):
            t = k / (points - 1)
            p = r + d * ln * t + Vector((0, 0, -0.25 * ln * t * t))
            pos.extend((p.x, p.y, p.z))
    curves.position_data.foreach_set("vector", pos)
    if "radius" not in curves.attributes:
        curves.attributes.new("radius", "FLOAT", "POINT")
    curves.attributes["radius"].data.foreach_set("value", [radius * (1.0 - 0.8 * (k / (points - 1))) for _ in roots for k in range(points)])
    obj = bpy.data.objects.new(name, curves)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material)
    return obj


def fuzzy_ellipsoid(name, centre, radii, material, fur_material, count, fur_len):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=1.0)
    body = bpy.context.object
    body.name = name
    body.location = centre
    body.scale = radii
    bpy.ops.object.shade_smooth()
    body.data.materials.append(material)
    roots, dirs, lens = [], [], []
    for _ in range(count):
        u, v = rnd.uniform(0, 2 * math.pi), math.acos(rnd.uniform(-1, 1))
        nrm = Vector((math.sin(v) * math.cos(u), math.sin(v) * math.sin(u), math.cos(v)))
        if nrm.y > 0.35:
            continue  # hinten sieht man nicht
        roots.append(centre + Vector((nrm.x * radii[0], nrm.y * radii[1], nrm.z * radii[2])))
        dirs.append((nrm + Vector((rnd.uniform(-0.3, 0.3), 0, rnd.uniform(-0.5, 0.1)))).normalized())
        lens.append(fur_len * rnd.uniform(0.6, 1.2))
    hair(name + "Pelz", roots, dirs, lens, 0.35, fur_material)
    return body


def detailed_moth(x, y, scale):
    """Die Motte mit echtem Körper: pelziger Brustkorb, geringelter Hinterleib, Kopf mit Augen, gefiederte Fühler, Beine"""
    s = scale
    path = os.path.join(K.ROOT, "Scripts", "Data", "T_Floromant_Fluegel.png")
    wing = K.cutout_mat(path, emit=0.6)
    w, h = 66 * s, 99 * s
    K.quad("FluegelR", x + 2 * s, y - h * 0.45, x + w + 2 * s, y + h * 0.55, wing, depth=-20)
    left = K.quad("FluegelL", x - w - 2 * s, y - h * 0.45, x - 2 * s, y + h * 0.55, wing, depth=-20)
    for loop in left.data.uv_layers[0].data:
        loop.uv = (1.0 - loop.uv[0], loop.uv[1])
    fur = bpy.data.materials.new("Mottenpelz")
    fur.use_nodes = True
    fb = fur.node_tree.nodes["Principled BSDF"]
    fb.inputs["Base Color"].default_value = (0.42, 0.3, 0.2, 1)
    fb.inputs["Roughness"].default_value = 0.6
    fb.inputs["Sheen Weight"].default_value = 1.0
    fb.inputs["Sheen Tint"].default_value = (1.0, 0.85, 0.7, 1)
    chitin, _, cb = node_tree("Chitin")
    cb.inputs["Base Color"].default_value = (0.16, 0.1, 0.06, 1)
    cb.inputs["Roughness"].default_value = 0.4
    cb.inputs["Coat Weight"].default_value = 0.5
    # Brustkorb, dicht behaart
    thorax = K.P(x, y + 2 * s, -26)
    fuzzy_ellipsoid("Brust", thorax, (6.5 * s, 5.0 * s, 7.5 * s), chitin, fur, 900, 3.2 * s)
    # Hinterleib: Segmente, abwechselnd dunkel und golden geringelt, nach unten spitz zulaufend
    gold_band = K.mat("Ring", (0.6, 0.45, 0.22), 0.5)
    for k in range(7):
        t = k / 6
        cy = y + 10 * s + k * 5.2 * s
        r = (5.6 - 3.6 * t ** 1.4) * s
        seg = K.sphere("Segment", x, cy, 1.0, chitin if k % 2 == 0 else gold_band, depth=-26 + k * 0.3)
        seg.scale = (r, r * 0.8, 3.4 * s)
        fuzzy_ellipsoid("Ringpelz", K.P(x, cy, -26), (r * 0.98, r * 0.78, 3.3 * s), chitin, fur, 60, 1.6 * s)
    # Kopf mit zwei großen Facettenaugen
    head = K.P(x, y - 9 * s, -27)
    fuzzy_ellipsoid("Kopf", head, (4.4 * s, 3.8 * s, 3.8 * s), chitin, fur, 200, 1.8 * s)
    eye = K.mat("Auge", (0.02, 0.015, 0.02), 0.15)
    eye.node_tree.nodes["Principled BSDF"].inputs["Coat Weight"].default_value = 1.0
    for side in (-1, 1):
        K.sphere("Auge", x + side * 3.3 * s, y - 10 * s, 2.3 * s, eye, depth=-30)
    # Gefiederte Fühler: ein Schaft, links und rechts feine Fiedern, zur Spitze kürzer
    feather = K.mat("Fieder", (0.55, 0.42, 0.25), 0.5)
    for side in (-1, 1):
        shaft = []
        for k in range(16):
            t = k / 15
            shaft.append((x + side * (2 * s + 15 * s * t + 3 * s * t * t), y - 12 * s - 20 * s * t + 4 * s * t * t))
        K.tube("Fuehler", shaft, 0.45 * s, feather, depth=-28)
        for k in range(1, 15):
            px, py = shaft[k]
            nx, ny = shaft[k + 1][0] - px, shaft[k + 1][1] - py
            ln = math.hypot(nx, ny)
            nx, ny = nx / ln, ny / ln
            barb = 3.6 * s * (1.0 - k / 16)
            for fs in (-1, 1):
                K.tube("Fieder", [(px, py), (px - ny * fs * barb + nx * barb * 0.4, py + nx * fs * barb + ny * barb * 0.4)], 0.22 * s, feather, depth=-28, taper=True)
    # Sechs Beine, gelenkig, unter dem Brustkorb hervor
    for side in (-1, 1):
        for k, (a, b) in enumerate(((-0.6, 0.9), (0.1, 1.1), (0.7, 1.2))):
            hip = (x + side * 4 * s, y + (k * 3 - 2) * s)
            knee = (hip[0] + side * 7 * s * math.cos(a), hip[1] + 7 * s * math.sin(a) - 2 * s)
            foot = (knee[0] + side * 4 * s, knee[1] + 7 * s * b)
            K.tube("Bein", [hip, knee, foot], 0.55 * s, chitin, depth=-24, taper=True)
    K.glow(x, y, 120 * s, K.glow_mat("Mottenschein", (1.0, 0.78, 0.86), 1.0), depth=10)


def gnarled_root(points, radius, material, depth):
    """Knorrige Wurzel: Dicke schwankt von Punkt zu Punkt, Knoten, feine Wurzelhaare, Seitentriebe"""
    curve = bpy.data.curves.new("Wurzel", "CURVE")
    curve.dimensions = "3D"
    curve.bevel_depth = radius
    curve.bevel_resolution = 5
    sp = curve.splines.new("POLY")
    sp.points.add(len(points) - 1)
    for i, (px, py) in enumerate(points):
        p = K.P(px, py, depth + rnd.uniform(-3, 3))
        sp.points[i].co = (p.x, p.y, p.z, 1)
        t = i / max(len(points) - 1, 1)
        sp.points[i].radius = max(0.12, (1.0 - 0.85 * t) * (1.0 + 0.3 * math.sin(i * 1.7) + rnd.uniform(-0.12, 0.12)))
    obj = bpy.data.objects.new("Wurzel", curve)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material)
    hair_mat = K.mat("Wurzelhaar", (0.3, 0.24, 0.17), 0.7)
    for i in range(1, len(points) - 1):
        if rnd.random() < 0.6:
            px, py = points[i]
            for _ in range(rnd.randint(2, 5)):
                a = rnd.uniform(0, 2 * math.pi)
                ln = rnd.uniform(6, 16)
                K.tube("Haar", [(px, py), (px + math.cos(a) * ln * 0.5, py + math.sin(a) * ln * 0.5 + 2), (px + math.cos(a + 0.3) * ln, py + math.sin(a + 0.3) * ln + 4)],
                       0.5, hair_mat, depth=depth - 2, taper=True)
    return obj


def detailed_room():
    """Die Kammer: knorrige Wurzeln mit Moos und Haaren aus den Ecken, Pilze, Moospolster aus Haaren"""
    bark = bark_mat("Rinde")
    for (sx, sy), (ex, ey), (bx, by), r in [((20, 1060), (560, 1066), (260, 990), 16), ((1900, 1060), (1360, 1068), (1650, 980), 16),
                                            ((20, 20), (30, 560), (120, 280), 14), ((1900, 20), (1890, 620), (1800, 300), 14),
                                            ((20, 20), (560, 26), (260, 110), 12), ((1900, 20), (1380, 22), (1660, 104), 12)]:
        pts = []
        for k in range(30):
            t = k / 29
            px = (1 - t) ** 2 * sx + 2 * (1 - t) * t * bx + t * t * ex + rnd.uniform(-6, 6)
            py = (1 - t) ** 2 * sy + 2 * (1 - t) * t * by + t * t * ey + rnd.uniform(-6, 6)
            pts.append((px, py))
        gnarled_root(pts, r, bark, -4)
        for k in range(4, 26, 6):
            px, py = pts[k]
            ang = rnd.uniform(0, 2 * math.pi)
            branch = [(px + math.cos(ang + 0.15 * j) * 9 * j, py + math.sin(ang + 0.15 * j) * 9 * j) for j in range(8)]
            gnarled_root(branch, r * 0.35, bark, -3)
    # Moospolster aus echten Haaren in den unteren Ecken
    moss_hair = bpy.data.materials.new("MoosHaar")
    moss_hair.use_nodes = True
    mb = moss_hair.node_tree.nodes["Principled BSDF"]
    mb.inputs["Base Color"].default_value = (0.05, 0.17, 0.03, 1)
    mb.inputs["Roughness"].default_value = 0.8
    mb.inputs["Sheen Weight"].default_value = 0.6
    for cx, cy, spread in ((90, 1030, 120), (1830, 1030, 120), (640, 1062, 160), (1280, 1062, 160)):
        roots, dirs, lens = [], [], []
        for _ in range(1600):
            px, py = cx + rnd.gauss(0, spread * 0.4), cy + rnd.gauss(0, 10)
            roots.append(K.P(px, py, -8 + rnd.uniform(-6, 6)))
            dirs.append(Vector((rnd.uniform(-0.4, 0.4), rnd.uniform(-0.6, 0.0), 1.0)).normalized())
            lens.append(rnd.uniform(4, 11))
        hair("Moos", roots, dirs, lens, 0.6, moss_hair)
    # Pilze mit Lamellen-Rand und Leuchten
    for px, py, s in ((96, 1000, 1.0), (124, 1012, 0.7), (1818, 996, 0.9), (1796, 1012, 0.6)):
        K.tube("Stiel", [(px, py + 2), (px + 1, py - 9 * s), (px + 2, py - 18 * s)], 2.6 * s, K.mat("PilzStiel", (0.75, 0.8, 0.7), 0.6, emit=0.4), depth=-10)
        cap = K.sphere("Hut", px + 2, py - 20 * s, 11 * s, K.MUSHROOM, depth=-10, squash=0.5)
        cap.scale = (1.0, 0.5, 0.55)
        K.ring("Lamellen", px + 2, py - 17 * s, 9.5 * s, 1.2 * s, K.mat("Lamelle", (0.2, 0.7, 0.6), 0.5, emit=1.0), depth=-11)
        K.glow(px + 2, py - 20 * s, 56 * s, K.glow_mat("Pilzschein", (0.3, 1.0, 0.85), 1.2), depth=-8)


def bokeh_hex(x, y, r, color, strength, depth=50):
    """Unscharfes Licht wie durch eine Linse: ein Sechseck mit hellerem Rand"""
    verts = [K.P(x, y, depth)] + [K.P(x + math.cos(a) * r, y + math.sin(a) * r, depth) for a in [k * math.pi / 3 + 0.2 for k in range(6)]]
    faces = [(0, k + 1, (k + 1) % 6 + 1) for k in range(6)]
    mesh = bpy.data.meshes.new("Bokeh")
    mesh.from_pydata(verts, [], faces)
    obj = bpy.data.objects.new("Bokeh", mesh)
    bpy.context.collection.objects.link(obj)
    m = bpy.data.materials.new("BokehMat")
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Color"].default_value = (*color, 1)
    em.inputs["Strength"].default_value = strength
    tr = nt.nodes.new("ShaderNodeBsdfTransparent")
    mix = nt.nodes.new("ShaderNodeMixShader")
    mix.inputs["Fac"].default_value = 0.35
    nt.links.new(tr.outputs[0], mix.inputs[1])
    nt.links.new(em.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    obj.data.materials.append(m)
    obj.visible_shadow = False
    rim = K.ring("BokehRand", x, y, r * 0.96, max(1.0, r * 0.04), K.mat("Rand", color, 0.5, emit=strength * 0.6), depth=depth - 1)
    rim.data.materials.clear()
    rim.data.materials.append(m)
    return obj


def god_rays():
    """Dunst in der Kammer und drei Lichtkegel von oben: echte Lichtstrahlen statt flacher Streifen"""
    bpy.ops.mesh.primitive_cube_add(size=1)
    fog = bpy.context.object
    fog.name = "Dunst"
    fog.location = K.P(K.W / 2, K.H / 2, 46)
    fog.scale = (K.W * 1.1, 20, K.H * 1.1)
    fm = bpy.data.materials.new("Dunst")
    fm.use_nodes = True
    nt = fm.node_tree
    nt.nodes.remove(nt.nodes["Principled BSDF"])
    vol = nt.nodes.new("ShaderNodeVolumePrincipled")
    vol.inputs["Density"].default_value = 0.0035
    vol.inputs["Anisotropy"].default_value = 0.45
    vol.inputs["Color"].default_value = (0.8, 1.0, 0.85, 1)
    nt.links.new(vol.outputs[0], nt.nodes["Material Output"].inputs["Volume"])
    fog.data.materials.append(fm)
    fog.visible_shadow = False
    for k, (x, ang, energy) in enumerate(((620, -14, 2.2e8), (860, -6, 3.0e8), (1180, 8, 1.6e8))):
        spot = bpy.data.lights.new("Strahl", "SPOT")
        spot.energy = energy
        spot.spot_size = math.radians(18)
        spot.spot_blend = 0.8
        spot.color = (0.85, 1.0, 0.8)
        spot.shadow_soft_size = 40
        obj = bpy.data.objects.new("Strahl", spot)
        obj.location = K.P(x, -500, 46)
        obj.rotation_euler = (0, math.radians(ang), 0)
        bpy.context.collection.objects.link(obj)


def fireflies():
    body = K.mat("Gluehwurm", (1.0, 0.95, 0.55), 0.4, emit=12.0)
    for _ in range(18):
        x, y = rnd.uniform(120, 1800), rnd.uniform(160, 960)
        K.sphere("Gluehwurm", x, y, 1.6, body, depth=-32)
        K.glow(x, y, rnd.uniform(14, 26), K.glow_mat("Gluehschein", (1.0, 0.9, 0.5), 1.4), depth=-31)


# ---------------------------------------------------------------- Zusammenbau

def main():
    out = sys.argv[sys.argv.index("--") + 1]
    # Ersatz der einfachen Formen in Entwurf B/C
    K.leaf = detailed_leaf
    K.moth_emblem = detailed_moth
    K.alive_room = detailed_room
    K.aged_paper = parchment_mat
    C.rnd.seed(2026)
    K.setup()
    K.materials()
    scene = bpy.context.scene
    scene.cycles.samples = 96
    scene.cycles.volume_step_rate = 4.0
    scene.view_settings.exposure = -0.85
    bpy.data.objects["Key"].data.energy *= 0.6
    bpy.data.objects["Fill"].data.energy *= 0.5
    heart = bpy.data.lights.new("Herzlicht", "POINT")
    heart.energy = 0.9e7
    heart.color = (0.6, 1.0, 0.7)
    heart.shadow_soft_size = 120
    obj = bpy.data.objects.new("Herzlicht", heart)
    obj.location = K.P(790, 560, -300)
    bpy.context.collection.objects.link(obj)
    warm = bpy.data.lights.new("Tafellicht", "AREA")
    warm.energy = 0.75e7
    warm.size = 900
    warm.color = (1.0, 0.85, 0.65)
    wobj = bpy.data.objects.new("Tafellicht", warm)
    wobj.location = K.P(1430, 300, -900)
    wobj.rotation_euler = (math.radians(90), 0, 0)
    bpy.context.collection.objects.link(wobj)
    # flache Bokeh-Kreise von C werden zu Linsen-Sechsecken: C zeichnet seine eigenen, dazu kommen diese
    C.build()
    god_rays()
    fireflies()
    scene.render.filepath = out
    bpy.ops.render.render(write_still=True)
    print("[Vael] gerendert:", out)


if __name__ == "__main__":
    main()
