"""Vael Wurzelforst-Generator.
Aufruf:  python wf_generator.py   (braucht: pip install numpy scipy matplotlib pillow)
Liest WF_Silhouette.json und schreibt in denselben Ordner: WF_Hoehenkarte_2017.png,
T_WF_Zonenkarte.png, WF_Platzhalter.json, WF_Vorschau.png.
Feste Orte (Urstamm, Eihoehle, Nid-Quelle ...) stehen weiter unten im Abschnitt Platzhalter.

Erzeugt Hoehenkarte, Zonentextur und Platzhalterliste fuer den Wurzelforst-Blockout.
Kartenkoordinaten (px) = Koordinaten des 2D-Entwurfs (1600x1100). 1 px = 2 m.
"""
import json, math, os
import numpy as np
from matplotlib.path import Path
from scipy import ndimage
from PIL import Image, ImageDraw

OUT = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(OUT, "WF_Silhouette.json"), encoding="utf-8") as _f:
    SIL = json.load(_f)

N = 2017            # Landscape-Vertices pro Seite
OX, OY = 200, 150   # Versatz der Karte im Gitter
M_PER_PX = 2.0
CM = 100.0 * M_PER_PX
rng = np.random.default_rng(7)

yy, xx = np.mgrid[0:N, 0:N].astype(np.float32)
mx, my = xx - OX, yy - OY  # Kartenkoordinaten je Gitterpunkt

def bez_path(segs, start):
    verts, codes = [start], [Path.MOVETO]
    for s in segs:
        if len(s) == 6:
            verts += [(s[0], s[1]), (s[2], s[3]), (s[4], s[5])]; codes += [Path.CURVE4] * 3
        else:
            verts.append((s[0], s[1])); codes.append(Path.LINETO)
    verts.append(start); codes.append(Path.CLOSEPOLY)
    return Path(verts, codes)

land_path = bez_path(SIL["kueste_wurzelforst"]["stuecke"], tuple(SIL["kueste_wurzelforst"]["start"]))
_ap = [tuple(p) for p in SIL["anschluss_aschenmark"]]
asche_path = Path(_ap + [_ap[0]])
jaw_path = bez_path(SIL["kiefer_sturmzinnen"]["stuecke"], tuple(SIL["kiefer_sturmzinnen"]["start"]))

pts = np.stack([mx.ravel(), my.ravel()], 1)
land = land_path.contains_points(pts).reshape(N, N)
land |= asche_path.contains_points(pts).reshape(N, N)
jaw = jaw_path.contains_points(pts).reshape(N, N)
land |= jaw
land = ndimage.gaussian_filter(land.astype(np.float32), 8) > 0.5
jaw = jaw & land

def ell(cx, cy, rx, ry):
    return np.sqrt(((mx - cx) / rx) ** 2 + ((my - cy) / ry) ** 2)

def noise(sigma, seed):
    r = np.random.default_rng(seed).standard_normal((N, N)).astype(np.float32)
    r = ndimage.gaussian_filter(r, sigma)
    return r / (np.abs(r).max() + 1e-6)

n_small, n_mid, n_large = noise(4, 1), noise(14, 2), noise(40, 3)
fbm = 0.5 * n_small + 0.3 * n_mid + 0.2 * n_large

d_in = ndimage.distance_transform_edt(land).astype(np.float32)
d_out = ndimage.distance_transform_edt(~land).astype(np.float32)

# ---------- Zonen (hart) ----------
Z = np.full((N, N), -1, np.int8)
ZN = ["saum", "moor", "hain", "kamm", "grat", "asche", "jaw"]
Z[land & (my >= 850)] = 0
Z[land & (my >= 590) & (my < 850)] = 1
Z[land & (my >= 340) & (my < 590)] = 2
Z[land & (ell(310, 480, 230, 140) < 1)] = 2
Z[land & (my < 340) & (Z != 2)] = 3
Z[land & (mx >= 560) & (my < 340)] = 4
Z[land & (my >= 1040)] = 5
Z[jaw] = 6

# ---------- Zielhoehen je Zone (m) ----------
base = 1.5 + np.clip(d_in / 40, 0, 1) * 2.5
dist_ur = np.hypot(mx - 290, my - 470)
T = {}
T["saum"] = base + 1.0 + 0.8 * fbm
T["moor"] = 0.5 + 0.35 * fbm
T["hain"] = base + 4 + 7 * np.clip(1 - dist_ur / 70, 0, 1) ** 1.5 + 0.8 * fbm
T["kamm"] = base + 8 + 30 * np.clip((380 - my) / 250, 0, 1) + 5 * fbm
gx = np.array([p[0] for p in SIL["gratlinie"]], float); gy = np.array([p[1] for p in SIL["gratlinie"]], float)
ypath = np.interp(mx, gx, gy)
dy = np.abs(my - ypath)
peaks = 110 * np.clip((dy - 12) / 45, 0, 1) ** 1.2 * (0.6 + 0.4 * (n_large * 0.5 + 0.5))
taper = np.clip((1240 - mx) / 180, 0, 1)
T["grat"] = 6 + taper * (39 + peaks + 3 * fbm)
T["asche"] = base + 2 + 0.8 * fbm
T["jaw"] = 25 + 18 * (n_mid * 0.5 + 0.5)

W = np.zeros((len(ZN), N, N), np.float32)
for i in range(len(ZN)):
    W[i] = ndimage.gaussian_filter((Z == i).astype(np.float32), 18)
Wsum = W.sum(0) + 1e-6
H_land = sum(W[i] * T[ZN[i]] for i in range(len(ZN))) / Wsum

