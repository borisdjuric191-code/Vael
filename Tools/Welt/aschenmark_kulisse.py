"""
Scenery pieces of the Aschenmark that no pack has, built procedurally (no Tripo) with the builder of the Glutkammer
stations and exported as FBX for Scripts/BuildKulisse.py:

  SM_AM_Schlacke_A/B/C   lumps of glassy slag, from knee-high to taller than a man, glowing seams low down
  SM_AM_Glutspalte_A/B   a crack in the ground: two broken lips of rock along a wandering line, glowing deep inside
  SM_AM_Pfaehle_A/B      charred stakes, some snapped, leaning in a loose group
  SM_AM_Gerippe_A/B      the ribcage of a large beast half sunk in ash; B with the long skull and a horn
  SM_AM_Wegmarke_A/B     a cairn of stacked stones; A with a pole and a torn rag, B with a skull on top
  SM_AM_Grabhuegel_A/B   a low mound of earth ringed with stones; A with a stake, B with a sword stuck in it
  SM_AM_Karrenwrack      a tipped cart: the bed on its side, one wheel off and lying flat, the shaft snapped

Slots: 0 Holz, 1 Leder, 2 Metall, 3 Faden, 4 Glut, 5 Fels, 6 Erde, 7 Knochen, 8 Kohle. Metres, pivot at the bottom
middle, sunk a little into the ground so slopes do not show a gap. UV1 = glow mask for Fels and Kohle.
Imperfect on purpose. All pieces are rendered side by side to <out>/vorschau_kulisse.png.

  blender -b --factory-startup -P aschenmark_kulisse.py -- C:/Projekte/Vael_Quellen/Aschenmark/export
"""
import math
import os
import random
import sys

import bpy
from mathutils import Vector, noise

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Refugium"))
import glutkammer_probe as gk  # noqa: E402

SLOTS = ["Holz", "Leder", "Metall", "Faden", "Glut", "Fels", "Erde", "Knochen", "Kohle"]
HOLZ, LEDER, METALL, FADEN, GLUT, FELS, ERDE, KNOCHEN, KOHLE = range(9)
V = Vector


def jitter(amount):
    return random.uniform(-amount, amount)


def blob(b, centre, size, material, seed, rough=0.35, rings=14, sides=18, flat=0.75, sink=0.12, glow_low=0.0, glow_seams=0.0):
    """A lumpy closed body (rock, slag, mound): a sphere pushed in and out by noise, squashed, sunk into the ground.
    glow_low lights seams near the ground, glow_seams anywhere"""
    off = V((seed * 3.7, seed * 1.3, seed * 2.1))
    rows = []
    masks = []
    for j in range(rings + 1):
        lat = -math.pi / 2 + math.pi * j / rings
        row, mrow = [], []
        for i in range(sides):
            lon = 2 * math.pi * i / sides
            d = V((math.cos(lat) * math.cos(lon), math.cos(lat) * math.sin(lon), math.sin(lat)))
            q = d * 1.6 + off
            bump = noise.noise(q) * 0.6 + noise.noise(q * 2.3) * 0.28 + noise.noise(q * 5.0) * 0.12
            cell, _ = noise.voronoi(q * 1.4, distance_metric="DISTANCE", exponent=2.5)
            seam = gk.smooth(0.25, 0.0, cell[1] - cell[0])
            r = 1.0 + bump * rough - seam * 0.08
            p = V((d.x * size.x * r, d.y * size.y * r, d.z * size.z * r * flat))
            row.append(centre + p + V((0, 0, size.z * flat * 0.9 - sink)))
            low = gk.smooth(size.z * 0.9, 0.0, p.z + size.z * flat * 0.9)
            mrow.append(max(0.0, min(1.0, seam * (glow_low * low + glow_seams))))
        rows.append(row)
        masks.append(mrow)
    b.grid(rows, material, uv_of=lambda j, i: (i / sides, j / rings), mask_of=lambda j, i: masks[j][i % sides], closed_u=True)


def pole(b, a, c, r, material, seed, rough=0.15, sides=7, mask=0.0):
    b.tube([a, a.lerp(c, 0.5) + V((jitter(0.03), jitter(0.03), 0)), c], [r, r * 0.95, r * 0.8], material, sides=sides, rough=rough, seed=seed,
           mask_of=lambda t: mask)
    b.tube([c, a], [r * 0.8, r], material, sides=sides, rough=rough, seed=seed + 300)


