"""Skeleton, weights, poses, baking, check renders and export for the night creatures (see gemein.py for the mesh
and render basics)."""
import glob
import math
import os
import shutil

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

from gemein import ANSICHTEN, FPS, log, oeffnen, ordner, punkte, rendern


# ---------------------------------------------------------------- Skeleton and weights

def K(k, e, eltern=None, gewicht=True, maske=None, radius=1.0, rolle=None):
    """One bone of a plan: head, tail, parent; maske(P) -> bool array limits which points it may move"""
    return dict(k=Vector(k), e=Vector(e), eltern=eltern, gewicht=gewicht, maske=maske, radius=radius, rolle=rolle)


def kette(plan, stamm, bahn, eltern, **opt):
    """A chain of bones through the points, named <stamm>_1 ..; returns the names"""
    namen = []
    for i in range(len(bahn) - 1):
        n = "%s_%d" % (stamm, i + 1)
        plan[n] = K(bahn[i], bahn[i + 1], eltern if i == 0 else namen[-1], **opt)
        namen.append(n)
    return namen


def skelett_bauen(name, plan):
    data = bpy.data.armatures.new(name + "_Skelett")
    arm = bpy.data.objects.new(name + "_Skelett", data)
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for n, b in plan.items():
        eb = data.edit_bones.new(n)
        eb.head, eb.tail = b["k"], b["e"]
        if b["rolle"] is not None:
            eb.align_roll(Vector(b["rolle"]))
        else:
            eb.align_roll(Vector((0, 0, 1)) if abs((eb.tail - eb.head).normalized().z) < 0.9 else Vector((1, 0, 0)))
    for n, b in plan.items():
        if b["eltern"]:
            data.edit_bones[n].parent = data.edit_bones[b["eltern"]]
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    return arm


def abstand_strecke(P, a, b):
    a, b = np.array(a), np.array(b)
    ab = b - a
    t = np.clip(((P - a) @ ab) / max(ab @ ab, 1e-12), 0.0, 1.0)
    return np.linalg.norm(P - (a + t[:, None] * ab), axis=1)


def kanten(obj):
    k = np.empty(len(obj.data.edges) * 2, dtype=np.int64)
    obj.data.edges.foreach_get("vertices", k)
    return k.reshape(-1, 2)


def nachbarn_mitteln(obj, w, runden):
    """Smooths weights over the mesh edges"""
    n = len(obj.data.vertices)
    k = kanten(obj)
    grad = np.zeros(n)
    np.add.at(grad, k[:, 0], 1)
    np.add.at(grad, k[:, 1], 1)
    grad = np.maximum(grad, 1)[:, None]
    for _ in range(runden):
        s = np.zeros_like(w)
        np.add.at(s, k[:, 0], w[k[:, 1]])
        np.add.at(s, k[:, 1], w[k[:, 0]])
        w = 0.5 * w + 0.5 * s / grad
    return w


def inseln(obj):
    """Label of the connected piece each point belongs to"""
    n = len(obj.data.vertices)
    eltern = np.arange(n)

    def finde(i):
        while eltern[i] != i:
            eltern[i] = eltern[eltern[i]]
            i = eltern[i]
        return i
    for a, b in kanten(obj):
        ra, rb = finde(a), finde(b)
        if ra != rb:
            eltern[ra] = rb
    return np.array([finde(i) for i in range(n)])


def gewichte_rechnen(obj, plan, schaerfe=0.22, glatt=3):
    """Distance weights: every point belongs to the nearest bone it is allowed to, blending only near joints.
    Returns (bone names, weights array points x bones)"""
    P = punkte(obj)
    namen = [n for n, b in plan.items() if b["gewicht"]]
    d = np.full((len(P), len(namen)), 1e9)
    masken = {}
    for j, n in enumerate(namen):
        b = plan[n]
        dj = abstand_strecke(P, b["k"], b["e"]) / b["radius"]
        if b["maske"] is not None:
            m = b["maske"](P)
            masken[j] = m
            dj = np.where(m, dj, 1e9)
        d[:, j] = dj
    dmin = d.min(axis=1, keepdims=True)
    w = np.exp(-(d - dmin) / (schaerfe * dmin + 0.004))
    w = nachbarn_mitteln(obj, w, glatt)
    for j, m in masken.items():
        w[~m, j] = 0.0
    w /= np.maximum(w.sum(axis=1, keepdims=True), 1e-9)
    return namen, w