# Lagunen im versunkenen Wald
lagoons = [tuple(l) for l in SIL["lagunen_pfuhlmoor"]]
lag_w = np.zeros((N, N), np.float32)
for (cx, cy, rx, ry) in lagoons:
    lag_w = np.maximum(lag_w, np.clip((1 - ell(cx, cy, rx, ry)) / 0.2, 0, 1))
H_land = H_land * (1 - lag_w) + (-0.6) * lag_w

# Kuestenrampe (am Grat steil = Klippen)
grat_w = W[4] / Wsum
ramp_w = np.clip(d_in / (10 * (1 - grat_w) + 2.5 * grat_w), 0, 1)
ramp_w = ramp_w * ramp_w * (3 - 2 * ramp_w)
H_land = 0.3 + (H_land - 0.3) * ramp_w

# ---------- Meer ----------
shallow = d_out < 35
for _b in SIL["flachwasser_buchten"]:
    shallow = shallow | (ell(*_b) < 1)
H_sea = np.where(shallow, -0.6, -0.6 - np.clip((d_out - 35) / 60, 0, 1) * 17.4)
H_sea = ndimage.gaussian_filter(H_sea, 4)
faenge_w = np.clip((1 - ell(*SIL["watt_die_faenge"])) / 0.3, 0, 1)
H_sea = H_sea * (1 - faenge_w) + (-1.2) * faenge_w

H = np.where(land, H_land, H_sea).astype(np.float32)

# ---------- Kampagnenweg ----------
route = [tuple(p) for p in SIL["kampagnenweg"]]
img = Image.new("L", (N, N), 0)
ImageDraw.Draw(img).line([(x + OX, y + OY) for x, y in route], fill=255, width=1)
dp = ndimage.distance_transform_edt(np.array(img) == 0).astype(np.float32)
H_smooth = ndimage.gaussian_filter(H, 12)
path_w = np.clip((14 - dp) / 6, 0, 1)
H = H * (1 - path_w) + np.maximum(H_smooth, 0.4) * path_w

H = ndimage.gaussian_filter(H, 1.2)

# ---------- Export Hoehenkarte ----------
hv = np.clip(32768 + H * 128.0, 0, 65535).astype(np.uint16)
Image.fromarray(hv).save(os.path.join(OUT, "WF_Hoehenkarte_2017.png"))
print("Hoehe min/max (m):", float(H.min()), float(H.max()))

# ---------- Zonentextur ----------
def c(h):
    h = h.lstrip("#"); return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], np.float32)
ZC = {"saum": c("6e6448"), "moor": c("3e6b55"), "hain": c("3f7f45"), "kamm": c("3b6670"),
      "grat": c("6a6d60"), "asche": c("7a4a2a"), "jaw": c("4a5f78")}
col = sum(W[i][..., None] * ZC[ZN[i]] for i in range(len(ZN))) / Wsum[..., None]
hl = np.clip((H - 40) / 100, 0, 1)[..., None]
col = col * (1 - hl * grat_w[..., None]) + c("9a9c90") * hl * grat_w[..., None]
lagc = c("2f6f74")
col = col * (1 - lag_w[..., None]) + lagc * lag_w[..., None]
depth = np.clip(-H / 18, 0, 1)[..., None]
sea_col = c("2f6f74") * (1 - depth) + c("0e2733") * depth
col = np.where(land[..., None], col, sea_col)
nid_w = np.clip(1 - np.hypot(mx - 392, my - 726) / 45, 0, 1)[..., None] * 0.6
col = col * (1 - nid_w) + c("5a3a7a") * nid_w
pw = np.clip((5 - dp) / 3, 0, 1)[..., None]
col = col * (1 - pw) + c("b9a77a") * pw
# Hillshade fuer Lesbarkeit
gy_, gx_ = np.gradient(H)
shade = np.clip(1 - (gx_ * 0.35 + gy_ * 0.35) * 0.5, 0.65, 1.25)[..., None]
col_tex = np.clip(col * shade, 0, 255).astype(np.uint8)
Image.fromarray(col_tex, "RGB").save(os.path.join(OUT, "T_WF_Zonenkarte.png"))
Image.fromarray(col_tex, "RGB").crop((OX - 60, OY - 60, OX + 1660, OY + 1180)).resize((860, 620)).save(
    os.path.join(OUT, "WF_Vorschau.png"))

# ---------- Platzhalter ----------
def g(px, py):
    i, j = int(round(py + OY)), int(round(px + OX))
    i, j = min(max(i, 0), N - 1), min(max(j, 0), N - 1)
    return float(H[i, j])
def isl(px, py):
    return bool(land[int(round(py + OY)), int(round(px + OX))])
def dpath(px, py):
    return float(dp[int(round(py + OY)), int(round(px + OX))])
def zone(px, py):
    return int(Z[int(round(py + OY)), int(round(px + OX))])
def in_lagoon(px, py, pad=1.0):
    return any(((px - cx) / rx) ** 2 + ((py - cy) / ry) ** 2 < pad for cx, cy, rx, ry in lagoons)
def W3(px, py, zm):
    return [(px + OX) * CM, (py + OY) * CM, zm * 100.0]

P = []   # Primitive
L = []   # Lichter
TX = []  # Beschriftungen
def prim(mesh, px, py, zm, s, mat, name, folder, rot=(0, 0, 0), tags=()):
    P.append({"m": mesh, "l": W3(px, py, zm), "r": [round(v, 2) for v in rot], "s": [round(v, 2) for v in s],
              "mat": mat, "n": name, "f": folder, "t": list(tags)})
