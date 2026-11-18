"""
Vael - the painted mage menus from Tripo, made ready for the game (Blender headless, only numpy, no Tripo).

Boris 18.11.: the four pictures painted by Tripo (C:/Projekte/Vael_Quellen/HUD_Tripo/Magier) become the mage's HUD, formula
page, inventory and menu frame. The game draws into their empty places; this script only tidies them:
  - Inventar: the four gloves go (they make no sense), three new rings from the painted ones take their place and one under
    the figure, so there are ten places for the ten pieces of equipment; the 5x3 grid of the right page is taken out and
    painted again as 7x5 for the 35 fields of the backpack; the lit tab is put out (the game lights the open one).
  - Formeln: every row gets a third circle for the elements, one square on the right for the result; the lit quick slot is
    put out.
  - Menue: the lit tab is put out, the bar emptied (the game fills it).
  - HUD: the white ground is cut away.
Places are in pixels of the 1344x768 pictures, the same numbers are in Source/Vael/UI/VaelHUDMage.cpp.

  blender -b --factory-startup -P vael_tripo_magier.py -- <source folder> <target folder>
"""
import math
import sys

import bpy
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
SOURCE = args[0] if len(args) > 0 else "C:/Projekte/Vael_Quellen/HUD_Tripo/Magier"
TARGET = args[1] if len(args) > 1 else "C:/Projekte/Vael/Scripts/Data/HUD/Gemalt"
rng = np.random.default_rng(18)
# inside of the bar of the menu frame
BAR_X0, BAR_Y0, BAR_Y1 = 492, 621, 640


def load(name):
    im = bpy.data.images.load(SOURCE + "/" + name)
    w, h = im.size
    a = np.array(im.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1].copy()
    bpy.data.images.remove(im)
    return a


def save(a, name):
    h, w = a.shape[:2]
    o = bpy.data.images.new(name, w, h, alpha=True)
    o.pixels[:] = np.clip(a, 0, 1)[::-1].ravel()
    o.filepath_raw = TARGET + "/" + name + ".png"
    o.file_format = "PNG"
    o.save()
    print("SAVED", o.filepath_raw)


def box_mask(h, w, feather):
    yy, xx = np.mgrid[0:h, 0:w]
    d = np.minimum.reduce([xx + 0.5, w - xx - 0.5, yy + 0.5, h - yy - 0.5])
    return np.clip(d / max(feather, 0.01), 0, 1)


def ring_mask(h, w, r0, r1, soft=1.5):
    yy, xx = np.mgrid[0:h, 0:w]
    r = np.hypot(xx - (w - 1) / 2, yy - (h - 1) / 2)
    return np.clip((r - r0) / soft + 0.5, 0, 1) * np.clip((r1 - r) / soft + 0.5, 0, 1)


def blur(x, radius):
    """box blur along both axes, a few times: near enough to a gaussian"""
    for _ in range(3):
        for axis in (0, 1):
            k = 2 * radius + 1
            pad = [(0, 0)] * x.ndim
            pad[axis] = (radius + 1, radius)
            c = np.cumsum(np.pad(x, pad, mode="edge"), axis=axis)
            x = (np.take(c, np.arange(k, c.shape[axis]), axis=axis) - np.take(c, np.arange(0, c.shape[axis] - k), axis=axis)) / k
    return x


def patch(a, cx, cy, hw, hh):
    x0, y0 = int(round(cx - hw)), int(round(cy - hh))
    return a[y0:y0 + 2 * hh, x0:x0 + 2 * hw].copy()


def paste(a, piece, cx, cy, mask):
    hh, hw = piece.shape[0] // 2, piece.shape[1] // 2
    x0, y0 = int(round(cx - hw)), int(round(cy - hh))
    region = a[y0:y0 + piece.shape[0], x0:x0 + piece.shape[1]]
    m = mask[..., None]
    region[..., :3] = region[..., :3] * (1 - m) + piece[..., :3] * m


def copy(a, src, dst, hw, hh, feather=4.0):
    paste(a, patch(a, src[0], src[1], hw, hh), dst[0], dst[1], box_mask(2 * hh, 2 * hw, feather))


