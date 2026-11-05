# Die Bausteine des Keimgeflechts im Spiel nach Entwurf D (Boris 2026-11-05: "Ja, ins Spiel"). Der feste Hintergrund (Kammer,
# Titel, Lichtstrahlen, Fäden der Schaugläser, der Erbstrang mit Motte und Blattrosetten, das Pergament mit seinen festen
# Überschriften, Zeichen und Stammbaum) wird als ein Bild gerendert; alles, was sich ändert oder bewegt, als freigestelltes
# Einzelteil, das Source/Vael/UI/VaelHUDKeimgeflecht.cpp darüber zeichnet:
#
#   T_Keim_Grund                der Hintergrund, 1920 x 1080
#   T_Keim_Glas / _Gold         Fassung und Glas eines Schauglases im Herbarium (schwingt am Knauf)
#   T_Keim_Kapsel               Glas und Rand einer Kapsel am Strang
#   T_Keim_Knospe               die verschlossene Knospe eines noch nicht gewachsenen Radplatzes
#   T_Keim_Same / _Zu           die Samenkapsel einer Erbanlage (offen / Stufe noch zu)
#   T_Keim_Bluete               die aufgeblühte, gewählte Erbanlage mit Goldring
#   T_Keim_Zeichen_<Art>        weiße Zeichen (Dorn, Wirbel, Tropfen, Nid) zum Einfärben
#   T_Keim_Schein               ein weicher Lichthof (Saftperlen, Glühwürmchen, Pollen)
#   T_Keim_Rund_<Id>            die Pflanzenbilder rund (Kapseln, Kreuzungen, Schattenform)
#   T_Keim_Oval_<Id>            die Pflanzenbilder oval (Schaugläser, Miniatur der Tafel)
#
# blender.exe -b --factory-startup -P Tools/HUD/vael_keimgeflecht_spiel.py -- <Ordner>   (danach Scripts/CreateKeimgeflechtKit.py)
# Die Lagen hier und im C++ gehören zusammen: wer eine verschiebt, verschiebt die andere mit.

import math
import os
import random
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vael_keimgeflecht_d as D  # noqa: E402
import vael_tusche as T  # noqa: E402

C = D.C
K = D.K
rnd = random.Random(2026)

# Lagen in Designpixeln (wie im C++)
STRAND_X, STRAND_TOP, STRAND_BOTTOM, STRAND_AMP = 790, 214, 972, 46
SHEET = (990, 150, 1872, 1000)


def patch():
    K.leaf = D.detailed_leaf
    K.moth_emblem = D.detailed_moth
    K.alive_room = D.detailed_room
    K.aged_paper = papyrus_mat


def lights():
    scene = bpy.context.scene
    scene.view_settings.exposure = -0.85
    bpy.data.objects["Key"].data.energy *= 0.6
    bpy.data.objects["Fill"].data.energy *= 0.5


def room_lights():
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


# ---------------------------------------------------------------- Hintergrund