def light(px, py, zm, rgb, inten, radius_m, name, folder):
    L.append({"l": W3(px, py, zm), "c": rgb, "i": inten, "r": radius_m * 100, "n": name, "f": folder})
def text(px, py, zm, t, size=1500, rgb=(236, 229, 210)):
    TX.append({"l": W3(px, py, zm), "t": t, "s": size, "c": rgb})

def far_enough(px, py, placed, mind):
    return all((px - a) ** 2 + (py - b) ** 2 >= mind * mind for a, b in placed)

def scatter(n, cond, mind, tries=6000, seed=0, box=(0, 0, 1600, 1100), placed=None):
    r = np.random.default_rng(seed); out = [] if placed is None else list(placed); new = []
    for _ in range(tries):
        if len(new) >= n: break
        px, py = r.uniform(box[0], box[2]), r.uniform(box[1], box[3])
        if cond(px, py) and far_enough(px, py, out, mind):
            out.append((px, py)); new.append((px, py))
    return new

F_HAIN, F_MOOR, F_SAUM, F_KAMM, F_GRAT, F_FAENGE, F_WEG = (
    "Wurzelforst/3_Ewiger_Hain", "Wurzelforst/2_Pfuhlmoor", "Wurzelforst/1_Forstsaum", "Wurzelforst/4_Kammweg",
    "Wurzelforst/U_Schwanzgrat", "Wurzelforst/Die_Faenge", "Wurzelforst/Kampagnenweg")

# ===== 3 Ewiger Hain =====
ux, uy = 290, 470; gu = g(ux, uy)
prim("Cylinder", ux, uy, gu + 41, (32, 32, 90), "Urstamm", "Urstamm_Stamm", F_HAIN + "/Urstamm", tags=["TRIPO_1"])
prim("Sphere", ux, uy, gu + 100, (130, 130, 60), "Krone", "Urstamm_Krone", F_HAIN + "/Kronen", tags=["KRONE"])
for i, (ox, oy) in enumerate([(-22, -10), (20, 14), (6, -24)]):
    prim("Sphere", ux + ox, uy + oy, gu + 88, (70, 70, 35), "KroneHell", f"Urstamm_Krone_{i+1}", F_HAIN + "/Kronen", tags=["KRONE"])
roots = [(190, 430), (200, 540), (300, 590), (400, 560), (420, 440), (360, 370), (250, 360), (180, 490), (340, 610), (225, 395)]
for i, (ex, ey) in enumerate(roots):
    ddx, ddy = ex - ux, ey - uy; dist = math.hypot(ddx, ddy)
    mxp, myp = ux + ddx * 0.55, uy + ddy * 0.55
    length = dist * 2 * 0.9
    yaw = math.degrees(math.atan2(ddy, ddx))
    prim("Cube", mxp, myp, g(mxp, myp) + 1.5, (length, 8, 5), "Wurzel", f"Urstamm_Wurzel_{i+1:02d}", F_HAIN + "/Urstamm",
         rot=(-2.5, yaw, 0), tags=["TRIPO_1"])
light(ux, uy, gu + 30, (0.75, 1.0, 0.6), 8000, 120, "Urstamm_Licht", F_HAIN + "/Urstamm")

# Eihoehle
ex_, ey_ = 225, 540; ge = g(ex_, ey_)
ang_open = math.atan2(uy - ey_, ux - ex_)
for k in range(8):
    a = ang_open + math.radians(25 + k * (310 / 7))
    px, py = ex_ + math.cos(a) * 8, ey_ + math.sin(a) * 8
    prim("Cube", px, py, ge + 5, (10, 7, 13), "FelsMoos", f"Eihoehle_Fels_{k+1}", F_HAIN + "/Eihoehle",
         rot=(0, math.degrees(a) + 90, 6 * (-1) ** k))
prim("Cylinder", ex_, ey_, ge + 0.1, (14, 14, 0.2), "WASSER", "Eihoehle_Teich", F_HAIN + "/Eihoehle")
prim("Cylinder", ex_ + 2, ey_ - 1, ge + 4.5, (1.6, 1.6, 9), "Rinde", "Eihoehle_RosaBaum_Stamm", F_HAIN + "/Eihoehle", tags=["TRIPO_REGION2_EXTRA"])
prim("Sphere", ex_ + 2, ey_ - 1, ge + 11, (11, 11, 7), "Rosa", "Eihoehle_RosaBaum_Krone", F_HAIN + "/Eihoehle", tags=["TRIPO_REGION2_EXTRA"])
prim("Cylinder", ex_, ey_, ge + 35, (8, 8, 60), "Lichtschacht", "Eihoehle_Lichtschacht", F_HAIN + "/Eihoehle")
light(ex_ + 2, ey_ - 1, ge + 8, (1.0, 0.55, 0.75), 3000, 30, "Eihoehle_Licht", F_HAIN + "/Eihoehle")

# Feenkreis
fx, fy = 400, 430; gf = g(fx, fy)
prim("Cylinder", fx, fy, gf + 0.15, (14, 14, 0.3), "Feenlicht", "Feenkreis_Boden", F_HAIN + "/Feenkreis")
for k in range(9):
    a = k * 2 * math.pi / 9
    px, py = fx + math.cos(a) * 4.5, fy + math.sin(a) * 4.5
    prim("Cylinder", px, py, gf + 0.8, (0.6, 0.6, 1.6), "Pilzstiel", f"Feenkreis_Pilz_{k+1}_Stiel", F_HAIN + "/Feenkreis")
    prim("Sphere", px, py, gf + 1.7, (2.4, 2.4, 1.0), "Feenlicht", f"Feenkreis_Pilz_{k+1}_Hut", F_HAIN + "/Feenkreis")
