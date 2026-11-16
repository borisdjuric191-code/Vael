"""
The engineer's shots as real pieces (goal 16.11.: "die ersten Munitionstypen visuell bauen"), procedural (no Tripo), exported as
FBX for Scripts/ImportEngineerShots.py. One shape for every hull of the formulas (Hülle + Kern + Verstärker); the core shows in the
colour of the formula through the slot "Kern", which the game tints and lets glow (AVaelEngineerShot).

  SM_Ing_Ladung_Schlacke   a lump of slag, cracked open: the glowing core shows in the cracks, two turns of iron wire round it
  SM_Ing_Ladung_Blasenhaut a bladder of the Aschblase, wrinkled, tied at the neck with cord, a tuft of skin above; glowing seams
  SM_Ing_Ladung_Latex      a drop of Sternmilch latex, the filling shimmers through, knotted at its tail and bound with thread
  SM_Ing_Ladung_Glas       an ampoule of slag glass cut in facets, the core floating inside, a wax plug with drips
  SM_Ing_Ladung_Kiel       a needle of Hohlkiel: a sharpened quill with the core behind its tip and three vanes of soot feather
  SM_Ing_Stein             the plain sling stone: a round river pebble, flattened, with a few chips
  SM_Ing_Spur              the streak behind a shot in flight: two crossed ribbons, head at the origin, 1 m long along -X,
                           U runs from the head (0) to the tail (1) so the material can fade it out

Slots: 0 Schlacke, 1 Eisen, 2 Faden, 3 Blase, 4 Glas, 5 Wachs, 6 Kiel, 7 Feder, 8 Kern, 9 Latex, 10 Stein, 11 Spur.
Sizes are real (about 5 cm across); the shots scale them to their size in flight. Pivot in the middle; the needle points to +X.

  blender -b --factory-startup -P vael_ingenieur_ladungen.py -- C:/Projekte/Vael_Quellen/Ingenieur/export
"""
import math
import os
import random
import sys

import bpy
from mathutils import Vector, noise

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Refugium"))
import glutkammer_probe as gk  # noqa: E402

SLOTS = ["Schlacke", "Eisen", "Faden", "Blase", "Glas", "Wachs", "Kiel", "Feder", "Kern", "Latex", "Stein", "Spur"]
SCHLACKE, EISEN, FADEN, BLASE, GLAS, WACHS, KIEL, FEDER, KERN, LATEX, STEIN, SPUR = range(len(SLOTS))
V = Vector


def ball(b, radius, material, shape=None, rings=14, sides=20, keep=None, mask=0.0, seed=0):
    """A closed ball of rows (v pole to pole); shape(dir, v, u) returns the radius scale, keep(dir) can leave faces out (cracks)"""
    rows = []
    for j in range(rings + 1):
        v = j / rings
        theta = math.pi * v
        row = []
        for i in range(sides):
            u = i / sides
            phi = 2 * math.pi * u
            d = V((math.sin(theta) * math.cos(phi), math.sin(theta) * math.sin(phi), math.cos(theta)))
            s = shape(d, v, u) if shape else 1.0
            row.append(d * radius * s)
        rows.append(row)
    if keep is None:
        b.grid(rows, material, uv_of=lambda j, i: (i / sides, j / rings), mask_of=lambda j, i: mask, closed_u=True)
        return rows
    # only the faces whose middle is kept: a shell with holes
    for j in range(rings):
        for i in range(sides):
            i2 = (i + 1) % sides
            quad = [rows[j][i], rows[j][i2], rows[j + 1][i2], rows[j + 1][i]]
            middle = sum(quad, V()) / 4.0
            if not keep(middle.normalized()):
                continue
            b.grid([[quad[0], quad[1]], [quad[3], quad[2]]], material, uv_of=lambda jj, ii: (ii, jj), mask_of=lambda jj, ii: mask)
    return rows


