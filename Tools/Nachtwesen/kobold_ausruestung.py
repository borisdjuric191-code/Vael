"""Gear of the Kobolde (Boris 2026-11-23), made in Blender and joined into the body before it gets its skeleton, so it
moves with the bone it is bound to:

  Hortwaechter     an ore mace in the right hand: a wooden haft with iron bands and a head of Kobaltbluete crystals
                   (the hoard ore)
  (Kontoraeltester: his collar gems are recoloured in the ore colours by kobold_variante.py; kette() below, an extra chain with
                   pendants, was tried and dropped on 2026-11-23: it hung over the robe like a second necklace)
  Sklaventreiber   a coiled whip at the right hip, and the same whip in the right hand on a chain of bones
                   (peitsche_1 ..) that the Peitschen clip cracks; the game shows one of the two at a time

Each part has its own material slot (Ausruestung, Peitsche_Huefte, Peitsche_Hand); its colours are vertex colours, the
alpha is how much it glows. The binding (which points follow which bone, with what weight) is written to the object as
the custom property "bindung" and read by kobold_skelett.py.
"""
import json
import math
import random

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

# Ore colours (BuildKristallAura.py)
ERZE = {
    "Rosteisen": (1.0, 0.45, 0.12), "Schwefelkies": (1.0, 0.85, 0.3), "Funkenerz": (1.0, 0.35, 0.05),
    "Gruenspan": (0.1, 0.9, 0.5), "Salzspat": (1.0, 0.55, 0.65), "Drahtsilber": (0.7, 0.85, 1.0),
    "Blutzinnober": (1.0, 0.12, 0.25), "Kobaltbluete": (1.0, 0.2, 0.6), "Siechglas": (0.75, 1.0, 0.2),
    "Rippenspat": (1.0, 0.65, 0.3), "Kernerz": (0.3, 1.0, 0.95),
}
HOLZ = (0.16, 0.09, 0.05, 0.0)
EISEN = (0.11, 0.11, 0.12, 0.0)
GOLD = (0.55, 0.38, 0.1, 0.0)
LEDER = (0.09, 0.06, 0.04, 0.0)

SLOTS = ("Ausruestung", "Peitsche_Huefte", "Peitsche_Hand")