light(fx, fy, gf + 4, (0.4, 1.0, 0.9), 4000, 25, "Feenkreis_Licht", F_HAIN + "/Feenkreis")

# Feenhoehlen
for i, (hx, hy) in enumerate([(200, 445), (230, 395), (365, 545), (180, 520), (340, 395)]):
    gh = g(hx, hy); a = math.atan2(hy - uy, hx - ux)
    prim("Sphere", hx, hy, gh + 1, (16, 16, 10), "FelsMoos", f"Feenhoehle_{i+1}_Huegel", F_HAIN + "/Feenhoehlen")
    prim("Cylinder", hx + math.cos(a) * 3.5, hy + math.sin(a) * 3.5, gh + 2, (3.5, 3.5, 2.5), "Dunkel",
         f"Feenhoehle_{i+1}_Eingang", F_HAIN + "/Feenhoehlen", rot=(90, math.degrees(a), 0))
    light(hx + math.cos(a) * 4.5, hy + math.sin(a) * 4.5, gh + 2, (1.0, 0.9, 0.5), 1500, 12, f"Feenhoehle_{i+1}_Licht", F_HAIN + "/Feenhoehlen")

# Riesenbaeume im Hain
def hain_ok(px, py):
    return (zone(px, py) == 2 and isl(px, py) and math.hypot(px - ux, py - uy) > 72 and math.hypot(px - ex_, py - ey_) > 26
            and math.hypot(px - fx, py - fy) > 18 and dpath(px, py) > 9 and not in_lagoon(px, py, 1.3))
hain_trees = scatter(24, hain_ok, 26, seed=11, box=(120, 330, 520, 640))
for i, (px, py) in enumerate(hain_trees):
    gg = g(px, py); r = np.random.default_rng(100 + i)
    dia, h, cd = r.uniform(8, 14), r.uniform(40, 60), r.uniform(28, 44)
    prim("Cylinder", px, py, gg + h / 2 - 2, (dia, dia, h), "Rinde", f"Riesenbaum_{i+1:02d}_Stamm", F_HAIN + "/Riesenbaeume", tags=["TRIPO_2"])
    prim("Sphere", px, py, gg + h + cd * 0.2, (cd, cd, cd * 0.55), "Krone", f"Riesenbaum_{i+1:02d}_Krone", F_HAIN + "/Kronen", tags=["KRONE"])
    # Brettwurzeln
    for k in range(3):
        a = r.uniform(0, 2 * math.pi)
        prim("Cube", px + math.cos(a) * dia / 4, py + math.sin(a) * dia / 4, gg + 1.5, (dia * 1.4, 2, 4), "Wurzel",
             f"Riesenbaum_{i+1:02d}_Brettwurzel_{k+1}", F_HAIN + "/Riesenbaeume", rot=(-8, math.degrees(a), 0), tags=["TRIPO_2"])
# Haengestege
pairs, used = [], set()
for i, a in enumerate(hain_trees):
    best = None
    for j, b in enumerate(hain_trees):
        if j <= i: continue
        d = math.hypot(a[0] - b[0], a[1] - b[1])
        if 28 < d < 55 and (best is None or d < best[0]): best = (d, j)
    if best and len(pairs) < 7 and i not in used and best[1] not in used:
        pairs.append((i, best[1])); used |= {i, best[1]}
for k, (i, j) in enumerate(pairs):
    a, b = hain_trees[i], hain_trees[j]
    mxp, myp = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    z = min(g(*a), g(*b)) + 18
    prim("Cube", mxp, myp, z, (math.hypot(b[0] - a[0], b[1] - a[1]) * 2 - 10, 2.5, 0.5), "Holz", f"Haengesteg_{k+1}",
         F_HAIN + "/Haengestege", rot=(0, math.degrees(math.atan2(b[1] - a[1], b[0] - a[0])), 0))
# Wurzel-Kit verstreut
def kit_ok(px, py):
    return zone(px, py) == 2 and isl(px, py) and dpath(px, py) > 7 and math.hypot(px - ux, py - uy) > 50 and not in_lagoon(px, py, 1.2)
for i, (px, py) in enumerate(scatter(34, kit_ok, 12, seed=21, box=(120, 330, 520, 640), placed=hain_trees)):
    r = np.random.default_rng(300 + i)
    prim("Cube", px, py, g(px, py) + 0.6, (r.uniform(18, 36), r.uniform(3, 5), r.uniform(2, 3.5)), "Wurzel",
         f"Wurzelbogen_{i+1:02d}", F_HAIN + "/Wurzel_Kit", rot=(r.uniform(-6, 6), r.uniform(0, 180), r.uniform(-8, 8)), tags=["TRIPO_5"])
# Lichtnymphen
for i, (px, py) in enumerate([(520, 560), (505, 455), (585, 640)]):
    prim("Cylinder", px, py, 0.6, (0.8, 0.8, 1.2), "Nymphe", f"Lichtnymphe_{i+1}", F_HAIN + "/Nymphen")
    prim("Sphere", px, py, 1.5, (0.8, 0.8, 0.8), "Nymphe", f"Lichtnymphe_{i+1}_Kopf", F_HAIN + "/Nymphen")

# ===== 2 Pfuhlmoor =====
mangrove_pts = [(200, 650), (215, 640), (320, 680), (300, 695), (290, 725), (470, 720), (455, 740), (280, 790),
                (180, 770), (400, 800), (510, 760), (500, 840), (380, 640), (250, 610), (440, 860)]