def stone(b, centre, size, seed):
    blob(b, centre, size, FELS, seed, rough=0.3, rings=8, sides=10, flat=0.8, sink=size.z * 0.25)


# ---------------------------------------------------------------- pieces

def build_slag(name, size, seed):
    obj = gk.new_object(name, SLOTS)
    b = gk.Builder()
    random.seed(seed)
    blob(b, V((0, 0, 0)), size, FELS, seed, rough=0.45, rings=18, sides=24, flat=0.8, sink=size.z * 0.2, glow_low=0.9, glow_seams=0.15)
    # a few smaller lumps broken off round its foot
    for k in range(random.randint(3, 5)):
        a = random.uniform(0, 2 * math.pi)
        d = size.x * random.uniform(1.0, 1.5)
        s = size * random.uniform(0.15, 0.3)
        blob(b, V((math.cos(a) * d, math.sin(a) * d, 0)), s, FELS, seed + k + 10, rough=0.5, rings=8, sides=10, sink=s.z * 0.3, glow_low=0.6)
    b.finish(obj)
    gk.export(obj)
    return obj


def build_crack(name, length, seed):
    """Two lips of broken rock along a wandering line; between them a glowing trough sunk below the ground"""
    obj = gk.new_object(name, SLOTS)
    b = gk.Builder()
    random.seed(seed)
    n = int(length / 0.25)
    centre = []
    for k in range(n + 1):
        t = k / n
        x = (t - 0.5) * length
        y = noise.noise(V((t * 3.0, seed, 0))) * 0.9
        centre.append(V((x, y, 0)))
    width = [0.15 + 0.45 * math.sin(math.pi * k / n) * (0.7 + 0.3 * noise.noise(V((k * 0.2, seed, 1)))) for k in range(n + 1)]
    # glowing floor of the crack
    rows = []
    for k in range(n + 1):
        side = V((-(centre[min(k + 1, n)] - centre[max(k - 1, 0)]).y, (centre[min(k + 1, n)] - centre[max(k - 1, 0)]).x, 0)).normalized()
        rows.append([centre[k] + side * width[k] * s + V((0, 0, 0.03 - 0.05 * (1 - abs(s)))) for s in (-1.0, -0.5, 0.0, 0.5, 1.0)])
    b.grid(rows, GLUT, uv_of=lambda j, i: (i / 4, j / n), mask_of=lambda j, i: 1.0)
    # the two lips: a cross-section from deep inside up over the edge and down to the ground outside
    for s in (-1, 1):
        rows = []
        masks = []
        for k in range(n + 1):
            d = centre[min(k + 1, n)] - centre[max(k - 1, 0)]
            side = V((-d.y, d.x, 0)).normalized() * s
            w = width[k]
            q = centre[k] * 1.3 + V((seed, s * 4, 0))
            taper = math.sin(math.pi * k / n) ** 0.5
            lift = (0.15 + 0.25 * max(0.0, noise.noise(q)) + 0.08 * noise.noise(q * 3.0)) * taper
            profile = [(w * 0.9, -0.05), (w * 0.95, lift * 0.5), (w + 0.05, lift), (w + 0.3, lift * 0.85), (w + 0.6, lift * 0.3), (w + 0.9, -0.12)]
            rows.append([centre[k] + side * a + V((0, 0, z + noise.noise(q * 4.0 + V((a, z, 0))) * 0.05)) for a, z in profile])
            masks.append([0.9, 0.5, 0.05, 0.0, 0.0, 0.0])
        if s < 0:
            rows = [list(reversed(r)) for r in rows]
            masks = [list(reversed(m)) for m in masks]
        b.grid(rows, FELS, uv_of=lambda j, i: (i / 5, j * 0.25), mask_of=lambda j, i: masks[j][i])
    b.finish(obj)
    gk.export(obj)
    return obj


def build_stakes(name, count, seed):
    obj = gk.new_object(name, SLOTS)
    b = gk.Builder()
    random.seed(seed)
    for k in range(count):
        a = random.uniform(0, 2 * math.pi)
        d = random.uniform(0.0, 1.4)
        base = V((math.cos(a) * d, math.sin(a) * d, -0.3))
        h = random.uniform(0.8, 2.8) if random.random() > 0.3 else random.uniform(0.3, 0.7)
        lean = V((jitter(0.35), jitter(0.35), 1)).normalized()
        top = base + lean * h
        r = random.uniform(0.06, 0.12)
        b.tube([base, base.lerp(top, 0.6), top, top + lean * r * 2.5], [r, r * 0.95, r * 0.7, 0.01], KOHLE, sides=6, rough=0.3, seed=seed + k,
               mask_of=lambda t: 0.3 * gk.smooth(0.35, 0.0, t))
    # fallen pieces
    for k in range(3):
        a = V((jitter(1.5), jitter(1.5), 0.05))
        c = a + V((jitter(1.2), jitter(1.2), 0.03))
        b.tube([a, c], [0.07, 0.05], KOHLE, sides=6, rough=0.3, seed=seed + 50 + k)
    b.finish(obj)
    gk.export(obj)
    return obj


