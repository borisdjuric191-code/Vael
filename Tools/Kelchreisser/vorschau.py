"""Preview renders for the Kelchreisser scripts: side view, top view and the game camera's view.

Workbench with the model's own texture colours, orthographic, plain grey background (as in Tools/Pfuhlnatter).
"""
import math
import os

import bpy
from mathutils import Matrix, Vector

# Working files live outside the repository, next to the Tripo export
ORDNER = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))),
                      "Vael_Quellen", "Kelchreisser")
VORSCHAU = os.path.join(ORDNER, "vorschau")
ARBEIT = os.path.join(ORDNER, "arbeit")


def szene_einrichten(aufloesung=900):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.display.shading.show_shadows = True
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
        data = bpy.data.cameras.new("VorschauKamera")
        cam = bpy.data.objects.new("VorschauKamera", data)
        bpy.context.scene.collection.objects.link(cam)
    cam.data.type = "ORTHO"
    bpy.context.scene.camera = cam
    return cam


def rendern(name, richtung, mitte, groesse):
    """Renders from a direction (vector from the subject to the camera) onto vorschau/<name>.png"""
    os.makedirs(VORSCHAU, exist_ok=True)
    szene_einrichten()
    cam = _kamera()
    richtung = Vector(richtung).normalized()
    cam.location = Vector(mitte) + richtung * 20.0
    vorwaerts = -richtung
    rechts = vorwaerts.cross(Vector((0.0, 0.0, 1.0)))
    if rechts.length < 1e-4:
        rechts = Vector((1.0, 0.0, 0.0))
    rechts.normalize()
    hoch = rechts.cross(vorwaerts).normalized()
    cam.rotation_euler = Matrix((rechts, hoch, -vorwaerts)).transposed().to_euler()
    cam.data.ortho_scale = groesse
    cam.data.clip_end = 200.0
    pfad = os.path.join(VORSCHAU, name + ".png")
    bpy.context.scene.render.filepath = pfad
    bpy.ops.render.render(write_still=True)
    return pfad


def standard_ansichten(praefix, mitte, groesse):
    """Side (from -Y, head to the right), game camera (from the front left above, about 50 degrees down), top"""
    return [rendern(praefix + "_seite", (0.0, -1.0, 0.05), mitte, groesse),
            rendern(praefix + "_spielkamera", (0.45, -0.8, math.tan(math.radians(50))), mitte, groesse),
            rendern(praefix + "_oben", (0.0, 0.0, 1.0), mitte, groesse)]
