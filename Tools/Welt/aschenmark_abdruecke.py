# Marks on the ground of the Aschenmark, laid as decals by Scripts/BuildAschenmarkBoden.py:
#   Brandfleck  a charred patch with a grey ash ring
#   Glutriss    a glowing crack (RGB = glow mask, A = cover), only in the Glutfelder
#   Aschewehe   a long drift of pale ash with ripples
# Writes D_<Name>.png (1024 px, RGB + alpha) to the export folder and AM_Abdruecke.json (where they lie, world metres).
#
#   blender -b --factory-startup -P Tools/Welt/aschenmark_abdruecke.py -- C:/Projekte/Vael_Quellen/Aschenmark/boden C:/Projekte/Vael/Scripts/Data/Welt

import json
import math
import os
import sys

import bpy
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
root = args[0] if args else "C:/Projekte/Vael_Quellen/Aschenmark/boden"
welt = args[1] if len(args) > 1 else "C:/Projekte/Vael/Scripts/Data/Welt"
OUT = os.path.join(root, "export")
os.makedirs(OUT, exist_ok=True)
S = 1024
SIZE_X, SIZE_Y = 2017, 2521
rng = np.random.default_rng(811)


def fbm(beta, seed, size=S):
    r = np.random.default_rng(seed)
    f = np.fft.fftfreq(size) * size
    fx, fy = np.meshgrid(f, f)
    k = np.hypot(fx, fy)
    k[0, 0] = 1
    n = np.real(np.fft.ifft2((r.normal(size=(size, size)) + 1j * r.normal(size=(size, size))) / k ** (beta / 2)))
    return (n - n.min()) / (n.max() - n.min())


def save(rgb, alpha, name):
    image = bpy.data.images.new(name, S, S, alpha=True)
    image.pixels.foreach_set(np.ascontiguousarray(np.dstack([np.clip(rgb, 0, 1), np.clip(alpha, 0, 1)])[::-1]).astype(np.float32).ravel())
    image.filepath_raw = os.path.join(OUT, name + ".png")
    image.file_format = "PNG"
    image.save()
    bpy.data.images.remove(image)


yy, xx = (np.mgrid[0:S, 0:S] + 0.5) / S * 2 - 1
radius = np.hypot(xx, yy)
angle = np.arctan2(yy, xx)

# Brandfleck: a ragged charred blot, darkest inside, a grey ring of ash round it
edge_noise = fbm(2.8, 1) * 0.7 + fbm(1.9, 11) * 0.3
ragged = radius * (1.0 + 0.25 * np.cos(angle * 2 + 1.0)) + (edge_noise - 0.5) * 0.7
core = np.clip((0.75 - ragged) / 0.25, 0, 1)
ring = np.clip(1 - np.abs(ragged - 0.78) / 0.12, 0, 1) * (fbm(1.5, 2) > 0.4)
grain = fbm(1.2, 3)
char = np.array([0.03, 0.028, 0.025]) * (0.7 + 0.6 * grain)[..., None]
ashring = np.array([0.42, 0.40, 0.38]) * (0.8 + 0.3 * grain)[..., None]
rgb = char * core[..., None] + ashring * (1 - core[..., None])
holes = np.clip((fbm(2.4, 12) - 0.35) * 3, 0, 1)
alpha = np.maximum(core * (0.45 + 0.4 * grain) * holes, ring * 0.45) * np.clip((1.0 - radius) / 0.2, 0, 1)
save(rgb, alpha, "D_Brandfleck")

# Glutriss: one long crack with branches; glow in the middle, a dark scorched rim
warp = fbm(3.2, 4)
line = np.abs(yy + (warp - 0.5) * 0.6 + 0.15 * np.sin(xx * 5))
branch = np.abs(xx * 0.6 + yy * 0.9 - 0.2 + (fbm(3.0, 5) - 0.5) * 0.5) + np.clip(xx + 0.1, 0, 1) * 3
crack = np.minimum(line, branch * 1.4)
fade = np.clip((0.95 - np.abs(xx)) / 0.3, 0, 1) * np.clip((1.0 - radius * 0.8), 0, 1)
width = 0.012 + 0.018 * fbm(3.0, 6)
glow = np.clip(1 - crack / width, 0, 1) ** 1.5 * fade
rim = np.clip(1 - crack / (width * 4), 0, 1) ** 0.7 * fade
save(np.dstack([glow, glow, glow]), np.maximum(rim * 0.8, glow), "D_Glutriss")

