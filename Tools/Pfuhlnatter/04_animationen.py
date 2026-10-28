"""Step 4: the Pfuhlnatter animations as separate actions (30 frames per second).

  Kriechen     1.2 s loop: lying flat, lateral waves running from the head to the tail, head a little raised,
               all fins folded flat against the body
  Schwimmen    1.8 s loop: longer, slower waves, head higher, fins folded, the tail fan stirring
  Aufrichten   1.0 s: from crawling the front rears up into the Tripo pose; the fins rise last, trembling
  Kampf        2.0 s loop: reared, the neck swaying, the fins raised and quivering
  Spucken      1.0 s: the head draws back, then thrusts forward (the spit leaves at 0.55 s), fins flare
  Biss         0.8 s: a lunge of the neck (strike at 0.4 s)
  Ablegen      0.8 s: from reared back down to crawling, the fins fold first
  Tod          1.5 s: collapses onto the ground and curls a little, fins half folded

The body is posed by shaping its centre line: for every frame a target line is built (waves along the ground, or
the rest pose) and each spine bone is turned onto it. Fins fold by a turn of up to 50 degrees and by being drawn in
(see 03_skelett.py). Renders key poses to vorschau/04_*. Saves arbeit/04_animationen.blend.

    blender -b --factory-startup -P Tools/Pfuhlnatter/04_animationen.py
"""
import json
import math
import os
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import vorschau  # noqa: E402

FPS = 30
KOPF_X_KRIECHEN = 1.9     # m: where the head lies while crawling (the actor's origin stays at 0)


