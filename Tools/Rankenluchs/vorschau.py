"""Preview renders for the Rankenluchs scripts: side view and the game camera's view (from above at an angle).

Workbench with the model's own texture colours, orthographic, plain grey background. Only the lynx itself is ever
rendered here (no Fab or Megascans content).
"""
import math
import os

import bpy
from mathutils import Matrix, Vector

ORDNER = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VORSCHAU = os.path.join(ORDNER, "vorschau")


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


def rendern(name, richtung, mitte, groesse, objekte=None):
    """Renders from a direction (vector from the subject to the camera) onto vorschau/<name>.png"""
    os.makedirs(VORSCHAU, exist_ok=True)
    szene_einrichten()
    cam = _kamera()
    richtung = Vector(richtung).normalized()
    cam.location = Vector(mitte) + richtung * 10.0
    # Look along -richtung with the world's up staying up in the picture
    vorwaerts = -richtung
    rechts = vorwaerts.cross(Vector((0.0, 0.0, 1.0)))
    if rechts.length < 1e-4:
        rechts = Vector((1.0, 0.0, 0.0)) if abs(vorwaerts.z) > 0.99 and vorwaerts.z < 0 else Vector((-1.0, 0.0, 0.0))
    rechts.normalize()
    hoch = rechts.cross(vorwaerts).normalized()
    cam.rotation_euler = Matrix((rechts, hoch, -vorwaerts)).transposed().to_euler()
    cam.data.ortho_scale = groesse
    cam.data.clip_end = 100.0
    pfad = os.path.join(VORSCHAU, name + ".png")
    bpy.context.scene.render.filepath = pfad
    bpy.ops.render.render(write_still=True)
    return pfad


def standard_ansichten(praefix, mitte=(0.0, 0.0, 0.35), groesse=1.6):
    """Side (from -Y, head to the right), game camera (from the front left above, about 50 degrees down), front, top (head to the right)"""
    seite = rendern(praefix + "_seite", (0.0, -1.0, 0.05), mitte, groesse)
    spiel = rendern(praefix + "_spielkamera", (0.45, -0.8, math.tan(math.radians(50))), mitte, groesse)
    vorn = rendern(praefix + "_vorn", (1.0, 0.0, 0.1), mitte, groesse)
    oben = rendern(praefix + "_oben", (0.0, 0.0, 1.0), mitte, groesse)
    return [seite, spiel, vorn, oben]
