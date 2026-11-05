# Entwurf C des Keimgeflechts (Boris 2026-11-04): "noch zu sehr Diablo 1/2, soll in 2026 ankommen, man soll gern Zeit darin
# verbringen, jedes Fenster mit eigenem Charakter". Leitidee: keine Bretter und Kästen mehr, sondern eine Kammer mit Tiefe
# (Vordergrund-Blätter, Lichtbahnen, weiche Lichter im Dunkel), ein Licht als Mittelpunkt (der Strang), organische Formen.
#
#   Herbarium   Pflanzen in ovalen Schaugläsern, an Fäden aufgehängt, versetzt wie an einer Wurzelwand; die gewählte schwingt vor
#   Erbstrang   der Strang als leuchtendes Herz der Kammer, Saft pulst hinauf, Kapseln aus Glas, verschlossene Knospen
#   Tafel       ein freies Pergamentblatt (keine Kiste), die Pflanze als Miniatur mit Tusche-Anmerkungen, Werte als Zeichen
#   Erbgut      ein Stammbaum aus Ranken: je Stufe gabelt sich der Stängel zu zwei Samen; der gewählte ist aufgeblüht
#   Kreuzungen  Doppelmedaillons, unbekannte als Schattenriss
#
# blender.exe -b --factory-startup -P Tools/HUD/vael_keimgeflecht_c.py -- <Ausgabe.png>

import math
import os
import random
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vael_keimgeflecht_entwurf as K  # noqa: E402

rnd = random.Random(2026)


# ---------------------------------------------------------------- Bausteine

def oval_ring(name, x, y, rx, ry, thick, material, depth=-4.0):
    obj = K.ring(name, x, y, 50.0, thick * 50.0 / min(rx, ry), material, depth)
    obj.scale = (rx / 50.0, ry / 50.0, 1.0)
    return obj


def oval_disc(name, x, y, rx, ry, material, depth=0.0):
    obj = K.disc(name, x, y, 50.0, material, depth)
    obj.scale = (rx / 50.0, ry / 50.0, 1.0)
    return obj


