"""
Skelett der Erztochter (nach 'vorbereiten' der Kreaturenfabrik, vor 'binden').

Wie ameise_skelett.py, aber für die doppelt so große junge Königin:
- Jedes der sechs Beine wird vom Fuß aus über das Netz verfolgt (fehlende Beinknochen der Vorlage werden angelegt,
  die Fabrik findet nicht immer alle sechs Füße).
- Rumpf und Kopf werden auf Brust/Hinterleib und Kopf gelegt.
- Neu: je Seite ein Flügelknochen (wing_L, wing_R) für die beiden Flügelstummel dieser Seite. Daran hängen
  Resonanz (Vibrieren) und das Abbrechen (Knochen auf 0 skalieren blendet den Stummel aus).
- Danach wird gebunden: automatische Gewichtung, dann bekommen die Flügelstummel ihre Seite zu 100 %.
  'binden' der Fabrik exportiert anschließend nur noch (der Armature-Modifier ist schon da).

  blender -b <Erztochter.blend> -P Tools/Ameisen/erztochter_skelett.py
"""
import heapq

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
S = L / 1.1                      # Maßstab gegenüber der Arbeiterin (1,1 m), deren Werte hier übernommen sind
nach = [[] for _ in P]
for e in me.edges:
    a, b = e.vertices
    d = (P[a] - P[b]).length
    nach[a].append((b, d))
    nach[b].append((a, d))

# Flügelstummel: hoch über dem Rücken, hinter dem Kopfkamm, seitlich der Mittellinie
FLUEGEL_Z = 0.6 * H
def ist_fluegel(p):
    return p.z > FLUEGEL_Z and p.y > lo.y + 0.42 * L and abs(p.x) > 0.05 * S


def rumpf_halbbreite(y):
    pts = [p for p in P if abs(p.y - y) < 0.02 * S and 0.2 * H < p.z < 0.5 * H]
    return max((abs(p.x) for p in pts), default=0.05 * S)


# 1) Füße: tiefste Punkte, nach Lage gruppiert (der Hinterleib hängt knapp über dem Boden, darum nur die untersten 2 cm)
tief = [i for i, p in enumerate(P) if p.z < lo.z + 0.02 * S]
gruppen = []
for i in sorted(tief, key=lambda i: P[i].z):
    for g in gruppen:
        if (P[g[0]].xy - P[i].xy).length < 0.08 * S:
            g.append(i)
            break
    else:
        gruppen.append([i])
gruppen = sorted(gruppen, key=len, reverse=True)[:6]
print("FUESSE", len(gruppen), [(len(g), round(P[g[0]].x, 2), round(P[g[0]].y, 2)) for g in gruppen])


def verfolgen(start):
    grenze = 1.2 * S
    dist = {start: 0.0}
    q = [(0.0, start)]
    while q:
        d, i = heapq.heappop(q)
        if d > dist.get(i, 9e9) or d > grenze:
            continue
        for j, w in nach[i]:
            nd = d + w
            if nd < dist.get(j, 9e9):
                dist[j] = nd
                heapq.heappush(q, (nd, j))
    linie = [P[start].copy()]
    band = 0.025 * S
    k = 1
    while k * band < grenze:
        pts = [P[i] for i, d in dist.items() if (k - 1) * band <= d < k * band and (P[i] - linie[-1]).length < 0.08 * S]
        if len(pts) < 3:
            break
        c = sum(pts, Vector()) / len(pts)
        linie.append(c)
        if len(linie) > 4 and abs(c.x) < 0.075 * S:
            break           # an der Körperunterseite angekommen (das Bein steigt außen hoch und knickt dann nach innen)
        k += 1
    return linie


