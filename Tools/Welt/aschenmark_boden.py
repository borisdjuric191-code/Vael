# The eight ground layers of the Aschenmark, packed for the texture arrays of M_AM_Boden (Scripts/BuildAschenmarkBoden.py).
# Pack surfaces (exported read only by Scripts/ExportBodenQuellen.py) are graded towards ash; cracked earth, slag and rock
# are made here, tileable. Every layer becomes two 2048 px pictures:
#   L<i>_BH.png   albedo (RGB) + height (A)
#   L<i>_NR.png   normal X, Y (RG, 0.5 = flat), roughness (B), occlusion (A)
#
#   blender -b --factory-startup -P Tools/Welt/aschenmark_boden.py -- C:/Projekte/Vael_Quellen/Aschenmark/boden
#
# Layers: 0 Asche, 1 rissige Erde, 2 Schlacke, 3 Fels, 4 Kies, 5 Moos, 6 verbranntes Gras, 7 Weg.
# Also writes vorschau_boden.png, all layers side by side.

import os
import sys

import bpy
import numpy as np

root = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else "C:/Projekte/Vael_Quellen/Aschenmark/boden"
PACK = os.path.join(root, "pack")
OUT = os.path.join(root, "export")
os.makedirs(OUT, exist_ok=True)
S = 2048
rng = np.random.default_rng(1108)


# ---------------------------------------------------------------- helpers