# Aschewehe: a long soft drift (stretched along x) with fine ripples across it
drift = np.clip(1 - np.hypot(xx * 0.6, yy * 1.9 + (fbm(2.0, 7) - 0.5) * 0.6), 0, 1)
ripples = 0.5 + 0.5 * np.sin((xx * 22 + yy * 3 + fbm(2.6, 8) * 9))
grain = fbm(1.3, 9)
rgb = np.array([0.55, 0.53, 0.50]) * (0.85 + 0.12 * ripples + 0.1 * grain)[..., None]
alpha = drift ** 1.2 * (0.55 + 0.25 * ripples) * (0.8 + 0.2 * grain)
save(rgb, alpha, "D_Aschewehe")

# ---------------------------------------------------------------- where they lie
raw = np.fromfile(os.path.join(welt, "AM_Hoehen.r16"), dtype="<u2")
h = ((raw.astype(np.float32) - 32768.0) / 128.0).reshape(SIZE_Y, SIZE_X).T            # h[x, y]
wege = bpy.data.images.load(os.path.join(welt, "T_AM_Wege.png"))
ww, wh = wege.size
mask = np.array(wege.pixels[:], dtype=np.float32).reshape(wh, ww, 4)[::-1]             # mask[x / 2, y / 2]


def at(x, y):
    ix, iy = int(np.clip(x, 0, SIZE_X - 1)), int(np.clip(y, 0, SIZE_Y - 1))
    m = mask[min(ix // 2, wh - 1), min(iy // 2, ww - 1)]
    slope = abs(h[min(ix + 2, SIZE_X - 1), iy] - h[max(ix - 2, 0), iy]) + abs(h[ix, min(iy + 2, SIZE_Y - 1)] - h[ix, max(iy - 2, 0)])
    return h[ix, iy], m[0], m[1], m[2], slope / 4.0


marks = []
counts = {"Brandfleck": 0, "Glutriss": 0, "Aschewehe": 0}
WANT = {"Brandfleck": 320, "Glutriss": 220, "Aschewehe": 300}
tries = 0
while any(counts[k] < WANT[k] for k in WANT) and tries < 200000:
    tries += 1
    x, y = rng.uniform(5, SIZE_X - 5), rng.uniform(5, SIZE_Y - 5)
    z, way, clear, zone, slope = at(x, y)
    if z < 1.0 or way > 0.3 or slope > 0.6:
        continue
    kind = rng.choice(["Brandfleck", "Glutriss", "Aschewehe"])
    if counts[kind] >= WANT[kind]:
        continue
    glut = max(0.0, 1.0 - abs(zone - 0.667) * 3.0)
    if kind == "Glutriss" and glut < 0.5:
        continue
    if kind == "Brandfleck" and rng.random() > 0.35 + glut * 0.6:
        continue
    if kind == "Aschewehe" and zone > 0.85:
        continue
    size = {"Brandfleck": rng.uniform(2.0, 6.0), "Glutriss": rng.uniform(4.0, 10.0), "Aschewehe": rng.uniform(5.0, 12.0)}[kind]
    marks.append({"art": kind, "x": round(float(x), 2), "y": round(float(y), 2), "z": round(float(z), 2),
                  "yaw": round(float(rng.uniform(0, 360)), 1), "groesse": round(float(size), 2)})
    counts[kind] += 1

with open(os.path.join(welt, "AM_Abdruecke.json"), "w", encoding="utf-8") as f:
    json.dump({"info": "Abdrücke auf dem Boden in Weltmetern (x Norden, y Osten, z Boden), groesse = Durchmesser in m", "abdruecke": marks}, f,
              ensure_ascii=False, indent=0)
print("[aschenmark_abdruecke] written", counts)
