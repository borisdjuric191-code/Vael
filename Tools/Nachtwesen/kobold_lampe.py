"""The lantern of the Kobold-Laternentraeger as the lamp of the Kobold tunnels (Boris 2026-11-23: the lamp from its back in the
tunnels instead of the old lamps, the wooden holders stay; real lamps and its false light then look alike, which makes its
trap better and the tunnels nicer).

  SM_HK_KoboldLampe    the lantern cut out of the Laternentraeger body (Tripo texture, slot KoboldLampe) with a glowing crystal
                       core (slot Kristall), scaled to a tunnel lamp; hangs from its origin (the top of its hook)
  SM_HK_Laternenpfahl  the wooden post with its arm of SM_HK_Laterne, without the old lamp; the lamp hangs at (0.45, 0, 2.47)

    blender -b --factory-startup -P Tools/Nachtwesen/kobold_lampe.py -- C:/Projekte/Vael_Quellen/Hoehlen/export
"""
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

HIER = os.path.dirname(os.path.abspath(__file__))
sys.path.append(HIER)
import gemein  # noqa: E402

OUT = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else "C:/Projekte/Vael_Quellen/Hoehlen/export"
GROESSE = 0.42      # height of the tunnel lamp (m); on the Laternentraeger it is about 0.17 m


def exportieren(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    pfad = os.path.join(OUT, obj.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=pfad, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_ALL", bake_anim=False, mesh_smooth_type="FACE", colors_type="LINEAR")
    print("[KoboldLampe] %s: %d Dreiecke -> %s" % (obj.name, sum(len(p.vertices) - 2 for p in obj.data.polygons), pfad))


def lampe():
    obj, _ = gemein.oeffnen("Laternentraeger", "01_vorbereitet.blend")
    P = gemein.punkte(obj)
    # the lantern hangs in front of and beside the body on its pole (kobold_skelett.py: laterne_licht), a little wider for the cage
    drin = (P[:, 0] > 0.13) & (np.abs(P[:, 1]) > 0.13) & (P[:, 2] > 0.57) & (P[:, 2] < 0.8)
    # only its own side (the pole bends to one side)
    seite = np.sign(np.median(P[drin, 1]))
    drin &= np.sign(P[:, 1]) == seite
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    weg = [f for f in bm.faces if not all(drin[v.index] for v in f.verts)]
    bmesh.ops.delete(bm, geom=weg, context="FACES")
    lose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=lose, context="VERTS")
    # the biggest connected piece is the lantern (stray bits of hand or pole go)
    stuecke, gesehen = [], set()
    for f in bm.faces:
        if f.index in gesehen:
            continue
        stapel, stueck = [f], []
        gesehen.add(f.index)
        while stapel:
            g = stapel.pop()
            stueck.append(g)
            for e in g.edges:
                for h in e.link_faces:
                    if h.index not in gesehen:
                        gesehen.add(h.index)
                        stapel.append(h)
        stuecke.append(stueck)
    bm.faces.ensure_lookup_table()
    behalten = set()
    groesstes = max(stuecke, key=len)
    mitte_g = sum((f.calc_center_median() for f in groesstes), Vector()) / len(groesstes)
    for stueck in stuecke:
        m = sum((f.calc_center_median() for f in stueck), Vector()) / len(stueck)
        if stueck is groesstes or (m - mitte_g).length < 0.06:
            behalten.update(f.index for f in stueck)
    weg = [f for f in bm.faces if f.index not in behalten]
    bmesh.ops.delete(bm, geom=weg, context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    me = bpy.data.meshes.new("SM_HK_KoboldLampe")
    bm.to_mesh(me)
    bm.free()
    lamp = bpy.data.objects.new("SM_HK_KoboldLampe", me)
    bpy.context.scene.collection.objects.link(lamp)
    for m in list(me.materials):
        pass
    me.materials.clear()
    mat = bpy.data.materials.new("KoboldLampe")
    me.materials.append(mat)
    # upright: hang from the top middle, scaled to a tunnel lamp
    co = np.array([v.co[:] for v in me.vertices])
    lo, hi = co.min(axis=0), co.max(axis=0)
    oben = Vector(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, hi[2]))
    s = GROESSE / max(hi[2] - lo[2], 1e-3)
    me.transform(Matrix.Scale(s, 4) @ Matrix.Translation(-oben))
    # the glowing crystal core (slot Kristall: the cave's crystal glow)
    co = np.array([v.co[:] for v in me.vertices])
    lo, hi = co.min(axis=0), co.max(axis=0)
    kern = Vector(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2] + (hi[2] - lo[2]) * 0.42))
    kmat = bpy.data.materials.new("Kristall")
    me.materials.append(kmat)
    bm = bmesh.new()
    bm.from_mesh(me)
    r = 0.25 * min(hi[0] - lo[0], hi[1] - lo[1])
    h = 0.32 * (hi[2] - lo[2])
    oben_v = bm.verts.new(kern + Vector((0, 0, h)))
    unten_v = bm.verts.new(kern - Vector((0, 0, h)))
    ring = [bm.verts.new(kern + Vector((math.cos(a) * r, math.sin(a) * r, 0.0))) for a in np.linspace(0, 2 * math.pi, 7)[:-1]]
    for k in range(6):
        for f in (bm.faces.new((ring[k], ring[(k + 1) % 6], oben_v)), bm.faces.new((ring[(k + 1) % 6], ring[k], unten_v))):
            f.material_index = 1
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    exportieren(lamp)
    return lamp


def pfahl():
    """The wooden post and arm of SM_HK_Laterne (hoehlen_baukasten.lantern) without its lamp"""
    sys.path.append(os.path.join(os.path.dirname(HIER), "Welt"))
    sys.path.append(os.path.join(os.path.dirname(HIER), "Refugium"))
    import hoehlen_baukasten as hb
    import glutkammer_probe as gk
    gk.clear_scene()
    obj = gk.new_object("SM_HK_Laternenpfahl", hb.KIT)
    b = gk.Builder()
    hb.timber(b, Vector((0, 0, 0)), Vector((0, 0, 2.5)), 0.08)
    hb.timber(b, Vector((0, 0, 2.45)), Vector((0.45, 0, 2.5)), 0.04, seed=2)
    # a short iron hook at the end of the arm
    hb.lathe(b, [(0.0, -0.05), (0.012, -0.045), (0.012, 0.0), (0.0, 0.005)], hb.METALL, Vector((0.45, 0, 2.47)), sides=6)
    b.finish(obj)
    gk.OUT = OUT
    gk.export(obj)


lampe()
pfahl()