class Teile:
    """Collects geometry with vertex colours and a bone per point, then joins it into the body"""

    def __init__(self):
        self.bm = bmesh.new()
        self.farbe = self.bm.loops.layers.float_color.new("Farbe")
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.knochen = []      # per point: list of (bone, weight)
        self.slot = {}         # face -> slot name

    def _punkt(self, p, knochen):
        v = self.bm.verts.new(Vector(p))
        self.knochen.append(knochen if isinstance(knochen, list) else [(knochen, 1.0)])
        return v

    def _flaeche(self, verts, farbe, slot):
        try:
            f = self.bm.faces.new(verts)
        except ValueError:
            return None
        f.smooth = True
        for loop in f.loops:
            loop[self.farbe] = farbe
        self.slot[f] = slot
        return f

    def schlauch(self, bahn, radien, farbe, slot, knochen, seiten=6, deckel=True):
        """A tube along points; knochen is one bone name or a function (index along the path) -> [(bone, weight)]"""
        ringe = []
        for i, (p, r) in enumerate(zip(bahn, radien)):
            p = Vector(p)
            t = (Vector(bahn[min(i + 1, len(bahn) - 1)]) - Vector(bahn[max(i - 1, 0)])).normalized()
            a = t.cross(Vector((0.0, 0.0, 1.0)))
            if a.length < 0.2:
                a = t.cross(Vector((1.0, 0.0, 0.0)))
            a.normalize()
            b = t.cross(a)
            kn = knochen(i) if callable(knochen) else knochen
            ringe.append([self._punkt(p + (a * math.cos(2 * math.pi * k / seiten) + b * math.sin(2 * math.pi * k / seiten)) * r, kn)
                          for k in range(seiten)])
        for i in range(len(ringe) - 1):
            for k in range(seiten):
                self._flaeche((ringe[i][k], ringe[i][(k + 1) % seiten], ringe[i + 1][(k + 1) % seiten], ringe[i + 1][k]), farbe, slot)
        if deckel:
            self._flaeche(ringe[0][::-1], farbe, slot)
            self._flaeche(ringe[-1], farbe, slot)

    def kristall(self, fuss, achse, laenge, dicke, farbe, slot, knochen, seiten=5, dreh=0.0):
        """A pointed crystal: a prism with a tip, standing on fuss along achse"""
        achse = Vector(achse).normalized()
        a = achse.cross(Vector((0.0, 0.0, 1.0)))
        if a.length < 0.2:
            a = achse.cross(Vector((1.0, 0.0, 0.0)))
        a.normalize()
        b = achse.cross(a)
        fuss = Vector(fuss)
        unten, oben = [], []
        for k in range(seiten):
            w = dreh + 2 * math.pi * k / seiten
            r = (a * math.cos(w) + b * math.sin(w)) * dicke
            unten.append(self._punkt(fuss + r * 0.8, knochen))
            oben.append(self._punkt(fuss + r + achse * laenge * 0.7, knochen))
        spitze = self._punkt(fuss + achse * laenge, knochen)
        for k in range(seiten):
            n = (k + 1) % seiten
            self._flaeche((unten[k], unten[n], oben[n], oben[k]), farbe, slot)
            self._flaeche((oben[k], oben[n], spitze), farbe, slot)
        self._flaeche(unten[::-1], farbe, slot)

    def einfuegen(self, obj):
        """Joins the parts into the body: new material slots, vertex colours, binding as custom property"""
        me = obj.data
        start = len(me.vertices)
        alt_bindung = json.loads(obj.get("bindung", "[]"))
        # The body's faces get colour white with no glow; its material slot is 0
        if "Farbe" not in me.color_attributes:
            me.color_attributes.new("Farbe", "FLOAT_COLOR", "CORNER")
        neu = bpy.data.meshes.new("teile")
        slot_namen = sorted(set(self.slot.values()), key=SLOTS.index)
        for f in self.bm.faces:
            f.material_index = slot_namen.index(self.slot[f])
        self.bm.to_mesh(neu)
        teil = bpy.data.objects.new("teile", neu)
        bpy.context.scene.collection.objects.link(teil)
        for s in slot_namen:
            mat = bpy.data.materials.get(s) or bpy.data.materials.new(s)
            teil.data.materials.append(mat)
        # The body's first slot stays first; join appends the part slots after it
        for o in bpy.context.scene.objects:
            o.select_set(o in (obj, teil))
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.join()
        bindung = alt_bindung + [[start + i, kn] for i, kn in enumerate(self.knochen)]
        obj["bindung"] = json.dumps(bindung)
        self.bm.free()
        return len(self.knochen)


def flaechen_weiss(obj):
    """Body colour attribute: white, no glow (the body material ignores it, the gear material reads it)"""
    me = obj.data
    if "Farbe" not in me.color_attributes:
        attr = me.color_attributes.new("Farbe", "FLOAT_COLOR", "CORNER")
        werte = np.ones(len(me.loops) * 4, dtype=np.float32)
        werte[3::4] = 0.0
        attr.data.foreach_set("color", werte)


def V(t, spiegeln=False):
    return Vector((t[0], -t[1] if spiegeln else t[1], t[2]))


def punkte(obj):
    co = np.empty(len(obj.data.vertices) * 3)
    obj.data.vertices.foreach_get("co", co)
    return co.reshape(-1, 3)


def aussen(P, mitte, richtung, hoehe, band=0.025):
    """How far the body reaches from mitte in a horizontal direction at a height (for laying things on the surface)"""
    d = P[:, :2] - np.array(mitte[:2])
    r = np.array(richtung[:2]) / max(np.linalg.norm(richtung[:2]), 1e-6)
    entlang = d @ r
    quer = np.abs(d[:, 0] * r[1] - d[:, 1] * r[0])
    m = (np.abs(P[:, 2] - hoehe) < band) & (quer < band * 1.5) & (entlang > 0)
    return float(entlang[m].max()) if m.any() else 0.0


# ---------------------------------------------------------------- Per Kobold