def build_background():
    ink = K.mat("Tinte", (0.022, 0.014, 0.008), 0.6)
    ink_faint = K.mat("TinteBlass", (0.06, 0.04, 0.025), 0.7)
    pale = K.mat("Hell", (0.88, 0.82, 0.68), 0.6, emit=1.0)
    dim = K.mat("Dunkeltext", (0.5, 0.46, 0.4), 0.6, emit=0.6)
    gold_text = K.mat("Titelgold", (0.95, 0.74, 0.36), 0.3, 0.6, emit=1.3)
    violet = K.mat("Nidtropfen", (0.6, 0.3, 0.9), 0.3, emit=1.4)

    # Tiefe wie in C/D: Grund, weiche Lichter, Lichtbahnen, Wurzelkammer, Vignette, dunkles Laub ganz vorn
    K.quad("Grund", 0, 0, K.W, K.H, K.WOOD, depth=60)
    for _ in range(26):
        x, y = rnd.uniform(0, K.W), rnd.uniform(0, K.H)
        color = rnd.choice(((0.3, 0.9, 0.6), (0.9, 0.6, 0.8), (0.95, 0.8, 0.5)))
        r = rnd.uniform(40, 130)
        strength = rnd.uniform(0.08, 0.22)
        if x < 600 and y > 750:
            continue  # dieser Lichtfleck zeichnete sich unten links als Viereck ab
        K.glow(x, y, r, K.glow_mat("Bokeh", color, strength), depth=58)
    # Die flachen Lichtbahnen aus C fallen weg: die echten Lichtstrahlen (god_rays) ersetzen sie, und ihre Kanten zeichneten
    # sich unten links als graue Fläche ab
    K.alive_room()
    frame_roots()
    # Vignette erst nach dem Rendern (T.vignette_png), weicher und gleichmäßiger als eine Fläche vor der Kamera
    for x, y, a, l in ((-30, 90, 25, 200), (-20, 1060, -35, 220), (1950, 40, 160, 190)):
        K.leaf(x, y, a, l, l * 0.42, K.mat("Vorderblatt", (0.008, 0.02, 0.008), 0.8), depth=-60, bend=0.2)

    # Titel mit Ranke
    K.text("Keimgeflecht", K.W / 2, 44, 52, gold_text, "AlmendraSC-Regular.ttf", "CENTER", depth=-20)
    curve = [(K.W / 2 - 220 + k * 20, 112 + 5 * math.sin(k * 0.9)) for k in range(23)]
    K.tube("Titelranke", curve, 1.6, K.GOLD, depth=-18)
    K.leaf(K.W / 2 - 230, 112, 200, 24, 10, K.LEAF, depth=-18)
    K.leaf(K.W / 2 + 230, 112, -20, 24, 10, K.LEAF, depth=-18)

    # Herbarium: nur die Überschrift und die Fäden; Gläser, Bilder und Namen zeichnet das Spiel
    K.text("Herbarium", 330, 150, 34, pale, "AlmendraSC-Regular.ttf", "CENTER", depth=-20)
    for i in range(12):
        col, row = i % 3, i // 3
        x = 170 + col * 160
        y = 300 + row * 178 + (40 if col == 1 else 0)
        K.tube("Faden", [(x + rnd.uniform(-10, 10), y - 64 - 100), (x, y - 80)], 1.5, K.mat("Faden", (0.45, 0.36, 0.24), 0.9), depth=8)

    # Erbstrang: beide Ranken, Sprossen, Motte, Stiele und Blattrosetten aller acht Plätze (Kapseln zeichnet das Spiel)
    cx, top, bottom, amp = STRAND_X, STRAND_TOP, STRAND_BOTTOM, STRAND_AMP
    tall = K.quad("Herzschein", cx - 260, top - 120, cx + 260, bottom + 80, K.glow_mat("Herz", (0.4, 1.0, 0.55), 0.35), depth=30)
    tall.visible_shadow = False
    steps = 180

    def strand(phase0, radius, material, leaves, thorns):
        piece, front = [], None
        for k in range(steps + 1):
            t = k / steps
            y = top + (bottom - top) * t
            phase = t * 4.4 * math.pi + phase0
            x = cx + amp * math.sin(phase) + 3.0 * math.sin(t * 37.0 + phase0)
            now = math.cos(phase) > 0
            if front is None:
                front = now
            piece.append((x, y))
            if now != front or k == steps:
                C.braid(piece, radius * (1.0 + 0.15 * math.sin(t * 23.0)), material, -10 if front else 8)
                piece, front = [(x, y)], now
            if k % 8 == 3 and now:
                side = 1 if math.sin(phase) > 0 else -1
                if leaves:
                    K.leaf(x, y, (0 if side > 0 else 180) + side * (-30 + 25 * math.sin(k)), 30, 14, K.LEAF, depth=-14)
                    if k % 24 == 3:
                        K.blossom(x - side * 6, y + 8, 10, K.PETAL, K.GOLD)
                if thorns:
                    K.tube("Dorn", [(x, y), (x + side * 13, y + 3)], 2.8, material, depth=-12, taper=True)

    # Realistisch (Boris 2026-11-05): Lichtstrang aus lebendem grünem Holz mit Rinde, Schattenstrang eine dunkle, verdrehte Wurzel
    living_wood = K.noisy("Lichtholz", (0.05, 0.10, 0.035), (0.20, 0.30, 0.11), 0.09, detail=12.0, rough=0.55, stripes=1.2, emit=0.12)
    strand(0.0, 9.0, living_wood, True, False)
    strand(math.pi, 8.0, D.bark_mat("Schattenwurzel"), False, True)
    for k in range(0, steps, 7):
        t = k / steps
        y = top + (bottom - top) * t
        phase = t * 4.4 * math.pi
        a, b = cx + amp * math.sin(phase), cx + amp * math.sin(phase + math.pi)
        if abs(a - b) > 26:
            K.tube("Sprosse", [(a, y), ((a + b) / 2, y + 5), (b, y)], 1.6, K.mat("Wurzelfaser", (0.22, 0.26, 0.15), 0.7, emit=0.12), depth=0)
    K.moth_emblem(cx, top - 30, 1.15)
    for i in range(8):
        y = top + 50 + i * 92
        side = -1 if i % 2 == 0 else 1
        px = cx + side * 106
        K.tube("Stiel", [(cx + side * 30, y + 12), (cx + side * 58, y + 2), (px - side * 42, y)], 3.2, living_wood, depth=2)
        for a in range(6):
            K.leaf(px, y, a * 60 + 90, 52, 24, K.LEAF_DARK, depth=6, bend=0.12)
        K.disc("Kapselgrund", px, y, 38, K.PAPER_DARK, depth=3)
        K.text(str(i + 1), px + side * 58, y - 13, 20, pale if i < 4 else dim, "AlegreyaSans-Bold.ttf", "CENTER", depth=-14)

    # Die Tafel: Pergament gerade (damit die Schrift des Spiels aufliegt), Miniaturfenster, feste Überschriften
    x0, y0, x1, y1 = SHEET
    C.sheet("Pergament", x0, y0, x1, y1, K.PAPER, depth=14)
    mx, my, mrx, mry = x0 + 210, y0 + 375, 150, 185
    K.glow(mx, my, 300, K.glow_mat("Miniaturschein", (0.95, 0.85, 0.6), 0.25), depth=8)
    C.oval_disc("Miniaturgrund", mx, my, mrx, mry, K.mat("Fenster", (0.02, 0.025, 0.02), 0.6), depth=2)
    C.oval_ring("Haarlinie", mx, my, mrx + 6, mry + 6, 1.4, K.GOLD, depth=0)
    C.oval_ring("Haarlinie2", mx, my, mrx + 12, mry + 12, 0.6, ink, depth=0)
    # Tuschefassungen wie auf einer Traktat-Seite: um die Werte und die Kreuzungen, Zierlinie unter dem Namen
    ink_frame(x0 + 40, y0 + 572, x0 + 502, y0 + 664, ink_faint)
    ink_frame(x0 + 40, y0 + 668, x0 + 502, y0 + 826, ink_faint)
    flourish = [(x0 + 56 + k * 12, y0 + 178 + 3 * math.sin(k * 0.8)) for k in range(36)]
    K.tube("Zierlinie", flourish, 0.9, ink, depth=4)
    for fx, ang in ((x0 + 56, 180), (x0 + 476, 0)):
        K.leaf(fx, y0 + 178, ang, 18, 7, K.LEAF, depth=2)

    # Werte: Zeichen, Namen und leere Balken; Zahlen und Füllung zeichnet das Spiel
    stats = [("dorn", "Kraft"), ("wirbel", "Tempo"), ("tropfen", "Saft"), ("nid", "Nid")]
    for k, (kind, label) in enumerate(stats):
        sx, sy = x0 + 64 + k * 112, y0 + 594
        C.icon(kind, sx + 12, sy + 12, 1.3, violet if kind == "nid" else K.mat("Zeichen", (0.12, 0.3, 0.1), 0.5, emit=0.2))
        K.text(label, sx + 34, sy - 2, 17, ink_faint, "Almendra-Regular.ttf", depth=4)
        K.tube("Balken", [(sx, sy + 48), (sx + 92, sy + 48)], 1.0, ink_faint, depth=2)

    erbgut_tree(x0 + 690, y0 + 50, ink, ink_faint)

    K.text("Kreuzungen", x0 + 56, y0 + 676, 28, ink, "AlmendraSC-Regular.ttf", depth=4)


# ---------------------------------------------------------------- Rahmen und Erbgut (Boris 2026-11-05: rechte Tafelseite zu
# einfach, Äste und Wurzeln am Rahmen mehr und länger)

def god_rays():
    """Wie in Entwurf D, aber der linke Lichtkegel fällt steiler hinter den Strang statt als harte helle Fläche unten links"""
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
    for k, (x, ang, energy) in enumerate(((700, -7, 2.2e8), (860, -6, 3.0e8), (1180, 8, 1.6e8))):
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



def wander(start, end, count, jitter, sag=0.0):
    """Ein Weg von start nach end mit Durchhang und unruhigem Verlauf"""
    pts = []
    for k in range(count):
        t = k / (count - 1)
        x = start[0] + (end[0] - start[0]) * t
        y = start[1] + (end[1] - start[1]) * t
        nx, ny = -(end[1] - start[1]), end[0] - start[0]
        ln = math.hypot(nx, ny) or 1.0
        off = sag * math.sin(math.pi * t) + jitter * (math.sin(t * 17.0 + start[0]) * 0.6 + math.sin(t * 41.0 + start[1]) * 0.4)
        pts.append((x + nx / ln * off, y + ny / ln * off))
    return pts


def tendril(x, y, size, turns, material, depth, sign=1):
    """Eine Ranke, die sich am Ende einrollt"""
    pts = []
    for k in range(28):
        t = k / 27
        a = sign * t * turns * 2 * math.pi
        r = size * (1.0 - 0.85 * t)
        pts.append((x + sign * size * t * 1.4 + math.cos(a) * r * t, y - size * t * 0.6 + math.sin(a) * r * t))
    K.tube("Ranke", pts, 1.1, material, depth=depth, taper=True)