def ring(b, centre, radius, axis_z, thickness, material, turns=1.0, rise=0.0, sides=5):
    """A wire or cord wound round an axis (turns, rising by rise per turn)"""
    pts = []
    steps = int(24 * turns) + 1
    side = V((1, 0, 0)) if abs(axis_z.x) < 0.9 else V((0, 1, 0))
    a = axis_z.cross(side).normalized()
    c = axis_z.cross(a).normalized()
    for k in range(steps):
        t = k / (steps - 1)
        ang = 2 * math.pi * turns * t
        pts.append(centre + (a * math.cos(ang) + c * math.sin(ang)) * radius + axis_z * rise * turns * t)
    b.tube(pts, [thickness] * len(pts), material, sides=sides, cap_end=False)


# ---------------------------------------------------------------- hulls

def build_schlacke():
    """Slag cracked open by the heat of its core: a dark porous shell, wide cracks, the core glowing inside"""
    obj = gk.new_object("SM_Ing_Ladung_Schlacke", SLOTS)
    b = gk.Builder()
    random.seed(3)
    base = lambda d: 1.0 + 0.16 * noise.noise(d * 2.2 + V((1.3, 0, 0))) + 0.05 * noise.noise(d * 9.0)  # noqa: E731
    # cracks: jagged lines wandering over the lump, branching once; the shell sinks into a groove along them and the
    # core glows out of the groove
    cracks = []
    for k in range(6):
        d = V((random.uniform(-1, 1), random.uniform(-1, 1), random.uniform(-1, 1))).normalized()
        heading = d.cross(V((random.uniform(-1, 1), random.uniform(-1, 1), random.uniform(-1, 1)))).normalized()
        line = [d]
        for s in range(9):
            heading = (heading + d.cross(heading) * random.uniform(-0.9, 0.9)).normalized()
            d = (d + heading * 0.16).normalized()
            heading = (heading - d * heading.dot(d)).normalized()
            line.append(d)
        cracks.append(line)
        branch = line[random.randint(3, 6)]
        side = branch.cross(heading).normalized()
        cracks.append([branch, (branch + side * 0.14).normalized(), (branch + side * 0.26 + heading * 0.08).normalized()])

    def near_crack(d):
        best = 1.0
        for line in cracks:
            for a, c in zip(line, line[1:]):
                ab = c - a
                t = max(0.0, min(1.0, (d - a).dot(ab) / max(ab.length_squared, 1e-9)))
                best = min(best, (a + ab * t - d).length)
        return best

    lump = lambda d, v, u: base(d) * (1.0 - 0.09 * math.exp(-(near_crack(d) / 0.045) ** 2))  # noqa: E731
    ball(b, 0.024, SCHLACKE, shape=lump, rings=30, sides=44)
    for line in cracks:
        pts = [d * 0.024 * base(d) * 0.962 for d in line]
        b.tube(pts, [0.0016 * (1.0 - 0.5 * abs(2 * k / (len(pts) - 1) - 1)) for k in range(len(pts))], KERN, sides=5, mask_of=lambda t: 1.0)
    # two turns of iron wire, crossed, holding the cracked lump together
    ring(b, V((0, 0, 0)), 0.0262, V((0.2, 0.0, 1.0)).normalized(), 0.0016, EISEN, sides=5)
    ring(b, V((0, 0, 0)), 0.0262, V((0.0, 1.0, 0.25)).normalized(), 0.0016, EISEN, sides=5)
    b.finish(obj)
    gk.export(obj)