def kolben(teile, g, rnd):
    """Hortwaechter: an ore mace in the right fist, the head hanging forward and down"""
    hg, fi = V(g["handgelenk"], True), V(g["finger"], True)
    griff = hg.lerp(fi, 0.5)
    achse = Vector((0.6, -0.05, -0.8)).normalized()
    hinten = griff - achse * 0.1
    kopf = griff + achse * 0.62
    n = 9
    bahn = [hinten.lerp(kopf, i / (n - 1)) for i in range(n)]
    teile.schlauch(bahn, [0.026, 0.028] + [0.024] * (n - 3) + [0.03], HOLZ, "Ausruestung", "hand_r", seiten=7)
    # Iron bands round the haft and a heavy iron collar under the head
    for f, r, h in ((0.25, 0.033, 0.02), (0.55, 0.031, 0.02), (0.92, 0.045, 0.05)):
        m = hinten.lerp(kopf, f)
        teile.schlauch([m - achse * h, m + achse * h], [r, r], EISEN, "Ausruestung", "hand_r", seiten=8)
    # The head: a fist of Kobaltbluete crystals grown out of a lump of iron
    mitte = kopf + achse * 0.06
    teile.schlauch([kopf, mitte + achse * 0.06], [0.06, 0.05], EISEN, "Ausruestung", "hand_r", seiten=8)
    farbe = ERZE["Kobaltbluete"]
    for i in range(14):
        z = rnd.uniform(-1.0, 1.0)
        w = rnd.uniform(0.0, 2 * math.pi)
        d = Vector((math.sqrt(1 - z * z) * math.cos(w), math.sqrt(1 - z * z) * math.sin(w), z))
        d = (d + achse * 0.8).normalized()
        hell = rnd.uniform(0.75, 1.0)
        teile.kristall(mitte + d * 0.035, d, rnd.uniform(0.07, 0.13), rnd.uniform(0.016, 0.026),
                       (farbe[0] * hell, farbe[1] * hell, farbe[2] * hell, 0.9), "Ausruestung", "hand_r", dreh=rnd.uniform(0, 6.3))
    return griff


def kette(teile, g, P, rnd):
    """Kontoraeltester: a gold chain lying on the robe round the neck, crystal pendants of every ore at the front"""
    hals = V(g["hals"])
    z0 = hals.z - 0.04
    mitte = Vector((hals.x - 0.01, 0.0, z0))
    bahn = []
    for i in range(41):
        w = 2 * math.pi * i / 40
        vorn = max(math.cos(w), 0.0)
        z = z0 - 0.1 * vorn ** 1.5
        richt = (math.cos(w), math.sin(w))
        r = aussen(P, mitte, richt, z) + 0.012
        if r < 0.05:
            r = 0.1
        bahn.append(Vector((mitte.x + r * richt[0], r * richt[1], z)))
    bahn[-1] = bahn[0]
    teile.schlauch(bahn, [0.006] * len(bahn), GOLD, "Ausruestung", "spine_03", seiten=5, deckel=False)
    # Pendants along the front: Kernerz in the middle, the others in a fan
    namen = ["Rippenspat", "Gruenspan", "Blutzinnober", "Salzspat", "Kernerz", "Kobaltbluete", "Siechglas", "Drahtsilber", "Funkenerz"]
    for k, erz in enumerate(namen):
        i = 40 - 4 + k if k < 4 else k - 4      # indices -4 .. 4 round the front (index 0)
        p = bahn[i % 40]
        aus = Vector((p.x - mitte.x, p.y, 0.0)).normalized()
        gross = 1.6 if erz == "Kernerz" else rnd.uniform(0.9, 1.15)
        fuss = p + aus * 0.006 + Vector((0.0, 0.0, -0.006))
        achse = (Vector((0.0, 0.0, -1.0)) + aus * 0.35).normalized()
        c = ERZE[erz]
        # A little gold cap, then the crystal hanging from it
        teile.schlauch([p, fuss], [0.008, 0.009], GOLD, "Ausruestung", "spine_03", seiten=5)
        teile.kristall(fuss, achse, 0.045 * gross, 0.012 * gross, (c[0], c[1], c[2], 1.0 if erz == "Kernerz" else 0.7),
                       "Ausruestung", "spine_03", seiten=6, dreh=rnd.uniform(0, 6.3))


PEITSCHE_GLIEDER = 10
PEITSCHE_LAENGE = 2.1


