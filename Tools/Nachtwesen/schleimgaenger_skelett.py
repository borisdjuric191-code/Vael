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


def _anhaengen(teil, knochen, material, obj):
    """Links a shape into the scene, binds it fully to one bone and gives it a material slot of the skeleton"""
    bpy.context.scene.collection.objects.link(teil)
    gruppe = teil.vertex_groups.new(name=knochen)
    gruppe.add(list(range(len(teil.data.vertices))), 1.0, "REPLACE")
    teil.data.materials.append(material)
    return teil


def seuche_anbauen(obj, arm):
    """The Tropfseuche in the body (Boris 2026-11-21): a glowing heart in the middle of the ribs, held by a web of slime
    threads to the ribs, and thick lumps of slime in patches over the bones. Material slots "Herz" and "Seuche": the game
    gives them the colour of the strain."""
    import random
    from mathutils import Matrix as M, Vector as V
    import tropfseuche_formen as formen

    rng = random.Random(21)
    seuche = bpy.data.materials.get("Seuche") or bpy.data.materials.new("Seuche")
    herz_mat = bpy.data.materials.get("Herz") or bpy.data.materials.new("Herz")
    namen = [g.name for g in obj.vertex_groups]
    orte, normalen, knochen = [], [], []
    for v in obj.data.vertices:
        if not v.groups:
            continue
        g = max(v.groups, key=lambda x: x.weight)
        orte.append(v.co.copy())
        normalen.append(v.normal.copy())
        knochen.append(namen[g.group])

    # The heart: in the middle of the rib cage, a little to the left like a real one
    rippen = [p for p, k in zip(orte, knochen) if k == "spine_03"]
    lo = V((min(p.x for p in rippen), min(p.y for p in rippen), min(p.z for p in rippen)))
    hi = V((max(p.x for p in rippen), max(p.y for p in rippen), max(p.z for p in rippen)))
    mitte = (lo + hi) / 2
    herz = V((mitte.x + 0.03, mitte.y + 0.02, mitte.z + 0.035))
    herz_r = 0.065
    teile = []
    kern = formen.klumpen("Seuchenherz", herz_r, flach=1.0, saat=3, unterteilung=3, beulen=0.18)
    kern.data.transform(M.Translation(herz))
    teile.append(_anhaengen(kern, "spine_03", herz_mat, obj))

    # The web: threads from the heart to ribs all round it, the anchors spread over every direction
    kandidaten = [p for p in rippen if 0.07 < (p - herz).length < 0.2 and abs(p.z - herz.z) < 0.13]
    anker = []
    for p in sorted(kandidaten, key=lambda p: rng.random()):
        r = (p - herz).normalized()
        if all(r.dot((q - herz).normalized()) < 0.72 for q in anker):
            anker.append(p)
        if len(anker) >= 13:
            break
    for i, p in enumerate(anker):
        start = herz + (p - herz).normalized() * herz_r * 0.8
        teile.append(_anhaengen(formen.faden("Faden%d" % i, start, p, 0.007, 0.005, durchhang=0.012, zittern=0.008, saat=i),
                                "spine_03", seuche, obj))
    # Cross threads between neighbouring anchors close the web
    for i, p in enumerate(anker):
        nachbar = min((q for q in anker if q is not p), key=lambda q: (q - p).length)
        if (nachbar - p).length < 0.16:
            teile.append(_anhaengen(formen.faden("Netz%d" % i, p, nachbar, 0.0035, 0.0035, durchhang=0.018, zittern=0.006, saat=40 + i),
                                    "spine_03", seuche, obj))

    # Thick lumps in patches: a few places, several lumps each, on the big bones (not on fingers or the face)
    erlaubt = {"pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "clavicle_l", "clavicle_r", "upperarm_l", "upperarm_r",
               "lowerarm_l", "lowerarm_r", "thigh_l", "thigh_r", "calf_l", "calf_r", "head"}
    # not in front of the heart (it must shine through the ribs) and not on the face
    stellen = [i for i, k in enumerate(knochen) if k in erlaubt
               and not (k == "spine_03" and orte[i].x > herz.x + 0.03)
               and not (k == "head" and orte[i].x > -0.02)]
    zentren = rng.sample(stellen, 11)
    n_klumpen = 0
    for z, zi in enumerate(zentren):
        nah = [i for i in stellen if (orte[i] - orte[zi]).length < 0.09 and knochen[i] == knochen[zi]]
        for k in range(rng.randint(4, 7)):
            i = rng.choice(nah)
            r = rng.uniform(0.018, 0.04)
            blob = formen.klumpen("Klumpen%d_%d" % (z, k), r, flach=rng.uniform(0.45, 0.7), saat=z * 10 + k, unterteilung=2)
            # Z of the lump along the surface normal, sunk a little into the bone
            n = normalen[i].normalized()
            dreh = n.to_track_quat("Z", "Y").to_matrix().to_4x4()
            blob.data.transform(M.Translation(orte[i] + n * r * 0.25) @ dreh)
            teile.append(_anhaengen(blob, knochen[i], seuche, obj))
            n_klumpen += 1

    for x in bpy.data.objects:
        x.select_set(x in teile or x == obj)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.join()
    gemein.log(NAME, "Seuche: Herz bei %s, %d Faeden ans Netz, %d Klumpen an %d Stellen, Materialien %s" % (
        tuple(round(c, 3) for c in herz), len(anker), n_klumpen, len(zentren), [m.name for m in obj.data.materials]))


def main():
    obj, arm = modell_vorbereiten()
    seuche_anbauen(obj, arm)
    gemein.speichern(NAME, "02_skelett.blend")
    texturen()
    lo, hi = gemein.grenzen(obj)
    gemein.ansichten(NAME, "02", (0.0, 0.0, (hi.z - lo.z) / 2), max(hi - lo) * 1.1, ("vorne", "seite", "spiel"), 600)
    for b in arm.data.bones:
        if b.name in ("pelvis", "head", "hand_l", "foot_l", "foot_r", "upperarm_l", "thigh_l"):
            gemein.log(NAME, "  %s Kopf %s Ende %s" % (b.name, tuple(round(x, 3) for x in b.head_local), tuple(round(x, 3) for x in b.tail_local)))


main()
