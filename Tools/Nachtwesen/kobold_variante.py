"""Kobold variants (Boris 2026-11-22: "über Blender Kobolde leicht abwandeln, sodass eine größere Vielfalt vorhanden ist"): a
variant is a Kobold body bent into another build (head, trunk, arms, legs) with its texture recoloured (clothes and crystals in
another hue, the skin a little lighter or darker). It becomes a creature of its own, <Basis>_B, with the whole pipeline after it:

    blender -b --factory-startup -P Tools/Nachtwesen/kobold_variante.py -- <Variante>
    blender ... kobold_skelett.py -- <Variante>      (the joints are the bent joints of the base)
    blender ... kobold_clips.py, kobold_lebend_clips.py, 04_export.py -- <Variante>; 05_unreal_import.py <Variante>

Writes Vael_Quellen/<Variante>/arbeit/01_vorbereitet.blend and 02_tripo/<Variante>.fbm with the textures.
"""
import colorsys
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


def weich(a, b, x):
    t = np.clip((x - a) / max(b - a, 1e-6), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def biegen(P, g, v):
    """Bends points (N x 3, the body looks along +X, +Y left, metres, feet at 0) by the variant v around the joints g of the base"""
    P = np.array(P, dtype=float)
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
    neu.pop("extra", None)
    return neu


def umfaerben(quelle, ziel, v):
    """Recolours the base colour texture: strong colours (clothes, crystals) turn their hue, the skin only gets lighter or darker"""
    import bpy
    img = bpy.data.images.load(quelle)
    px = np.array(img.pixels[:], dtype=np.float32).reshape(-1, 4)
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
    neu = biegen(co.reshape(-1, 3), g, v)
    obj.data.vertices.foreach_set("co", neu.ravel())
    obj.data.update()
    obj.name = name
    obj.data.name = name
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
            umfaerben(datei, nach, v)
        else:
            shutil.copyfile(datei, nach)
    hoehe = max(neu[:, 2])
    gemein.log(name, "aus %s gebogen, %.2f m hoch, Textur umgefärbt" % (basis, hoehe))


if __name__ == "__main__":
    main()