def gewichte_schreiben(obj, arm, namen, w):
    """Keeps the 4 strongest per point, normalised, as vertex groups, and binds the mesh to the skeleton"""
    obj.vertex_groups.clear()
    gruppen = [obj.vertex_groups.new(name=n) for n in namen]
    for b in arm.data.bones:
        if b.name not in namen:
            obj.vertex_groups.new(name=b.name)
    staerkste = np.argsort(-w, axis=1)[:, :4]
    for i in range(len(w)):
        gw = w[i, staerkste[i]]
        gw = gw / max(gw.sum(), 1e-9)
        for g, x in zip(staerkste[i], gw):
            if x > 0.002:
                gruppen[g].add([i], float(x), "REPLACE")
    for m in list(obj.modifiers):
        obj.modifiers.remove(m)
    mod = obj.modifiers.new("Skelett", "ARMATURE")
    mod.object = arm
    obj.parent = arm


def uv_bei(obj, ort):
    """UV coordinate of the mesh point nearest to a place (to give added geometry that place's colour)"""
    P = punkte(obj)
    i = int(np.argmin(np.linalg.norm(P - np.array(ort), axis=1)))
    for loop in obj.data.loops:
        if loop.vertex_index == i:
            return tuple(obj.data.uv_layers.active.data[loop.index].uv)
    return (0.5, 0.5)


def schlauch_anfuegen(obj, bahn, radien, uv, seiten=5):
    """Adds a closed tube along the points (a filament) to the mesh, all in one colour (uv).
    Returns the indices of its new points"""
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    uvl = bm.loops.layers.uv.active
    start = len(bm.verts)
    ringe = []
    for i, (p, r) in enumerate(zip(bahn, radien)):
        p = Vector(p)
        t = (Vector(bahn[min(i + 1, len(bahn) - 1)]) - Vector(bahn[max(i - 1, 0)])).normalized()
        a = t.cross(Vector((1, 0, 0)))
        if a.length < 0.1:
            a = t.cross(Vector((0, 1, 0)))
        a.normalize()
        b = t.cross(a)
        ringe.append([bm.verts.new(p + (a * math.cos(2 * math.pi * k / seiten) + b * math.sin(2 * math.pi * k / seiten)) * r)
                      for k in range(seiten)])
    neu = []
    for i in range(len(ringe) - 1):
        for k in range(seiten):
            neu.append(bm.faces.new((ringe[i][k], ringe[i][(k + 1) % seiten], ringe[i + 1][(k + 1) % seiten], ringe[i + 1][k])))
    neu.append(bm.faces.new(ringe[0][::-1]))
    neu.append(bm.faces.new(ringe[-1]))
    for f in neu:
        f.smooth = True
        for loop in f.loops:
            loop[uvl].uv = uv
    anzahl = len(bm.verts) - start
    bm.to_mesh(obj.data)
    bm.free()
    return list(range(start, start + anzahl))


# ---------------------------------------------------------------- Poses