def load(name):
    """A pack picture as float [S, S, 4], halved if it is bigger; None if it is missing"""
    path = os.path.join(PACK, name + ".png")
    if not os.path.isfile(path):
        return None
    image = bpy.data.images.load(path)
    image.colorspace_settings.name = "Non-Color"
    w, h = image.size
    a = np.array(image.pixels[:], dtype=np.float32).reshape(h, w, 4)
    bpy.data.images.remove(image)
    while a.shape[0] > S:
        a = (a[0::2, 0::2] + a[1::2, 0::2] + a[0::2, 1::2] + a[1::2, 1::2]) * 0.25
    if a.shape[0] < S:
        a = np.repeat(np.repeat(a, S // a.shape[0], axis=0), S // a.shape[1], axis=1)
    return a


def save(a, name):
    image = bpy.data.images.new(name, S, S, alpha=True, float_buffer=False)
    image.colorspace_settings.name = "Non-Color"
    image.pixels.foreach_set(np.ascontiguousarray(np.clip(a, 0, 1)).astype(np.float32).ravel())
    image.filepath_raw = os.path.join(OUT, name + ".png")
    image.file_format = "PNG"
    image.save()
    bpy.data.images.remove(image)


def fbm(beta=2.0, low=2, seed=0):
    """Periodic noise: white noise shaped to 1/f^beta in frequency space, 0..1"""
    r = np.random.default_rng(seed)
    f = np.fft.fftfreq(S) * S
    fx, fy = np.meshgrid(f, f)
    k = np.hypot(fx, fy)
    k[0, 0] = 1
    spectrum = (r.normal(size=(S, S)) + 1j * r.normal(size=(S, S))) / k ** (beta / 2)
    spectrum[k < low] = 0
    n = np.real(np.fft.ifft2(spectrum))
    return (n - n.min()) / (n.max() - n.min())


def voronoi(cells, seed=0, jitter=0.9, warp=0.0):
    """Periodic Voronoi on a grid of cells x cells: F1, F2 (in cell units) and the id of the nearest point;
    warp bends the cell walls (in cells) with periodic noise, so they wander like real cracks"""
    r = np.random.default_rng(seed)
    points = r.random((cells, cells, 2)) * jitter + (1 - jitter) / 2
    coords = (np.arange(S) + 0.5) / S * cells
    gx, gy = np.meshgrid(coords, coords)            # gx along columns
    if warp > 0:
        gx = (gx + (fbm(2.0, 3, seed=seed + 100) - 0.5) * warp) % cells
        gy = (gy + (fbm(2.0, 3, seed=seed + 200) - 0.5) * warp) % cells
    cx, cy = np.floor(gx).astype(np.int32), np.floor(gy).astype(np.int32)
    f1 = np.full((S, S), 9.0, dtype=np.float32)
    f2 = np.full((S, S), 9.0, dtype=np.float32)
    ident = np.zeros((S, S), dtype=np.int32)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            nx, ny = cx + dx, cy + dy
            p = points[ny % cells, nx % cells]
            d = np.hypot(nx + p[..., 0] - gx, ny + p[..., 1] - gy).astype(np.float32)
            closer = d < f1
            f2 = np.where(closer, f1, np.minimum(f2, d))
            ident = np.where(closer, (ny % cells) * cells + (nx % cells), ident)
            f1 = np.where(closer, d, f1)
    return f1, f2, ident


def blur(a, r):
    for _ in range(2):
        for axis in (0, 1):
            acc = np.zeros_like(a)
            for k in range(-r, r + 1):
                acc += np.roll(a, k, axis=axis)
            a = acc / (2 * r + 1)
    return a


def normals_from(height, strength):
    """Tangent normal (RG, 0.5 = flat) from a periodic height"""
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * strength
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * strength
    n = np.dstack([-dx, -dy, np.ones_like(height)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    return n[..., 0] * 0.5 + 0.5, n[..., 1] * 0.5 + 0.5


def grade(rgb, saturation, tint, gain):
    grey = rgb.mean(axis=2, keepdims=True)
    return (grey + (rgb - grey) * saturation) * np.array(tint) * gain


def lum(rgb):
    return rgb[..., 0] * 0.3 + rgb[..., 1] * 0.59 + rgb[..., 2] * 0.11


def srgb(linear_like):
    """The pack pictures come in sRGB already; procedural colours are written as sRGB values directly"""
    return linear_like


def layer(index, albedo, height, nx, ny, rough, ao=None):
    height = (height - height.min()) / max(height.max() - height.min(), 1e-5)
    save(np.dstack([albedo, height]), "L{}_BH".format(index))
    save(np.dstack([nx, ny, rough, np.ones_like(rough) if ao is None else ao]), "L{}_NR".format(index))
    return albedo


def from_pack(rgb_name, normal_name, height_name, rough_name, rough_channel=0):
    rgb = load(rgb_name)
    normal = load(normal_name) if normal_name else None
    height = load(height_name) if height_name else None
    rough = load(rough_name) if rough_name else None
    h = height[..., 0] if height is not None else blur(lum(rgb[..., :3]), 2)
    if normal is not None:
        nx, ny = normal[..., 0], normal[..., 1]
    else:
        nx, ny = normals_from(h - blur(h, 12), 6.0)
    r = rough[..., rough_channel] if rough is not None else np.full((S, S), 0.85, dtype=np.float32)
    return rgb[..., :3], h, nx, ny, r


previews = []

# 0 Asche: the sandy mud, greyed to fine ash with a warm hint
rgb, h, nx, ny, r = from_pack("asche_D", None, None, None)
ash = grade(rgb, 0.08, (1.0, 0.99, 0.97), 0.78)
previews.append(layer(0, ash, h, nx, ny, np.clip(r * 1.05, 0, 1)))

# 1 rissige Erde: dried ash-mud broken into wandering plates, fine cracks inside, the ash itself as the surface
ash_rgb0, ash_h0, _, _, _ = from_pack("asche_D", None, None, None)
f1, f2, cell = voronoi(7, seed=1, warp=1.4)
g1, g2, _ = voronoi(22, seed=2, warp=1.2)
width = fbm(2.2, 3, seed=3)
edge = f2 - f1
crack = np.clip(1.0 - edge / (0.05 + 0.06 * width), 0, 1) ** 1.2
small = np.clip(1.0 - (g2 - g1) / 0.025, 0, 1) ** 2 * np.clip((fbm(1.6, 4, seed=4) - 0.4) * 4, 0, 1)
plate_value = np.random.default_rng(5).random(49)[cell]
curl = np.clip(edge * 3, 0, 1) ** 0.5
height = ash_h0 * 0.3 + curl * 0.5 - crack * 0.8 - small * 0.3
earth = grade(ash_rgb0, 0.25, (1.0, 0.94, 0.88), 0.55) * (0.9 + 0.15 * plate_value)[..., None]
earth = earth * (1.0 - crack[..., None] * 0.75 - small[..., None] * 0.35)
nx, ny = normals_from(height, 10.0)
previews.append(layer(1, earth, height, nx, ny, 0.9 - crack * 0.1))

# 2 Schlacke: glassy dark lumps, gas bubbles, rust between them and a dull shine on the tops
f1, f2, cell = voronoi(14, seed=7, jitter=1.0, warp=0.6)
b1, b2, _ = voronoi(55, seed=8)
lumps = np.clip(1.0 - f1 * 1.1, 0, 1) ** 1.3
bubbles = np.clip(1.0 - b1 / 0.25, 0, 1) * np.clip((fbm(2.0, 4, seed=9) - 0.45) * 5, 0, 1)
gaps = np.clip(1.0 - (f2 - f1) / 0.1, 0, 1)
noise = fbm(1.4, 6, seed=10)
lump_value = np.random.default_rng(17).random(196)[cell]
height = lumps * 0.8 + noise * 0.25 - bubbles * 0.35 - gaps * 0.25
rust = np.clip((fbm(1.8, 3, seed=11) - 0.5) * 3, 0, 1) * (1 - lumps * 0.7)
slag = np.array([0.15, 0.14, 0.135])[None, None, :] * (0.65 + 0.5 * noise + 0.35 * lump_value)[..., None]
slag = slag * (1 - rust[..., None] * 0.8) + np.array([0.30, 0.16, 0.08]) * rust[..., None] * 0.8
slag = slag * (1 - gaps[..., None] * 0.5) * (1 - bubbles[..., None] * 0.5)
slag = slag + np.array([0.06, 0.06, 0.065]) * np.clip(lumps - 0.55, 0, 1)[..., None] * 2
nx, ny = normals_from(height, 12.0)
previews.append(layer(2, slag, height, nx, ny, np.clip(0.7 - lumps * 0.4 + rust * 0.25 + gaps * 0.2, 0.25, 1)))

# 3 Fels: weathered grey-brown rock, ridged noise with faint layers, no cells
warp = fbm(2.4, 2, seed=12)
yy = np.mgrid[0:S, 0:S][0] / S
strata = np.sin((yy * 6 + warp * 2.0) * 2 * np.pi) * 0.5 + 0.5
ridged = 1.0 - np.abs(fbm(1.9, 3, seed=13) * 2 - 1)
detail = fbm(1.4, 6, seed=14)
cracks = np.clip((ridged - 0.9) * 10, 0, 1)
height = strata * 0.2 + detail * 0.45 + (1 - ridged) * 0.35 - cracks * 0.3
rock = np.array([0.33, 0.31, 0.29])[None, None, :] * (0.7 + 0.2 * strata + 0.45 * detail)[..., None]
lichen = np.clip((fbm(1.7, 5, seed=18) - 0.62) * 6, 0, 1)
rock = rock * (1 - cracks[..., None] * 0.55) * (1 - lichen[..., None] * 0.15) + np.array([0.05, 0.045, 0.03]) * lichen[..., None]
nx, ny = normals_from(height, 14.0)
previews.append(layer(3, rock, height, nx, ny, np.full((S, S), 0.82, dtype=np.float32)))

# 4 Kies: cinder gravel, darker and cooler than the pack
rgb, h, nx, ny, r = from_pack("kies_B", "kies_N", None, "kies_ORM", rough_channel=1)
previews.append(layer(4, grade(rgb, 0.3, (0.95, 0.95, 0.97), 0.62), h, nx, ny, r))

# 5 Moos: the forest moss towards the Aschenpass, darker and a little dusty
rgb, h, nx, ny, r = from_pack("moos_B", "moos_N", None, "moos_ORM", rough_channel=1)
previews.append(layer(5, grade(rgb, 0.35, (0.85, 1.0, 0.85), 0.55), h, nx, ny, r))

# 6 verbranntes Gras: the grassy mud, burnt: green to olive-brown, black char spots
rgb, h, nx, ny, r = from_pack("schlamm_A", "schlamm_N", "schlamm_D", "schlamm_R")
burnt = grade(rgb, 0.22, (1.0, 0.92, 0.82), 0.5)
char = np.clip((fbm(1.8, 6, seed=15) - 0.6) * 5, 0, 1)
burnt = burnt * (1 - char[..., None] * 0.75)
previews.append(layer(6, burnt, h, nx, ny, r))

# 7 Weg: trodden ash and gravel, lighter, smoother, a few ruts along the u axis
ash_rgb, ash_h, _, _, _ = from_pack("asche_D", None, None, None)
gravel_rgb, gravel_h, _, _, _ = from_pack("kies_B", None, None, None)
mix = np.clip((fbm(1.6, 4, seed=16) - 0.45) * 4, 0, 1)[..., None]
road = grade(ash_rgb, 0.15, (1.0, 0.97, 0.93), 0.78) * (1 - mix) + grade(gravel_rgb, 0.3, (1, 1, 1), 0.75) * mix
xx = np.mgrid[0:S, 0:S][1] / S
ruts = np.exp(-((xx - 0.3) / 0.05) ** 2) + np.exp(-((xx - 0.7) / 0.05) ** 2)
height = blur(ash_h, 3) * 0.6 + gravel_h * 0.3 * mix[..., 0] - ruts * 0.15
road = road * (1 - ruts[..., None] * 0.12)
nx, ny = normals_from(height - blur(height, 10), 5.0)
previews.append(layer(7, road, height, nx, ny, np.full((S, S), 0.9, dtype=np.float32)))

# Preview: all layers side by side, each 512 px
tiles = [p[::4, ::4] for p in previews]
row = np.concatenate(tiles, axis=1)
image = bpy.data.images.new("vorschau_boden", row.shape[1], row.shape[0], alpha=True)
image.pixels.foreach_set(np.ascontiguousarray(np.dstack([np.clip(row, 0, 1), np.ones(row.shape[:2])])[::-1]).astype(np.float32).ravel())
image.filepath_raw = os.path.join(OUT, "vorschau_boden.png")
image.file_format = "PNG"
image.save()
print("[aschenmark_boden] written 8 layers to", OUT)