def frame_roots():
    """Lange, knorrige Wurzeln und Äste rings um den Rahmen, mit Seitentrieben, hängenden Würzelchen, Blättern und Ranken"""
    bark = D.bark_mat("RindeRahmen")
    vine = K.mat("RahmenRanke", (0.1, 0.26, 0.08), 0.5, emit=0.1)
    hang = K.mat("Haengewurzel", (0.22, 0.16, 0.1), 0.7)
    mains = [  # Start, Ende, Dicke, Durchhang, Tiefe
        ((-10, 14), (780, 26), 13, 14, -6), ((1930, 12), (1150, 24), 13, -12, -6),
        ((10, -10), (24, 1080), 14, 16, -6), ((1912, -10), (1902, 1090), 14, -14, -6),
        ((-10, 1066), (700, 1070), 13, -8, -6), ((1930, 1064), (1220, 1072), 13, 8, -6),
        ((30, 40), (380, 8), 7, -18, -8), ((1890, 40), (1500, 6), 7, 18, -8),
        ((30, 300), (70, 760), 6, 22, -8), ((1890, 320), (1900, 820), 6, -10, -8),
        ((640, 1076), (1280, 1076), 5, -4, -8),
    ]
    for (sx, sy), (ex, ey), r, sag, depth in mains:
        pts = wander((sx, sy), (ex, ey), 46, 5, sag)
        D.gnarled_root(pts, r, bark, depth)
        vertical = abs(ex - sx) < abs(ey - sy)
        # Seitentriebe: kürzere Wurzeln, die vom Hauptstrang abzweigen, nur im Rand
        for k in range(4, 42, 5):
            px, py = pts[k]
            inward_x = 1 if px < 960 else -1
            inward_y = 1 if py < 540 else -1
            length = rnd.uniform(60, 150) * (0.6 if r < 8 else 1.0)
            if vertical:
                ang = rnd.uniform(-0.9, 0.9)
                dx, dy = math.cos(ang) * inward_x, math.sin(ang)
            else:
                dx, dy = rnd.uniform(-0.8, 0.8), inward_y * rnd.uniform(0.2, 0.5)
            end = (px + dx * length, py + dy * length)
            if 100 < end[0] < 1880 and 130 < end[1] < 1045:
                end = (px + dx * length * 0.35, py + dy * length * 0.35)
            branch = wander((px, py), end, 14, 3, rnd.uniform(-10, 10))
            D.gnarled_root(branch, r * rnd.uniform(0.3, 0.45), bark, depth + 1)
            bx, by = branch[-4]
            if rnd.random() < 0.7:
                K.leaf(bx, by, rnd.uniform(0, 360), rnd.uniform(26, 44), rnd.uniform(11, 17), rnd.choice((K.LEAF, K.LEAF_DARK)), depth=depth - 4, bend=0.2)
            if rnd.random() < 0.4:
                tendril(bx, by, rnd.uniform(12, 22), rnd.uniform(1.2, 1.8), vine, depth - 3, rnd.choice((-1, 1)))
    # Hängende Würzelchen von der oberen Kante: an den Seiten lang, sonst kurz; nicht über Titel und Reitern
    for x in range(20, 1900, 26):
        if 780 < x < 1150 or 1380 < x < 1880 or rnd.random() < 0.35:
            continue
        top = 22 + rnd.uniform(-6, 6)
        limit = 420 if x < 100 or x > 1880 else 120 if x < 780 else 125
        length = rnd.uniform(0.35, 1.0) * limit
        pts = [(x + 4 * math.sin(k * 0.7 + x), top + length * k / 11) for k in range(12)]
        K.tube("Haengewurzel", pts, rnd.uniform(0.9, 1.8), hang, depth=-5, taper=True)
        if rnd.random() < 0.3:
            K.leaf(pts[-2][0], pts[-2][1], 90 + rnd.uniform(-30, 30), 18, 8, K.LEAF_DARK, depth=-7)
    # Blätterbüschel, wo sich die Wurzeln in den Ecken kreuzen
    for cx, cy in ((40, 40), (1880, 40), (40, 1040), (1880, 1040)):
        for _ in range(9):
            K.leaf(cx + rnd.uniform(-30, 30), cy + rnd.uniform(-30, 30), rnd.uniform(0, 360), rnd.uniform(30, 52), rnd.uniform(12, 20),
                   rnd.choice((K.LEAF, K.LEAF_DARK)), depth=-12, bend=0.25)