def build_ribcage(name, seed, skull):
    obj = gk.new_object(name, SLOTS)
    b = gk.Builder()
    random.seed(seed)
    length = 5.5
    spine = [V((x, 0.2 * math.sin(x * 0.6), 0.75 + 0.25 * math.sin(x * 0.8) - 0.05 * x)) for x in [i * length / 16 - length / 2 for i in range(17)]]
    b.tube(spine, [0.13 - 0.04 * abs(i - 8) / 8 for i in range(17)], KNOCHEN, sides=8, flat=0.8, rough=0.15, seed=seed)
    for k in range(10):
        t = 0.15 + 0.7 * k / 9
        root = spine[int(t * 16)]
        span = 1.3 * math.sin(math.pi * (0.2 + 0.8 * t)) + 0.4
        for s in (-1, 1):
            if random.random() < 0.15:
                continue                                  # a rib missing
            broken = random.random() < 0.3
            pts = []
            steps = 12 if not broken else random.randint(5, 9)
            for i in range(steps + 1):
                u = i / 12
                ang = u * math.pi * 0.95
                pts.append(root + V((u * 0.25, s * math.sin(ang) * span, (math.cos(ang) - 1) * 0.85 * span + 0.05 * jitter(1))))
            b.tube(pts, [0.07 * (1 - 0.5 * i / 12) for i in range(len(pts))], KNOCHEN, sides=6, flat=0.55, rough=0.1, seed=seed + k * 2 + s)
    if skull:
        head = spine[-1] + V((0.9, 0.0, -0.35))
        blob(b, head, V((0.9, 0.38, 0.32)), KNOCHEN, seed + 99, rough=0.2, rings=10, sides=14, flat=0.9, sink=0.25)
        # jaw and a long curved horn
        b.tube([head + V((0.3, 0.15, -0.2)), head + V((1.1, 0.2, -0.45)), head + V((1.5, 0.15, -0.55))], [0.09, 0.07, 0.05], KNOCHEN, sides=6, flat=0.6)
        horn = [head + V((-0.2 - 0.25 * i, 0.3 + 0.08 * i, 0.25 + 0.3 * i - 0.04 * i * i)) for i in range(8)]
        b.tube(horn, [0.14 * (1 - i / 8) + 0.01 for i in range(8)], KNOCHEN, sides=7, rough=0.08, seed=seed + 7)
    # ash heaped against it
    blob(b, V((0, 0, 0)), V((length * 0.45, 1.5, 0.35)), ERDE, seed + 5, rough=0.3, rings=8, sides=16, flat=0.6, sink=0.3)
    b.finish(obj)
    gk.export(obj)
    return obj


def build_cairn(name, seed, rag):
    obj = gk.new_object(name, SLOTS)
    b = gk.Builder()
    random.seed(seed)
    z = 0.0
    r = 0.55
    while r > 0.12:
        for k in range(max(1, int(r * 9))):
            a = 2 * math.pi * k / max(1, int(r * 9)) + jitter(0.3)
            size = V((random.uniform(0.18, 0.3), random.uniform(0.15, 0.25), random.uniform(0.1, 0.16)))
            stone(b, V((math.cos(a) * r * 0.7, math.sin(a) * r * 0.7, z)), size, seed + int(z * 100) + k)
        z += 0.2
        r -= 0.12
    if rag:
        top = V((0.05, 0.0, 2.3))
        pole(b, V((0, 0, -0.2)), top, 0.05, HOLZ, seed)
        b.tube([top + V((0, 0, -0.15)), top + V((0, 0.05, -0.12))], [0.04, 0.04], FADEN, sides=5)
        rows = []
        for j in range(6):
            rows.append([top + V((0.02, 0.05 + 0.13 * i, -0.15 - 0.12 * j - 0.04 * i * i + 0.05 * math.sin(i + j)))
                         for i in range(6)])
        b.grid(rows, LEDER, uv_of=lambda j, i: (i / 5, j / 5), mask_of=lambda j, i: 0.0)
        b.grid([list(reversed(r_)) for r_ in rows], LEDER, uv_of=lambda j, i: (i / 5, j / 5), mask_of=lambda j, i: 0.0)
    else:
        blob(b, V((0, 0, z - 0.05)), V((0.28, 0.16, 0.16)), KNOCHEN, seed + 77, rough=0.2, rings=8, sides=12, sink=0.0)
    b.finish(obj)
    gk.export(obj)
    return obj


