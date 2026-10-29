"""Step 4: every vine gets a pitcher mouth (Boris 2026-10-29: all vines on the back bite). The bud of a mouthed vine
(MAUL_VORLAGE) is copied onto the bare vines: their thin hooked tip is cut off and the bud put on in its place, along
the vine, scaled to its thickness, with its slit standing so the jaws open up and down.

Every vertex keeps three integer layers for the skeleton (step 5):
  ranke   vine index (ranken.json), -1 = body
  maul    vine index of the mouth it belongs to, -1 = none
  kiefer  +1 upper jaw, -1 lower jaw, 0 the bud's base (stays on the stem)

Writes arbeit/04_maeuler.blend, arbeit/maeuler.json (per vine: the bud's base, axis, slit normal and length, the
cut-off vine line) and vorschau/04_*.png.

    blender -b --factory-startup -P Tools/Kelchreisser/04_maeuler.py
"""
import json
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import vorschau  # noqa: E402

MAUL_VORLAGE = 3      # vine whose bud is copied (the one on the right flank, fully free of the body)
SPITZE_AB_M = 0.7     # m cut off the bare vines' tips (the thin hook that curls up)
UEBERLAPPUNG_M = 0.08 # the copied stem reaches this far back over the cut, so no hole shows
MAUL_GROESSE = (0.85, 1.0)  # the copy's scale against the template, by the vine's thickness, kept in these bounds
GELENK_ANTEIL = 0.3   # share of the bud's length from its base before the jaws part (the hinge)
INNEN_ROT = 2.2       # the mouth's inside is deep red: red at least this many times green


def bogen(linie):
    return np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(linie, axis=0), axis=1))])


def punkt_bei(linie, s):
    lang = bogen(linie)
    return np.array([np.interp(s, lang, linie[:, i]) for i in range(3)])


def tangente_bei(linie, s, h=0.1):
    t = punkt_bei(linie, s + h) - punkt_bei(linie, s - h)
    return t / np.linalg.norm(t)


def quer(achse, oben):
    """A unit vector at right angles to the axis, as close to 'oben' as possible"""
    n = oben - achse * float(oben @ achse)
    if np.linalg.norm(n) < 1e-3:
        n = np.array([1.0, 0, 0]) - achse * achse[0]
    return n / np.linalg.norm(n)


def vorlage(P, F, ranke_v, rk):
    """Frame of the template bud: base on the vine line where the bud's sphere begins, axis to its middle, slit
    normal across the red inside (the jaws lie on either side of it)"""
    linie = np.array(rk["linie"])
    mitte = np.array(rk["maul_mitte"])
    maul = np.array(rk["maul"])
    weite = np.linalg.norm(P[maul] - mitte, axis=1).max()
    lang = bogen(linie)
    drin = np.where(np.linalg.norm(linie - mitte, axis=1) < weite)[0]
    s_basis = lang[drin[0]] if len(drin) else lang[-1]
    basis = punkt_bei(linie, s_basis)
    achse = (mitte - basis) / np.linalg.norm(mitte - basis)
    rot = maul[(F[maul, 0] > INNEN_ROT * F[maul, 1]) & (F[maul, 0] > 0.05)]
    d = P[rot] - basis
    d -= np.outer(d @ achse, achse)
    werte, vektoren = np.linalg.eigh(np.cov(d.T))
    spalt = quer(achse, vektoren[:, 0])
    if spalt[2] < 0:
        spalt = -spalt
    laenge = float(((P[maul] - basis) @ achse).max())
    radius = stielradius(rk, s_basis)
    print("[Kelchreisser] Vorlage: Basis %s, Achse %s, Spalt %s, Laenge %.2f m, Stielradius %.3f, %d rote Vertices" % (
        basis.round(2), achse.round(2), spalt.round(2), laenge, radius, len(rot)))
    return {"basis": basis, "achse": achse, "spalt": spalt, "laenge": laenge, "radius": radius, "maul": maul}


def stielradius(rk, s_ende):
    """Median thickness of the half metre of stem before s_ende (the rings right at a bud swell with it)"""
    lang = bogen(np.array(rk["linie"]))
    nimm = (lang > s_ende - 0.6) & (lang < s_ende - 0.1)
    return float(np.median(np.array(rk["radius"])[nimm])) if nimm.any() else float(np.median(rk["radius"]))


def kiefer_seite(punkte, basis, achse, spalt, laenge):
    """+1 / -1 for the two jaws, 0 for the base below the hinge"""
    t = (punkte - basis) @ achse
    seite = np.where((punkte - basis) @ spalt >= 0, 1, -1)
    return np.where(t > GELENK_ANTEIL * laenge, seite, 0)