def peitsche(teile, g, P, rnd):
    """Sklaventreiber: the whip coiled at the right hip, and the same whip in the right hand on a bone chain.
    Returns the bones of the chain for the skeleton plan"""
    # At the hip: three loops of plaited leather, on the outside of the right thigh, the handle sticking up
    huefte = V(g["huefte"], True)
    z = huefte.z - 0.06
    y = -(aussen(P, (huefte.x, 0.0, z), (0.0, -1.0), z, 0.03) + 0.018)
    mitte = Vector((huefte.x + 0.02, y, z))
    bahn = []
    for i in range(3 * 16 + 1):
        w = 2 * math.pi * i / 16
        r = 0.085 + 0.008 * (i / 16.0)
        bahn.append(mitte + Vector((r * math.cos(w), -0.012 * i / 16.0, r * math.sin(w) * 1.15)))
    teile.schlauch(bahn, [0.011] * len(bahn), LEDER, "Peitsche_Huefte", "pelvis", seiten=5)
    oben = mitte + Vector((0.03, -0.01, 0.1))
    teile.schlauch([oben, oben + Vector((0.02, 0.0, 0.2))], [0.018, 0.016], HOLZ, "Peitsche_Huefte", "pelvis", seiten=6)
    teile.schlauch([oben + Vector((0.02, 0.0, 0.2)), oben + Vector((0.022, 0.0, 0.23))], [0.024, 0.02], EISEN, "Peitsche_Huefte", "pelvis", seiten=6)

    # In the hand: the haft in the fist, the lash hanging forward and down along a chain of bones
    hg, fi = V(g["handgelenk"], True), V(g["finger"], True)
    griff = hg.lerp(fi, 0.5)
    achse = Vector((0.75, 0.0, -0.66)).normalized()
    stiel_a, stiel_b = griff - achse * 0.06, griff + achse * 0.2
    teile.schlauch([stiel_a, stiel_b], [0.019, 0.016], HOLZ, "Peitsche_Hand", "hand_r", seiten=6)
    teile.schlauch([stiel_a - achse * 0.02, stiel_a + achse * 0.015], [0.024, 0.024], EISEN, "Peitsche_Hand", "hand_r", seiten=6)
    knochen = []
    glied = PEITSCHE_LAENGE / PEITSCHE_GLIEDER
    for i in range(PEITSCHE_GLIEDER):
        knochen.append(("peitsche_%d" % (i + 1), stiel_b + achse * (glied * i), stiel_b + achse * (glied * (i + 1)),
                        "hand_r" if i == 0 else "peitsche_%d" % i))
    # Lash: 4 rings per bone, each point weighted between its bone and the next by where it lies
    n = PEITSCHE_GLIEDER * 4 + 1
    bahn = [stiel_b + achse * (PEITSCHE_LAENGE * i / (n - 1)) for i in range(n)]
    radien = [0.013 * (1 - i / (n - 1)) + 0.0025 for i in range(n)]

    def gewicht(i):
        t = i / 4.0 - 0.5                      # bone index the ring's middle sits in
        a = int(math.floor(t))
        f = t - a
        a = max(a, 0)
        b = min(a + 1, PEITSCHE_GLIEDER - 1)
        if t < 0:
            return [("hand_r", 0.5), ("peitsche_1", 0.5)]
        return [("peitsche_%d" % (a + 1), 1.0 - f), ("peitsche_%d" % (b + 1), f)] if a != b else [("peitsche_%d" % (a + 1), 1.0)]
    teile.schlauch(bahn, radien, LEDER, "Peitsche_Hand", gewicht, seiten=5)
    # A knot of ore wire at the tip (the "Erzdraht" of the compendium)
    spitze = bahn[-1]
    teile.schlauch([spitze - achse * 0.05, spitze + achse * 0.02], [0.012, 0.008], (0.5, 0.52, 0.55, 0.15), "Peitsche_Hand",
                   "peitsche_%d" % PEITSCHE_GLIEDER, seiten=5)
    return knochen


def anbauen(obj, basis, g, saat=0):
    """Builds the gear of a Kobold type into its (already bent) body. Returns extra bones for the skeleton plan:
    list of (name, head, tail, parent)"""
    rnd = random.Random(1000 + saat)
    flaechen_weiss(obj)
    P = punkte(obj)
    teile = Teile()
    extra = []
    if basis == "Hortwaechter":
        kolben(teile, g, rnd)
    # Kontoraeltester: his own collar gems are recoloured in the ore colours instead (kobold_variante.UMFAERBEN); the extra chain
    # (kette) hung like a second necklace over the robe and changed the design too much
    elif basis == "Sklaventreiber":
        extra = peitsche(teile, g, P, rnd)
    else:
        teile.bm.free()
        return extra
    n = teile.einfuegen(obj)
    obj["extra_knochen"] = json.dumps([[k, list(a), list(b), e] for k, a, b, e in extra])
    print("[%s] Ausruestung: %d Punkte, Slots %s" % (obj.name, n, [m.name for m in obj.data.materials]))
    return extra