def build_blasenhaut():
    """A bladder of the Aschblase: wrinkled skin, pinched at the neck and tied with cord, a tuft of skin above, glowing seams"""
    obj = gk.new_object("SM_Ing_Ladung_Blasenhaut", SLOTS)
    b = gk.Builder()

    def bag(d, v, u):
        # round below, drawn together to a neck at the top pole; wrinkles running down from the neck
        neck = max(0.0, 1.0 - v / 0.22)
        wrinkle = 0.05 * math.sin(u * 2 * math.pi * 7 + 2.0 * v) * (1.0 - v) * (1.0 - neck)
        return (1.0 - 0.78 * neck ** 1.6) * (1.0 + wrinkle) * (1.0 + 0.06 * noise.noise(d * 4.0))
    rows = ball(b, 0.025, BLASE, shape=bag, rings=18, sides=24)
    neck_z = rows[3][0].z
    # the tuft: the gathered skin above the cord, a short frill
    tuft = []
    for j in range(5):
        t = j / 4
        r = 0.005 + 0.006 * t
        tuft.append([V((r * math.cos(2 * math.pi * i / 16) * (1 + 0.25 * math.sin(i * 2.4)), r * math.sin(2 * math.pi * i / 16) * (1 + 0.25 * math.sin(i * 2.4)),
                        neck_z + 0.002 + 0.011 * t)) for i in range(16)])
    b.grid(tuft, BLASE, uv_of=lambda j, i: (i / 16, j / 4), mask_of=lambda j, i: 0.0, closed_u=True)
    b.grid([list(reversed(r)) for r in tuft], BLASE, uv_of=lambda j, i: (i / 16, j / 4), mask_of=lambda j, i: 0.0, closed_u=True)
    ring(b, V((0, 0, neck_z - 0.002)), 0.0058, V((0, 0, 1)), 0.0012, FADEN, turns=3.0, rise=0.0016, sides=4)
    # seams that glow with what is inside: four meridians from the neck down to the bottom
    for k in range(4):
        u = k / 4 + 0.06
        seam = []
        for j in range(4, 19):
            v = j / 18
            theta = math.pi * v
            phi = 2 * math.pi * u
            d = V((math.sin(theta) * math.cos(phi), math.sin(theta) * math.sin(phi), math.cos(theta)))
            seam.append(d * 0.025 * bag(d, v, u) * 1.01)
        b.tube(seam, [0.0011] * len(seam), KERN, sides=4, cap_end=False)
    b.finish(obj)
    gk.export(obj)


def build_latex():
    """A drop of latex, the filling shimmering through, knotted at its tail and bound with thread"""
    obj = gk.new_object("SM_Ing_Ladung_Latex", SLOTS)
    b = gk.Builder()
    profile = []
    radius = 0.021
    for k in range(17):
        t = k / 16
        # a drop: a round belly below, drawn out to a neck at the top where it is knotted (the shot tumbles anyway)
        z = -radius + 2.3 * radius * t
        r = radius * math.sin(math.pi * t) if t <= 0.5 else radius * (1.0 - 1.64 * (t - 0.5) ** 1.15)
        profile.append((max(r, 0.0001 if k == 0 else 0.0035), z))
    rows = [[V((r * math.cos(2 * math.pi * i / 22), r * math.sin(2 * math.pi * i / 22), z)) for i in range(22)] for r, z in profile]
    b.grid(rows, LATEX, uv_of=lambda j, i: (i / 22, j / 16), mask_of=lambda j, i: 0.0, closed_u=True)
    # the filling inside, seen through the latex
    ball(b, 0.016, KERN, shape=lambda d, v, u: 1.0 - 0.2 * max(0.0, d.z), rings=10, sides=14, mask=1.0)
    # the knot: a twisted nub, and thread wound below it
    top = profile[-1][1]
    knot = [V((0.0, 0.0, top - 0.002)), V((0.002, 0.001, top + 0.004)), V((-0.001, 0.003, top + 0.008)), V((0.003, 0.002, top + 0.011))]
    b.tube(knot, [0.0042, 0.0048, 0.004, 0.0025], LATEX, sides=8)
    ring(b, V((0, 0, top - 0.004)), 0.0042, V((0, 0, 1)), 0.0009, FADEN, turns=2.5, rise=0.0012, sides=4)
    b.finish(obj)
    gk.export(obj)