def build_grave(name, seed, sword):
    obj = gk.new_object(name, SLOTS)
    b = gk.Builder()
    random.seed(seed)
    blob(b, V((0, 0, 0)), V((1.2, 0.6, 0.45)), ERDE, seed, rough=0.2, rings=10, sides=18, flat=0.7, sink=0.25)
    for k in range(12):
        a = 2 * math.pi * k / 12 + jitter(0.15)
        stone(b, V((math.cos(a) * 1.35, math.sin(a) * 0.8, 0)), V((0.2, 0.16, 0.13)) * random.uniform(0.8, 1.3), seed + k)
    if sword:
        blade = [V((-0.9, 0.0, -0.2)), V((-0.9, 0.0, 0.85))]
        b.tube(blade, [0.035, 0.03], METALL, sides=4, flat=0.25)
        b.tube([V((-0.9, -0.2, 0.85)), V((-0.9, 0.2, 0.85))], [0.03, 0.03], METALL, sides=5)
        b.tube([V((-0.9, 0.0, 0.85)), V((-0.9, 0.0, 1.05))], [0.03, 0.025], LEDER, sides=6)
    else:
        pole(b, V((-0.95, 0.0, -0.3)), V((-0.92, 0.04, 1.2)), 0.06, HOLZ, seed + 3)
        pole(b, V((-0.92, -0.32, 0.9)), V((-0.92, 0.36, 0.95)), 0.04, HOLZ, seed + 4)
    b.finish(obj)
    gk.export(obj)
    return obj


def plank(b, a, c, width, thick, seed, material=HOLZ):
    b.tube([a, c], [width, width], material, sides=4, flat=thick / width, rough=0.05, seed=seed)
    b.tube([c, a], [width, width], material, sides=4, flat=thick / width, rough=0.05, seed=seed + 100)


def wheel(b, centre, axis_normal, radius, seed):
    n = axis_normal.normalized()
    u = n.orthogonal().normalized()
    w = n.cross(u)
    rim = [centre + (u * math.cos(2 * math.pi * k / 20) + w * math.sin(2 * math.pi * k / 20)) * radius for k in range(21)]
    b.tube(rim, [0.05] * 21, HOLZ, sides=6, flat=0.7, rough=0.08, seed=seed)
    for k in range(8):
        if random.random() < 0.2:
            continue
        a = 2 * math.pi * k / 8
        b.tube([centre, centre + (u * math.cos(a) + w * math.sin(a)) * radius * 0.95], [0.03, 0.025], HOLZ, sides=5)
    b.tube([centre - n * 0.12, centre + n * 0.12], [0.1, 0.1], HOLZ, sides=8)
    b.tube(rim[::5], [0.055] * len(rim[::5]), METALL, sides=6, flat=0.4)


def build_cart(name, seed):
    obj = gk.new_object(name, SLOTS)
    b = gk.Builder()
    random.seed(seed)
    # bed on its side: planks stand up along Y, tilted 70 degrees
    tilt = math.radians(70)
    for k in range(6):
        h = 0.1 + k * 0.2
        y0, y1 = -1.3 + jitter(0.05), 1.3 + jitter(0.05)
        z = math.sin(tilt) * h
        x = math.cos(tilt) * h
        if k == 4:
            y1 = 0.3                                       # a broken plank
        plank(b, V((x, y0, z)), V((x + jitter(0.02), y1, z + jitter(0.02))), 0.1, 0.03, seed + k)
    # side boards and the axle stub
    plank(b, V((0.05, -1.3, 0.05)), V((0.05, -1.3, 1.2)), 0.08, 0.03, seed + 20)
    plank(b, V((0.05, 1.3, 0.05)), V((0.05, 1.3, 1.2)), 0.08, 0.03, seed + 21)
    b.tube([V((-0.15, -0.9, 1.2)), V((-0.6, -0.9, 1.25))], [0.06, 0.06], HOLZ, sides=6)
    wheel(b, V((-0.75, -0.9, 1.25)), V((1, 0, 0.05)), 0.65, seed + 30)
    wheel(b, V((1.4, 0.8, 0.08)), V((0.05, 0.1, 1)), 0.65, seed + 31)
    # snapped shaft
    b.tube([V((0.3, 1.3, 0.2)), V((0.6, 2.4, 0.12)), V((0.75, 2.9, 0.05))], [0.06, 0.055, 0.05], HOLZ, sides=6, rough=0.1)
    b.tube([V((1.0, 3.3, 0.04)), V((1.6, 4.1, 0.05))], [0.05, 0.045], HOLZ, sides=6, rough=0.1)
    # spilled load: a sack and a few stones
    blob(b, V((1.1, -0.6, 0)), V((0.4, 0.3, 0.22)), LEDER, seed + 40, rough=0.2, rings=8, sides=12, sink=0.05)
    for k in range(4):
        stone(b, V((1.5 + jitter(0.5), -0.2 + jitter(0.7), 0)), V((0.15, 0.12, 0.1)), seed + 50 + k)
    b.finish(obj)
    gk.export(obj)
    return obj


