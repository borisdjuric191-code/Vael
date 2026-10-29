"""Step 3 (outside Blender, numpy + scipy): finds the vines on the back in arbeit/analyse_roh.npz and writes

  arbeit/ranken.json   per vine: centre line from the root on the back to the tip (evenly spaced), radius along it,
                       whether it already ends in a pitcher mouth, and the mouth (bud) vertices of the mouthed vines
  arbeit/ranken.npz    ranke: vine index of every vertex (-1 = body)

How: the vines are dark green (the moss on the back too). Shortest paths through the green part of the mesh from
where green touches the back run out to the vine tips; tips whose path mostly follows another one are the same vine
(a bud or a curl). Rings of equal surface distance along a path give the tube's middle (see mittellinie).

    C:/Users/boris/Tools/wf_generator_venv/Scripts/python.exe -I Tools/Kelchreisser/03_ranken.py
"""
import json
import os

import numpy as np
import scipy.sparse as sp
from scipy.sparse.csgraph import dijkstra
from scipy.spatial import cKDTree

ARBEIT = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))),
                      "Vael_Quellen", "Kelchreisser", "arbeit")
# The back where vines grow from (x from tail to neck, |y|, z band; m after step 1)
RUECKEN = ((-2.2, 1.3), 1.3, (3.3, 4.6))
MIN_LAENGE = 0.8      # shorter green runs are moss tufts or stubs
SCHRITT = 0.05        # spacing of the centre line
SCHEIBE = 0.04        # width of a ring: band of surface distance
MAUL_RADIUS = 0.75    # around a mouthed tip: coral/bronze vertices this close belong to the bud
MAUL_WEITE = 1.35     # the bud reaches this much further than most of its coral (sepals, teeth, thorn tips)
MIN_MAUL = 500        # coral vertices a bud has at least (fewer: a neighbour's bud reaching close)
WURZEL_MAX = 0.5      # m the tube is followed on below the green
# The short thick vine on the left flank lies along the back near its root, where the rings and the slabs both
# wander off over the skin: its root read off a plot of its vertices (tip, root; m)
WURZEL_VON_HAND = [((-1.1, 1.17, 4.79), (-0.45, 0.72, 3.95))]
MAX_RADIUS = 0.2      # a fatter "vine" is a moss hump or a cluster of back spikes


def gruen(F):
    r, g, b = F.T
    return (g > r * 1.15) & (g > b * 1.05) & (g > 0.03)


def gleichmaessig(punkte, schritt):
    punkte = np.asarray(punkte, dtype=float)
    laenge = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(punkte, axis=0), axis=1))])
    ziel = np.linspace(0.0, laenge[-1], max(2, int(laenge[-1] / schritt) + 1))
    return np.stack([np.interp(ziel, laenge, punkte[:, i]) for i in range(3)], axis=1)


def glaetten(linie, n=3):
    for _ in range(n):
        linie[1:-1] = (linie[:-2] + 2 * linie[1:-1] + linie[2:]) / 4
    return linie


def wege(P, K, gr):
    n = len(P)
    m = gr[K[:, 0]] & gr[K[:, 1]]
    lang = np.linalg.norm(P[K[m, 0]] - P[K[m, 1]], axis=1)
    A = sp.coo_matrix((lang, (K[m, 0], K[m, 1])), shape=(n, n)).tocsr()
    gi = np.where(gr)[0]
    abstand, _ = cKDTree(P[~gr]).query(P[gi])
    Q = P[gi]
    (x0, x1), yb, (z0, z1) = RUECKEN
    ruecken = (Q[:, 0] > x0) & (Q[:, 0] < x1) & (np.abs(Q[:, 1]) < yb) & (Q[:, 2] > z0) & (Q[:, 2] < z1)
    quellen = gi[ruecken & (abstand < 0.04)]
    dist, vor, _ = dijkstra(A, directed=False, indices=quellen, min_only=True, return_predecessors=True)
    dist[~gr] = np.inf

    def weg(v):
        p = []
        while v >= 0:
            p.append(v)
            v = vor[v]
        return p[::-1]   # root first

    kandidaten = np.where(np.isfinite(dist) & (dist > MIN_LAENGE))[0]
    kandidaten = kandidaten[np.argsort(-dist[kandidaten])]
    baum = cKDTree(P[kandidaten])
    erledigt = np.zeros(n, bool)
    gefunden = []
    for v in kandidaten:
        if erledigt[v]:
            continue
        p = weg(v)
        q = P[p]
        neu = True
        for alt in gefunden:
            d, _ = cKDTree(P[alt]).query(q)
            if (d < 0.2).mean() > 0.5:
                neu = False
                break
        for u in p:
            for w in baum.query_ball_point(P[u], 0.25):
                erledigt[kandidaten[w]] = True
        if neu:
            gefunden.append(p)
    return gefunden, dist