def wash_mat(name, tint):
    """Eine Aquarell-Lasur: färbt das Pergament darunter leicht ein, fleckig zum Rand hin verlaufend"""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    coord = nt.nodes.new("ShaderNodeTexCoord")
    mapping = nt.nodes.new("ShaderNodeMapping")
    mapping.inputs["Location"].default_value = (-1.0, -1.0, 0.0)
    mapping.inputs["Scale"].default_value = (2.0, 2.0, 0.0)
    nt.links.new(coord.outputs["UV"], mapping.inputs["Vector"])
    grad = nt.nodes.new("ShaderNodeTexGradient")
    grad.gradient_type = "SPHERICAL"
    nt.links.new(mapping.outputs["Vector"], grad.inputs["Vector"])
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 6.0
    noise.inputs["Detail"].default_value = 8.0
    nt.links.new(coord.outputs["UV"], noise.inputs["Vector"])
    mul = nt.nodes.new("ShaderNodeMath")
    mul.operation = "MULTIPLY"
    nt.links.new(grad.outputs["Fac"], mul.inputs[0])
    nt.links.new(noise.outputs["Fac"], mul.inputs[1])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.12
    ramp.color_ramp.elements[1].position = 0.45
    nt.links.new(mul.outputs[0], ramp.inputs["Fac"])
    clear = nt.nodes.new("ShaderNodeBsdfTransparent")
    tinted = nt.nodes.new("ShaderNodeBsdfTransparent")
    tinted.inputs["Color"].default_value = (*tint, 1)
    mix = nt.nodes.new("ShaderNodeMixShader")
    nt.links.new(ramp.outputs["Color"], mix.inputs["Fac"])
    nt.links.new(clear.outputs[0], mix.inputs[1])
    nt.links.new(tinted.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    return m


def ink_frame(x0, y0, x1, y1, ink):
    """Eine feine Tuschefassung mit Doppellinie und eingerollten Ecken"""
    for inset, r in ((0, 0.9), (6, 0.45)):
        K.tube("Fassung", [(x0 + inset, y0 + inset), (x1 - inset, y0 + inset), (x1 - inset, y1 - inset), (x0 + inset, y1 - inset), (x0 + inset, y0 + inset)],
               r, ink, depth=4)
    for cx, cy, sx, sy in ((x0, y0, 1, 1), (x1, y0, -1, 1), (x0, y1, 1, -1), (x1, y1, -1, -1)):
        spiral = [(cx + sx * (10 + math.cos(a) * (14 - a * 1.6)), cy + sy * (10 + math.sin(a) * (14 - a * 1.6))) for a in [k * 0.3 for k in range(26)]]
        K.tube("Ecke", spiral, 0.8, ink, depth=4)
        K.leaf(cx + sx * 4, cy + sy * 4, math.degrees(math.atan2(sy, sx)) + 180, 18, 7, K.LEAF, depth=2)


def erbgut_tree(tx, ty, ink, ink_faint):
    """Erbgut: Tuschefassung, Aquarell-Lasur, geflochtener Stängel mit Ranken, Keimling oben, Wurzeln unten ins Papier,
    je Stufe zwei Äste zu den Samen (die zeichnet das Spiel), Trennlinien mit Punkten"""
    K.text("Erbgut", tx, ty - 4, 34, ink, "AlmendraSC-Regular.ttf", "CENTER", depth=4)
    K.text("je Stufe eine Anlage", tx, ty + 38, 16, ink_faint, "Almendra-Regular.ttf", "CENTER", depth=4)
    ink_frame(tx - 168, ty - 26, tx + 168, ty + 690, ink_faint)
    wash = K.quad("Lasur", tx - 200, ty + 40, tx + 200, ty + 680, wash_mat("Lasur", (0.78, 0.9, 0.72)), depth=8)
    wash.visible_shadow = False
    wash2 = K.quad("Lasur2", tx - 90, ty + 300, tx + 150, ty + 560, wash_mat("Lasur2", (0.86, 0.92, 0.7)), depth=7)
    wash2.visible_shadow = False

    def stem_x(y):
        return tx + 7 * math.sin((y - ty - 82) / 20 * 0.45)

    stem = [(stem_x(ty + 92 + k * 10), ty + 92 + k * 10) for k in range(54)]
    C.braid(stem, 3.4, K.LIGHTVINE, 0)
    bottom = stem[-1]
    # Keimling oben: Samenschale mit zwei Keimblättern
    seed = K.sphere("Same", tx, ty + 92, 10, K.mat("Same", (0.32, 0.2, 0.1), 0.4), depth=-4)
    seed.scale = (1.0, 0.7, 0.8)
    K.leaf(tx, ty + 86, -160, 24, 11, K.LEAF, depth=-8, bend=0.3)
    K.leaf(tx, ty + 86, -20, 24, 11, K.LEAF, depth=-8, bend=-0.3)
    # Blätter und Ranken am Stängel zwischen den Stufen
    for k, y in enumerate((ty + 120, ty + 255, ty + 290, ty + 425, ty + 455, ty + 590)):
        side = 1 if k % 2 else -1
        K.leaf(stem_x(y), y, 0 if side > 0 else 180, 26, 11, K.LEAF, depth=-4, bend=0.2 * side)
        tendril(stem_x(y), y + 8, 14, 1.5, K.LIGHTVINE, -3, -side)
    # Wurzeln unten fächern ins Papier aus
    root_ink = K.mat("Wurzeltinte", (0.2, 0.13, 0.08), 0.8)
    for a in (-150, -120, -95, -70, -40):
        reach = 70 if a == -95 else 120
        end = (bottom[0] + math.cos(math.radians(-a)) * reach, bottom[1] + abs(math.sin(math.radians(-a))) * 36)
        pts = wander(bottom, end, 12, 3, rnd.uniform(-8, 8))
        D.gnarled_root(pts, 2.4, root_ink, 0)

    green = K.mat("Stammgruen", (0.14, 0.32, 0.12), 0.5, emit=0.15)
    for k, roman in enumerate(("I", "II", "III")):
        fy = ty + 170 + k * 168
        K.text(roman, tx - 150, fy - 58, 26, ink, "AlmendraSC-Regular.ttf", depth=4)
        for side in (-1, 1):
            nx, ny = tx + side * 92, fy + 6
            start = (stem_x(fy - 6), fy - 6)
            arc = []
            for j in range(13):
                t = j / 12
                bx = (1 - t) ** 2 * start[0] + 2 * (1 - t) * t * (tx + side * 30) + t * t * (nx - side * 20)
                by = (1 - t) ** 2 * start[1] + 2 * (1 - t) * t * (fy - 46) + t * t * (ny - 14)
                arc.append((bx, by))
            K.tube("Ast", arc, 3.0, K.mat("Astgruen", (0.1, 0.26, 0.07), 0.5, emit=0.12), depth=0, taper=True)
            C.braid(arc[:9], 2.4, K.LIGHTVINE, -2)
            K.leaf(arc[4][0], arc[4][1], -90 + side * 50, 18, 8, K.LEAF, depth=-4, bend=0.2)
            K.leaf(arc[8][0], arc[8][1], -90 - side * 20, 14, 6, K.LEAF, depth=-4, bend=-0.2)
            tendril(arc[6][0], arc[6][1] - 2, 10, 1.6, green, -3, side)
        # Trennlinie unter der Stufe: Tuschelinie mit drei Punkten
        if k < 2:
            sep = fy + 80
            K.tube("Trenner", wander((tx - 140, sep), (tx - 24, sep), 10, 1), 0.6, ink_faint, depth=4)
            K.tube("Trenner", wander((tx + 24, sep), (tx + 140, sep), 10, 1), 0.6, ink_faint, depth=4)
            for dx in (-12, 0, 12):
                K.disc("Punkt", tx + dx, sep, 1.8, ink_faint, depth=3)


# ---------------------------------------------------------------- Einzelteile

def sprite(path, w, h, builder, scale=2, samples=64, setup_lights=None):
    """Rendert ein freigestelltes Teil: w x h Designpixel, scale-fach aufgelöst, Glas lässt den Grund durch"""
    K.W, K.H = w, h
    D._leaf_mats.clear()
    K.setup()
    K.materials()
    lights()
    scene = bpy.context.scene
    scene.cycles.samples = samples
    if setup_lights is not None:
        setup_lights(scene)
    scene.render.resolution_x, scene.render.resolution_y = w * scale, h * scale
    scene.render.film_transparent = True
    scene.cycles.film_transparent_glass = True
    scene.camera.data.ortho_scale = max(w, h)
    builder()
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    K.W, K.H = 1920, 1080


def specimen_glass(gold):
    """Ein Sammelglas mit Korken und Schnur (wie in den Vorlagen Boris 2026-11-05), die Pflanze zeichnet das Spiel dahinter"""
    def build():
        x, y = 75, 105
        rim = K.GOLD if gold else K.BRONZE
        cork = K.noisy("Kork", (0.32, 0.2, 0.1), (0.55, 0.38, 0.2), 0.3, rough=0.9)
        twine = K.mat("Schnur", (0.5, 0.4, 0.26), 0.9)

        def cylinder(name, cy, radius, height, material, bevel=0.0):
            bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=radius, depth=height, location=K.P(x, cy, -4))
            obj = bpy.context.object
            obj.name = name
            if bevel > 0:
                mod = obj.modifiers.new("Rund", "BEVEL")
                mod.width = bevel
                mod.segments = 5
            bpy.ops.object.shade_smooth()
            obj.data.materials.append(material)
            return obj

        cylinder("Glas", y, 55, 132, K.CAPSULE_GLASS, bevel=16)
        cylinder("Hals", 35, 45, 10, K.CAPSULE_GLASS, bevel=3)
        cylinder("Lippe", 29, 48, 6, rim, bevel=2)
        cylinder("Korken", 17, 40, 22, cork, bevel=3)
        bpy.ops.mesh.primitive_torus_add(major_radius=46, minor_radius=2.2, location=K.P(x, 36, -4))
        bpy.context.object.data.materials.append(twine)
        K.tube("Schlaufe", [(x - 40, 37), (x - 52, 48), (x - 47, 58)], 1.6, twine, depth=-30)
        # Glanz auf dem Glas: ein heller Streif links, ein kurzer rechts
        for x0, x1, y0, y1 in ((x - 44, x - 40, 60, 140), (x + 39, x + 42, 66, 88)):
            T.no_lines(K.quad("Glanz", x0, y0, x1, y1, K.mat("Glanz", (1.0, 1.0, 0.95), 0.2, emit=1.2, alpha=0.35), depth=-70))
    return build


