"""Shared helpers for the four night creatures (Moderglocke, Kahlschwinge, Peitschenunke, Schnappwurz):
preparing the Tripo mesh, building a skeleton from a plan, weights, poses, baking actions, check renders, export.

Working files live outside the repository in Vael_Quellen/<Name>/{arbeit,vorschau,export}.
Axes: the creature looks along +X, +Y is its left, Z up, metres (as Tools/Kelchreisser).
"""
import glob
import math
import os

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

QUELLEN = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))),
                       "Vael_Quellen")
FPS = 30


def ordner(name, *teile):
    pfad = os.path.join(QUELLEN, name, *teile)
    return pfad


def log(name, *a):
    print("[%s]" % name, *a)


# ---------------------------------------------------------------- Mesh

def grenzen(obj):
    co = np.empty(len(obj.data.vertices) * 3, dtype=np.float32)
    obj.data.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    return Vector(co.min(axis=0)), Vector(co.max(axis=0))


def punkte(obj):
    co = np.empty(len(obj.data.vertices) * 3, dtype=np.float32)
    obj.data.vertices.foreach_get("co", co)
    return co.reshape(-1, 3).astype(np.float64)


def laden(name, dreiecke, drehung_z=0.0, hoehe=None, laenge=None):
    """Imports the Tripo FBX, joins it, reduces it to the triangle budget, turns it to +X, scales it (height or
    length in metres) and puts the origin on the ground under its middle"""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    fbx = glob.glob(ordner(name, "02_tripo", "*.fbx"))[0]
    bpy.ops.import_scene.fbx(filepath=fbx)
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    for o in bpy.data.objects:
        o.select_set(o in meshes)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    welt = obj.matrix_world.copy()
    obj.parent = None
    obj.matrix_world = welt
    for o in [o for o in bpy.data.objects if o.type != "MESH"]:
        bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    obj.name = obj.data.name = name
    n = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    if n > dreiecke:
        mod = obj.modifiers.new("Budget", "DECIMATE")
        mod.ratio = dreiecke / n
        bpy.ops.object.modifier_apply(modifier=mod.name)
    obj.data.transform(Matrix.Rotation(math.radians(drehung_z), 4, "Z"))
    lo, hi = grenzen(obj)
    obj.data.transform(Matrix.Translation(-Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))))
    lo, hi = grenzen(obj)
    g = hi - lo
    s = (hoehe / g.z) if hoehe else (laenge / g.x) if laenge else 1.0
    obj.data.transform(Matrix.Scale(s, 4))
    for p in obj.data.polygons:
        p.use_smooth = True
    lo, hi = grenzen(obj)
    log(name, "%d -> %d Dreiecke, Groesse %.2f x %.2f x %.2f m" % (
        n, sum(len(p.vertices) - 2 for p in obj.data.polygons), *(hi - lo)))
    return obj


def speichern(name, datei):
    os.makedirs(ordner(name, "arbeit"), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=ordner(name, "arbeit", datei))


def oeffnen(name, datei):
    bpy.ops.wm.open_mainfile(filepath=ordner(name, "arbeit", datei))
    bpy.context.scene.render.fps = FPS
    return bpy.data.objects[name], bpy.data.objects.get(name + "_Skelett")


# ---------------------------------------------------------------- Renders

def szene_einrichten(aufloesung=700):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.display.shading.show_shadows = False
    scene.display.shading.show_cavity = True
    scene.render.resolution_x = aufloesung
    scene.render.resolution_y = aufloesung
    scene.render.film_transparent = False
    world = bpy.data.worlds.get("Vorschau") or bpy.data.worlds.new("Vorschau")
    world.color = (0.32, 0.32, 0.33)
    scene.world = world
    return scene


def _kamera():
    cam = bpy.data.objects.get("VorschauKamera")
    if cam is None:
        cam = bpy.data.objects.new("VorschauKamera", bpy.data.cameras.new("VorschauKamera"))
        bpy.context.scene.collection.objects.link(cam)
    cam.data.type = "ORTHO"
    bpy.context.scene.camera = cam
    return cam


ANSICHTEN = {
    "vorne": (1.0, 0.0, 0.05),          # camera in front, looking at the face
    "hinten": (-1.0, 0.0, 0.05),
    "seite": (0.0, -1.0, 0.05),         # from its right, head to the right of the picture
    "oben": (0.0, 0.0, 1.0),
    "spiel": (0.45, -0.8, math.tan(math.radians(45))),   # the game camera: 45 degrees down
}


def rendern(pfad, richtung, mitte, groesse, aufloesung=700):
    os.makedirs(os.path.dirname(pfad), exist_ok=True)
    szene_einrichten(aufloesung)
    cam = _kamera()
    richtung = Vector(richtung).normalized()
    cam.location = Vector(mitte) + richtung * 30.0
    vorwaerts = -richtung
    rechts = vorwaerts.cross(Vector((0.0, 0.0, 1.0)))
    if rechts.length < 1e-4:
        rechts = Vector((0.0, -1.0, 0.0))   # top view: +X up in the picture
    rechts.normalize()
    hoch = rechts.cross(vorwaerts).normalized()
    cam.rotation_euler = Matrix((rechts, hoch, -vorwaerts)).transposed().to_euler()
    cam.data.ortho_scale = groesse
    cam.data.clip_end = 300.0
    bpy.context.scene.render.filepath = pfad
    bpy.ops.render.render(write_still=True)
    return pfad


def ansichten(name, praefix, mitte, groesse, welche=("vorne", "hinten", "seite", "spiel"), aufloesung=420):
    return [rendern(ordner(name, "vorschau", "%s_%s.png" % (praefix, a)), ANSICHTEN[a], mitte, groesse, aufloesung)
            for a in welche]
