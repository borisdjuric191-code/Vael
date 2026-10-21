"""Step 2: quadruped skeleton for the Rankenluchs, weights, and test poses.

Bones (German names, all lower case):
  wurzel (root on the ground) > becken > wirbel_01..03 > hals > kopf > kiefer, ohr_l_01/02, ohr_r_01/02
  becken > schweif_01..10 (the long tail, measured along its centre line)
  per leg (vl, vr = front, hl, hr = hind): bein_XX_oben > bein_XX_unten > bein_XX_mittelfuss > bein_XX_pfote
    (front legs hang from wirbel_03, hind legs from becken)

Weights: Blender's automatic (heat) weights fail on the open Tripo mesh with its leaves and bark plates, so they are
computed on a closed voxel copy of the body and carried over to the real mesh by nearest surface.
Checks: renders with bent legs, a curled tail, lowered head (vorschau/02_*) and a picture of the bones in the body.
Saves arbeit/02_skelett.blend.

    blender -b --factory-startup -P skripte/02_skelett.py
"""
import json
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import vorschau  # noqa: E402

ORDNER = vorschau.ORDNER
SCHWEIF_KNOCHEN = 10


def v(*a):
    return Vector(a)


def mitte(pts):
    return sum(pts, Vector()) / len(pts) if pts else None


def schweif_linie(P):
    """Centre line of the tail from the rump to the tip, as SCHWEIF_KNOCHEN + 1 points"""
    lo_x = min(p.x for p in P)
    start_x = -0.47
    roh = []
    n = 24
    for k in range(n + 1):
        x = start_x + (lo_x + 0.015 - start_x) * k / n
        s = [p for p in P if abs(p.x - x) < 0.012 and p.z > 0.1 and (x < -0.5 or p.z > 0.5)]
        m = mitte(s)
        if m:
            roh.append(Vector((x, m.y, m.z)))
    # Evenly spaced along the path
    laengen = [0.0]
    for a, b in zip(roh, roh[1:]):
        laengen.append(laengen[-1] + (b - a).length)
    gesamt = laengen[-1]
    punkte = []
    for i in range(SCHWEIF_KNOCHEN + 1):
        ziel = gesamt * i / SCHWEIF_KNOCHEN
        j = max(k for k in range(len(laengen)) if laengen[k] <= ziel + 1e-9)
        j = min(j, len(roh) - 2)
        t = (ziel - laengen[j]) / max(laengen[j + 1] - laengen[j], 1e-6)
        punkte.append(roh[j].lerp(roh[j + 1], min(max(t, 0.0), 1.0)))
    return punkte


def knochen_plan(L, P):
    """Head/tail points of every bone from the landmarks"""
    plan = {}
    plan["wurzel"] = (v(0, 0, 0), v(0.15, 0, 0), None)
    plan["becken"] = (v(-0.34, 0, 0.56), v(-0.34, 0, 0.66), "wurzel")
    plan["wirbel_01"] = (v(-0.34, 0, 0.6), v(-0.14, 0, 0.61), "becken")
    plan["wirbel_02"] = (v(-0.14, 0, 0.61), v(0.03, 0, 0.61), "wirbel_01")
    plan["wirbel_03"] = (v(0.03, 0, 0.61), v(0.17, 0, 0.6), "wirbel_02")
    nase = Vector(L["nase"])
    kopf_basis = v(0.36, nase.y * 0.4, 0.73)
    plan["hals"] = (v(0.17, 0, 0.6), kopf_basis, "wirbel_03")
    kopf_spitze = kopf_basis.lerp(nase, 0.6) + v(0, 0, 0.03)
    plan["kopf"] = (kopf_basis, kopf_spitze, "hals")
    plan["kiefer"] = (kopf_basis.lerp(nase, 0.3) + v(0, 0, -0.05), kopf_basis.lerp(nase, 0.85) + v(0, 0, -0.05), "kopf")
    for seite in ("l", "r"):
        spitze = Vector(L["ohrspitze_" + seite])
        # Ear base: on the skull below and inward of the tip
        basis = v(kopf_basis.x + 0.05, kopf_basis.y + (0.05 if seite == "l" else -0.06) + (nase.y * 0.3), 0.84)
        mitte_ohr = basis.lerp(spitze, 0.5)
        plan["ohr_%s_01" % seite] = (basis, mitte_ohr, "kopf")
        plan["ohr_%s_02" % seite] = (mitte_ohr, spitze, "ohr_%s_01" % seite)

    linie = schweif_linie(P)
    for i in range(SCHWEIF_KNOCHEN):
        plan["schweif_%02d" % (i + 1)] = (linie[i], linie[i + 1], "becken" if i == 0 else "schweif_%02d" % i)

    for bein in ("vl", "vr", "hl", "hr"):
        pfote = Vector(L["pfote_" + bein])
        saeule = {round(r[0], 2): Vector(r[1:4]) for r in L["bein_" + bein] if r[1] is not None}
        y = pfote.y
        vorn = bein[0] == "v"
        if vorn:
            oben = v(0.29, y * 0.85, 0.5)
            knie = v(saeule[0.36].x - 0.01, saeule[0.36].y, 0.36)
            gelenk = v(saeule[0.12].x, saeule[0.12].y, 0.09)
            eltern = "wirbel_03"
        else:
            oben = v(-0.37, y * 0.8, 0.55)
            knie = v(saeule[0.44].x + 0.01, saeule[0.44].y, 0.42)
            gelenk = v(saeule[0.12].x, saeule[0.12].y, 0.14)
            eltern = "becken"
        ballen = v(pfote.x, pfote.y, 0.035)
        zehen = ballen + v(0.06, 0, -0.015)
        plan["bein_%s_oben" % bein] = (oben, knie, eltern)
        plan["bein_%s_unten" % bein] = (knie, gelenk, "bein_%s_oben" % bein)
        plan["bein_%s_mittelfuss" % bein] = (gelenk, ballen, "bein_%s_unten" % bein)
        plan["bein_%s_pfote" % bein] = (ballen, zehen, "bein_%s_mittelfuss" % bein)
    return plan


