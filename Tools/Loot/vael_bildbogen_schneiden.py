"""
Vael - gemalte Bildboegen freistellen (Boris 17.11.)
====================================================

Schneidet jeden 2 x 2-Bogen der Bildgenerierung (vael_bildbogen_prompts.py) in seine vier Viertel, misst die Farbe des
Chroma-Hintergrunds am Rand jedes Viertels (gruen, magenta oder blau, wie das Modell ihn gemalt hat) und rechnet ihn heraus:
wie viel Hintergrundfarbe in einem Pixel steckt, gibt seine Deckkraft; was durchscheint (Glas, Rauch, Funken), behaelt seine
eigene Farbe, der Schatten auf dem Grund wird zu weichem Schwarz. Dann wird das Ding auf ein Quadrat von 512 px gesetzt.

Aufruf (Blender, wegen numpy und der Bildfunktionen):
  blender -b --factory-startup -P vael_bildbogen_schneiden.py -- <bogen.json> <Bogenordner> <Zielordner>
"""
import json
import os
import sys

import bpy
import numpy as np

ARGS = sys.argv[sys.argv.index("--") + 1:]
SHEETS, FOLDER, OUT = ARGS[0], ARGS[1], ARGS[2]
SIZE = 512
FILL = 0.9


def load(path):
    img = bpy.data.images.load(path)
    w, h = img.size
    a = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1, :, :3]  # top row first
    bpy.data.images.remove(img)
    return a


def save(rgba, path):
    h, w = rgba.shape[:2]
    img = bpy.data.images.new("o", w, h, alpha=True)
    img.pixels[:] = rgba[::-1].ravel()
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)


def keyness(rgb, kind):
    """How much of the key colour is in each pixel: the key channel(s) above the others"""
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    if kind == "green":
        return g - np.maximum(r, b)
    if kind == "magenta":
        return np.minimum(r, b) - g
    return b - np.maximum(r, g)


def trim_white(quarter):
    """Some sheets have white gutters between the quarters: cut rows and columns off the edges while they are mostly white"""
    white = quarter.min(axis=2) > 0.8
    top, bottom, left, right = 0, quarter.shape[0], 0, quarter.shape[1]
    while top < bottom - 64 and white[top, left:right].mean() > 0.3:
        top += 1
    while bottom > top + 64 and white[bottom - 1, left:right].mean() > 0.3:
        bottom -= 1
    while left < right - 64 and white[top:bottom, left].mean() > 0.3:
        left += 1
    while right > left + 64 and white[top:bottom, right - 1].mean() > 0.3:
        right -= 1
    margin = 4 if (top, left) != (0, 0) or (bottom, right) != quarter.shape[:2] else 0
    return quarter[top + margin:bottom - margin, left + margin:right - margin]


def cut(quarter):
    quarter = trim_white(quarter)
    h, w = quarter.shape[:2]
    band = 14
    border = np.concatenate([quarter[:band].reshape(-1, 3), quarter[-band:].reshape(-1, 3), quarter[:, :band].reshape(-1, 3), quarter[:, -band:].reshape(-1, 3)])
    # the key is the colourful part of the border, not white gutters or grey shadows
    sat = border.max(axis=1) - border.min(axis=1)
    coloured = border[sat > np.percentile(sat, 50)]
    key = np.median(coloured if len(coloured) > 50 else border, axis=0)
    r, g, b = key
    kind = "green" if g > max(r, b) else "magenta" if min(r, b) > g else "blue"
    # how much key colour there is relative to how bright the pixel is: a darker shade of the key (vignette) is background too
    k_key = max(float(keyness(key[None, None], kind)[0, 0]) / max(float(key.max()), 0.05), 0.05)
    k = keyness(quarter, kind) / np.maximum(quarter.max(axis=2), 0.12) / k_key
    alpha = 1.0 - np.clip((k - 0.12) / (0.8 - 0.12), 0.0, 1.0)
    # un-mix the key out of what is left; what was only shadow of the key goes black
    a = np.maximum(alpha, 1e-3)[..., None]
    fg = np.clip((quarter - (1.0 - a) * key) / a, 0.0, 1.0)
    # despill: no channel of the key may stand above the others in what remains
    if kind == "green":
        fg[..., 1] = np.minimum(fg[..., 1], np.maximum(fg[..., 0], fg[..., 2]) * 1.05)
    elif kind == "magenta":
        cap = fg[..., 1] * 1.05 + 0.04
        fg[..., 0] = np.where(alpha < 0.98, np.minimum(fg[..., 0], np.maximum(cap, fg[..., 0] * alpha)), fg[..., 0])
        fg[..., 2] = np.where(alpha < 0.98, np.minimum(fg[..., 2], np.maximum(cap, fg[..., 2] * alpha)), fg[..., 2])
    else:
        fg[..., 2] = np.minimum(fg[..., 2], np.maximum(fg[..., 0], fg[..., 1]) * 1.05)
    alpha[:6] = alpha[-6:] = 0.0
    alpha[:, :6] = alpha[:, -6:] = 0.0
    alpha[alpha < 0.04] = 0.0
    return np.dstack([fg, alpha]), kind


