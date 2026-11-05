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
    K.aged_paper = D.parchment_mat


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
    ink = K.mat("Tinte", (0.09, 0.055, 0.03), 0.9)
    ink_faint = K.mat("TinteBlass", (0.16, 0.11, 0.07), 0.9)
    pale = K.mat("Hell", (0.88, 0.82, 0.68), 0.6, emit=1.0)
    dim = K.mat("Dunkeltext", (0.5, 0.46, 0.4), 0.6, emit=0.6)
    gold_text = K.mat("Titelgold", (0.95, 0.74, 0.36), 0.3, 0.6, emit=1.3)
    violet = K.mat("Nidtropfen", (0.6, 0.3, 0.9), 0.3, emit=1.4)

    # Tiefe wie in C/D: Grund, weiche Lichter, Lichtbahnen, Wurzelkammer, Vignette, dunkles Laub ganz vorn
    K.quad("Grund", 0, 0, K.W, K.H, K.WOOD, depth=60)
    for _ in range(26):
        x, y = rnd.uniform(0, K.W), rnd.uniform(0, K.H)
        color = rnd.choice(((0.3, 0.9, 0.6), (0.9, 0.6, 0.8), (0.95, 0.8, 0.5)))
        K.glow(x, y, rnd.uniform(40, 130), K.glow_mat("Bokeh", color, rnd.uniform(0.08, 0.22)), depth=50)
    for k, (x, w, s) in enumerate(((700, 180, 0.22), (860, 120, 0.3), (1000, 220, 0.12))):
        shaft = K.quad("Lichtbahn", x - w / 2, -200, x + w / 2, 1300, K.glow_mat("Bahn", (0.75, 1.0, 0.75), s), depth=40)
        shaft.rotation_euler = (0, math.radians(-8 + k * 5), 0)
        shaft.visible_shadow = False
    K.alive_room()
    vig = K.quad("Vignette", -200, -150, K.W + 200, K.H + 150, C.vignette_mat(), depth=-70)
    vig.visible_shadow = False
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
        K.tube("Faden", [(x + rnd.uniform(-10, 10), y - 64 - 90), (x, y - 64 - 12)], 1.0, K.THORN, depth=8)

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
    for i in range(8):
        y = top + 50 + i * 92
        side = -1 if i % 2 == 0 else 1
        px = cx + side * 106
        K.tube("Stiel", [(cx + side * 30, y + 12), (cx + side * 58, y + 2), (px - side * 42, y)], 3.2, K.LIGHTVINE, depth=2)
        for a in range(6):
            K.leaf(px, y, a * 60 + 90, 52, 24, K.LEAF, depth=6, bend=0.12)
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

    # Werte: Zeichen, Namen und leere Balken; Zahlen und Füllung zeichnet das Spiel
    stats = [("dorn", "Kraft"), ("wirbel", "Tempo"), ("tropfen", "Saft"), ("nid", "Nid")]
    for k, (kind, label) in enumerate(stats):
        sx, sy = x0 + 64 + k * 112, y0 + 594
        C.icon(kind, sx + 12, sy + 12, 1.3, violet if kind == "nid" else K.mat("Zeichen", (0.12, 0.3, 0.1), 0.5, emit=0.2))
        K.text(label, sx + 34, sy - 2, 17, ink_faint, "Almendra-Regular.ttf", depth=4)
        K.tube("Balken", [(sx, sy + 48), (sx + 92, sy + 48)], 1.0, ink_faint, depth=2)

    # Erbgut: Überschrift, Stängel mit Samen, je Stufe die römische Zahl und zwei Äste (Samen und Blüten zeichnet das Spiel)
    tx, ty = x0 + 690, y0 + 50
    K.text("Erbgut", tx, ty - 4, 34, ink, "AlmendraSC-Regular.ttf", "CENTER", depth=4)
    K.text("je Stufe eine Anlage", tx, ty + 38, 16, ink_faint, "Almendra-Regular.ttf", "CENTER", depth=4)
    green = K.mat("Stammgruen", (0.14, 0.32, 0.12), 0.5, emit=0.15)
    stem = [(tx + 7 * math.sin(k * 0.45), ty + 82 + k * 20) for k in range(29)]
    K.tube("Stamm", stem, 3.6, green, depth=0, taper=False)
    for k in range(3, 28, 5):
        sx, sy = stem[k]
        K.leaf(sx, sy, 180 if k % 2 else 0, 16, 7, K.LEAF, depth=-4)
    K.sphere("Same", tx, ty + 78, 10, K.mat("Same", (0.3, 0.2, 0.1), 0.4), depth=-4)
    for k, roman in enumerate(("I", "II", "III")):
        fy = ty + 170 + k * 168
        K.text(roman, tx, fy - 70, 22, ink, "AlmendraSC-Regular.ttf", "CENTER", depth=4)
        for side in (-1, 1):
            nx, ny = tx + side * 92, fy + 6
            start = (tx + 7 * math.sin((fy - ty - 82) / 20 * 0.45), fy - 6)
            arc = []
            for j in range(13):
                t = j / 12
                bx = (1 - t) ** 2 * start[0] + 2 * (1 - t) * t * (tx + side * 30) + t * t * (nx - side * 20)
                by = (1 - t) ** 2 * start[1] + 2 * (1 - t) * t * (fy - 46) + t * t * (ny - 14)
                arc.append((bx, by))
            K.tube("Ast", arc, 2.8, green, depth=0, taper=True)
            K.leaf(arc[5][0], arc[5][1], -90 + side * 40, 14, 6, K.LEAF, depth=-4)

    K.text("Kreuzungen", x0 + 56, y0 + 676, 28, ink, "AlmendraSC-Regular.ttf", depth=4)