def glatt01(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


class Natter:
    def __init__(self):
        self.arm = bpy.data.objects["Pfuhlnatter_Skelett"]
        with open(os.path.join(vorschau.ARBEIT, "skelett.json"), encoding="utf-8") as f:
            self.daten = json.load(f)
        self.n = self.daten["rumpf_knochen"]
        self.rumpf = ["rumpf_%02d" % (i + 1) for i in range(self.n)]
        bones = self.arm.data.bones
        self.laengen = [bones[b].length for b in self.rumpf]
        self.kopf_laenge = bones["kopf"].length
        self.radien = self.daten["radien"]
        self.flossen = self.daten["flossen"]
        self.groesse = Vector(self.daten["falten_groesse"])
        # Fold turn of each fin, local to its bone
        self.falten = {}
        for fl in self.flossen:
            rest = bones[fl["name"]].matrix_local.to_3x3()
            richtung = (Vector(fl["spitze"]) - Vector(fl["basis"])).normalized()
            welt = richtung.rotation_difference(Vector(fl["gefaltet"])).to_matrix()
            self.falten[fl["name"]] = (rest.inverted() @ welt @ rest).to_quaternion()

    def radius(self, s_von_schwanz):
        r = self.radien
        x = max(0.0, min(1.0, s_von_schwanz)) * (len(r) - 1)
        i = min(int(x), len(r) - 2)
        return r[i] + (r[i + 1] - r[i]) * (x - i)

    # ------------------------------------------------------------ Body shapes

    def wellen_linie(self, phase, amplitude_grad, wellenlaenge, kopf_hoehe, kopf_x=KOPF_X_KRIECHEN, kruemmung=0.0):
        """Joints from the neck top back to the tail root, lying on the ground with lateral waves (world, m)"""
        gesamt = sum(self.laengen)
        punkte = [Vector((kopf_x, 0.0, 0.0))]
        s = 0.0
        richtungen = []
        for i in reversed(range(self.n)):
            ds = self.laengen[i]
            mitte = s + ds / 2
            # Waves grow from the head backwards, a little smaller again at the tail
            huelle = min(1.0, 0.35 + mitte / 1.2) * (1.0 - 0.25 * max(0.0, (mitte - gesamt * 0.75) / (gesamt * 0.25)))
            winkel = math.radians(amplitude_grad) * huelle * math.sin(2 * math.pi * (mitte / wellenlaenge) - phase)
            winkel += kruemmung * mitte / gesamt
            richtungen.append(Vector((-math.cos(winkel), math.sin(winkel), 0.0)))
            s += ds
        for i, rr in enumerate(richtungen):
            punkte.append(punkte[-1] + rr * self.laengen[self.n - 1 - i])
        # Heights: belly on the ground, the head end raised and easing down over the first 0.8 m
        s = 0.0
        hoehen = []
        for k in range(len(punkte)):
            s_schwanz = 1.0 - s / gesamt
            anhebung = kopf_hoehe * (1.0 - glatt01(s / 0.9))
            hoehen.append(self.radius(s_schwanz) * 0.85 + anhebung)
            if k < self.n:
                s += self.laengen[self.n - 1 - k]
        # Keep each bone its length after lifting: step again along the lifted directions
        fertig = [Vector((punkte[0].x, punkte[0].y, hoehen[0]))]
        for k in range(1, len(punkte)):
            soll = Vector((punkte[k].x, punkte[k].y, hoehen[k]))
            d = (soll - fertig[-1]).normalized()
            fertig.append(fertig[-1] + d * self.laengen[self.n - k])
        return list(reversed(fertig))  # tail root .. neck top

    # ------------------------------------------------------------ Posing

    def koerper_pose(self, gelenke, kopf_richtung):
        """Local rotations (and the first bone's offset) that lay the spine onto the joints"""
        bones = self.arm.data.bones
        welt = {"wurzel": bones["wurzel"].matrix_local.copy()}
        pose = {}
        oben = Vector((0.0, 0.0, 1.0))
        for i, name in enumerate(self.rumpf + ["kopf"]):
            kopf = gelenke[i]
            y = (gelenke[i + 1] - kopf).normalized() if i < self.n else kopf_richtung.normalized()
            z = (oben - y * oben.dot(y)).normalized()
            oben = z
            x = y.cross(z)
            soll = Matrix((x, y, z)).transposed().to_4x4()
            soll.translation = kopf
            b = bones[name]
            eltern = b.parent
            rel = eltern.matrix_local.inverted() @ b.matrix_local
            basis = (welt[eltern.name] @ rel).inverted() @ soll
            pose[name] = (basis.to_quaternion(), basis.translation.copy() if i == 0 else Vector())
            welt[name] = soll
        return pose

    def ruhe_pose(self):
        return {name: (Quaternion(), Vector()) for name in self.rumpf + ["kopf"]}

    def drehung_welt(self, name, achse, grad):
        rest = self.arm.data.bones[name].matrix_local.to_3x3()
        return (rest.inverted() @ Matrix.Rotation(math.radians(grad), 3, achse) @ rest).to_quaternion()

    def setzen(self, pose, falten=0.0, zittern=0.0, zeit=0.0, flossen_groesse=1.0, schwanz_falten=None):
        pb = self.arm.pose.bones
        for name, (q, ort) in pose.items():
            pb[name].rotation_mode = "QUATERNION"
            pb[name].rotation_quaternion = q
            pb[name].location = ort
        for i, fl in enumerate(self.flossen):
            name = fl["name"]
            f = falten
            if schwanz_falten is not None and fl["eltern"] in ("rumpf_01", "rumpf_02"):
                f = schwanz_falten
            q = Quaternion().slerp(self.falten[name], f)
            if zittern > 0:
                # Quivering: each fin its own fast, uneven shake
                a = math.radians(zittern) * (math.sin(zeit * 2 * math.pi * 13 + i * 1.7) + 0.5 * math.sin(zeit * 2 * math.pi * 21 + i))
                q = q @ Quaternion(Vector((1, 0, 0)), a) @ Quaternion(Vector((0, 0, 1)), a * 0.6)
            g = Vector((1, 1, 1)).lerp(self.groesse, f) * flossen_groesse
            pb[name].rotation_mode = "QUATERNION"
            pb[name].rotation_quaternion = q
            pb[name].scale = g

    def keyen(self, rahmen):
        for pb in self.arm.pose.bones:
            if pb.name == "wurzel":
                continue
            pb.keyframe_insert("rotation_quaternion", frame=rahmen)
            pb.keyframe_insert("location", frame=rahmen)
            pb.keyframe_insert("scale", frame=rahmen)


def mischen(a, b, t):
    return {n: (a[n][0].slerp(b[n][0], t), a[n][1].lerp(b[n][1], t)) for n in a}


def neue_aktion(natter, name):
    aktion = bpy.data.actions.new(name)
    aktion.use_fake_user = True
    natter.arm.animation_data_create()
    natter.arm.animation_data.action = aktion
    return aktion


def main():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(vorschau.ARBEIT, "03_skelett.blend"))
    natter = Natter()
    vorn = Vector((1.0, 0.0, -0.12))

    def kriechen_pose(phase, amplitude=34.0, wellenlaenge=1.9, hoehe=0.22):
        return natter.koerper_pose(natter.wellen_linie(phase, amplitude, wellenlaenge, hoehe), vorn)

    # Kriechen: one wave length per loop, travelling backwards
    neue_aktion(natter, "Kriechen")
    n = int(1.2 * FPS)
    for f in range(n + 1):
        natter.setzen(kriechen_pose(2 * math.pi * f / n), falten=1.0)
        natter.keyen(f)

    neue_aktion(natter, "Schwimmen")
    n = int(1.8 * FPS)
    for f in range(n + 1):
        t = f / FPS
        pose = natter.koerper_pose(natter.wellen_linie(2 * math.pi * f / n, 30.0, 2.4, 0.36), Vector((1.0, 0.0, 0.0)))
        natter.setzen(pose, falten=1.0, schwanz_falten=0.75 + 0.2 * math.sin(2 * math.pi * f / n), zeit=t)
        natter.keyen(f)

    ruhe = natter.ruhe_pose()
    liegend = kriechen_pose(0.0)

    neue_aktion(natter, "Aufrichten")
    for f in range(int(1.0 * FPS) + 1):
        t = f / FPS
        u = glatt01(t / 0.8)
        aufstellen = glatt01((t - 0.4) / 0.5)
        zittern = 7.0 * aufstellen * (1.0 - 0.6 * glatt01((t - 0.7) / 0.3))
        natter.setzen(mischen(liegend, ruhe, u), falten=1.0 - aufstellen, zittern=zittern, zeit=t,
                      flossen_groesse=1.0 + 0.08 * math.sin(math.pi * aufstellen))
        natter.keyen(f)

    hals = natter.rumpf[-9:] + ["kopf"]

    neue_aktion(natter, "Kampf")
    n = int(2.0 * FPS)
    for f in range(n + 1):
        t = f / FPS
        w = 2 * math.pi * f / n
        pose = dict(ruhe)
        for k, name in enumerate(hals):
            anteil = (k + 1) / len(hals)
            q = natter.drehung_welt(name, "Z", 5.0 * anteil * math.sin(w + k * 0.25)) @ \
                natter.drehung_welt(name, "Y", 2.0 * anteil * math.sin(2 * w + 0.6))
            pose[name] = (q, Vector())
        natter.setzen(pose, falten=0.0, zittern=2.0, zeit=t, flossen_groesse=1.0 + 0.03 * math.sin(4 * w))
        natter.keyen(f)

    def ruck(name_aktion, dauer, verlauf, flare):
        neue_aktion(natter, name_aktion)
        for f in range(int(dauer * FPS) + 1):
            t = f / FPS
            grad = verlauf(t)
            pose = dict(ruhe)
            for k, name in enumerate(hals):
                anteil = (k + 1) / len(hals)
                pose[name] = (natter.drehung_welt(name, "Y", grad * anteil), Vector())
            natter.setzen(pose, falten=0.0, zittern=3.0 * flare(t), zeit=t, flossen_groesse=1.0 + 0.12 * flare(t))
            natter.keyen(f)

    # Positive turns about Y pitch the head down and forward, negative draw it back and up
    ruck("Spucken", 1.0,
         lambda t: -9.0 * glatt01(t / 0.45) + 21.0 * glatt01((t - 0.45) / 0.12) - 12.0 * glatt01((t - 0.65) / 0.35),
         lambda t: glatt01((t - 0.4) / 0.15) * (1.0 - glatt01((t - 0.7) / 0.3)))
    ruck("Biss", 0.8,
         lambda t: -7.0 * glatt01(t / 0.3) + 26.0 * glatt01((t - 0.3) / 0.1) - 19.0 * glatt01((t - 0.45) / 0.35),
         lambda t: glatt01((t - 0.25) / 0.1) * (1.0 - glatt01((t - 0.5) / 0.3)))

    neue_aktion(natter, "Ablegen")
    for f in range(int(0.8 * FPS) + 1):
        t = f / FPS
        natter.setzen(mischen(ruhe, liegend, glatt01((t - 0.1) / 0.7)), falten=glatt01(t / 0.4))
        natter.keyen(f)

    tot = natter.koerper_pose(natter.wellen_linie(0.0, 10.0, 3.0, -0.05, kruemmung=1.4), Vector((1.0, 0.3, -0.5)))
    neue_aktion(natter, "Tod")
    for f in range(int(1.5 * FPS) + 1):
        t = f / FPS
        natter.setzen(mischen(ruhe, tot, glatt01(t / 1.1)), falten=0.6 * glatt01(t / 0.8))
        natter.keyen(f)

    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(vorschau.ARBEIT, "04_animationen.blend"))

    # Key poses for checking
    for aktion, rahmen in (("Kriechen", 0), ("Schwimmen", 20), ("Aufrichten", 18), ("Kampf", 0), ("Spucken", 17), ("Tod", 45)):
        natter.arm.animation_data.action = bpy.data.actions[aktion]
        bpy.context.scene.frame_set(rahmen)
        vorschau.rendern("04_%s_spielkamera" % aktion.lower(), (0.45, -0.8, math.tan(math.radians(50))), (0.0, 0.0, 0.6), 5.4)
    natter.arm.animation_data.action = bpy.data.actions["Kriechen"]
    bpy.context.scene.frame_set(0)
    vorschau.rendern("04_kriechen_oben", (0.0, 0.0, 1.0), (-0.6, 0.0, 0.3), 6.2)
    print("[Pfuhlnatter] Aktionen:", [a.name for a in bpy.data.actions])


main()