COLOURS = {"Holz": (0.2, 0.12, 0.06), "Leder": (0.36, 0.26, 0.16), "Metall": (0.12, 0.12, 0.13), "Faden": (0.55, 0.48, 0.34),
           "Glut": (1.0, 0.35, 0.05), "Fels": (0.16, 0.15, 0.14), "Erde": (0.14, 0.11, 0.09), "Knochen": (0.62, 0.58, 0.5), "Kohle": (0.04, 0.035, 0.03)}


def render_preview(objects, path):
    for name, colour in COLOURS.items():
        mat = bpy.data.materials.get(name)
        if mat is None:
            continue
        mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.85
        if name == "Glut":
            bsdf.inputs["Emission Color"].default_value = (*colour, 1.0)
            bsdf.inputs["Emission Strength"].default_value = 4.0
    cols = 5
    for k, obj in enumerate(objects):
        obj.location = ((k // cols) * -8.0, (k % cols) * 7.0, 0.0)
    centre = V((-8.0, 14.0, 0.0))
    bpy.ops.mesh.primitive_plane_add(size=120, location=centre)
    ground = bpy.context.active_object
    gmat = bpy.data.materials.new("Boden")
    gmat.use_nodes = True
    gmat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.24, 0.22, 0.2, 1)
    ground.data.materials.append(gmat)
    bpy.ops.object.light_add(type="SUN", rotation=(math.radians(40), math.radians(15), math.radians(200)))
    bpy.context.active_object.data.energy = 3.5
    scene = bpy.context.scene
    scene.world = bpy.data.worlds.new("Welt")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.45, 0.42, 0.4, 1)
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.8
    direction = V((-1.0, -1.0, -1.15)).normalized()
    bpy.ops.object.camera_add(location=centre - direction * 60.0)
    cam = bpy.context.active_object
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 36.0
    scene.camera = cam
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 40
    scene.render.resolution_x, scene.render.resolution_y = 1800, 1100
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    gk.log("Vorschau -> " + path)


if __name__ == "__main__":
    os.makedirs(gk.OUT, exist_ok=True)
    gk.clear_scene()
    built = [
        build_slag("SM_AM_Schlacke_A", V((0.6, 0.5, 0.45)), 1),
        build_slag("SM_AM_Schlacke_B", V((1.1, 0.8, 0.8)), 2),
        build_slag("SM_AM_Schlacke_C", V((1.6, 1.2, 1.5)), 3),
        build_crack("SM_AM_Glutspalte_A", 6.0, 4),
        build_crack("SM_AM_Glutspalte_B", 9.0, 5),
        build_stakes("SM_AM_Pfaehle_A", 7, 6),
        build_stakes("SM_AM_Pfaehle_B", 4, 7),
        build_ribcage("SM_AM_Gerippe_A", 8, False),
        build_ribcage("SM_AM_Gerippe_B", 9, True),
        build_cairn("SM_AM_Wegmarke_A", 10, True),
        build_cairn("SM_AM_Wegmarke_B", 11, False),
        build_grave("SM_AM_Grabhuegel_A", 12, False),
        build_grave("SM_AM_Grabhuegel_B", 13, True),
        build_cart("SM_AM_Karrenwrack", 14),
    ]
    render_preview(built, os.path.join(gk.OUT, "vorschau_kulisse.png"))
    gk.log("Kulisse der Aschenmark fertig")