def skelett_bauen(plan):
    data = bpy.data.armatures.new("Rankenluchs_Skelett")
    arm = bpy.data.objects.new("Rankenluchs_Skelett", data)
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for name, (kopf, ende, _) in plan.items():
        b = data.edit_bones.new(name)
        b.head, b.tail = kopf, ende
        # Roll: keep each bone's local Z pointing up where possible, the same on both sides
        b.align_roll(Vector((0, 0, 1)) if abs((ende - kopf).normalized().z) < 0.9 else Vector((1, 0, 0)))
    for name, (_, _, eltern) in plan.items():
        if eltern:
            data.edit_bones[name].parent = data.edit_bones[eltern]
    bpy.ops.object.mode_set(mode="OBJECT")
    return arm


def gewichte(obj, arm):
    """Heat weights on a closed voxel copy, carried over to the real mesh"""
    kopie = obj.copy()
    kopie.data = obj.data.copy()
    kopie.name = "Gewichte_Huelle"
    bpy.context.scene.collection.objects.link(kopie)
    for m in list(kopie.modifiers):
        kopie.modifiers.remove(m)
    remesh = kopie.modifiers.new("Huelle", "REMESH")
    remesh.mode = "VOXEL"
    remesh.voxel_size = 0.009
    bpy.context.view_layer.objects.active = kopie
    bpy.ops.object.modifier_apply(modifier="Huelle")
    dezimieren = kopie.modifiers.new("Weniger", "DECIMATE")
    dezimieren.ratio = min(1.0, 60000 / max(len(kopie.data.polygons), 1))
    bpy.ops.object.modifier_apply(modifier="Weniger")
    print("[Rankenluchs] Huelle: %d Flaechen" % len(kopie.data.polygons))

    for o in bpy.context.scene.objects:
        o.select_set(False)
    kopie.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    # Heat weighting fails on small objects (1.5 m): done ten times as big, then the armature goes back to 1
    kopie.scale = (10.0, 10.0, 10.0)
    arm.scale = (10.0, 10.0, 10.0)
    bpy.context.view_layer.update()
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    arm.scale = (1.0, 1.0, 1.0)
    kopie.parent = None
    kopie.scale = (1.0, 1.0, 1.0)
    bpy.context.view_layer.update()
    gewichtet = sum(1 for vx in kopie.data.vertices if vx.groups)
    print("[Rankenluchs] Huelle gewichtet: %d von %d Vertices, %d Gruppen" % (gewichtet, len(kopie.data.vertices), len(kopie.vertex_groups)))

    # Carry over: nearest surface point of the shell, interpolated
    bpy.context.view_layer.objects.active = obj
    for b in arm.data.bones:
        if b.name not in obj.vertex_groups:
            obj.vertex_groups.new(name=b.name)
    transfer = obj.modifiers.new("Gewichte", "DATA_TRANSFER")
    transfer.object = kopie
    transfer.use_vert_data = True
    transfer.data_types_verts = {"VGROUP_WEIGHTS"}
    transfer.vert_mapping = "POLYINTERP_NEAREST"
    transfer.layers_vgroup_select_src = "ALL"
    transfer.layers_vgroup_select_dst = "NAME"
    for o in bpy.context.scene.objects:
        o.select_set(o == obj)
    bpy.ops.object.datalayout_transfer(modifier="Gewichte")
    bpy.ops.object.modifier_apply(modifier="Gewichte")
    bpy.data.objects.remove(kopie, do_unlink=True)

    # Thin bits the voxel shell lost (leaf tips, whiskers): fully on the nearest bone
    segmente = [(b.name, arm.matrix_world @ b.head_local, arm.matrix_world @ b.tail_local) for b in arm.data.bones if b.name != "wurzel"]
    gruppen = {vg.name: vg for vg in obj.vertex_groups}
    nachgetragen = 0
    for vx in obj.data.vertices:
        if any(g.weight > 0.001 for g in vx.groups):
            continue
        p = obj.matrix_world @ vx.co

        def abstand(seg):
            _, a, b = seg
            ab = b - a
            t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-9)))
            return (a + ab * t - p).length
        gruppen[min(segmente, key=abstand)[0]].add([vx.index], 1.0, "REPLACE")
        nachgetragen += 1
    print("[Rankenluchs] %d Vertices ohne Huellen-Gewicht an den naechsten Knochen gehaengt" % nachgetragen)

    # Smooth a little and keep at most 4 bones per vertex, normalised (Unreal's default)
    for o in bpy.context.scene.objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="WEIGHT_PAINT")
    bpy.ops.object.vertex_group_smooth(group_select_mode="ALL", factor=0.5, repeat=2)
    bpy.ops.object.vertex_group_limit_total(group_select_mode="ALL", limit=4)
    bpy.ops.object.vertex_group_normalize_all(lock_active=False)
    bpy.ops.object.mode_set(mode="OBJECT")

    mod = obj.modifiers.new("Skelett", "ARMATURE")
    mod.object = arm
    obj.parent = arm
    ohne = sum(1 for vx in obj.data.vertices if not vx.groups)
    print("[Rankenluchs] Vertices ohne Gewicht: %d" % ohne)