def capsule():
    K.sphere("Glas", 50, 50, 40, K.CAPSULE_GLASS, depth=-3, squash=0.4)
    K.ring("Rand", 50, 50, 40, 2.4, K.BRONZE, depth=-5)


def bud():
    px, py = 55, 55
    red_wax = K.mat("Wachs", (0.45, 0.05, 0.06), 0.35)
    K.disc("Knospe", px, py, 34, K.PAPER_DARK, depth=0)
    for a, s_ in ((-34, 0.9), (34, 0.9), (-14, 1.0), (14, 1.0), (0, 1.08)):
        K.leaf(px + a * 0.25, py + 30, -90 + a, 62 * s_, 30 * s_, K.LEAF_DARK, depth=-5 - abs(a) * 0.05, bend=-0.08 if a < 0 else 0.08)
    K.disc("Wachs", px, py + 22, 9, red_wax, depth=-9)


def pod(locked):
    def build():
        body = K.mat("Samenkapsel", (0.25, 0.22, 0.18) if locked else (0.22, 0.36, 0.16), 0.45)
        rib = K.mat("Rippe", (0.14, 0.12, 0.1) if locked else (0.1, 0.2, 0.07), 0.5)
        p = K.sphere("Knospe", 40, 42, 18, body, depth=-4)
        p.scale = (0.8, 0.6, 1.1)
        for dx in (-7, 0, 7):
            K.tube("Rippe", [(40 + dx * 0.4, 22), (40 + dx * 1.1, 34), (40 + dx * 1.15, 48), (40 + dx * 0.5, 61)], 0.8, rib, depth=-15)
        for a, l in ((-125, 16), (-55, 16), (-90, 13)):
            K.leaf(40, 60, -a, l, 8, K.LEAF_DARK if locked else K.LEAF, depth=-14, bend=0.15)
        K.tube("Spitze", [(40, 23), (41, 16), (44, 12)], 1.4, rib, depth=-12, taper=True)
    return build


def blossom():
    K.blossom(45, 45, 24, K.PETAL, K.GOLD, depth=-6)
    K.ring("Wahl", 45, 45, 34, 2.4, K.GOLD, depth=-8)


def vine_leaf():
    """Ein Blatt der beweglichen Ranken am Rahmen: Ansatz links in der Mitte, zeigt nach rechts (das Spiel dreht es)"""
    K.tube("Stiel", [(1, 16), (8, 16)], 1.2, K.mat("Stielgruen", (0.1, 0.2, 0.06), 0.6), depth=-4)
    K.leaf(7, 16, 0, 54, 24, K.LEAF, depth=-8, bend=0.12)


def sign(kind):
    def build():
        C.icon(kind, 20, 20, 1.3, K.mat("Weiss", (1.0, 1.0, 1.0), 0.5, emit=3.0), depth=-10)
    return build


def write_png(path, pixels, w, h):
    image = bpy.data.images.new("bild", w, h, alpha=True)
    image.pixels = pixels.reshape(-1)
    image.filepath_raw = path
    image.file_format = "PNG"
    image.save()


def glow_picture(path):
    import numpy as np
    n = 128
    yy, xx = np.mgrid[0:n, 0:n]
    r = np.hypot((xx + 0.5) / n - 0.5, (yy + 0.5) / n - 0.5) / 0.5
    pixels = np.ones((n, n, 4), np.float32)
    # vormultipliert: das Spiel legt den Schein additiv auf, dort zählt nur die Farbe
    pixels[..., 3] = np.clip(1.0 - r, 0.0, 1.0) ** 2.2
    pixels[..., :3] = pixels[..., 3:4]
    write_png(path, pixels, n, n)