def moor_ok(px, py):
    return 590 <= py <= 860 and 140 <= px <= 620 and dpath(px, py) > 6 and (isl(px, py) or g(px, py) > -1.0)
mangroves = [p for p in mangrove_pts if dpath(*p) > 5] + scatter(42, moor_ok, 14, seed=31, box=(140, 590, 620, 870), placed=mangrove_pts)
for i, (px, py) in enumerate(mangroves):
    gg = g(px, py); r = np.random.default_rng(500 + i)
    h = r.uniform(8, 12)
    prim("Cylinder", px, py, gg + 3 + h / 2, (1.6, 1.6, h), "Mangrove", f"Mangrove_{i+1:02d}_Stamm", F_MOOR + "/Mangroven", tags=["TRIPO_4"])
    prim("Sphere", px, py, gg + 3 + h + 1.5, (r.uniform(10, 15),) * 2 + (6,), "MangroveKrone", f"Mangrove_{i+1:02d}_Krone", F_MOOR + "/Kronen", tags=["KRONE"])
    for k in range(3):
        a = r.uniform(0, 2 * math.pi) if k == 0 else a + 2.1
        prim("Cube", px + math.cos(a) * 1.0, py + math.sin(a) * 1.0, gg + 1.8, (0.6, 0.6, 5.5), "Mangrove",
             f"Mangrove_{i+1:02d}_Stelze_{k+1}", F_MOOR + "/Mangroven", rot=(0, math.degrees(a), 35), tags=["TRIPO_4"])
# Moos-Huetten auf Stelzen
for i, (px, py) in enumerate([(230, 690), (335, 770), (450, 690), (195, 740)]):
    gg = max(g(px, py), 0)
    prim("Sphere", px, py, gg + 5.5, (9, 9, 6), "Moos", f"Moos_Huette_{i+1}", F_MOOR + "/Huetten")
    for k, (ox, oy) in enumerate([(-1.5, -1.5), (1.5, -1.5), (-1.5, 1.5), (1.5, 1.5)]):
        prim("Cylinder", px + ox, py + oy, gg + 1.5, (0.5, 0.5, 4), "Holz", f"Moos_Huette_{i+1}_Stelze_{k+1}", F_MOOR + "/Huetten")
    light(px, py + 2.5, gg + 5, (1.0, 0.75, 0.4), 1200, 15, f"Moos_Huette_{i+1}_Licht", F_MOOR + "/Huetten")
# Stammbruecken ueber Lagunen
for i, (px, py, yaw) in enumerate([(330, 815, 0), (225, 775, 30), (260, 660, -15), (430, 640, 60)]):
    prim("Cylinder", px, py, 0.4, (2.2, 2.2, 26), "Totholz", f"Stammbruecke_{i+1}", F_MOOR + "/Stammbruecken", rot=(90, yaw, 0))
# Seerosen
r = np.random.default_rng(41)
k = 0
for (cx, cy, rx, ry) in lagoons:
    for _ in range(9):
        a, rr = r.uniform(0, 2 * math.pi), math.sqrt(r.uniform(0.1, 0.8))
        px, py = cx + math.cos(a) * rx * rr, cy + math.sin(a) * ry * rr
        if math.hypot(px - 392, py - 726) < 18: continue
        k += 1; d = r.uniform(2, 4.5)
        prim("Cylinder", px, py, 0.03, (d, d, 0.1), "Seerose", f"Seerose_{k:02d}", F_MOOR + "/Seerosen", tags=["PFLANZE"])
        if k % 3 == 0:
            prim("Sphere", px, py, 0.4, (0.9, 0.9, 0.7), "Rosa", f"Seerose_{k:02d}_Bluete", F_MOOR + "/Seerosen", tags=["PFLANZE"])
# Nid-Quelle + Adern
nx, ny = 392, 726
prim("Cylinder", nx, ny, 0.05, (12, 12, 0.25), "Nid", "NidQuelle_Becken", F_MOOR + "/NidQuelle")
prim("Sphere", nx, ny, 1.2, (3, 3, 3), "Nid", "NidQuelle_Kern", F_MOOR + "/NidQuelle")
light(nx, ny, 3, (0.6, 0.25, 1.0), 9000, 45, "NidQuelle_Licht", F_MOOR + "/NidQuelle")
def bez(p0, p1, p2, p3, n):
    out = []
    for i in range(n + 1):
        t = i / n
        x = (1 - t) ** 3 * p0[0] + 3 * (1 - t) ** 2 * t * p1[0] + 3 * (1 - t) * t * t * p2[0] + t ** 3 * p3[0]
        y = (1 - t) ** 3 * p0[1] + 3 * (1 - t) ** 2 * t * p1[1] + 3 * (1 - t) * t * t * p2[1] + t ** 3 * p3[1]
        out.append((x, y))
    return out
vk = 0
for curve in [bez((392, 726), (430, 750), (470, 760), (520, 790), 10), bez((392, 726), (360, 760), (340, 790), (320, 815), 10)]:
    for a, b in zip(curve[:-1], curve[1:]):
        vk += 1
        prim("Cube", (a[0] + b[0]) / 2, (a[1] + b[1]) / 2, max(g(*a), 0) + 0.06, (math.hypot(b[0] - a[0], b[1] - a[1]) * 2 + 1, 0.8, 0.2),
             "Nid", f"NidAder_{vk:02d}", F_MOOR + "/NidQuelle", rot=(0, math.degrees(math.atan2(b[1] - a[1], b[0] - a[0])), 0))