def mittellinie(P, gr, dist, weg):
    """Centre line from the root to the tip and the radius along it. All vertices of a tube ring lie about equally
    far from the back along the surface, so the green vertices near the path in each band of that distance are one
    ring: its middle is a point of the centre line. Near the back the band spreads over the moss and the ring grows
    wide: the vine starts where the rings are still tube-sized."""
    gi = np.where(gr)[0]
    baum = cKDTree(P[gi])
    punkte, radien = [], []
    naechstes = -1.0
    for u in weg:
        if dist[u] < naechstes:
            continue
        naechstes = dist[u] + SCHRITT
        nah = gi[baum.query_ball_point(P[u], 0.45)]
        ring = P[nah[np.abs(dist[nah] - dist[u]) < SCHEIBE * 0.6]]
        if len(ring) < 4:
            continue
        mitte = ring.mean(axis=0)
        punkte.append(mitte)
        radien.append(float(np.linalg.norm(ring - mitte, axis=1).mean()))
    punkte, radien = np.array(punkte), np.array(radien)
    aussen = np.median(radien[len(radien) // 2:])
    start = 0
    for i in range(len(radien) - 1, -1, -1):
        if radien[i] > max(2.5 * aussen, 0.24):
            start = i + 1
            break
    punkte, radien = punkte[start:], radien[start:]
    if len(punkte) < 3:
        return punkte, radien
    linie = glaetten(gleichmaessig(punkte, SCHRITT), 3)
    lang = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(punkte, axis=0), axis=1))])
    radius = np.interp(np.linspace(0, lang[-1], len(linie)), lang, radien)
    return linie, glaetten(radius, 3)


def verlaengern(P, baum, linie, radius):
    """The vines' green fades into the back's colour near the root: follows the tube on down by geometry (slabs
    across the line, all vertices) until it widens into the back, at most WURZEL_MAX"""
    # The first rings at the green's edge are cut off and small: start a few steps up the tube
    linie, radius = linie[3:], radius[3:]
    c = linie[0].copy()
    t = linie[0] - linie[min(4, len(linie) - 1)]
    t /= np.linalg.norm(t)
    r0 = float(min(np.max(radius[:6]), 1.5 * np.median(radius)))
    davor, radien = [], []
    for _ in range(int(WURZEL_MAX / SCHRITT)):
        # Into the back means downwards: a line running on flat along the back is moss and spikes
        if t[2] > -0.5:
            break
        c = c + t * SCHRITT
        nah = P[baum.query_ball_point(c, 2.0 * r0)] - c
        s = nah[np.abs(nah @ t) < SCHEIBE]
        if len(s) < 6:
            break
        quer = s - np.outer(s @ t, t)
        aussen = (np.linalg.norm(quer, axis=1) > 1.6 * r0).mean()
        if aussen > 0.35:   # the back's surface spreads out round the line
            break
        neu = c + np.median(quer, axis=0)
        r = float(np.median(np.linalg.norm(quer - np.median(quer, axis=0), axis=1)))
        t = 0.7 * t + 0.3 * (neu - (davor[-1] if davor else linie[0]))
        t /= np.linalg.norm(t)
        c = neu
        davor.append(c)
        radien.append(max(r, r0))
    if not davor:
        return linie, radius
    roh = np.concatenate([np.array(davor[::-1]), linie])
    radien = np.concatenate([np.array(radien[::-1]), radius])
    neu = glaetten(gleichmaessig(roh, SCHRITT), 4)
    lang = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(roh, axis=0), axis=1))])
    return neu, np.interp(np.linspace(0, lang[-1], len(neu)), lang, radien)