def knochen_bild(arm, name, mitte_, groesse):
    """Renders the mesh see-through with the bones as red sticks"""
    stoecke = []
    for b in arm.data.bones:
        kopf = arm.matrix_world @ b.head_local
        ende = arm.matrix_world @ b.tail_local
        laenge = (ende - kopf).length
        bpy.ops.mesh.primitive_cylinder_add(radius=0.008, depth=laenge, location=(kopf + ende) / 2)
        stock = bpy.context.active_object
        stock.rotation_euler = (ende - kopf).to_track_quat("Z", "Y").to_euler()
        stoecke.append(stock)
    shading = bpy.context.scene.display.shading
    vorschau.szene_einrichten()
    shading.show_xray = True
    shading.xray_alpha = 0.35
    shading.color_type = "OBJECT"
    for s in stoecke:
        s.color = (1.0, 0.1, 0.1, 1.0)
    vorschau.rendern(name + "_seite", (0.0, -1.0, 0.05), mitte_, groesse)
    vorschau.rendern(name + "_oben", (0.0, 0.0, 1.0), mitte_, groesse)
    shading.show_xray = False
    for s in stoecke:
        bpy.data.objects.remove(s, do_unlink=True)


def pose_test(arm, mitte_, groesse):
    """Bent legs, curled tail, lowered head: the mesh must follow without folding into the body"""
    pb = arm.pose.bones
    for bein in ("vl", "vr"):
        pb["bein_%s_oben" % bein].rotation_mode = "XYZ"
        pb["bein_%s_unten" % bein].rotation_mode = "XYZ"
    beugen = {"bein_vl_oben": 35, "bein_vl_unten": -70, "bein_vl_mittelfuss": 40, "bein_hr_oben": -40, "bein_hr_unten": 60,
              "bein_hr_mittelfuss": -50, "hals": -20, "kopf": -15}
    for name, grad in beugen.items():
        pb[name].rotation_mode = "XYZ"
        pb[name].rotation_euler = (0.0, 0.0, math.radians(grad))
    for i in range(SCHWEIF_KNOCHEN):
        b = pb["schweif_%02d" % (i + 1)]
        b.rotation_mode = "XYZ"
        b.rotation_euler = (math.radians(12), 0.0, math.radians(9))
    bpy.context.view_layer.update()
    vorschau.rendern("02_pose_seite", (0.0, -1.0, 0.05), mitte_, groesse)
    vorschau.rendern("02_pose_spielkamera", (0.45, -0.8, math.tan(math.radians(50))), mitte_, groesse)
    for b in pb:
        b.rotation_euler = (0, 0, 0)
        b.rotation_quaternion = (1, 0, 0, 0)
    bpy.context.view_layer.update()


def main():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(ORDNER, "arbeit", "01_vorbereitet.blend"))
    obj = bpy.data.objects["Rankenluchs"]
    with open(os.path.join(ORDNER, "arbeit", "landmarken.json"), encoding="utf-8") as f:
        L = json.load(f)
    P = [vx.co.copy() for vx in obj.data.vertices]
    plan = knochen_plan(L, P)
    arm = skelett_bauen(plan)
    print("[Rankenluchs] %d Knochen" % len(arm.data.bones))
    knochen_bild(arm, "02_knochen", (-0.15, 0.0, 0.5), 1.7)
    gewichte(obj, arm)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ORDNER, "arbeit", "02_skelett.blend"))
    pose_test(arm, (-0.15, 0.0, 0.5), 1.7)


main()
