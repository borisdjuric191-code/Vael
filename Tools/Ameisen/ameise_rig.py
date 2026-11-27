"""
Skelett und Bindung für Schürfameise und Erztochter (ersetzt die Beinsuche aus ameise_skelett.py/erztochter_skelett.py).

Boris 27.11.: Die Beine der Arbeiterin bewegten sich nicht (fünf Oberschenkel hatten die Länge 0, die Beinrechnung im
Spiel überspringt solche Beine), bei der Tochter standen die Hinterbeine. Außerdem sollen Fühler und Zangen leben.

- Beine: jedes vom Fuß aus über das Netz verfolgt. Hüfte = wo die Linie an der Körperunterseite ankommt (liegt sie noch
  außen, wird sie nach innen gesetzt), Knie = höchster Punkt dazwischen (sonst 60 % der Linie), Fußgelenk = wo das Bein
  den Boden verlässt. Jeder Knochen hat eine echte Länge.
- Rumpf und Kopf wie bisher (Arbeiterin: Werte aus ameise_skelett.py, Tochter: aus ihrem Längsprofil); mit "taille" in
  rig.json endet der Rumpf (Brust) dort, und ein eigener Knochen "abdomen" trägt den Hinterleib.
- Fühler (zwei Knochen: Schaft und Geißel) und Zangen (ein Knochen) an den Punkten aus rig.json, Kind des Kopfes.
- Flügelstummel der Tochter wie in erztochter_skelett.py (je Seite ein Knochen).
- Bindung: automatische Gewichtung, dann bekommen Fühler, Zangen und Stummel ihre Teile fest (Korridor um die Knochen).

  blender -b <Modell.blend> -P Tools/Ameisen/ameise_rig.py -- <rig.json>
Danach exportiert 'binden' der Kreaturenfabrik nur noch (der Armature-Modifier ist schon da).
"""
import heapq
import json
import sys

import bpy
from mathutils import Vector
from mathutils.geometry import intersect_point_line

CFG = json.load(open(sys.argv[sys.argv.index("--") + 1], encoding="utf-8"))
obj = next(o for o in bpy.data.objects if o.type == "MESH" and o.name.startswith("SK_"))
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
me = obj.data
mw = obj.matrix_world
P = [mw @ v.co for v in me.vertices]
lo = Vector((min(p.x for p in P), min(p.y for p in P), min(p.z for p in P)))
hi = Vector((max(p.x for p in P), max(p.y for p in P), max(p.z for p in P)))
H = hi.z - lo.z
L = hi.y - lo.y
S = L / 1.1 if CFG["rumpf"] == "ameise" else L / 1.1
nach = [[] for _ in P]
for e in me.edges:
    a, b = e.vertices
    d = (P[a] - P[b]).length
    nach[a].append((b, d))
    nach[b].append((a, d))

FLUEGEL_Z = 0.6 * H
def ist_fluegel(p):
    return CFG.get("fluegel") and p.z > FLUEGEL_Z and p.y > lo.y + 0.42 * L and abs(p.x) > 0.05 * S


# ---------------------------------------------------------------- Beine
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
print("FUESSE", len(gruppen))


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
            break
        k += 1
    return linie