# Quellwaechter (Riesenschlange um toten Baum)
qx, qy = 362, 700; gq = max(g(qx, qy), 0)
prim("Cylinder", qx, qy, gq + 14, (5, 5, 28), "Totholz", "Quellwaechter_Baum", F_MOOR + "/Quellwaechter")
for i in range(16):
    a = math.radians(i * 48); rad = 3.4
    prim("Sphere", qx + math.cos(a) * rad / 2, qy + math.sin(a) * rad / 2, gq + 1 + i * 1.5, (4.6 - i * 0.16,) * 3,
         "Waechter", f"Quellwaechter_Segment_{i+1:02d}", F_MOOR + "/Quellwaechter", tags=["KREATUR_BOSS"])
# Schlicknixen
for i, (px, py) in enumerate([(430, 745), (560, 860), (250, 770)]):
    prim("Cylinder", px, py, 0.6, (0.8, 0.8, 1.2), "Nixe", f"Schlicknixe_{i+1}", F_MOOR + "/Schlicknixen", tags=["KREATUR"])
    prim("Sphere", px, py, 1.5, (0.8, 0.8, 0.8), "Nixe", f"Schlicknixe_{i+1}_Kopf", F_MOOR + "/Schlicknixen", tags=["KREATUR"])

# ===== 1 Forstsaum =====
def saum_ok(px, py):
    return zone(px, py) in (0, 5) and 850 <= py <= 1070 and isl(px, py) and dpath(px, py) > 7
for i, (px, py) in enumerate(scatter(30, saum_ok, 15, seed=51, box=(150, 850, 640, 1070))):
    gg = g(px, py); r = np.random.default_rng(700 + i)
    dead = py > 960 or r.random() < 0.35
    dia, h = r.uniform(2, 4), r.uniform(14, 24)
    prim("Cylinder", px, py, gg + h / 2 - 1, (dia, dia, h), "Totholz" if dead else "Rinde", f"Saumbaum_{i+1:02d}_Stamm", F_SAUM + "/Baeume", tags=["TRIPO_2"])
    if not dead:
        cd = r.uniform(9, 14)
        prim("Sphere", px, py, gg + h + 2, (cd, cd, cd * 0.6), "KroneHell", f"Saumbaum_{i+1:02d}_Krone", F_SAUM + "/Kronen", tags=["KRONE"])
for i, (px, py) in enumerate(scatter(12, saum_ok, 25, seed=52, box=(150, 850, 640, 1000))):
    d = np.random.default_rng(800 + i).uniform(8, 14)
    prim("Cylinder", px, py, g(px, py) + 0.1, (d, d, 0.25), "Moos", f"Farnfeld_{i+1:02d}", F_SAUM + "/Farnfelder", tags=["PFLANZE"])
for i, (px, py) in enumerate([(300, 900), (470, 880)]):
    gg = g(px, py)
    prim("Cylinder", px, py, gg + 0.6, (0.6, 0.6, 1.2), "Moos", f"Blattkobold_Spaeher_{i+1}", F_SAUM + "/Hueter_Spaeher", tags=["KREATUR"])
    prim("Sphere", px, py, gg + 1.4, (0.7, 0.7, 0.7), "Moos", f"Blattkobold_Spaeher_{i+1}_Kopf", F_SAUM + "/Hueter_Spaeher", tags=["KREATUR"])
sx, sy = 415, 1048; gs = g(sx, sy)
SPAWN = {"l": W3(sx, sy, gs + 1.0), "yaw": -90}
prim("Cylinder", sx + 4, sy, gs + 0.9, (0.6, 0.6, 1.8), "Spieler", "Spieler_Massstab_1m80", F_SAUM + "/Start")

# ===== 4 Kammweg =====
def kamm_ok(px, py):
    return zone(px, py) == 3 and isl(px, py) and dpath(px, py) > 8 and px < 560
shroom_pts = [p for p in [(280, 230), (250, 318), (450, 278), (330, 196), (490, 330), (400, 188)] if dpath(*p) > 7]
shrooms = shroom_pts + scatter(22, kamm_ok, 22, seed=61, box=(130, 120, 560, 380), placed=shroom_pts)
mats = ["Pilz_Tuerkis", "Pilz_Orange", "Pilz_Rot", "Pilz_Tuerkis"]
for i, (px, py) in enumerate(shrooms):
    gg = g(px, py); r = np.random.default_rng(900 + i)
    sd, h = r.uniform(3, 7), r.uniform(12, 35); cd = sd * r.uniform(3.5, 6)
    prim("Cylinder", px, py, gg + h / 2 - 1, (sd, sd, h), "Pilzstiel", f"Riesenpilz_{i+1:02d}_Stiel", F_KAMM + "/Riesenpilze", tags=["TRIPO_3"])
    prim("Sphere", px, py, gg + h, (cd, cd, cd * 0.3), mats[i % 4], f"Riesenpilz_{i+1:02d}_Hut", F_KAMM + "/Riesenpilze", tags=["TRIPO_3"])
    if mats[i % 4] == "Pilz_Tuerkis" and i % 2 == 0:
        light(px, py, gg + h - 3, (0.3, 0.9, 0.85), 2500, 30, f"Riesenpilz_{i+1:02d}_Leuchten", F_KAMM + "/Riesenpilze")