def vignette_mat():
    """Schwarz zu den Rändern, durchsichtig in der Mitte"""
    m = bpy.data.materials.new("Vignette")
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
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.0
    ramp.color_ramp.elements[0].color = (1, 1, 1, 1)
    ramp.color_ramp.elements[1].position = 0.55
    ramp.color_ramp.elements[1].color = (0, 0, 0, 1)
    nt.links.new(grad.outputs["Fac"], ramp.inputs["Fac"])
    black = nt.nodes.new("ShaderNodeEmission")
    black.inputs["Color"].default_value = (0, 0, 0, 1)
    transparent = nt.nodes.new("ShaderNodeBsdfTransparent")
    mix = nt.nodes.new("ShaderNodeMixShader")
    nt.links.new(ramp.outputs["Color"], mix.inputs["Fac"])
    nt.links.new(transparent.outputs[0], mix.inputs[1])
    nt.links.new(black.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    return m


def sheet(name, x0, y0, x1, y1, material, depth=10.0, ragged=5.0, curl=18.0):
    """Ein freies Blatt Pergament: unregelmäßiger Rand, eine Ecke rollt sich leicht nach vorn"""
    nx, ny = 40, 36
    verts, faces = [], []
    for j in range(ny + 1):
        for i in range(nx + 1):
            u, v = i / nx, j / ny
            x, y = x0 + (x1 - x0) * u, y0 + (y1 - y0) * v
            edge = i in (0, nx) or j in (0, ny)
            if edge:
                n = math.sin(u * 37.0 + v * 23.0) * 0.6 + math.sin(u * 91.0 + v * 57.0) * 0.4
                x += (-1 if i == 0 else 1 if i == nx else 0) * n * ragged
                y += (-1 if j == 0 else 1 if j == ny else 0) * n * ragged
            # untere rechte Ecke rollt sich zum Betrachter
            corner = max(0.0, (u - 0.82) / 0.18) * max(0.0, (v - 0.8) / 0.2)
            verts.append(K.P(x, y, depth - curl * corner ** 1.6 - 3.0 * math.sin(u * math.pi)))
    for j in range(ny):
        for i in range(nx):
            a = j * (nx + 1) + i
            faces.append((a, a + 1, a + nx + 2, a + nx + 1))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    for poly in mesh.polygons:
        poly.use_smooth = True
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.data.materials.append(material)
    return obj


def icon(kind, x, y, s, material, depth=-14.0):
    """Kleine Zeichen der Werte und Erbanlagen: Dorn (Kraft), Wirbel (Tempo), Tropfen (Saft), Nid-Tropfen"""
    if kind == "dorn":
        K.tube("Dorn", [(x - 8 * s, y + 8 * s), (x + 2 * s, y - 2 * s), (x + 9 * s, y - 10 * s)], 4.0 * s, material, depth, taper=True)
        K.tube("Dorn2", [(x - 4 * s, y + 9 * s), (x - 9 * s, y + 2 * s)], 2.5 * s, material, depth, taper=True)
    elif kind == "wirbel":
        pts = [(x + math.cos(a) * (2 + a * 1.4) * s, y + math.sin(a) * (2 + a * 1.4) * s) for a in [k * 0.35 for k in range(18)]]
        K.tube("Wirbel", pts, 1.6 * s, material, depth)
    elif kind in ("tropfen", "nid"):
        K.sphere("Tropfen", x, y + 3 * s, 6 * s, material, depth)
        K.tube("Spitze", [(x, y + 1 * s), (x, y - 10 * s)], 5.6 * s, material, depth, taper=True)
    elif kind == "bluete":
        K.blossom(x, y, 9 * s, material, K.GOLD, depth)


def braid(points, radius, material, depth):
    """Eine Ranke aus drei dünneren Fasern, die sich umeinander drehen, statt einer glatten Röhre"""
    if len(points) < 2:
        return
    for strand in range(3):
        twisted = []
        for k, (x, y) in enumerate(points):
            nx = points[min(k + 1, len(points) - 1)][0] - points[max(k - 1, 0)][0]
            ny = points[min(k + 1, len(points) - 1)][1] - points[max(k - 1, 0)][1]
            ln = max(math.hypot(nx, ny), 1e-3)
            angle = y * 0.09 + strand * 2.094
            off = math.cos(angle) * radius * 0.55
            twisted.append((x - ny / ln * off, y + nx / ln * off))
        K.tube("Faser", twisted, radius * 0.55, material, depth=depth + math.sin(strand * 2.094) * 2)


def specimen(x, y, name, state, ink, scale=1.0):
    """Ein ovales Schauglas mit der Pflanze, an einem Faden; state: normal, gewaehlt, imrad, unbekannt"""
    rx, ry = 52 * scale, 64 * scale
    K.tube("Faden", [(x + rnd.uniform(-10, 10), y - ry - 70), (x, y - ry - 6)], 1.0, K.THORN, depth=8)
    if state == "gewaehlt":
        K.glow(x, y, 190 * scale, K.glow_mat("Wahlschein", (1.0, 0.8, 0.45), 0.55), depth=16)
    oval_ring("Fassung", x, y, rx + 6, ry + 6, 5.0, K.GOLD if state == "gewaehlt" else K.BRONZE, depth=-4)
    oval_ring("Innen", x, y, rx, ry, 1.4, K.GOLD, depth=-5)
    # Knauf oben, an dem der Faden hängt
    K.sphere("Knauf", x, y - ry - 8, 6, K.GOLD if state == "gewaehlt" else K.BRONZE, depth=-6)
    if state == "unbekannt":
        oval_disc("Leer", x, y, rx, ry, K.mat("LeerGlas", (0.03, 0.04, 0.035), 0.3, emit=0.0), depth=2)
        K.text("?", x, y - 34, 64, K.mat("Frage", (0.35, 0.38, 0.32), 0.5, emit=0.3), "AlmendraSC-Regular.ttf", "CENTER", depth=-8)
    else:
        oval_disc("Bild", x, y, rx, ry, K.picture_mat(K.plant(name), emit=0.55), depth=2)
    glass = K.sphere("Glas", x, y, 60, K.CAPSULE_GLASS, depth=-2, squash=0.25)
    glass.scale = (rx / 60.0, 0.25, ry / 60.0)
    label = {"Russrose": "Rußrose"}.get(name, name) if name else "unentdeckt"
    K.text(label, x, y + ry + 12, 17 if state == "gewaehlt" else 15, ink, "Almendra-Regular.ttf", "CENTER", depth=-8)
    if state == "imrad" or state == "gewaehlt":
        K.leaf(x + rx - 6, y - ry + 6, -50, 26, 12, K.LEAF, depth=-12)
        K.leaf(x + rx - 6, y - ry + 6, -10, 20, 10, K.LEAF, depth=-12)


# ---------------------------------------------------------------- Bild

def build():
    ink = K.mat("Tinte", (0.09, 0.055, 0.03), 0.9)
    ink_faint = K.mat("TinteBlass", (0.16, 0.11, 0.07), 0.9)
    pale = K.mat("Hell", (0.88, 0.82, 0.68), 0.6, emit=1.0)
    dim = K.mat("Dunkeltext", (0.5, 0.46, 0.4), 0.6, emit=0.6)
    sap = K.mat("Saftlicht", (0.5, 1.0, 0.6), 0.4, emit=3.0)
    gold_text = K.mat("Titelgold", (0.95, 0.74, 0.36), 0.3, 0.6, emit=1.3)
    violet = K.mat("Nidtropfen", (0.6, 0.3, 0.9), 0.3, emit=1.4)
    red_wax = K.mat("Wachs", (0.45, 0.05, 0.06), 0.35)

    # ---- Tiefe: dunkler Grund, weiche Lichter weit hinten, Lichtbahnen von oben, Wurzeln und Moos, Blätter im Vordergrund
    K.quad("Grund", 0, 0, K.W, K.H, K.WOOD, depth=60)
    for k in range(26):
        x, y = rnd.uniform(0, K.W), rnd.uniform(0, K.H)
        color = rnd.choice(((0.3, 0.9, 0.6), (0.9, 0.6, 0.8), (0.95, 0.8, 0.5)))
        K.glow(x, y, rnd.uniform(40, 130), K.glow_mat("Bokeh", color, rnd.uniform(0.08, 0.22)), depth=50)
    for k, (x, w, s) in enumerate(((700, 180, 0.22), (860, 120, 0.3), (1000, 220, 0.12))):
        shaft = K.quad("Lichtbahn", x - w / 2, -200, x + w / 2, 1300, K.glow_mat("Bahn", (0.75, 1.0, 0.75), s), depth=40)
        shaft.rotation_euler = (0, math.radians(-8 + k * 5), 0)
        shaft.visible_shadow = False
    K.alive_room()
    vig = K.quad("Vignette", -200, -150, K.W + 200, K.H + 150, vignette_mat(), depth=-70)
    vig.visible_shadow = False
    # dunkle, große Blätter am Rand, ganz vorn: der Blick fällt durch Laub in die Kammer
    for x, y, a, l in ((-30, 90, 25, 200), (-20, 1060, -35, 220), (1950, 40, 160, 190)):
        K.leaf(x, y, a, l, l * 0.42, K.mat("Vorderblatt", (0.008, 0.02, 0.008), 0.8), depth=-60, bend=0.2)

    # ---- Titel: kleiner, mit einer Ranke statt eines Rahmens
    K.text("Keimgeflecht", K.W / 2, 44, 52, gold_text, "AlmendraSC-Regular.ttf", "CENTER", depth=-20)
    curve = [(K.W / 2 - 220 + k * 20, 112 + 5 * math.sin(k * 0.9)) for k in range(23)]
    K.tube("Titelranke", curve, 1.6, K.GOLD, depth=-18)
    K.leaf(K.W / 2 - 230, 112, 200, 24, 10, K.LEAF, depth=-18)
    K.leaf(K.W / 2 + 230, 112, -20, 24, 10, K.LEAF, depth=-18)

    # ---- Herbarium: Schaugläser an Fäden, versetzt
    K.text("Herbarium", 330, 150, 34, pale, "AlmendraSC-Regular.ttf", "CENTER", depth=-20)
    K.text("11 von 12 gefunden", 330, 192, 18, dim, "Almendra-Regular.ttf", "CENTER", depth=-20)
    plants = ["Windhalm", "Russrose", "Glutdistel", "Glimmklee", "Taublatt", "Schlotwinde", "Funkenmohn", "Harzkolben",
              "Feenragwurz", "Wachbeere", "Mondtraufe", None]
    for i, name in enumerate(plants):
        col, row = i % 3, i // 3
        x = 170 + col * 160
        y = 300 + row * 178 + (40 if col == 1 else 0)
        state = "unbekannt" if name is None else "gewaehlt" if name == "Russrose" else "imrad" if name in ("Windhalm", "Glutdistel", "Glimmklee") else "normal"
        specimen(x, y, name, state, pale if state != "unbekannt" else dim, 1.08 if state == "gewaehlt" else 1.0)

    # ---- Erbstrang: das Herz der Kammer
    cx, top, bottom, amp = 790, 214, 972, 46
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
                braid(piece, radius * (1.0 + 0.15 * math.sin(t * 23.0)), material, -10 if front else 8)
                piece, front = [(x, y)], now
            if k % 8 == 3 and now:
                side = 1 if math.sin(phase) > 0 else -1
                if leaves:
                    K.leaf(x, y, (0 if side > 0 else 180) + side * (-30 + 25 * math.sin(k)), 30, 14, K.LEAF, depth=-14)
                    if k % 24 == 3:
                        K.blossom(x - side * 6, y + 8, 10, K.PETAL, K.GOLD)
                if thorns:
                    K.tube("Dorn", [(x, y), (x + side * 13, y + 3)], 2.8, material, depth=-12, taper=True)
            # Saftperlen, die im Lichtstrang aufsteigen
            if leaves and k % 13 == 6:
                K.sphere("Saft", x, y, 4.0, sap, depth=-16)
                K.glow(x, y, 26, K.glow_mat("Saftschein", (0.5, 1.0, 0.6), 1.2), depth=-15)

    strand(0.0, 9.0, K.LIGHTVINE, True, False)
    strand(math.pi, 8.0, K.SHADOWVINE, False, True)
    for k in range(0, steps, 7):
        t = k / steps
        y = top + (bottom - top) * t
        phase = t * 4.4 * math.pi
        a, b = cx + amp * math.sin(phase), cx + amp * math.sin(phase + math.pi)
        if abs(a - b) > 26:
            K.tube("Sprosse", [(a, y), ((a + b) / 2, y + 5), (b, y)], 1.6, K.mat("Wurzelhaar", (0.5, 0.9, 0.6), 0.5, emit=0.6), depth=0)
    K.moth_emblem(cx, top - 30, 1.15)
    wheel = ["Windhalm", "Russrose", "Glutdistel", "Glimmklee"]
    for i in range(8):
        y = top + 50 + i * 92
        side = -1 if i % 2 == 0 else 1
        px = cx + side * 106
        K.tube("Stiel", [(cx + side * 30, y + 12), (cx + side * 58, y + 2), (px - side * 42, y)], 3.2, K.LIGHTVINE, depth=2)
        for a in range(6):
            K.leaf(px, y, a * 60 + 90, 52, 24, K.LEAF if i < 4 else K.LEAF_DARK, depth=6, bend=0.12)
        if i < 4:
            K.disc("Kapsel", px, y, 38, K.picture_mat(K.plant(wheel[i]), emit=0.6), depth=0)
            K.sphere("Glas", px, y, 40, K.CAPSULE_GLASS, depth=-3, squash=0.4)
            K.ring("Rand", px, y, 40, 2.4, K.GOLD if i == 1 else K.BRONZE, depth=-5)
            K.glow(px, y, 84, K.VINE_GLOW, depth=14)
        else:
            K.disc("Knospe", px, y, 34, K.PAPER_DARK, depth=0)
            # geschlossene Knospe: fünf Kelchblätter legen sich übereinander nach oben zusammen
            for a, s_ in ((-34, 0.9), (34, 0.9), (-14, 1.0), (14, 1.0), (0, 1.08)):
                K.leaf(px + a * 0.25, y + 30, -90 + a, 62 * s_, 30 * s_, K.BRONZE if i % 2 else K.LEAF_DARK, depth=-5 - abs(a) * 0.05, bend=-0.08 if a < 0 else 0.08)
            K.disc("Wachs", px, y + 22, 9, red_wax, depth=-9)
        K.text(str(i + 1), px + side * 58, y - 13, 20, pale if i < 4 else dim, "AlegreyaSans-Bold.ttf", "CENTER", depth=-14)

    # ---- Die Tafel: ein freies Pergamentblatt, leicht schräg, mit eingerollter Ecke
    x0, y0, x1, y1 = 990, 150, 1872, 1000
    page = sheet("Pergament", x0, y0, x1, y1, K.PAPER, depth=14)
    page.rotation_euler = (0, math.radians(-0.8), 0)
    K.text("Tafel V", x0 + 56, y0 + 40, 20, ink_faint, "IMFeENsc28P.ttf", depth=4)
    K.text("Rußrose", x0 + 54, y0 + 62, 66, ink, "AlmendraSC-Regular.ttf", depth=4)
    K.text("Rosa fuliginosa", x0 + 58, y0 + 140, 22, ink_faint, "Almendra-Regular.ttf", depth=4)
    # Siegel der Floromantin neben dem Namen
    K.disc("Siegel", x0 + 420, y0 + 104, 26, red_wax, depth=2)
    K.ring("Siegelrand", x0 + 420, y0 + 104, 26, 3.5, red_wax, depth=0)
    K.leaf(x0 + 420, y0 + 104, -60, 18, 8, K.mat("Praegung", (0.3, 0.03, 0.04), 0.4), depth=-2)
    K.leaf(x0 + 420, y0 + 104, -120, 18, 8, K.mat("Praegung2", (0.3, 0.03, 0.04), 0.4), depth=-2)

    # Miniatur der Pflanze: ein ovales Bild mit Goldhaarlinie, die Blätter wachsen über den Rand hinaus
    mx, my, mrx, mry = x0 + 210, y0 + 375, 150, 185
    K.glow(mx, my, 300, K.glow_mat("Miniaturschein", (0.95, 0.85, 0.6), 0.25), depth=8)
    oval_disc("Miniatur", mx, my, mrx, mry, K.picture_mat(K.plant("Russrose"), emit=0.55), depth=2)
    oval_ring("Haarlinie", mx, my, mrx + 6, mry + 6, 1.4, K.GOLD, depth=0)
    oval_ring("Haarlinie2", mx, my, mrx + 12, mry + 12, 0.6, ink, depth=0)
    K.leaf(mx + mrx - 10, my - mry + 60, -30, 70, 30, K.LEAF, depth=-6)
    K.leaf(mx - mrx + 20, my + mry - 30, 200, 60, 26, K.LEAF, depth=-6)
    figs = [((mx + 10, my - 120), "Fig. 1  Herzblüte", "schießt ihr Herz"), ((mx + 70, my + 10), "Fig. 2  Dornenranke", "Peitsche am Arm"),
            ((mx - 10, my + 170), "Fig. 3  Rußwurzel", "trinkt Asche")]
    for k, ((fx, fy), title, note) in enumerate(figs):
        lx, ly = mx + mrx + 40, my - 140 + k * 116
        K.tube("Zeiger", [(fx, fy), ((fx + lx) / 2, (fy + ly) / 2 + 12), (lx - 8, ly + 12)], 0.9, ink, depth=-2)
        K.disc("Punkt", fx, fy, 3.5, ink, depth=-3)
        K.text(title, lx, ly, 22, ink, "Almendra-Bold.ttf", depth=4)
        K.text(note, lx, ly + 28, 17, ink_faint, "Almendra-Regular.ttf", depth=4)

    # Werte als Zeichen in einer Reihe, mit feinen Tusche-Balken
    stats = [("dorn", "Kraft", "100 %", 0.5), ("wirbel", "Tempo", "8,3 / s", 0.8), ("tropfen", "Saft", "0,64", 0.35), ("nid", "Nid", "0,12 / s", 0.2)]
    for k, (kind, label, value, share) in enumerate(stats):
        sx, sy = x0 + 64 + k * 112, y0 + 594
        icon(kind, sx + 12, sy + 12, 1.3, violet if kind == "nid" else K.mat("Zeichen", (0.12, 0.3, 0.1), 0.5, emit=0.2))
        K.text(label, sx + 34, sy - 2, 17, ink_faint, "Almendra-Regular.ttf", depth=4)
        K.text(value, sx + 34, sy + 18, 19, ink, "Almendra-Bold.ttf", depth=4)
        K.tube("Balken", [(sx, sy + 48), (sx + 92, sy + 48)], 1.0, ink_faint, depth=2)
        K.tube("BalkenVoll", [(sx, sy + 48), (sx + 92 * share, sy + 48)], 2.2, K.mat("Saftbalken", (0.25, 0.6, 0.25), 0.5, emit=0.4), depth=1)

    # ---- Erbgut: ein Stammbaum aus Ranken auf der rechten Seite des Blattes
    tx, ty = x0 + 690, y0 + 50
    K.text("Erbgut", tx, ty - 4, 34, ink, "AlmendraSC-Regular.ttf", "CENTER", depth=4)
    K.text("je Stufe eine Anlage", tx, ty + 38, 16, ink_faint, "Almendra-Regular.ttf", "CENTER", depth=4)
    green = K.mat("Stammgruen", (0.14, 0.32, 0.12), 0.5, emit=0.15)
    # Ein Stängel, der aus einem Samen oben herabwächst, leicht geschwungen, mit kleinen Blättern
    stem = [(tx + 7 * math.sin(k * 0.45), ty + 82 + k * 20) for k in range(29)]
    K.tube("Stamm", stem, 3.6, green, depth=0, taper=False)
    for k in range(3, 28, 5):
        sx, sy = stem[k]
        K.leaf(sx, sy, 180 if k % 2 else 0, 16, 7, K.LEAF, depth=-4)
    K.sphere("Same", tx, ty + 78, 10, K.mat("Same", (0.3, 0.2, 0.1), 0.4), depth=-4)
    tiers = [("I", ("tropfen", "Sparsamer Saft", True), ("dorn", "Kräftiger Wuchs", False), True),
             ("II", ("wirbel", "Schneller Trieb", False), ("dorn", "Tiefe Wurzel", False), False),
             ("III", ("dorn", "Volle Blüte", False), ("nid", "Nidscheu", False), False)]
    for k, (roman, left, right, open_) in enumerate(tiers):
        fy = ty + 170 + k * 168
        K.text(roman, tx, fy - 70, 22, ink if open_ else ink_faint, "AlmendraSC-Regular.ttf", "CENTER", depth=4)
        if not open_:
            K.text("ab Stufe {}".format(6 if k == 1 else 12), tx, fy - 46, 15, ink_faint, "Almendra-Regular.ttf", "CENTER", depth=4)
        for side, (kind, label, taken) in ((-1, left), (1, right)):
            nx, ny = tx + side * 92, fy + 6
            # geschwungener Ast: vom Stängel erst hinauf, dann im Bogen zur Knospe
            start = (tx + 7 * math.sin((fy - ty - 82) / 20 * 0.45), fy - 6)
            arc = []
            for j in range(13):
                t = j / 12
                bx = (1 - t) ** 2 * start[0] + 2 * (1 - t) * t * (tx + side * 30) + t * t * (nx - side * 20)
                by = (1 - t) ** 2 * start[1] + 2 * (1 - t) * t * (fy - 46) + t * t * (ny - 14)
                arc.append((bx, by))
            K.tube("Ast", arc, 2.8, green if open_ else ink_faint, depth=0, taper=True)
            K.leaf(arc[5][0], arc[5][1], -90 + side * 40, 14, 6, K.LEAF if open_ else K.LEAF_DARK, depth=-4)
            if taken:
                K.glow(nx, ny, 70, K.glow_mat("Blueteschein", (0.6, 1.0, 0.6), 1.0), depth=6)
                K.blossom(nx, ny, 24, K.PETAL, K.GOLD, depth=-6)
                icon(kind, nx, ny, 1.0, K.mat("ZeichenHell", (0.95, 0.95, 0.85), 0.4, emit=1.0), depth=-12)
                K.ring("Wahl", nx, ny, 34, 2.4, K.GOLD, depth=-8)
            else:
                pod = K.sphere("Knospe", nx, ny, 18, K.mat("Samenkapsel", (0.25, 0.35, 0.18) if open_ else (0.25, 0.22, 0.18), 0.5), depth=-4)
                pod.scale = (0.8, 0.6, 1.1)
                icon(kind, nx, ny, 0.8, ink if open_ else ink_faint, depth=-10)
            K.text(label, nx, ny + 40, 17, ink if open_ else ink_faint, "Almendra-Regular.ttf", "CENTER", depth=4)

    # Die gewählte Anlage, in Worten und was sie ändert
    K.text("Sparsamer Saft", tx, y0 + 742, 22, ink, "Almendra-Bold.ttf", "CENTER", depth=4)
    K.text("Saft je Schuss  0,80  >  0,64", tx, y0 + 772, 18, K.mat("Gruentext", (0.05, 0.22, 0.05), 0.6), "Almendra-Regular.ttf", "CENTER", depth=4)

    # ---- Kreuzungen: Doppelmedaillons
    ky = y0 + 676
    K.text("Kreuzungen", x0 + 56, ky, 28, ink, "AlmendraSC-Regular.ttf", depth=4)
    K.text("2 von 10 gewachsen", x0 + 270, ky + 6, 16, ink_faint, "Almendra-Regular.ttf", depth=4)
    pairs = [("Glutdistel", "Glutrose", True), ("Glimmklee", "Laube", True), (None, "?", False), (None, "?", False), (None, "?", False)]
    for k, (partner, label, known) in enumerate(pairs):
        px, py = x0 + 96 + k * 112, ky + 84
        for dx, name in ((-14, "Russrose"), (14, partner)):
            if known:
                K.disc("Paarbild", px + dx, py, 24, K.picture_mat(K.plant(name), emit=0.5), depth=-2 if dx < 0 else 0)
            else:
                K.disc("Paarbild", px + dx, py, 24, K.mat("Schattenriss", (0.12, 0.1, 0.08), 0.6), depth=-2 if dx < 0 else 0)
            K.ring("Paarrand", px + dx, py, 24, 1.6, K.GOLD if known else ink_faint, depth=-4 if dx < 0 else -2)
        K.text(label, px, py + 32, 16, ink if known else ink_faint, "Almendra-Regular.ttf", "CENTER", depth=4)

    # ---- Knöpfe unten auf einem dunklen Verlauf, ohne Kasten
    band = K.quad("Fussband", 300, 1000, 1620, 1080, K.glow_mat("Fussband", (0.0, 0.0, 0.0), 0.0), depth=-30)
    band.visible_shadow = False
    hints = [("Kreuz", "Erbanlage aufnehmen"), ("Kreis", "Schließen"), ("Viereck", "Tafel wenden"), ("L1R1", "Seiten")]
    x = 560
    for button, label in hints:
        K.ring("Knopf", x, 1040, 13, 1.8, K.GOLD, depth=-34)
        if button == "Kreuz":
            K.tube("Z", [(x - 5, 1035), (x + 5, 1045)], 1.3, pale, depth=-35)
            K.tube("Z", [(x - 5, 1045), (x + 5, 1035)], 1.3, pale, depth=-35)
        elif button == "Kreis":
            K.ring("Z", x, 1040, 5, 1.2, pale, depth=-35)
        elif button == "Viereck":
            K.tube("Z", [(x - 5, 1035), (x + 5, 1035), (x + 5, 1045), (x - 5, 1045), (x - 5, 1035)], 1.2, pale, depth=-35)
        else:
            K.text("L1 R1", x, 1031, 13, pale, "AlegreyaSans-Bold.ttf", "CENTER", depth=-35)
        K.text(label, x + 24, 1027, 21, pale, "Almendra-Regular.ttf", depth=-35)
        x += 60 + len(label) * 11


def main():
    out = sys.argv[sys.argv.index("--") + 1]
    K.setup()
    K.materials()
    scene = bpy.context.scene
    scene.cycles.samples = 64
    scene.view_settings.exposure = -0.75
    # weicheres, stimmungsvolleres Licht: Hauptlicht schwächer, das Licht kommt vom Strang
    bpy.data.objects["Key"].data.energy *= 0.65
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
    build()
    scene.render.filepath = out
    bpy.ops.render.render(write_still=True)
    print("[Vael] gerendert:", out)


if __name__ == "__main__":
    main()