beine = []
for g in gruppen:
    start = min(g, key=lambda i: P[i].z)
    linie = verfolgen(start)
    spitze = max((P[i] for i in g), key=lambda p: abs(p.x) + abs(p.y) * 0.3)
    seite = 1.0 if linie[-1].x > 0 else -1.0
    huefte = linie[-1].copy()
    if abs(huefte.x) > 0.1 * S:
        # die Linie kam nicht bis unter den Körper: Hüfte nach innen setzen, die letzte Stelle wird das Knie
        huefte = Vector((seite * 0.08 * S, huefte.y, huefte.z - 0.02 * S))
    innen = [i for i in range(1, len(linie)) if abs(linie[i].x) > abs(huefte.x) + 0.03 * S]
    knie_i = max(innen, key=lambda i: linie[i].z) if innen else int(len(linie) * 0.6)
    knie = linie[knie_i]
    if (knie - huefte).length < 0.06 * S:
        knie_i = max(1, int(len(linie) * 0.6))
        knie = linie[knie_i]
    gelenk = next((c for c in linie[1:knie_i] if c.z > lo.z + 0.03 * S), linie[max(1, knie_i // 3)])
    beine.append({"seite": "L" if seite > 0 else "R", "y": huefte.y, "huefte": huefte, "knie": knie,
                  "gelenk": gelenk, "spitze": spitze})
    print("BEIN", beine[-1]["seite"], "y %.2f" % huefte.y, "Oberschenkel %.3f" % (knie - huefte).length,
          "Schiene %.3f" % (gelenk - knie).length, "Fuss %.3f" % (spitze - gelenk).length)

# ---------------------------------------------------------------- Flügel (Tochter)
fluegel = {}
for seite, vz in (("L", 1), ("R", -1)):
    pts = [p for p in P if ist_fluegel(p) and p.x * vz > 0]
    if len(pts) < 20:
        continue
    pts.sort(key=lambda p: abs(p.x))
    n = max(5, len(pts) // 10)
    wurzel = sum(pts[:n], Vector()) / n
    pts.sort(key=lambda p: (p - wurzel).length)
    n = max(5, len(pts) // 20)
    fluegel[seite] = (wurzel, sum(pts[-n:], Vector()) / n)

# ---------------------------------------------------------------- Knochen setzen
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
eb = arm.data.edit_bones
inv = arm.matrix_world.inverted()
body = eb["body"]


def knochen(name, eltern, a, z):
    k = eb.get(name) or eb.new(name)
    k.parent = eltern
    k.use_connect = False
    k.head = inv @ a
    k.tail = inv @ z
    if (k.tail - k.head).length < 0.01:
        k.tail = k.head + Vector((0, 0, 0.02 * S))
    return k


for seite in ("L", "R"):
    eigene = sorted([b for b in beine if b["seite"] == seite], key=lambda b: b["y"])
    for nr, b in zip(("1", "2", "3"), eigene):
        pre = "leg_{}{}_".format(seite, nr)
        ansatz = Vector((b["huefte"].x * 0.3, b["huefte"].y, b["huefte"].z))
        k = knochen(pre + "coxa", body, ansatz, b["huefte"])
        k = knochen(pre + "femur", k, b["huefte"], b["knie"])
        k = knochen(pre + "tibia", k, b["knie"], b["gelenk"])
        knochen(pre + "foot", k, b["gelenk"], b["spitze"])

if CFG["rumpf"] == "ameise":
    mitte_z = H * 0.6
    eb["body"].head = inv @ Vector((0, lo.y + L * 0.38, mitte_z))
    eb["body"].tail = inv @ Vector((0, lo.y + L * 0.92, mitte_z))
    eb["head"].head = inv @ Vector((0, lo.y + L * 0.36, mitte_z))
    eb["head"].tail = inv @ Vector((0, lo.y + L * 0.12, mitte_z))
else:
    eb["body"].head = inv @ Vector((0, lo.y + L * 0.33, H * 0.38))
    eb["body"].tail = inv @ Vector((0, lo.y + L * 0.94, H * 0.30))
    eb["head"].head = inv @ Vector((0, lo.y + L * 0.32, H * 0.47))
    eb["head"].tail = inv @ Vector((0, lo.y + L * 0.10, H * 0.42))
if CFG.get("taille"):
    # Boris 27.11.: der Hinterleib soll sich eigen bewegen (heben, senken, pumpen): eigener Knochen ab der Taille,
    # der Rumpfknochen trägt nur noch die Brust (Beine und Flügel hängen dort)
    ende = arm.matrix_world @ eb["body"].tail
    taille = Vector(CFG["taille"])
    eb["body"].tail = inv @ taille
    knochen("abdomen", eb["body"], taille, ende)
kopf = eb["head"]
for seite, (wurzel, spitze) in fluegel.items():
    knochen("wing_" + seite, body, wurzel, spitze)
teile = {}     # Knochenname -> (Anfang, Ende, Radius)
for seite, (a, m, z) in CFG.get("fuehler", {}).items():
    a, m, z = Vector(a), Vector(m), Vector(z)
    k = knochen("antenna_{}1".format(seite), kopf, a, m)
    knochen("antenna_{}2".format(seite), k, m, z)
    teile["antenna_{}1".format(seite)] = (a, m, CFG["fuehler_radius"])
    teile["antenna_{}2".format(seite)] = (m, z, CFG["fuehler_radius"])
for seite, (a, z) in CFG.get("zangen", {}).items():
    a, z = Vector(a), Vector(z)
    knochen("mandible_" + seite, kopf, a, z)
    teile["mandible_" + seite] = (a, z, CFG["zangen_radius"])
bpy.ops.object.mode_set(mode="OBJECT")

# ---------------------------------------------------------------- Binden
for m in [m for m in obj.modifiers if m.type == "ARMATURE"]:
    obj.modifiers.remove(m)
obj.vertex_groups.clear()
bpy.ops.object.select_all(action="DESELECT")
obj.select_set(True)
arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.parent_set(type="ARMATURE_AUTO")
benutzt = {g.group for v in me.vertices for g in v.groups if g.weight > 0.01}
print("GEWICHTE leer:", [g.name for g in obj.vertex_groups if g.index not in benutzt and g.name != "root"])


def fest(name, ids):
    """gibt den Ecken ganz dem Knochen und nimmt ihn allen anderen"""
    vg = obj.vertex_groups.get(name) or obj.vertex_groups.new(name=name)
    for g in obj.vertex_groups:
        g.remove(ids)
    vg.add(ids, 1.0, "REPLACE")
    menge = set(ids)
    vg.remove([v.index for v in me.vertices if v.index not in menge])


# Flügelstummel zuerst fest (sonst hielten Brustpunkte Reste der Wärme-Gewichtung der Flügel)
for seite in fluegel:
    vz = 1 if seite == "L" else -1
    fest("wing_" + seite, [v.index for v in me.vertices if ist_fluegel(mw @ v.co) and (mw @ v.co).x * vz > 0])

# Körperkern fest nach der Lage entlang der Längsachse (die Wärme-Gewichtung warf Kopf, Brust und Hinterleib durcheinander):
# vor dem Hals der Kopf (bei der Tochter auch der ganze Knochenkamm), bis zur Taille die Brust, dahinter der Hinterleib,
# an den Übergängen weich gemischt. Punkte, die an Beinen, Fühlern, Zangen oder Stummeln hängen, bleiben unberührt.
hals_y = (arm.matrix_world @ arm.data.bones["head"].head_local).y
taille_y = CFG["taille"][1] if CFG.get("taille") else None
weich = 0.03 * S
kern_namen = {"head", "body", "abdomen", "root"}
gid = {g.name: g.index for g in obj.vertex_groups}
name_von = {g.index: g.name for g in obj.vertex_groups}


def anteile(p):
    if CFG["rumpf"] != "ameise" and p.z > 0.62 * H and p.y < lo.y + 0.35 * L:
        return {"head": 1.0}                    # Knochenkamm der Tochter
    k = max(0.0, min(1.0, (p.y - (hals_y - weich)) / (2 * weich)))      # 0 = Kopf, 1 = Brust
    w = {"head": 1.0 - k, "body": k}
    if taille_y is not None:
        a = max(0.0, min(1.0, (p.y - (taille_y - weich)) / (2 * weich)))  # 0 = Brust, 1 = Hinterleib
        w = {"head": w["head"] * (1 - a), "body": w["body"] * (1 - a), "abdomen": a}
    return {n: x for n, x in w.items() if x > 0.001}


# Ein Bein behält nur Punkte nahe seiner Knochenkette; was die Wärme-Gewichtung ihm sonst gab (bei der Tochter ein Stück
# Sattel über der Hüfte), gehört zum Körperkern
kette = {}
for b in arm.data.bones:
    if b.name.startswith("leg_") and not b.name.endswith("_coxa"):    # die Hüfte liegt im Körper
        kette.setdefault(b.name[:7], []).append((arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local))
BEIN_R = 0.05 * S


def nahe_bein(p, bein):
    for a, z in kette.get(bein, []):
        q, t = intersect_point_line(p, a, z)
        t = max(0.0, min(1.0, t))
        if (p - (a + (z - a) * t)).length < BEIN_R:
            return True
    return False


kern = 0
for v in me.vertices:
    p = mw @ v.co
    stark = [name_von[g.group] for g in v.groups if g.weight > 0.25]
    if any(n not in kern_namen and not (n.startswith("leg_") and not nahe_bein(p, n[:7])) for n in stark):
        continue
    for g in list(v.groups):
        obj.vertex_groups[g.group].remove([v.index])
    for n, x in anteile(mw @ v.co).items():
        obj.vertex_groups[n].add([v.index], x, "REPLACE")
    kern += 1
print("KERN fest gewichtet", kern)


# Fühler und Zangen: Startpunkte im engen Korridor um die abgelesene Linie, dann über das Netz wachsen, solange der Punkt
# jenseits des Ansatzes liegt (der Kopf liegt dahinter) und nahe der Linie bleibt (die Teile sind gebogen)
def abstand_linie(p, punkte):
    best = 9e9
    for a, z in zip(punkte, punkte[1:]):
        q, t = intersect_point_line(p, a, z)
        t = max(0.0, min(1.0, t))
        best = min(best, (p - (a + (z - a) * t)).length)
    return best


gruppen_teile = []      # (Knochen je Abschnitt, Punkte der Linie, Radius)
for seite, (a, m, z) in CFG.get("fuehler", {}).items():
    gruppen_teile.append((["antenna_{}1".format(seite), "antenna_{}2".format(seite)],
                          [Vector(a), Vector(m), Vector(z)], CFG["fuehler_radius"]))
for seite, (a, z) in CFG.get("zangen", {}).items():
    gruppen_teile.append((["mandible_" + seite], [Vector(a), Vector(z)], CFG["zangen_radius"]))

vergeben = set()
for namen, punkte, r in gruppen_teile:
    a = punkte[0]
    richt = (punkte[1] - a).normalized()
    jenseits = lambda p: (p - a).dot(richt) > 0.08 * r / 0.03 * 0.01 + 0.004
    saat = [i for i, p in enumerate(P) if i not in vergeben and jenseits(p) and abstand_linie(p, punkte) < r]
    teil = set(saat)
    rand = list(saat)
    while rand:
        neu = []
        for i in rand:
            for j, _ in nach[i]:
                if j in teil or j in vergeben:
                    continue
                if jenseits(P[j]) and abstand_linie(P[j], punkte) < r * 3.0:
                    teil.add(j)
                    neu.append(j)
        rand = neu
    vergeben |= teil
    # auf die Abschnitte verteilen: jeder Punkt zum nächsten Abschnitt
    je = {n: [] for n in namen}
    for i in teil:
        k = min(range(len(namen)), key=lambda k: abstand_linie(P[i], punkte[k:k + 2]))
        je[namen[k]].append(i)
    for n in namen:
        fest(n, je[n])
        print("TEIL", n, len(je[n]))

# Punkte, die dabei ihren einzigen Knochen verloren haben, gehören zum Kopf
kopf_vg = obj.vertex_groups["head"]
ohne = [v.index for v in me.vertices if not any(g.weight > 0.001 for g in v.groups)]
kopf_vg.add(ohne, 1.0, "REPLACE")
print("OHNE KNOCHEN -> Kopf", len(ohne))
bpy.ops.wm.save_mainfile()
print("RIG fertig")