def main():
    d = np.load(os.path.join(ARBEIT, "analyse_roh.npz"))
    P, F, K = d["punkte"], d["farbe"], d["kanten"]
    gr = gruen(F)
    r, g, b = F.T
    koralle = (r > g * 1.4) & (r > 0.12)   # pitcher mouths: coral red inside, bronze outside
    ranken = []
    gefunden, dist = wege(P, K, gr)
    alle = cKDTree(P)
    for weg in gefunden:
        linie, radius = mittellinie(P, gr, dist, weg)
        if len(linie) < 3:
            continue
        linie, radius = verlaengern(P, alle, linie, radius)
        for spitze, wurzel in WURZEL_VON_HAND:
            if np.linalg.norm(linie[-1] - spitze) < 0.3:
                ab = int(np.argmin(np.linalg.norm(linie - np.array(wurzel), axis=1)))
                linie, radius = linie[ab:], radius[ab:]
        laenge = np.linalg.norm(np.diff(linie, axis=0), axis=1).sum()
        if laenge < MIN_LAENGE or np.median(radius) > MAX_RADIUS or any((cKDTree(rk["linie"]).query(linie)[0] < 0.15).mean() > 0.5 for rk in ranken):
            continue
        ranken.append({"linie": linie, "radius": radius, "maul": np.zeros(0, int)})
    # Mouths: the coral vertices round a tip (each to its nearest tip), and with them everything inside the bud
    spitzen = np.array([rk["linie"][-1] for rk in ranken])
    kor = np.where(koralle)[0]
    d, j = cKDTree(spitzen).query(P[kor])
    for i, rk in enumerate(ranken):
        nah = kor[(j == i) & (d < MAUL_RADIUS)]
        if len(nah) < MIN_MAUL:
            continue
        mitte = P[nah].mean(axis=0)
        weite = np.percentile(np.linalg.norm(P[nah] - mitte, axis=1), 95) * MAUL_WEITE
        rk["maul"] = np.where(np.linalg.norm(P - mitte, axis=1) < weite)[0]
        rk["maul_mitte"] = mitte
    # Vertex to vine: green (and the black thorns and coral buds on it) within a few radii of its centre line,
    # past the root
    ranke = np.full(len(P), -1)
    best = np.full(len(P), np.inf)
    for i, rk in enumerate(ranken):
        baum = cKDTree(rk["linie"])
        d, j = baum.query(P)
        r_ = rk["radius"][j]
        # Capped: round a thick root a few radii reach far over the back
        grenze = np.minimum(np.where(gr, 1.8, 3.0) * r_, np.where(gr, 0.22, 0.3))
        nimm = (d < grenze) & (j > 2) & (d < best)
        ranke[nimm] = i
        best[nimm] = d[nimm]
        ranke[rk["maul"]] = i
    aus = []
    for i, rk in enumerate(ranken):
        laenge = float(np.linalg.norm(np.diff(rk["linie"], axis=0), axis=1).sum())
        aus.append({"linie": rk["linie"].round(4).tolist(), "radius": rk["radius"].round(4).tolist(),
                    "laenge": round(laenge, 3), "maul": [int(v) for v in rk["maul"]],
                    "maul_mitte": [round(float(c), 4) for c in rk.get("maul_mitte", ())],
                    "vertices": int((ranke == i).sum())})
        print("[Kelchreisser] Ranke %d: %.2f m, Radius %.3f, Wurzel %s, Spitze %s, Maul %d, Vertices %d" % (
            i, laenge, np.median(rk["radius"]), rk["linie"][0].round(2), rk["linie"][-1].round(2),
            len(rk["maul"]), (ranke == i).sum()))
    with open(os.path.join(ARBEIT, "ranken.json"), "w", encoding="utf-8") as f:
        json.dump(aus, f)
    np.savez(os.path.join(ARBEIT, "ranken.npz"), ranke=ranke)


main()
