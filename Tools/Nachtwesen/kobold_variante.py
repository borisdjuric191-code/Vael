"""Kobold variants (Boris 2026-11-22: "über Blender Kobolde leicht abwandeln, sodass eine größere Vielfalt vorhanden ist"): a
variant is a Kobold body bent into another build (head, trunk, arms, legs) with its texture recoloured (clothes and crystals in
another hue, the skin a little lighter or darker). It becomes a creature of its own, <Basis>_B, with the whole pipeline after it:

    blender -b --factory-startup -P Tools/Nachtwesen/kobold_variante.py -- <Variante>
    blender ... kobold_skelett.py -- <Variante>      (the joints are the bent joints of the base)
    blender ... kobold_clips.py, kobold_lebend_clips.py, 04_export.py -- <Variante>; 05_unreal_import.py <Variante>

Writes Vael_Quellen/<Variante>/arbeit/01_vorbereitet.blend and 02_tripo/<Variante>.fbm with the textures.
"""
import colorsys
import math
import glob
import os
import shutil
import sys

import numpy as np

# basis: the Kobold it comes from; kopf: head size; rumpf: (depth, width) of the trunk; arme: arm length; beine: leg length;
# farbton: hue shift of strong colours (0..1 turns once round); saett, hell: saturation and lightness of everything
VARIANTEN = {
    "Schleimkratzer_B": dict(basis="Schleimkratzer", kopf=1.14, rumpf=(0.92, 0.88), arme=1.1, beine=0.94, farbton=0.08, saett=0.85, hell=0.9),
    "Hortwaechter_B": dict(basis="Hortwaechter", kopf=0.92, rumpf=(1.1, 1.12), arme=0.96, beine=0.9, farbton=-0.06, saett=1.1, hell=0.82),
    "Kontoraeltester_B": dict(basis="Kontoraeltester", kopf=1.08, rumpf=(0.95, 0.92), arme=1.06, beine=1.0, farbton=0.45, saett=1.0, hell=0.95),
    "Sklaventreiber_B": dict(basis="Sklaventreiber", kopf=1.06, rumpf=(1.06, 1.1), arme=0.97, beine=0.92, farbton=0.1, saett=0.9, hell=1.08),
    "Sprengmeister_B": dict(basis="Sprengmeister", kopf=1.1, rumpf=(0.94, 0.92), arme=1.05, beine=1.06, farbton=-0.12, saett=1.05, hell=0.88),
}

# The Kobold set of 2026-11-23 (Boris: more variety for scenes with many Kobolde, colour nuances, sizes, small details,
# without shaking the design; clean hands): K_<Basis>_<0..4>, 0 is the plain look. Every one gets the new hand rig and
# its gear (kobold_ausruestung.py); ohren stretches the ears, haut tints the skin (the dull colours), ladungen recolours
# the Sprengmeister's charge plugs in the ore colours.
KOBOLDE = ("Schleimkratzer", "Hortwaechter", "Kontoraeltester", "Sklaventreiber", "Sprengmeister", "Laternentraeger")
STILE = (
    dict(kopf=1.0, rumpf=(1.0, 1.0), arme=1.0, beine=1.0, farbton=0.0, saett=1.0, hell=1.0, haut=(1.0, 1.0, 1.0), ohren=1.0),
    dict(kopf=1.06, rumpf=(0.95, 0.93), arme=1.06, beine=0.96, farbton=0.07, saett=0.9, hell=0.9, haut=(0.9, 1.0, 0.88), ohren=1.2),
    dict(kopf=0.95, rumpf=(1.08, 1.1), arme=0.96, beine=0.93, farbton=-0.09, saett=1.1, hell=0.86, haut=(1.06, 0.96, 0.9), ohren=0.86),
    dict(kopf=1.09, rumpf=(0.93, 0.9), arme=1.07, beine=1.04, farbton=0.28, saett=0.95, hell=1.0, haut=(0.94, 0.98, 1.07), ohren=1.1),
    dict(kopf=1.0, rumpf=(1.04, 1.05), arme=1.0, beine=0.97, farbton=0.18, saett=0.8, hell=1.07, haut=(1.0, 1.05, 0.93), ohren=1.3),
)
for _b in KOBOLDE:
    for _i, _s in enumerate(STILE):
        _v = dict(_s, basis=_b, nummer=_i)
        if _b == "Laternentraeger":
            # its lantern bones are found by the shape of the plain body: only colours change
            _v.update(kopf=1.0, rumpf=(1.0, 1.0), arme=1.0, beine=1.0, ohren=1.0)
        VARIANTEN["K_%s_%d" % (_b, _i)] = _v