def resample(piece, w, h):
    """bilinear: the piece to w x h pixels"""
    sh, sw = piece.shape[:2]
    ys = (np.arange(h) + 0.5) * sh / h - 0.5
    xs = (np.arange(w) + 0.5) * sw / w - 0.5
    y0 = np.clip(np.floor(ys).astype(int), 0, sh - 2)
    x0 = np.clip(np.floor(xs).astype(int), 0, sw - 2)
    fy = np.clip(ys - y0, 0, 1)[:, None, None]
    fx = np.clip(xs - x0, 0, 1)[None, :, None]
    p = piece
    top = p[y0][:, x0] * (1 - fx) + p[y0][:, x0 + 1] * fx
    bottom = p[y0 + 1][:, x0] * (1 - fx) + p[y0 + 1][:, x0 + 1] * fx
    return top * (1 - fy) + bottom * fy


def metal(a):
    """the grey metal of the frames and their dark shadows, against the warm parchment"""
    mx = a[..., :3].max(-1)
    mn = a[..., :3].min(-1)
    sat = (mx - mn) / np.maximum(mx, 1e-4)
    return (sat < 0.3) | (mx < 0.4)


def dilate(m, n):
    for _ in range(n):
        m = m | np.roll(m, 1, 0) | np.roll(m, -1, 0) | np.roll(m, 1, 1) | np.roll(m, -1, 1)
    return m


def inpaint(a, x0, y0, x1, y1, mask, grain_from):
    """fills the masked pixels inside the box from their surroundings (diffusion), then gives them the grain of the parchment"""
    sub = a[y0:y1, x0:x1, :3]
    m = mask[y0:y1, x0:x1]
    out = sub.copy()
    out[m] = sub[~m].mean(0)
    for _ in range(500):
        s = (np.roll(out, 1, 0) + np.roll(out, -1, 0) + np.roll(out, 1, 1) + np.roll(out, -1, 1)) * 0.25
        out[m] = s[m]
    gx0, gy0, gx1, gy1 = grain_from
    clean = a[gy0:gy1, gx0:gx1, :3]
    detail = clean - blur(clean, 4)
    std = detail.std(axis=(0, 1))
    noise = blur(rng.standard_normal(out.shape[:2]).astype(np.float32), 1)
    noise /= max(noise.std(), 1e-4)
    out[m] += (noise[..., None] * std[None, None, :] * 0.9)[m]
    soft = blur(m.astype(np.float32), 1)[..., None]
    sub[...] = sub * (1 - soft) + out * soft


def cut_white(a, threshold=0.9):
    """the white ground round a painted thing becomes see-through: the white connected to the border goes, its fringe is unmixed"""
    white = a[..., :3].min(-1)
    cand = white > threshold
    h, w = cand.shape
    bg = np.zeros_like(cand)
    bg[0, :] = cand[0, :]
    bg[-1, :] = cand[-1, :]
    bg[:, 0] = cand[:, 0]
    bg[:, -1] = cand[:, -1]
    while True:
        grown = bg | ((np.roll(bg, 1, 0) | np.roll(bg, -1, 0) | np.roll(bg, 1, 1) | np.roll(bg, -1, 1)) & cand)
        if (grown == bg).all():
            break
        bg = grown
    # the fringe: two pixels round the ground, light pixels there are part white
    fringe = dilate(bg, 2) & ~bg
    alpha = np.ones((h, w), np.float32)
    alpha[bg] = 0.0
    f = np.clip((1.0 - white) / (1.0 - 0.35), 0.0, 1.0)
    alpha[fringe] = np.minimum(alpha[fringe], f[fringe])
    # unmix the white out of the fringe
    rgb = a[..., :3]
    am = np.maximum(alpha, 0.05)[..., None]
    unmixed = (rgb - (1 - am)) / am
    rgb[fringe] = np.clip(unmixed[fringe], 0, 1)
    a[..., 3] = alpha
    return a