for i, (px, py) in enumerate([(260, 165), (480, 185), (200, 300), (525, 335)]):
    gg = max(g(px, py), 0) + 40
    prim("Cone", px, py, gg, (22, 22, 16), "FelsMoos", f"Pilzinsel_{i+1}_Fels", F_KAMM + "/Pilzinseln", rot=(180, 0, 0))
    prim("Cylinder", px, py, gg + 8.5, (22, 22, 1.5), "Moos", f"Pilzinsel_{i+1}_Wiese", F_KAMM + "/Pilzinseln")
    prim("Cylinder", px + 2, py, gg + 14, (2.5, 2.5, 10), "Pilzstiel", f"Pilzinsel_{i+1}_Pilzstiel", F_KAMM + "/Pilzinseln", tags=["TRIPO_3"])
    prim("Sphere", px + 2, py, gg + 19, (12, 12, 4), "Pilz_Tuerkis", f"Pilzinsel_{i+1}_Pilzhut", F_KAMM + "/Pilzinseln", tags=["TRIPO_3"])
ax_, ay_, bx_, by_ = 470, 250, 545, 238
za = max(g(ax_, ay_), g(bx_, by_), 1) + 2
prim("Cube", (ax_ + bx_) / 2, (ay_ + by_) / 2, za, (math.hypot(bx_ - ax_, by_ - ay_) * 2, 4, 0.6), "Holz", "Pilzbruecke",
     F_KAMM + "/Pilzbruecke", rot=(0, math.degrees(math.atan2(by_ - ay_, bx_ - ax_)), 0))
for k in range(3):
    t = (k + 1) / 4; px, py = ax_ + (bx_ - ax_) * t, ay_ + (by_ - ay_) * t - 1.4
    prim("Cylinder", px, py, za + 1.5, (0.4, 0.4, 3), "Pilzstiel", f"Pilzbruecke_Lampe_{k+1}", F_KAMM + "/Pilzbruecke")
    prim("Sphere", px, py, za + 3.2, (1.6, 1.6, 0.9), "Lampe", f"Pilzbruecke_Lampe_{k+1}_Hut", F_KAMM + "/Pilzbruecke")
    light(px, py, za + 3, (1.0, 0.75, 0.4), 1500, 15, f"Pilzbruecke_Licht_{k+1}", F_KAMM + "/Pilzbruecke")
hx, hy = 215, 262; gh = g(hx, hy)
prim("Sphere", hx, hy, gh + 2, (34, 34, 18), "FelsMoos", "Pilzhoehle_Huegel", F_KAMM + "/Pilzhoehle")
prim("Cylinder", hx + 8, hy + 2, gh + 3, (6, 6, 3), "Dunkel", "Pilzhoehle_Eingang", F_KAMM + "/Pilzhoehle", rot=(90, 10, 0))
light(hx + 10, hy + 2, gh + 3, (0.3, 0.95, 0.85), 3000, 25, "Pilzhoehle_Licht", F_KAMM + "/Pilzhoehle")
def rock_ok(px, py):
    return zone(px, py) == 3 and py < 235 and isl(px, py) and dpath(px, py) > 8
for i, (px, py) in enumerate(scatter(14, rock_ok, 14, seed=71, box=(150, 100, 560, 235))):
    r = np.random.default_rng(1000 + i); s = r.uniform(6, 14)
    prim("Cube", px, py, g(px, py) + s * 0.3, (s, s * r.uniform(0.6, 1), s * r.uniform(0.5, 0.9)), "Fels", f"Felsbrocken_{i+1:02d}",
         F_KAMM + "/Felsen", rot=(r.uniform(-15, 15), r.uniform(0, 90), r.uniform(-15, 15)))

# ===== Schwanzgrat =====
towers = []
r = np.random.default_rng(81)
for i, x in enumerate(np.arange(600, 1185, 27)):
    side = -1 if i % 2 == 0 else 1
    y = float(np.interp(x, gx, gy)) + side * r.uniform(24, 40)
    if isl(x, y) and d_in[int(y + OY), int(x + OX)] > 3:
        towers.append((float(x), y))
for i, (px, py) in enumerate(towers):
    gg = g(px, py); rr = np.random.default_rng(1100 + i)
    dia, h = rr.uniform(14, 28), rr.uniform(40, 90)
    prim("Cylinder", px, py, gg - 5 + h / 2, (dia, dia, h), "Fels", f"Felsturm_{i+1:02d}", F_GRAT + "/Felstuerme")
    prim("Sphere", px, py, gg - 5 + h + 1, (dia * 1.2, dia * 1.2, 7), "Moos", f"Felsturm_{i+1:02d}_Bewuchs", F_GRAT + "/Felstuerme")
# Lianentreppe an einem Turm nahe dem Weg
tx_, ty_ = min(towers, key=lambda p: abs(p[0] - 705))
gt = g(tx_, ty_)
for k in range(10):
    a = math.radians(k * 36)
    prim("Cube", tx_ + math.cos(a) * 9, ty_ + math.sin(a) * 9, gt + 1 + k * 2.4, (6, 3, 0.6), "Holz", f"Lianentreppe_Stufe_{k+1:02d}",
         F_GRAT + "/Lianentreppe", rot=(0, math.degrees(a) + 90, 0))
# Wasserfaelle in den Ringsee
for i, (px, py) in enumerate([(745, 222), (752, 224), (985, 212), (992, 214)]):
    top = max(g(px, py - 6), 5)
    prim("Cube", px, py + 8, top / 2, (2.5, 2, top), "Wasserfall", f"Wasserfall_{i+1}", F_GRAT + "/Wasserfaelle")
