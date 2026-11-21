"""Schleimgaenger, steps 1+2: the walking skeleton host of the Tropfseuche, made from the Fab "Human Skeleton" by
Salt Lake Manor (CC BY, credit in Docs/Credits.md). The model already comes skinned to the Unreal mannequin skeleton
(pelvis, spine_01..03, neck_01, head, clavicle/upperarm/lowerarm/hand, thigh/calf/foot/ball): that skeleton is the one
every humanlike Schleimgaenger shares (Kobold, Gildenmensch, skeleton), so they can all use the same movements.

  - keeps the whole skinned piece SK_Skeleton, drops the single bones and the lights
  - modifiers of the Fab file: subdivision off, mirror applied, "Smooth by Angle" off (shading comes from Unreal)
  - turns it from -Y to +X (the night creatures look along +X), metres, armature at scale 1
  - reduces it to the triangle budget, keeps the bone weights
  - the Fab textures become T_Schleimgaenger_basecolor / normal / roughness / metallic in export/
    (roughness and metallic are split off the packed occlusion-roughness-metallic map)

Writes Vael_Quellen/Schleimgaenger/arbeit/02_skelett.blend (objects "Schleimgaenger" and "Schleimgaenger_Skelett"),
the textures and measuring pictures in vorschau/.

    blender -b C:/Projekte/Vael_Quellen/Skelett_Fab/skeleton.blend -P Tools/Nachtwesen/schleimgaenger_skelett.py
"""
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Matrix

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import gemein  # noqa: E402

NAME = "Schleimgaenger"
DREIECKE = 26000
QUELLE = os.path.join(gemein.QUELLEN, "Skelett_Fab")


def modell_vorbereiten():
    arm = bpy.data.objects["root"]
    obj = bpy.data.objects["SK_Skeleton"]
    for o in list(bpy.data.objects):
        if o not in (arm, obj):
            bpy.data.objects.remove(o, do_unlink=True)
    # Straight into the scene, visible: in the Fab file they sit in collections that hide them from selection, and an
    # unselectable mesh was silently left out of the FBX (Unreal: "nothing to import")
    szene = bpy.context.scene.collection
    for o in (arm, obj):
        for c in list(o.users_collection):
            c.objects.unlink(o)
        szene.objects.link(o)
        o.hide_viewport = False
        o.hide_render = False
        o.hide_set(False)
        o.hide_select = False
    for c in list(bpy.data.collections):
        bpy.data.collections.remove(c)

    # Modifiers: subdivision and the smoothing node group go, a mirror is applied, the armature is set up again later
    bpy.context.view_layer.objects.active = obj
    for m in list(obj.modifiers):
        if m.type in ("SUBSURF", "NODES", "ARMATURE"):
            obj.modifiers.remove(m)
    for m in list(obj.modifiers):
        bpy.ops.object.modifier_apply(modifier=m.name)

    # Both into plain world coordinates in metres, nothing parented (the Fab mesh data is shared: transform_apply
    # quietly refuses multi-user data, so both get their own copy first)
    obj.data = obj.data.copy()
    arm.data = arm.data.copy()
    welt = obj.matrix_world.copy()
    obj.parent = None
    obj.matrix_world = welt
    # Straight into the data (the operator did nothing here in the background)
    for o in (arm, obj):
        o.data.transform(o.matrix_world)
        o.matrix_world = Matrix.Identity(4)

    # From -Y to +X: a quarter turn to the left round Z
    drehung = Matrix.Rotation(math.radians(90.0), 4, "Z")
    obj.data.transform(drehung)
    arm.data.transform(drehung)

    n = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    if n > DREIECKE:
        mod = obj.modifiers.new("Budget", "DECIMATE")
        mod.ratio = DREIECKE / n
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=mod.name)
    for p in obj.data.polygons:
        p.use_smooth = True

    # One root as in the Unreal mannequin: the Fab file used the armature object as root, so pelvis and the two ik
    # roots were three roots of their own and Unreal found nothing to import. The ik bones are not needed.
    for x in bpy.data.objects:
        x.select_set(x == arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    knochen = arm.data.edit_bones
    for b in [b for b in knochen if b.name.startswith("ik_")]:
        knochen.remove(b)
    wurzel = knochen.new("root")
    wurzel.head, wurzel.tail = (0.0, 0.0, 0.0), (0.0, -0.25, 0.0)
    knochen["pelvis"].parent = wurzel
    bpy.ops.object.mode_set(mode="OBJECT")

    mod = obj.modifiers.new("Skelett", "ARMATURE")
    mod.object = arm
    obj.parent = arm
    obj.name = obj.data.name = NAME
    arm.name = arm.data.name = NAME + "_Skelett"
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
        pb.location = (0, 0, 0)
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.scale = (1, 1, 1)
    lo, hi = gemein.grenzen(obj)
    gemein.log(NAME, "%d -> %d Dreiecke, %d Knochen, Groesse %.2f x %.2f x %.2f m" % (
        n, sum(len(p.vertices) - 2 for p in obj.data.polygons), len(arm.data.bones), *(hi - lo)))
    return obj, arm


def bild_laden(name):
    pfad = os.path.join(QUELLE, "textures", name)
    img = bpy.data.images.load(pfad)
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    return img, px.reshape(h, w, 4)


def bild_speichern(px, pfad, format_):
    h, w = px.shape[:2]
    img = bpy.data.images.new(os.path.basename(pfad), w, h, alpha=False, float_buffer=False)
    img.pixels.foreach_set(px.astype(np.float32).ravel())
    img.filepath_raw = pfad
    img.file_format = format_
    if format_ == "JPEG":
        bpy.context.scene.render.image_settings.quality = 92
    img.save()
    bpy.data.images.remove(img)


def texturen():
    ziel = gemein.ordner(NAME, "export")
    os.makedirs(ziel, exist_ok=True)
    praefix = "Skeleton_LP_MI_Skeleton01_LP_"
    _, farbe = bild_laden(praefix + "BaseColor.png")
    bild_speichern(farbe, os.path.join(ziel, "T_%s_basecolor.jpg" % NAME), "JPEG")
    _, normal = bild_laden(praefix + "Normal.png")
    bild_speichern(normal, os.path.join(ziel, "T_%s_normal.png" % NAME), "PNG")
    _, orm = bild_laden(praefix + "OcclusionRoughnessMetallic.png")
    for kanal, art in ((1, "roughness"), (2, "metallic")):
        grau = np.repeat(orm[:, :, kanal:kanal + 1], 3, axis=2)
        grau = np.concatenate([grau, np.ones_like(grau[:, :, :1])], axis=2)
        bild_speichern(grau, os.path.join(ziel, "T_%s_%s.jpg" % (NAME, art)), "JPEG")
    gemein.log(NAME, "Texturen nach", ziel)


def main():
    obj, arm = modell_vorbereiten()
    gemein.speichern(NAME, "02_skelett.blend")
    texturen()
    lo, hi = gemein.grenzen(obj)
    gemein.ansichten(NAME, "02", (0.0, 0.0, (hi.z - lo.z) / 2), max(hi - lo) * 1.1, ("vorne", "seite", "spiel"), 600)
    for b in arm.data.bones:
        if b.name in ("pelvis", "head", "hand_l", "foot_l", "foot_r", "upperarm_l", "thigh_l"):
            gemein.log(NAME, "  %s Kopf %s Ende %s" % (b.name, tuple(round(x, 3) for x in b.head_local), tuple(round(x, 3) for x in b.tail_local)))


main()