def build_glas():
    """An ampoule of slag glass cut in facets, the core floating inside, a wax plug with drips running down"""
    obj = gk.new_object("SM_Ing_Ladung_Glas", SLOTS)
    b = gk.Builder()
    # few sides and rings: hard facets like cut glass
    ball(b, 0.023, GLAS, shape=lambda d, v, u: 1.0 - 0.55 * max(0.0, 1.0 - v / 0.2) ** 2, rings=8, sides=9)
    ball(b, 0.015, KERN, shape=lambda d, v, u: 1.0 + 0.12 * noise.noise(d * 3.5), rings=10, sides=14, mask=1.0)
    glass = lambda v: 0.023 * (1.0 - 0.55 * max(0.0, 1.0 - v / 0.2) ** 2)  # noqa: E731
    neck = glass(0.0) - 0.0012
    # the wax plug: a short cylinder on the neck, a rounded cap, drips running down the glass
    plug = []
    for j, (r, z) in enumerate(((0.0042, neck - 0.003), (0.0048, neck + 0.0015), (0.005, neck + 0.0045), (0.0036, neck + 0.0068), (0.0001, neck + 0.0076))):
        plug.append([V((r * math.cos(2 * math.pi * i / 14), r * math.sin(2 * math.pi * i / 14), z)) for i in range(14)])
    b.grid(plug, WACHS, uv_of=lambda j, i: (i / 14, j / 4), mask_of=lambda j, i: 0.0, closed_u=True)
    random.seed(9)
    for k in range(4):
        a = 2 * math.pi * (k / 4 + random.uniform(-0.06, 0.06))
        length = random.uniform(0.006, 0.013)
        drip = []
        for s in range(6):
            t = s / 5
            theta = math.radians(10 + 26 * t * length / 0.013)
            drip.append(V((math.sin(theta) * math.cos(a), math.sin(theta) * math.sin(a), math.cos(theta))) * (glass(theta / math.pi) + 0.0006))
        b.tube(drip, [0.0014 * (1 - 0.4 * s / 5) for s in range(6)], WACHS, sides=6)
    b.finish(obj)
    gk.export(obj)


def build_kiel():
    """A needle of Hohlkiel along +X: a sharpened quill, the core glowing behind its tip, three vanes of soot feather at the back"""
    obj = gk.new_object("SM_Ing_Ladung_Kiel", SLOTS)
    b = gk.Builder()
    pts = [V((x, 0.0, 0.0)) for x in (-0.05, -0.03, -0.01, 0.01, 0.024, 0.034, 0.042)]
    b.tube(pts, [0.0035, 0.0042, 0.0045, 0.0045, 0.004, 0.0024, 0.0004], KIEL, sides=10)
    # the core: a glowing band seen through the thin quill behind the tip, and a bead of it in the cut
    b.tube([V((0.012, 0, 0)), V((0.026, 0, 0))], [0.0049, 0.0046], KERN, sides=10, cap_end=False)
    # the vanes: three soot feathers along the back, curving a little
    for k in range(3):
        a = 2 * math.pi * k / 3
        d = V((0.0, math.cos(a), math.sin(a)))
        rows = []
        for j in range(5):
            t = j / 4
            x = -0.05 + 0.03 * t
            width = 0.011 * math.sin(math.pi * (0.15 + 0.85 * t)) * (1.0 - 0.3 * t)
            rows.append([V((x, 0, 0)) + d * 0.004, V((x - 0.003, 0, 0)) + d * (0.004 + width)])
        b.grid(rows, FEDER, uv_of=lambda j, i: (i, j / 4), mask_of=lambda j, i: 0.0)
        b.grid([list(reversed(r)) for r in rows], FEDER, uv_of=lambda j, i: (i, j / 4), mask_of=lambda j, i: 0.0)
    ring(b, V((-0.019, 0, 0)), 0.0047, V((1, 0, 0)), 0.0008, FADEN, turns=3.0, rise=0.0011, sides=4)
    b.finish(obj)
    gk.export(obj)


def build_stein():
    """The plain sling stone: a river pebble, flattened, with a few chips; grey stone"""
    obj = gk.new_object("SM_Ing_Stein", SLOTS)
    b = gk.Builder()
    pebble = lambda d, v, u: (1.0 + 0.07 * noise.noise(d * 2.0 + V((7, 1, 2))) - 0.05 * max(0.0, noise.noise(d * 6.0))) * (0.78 if abs(d.z) > 0.0 else 1.0) ** abs(d.z)  # noqa: E731
    ball(b, 0.022, STEIN, shape=pebble, rings=12, sides=18)
    b.finish(obj)
    # flatten it a little along Z, like a stone from a stream
    for v in obj.data.vertices:
        v.co.z *= 0.72
    gk.export(obj)