def main():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(vorschau.ARBEIT, "01_vorbereitet.blend"))
    obj = bpy.data.objects["Kelchreisser"]
    roh = np.load(os.path.join(vorschau.ARBEIT, "analyse_roh.npz"))
    P, F = roh["punkte"], roh["farbe"]
    ranke_v = np.load(os.path.join(vorschau.ARBEIT, "ranken.npz"))["ranke"]
    with open(os.path.join(vorschau.ARBEIT, "ranken.json"), encoding="utf-8") as f:
        ranken = json.load(f)

    v = vorlage(P, F, ranke_v, ranken[MAUL_VORLAGE])
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    l_ranke = bm.verts.layers.int.new("ranke")
    l_maul = bm.verts.layers.int.new("maul")
    l_kiefer = bm.verts.layers.int.new("kiefer")
    for vert in bm.verts:
        vert[l_ranke] = int(ranke_v[vert.index])
        vert[l_maul] = -1
        vert[l_kiefer] = 0
    maeuler = {}
    # The buds already there: jaws from their own frame
    for i, rk in enumerate(ranken):
        if not rk["maul"]:
            continue
        eigen = v if i == MAUL_VORLAGE else vorlage(P, F, ranke_v, rk)
        seiten = kiefer_seite(P[eigen["maul"]], eigen["basis"], eigen["achse"], eigen["spalt"], eigen["laenge"])
        for idx, s in zip(eigen["maul"], seiten):
            bm.verts[int(idx)][l_maul] = i
            bm.verts[int(idx)][l_kiefer] = int(s)
            bm.verts[int(idx)][l_ranke] = i
        maeuler[i] = {k: (eigen[k].tolist() if hasattr(eigen[k], "tolist") else eigen[k])
                      for k in ("basis", "achse", "spalt", "laenge")}
        maeuler[i]["linie"] = rk["linie"]
        maeuler[i]["neu"] = False

    # Template faces: all their corners in the bud
    in_maul = np.zeros(len(P), bool)
    in_maul[v["maul"]] = True
    vorlage_flaechen = [f for f in bm.faces if all(in_maul[vert.index] for vert in f.verts)]
    vorlage_verts = list({vert for f in vorlage_flaechen for vert in f.verts})

    for i, rk in enumerate(ranken):
        if rk["maul"]:
            continue
        linie = np.array(rk["linie"])
        lang = bogen(linie)
        s_schnitt = lang[-1] - SPITZE_AB_M
        ziel = punkt_bei(linie, s_schnitt - UEBERLAPPUNG_M)
        achse = tangente_bei(linie, s_schnitt - UEBERLAPPUNG_M)
        spalt = quer(achse, np.array([0.0, 0.0, 1.0]))
        r_ziel = stielradius(rk, s_schnitt)
        k = min(max(r_ziel / v["radius"], MAUL_GROESSE[0]), MAUL_GROESSE[1])
        # Rotation from the template's frame (axis, slit, their cross) onto the vine's
        a = np.stack([v["achse"], v["spalt"], np.cross(v["achse"], v["spalt"])], axis=1)
        b = np.stack([achse, spalt, np.cross(achse, spalt)], axis=1)
        drehung = b @ a.T
        m = (Matrix.Translation(Vector(ziel)) @ Matrix([list(r) + [0] for r in drehung] + [[0, 0, 0, 1]])
             @ Matrix.Scale(k, 4) @ Matrix.Translation(Vector(-v["basis"])))

        # Cut the hook off: vertices of this vine nearest to the line beyond the cut
        weg = []
        for vert in bm.verts:
            if vert[l_ranke] != i or vert[l_maul] >= 0:
                continue
            j = int(np.argmin(np.linalg.norm(linie - np.array(vert.co), axis=1)))
            if lang[j] > s_schnitt:
                weg.append(vert)
        bmesh.ops.delete(bm, geom=weg, context="VERTS")

        # The map runs both ways (copy to template too): only the template's corners lead to the copies; the jaw
        # layer comes along with the copy
        abbild = bmesh.ops.duplicate(bm, geom=vorlage_flaechen)["vert_map"]
        neue_verts = [abbild[alt] for alt in vorlage_verts]
        for kopie in neue_verts:
            kopie[l_ranke] = i
            kopie[l_maul] = i
        bmesh.ops.transform(bm, matrix=m, verts=neue_verts)
        maeuler[i] = {"basis": ziel.tolist(), "achse": achse.tolist(), "spalt": spalt.tolist(),
                      "laenge": v["laenge"] * k, "linie": linie[lang <= s_schnitt - UEBERLAPPUNG_M].tolist(),
                      "neu": True, "groesse": k}
        print("[Kelchreisser] Ranke %d: Spitze ab %.2f m, Maul %.2fx bei %s, %d Vertices weg, %d neu" % (
            i, s_schnitt, k, ziel.round(2), len(weg), len(neue_verts)))
        bm.verts.index_update()
        bm.verts.ensure_lookup_table()

    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    with open(os.path.join(vorschau.ARBEIT, "maeuler.json"), "w", encoding="utf-8") as f:
        json.dump({str(k): w for k, w in maeuler.items()}, f)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(vorschau.ARBEIT, "04_maeuler.blend"))
    vorschau.standard_ansichten("04", (0.0, 0.0, 3.4), 7.4)
    vorschau.rendern("04_hinten", (-1, -0.3, 0.4), (0.0, 0.0, 3.4), 7.4)
    vorschau.rendern("04_vorn", (1, 0.2, 0.3), (0.3, 0.0, 4.6), 4.5)


main()