# ---------------------------------------------------------------- Einzelteile

def sprite(path, w, h, builder, scale=2):
    """Rendert ein freigestelltes Teil: w x h Designpixel, scale-fach aufgelöst, Glas lässt den Grund durch"""
    K.W, K.H = w, h
    K.setup()
    K.materials()
    lights()
    scene = bpy.context.scene
    scene.cycles.samples = 64
    scene.render.resolution_x, scene.render.resolution_y = w * scale, h * scale
    scene.render.film_transparent = True
    scene.cycles.film_transparent_glass = True
    scene.camera.data.ortho_scale = max(w, h)
    builder()
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    K.W, K.H = 1920, 1080


def specimen_glass(gold):
    def build():
        x, y, rx, ry = 75, 105, 52, 64
        C.oval_ring("Fassung", x, y, rx + 6, ry + 6, 5.0, K.GOLD if gold else K.BRONZE, depth=-4)
        C.oval_ring("Innen", x, y, rx, ry, 1.4, K.GOLD, depth=-5)
        K.sphere("Knauf", x, y - ry - 8, 6, K.GOLD if gold else K.BRONZE, depth=-6)
        glass = K.sphere("Glas", x, y, 60, K.CAPSULE_GLASS, depth=-2, squash=0.25)
        glass.scale = (rx / 60.0, 0.25, ry / 60.0)
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
        p = K.sphere("Knospe", 40, 40, 18, K.mat("Samenkapsel", (0.25, 0.22, 0.18) if locked else (0.25, 0.35, 0.18), 0.5), depth=-4)
        p.scale = (0.8, 0.6, 1.1)
    return build


def blossom():
    K.blossom(45, 45, 24, K.PETAL, K.GOLD, depth=-6)
    K.ring("Wahl", 45, 45, 34, 2.4, K.GOLD, depth=-8)


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


def main():
    out = sys.argv[sys.argv.index("--") + 1]
    os.makedirs(out, exist_ok=True)
    only = sys.argv[sys.argv.index("--") + 2:]
    patch()
    if not only or "grund" in only:
        C.rnd.seed(2026)
        K.setup()
        K.materials()
        lights()
        scene = bpy.context.scene
        scene.cycles.samples = 96
        scene.cycles.volume_step_rate = 4.0
        room_lights()
        build_background()
        D.god_rays()
        scene.render.filepath = os.path.join(out, "T_Keim_Grund.png")
        bpy.ops.render.render(write_still=True)
    if not only or "teile" in only:
        sprite(os.path.join(out, "T_Keim_Glas.png"), 150, 190, specimen_glass(False))
        sprite(os.path.join(out, "T_Keim_Glas_Gold.png"), 150, 190, specimen_glass(True))
        sprite(os.path.join(out, "T_Keim_Kapsel.png"), 100, 100, capsule)
        sprite(os.path.join(out, "T_Keim_Knospe.png"), 110, 110, bud)
        sprite(os.path.join(out, "T_Keim_Same.png"), 80, 80, pod(False))
        sprite(os.path.join(out, "T_Keim_Same_Zu.png"), 80, 80, pod(True))
        sprite(os.path.join(out, "T_Keim_Bluete.png"), 90, 90, blossom)
        for kind in ("dorn", "wirbel", "tropfen", "nid"):
            sprite(os.path.join(out, "T_Keim_Zeichen_{}.png".format(kind.capitalize())), 40, 40, sign(kind))
    if not only or "bilder" in only:
        glow_picture(os.path.join(out, "T_Keim_Schein.png"))
        K.round_pictures(out)
        oval_pictures(out)
    print("[Vael] Keimgeflecht-Bausteine in", out)


if __name__ == "__main__":
    main()