# Schwebende Klippe
prim("Cone", 870, 315, 45, (45, 45, 35), "FelsMoos", "Schwebende_Klippe_Fels", F_GRAT + "/Schwebende_Klippe", rot=(180, 0, 0))
prim("Cylinder", 870, 315, 63.5, (45, 45, 3), "Moos", "Schwebende_Klippe_Wiese", F_GRAT + "/Schwebende_Klippe")
prim("Cube", 870, 326, 31, (2.5, 2, 62), "Wasserfall", "Schwebende_Klippe_Wasserfall", F_GRAT + "/Schwebende_Klippe")
# Drachensicht
dx_, dy_ = 1010, 152; gd = g(dx_, dy_)
prim("Cylinder", dx_, dy_, gd + 0.75, (14, 14, 1.5), "Fels", "Drachensicht_Plattform", F_GRAT + "/Drachensicht")
for k, ox in enumerate([-2.5, 2.5]):
    prim("Cube", dx_ + ox, dy_ - 3, gd + 5.5, (2, 2, 9), "Fels", f"Drachensicht_Saeule_{k+1}", F_GRAT + "/Drachensicht")
prim("Cube", dx_, dy_ - 3, gd + 10.5, (13, 2.4, 2), "Fels", "Drachensicht_Bogen", F_GRAT + "/Drachensicht")
light(dx_, dy_, gd + 3, (1.0, 0.85, 0.5), 2000, 20, "Drachensicht_Licht", F_GRAT + "/Drachensicht")
# Schwanzspitze + Anleger
for k, (px, py) in enumerate([(1228, 196), (1238, 214), (1222, 220)]):
    s = 8 + k * 2
    prim("Cube", px, py, g(px, py) + 2, (s, s * 0.8, s * 0.7), "Fels", f"Schwanzspitze_Fels_{k+1}", F_GRAT + "/Schwanzspitze", rot=(10, 30 * k, 5))
prim("Cube", 1248, 228, 0.6, (14, 5, 0.8), "Holz", "Faehranleger_Schwanz", F_GRAT + "/Schwanzspitze", rot=(0, 35, 0))

# ===== Die Faenge =====
for i, (px, py, bs, h) in enumerate([(1270, 210, 16, 38), (1294, 226, 18, 30), (1314, 208, 20, 45), (1331, 236, 15, 32), (1303, 258, 14, 26)]):
    prim("Cone", px, py, -1.2 + h / 2 - 3, (bs, bs, h), "Zahn", f"Fang_{i+1}", F_FAENGE + "/Zaehne", rot=(0, 0, 4 * (-1) ** i))
tide = [(1240, 212), (1270, 226), (1294, 236), (1314, 222), (1331, 244), (1360, 238), (1400, 245)]
sk = 0
for a, b in zip(tide[:-1], tide[1:]):
    n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / 6))
    for j in range(n):
        t = j / n; sk += 1
        px, py = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
        prim("Cube", px, py, -0.6, (5, 5, 0.8), "Fels", f"Ebbe_Trittstein_{sk:02d}", F_FAENGE + "/Ebbe_Pfad", rot=(0, sk * 23, 0))
prim("Cube", 1392, 262, 0.6, (14, 5, 0.8), "Holz", "Faehranleger_Kiefer", F_FAENGE + "/Faehre", rot=(0, -30, 0))
prim("Cube", 1310, 296, 0.5, (9, 3.2, 1.2), "Holz", "Faehre_Boot", F_FAENGE + "/Faehre", rot=(0, 20, 0))
prim("Cylinder", 1312, 296, 1.9, (0.8, 0.8, 1.4), "Nymphe", "Faehrnymphe", F_FAENGE + "/Faehre")
prim("Sphere", 1312, 296, 3.0, (0.8, 0.8, 0.8), "Nymphe", "Faehrnymphe_Kopf", F_FAENGE + "/Faehre")

# ===== Kampagnenweg (Streifen) =====
wk = 0
for a, b in zip(route[:-1], route[1:]):
    seg = math.hypot(b[0] - a[0], b[1] - a[1]); n = max(1, int(seg / 11)); yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
    for j in range(n):
        t = (j + 0.5) / n; wk += 1
        px, py = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
        prim("Cube", px, py, g(px, py) + 0.12, (seg * 2 / n + 1, 6, 0.25), "Weg", f"Weg_{wk:03d}", F_WEG, rot=(0, yaw, 0))

# ===== Beschriftungen =====
text(380, 960, g(380, 960) + 35, "1 Forstsaum")
text(330, 690, 35, "2 Pfuhlmoor")
text(ux, uy, gu + 175, "3 Ewiger Hain", 2500)
text(370, 260, g(370, 260) + 60, "4 Kammweg")
text(850, 150, g(850, 150) + 90, "Schwanzgrat")
text(1305, 230, 60, "Die Faenge")
text(1460, 240, 70, "-> III Sturmzinnen")
text(392, 726, 22, "Nid-Quelle", 900, (200, 150, 255))
text(225, 540, ge + 75, "Eihoehle", 900, (255, 190, 220))
text(1010, 152, gd + 25, "Drachensicht", 900)

data = {"N": N, "OX": OX, "OY": OY, "cm_per_px": CM, "prims": P, "lights": L, "texts": TX, "spawn": SPAWN,
        "center": [N / 2 * CM, N / 2 * CM]}
with open(os.path.join(OUT, "WF_Platzhalter.json"), "w", encoding="utf-8") as f:
    json.dump(data, f, separators=(",", ":"))
print("Primitive:", len(P), "Lichter:", len(L), "Texte:", len(TX))
from collections import Counter
print(Counter(t for p in P for t in p["t"]))