def place(rgba):
    """The thing alone, centred, filling FILL of a square of SIZE"""
    ys, xs = np.nonzero(rgba[..., 3] > 0.15)
    if len(xs) == 0:
        return None
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    crop = rgba[y0:y1, x0:x1]
    side = max(crop.shape[0], crop.shape[1])
    scale = SIZE * FILL / side
    nh, nw = max(1, int(round(crop.shape[0] * scale))), max(1, int(round(crop.shape[1] * scale)))
    # resample with premultiplied colour so the edges keep no key fringe
    pre = crop.copy()
    pre[..., :3] *= pre[..., 3:4]
    yi = np.clip((np.arange(nh) + 0.5) / scale - 0.5, 0, crop.shape[0] - 1)
    xi = np.clip((np.arange(nw) + 0.5) / scale - 0.5, 0, crop.shape[1] - 1)
    y0i, x0i = np.floor(yi).astype(int), np.floor(xi).astype(int)
    y1i, x1i = np.minimum(y0i + 1, crop.shape[0] - 1), np.minimum(x0i + 1, crop.shape[1] - 1)
    fy, fx = (yi - y0i)[:, None, None], (xi - x0i)[None, :, None]
    top = pre[y0i][:, x0i] * (1 - fx) + pre[y0i][:, x1i] * fx
    bottom = pre[y1i][:, x0i] * (1 - fx) + pre[y1i][:, x1i] * fx
    res = top * (1 - fy) + bottom * fy
    res[..., :3] /= np.maximum(res[..., 3:4], 1e-4)
    out = np.zeros((SIZE, SIZE, 4), np.float32)
    oy, ox = (SIZE - nh) // 2, (SIZE - nw) // 2
    out[oy:oy + nh, ox:ox + nw] = res
    return out


def main():
    os.makedirs(OUT, exist_ok=True)
    sheets = json.load(open(SHEETS, encoding="utf-8"))
    done = 0
    for sheet in sheets:
        path = os.path.join(FOLDER, sheet["id"] + ".png")
        if not os.path.exists(path):
            print("SCHNITT fehlt", sheet["id"], flush=True)
            continue
        image = load(path)
        h, w = image.shape[:2]
        for k, name in enumerate(sheet["items"]):
            row, col = divmod(k, 2)
            quarter = image[row * h // 2:(row + 1) * h // 2, col * w // 2:(col + 1) * w // 2]
            rgba, kind = cut(quarter)
            placed = place(rgba)
            if placed is None:
                print("SCHNITT leer", sheet["id"], name, flush=True)
                continue
            save(placed, os.path.join(OUT, "T_Icon_" + name + ".png"))
            done += 1
            print("SCHNITT", sheet["id"], kind, name, flush=True)
    print("SCHNITT FERTIG", done, flush=True)


main()