def weich(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def huegel(t, a, b, c, d):
    """0 before a, rising to 1 at b, 1 until c, falling to 0 at d"""
    return weich((t - a) / max(b - a, 1e-6)) * (1 - weich((t - c) / max(d - c, 1e-6)))


def rampe(t, a, b):
    return weich((t - a) / max(b - a, 1e-6))


def welle(t, periode, phase=0.0):
    return math.sin(2 * math.pi * (t / periode + phase))


def unruhe(t, schleife, saat=0.0):
    """A soft uneven wobble -1..1 that closes over the loop (three sines on whole beats of the loop)"""
    return (0.55 * math.sin(2 * math.pi * (1 * t / schleife) + 1.3 + saat * 2.1)
            + 0.3 * math.sin(2 * math.pi * (2 * t / schleife) + 4.0 + saat * 3.7)
            + 0.15 * math.sin(2 * math.pi * (3 * t / schleife) + 2.2 + saat * 5.3))


class Pose:
    """One frame: bone angles about the world axes at rest (beuge about Y: a bone along +X tips its end down, a bone
    hanging down swings its end back to -X; seite about X; wende about Z), offsets in metres (world axes) and
    two-bone IK jobs"""

    def __init__(self):
        self.winkel = {}
        self.versatz = {}
        self.ik = []
        self.skala = {}

    def dazu(self, name, beuge=0.0, seite=0.0, wende=0.0):
        alt = self.winkel.get(name, (0.0, 0.0, 0.0))
        self.winkel[name] = (alt[0] + beuge, alt[1] + seite, alt[2] + wende)

    def schieben(self, name, x=0.0, y=0.0, z=0.0):
        self.versatz[name] = self.versatz.get(name, Vector()) + Vector((x, y, z))

    def greifen(self, oben, unten, ziel, pol, ende=None, ende_winkel=(0.0, 0.0, 0.0)):
        """Two-bone IK: the tail of <unten> goes to ziel (armature space), the joint bends towards pol (a direction);
        <ende> (hand, foot) keeps its rest turn plus ende_winkel (world axes)"""
        self.ik.append((oben, unten, Vector(ziel), Vector(pol), ende, ende_winkel))


def _welt(winkel):
    beuge, seite, wende = winkel
    return Matrix.Rotation(math.radians(wende), 3, "Z") @ Matrix.Rotation(math.radians(seite), 3, "X") @ \
        Matrix.Rotation(math.radians(beuge), 3, "Y")


def _ausrichten(pb, kopf, richtung):
    m3 = pb.matrix.to_3x3()
    q = m3.col[1].normalized().rotation_difference(richtung.normalized())
    m = (q.to_matrix() @ m3).to_4x4()
    m.translation = kopf
    pb.matrix = m


def pose_setzen(arm, p):
    for pb in arm.pose.bones:
        rest = arm.data.bones[pb.name].matrix_local.to_3x3()
        w = p.winkel.get(pb.name)
        if w is None:
            pb.rotation_quaternion = Quaternion()
        elif isinstance(w, Matrix):
            pb.rotation_quaternion = (rest.inverted() @ w @ rest).to_quaternion()
        elif isinstance(w, Quaternion):
            pb.rotation_quaternion = w
        else:
            pb.rotation_quaternion = (rest.inverted() @ _welt(w) @ rest).to_quaternion()
        v = p.versatz.get(pb.name)
        pb.location = (rest.inverted() @ v) if v is not None else (0, 0, 0)
        s = p.skala.get(pb.name)
        pb.scale = s if s is not None else (1, 1, 1)
    lage = bpy.context.view_layer
    for oben, unten, ziel, pol, ende, ende_winkel in p.ik:
        lage.update()
        po, pu = arm.pose.bones[oben], arm.pose.bones[unten]
        a = po.head.copy()
        l1, l2 = arm.data.bones[oben].length, arm.data.bones[unten].length
        zu = ziel - a
        dist = min(max(zu.length, abs(l1 - l2) + 1e-4), l1 + l2 - 1e-4)
        richt = zu.normalized()
        quer = pol - richt * pol.dot(richt)
        quer = quer.normalized() if quer.length > 1e-6 else Vector((0, 0, 1))
        c = (l1 * l1 + dist * dist - l2 * l2) / (2 * l1 * dist)
        knie = a + richt * (l1 * c) + quer * (l1 * math.sqrt(max(0.0, 1 - c * c)))
        _ausrichten(po, a, knie - a)
        lage.update()
        _ausrichten(pu, pu.head.copy(), (a + richt * dist) - pu.head)
        if ende:
            lage.update()
            pe = arm.pose.bones[ende]
            rest = arm.data.bones[ende].matrix_local.to_3x3()
            m = (_welt(ende_winkel) @ rest).to_4x4()
            m.translation = pe.head
            pe.matrix = m
    lage.update()


def backen(name, arm, aktion, dauer, pose_bei, schleife):
    """Sets every frame, reads the bones' local transforms and keys them into an action"""
    rahmen = int(round(dauer * FPS))
    act = bpy.data.actions.get(aktion)
    if act:
        bpy.data.actions.remove(act)
    arm.animation_data_create()
    arm.animation_data.action = None
    werte = []
    for f in range(rahmen + 1):
        pose_setzen(arm, pose_bei(f / FPS))
        bild = {}
        for pb in arm.pose.bones:
            q = pb.rotation_quaternion.copy()
            if werte and werte[-1][pb.name][1].dot(q) < 0:
                q.negate()
            bild[pb.name] = (pb.location.copy(), q, pb.scale.copy())
        werte.append(bild)
    if schleife:
        # how far the step over the seam is from an ordinary step (a jolt at the loop's end shows here)
        def schritt(a, b):
            return max(math.degrees(a[n][1].rotation_difference(b[n][1]).angle) for n in a)
        naht = schritt(werte[-2], werte[0])
        sonst = max(schritt(werte[i], werte[i + 1]) for i in range(rahmen - 1))
        log(name, "  Naht %s: %.2f Grad (groesster Schritt sonst %.2f)%s" % (
            aktion, naht, sonst, "  <-- RUCK" if naht > sonst * 1.6 + 0.3 else ""))
        werte[-1] = werte[0]
    act = bpy.data.actions.new(aktion)
    act.use_fake_user = True
    arm.animation_data.action = act
    skaliert = any(any(abs(c - 1) > 1e-4 for c in bild[n][2]) for bild in werte for n in bild)
    for f, bild in enumerate(werte):
        for pb in arm.pose.bones:
            ort, quat, skala = bild[pb.name]
            pb.location, pb.rotation_quaternion, pb.scale = ort, quat, skala
            pb.keyframe_insert("rotation_quaternion", frame=f, group=pb.name)
            pb.keyframe_insert("location", frame=f, group=pb.name)
            if skaliert:
                pb.keyframe_insert("scale", frame=f, group=pb.name)
    act.frame_range = (0, rahmen - 1 if schleife else rahmen)
    act.use_frame_range = True
    act.use_cyclic = schleife
    arm.animation_data.action = None
    log(name, "Aktion %s: %d Bilder (%.2f s)%s" % (aktion, rahmen, rahmen / FPS, ", Schleife" if schleife else ""))
    return act


def ruhe(arm):
    if arm.animation_data:
        arm.animation_data.action = None
    for pb in arm.pose.bones:
        pb.location = (0, 0, 0)
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.scale = (1, 1, 1)


def aktion_zeigen(arm, aktion):
    act = bpy.data.actions[aktion]
    arm.animation_data_create()
    arm.animation_data.action = act
    if hasattr(arm.animation_data, "action_slot") and getattr(act, "slots", None):
        arm.animation_data.action_slot = act.slots[0]


def clip_pruefen(name, arm, aktion, bilder, mitte, groesse, welche=("vorne", "hinten", "seite", "spiel"), aufloesung=300):
    """Renders frames of a clip from every side into vorschau/clips/<Aktion>_<Ansicht>_<Bild>.png"""
    aktion_zeigen(arm, aktion)
    for f in bilder:
        bpy.context.scene.frame_set(f)
        for a in welche:
            rendern(ordner(name, "vorschau", "clips", "%s_%s_%03d.png" % (aktion, a, f)), ANSICHTEN[a], mitte, groesse, aufloesung)
    arm.animation_data.action = None


def pose_bild(name, arm, praefix, p, mitte, groesse, welche=("vorne", "seite", "spiel"), aufloesung=500):
    """Renders one pose (for weight tests)"""
    pose_setzen(arm, p)
    bpy.context.view_layer.update()
    return [rendern(ordner(name, "vorschau", "%s_%s.png" % (praefix, a)), ANSICHTEN[a], mitte, groesse, aufloesung)
            for a in welche]


def knochen_json(name, plan):
    """Writes the joints to arbeit/knochen.json, for drawing them over the measuring pictures"""
    import json
    with open(ordner(name, "arbeit", "knochen.json"), "w", encoding="utf-8") as f:
        json.dump({n: [list(b["k"]), list(b["e"])] for n, b in plan.items()}, f)


# ---------------------------------------------------------------- Export

def exportieren(name):
    """export/SK_<Name>.fbx with skeleton, mesh and every action, plus the Tripo textures as T_<Name>_<art>"""
    obj, arm = oeffnen(name, "03_animationen.blend")
    os.makedirs(ordner(name, "export"), exist_ok=True)
    arm.name = "Armature"
    obj.name = "SK_" + name
    ruhe(arm)
    for datei in glob.glob(ordner(name, "02_tripo", "*.fbm", "*")):
        klein = os.path.basename(datei).lower()
        art = next((a for a in ("basecolor", "normal", "roughness", "metallic") if a in klein), None)
        if art:
            shutil.copyfile(datei, ordner(name, "export", "T_%s_%s%s" % (name, art, ".png" if klein.endswith(".png") else ".jpg")))
    for o in bpy.context.scene.objects:
        o.select_set(o in (arm, obj))
    bpy.context.view_layer.objects.active = arm
    ziel = ordner(name, "export", "SK_%s.fbx" % name)
    bpy.ops.export_scene.fbx(
        filepath=ziel, use_selection=True, object_types={"ARMATURE", "MESH"},
        apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
        add_leaf_bones=False, mesh_smooth_type="FACE", path_mode="STRIP", embed_textures=False, colors_type="LINEAR",
        bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
        bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0,
    )
    log(name, "exportiert:", ziel, "Aktionen:", [a.name for a in bpy.data.actions], "Knochen:", len(arm.data.bones))