# ------------------------------------------------------------------------------------------------------------- Inventar
def inventar():
    a = load("3_Inventar.png")

    # the gloves go: their places filled from the parchment round them, with its grain
    gloves = np.zeros(a.shape[:2], bool)
    yy, xx = np.mgrid[0:a.shape[0], 0:a.shape[1]]
    for cx, cy, rx, ry in ((328.5, 362, 23, 46), (608.5, 362, 23, 46), (381.5, 494, 25, 37), (556.5, 494, 24, 37)):
        gloves |= ((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2 <= 1.0
    inpaint(a, 290, 300, 650, 545, gloves, grain_from=(305, 456, 355, 540))

    # new places, cloned from the painted rings: hands and belt where the upper gloves were, the boots under the figure
    big = patch(a, 348.5, 286.5, 31, 31)
    small = patch(a, 346, 430, 25, 25)
    for cx, cy in ((338, 362), (592, 362)):
        paste(a, big, cx, cy, ring_mask(62, 62, 19.0, 29.5))
    paste(a, small, 468.5, 575, ring_mask(50, 50, 13.0, 23.5))

    # the right page: the lit tab put out (a copy of its neighbour), then the 5x3 grid away and 7x5 fields painted
    copy(a, (823, 175), (887, 175), 30, 40, feather=3.0)
    field = patch(a, 755, 446.5, 27, 28)
    field_mask = blur(dilate(metal(field), 1).astype(np.float32), 1)
    x0, y0, x1, y1 = 724, 409, 1041, 605
    lines = np.zeros(a.shape[:2], bool)
    lines[y0:y1, x0:x1] = dilate(metal(a[y0:y1, x0:x1]), 2)[...]
    inpaint(a, x0 - 6, y0 - 6, x1 + 6, y1 + 6, lines, grain_from=(760, 260, 1000, 380))
    cell = 37
    small_field = resample(field, cell, cell)
    small_mask = resample(field_mask[..., None], cell, cell)[..., 0]
    for row in range(5):
        for column in range(7):
            paste(a, small_field, 882 + (column - 3) * 43, 431 + row * 38.5, small_mask)
    save(a, "T_HUD_Tripo_Magier_Inventar")


# ------------------------------------------------------------------------------------------------------------- Formeln
def formeln():
    a = load("2_Formeln.png")
    rows = [195 + 66 * i for i in range(7)]
    for y in rows:
        # a third circle for the elements
        copy(a, (297, y), (347, y), 24, 22, feather=3.0)
    # one square on the right for the result: the left one of the pairs goes
    for i in (0, 4, 5):
        copy(a, (1020, rows[i]), (1080, rows[i]), 22, 22, feather=3.0)
    # the lit quick slot put out
    copy(a, (1009, 121), (1082, 121), 30, 31, feather=3.0)
    save(cut_white(a), "T_HUD_Tripo_Magier_Formeln")


# ------------------------------------------------------------------------------------------------------------- Menue
def menue():
    a = load("4_Menue.png")
    copy(a, (727, 159), (638.5, 159), 44, 40, feather=3.0)
    # the bar emptied: its unlit right part laid over the lit one
    empty = a[BAR_Y0:BAR_Y1, 800:880].copy()
    strip = np.concatenate([empty] * 4, axis=1)[:, :800 - BAR_X0]
    m = box_mask(strip.shape[0], strip.shape[1], 2.0)
    m[:, -3:] = 1.0
    region = a[BAR_Y0:BAR_Y1, BAR_X0:800]
    region[..., :3] = region[..., :3] * (1 - m[..., None]) + strip[..., :3] * m[..., None]
    save(a, "T_HUD_Tripo_Magier_Menue")


# ------------------------------------------------------------------------------------------------------------- HUD
# where the HUD picture is cut out of the painted sheet (pixels of 1_HUD.png); Source/Vael/UI/VaelPlayerHUDWidget.cpp counts from here
HUD_X0, HUD_Y0, HUD_X1, HUD_Y1 = 168, 200, 1184, 548


def hud():
    a = cut_white(load("1_HUD.png"))
    ys, xs = np.nonzero(a[..., 3] > 0.02)
    print("HUD BBOX", xs.min(), ys.min(), xs.max(), ys.max())
    save(a[HUD_Y0:HUD_Y1, HUD_X0:HUD_X1], "T_HUD_Tripo_Magier_HUD")


def sheet_px():
    """the HUD picture as cut by hud(), with its alpha"""
    return cut_white(load("1_HUD.png"))[HUD_Y0:HUD_Y1, HUD_X0:HUD_X1]


# ------------------------------------------------------------------------------------------------------------- Drachenkopf
def drachenkopf():
    """Boris 18.11.: the dragon head on top of the painted menu frame, cut out to crown the HUD (silver on dark blue: the alpha
    from how light and how little blue a pixel is; below the head's chin the frame's edge line is left out)"""
    a = load("4_Menue.png")[14:76, 588:748].copy()
    rgb = a[..., :3]
    lum = rgb @ np.array([0.3, 0.59, 0.11], np.float32)
    blue = rgb[..., 2] - rgb[..., 0]
    alpha = np.clip((lum - 0.2 - blue * 1.2) / 0.16, 0, 1)
    yy, xx = np.mgrid[0:a.shape[0], 0:a.shape[1]]
    alpha[(yy > 50) & (np.abs(xx - 80) > 20)] = 0.0
    alpha[yy > 56] = 0.0
    alpha = blur(alpha, 1) * 1.1
    a[..., 3] = np.clip(alpha, 0, 1)
    # darker toward its edges like the silver of the frame, three times the size for a smooth edge
    big = resample(a, a.shape[1] * 3, a.shape[0] * 3)
    # the head is painted small: sharpened after the enlargement
    big[..., :3] = np.clip(big[..., :3] + 0.9 * (big[..., :3] - blur(big[..., :3], 2)), 0, 1)
    big[..., 3] = np.clip(big[..., 3] + 0.6 * (big[..., 3] - blur(big[..., 3], 2)), 0, 1)
    save(big, "T_HUD_Tripo_Magier_Drachenkopf")


# ------------------------------------------------------------------------------------------------------------- Nid-Adern
# Places on the HUD in pixels of 1_HUD.png (the same as in VaelPlayerHUDWidget.cpp, PaintMageHud)
HUD_ORBS = [(270.0, 390.0, 70.0), (1080.0, 390.0, 70.0)]
HUD_SEAL = (677.0, 311.0, 50.0)
HUD_ROUND = [(375.0, 472.0, 12.0), (975.0, 472.0, 12.0), (748.0, 313.0, 13.0), (881.0, 252.0, 15.0), (488.0, 250.0, 17.0), (413.0, 315.0, 17.0),
             (955.0, 312.0, 17.0), (455.0, 493.0, 19.0), (885.0, 493.0, 19.0)]
HUD_SQUARES = [418.5, 503.5, 591.5, 676.5, 762.5, 846.5, 933.5]
HUD_SQUARE_Y, HUD_SQUARE_HALF = 414.0, 38.0
HUD_BOXES = [(835.0, 320.0, 955.0, 350.0), (428.0, 322.0, 505.0, 348.0)]
NID_SQUARE = 4      # R1, the fifth square


def adern():
    """The mask of the painted HUD for the frame material (R the frame, G its gems) and its Nid veins (Tools/HUD/vael_hud_formen.py,
    vein_net): from the Nid square R1 in all directions over the dark leather, round the orbs, the seal and the squares, never over
    them; thin wisps curl at their ends (Boris 18.11.: "die Ader noch ein bisschen schöner")"""
    sys.path.insert(0, "C:/Projekte/Vael/Tools/HUD")
    import vael_hud_formen as formen

    picture = sheet_px()
    ph, pw = picture.shape[:2]
    alpha = picture[..., 3]
    rgb = picture[..., :3]
    mx, mn = rgb.max(-1), rgb.min(-1)
    sat = (mx - mn) / np.maximum(mx, 1e-4)
    yy, xx = np.mgrid[0:ph, 0:pw]
    far_from_orbs = np.ones((ph, pw), bool)
    for cx, cy, r in HUD_ORBS:
        far_from_orbs &= np.hypot(xx + HUD_X0 - cx, yy + HUD_Y0 - cy) > r
    gems = (sat > 0.45) & (rgb[..., 2] > rgb[..., 0] + 0.15) & (mx > 0.45) & far_from_orbs
    mask = np.zeros((ph, pw, 4), np.float32)
    mask[..., 0] = alpha
    mask[..., 1] = blur(dilate(gems, 1).astype(np.float32), 1)
    mask[..., 3] = 1.0
    save(mask, "T_HUD_Tripo_Magier_Maske")

    # vein_net counts in heights of the texture: a pixel of the sheet is 1/ph of it
    def unit(x, y):
        return (x - HUD_X0) / ph, (y - HUD_Y0) / ph

    def box_sdf(x, y, x0, y0, x1, y1):
        dx = np.maximum(np.maximum(x0 - x, x - x1), 0.0)
        dy = np.maximum(np.maximum(y0 - y, y - y1), 0.0)
        inside = np.minimum(np.maximum(x0 - x, x - x1), 0.0) + np.minimum(np.maximum(y0 - y, y - y1), 0.0)
        return np.hypot(dx, dy) + np.minimum(inside, 0.0)

    def symbols(x, y):
        """distance to everything painted that the veins go round, in heights of the texture, inside negative"""
        sx, sy = x * ph + HUD_X0, y * ph + HUD_Y0
        d = np.full(np.shape(sx), 1e3, np.float32) if np.ndim(sx) else 1e3
        for cx, cy, r in HUD_ORBS + [HUD_SEAL] + HUD_ROUND:
            d = np.minimum(d, np.hypot(sx - cx, sy - cy) - r)
        for cx in HUD_SQUARES:
            d = np.minimum(d, box_sdf(sx, sy, cx - HUD_SQUARE_HALF, HUD_SQUARE_Y - HUD_SQUARE_HALF, cx + HUD_SQUARE_HALF, HUD_SQUARE_Y + HUD_SQUARE_HALF))
        for x0, y0, x1, y1 in HUD_BOXES:
            d = np.minimum(d, box_sdf(sx, sy, x0, y0, x1, y1))
        return d / ph

    soft_alpha = blur(alpha, 2)

    def keep(x, y):
        return soft_alpha[np.clip((y * ph).astype(int), 0, ph - 1), np.clip((x * ph).astype(int), 0, pw - 1)]

    # the texture a bit finer than the picture
    w, h = int(pw * 1.5), int(ph * 1.5)
    scale = 202.0 / 301.0             # the old veins were made for a frame 202 design pixels high, this one is 301
    nid = unit(HUD_SQUARES[NID_SQUARE], HUD_SQUARE_Y)
    # fewer, stronger trunks than on the old frame: they branch on their way and thin out, no tangle (Boris 18.11.: schöner)
    trunks = [(math.pi, 0.042), (0.0, 0.04), (math.pi + 0.6, 0.034), (-0.6, 0.034), (-math.pi / 2 - 0.25, 0.03), (math.pi / 2 + 0.3, 0.028),
              (math.pi - 0.5, 0.026), (0.45, 0.026)]
    trunks = [(a, r * scale) for a, r in trunks]

    def from_nid(px, py, late=0.8):
        start = late * min(math.hypot(px - nid[0], py - nid[1]) / 2.4, 1.0)
        return lambda s: min(start + s / 2.4, 1.0)

    def hug(sense, off=0.02):
        def steer(px, py, heading):
            e = 0.002
            d = symbols(px, py)
            normal = math.atan2(symbols(px, py + e) - symbols(px, py - e), symbols(px + e, py) - symbols(px - e, py))
            return normal + sense * (math.pi / 2 + float(np.clip((d - off) * 25.0, -0.7, 0.7)))
        return steer

    # Out of the top and the bottom of R1 (its neighbours stand too close at the sides), then along the leather bands above and
    # below the row of squares, gently waving; their branches are free
    def along_band(px, py, heading):
        return (math.pi if math.cos(heading) < 0 else 0.0) + 0.25 * math.sin(px * 7.0)

    top, bottom = unit(HUD_SQUARES[NID_SQUARE], HUD_SQUARE_Y - HUD_SQUARE_HALF - 2), unit(HUD_SQUARES[NID_SQUARE], HUD_SQUARE_Y + HUD_SQUARE_HALF + 2)
    up = [(-math.pi / 2 - 0.8, 0.04 * scale), (-math.pi / 2 + 0.8, 0.038 * scale)]
    down = [(math.pi / 2 + 0.8, 0.036 * scale), (math.pi / 2 - 0.8, 0.034 * scale)]
    sources = [(top[0], top[1], up, lambda s: min(s / 2.4, 1.0), along_band), (bottom[0], bottom[1], down, lambda s: min(s / 2.4, 1.0), along_band),
               (top[0], top[1], [(-math.pi / 2, 0.026 * scale), (-math.pi / 2 - 0.3, 0.02 * scale)], lambda s: min(s / 1.6, 1.0))]
    rng = np.random.default_rng(29)
    # round the orbs and the seal the veins wind like the claws hold them, each its own small vessel
    for cx, cy, r in HUD_ORBS + [HUD_SEAL]:
        wanted, found = (2 if r > 60 else 0), 0
        for phi in rng.permutation(np.linspace(0, 2 * math.pi, 28, endpoint=False)):
            px, py = unit(cx + math.cos(phi) * (r + 5), cy + math.sin(phi) * (r + 5))
            if found == wanted or not (0.02 < px < pw / ph - 0.02 and 0.02 < py < 0.98) or keep(np.array(px), np.array(py)) < 0.5:
                continue
            found += 1
            sense = rng.choice([-1, 1])
            sources.append((px, py, [(phi + sense * math.pi / 2, rng.uniform(0.012, 0.019) * scale)], from_nid(px, py), hug(sense, rng.uniform(0.006, 0.03))))
    formen.vein_net(TARGET + "/T_HUD_Tripo_Magier_Adern.png", w, h, 37, sources, keep=keep, avoid=symbols, margin=0.02, curls=False, pull=0.22)
    print("SAVED", TARGET + "/T_HUD_Tripo_Magier_Adern.png")


only = args[2:] if len(args) > 2 else ["inventar", "formeln", "menue", "hud", "drachenkopf", "adern"]
for job in only:
    globals()[job]()