def build_spur():
    """The streak behind a shot: two crossed ribbons from the head (origin, full width) back to the tail at -X (a point).
    U = 0 at the head, 1 at the tail; V across the ribbon (0 .. 1)."""
    obj = gk.new_object("SM_Ing_Spur", SLOTS)
    b = gk.Builder()
    for axis in (V((0, 1, 0)), V((0, 0, 1))):
        rows = []
        for k in range(9):
            t = k / 8
            half = 0.5 * (1.0 - t) ** 0.8
            rows.append([V((-t, 0, 0)) - axis * half, V((-t, 0, 0)), V((-t, 0, 0)) + axis * half])
        b.grid(rows, SPUR, uv_of=lambda j, i: (j / 8, i / 2), mask_of=lambda j, i: j / 8)
        b.grid([list(reversed(r)) for r in rows], SPUR, uv_of=lambda j, i: (j / 8, 1 - i / 2), mask_of=lambda j, i: j / 8)
    b.finish(obj)
    for poly in obj.data.polygons:
        poly.use_smooth = False
    gk.export(obj)


def preview(out):
    """A picture of all shots side by side for the morning check (Eevee, warm light)"""
    gk.clear_scene()
    for name in ("SM_Ing_Stein", "SM_Ing_Ladung_Schlacke", "SM_Ing_Ladung_Blasenhaut", "SM_Ing_Ladung_Latex", "SM_Ing_Ladung_Glas", "SM_Ing_Ladung_Kiel"):
        bpy.ops.import_scene.fbx(filepath=os.path.join(out, name + ".fbx"))
    colours = {"Schlacke": (0.05, 0.045, 0.04, 1), "Eisen": (0.25, 0.25, 0.27, 1), "Faden": (0.5, 0.4, 0.25, 1), "Blase": (0.32, 0.27, 0.22, 1),
               "Glas": (0.55, 0.5, 0.42, 1), "Wachs": (0.45, 0.08, 0.06, 1), "Kiel": (0.8, 0.74, 0.6, 1), "Feder": (0.05, 0.05, 0.06, 1),
               "Kern": (1.0, 0.45, 0.1, 1), "Latex": (0.85, 0.82, 0.66, 1), "Stein": (0.4, 0.38, 0.35, 1), "Spur": (1, 1, 1, 1)}
    for mat in bpy.data.materials:
        key = mat.name.split(".")[0]
        if key in colours:
            mat.use_nodes = True
            bsdf = mat.node_tree.nodes.get("Principled BSDF")
            if bsdf:
                bsdf.inputs["Base Color"].default_value = colours[key]
                if key == "Kern":
                    bsdf.inputs["Emission Color"].default_value = colours[key]
                    bsdf.inputs["Emission Strength"].default_value = 6.0
                if key == "Glas":
                    bsdf.inputs["Transmission Weight"].default_value = 0.85
                    bsdf.inputs["Roughness"].default_value = 0.05
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    for k, o in enumerate(sorted(meshes, key=lambda o: o.name)):
        o.location = (k * 0.07 - 0.175, 0, 0)
        o.rotation_euler = (0.3, 0.2, 0.4)
    bpy.ops.object.camera_add(location=(0, -0.42, 0.12), rotation=(math.radians(76), 0, 0))
    cam = bpy.context.active_object
    cam.data.lens = 50
    bpy.context.scene.camera = cam
    bpy.ops.object.light_add(type="AREA", location=(0.1, -0.2, 0.3))
    bpy.context.active_object.data.energy = 30
    bpy.ops.object.light_add(type="POINT", location=(-0.2, 0.1, 0.1))
    bpy.context.active_object.data.energy = 6
    bpy.context.active_object.data.color = (1.0, 0.6, 0.3)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
    scene.render.resolution_x, scene.render.resolution_y = 1400, 420
    world = bpy.data.worlds.new("W")
    world.color = (0.03, 0.025, 0.02)
    scene.world = world
    scene.render.filepath = os.path.join(out, "vorschau_ladungen.png")
    bpy.ops.render.render(write_still=True)
    gk.log("Vorschau: " + scene.render.filepath)


if __name__ == "__main__":
    for build in (build_schlacke, build_blasenhaut, build_latex, build_glas, build_kiel, build_stein, build_spur):
        gk.clear_scene()
        build()
    try:
        preview(gk.OUT)
    except Exception as error:  # the preview is only for looking, never stops the export
        gk.log("Vorschau fehlgeschlagen: {}".format(error))
    gk.log("Ladungen des Ingenieurs fertig")