def weich(a, b, x):
    t = np.clip((x - a) / max(b - a, 1e-6), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def ohren(P, g, f):
    """Ears longer or shorter: head points far out to the side move further out (and a little up)"""
    if abs(f - 1.0) < 1e-3:
        return P
    im_kopf = P[:, 2] > g["hals"][2]
    weit = np.abs(P[:, 1])
    if not im_kopf.any():
        return P
    ab = 0.55 * weit[im_kopf].max()
    raus = np.clip(weit - ab, 0.0, None) * im_kopf
    P = P.copy()
    P[:, 1] += np.sign(P[:, 1]) * raus * (f - 1.0)
    P[:, 2] += raus * (f - 1.0) * 0.35
    return P


def biegen(P, g, v, mit_ohren=False):
    """Bends points (N x 3, the body looks along +X, +Y left, metres, feet at 0) by the variant v around the joints g of the base"""
    P = np.array(P, dtype=float)
    if mit_ohren:
        P = ohren(P, g, v.get("ohren", 1.0))
    hals, kopf = np.array(g["hals"]), np.array(g["kopf"])
    becken = np.array(g["becken"])
    hz, halsz = g["huefte"][2], hals[2]
    # head: grows or shrinks around the top of the neck
    w = weich(halsz - 0.02, kopf[2], P[:, 2])[:, None]
    P = P + w * ((kopf + (P - kopf) * v["kopf"]) - P)
    # trunk: deeper/wider between hips and neck, fading out at both ends
    w = (weich(hz - 0.05, hz + 0.08, P[:, 2]) * (1 - weich(halsz - 0.06, halsz + 0.02, P[:, 2])))
    P[:, 0] = P[:, 0] + w * ((becken[0] + (P[:, 0] - becken[0]) * v["rumpf"][0]) - P[:, 0])
    P[:, 1] = P[:, 1] + w * (P[:, 1] * v["rumpf"][1] - P[:, 1])
    # arms: longer or shorter from the shoulder, only what stands out from the trunk
    b = g["rumpfbreite"]
    for s in (1.0, -1.0):
        sch = np.array(g["schulter"]) * np.array([1.0, s, 1.0])
        w = (weich(b - 0.03, b + 0.05, P[:, 1] * s) * weich(hz, hz + 0.1, P[:, 2]))[:, None]
        P = P + w * ((sch + (P - sch) * v["arme"]) - P)
    # legs: everything below the hips stretches, everything above moves with it
    z = P[:, 2]
    P[:, 2] = np.where(z < hz, z * v["beine"], z - (1 - v["beine"]) * hz)
    return P


def gelenke(name, basis_gelenke):
    """The joints of a variant: those of its base, bent the same way"""
    v = VARIANTEN[name]
    g = basis_gelenke[v["basis"]]
    keys = [k for k in g if isinstance(g[k], tuple) and len(g[k]) == 3]
    P = biegen([g[k] for k in keys], g, v)
    neu = dict(g)
    for k, p in zip(keys, P):
        neu[k] = tuple(float(c) for c in p)
    neu["rumpfbreite"] = g["rumpfbreite"] * v["rumpf"][1]
    if v["basis"] != "Laternentraeger" or not name.startswith("K_"):
        neu.pop("extra", None)
    return neu


# Where on the body violet crystal patches take the ore colours (face middles, plain body, metres)
UMFAERBEN = {
    "Sprengmeister": lambda m: 0.86 < m[2] < 1.42 and abs(m[1]) < 0.34,
    "Kontoraeltester": lambda m: 0.64 < m[2] < 0.87 and abs(m[1]) < 0.27,
}


FLAECHEN = {}      # face -> its points, for the faces uv_maske looked at
MITTEN = {}        # face -> its middle (m)
STEIN = 0.05       # m: about the size of one gem or charge plug


def uv_maske(obj, groesse, auswahl):
    """Per texture pixel the 3D cell (ZELLE) of the face covering it, for faces whose middle passes auswahl(point); -1 elsewhere
    (UV triangles rasterised)"""
    breite, hoehe = groesse
    maske = -np.ones((hoehe, breite), dtype=np.int64)
    me = obj.data
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    for poly in me.polygons:
        m = co[list(poly.vertices)].mean(axis=0)
        if not auswahl(m):
            continue
        marke = poly.index
        FLAECHEN[poly.index] = list(poly.vertices)
        MITTEN[poly.index] = m
        ecken = uv[poly.loop_start:poly.loop_start + poly.loop_total] * (breite, hoehe)
        for k in range(1, len(ecken) - 1):
            a, b, c = ecken[0], ecken[k], ecken[k + 1]
            x0, y0 = np.floor(np.minimum(np.minimum(a, b), c)).astype(int) - 1
            x1, y1 = np.ceil(np.maximum(np.maximum(a, b), c)).astype(int) + 1
            x0, y0 = max(x0, 0), max(y0, 0)
            x1, y1 = min(x1, breite - 1), min(y1, hoehe - 1)
            if x1 < x0 or y1 < y0:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
            if abs(d) < 1e-9:
                continue
            l1 = ((b[1] - c[1]) * (gx - c[0]) + (c[0] - b[0]) * (gy - c[1])) / d
            l2 = ((c[1] - a[1]) * (gx - c[0]) + (a[0] - c[0]) * (gy - c[1])) / d
            drin = (l1 >= -0.02) & (l2 >= -0.02) & (1 - l1 - l2 >= -0.02)
            maske[y0:y1 + 1, x0:x1 + 1][drin] = marke
    return maske.ravel()


def ladungen_faerben(rgb, alt, marken, groesse, nummer):
    """Sprengmeister: the violet plugs of the charges on its chest belts take the colours of all eleven ores, mixed;
    Kontoraeltester: the gems of his collar. Every plug or gem (one 3D cell, uv_maske) one colour; only bright, strong violet
    (crystal), not the dull violet of cloth"""
    import kobold_ausruestung as ka
    a = alt[:, :3]
    mx, mn = a.max(axis=1), a.min(axis=1)
    sat = np.where(mx > 1e-4, (mx - mn) / np.maximum(mx, 1e-4), 0.0)
    violett = (marken >= 0) & (sat > 0.3) & (mx > 0.2) & (a[:, 2] > a[:, 1] * 1.25) & (a[:, 0] > a[:, 1] * 1.1)
    # faces that are mostly crystal; touching ones form one gem / plug (union over shared points)
    alle = np.bincount(marken[marken >= 0])
    lila = np.bincount(marken[violett], minlength=len(alle))
    steine = [f for f in np.nonzero(lila)[0] if lila[f] >= 0.25 * alle[f]]
    eltern = {}

    def wurzel(x):
        while eltern.setdefault(x, x) != x:
            eltern[x] = eltern.setdefault(eltern[x], eltern[x])
            x = eltern[x]
        return x
    punkt_zu = {}
    for f in steine:
        for v in FLAECHEN[int(f)]:
            if v in punkt_zu:
                ra, rb = wurzel(int(f)), wurzel(punkt_zu[v])
                if ra != rb:
                    eltern[ra] = rb
            else:
                punkt_zu[v] = int(f)
    gruppe = {int(f): wurzel(int(f)) for f in steine}
    # gems that touch each other came out as one group: split big groups in space, about one part per gem size (k-means on the
    # face middles, seeds spread by farthest point)
    neu_gruppe = {}
    for g in set(gruppe.values()):
        fl = [f for f, h in gruppe.items() if h == g]
        M = np.array([MITTEN[f] for f in fl])
        weit = float((M.max(axis=0) - M.min(axis=0)).max()) if len(M) else 0.0
        k = max(1, int(math.ceil(weit / STEIN)))
        if k == 1 or len(fl) < 2 * k:
            for f in fl:
                neu_gruppe[f] = g
            continue
        mitten = [M[0]]
        for _ in range(k - 1):
            d = np.min([np.linalg.norm(M - c, axis=1) for c in mitten], axis=0)
            mitten.append(M[int(np.argmax(d))])
        mitten = np.array(mitten)
        for _ in range(12):
            zu = np.argmin(np.linalg.norm(M[:, None, :] - mitten[None, :, :], axis=2), axis=1)
            for j in range(k):
                if (zu == j).any():
                    mitten[j] = M[zu == j].mean(axis=0)
        for f, j in zip(fl, zu):
            neu_gruppe[f] = g * 100 + int(j)
    gruppe = neu_gruppe
    gruppen = sorted(set(gruppe.values()))
    namen = list(ka.ERZE)
    rng = np.random.default_rng(77 + nummer)
    reihe = rng.permutation(np.arange(len(gruppen)) % len(namen))
    farbe_g = {g: np.array(ka.ERZE[namen[i]], dtype=np.float32) for g, i in zip(gruppen, reihe)}
    farbe_von = {f: farbe_g[g] for f, g in gruppe.items()}
    violett &= np.isin(marken, np.array(steine, dtype=np.int64))
    zellen = gruppen
    rgb = rgb.copy()
    idx = np.nonzero(violett)[0]
    hell = a[idx].max(axis=1, keepdims=True)
    farben = np.stack([farbe_von[int(z)] for z in marken[idx]]) if len(idx) else np.zeros((0, 3), np.float32)
    rgb[idx] = np.clip(farben * (0.25 + 1.1 * hell), 0.0, 1.0)
    print("Ladungen: %d Steine/Pfropfen umgefaerbt (%d Pixel)" % (len(zellen), len(idx)))
    return rgb


def umfaerben(quelle, ziel, v, ladungen=None):
    """Recolours the base colour texture: strong colours (clothes, crystals) turn their hue, the skin only gets lighter or darker"""
    import bpy
    img = bpy.data.images.load(quelle)
    px = np.array(img.pixels[:], dtype=np.float32).reshape(-1, 4)
    px_alt = px.copy()
    rgb = px[:, :3]
    mx, mn = rgb.max(axis=1), rgb.min(axis=1)
    sat = np.where(mx > 1e-4, (mx - mn) / np.maximum(mx, 1e-4), 0.0)
    # hue turn as a rotation in RGB around the grey axis (fast, no per-pixel colorsys)
    ang = v["farbton"] * 2 * np.pi
    c, s = np.cos(ang), np.sin(ang)
    k = 1.0 / 3.0
    sq = np.sqrt(k)
    m = np.array([[c + (1 - c) * k, k * (1 - c) - sq * s, k * (1 - c) + sq * s],
                  [k * (1 - c) + sq * s, c + k * (1 - c), k * (1 - c) - sq * s],
                  [k * (1 - c) - sq * s, k * (1 - c) + sq * s, c + k * (1 - c)]], dtype=np.float32)
    gedreht = np.clip(rgb @ m.T, 0.0, 1.0)
    w = np.clip((sat - 0.28) / 0.25, 0.0, 1.0)[:, None]          # only strong colours turn
    rgb = rgb * (1 - w) + gedreht * w
    grau = rgb.mean(axis=1, keepdims=True)
    rgb = np.clip((grau + (rgb - grau) * v["saett"]) * v["hell"], 0.0, 1.0)
    # the skin and the dull colours get a tint of their own
    haut = np.array(v.get("haut", (1.0, 1.0, 1.0)), dtype=np.float32)
    rgb = np.clip(rgb * w + rgb * haut * (1 - w), 0.0, 1.0)
    if ladungen is not None:
        rgb = ladungen_faerben(rgb, px_alt, ladungen.ravel(), img.size, v.get("nummer", 0))
    px[:, :3] = rgb
    img.pixels[:] = px.ravel()
    img.filepath_raw = ziel
    img.file_format = "JPEG" if ziel.lower().endswith(".jpg") else "PNG"
    img.save()


def main():
    import bpy
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    import gemein
    import kobold_skelett as kg
    name = sys.argv[sys.argv.index("--") + 1]
    v = VARIANTEN[name]
    basis = v["basis"]
    g = kg.GELENKE[basis]
    obj, _ = gemein.oeffnen(basis, "01_vorbereitet.blend")
    co = np.empty(len(obj.data.vertices) * 3)
    obj.data.vertices.foreach_get("co", co)
    # Sprengmeister: where its charge belts are in the texture (faces on the chest, read on the plain body)
    # Kontoraeltester: the gems of his collar (Boris: "Juwelen an seinem Hals umbauen") the same way, every ore once
    ladungen = None
    if basis in UMFAERBEN and name.startswith("K_"):
        farbe = next(d for d in glob.glob(gemein.ordner(basis, "02_tripo", "*.fbm", "*")) if "basecolor" in d.lower())
        groesse = tuple(bpy.data.images.load(farbe).size)
        ladungen = uv_maske(obj, groesse, UMFAERBEN[basis])
    neu = biegen(co.reshape(-1, 3), g, v, mit_ohren=name.startswith("K_"))
    obj.data.vertices.foreach_set("co", neu.ravel())
    obj.data.update()
    obj.name = name
    obj.data.name = name
    if name.startswith("K_"):
        import kobold_ausruestung as ka
        ka.anbauen(obj, basis, gelenke(name, kg.GELENKE), saat=v["nummer"])
    gemein.speichern(name, "01_vorbereitet.blend")
    # textures: the base colour recoloured, the others as they are
    ziel = gemein.ordner(name, "02_tripo", name + ".fbm")
    os.makedirs(ziel, exist_ok=True)
    for datei in glob.glob(gemein.ordner(basis, "02_tripo", "*.fbm", "*")):
        klein = os.path.basename(datei).lower()
        art = next((a for a in ("basecolor", "normal", "roughness", "metallic") if a in klein), None)
        if not art:
            continue
        nach = os.path.join(ziel, "%s_%s%s" % (name, art, os.path.splitext(klein)[1]))
        if art == "basecolor":
            umfaerben(datei, nach, v, ladungen)
        else:
            shutil.copyfile(datei, nach)
    hoehe = max(neu[:, 2])
    gemein.log(name, "aus %s gebogen, %.2f m hoch, Textur umgefärbt" % (basis, hoehe))


if __name__ == "__main__":
    main()