def glass_pictures(folder):
    """Die Tuschebilder für die Sammelgläser: Rechteck 52 : 64 mit runden Ecken, Rand weich und dunkler"""
    import numpy as np
    for name in sorted(os.listdir(K.PLANTS)):
        if not name.startswith("T_Pflanze_") or not name.endswith(".png"):
            continue
        image = bpy.data.images.load(os.path.join(K.PLANTS, name))
        w, h = image.size
        pixels = np.array(image.pixels[:], np.float32).reshape(h, w, 4)
        cw = int(round(h * 52.0 / 64.0))
        x0 = max((w - cw) // 2, 0)
        pixels = pixels[:, x0:x0 + cw].copy()
        yy, xx = np.mgrid[0:h, 0:cw]
        corner = 0.3 * cw
        qx = np.maximum(np.abs(xx + 0.5 - cw / 2) - (cw / 2 - corner), 0.0)
        qy = np.maximum(np.abs(yy + 0.5 - h / 2) - (h / 2 - corner), 0.0)
        d = np.hypot(qx, qy) - corner
        alpha = np.clip(-d / (0.03 * cw), 0.0, 1.0)
        edge = np.clip(1.0 + d / (0.25 * cw), 0.0, 1.0)
        pixels[..., :3] *= (1.0 - 0.45 * edge)[..., None]
        pixels[..., 3] = alpha
        write_png(os.path.join(folder, name.replace("T_Pflanze_", "T_Keim_Glasbild_")), pixels, cw, h)


def oval_pictures(folder):
    """Die Tuschebilder oval freigestellt (Seitenverhältnis 52 : 64 wie die Schaugläser), Rand weich und dunkler"""
    import numpy as np
    for name in sorted(os.listdir(K.PLANTS)):
        if not name.startswith("T_Pflanze_") or not name.endswith(".png"):
            continue
        image = bpy.data.images.load(os.path.join(K.PLANTS, name))
        w, h = image.size
        pixels = np.array(image.pixels[:], np.float32).reshape(h, w, 4)
        cw = int(round(h * 52.0 / 64.0))
        x0 = max((w - cw) // 2, 0)
        pixels = pixels[:, x0:x0 + cw].copy()
        yy, xx = np.mgrid[0:h, 0:cw]
        r = np.hypot((xx + 0.5) / cw - 0.5, (yy + 0.5) / h - 0.5) / 0.5
        alpha = np.clip((0.98 - r) / 0.03, 0.0, 1.0)
        shade = np.clip(1.15 - 0.55 * r ** 3, 0.0, 1.0)
        pixels[..., :3] *= shade[..., None]
        pixels[..., 3] = alpha
        write_png(os.path.join(folder, name.replace("T_Pflanze_", "T_Keim_Oval_")), pixels, cw, h)


# ---------------------------------------------------------------- Realistisch (Boris 2026-11-05: "nicht kindlich, Cartoon,
# sondern wirklich realistisch, ernster Diablo-Stil, düster; Papier wie Papyrus aus einem Grimoire; Gläser mit mehr Licht und
# Schatten, das Bild darin schöner platziert; Ranken holzig wie Wurzeln und Äste, ein Korb aus Wurzeln, alles bewegt sich")

def _nodes(name):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    return m, nt, nt.nodes["Principled BSDF"]


def papyrus_mat(name):
    """Papyrus wie aus einem alten Grimoire: zwei Lagen gepresster Streifen (waagrecht oben, senkrecht darunter), lange
    Fasern, Prägung im Streiflicht, Stockflecken, Wasserränder, zum Rand hin gebräunt und angesengt"""
    m, nt, b = _nodes(name)
    n, l = nt.nodes, nt.links
    coord = n.new("ShaderNodeTexCoord")

    def mapped(scale, offset=(0.0, 0.0, 0.0)):
        mp = n.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = scale
        mp.inputs["Location"].default_value = offset
        l.new(coord.outputs["Object"], mp.inputs["Vector"])
        return mp.outputs[0]

    def noise(vec, scale, detail, rough=0.55, dim="3D"):
        t = n.new("ShaderNodeTexNoise")
        t.inputs["Scale"].default_value = scale
        t.inputs["Detail"].default_value = detail
        t.inputs["Roughness"].default_value = rough
        l.new(vec, t.inputs["Vector"])
        return t.outputs["Fac"]

    def math(op, a, bval):
        node = n.new("ShaderNodeMath")
        node.operation = op
        if isinstance(a, float):
            node.inputs[0].default_value = a
        else:
            l.new(a, node.inputs[0])
        if isinstance(bval, float):
            node.inputs[1].default_value = bval
        else:
            l.new(bval, node.inputs[1])
        return node.outputs[0]

    def ramp(fac, stops):
        r = n.new("ShaderNodeValToRGB")
        els = r.color_ramp.elements
        els[0].position, els[0].color = stops[0][0], stops[0][1]
        els[1].position, els[1].color = stops[-1][0], stops[-1][1]
        for pos, col in stops[1:-1]:
            e = els.new(pos)
            e.color = col
        l.new(fac, r.inputs["Fac"])
        return r.outputs["Color"]

    def mix(mode, fac, a, c):
        mx = n.new("ShaderNodeMix")
        mx.data_type = "RGBA"
        mx.blend_type = mode
        if isinstance(fac, float):
            mx.inputs["Factor"].default_value = fac
        else:
            l.new(fac, mx.inputs["Factor"])
        if isinstance(a, tuple):
            mx.inputs[6].default_value = a
        else:
            l.new(a, mx.inputs[6])
        if isinstance(c, tuple):
            mx.inputs[7].default_value = c
        else:
            l.new(c, mx.inputs[7])
        return mx.outputs[2]

    # Streifen: waagrechte Lage (Bänder in Z), darunter senkrechte (Bänder in X), je 26 bis 40 px breit
    strips_h = noise(mapped((0.0006, 0.0, 0.028)), 1.0, 2.0, 0.5)
    strips_v = noise(mapped((0.028, 0.0, 0.0006), (13.0, 0.0, 7.0)), 1.0, 2.0, 0.5)
    # Fasern: lange feine Striche entlang der Streifen
    fibre_h = noise(mapped((0.012, 0.0, 0.55)), 1.0, 12.0, 0.7)
    fibre_v = noise(mapped((0.55, 0.0, 0.012), (3.0, 0.0, 5.0)), 1.0, 12.0, 0.7)
    grain = noise(mapped((0.08, 0.08, 0.08)), 1.0, 14.0, 0.75)
    stains = noise(mapped((0.004, 0.004, 0.004), (9.0, 2.0, 4.0)), 1.0, 6.0, 0.6)

    base = ramp(strips_h, [(0.3, (0.44, 0.31, 0.16, 1)), (0.5, (0.62, 0.47, 0.27, 1)), (0.7, (0.72, 0.57, 0.35, 1))])
    under = ramp(strips_v, [(0.35, (0.82, 0.78, 0.74, 1)), (0.65, (1.0, 1.0, 1.0, 1))])
    color = mix("MULTIPLY", 0.55, base, under)
    fib = ramp(fibre_h, [(0.42, (0.78, 0.72, 0.64, 1)), (0.58, (1.0, 1.0, 1.0, 1)), (0.66, (1.08, 1.04, 0.96, 1))])
    color = mix("MULTIPLY", 0.8, color, fib)
    fibv = ramp(fibre_v, [(0.45, (0.86, 0.82, 0.76, 1)), (0.6, (1.0, 1.0, 1.0, 1))])
    color = mix("MULTIPLY", 0.45, color, fibv)
    # Stockflecken und Wasserränder
    stain_c = ramp(stains, [(0.52, (1.0, 1.0, 1.0, 1)), (0.6, (0.72, 0.58, 0.40, 1)), (0.62, (0.55, 0.40, 0.25, 1)), (0.66, (0.86, 0.78, 0.66, 1))])
    color = mix("MULTIPLY", 1.0, color, stain_c)
    fox = n.new("ShaderNodeTexVoronoi")
    fox.inputs["Scale"].default_value = 1.0
    l.new(mapped((0.05, 0.05, 0.05), (2.0, 0.0, 1.0)), fox.inputs["Vector"])
    fox_c = ramp(fox.outputs["Distance"], [(0.03, (0.45, 0.30, 0.16, 1)), (0.09, (1.0, 1.0, 1.0, 1))])
    gate = ramp(noise(mapped((0.01, 0.01, 0.01), (5.0, 0.0, 9.0)), 1.0, 2.0), [(0.55, (0, 0, 0, 1)), (0.62, (1, 1, 1, 1))])
    fox_mix = n.new("ShaderNodeMix")
    fox_mix.data_type = "RGBA"
    fox_mix.blend_type = "MULTIPLY"
    bw = n.new("ShaderNodeRGBToBW")
    l.new(gate, bw.inputs[0])
    l.new(bw.outputs[0], fox_mix.inputs["Factor"])
    l.new(color, fox_mix.inputs[6])
    l.new(fox_c, fox_mix.inputs[7])
    color = fox_mix.outputs[2]
    # Zum Rand gebräunt und angesengt: Abstand zum Blattrand aus den Objektkoordinaten (Blatt 990..1872 x 150..1000)
    sep = n.new("ShaderNodeSeparateXYZ")
    l.new(coord.outputs["Object"], sep.inputs[0])
    u = math("ABSOLUTE", math("DIVIDE", math("SUBTRACT", sep.outputs["X"], 471.0), 441.0), 0.0)
    v = math("ABSOLUTE", math("DIVIDE", math("ADD", sep.outputs["Z"], 35.0), 425.0), 0.0)
    edge = math("MAXIMUM", u, v)
    edge = math("ADD", edge, math("MULTIPLY", math("SUBTRACT", noise(mapped((0.02, 0.02, 0.02)), 1.0, 6.0), 0.5), 0.12))
    burn = ramp(edge, [(0.72, (1.0, 1.0, 1.0, 1)), (0.9, (0.62, 0.44, 0.26, 1)), (0.97, (0.25, 0.14, 0.07, 1)), (1.0, (0.06, 0.035, 0.02, 1))])
    color = mix("MULTIPLY", 1.0, color, burn)
    grain_c = ramp(grain, [(0.4, (0.9, 0.88, 0.85, 1)), (0.6, (1.0, 1.0, 1.0, 1))])
    color = mix("MULTIPLY", 0.6, color, grain_c)
    l.new(color, b.inputs["Base Color"])
    b.inputs["Roughness"].default_value = 0.88
    b.inputs["Sheen Weight"].default_value = 0.25
    # Prägung: Streifenkanten, Fasern und Korn, im Streiflicht sichtbar
    height = math("ADD", math("MULTIPLY", strips_h, 1.0), math("ADD", math("MULTIPLY", fibre_h, 0.6), math("MULTIPLY", grain, 0.35)))
    height = math("ADD", height, math("MULTIPLY", strips_v, 0.45))
    bump = n.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.45
    bump.inputs["Distance"].default_value = 3.0
    l.new(height, bump.inputs["Height"])
    l.new(bump.outputs[0], b.inputs["Normal"])
    return m


def raking_light():
    """Ein flaches Streiflicht über die Tafel von links oben: die Papyrusstreifen und Fasern werfen kleine Schatten"""
    data = bpy.data.lights.new("Streiflicht", "AREA")
    data.energy = 2.2e6
    data.size = 300
    data.color = (1.0, 0.82, 0.6)
    obj = bpy.data.objects.new("Streiflicht", data)
    obj.location = K.P(900, -150, -120)
    target = K.P(1430, 575, 14)
    obj.rotation_euler = (target - obj.location).to_track_quat("-Z", "Y").to_euler()
    bpy.context.collection.objects.link(obj)


# ---------------------------------------------------------------- Gläser mit der Pflanze darin

JAR_W, JAR_H = 150, 190


def jar(picture, empty=False):
    """Ein Sammelglas mit der Pflanze darin: ihr Bild als Karte im Glas, unten Erde und Moos davor, Glas mit Brechung,
    Korken mit Schnur; Streiflicht von links, Kantenlicht von hinten rechts, damit das Glas mit Licht und Schatten spielt"""
    def build():
        x, y = 75, 105
        cork = K.noisy("Kork", (0.20, 0.12, 0.06), (0.42, 0.28, 0.14), 0.25, detail=10.0, rough=0.9)
        twine = K.noisy("Schnur", (0.30, 0.22, 0.12), (0.48, 0.38, 0.24), 0.6, detail=6.0, rough=0.95, stripes=1.0)
        soil = K.noisy("Erde", (0.03, 0.02, 0.015), (0.10, 0.07, 0.045), 0.15, detail=10.0, rough=0.95)
        glass = bpy.data.materials.new("Glas")
        glass.use_nodes = True
        gb = glass.node_tree.nodes["Principled BSDF"]
        gb.inputs["Base Color"].default_value = (0.86, 0.95, 0.88, 1)
        gb.inputs["Roughness"].default_value = 0.04
        gb.inputs["Transmission Weight"].default_value = 1.0
        gb.inputs["IOR"].default_value = 1.47

        def cylinder(name, cy, radius, height, material, bevel=0.0, depth=-4.0):
            bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=radius, depth=height, location=K.P(x, cy, depth))
            obj = bpy.context.object
            obj.name = name
            if bevel > 0:
                mod = obj.modifiers.new("Rund", "BEVEL")
                mod.width = bevel
                mod.segments = 6
            bpy.ops.object.shade_smooth()
            obj.data.materials.append(material)
            return obj

        # Glas als Hohlkörper (Wand 3 px), damit es innen bricht und außen spiegelt
        outer = cylinder("Glas", y, 55, 132, glass, bevel=16)
        solid = outer.modifiers.new("Wand", "SOLIDIFY")
        solid.thickness = 3.0
        cylinder("Hals", 35, 45, 10, glass, bevel=3)
        cylinder("Lippe", 29, 48, 6, glass, bevel=2.5)
        cylinder("Korken", 17, 40, 22, cork, bevel=3)
        bpy.ops.mesh.primitive_torus_add(major_radius=46.5, minor_radius=2.0, location=K.P(x, 36, -4))
        bpy.context.object.data.materials.append(twine)
        bpy.ops.mesh.primitive_torus_add(major_radius=46.5, minor_radius=2.0, location=K.P(x, 40, -4))
        bpy.context.object.data.materials.append(twine)
        K.tube("Schlaufe", [(x - 42, 38), (x - 54, 50), (x - 50, 62), (x - 46, 68)], 1.6, twine, depth=-52)
        # Innen: die Pflanze als Karte, etwas hinter der Mitte, unten Erde mit Moos davor
        if not empty and picture:
            card = K.quad("Bild", x - 46, y - 58, x + 46, y + 56, K.cutout_mat(picture, emit=1.0), depth=12)
            card.visible_shadow = True
        heap = K.sphere("Erde", x, y + 50, 44, soil, depth=-2)
        heap.scale = (1.0, 0.9, 0.28)
        for _ in range(14 if not empty else 6):
            px, py = x + rnd.uniform(-36, 36), y + 42 + rnd.uniform(-3, 4)
            tuft = K.sphere("Moos", px, py, rnd.uniform(4, 8), K.MOSS, depth=rnd.uniform(-30, -6))
            tuft.scale = (1.0, 1.0, 0.6)
        if empty:
            # leeres Glas: nur ein vertrockneter Halm und Staub
            K.tube("Halm", [(x - 6, y + 50), (x - 2, y + 10), (x + 6, y - 20)], 1.4, K.mat("Trocken", (0.25, 0.2, 0.12), 0.9), depth=-6)

    return build


def jar_lights(scene):
    """Licht für die Gläser: Streiflicht von links oben vorn, kräftiges Kantenlicht von hinten rechts, ein schwaches warmes
    Füllicht; die Welt bleibt fast schwarz, damit das Glas dunkel und tief wirkt"""
    for obj in list(bpy.data.objects):
        if obj.type == "LIGHT":
            bpy.data.objects.remove(obj)

    def area(name, loc, energy, size, color):
        data = bpy.data.lights.new(name, "AREA")
        data.energy, data.size, data.color = energy, size, color
        obj = bpy.data.objects.new(name, data)
        obj.location = loc
        obj.rotation_euler = (K.P(75, 105, 0) - obj.location).to_track_quat("-Z", "Y").to_euler()
        scene.collection.objects.link(obj)
    area("Schlag", K.P(-120, -80, -260), 2.2e5, 120, (1.0, 0.88, 0.7))
    area("Kante", K.P(260, 60, 220), 3.5e5, 60, (0.75, 0.95, 0.85))
    area("Fuell", K.P(75, 120, -400), 0.25e5, 400, (1.0, 0.8, 0.6))
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.08


def render_jars(out):
    for name in sorted(os.listdir(out)):
        if not name.startswith("T_Keim_Glasbild_") or "_Schatten" in name:
            continue
        plant_id = name[len("T_Keim_Glasbild_"):-4]
        sprite(os.path.join(out, "T_Keim_Glas_{}.png".format(plant_id)), JAR_W, JAR_H, jar(os.path.join(out, name)), samples=96, setup_lights=jar_lights)
    sprite(os.path.join(out, "T_Keim_Glas_Leer.png"), JAR_W, JAR_H, jar(None, empty=True), samples=96, setup_lights=jar_lights)


# ---------------------------------------------------------------- Holz für die beweglichen Ranken

def root_strip(radius, moss, length=1024, height=64):
    """Ein langer, knorriger Wurzelstrang von links nach rechts, als Band für die Ranken im Spiel (das Spiel legt es entlang
    ihres Weges): Rinde mit Rissen, Knoten, Moos auf der Oberseite, Licht von oben"""
    def build():
        bark = D.bark_mat("Rinde")
        pts = []
        for k in range(240):
            t = k / 239
            pts.append((-20 + (length + 40) * t, height / 2 + 2.5 * math.sin(t * 31.0) + 1.5 * math.sin(t * 77.0)))
        curve = bpy.data.curves.new("Strang", "CURVE")
        curve.dimensions = "3D"
        curve.bevel_depth = radius
        curve.bevel_resolution = 6
        sp = curve.splines.new("POLY")
        sp.points.add(len(pts) - 1)
        for i, (px, py) in enumerate(pts):
            p = K.P(px, py, 0)
            sp.points[i].co = (p.x, p.y, p.z, 1)
            t = i / (len(pts) - 1)
            sp.points[i].radius = 1.0 + 0.12 * math.sin(t * 23.0) + 0.06 * math.sin(t * 61.0)
        obj = bpy.data.objects.new("Strang", curve)
        bpy.context.collection.objects.link(obj)
        obj.data.materials.append(bark)
        # ein zweiter, dünnerer Strang windet sich um den ersten: geflochten wie ein Korb
        twist = []
        for k in range(160):
            t = k / 159
            twist.append((-20 + (length + 40) * t, height / 2 + radius * 0.75 * math.sin(t * 2 * math.pi * 7)))
        K.tube("Windung", twist, radius * 0.32, bark, depth=-radius * 0.6)
        if moss:
            for _ in range(int(length / 26)):
                px = rnd.uniform(0, length)
                tuft = K.sphere("Moos", px, height / 2 - radius * 0.75, rnd.uniform(radius * 0.18, radius * 0.32), K.MOSS, depth=-radius * 0.5)
                tuft.scale = (rnd.uniform(1.2, 2.4), 0.8, 0.55)
    return build


def strip_lights(scene):
    for obj in list(bpy.data.objects):
        if obj.type == "LIGHT":
            bpy.data.objects.remove(obj)
    data = bpy.data.lights.new("Oben", "AREA")
    data.energy, data.size, data.color = 2.4e7, 2000, (1.0, 0.9, 0.75)
    obj = bpy.data.objects.new("Oben", data)
    obj.location = K.P(512, -900, -600)
    obj.rotation_euler = (K.P(512, 32, 0) - obj.location).to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(obj)
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.15


def real_leaf():
    """Ein dunkles, echtes Blatt für die Ranken: Ansatz links in der Mitte"""
    K.tube("Stiel", [(1, 16), (9, 16)], 1.3, K.mat("Stiel", (0.06, 0.09, 0.03), 0.6), depth=-4)
    K.leaf(8, 16, 0, 54, 24, K.LEAF_DARK, depth=-8, bend=0.14)


def strip_scale(w, h):
    """Bänder sind breiter als hoch: die Kamera nimmt die Breite"""
    return max(w, h)


def main():
    out = sys.argv[sys.argv.index("--") + 1]
    os.makedirs(out, exist_ok=True)
    only = sys.argv[sys.argv.index("--") + 2:]
    patch()
    if not only or "bilder" in only:
        glow_picture(os.path.join(out, "T_Keim_Schein.png"))
        K.round_pictures(out)
        oval_pictures(out)
        glass_pictures(out)
    if not only or "grund" in only:
        C.rnd.seed(2026)
        K.setup()
        K.materials()
        lights()
        scene = bpy.context.scene
        scene.view_settings.exposure = -1.05
        scene.cycles.samples = 128
        scene.cycles.volume_step_rate = 4.0
        room_lights()
        raking_light()
        build_background()
        god_rays()
        scene.render.filepath = os.path.join(out, "T_Keim_Grund.png")
        bpy.ops.render.render(write_still=True)
        T.vignette_png(os.path.join(out, "T_Keim_Grund.png"))
    if not only or "glaeser" in only:
        render_jars(out)
    if not only or "holz" in only:
        sprite(os.path.join(out, "T_Keim_Ast.png"), 1024, 64, root_strip(20, True), samples=64, setup_lights=strip_lights)
        sprite(os.path.join(out, "T_Keim_Wurzel.png"), 1024, 32, root_strip(8, False), samples=64, setup_lights=strip_lights)
        sprite(os.path.join(out, "T_Keim_Rankenblatt.png"), 64, 32, real_leaf, scale=3)
    if not only or "teile" in only:
        sprite(os.path.join(out, "T_Keim_Kapsel.png"), 100, 100, capsule)
        sprite(os.path.join(out, "T_Keim_Knospe.png"), 110, 110, bud)
        sprite(os.path.join(out, "T_Keim_Same.png"), 80, 80, pod(False))
        sprite(os.path.join(out, "T_Keim_Same_Zu.png"), 80, 80, pod(True))
        sprite(os.path.join(out, "T_Keim_Bluete.png"), 90, 90, blossom)
        for kind in ("dorn", "wirbel", "tropfen", "nid"):
            sprite(os.path.join(out, "T_Keim_Zeichen_{}.png".format(kind.capitalize())), 40, 40, sign(kind))
    print("[Vael] Keimgeflecht-Bausteine in", out)


if __name__ == "__main__":
    main()
