"""
Setzt die Beinknochen der Schürfameise auf die echten Beine (nach 'vorbereiten' der Kreaturenfabrik, vor 'binden').

Das Vielbeiner-Vorlage-Skelett ist für flache Käfer gedacht; die Ameise hat hohe Knie und lange Beine. Darum wird jedes
Bein vom Fuß aus über das Netz verfolgt (Abstand entlang der Oberfläche, in Bändern zu 2,5 cm): die Bandmitten ergeben
die Mittellinie des Beins bis zum Körper. Daraus: Hüfte (wo die Linie den Körper erreicht), Knie (höchster Punkt),
Fußgelenk (Übergang zum Fuß) und Fußspitze. Rumpf und Kopf werden auf Brust/Hinterleib und Kopf gelegt.

  blender -b <Schuerfameise.blend> -P Tools/Ameisen/ameise_skelett.py
"""
import heapq
import math

import bpy
from mathutils import Vector

obj = next(o for o in bpy.data.objects if o.type == "MESH" and o.name.startswith("SK_"))
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
me = obj.data
mw = obj.matrix_world
P = [mw @ v.co for v in me.vertices]
lo = Vector((min(p.x for p in P), min(p.y for p in P), min(p.z for p in P)))
hi = Vector((max(p.x for p in P), max(p.y for p in P), max(p.z for p in P)))
H = hi.z - lo.z
L = hi.y - lo.y
nach = [[] for _ in P]
for e in me.edges:
    a, b = e.vertices
    d = (P[a] - P[b]).length
    nach[a].append((b, d))
    nach[b].append((a, d))

# Körperbreite entlang der Länge: die halbe Breite des Rumpfs in Höhe der Körpermitte (Beine liegen tiefer/außen)
def rumpf_halbbreite(y):
    pts = [p for p in P if abs(p.y - y) < 0.02 and p.z > H * 0.45]
    return max((abs(p.x) for p in pts), default=0.05)

# 1) Füße: tiefste Punkte, nach Lage gruppiert
tief = [i for i, p in enumerate(P) if p.z < lo.z + 0.02]
gruppen = []
for i in sorted(tief, key=lambda i: P[i].z):
    for g in gruppen:
        if (P[g[0]].xy - P[i].xy).length < 0.08:
            g.append(i)
            break
    else:
        gruppen.append([i])
gruppen = sorted(gruppen, key=len, reverse=True)[:6]
print("FUESSE", len(gruppen), [len(g) for g in gruppen])


def verfolgen(start):
    dist = {start: 0.0}
    q = [(0.0, start)]
    while q:
        d, i = heapq.heappop(q)
        if d > dist.get(i, 9e9) or d > 1.2:
            continue
        for j, w in nach[i]:
            nd = d + w
            if nd < dist.get(j, 9e9):
                dist[j] = nd
                heapq.heappush(q, (nd, j))
    linie = [P[start].copy()]
    band = 0.025
    k = 1
    while k * band < 1.2:
        pts = [P[i] for i, d in dist.items() if (k - 1) * band <= d < k * band and (P[i] - linie[-1]).length < 0.08]
        if len(pts) < 3:
            break
        c = sum(pts, Vector()) / len(pts)
        linie.append(c)
        if abs(c.x) < rumpf_halbbreite(c.y) * 0.85 and c.z > H * 0.25:
            break           # im Körper angekommen
        k += 1
    return linie


beine = []
for g in gruppen:
    start = min(g, key=lambda i: P[i].z)
    linie = verfolgen(start)
    spitze = max((P[i] for i in g), key=lambda p: abs(p.x) + abs(p.y) * 0.3)
    knie_i = max(range(len(linie)), key=lambda i: linie[i].z if abs(linie[i].x) > 0.06 else -1)
    knie = linie[knie_i]
    huefte = linie[-1]
    # Fußgelenk: auf einem Drittel vom Fuß zum Knie (Tarsus liegt flach am Boden, die Tibia steigt steil)
    gelenk = linie[max(1, knie_i // 3)]
    beine.append({"seite": "L" if huefte.x > 0 else "R", "y": huefte.y, "huefte": huefte, "knie": knie,
                  "gelenk": gelenk, "spitze": spitze, "n": len(linie)})
    print("BEIN", beine[-1]["seite"], round(huefte.y, 2), "Linie", len(linie), "Knie z", round(knie.z, 2))

bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
eb = arm.data.edit_bones
inv = arm.matrix_world.inverted()
for seite in ("L", "R"):
    eigene = sorted([b for b in beine if b["seite"] == seite], key=lambda b: b["y"])   # vorne = kleines y
    for nr, b in zip(("1", "2", "3"), eigene):
        pre = "leg_{}{}_".format(seite, nr)
        hw = rumpf_halbbreite(b["huefte"].y)
        ansatz = Vector((b["huefte"].x * 0.3, b["huefte"].y, b["huefte"].z))
        for name, a, z in (("coxa", ansatz, b["huefte"]), ("femur", b["huefte"], b["knie"]),
                           ("tibia", b["knie"], b["gelenk"]), ("foot", b["gelenk"], b["spitze"])):
            k = eb.get(pre + name)
            if k:
                k.use_connect = False
                k.head = inv @ a
                k.tail = inv @ z
# Rumpf: von der Brust bis ans Hinterleibende in Körpermitte, Kopf vorne
mitte_z = H * 0.6
eb["body"].head = inv @ Vector((0, lo.y + L * 0.38, mitte_z))
eb["body"].tail = inv @ Vector((0, lo.y + L * 0.92, mitte_z))
eb["head"].head = inv @ Vector((0, lo.y + L * 0.36, mitte_z))
eb["head"].tail = inv @ Vector((0, lo.y + L * 0.12, mitte_z))
bpy.ops.object.mode_set(mode="OBJECT")
bpy.ops.wm.save_mainfile()
print("SKELETT gesetzt")