beine = []
for g in gruppen:
    start = min(g, key=lambda i: P[i].z)
    linie = verfolgen(start)
    spitze = max((P[i] for i in g), key=lambda p: abs(p.x) + abs(p.y) * 0.3)
    knie_i = max(range(len(linie)), key=lambda i: linie[i].z if abs(linie[i].x) > 0.06 * S else -1)
    knie = linie[knie_i]
    huefte = linie[-1]
    # Fußgelenk: wo das Bein den Boden verlässt (der Fuß liegt kurz und flach auf)
    gelenk = next((c for c in linie[1:knie_i] if c.z > 0.03 * S), linie[max(1, knie_i // 3)])
    beine.append({"seite": "L" if huefte.x > 0 else "R", "y": huefte.y, "huefte": huefte, "knie": knie,
                  "gelenk": gelenk, "spitze": spitze})
    print("BEIN", beine[-1]["seite"], round(huefte.y, 2), "Linie", len(linie), "Knie z", round(knie.z, 2),
          "Huefte", [round(v, 2) for v in huefte], "Gelenk z", round(gelenk.z, 2))

# Flügel: Wurzel = Mitte der rumpfnahen Stummelpunkte, Spitze = Mitte der fernsten
fluegel = {}
for seite, vz in (("L", 1), ("R", -1)):
    pts = [p for p in P if ist_fluegel(p) and p.x * vz > 0]
    if len(pts) < 20:
        print("FLUEGEL fehlen", seite, len(pts))
        continue
    pts.sort(key=lambda p: abs(p.x))
    wurzel = sum(pts[:max(5, len(pts) // 10)], Vector()) / max(5, len(pts) // 10)
    pts.sort(key=lambda p: (p - wurzel).length)
    spitze = sum(pts[-max(5, len(pts) // 20):], Vector()) / max(5, len(pts) // 20)
    fluegel[seite] = (wurzel, spitze, len(pts))
    print("FLUEGEL", seite, len(pts), [round(v, 2) for v in wurzel], [round(v, 2) for v in spitze])

bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
eb = arm.data.edit_bones
inv = arm.matrix_world.inverted()
body = eb["body"]
for seite in ("L", "R"):
    eigene = sorted([b for b in beine if b["seite"] == seite], key=lambda b: b["y"])   # vorne = kleines y
    for nr, b in zip(("1", "2", "3"), eigene):
        pre = "leg_{}{}_".format(seite, nr)
        ansatz = Vector((b["huefte"].x * 0.3, b["huefte"].y, b["huefte"].z))
        eltern = body
        for name, a, z in (("coxa", ansatz, b["huefte"]), ("femur", b["huefte"], b["knie"]),
                           ("tibia", b["knie"], b["gelenk"]), ("foot", b["gelenk"], b["spitze"])):
            k = eb.get(pre + name) or eb.new(pre + name)
            k.parent = eltern
            k.use_connect = False
            k.head = inv @ a
            k.tail = inv @ z
            if (k.tail - k.head).length < 0.01:
                k.tail = k.head + Vector((0, 0, 0.02 * S))
            eltern = k
# Rumpf von der Brust bis ans Hinterleibende, Kopf nach vorne (Maße aus dem Längsprofil des Modells)
eb["body"].head = inv @ Vector((0, lo.y + L * 0.33, H * 0.38))
eb["body"].tail = inv @ Vector((0, lo.y + L * 0.94, H * 0.30))
eb["head"].head = inv @ Vector((0, lo.y + L * 0.32, H * 0.47))
eb["head"].tail = inv @ Vector((0, lo.y + L * 0.10, H * 0.42))
for seite, (wurzel, spitze, _) in fluegel.items():
    k = eb.get("wing_" + seite) or eb.new("wing_" + seite)
    k.parent = body
    k.use_connect = False
    k.head = inv @ wurzel
    k.tail = inv @ spitze
bpy.ops.object.mode_set(mode="OBJECT")

# Binden: automatische Gewichtung, dann die Stummel fest an ihre Seite
for m in [m for m in obj.modifiers if m.type == "ARMATURE"]:
    obj.modifiers.remove(m)
obj.vertex_groups.clear()
bpy.ops.object.select_all(action="DESELECT")
obj.select_set(True)
arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.parent_set(type="ARMATURE_AUTO")
benutzt = {g.group for v in me.vertices for g in v.groups if g.weight > 0.01}
leer = [g.name for g in obj.vertex_groups if g.index not in benutzt and g.name != "root"]
print("GEWICHTE leer:", leer)
gruppen_name = {g.index: g.name for g in obj.vertex_groups}
fest = 0
for seite in fluegel:
    vg = obj.vertex_groups["wing_" + seite]
    vz = 1 if seite == "L" else -1
    ids = [v.index for v in me.vertices if ist_fluegel(mw @ v.co) and (mw @ v.co).x * vz > 0]
    for g in obj.vertex_groups:
        g.remove(ids)
    vg.add(ids, 1.0, "REPLACE")
    fest += len(ids)
    # außerhalb der Stummel trägt der Flügelknochen nichts
    menge = set(ids)
    andere = [v.index for v in me.vertices if v.index not in menge]
    vg.remove(andere)
print("FLUEGEL fest gewichtet:", fest)
bpy.ops.wm.save_mainfile()
print("SKELETT gesetzt und gebunden")
