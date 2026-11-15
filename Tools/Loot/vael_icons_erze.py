"""
Taschenbilder der Eisenschlund-Erze aus ihren echten Tripo-Modellen (15.11.2026): je Erz das aufbereitete Modell
(Vael_Quellen/<Erz>/04_export/SM_<Erz>.fbx) schräg von oben wie im Loot-Kit, 256 x 256, durchsichtiger Hintergrund.
Danach Scripts/ImportAutoIcons.py (gibt jedem Material ohne Bild seins).

  blender -b --factory-startup -P Tools/Loot/vael_icons_erze.py -- <Ausgabeordner> [Erz ...]
"""
import math
import os
import sys

import bpy
from mathutils import Vector

ERZE = ["Rosteisen", "Schwefelkies", "Funkenerz", "Gruenspan", "Salzspat", "Drahtsilber", "Blutzinnober", "Siechglas", "Rippenspat",
        "Kernerz", "Kobaltbluete"]
args = sys.argv[sys.argv.index("--") + 1:]
OUT = args[0]
NAMEN = args[1:] or ERZE
os.makedirs(OUT, exist_ok=True)


def render(name):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = scene.render.resolution_y = 256
    scene.render.film_transparent = True
    scene.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w")
    world.color = (0.12, 0.1, 0.09)
    scene.world = world
    bpy.ops.import_scene.fbx(filepath="C:/Projekte/Vael_Quellen/{0}/04_export/SM_{0}.fbx".format(name))
    meshes = [o for o in scene.objects if o.type == "MESH"]
    pts = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    centre = (lo + hi) / 2
    size = max(hi - lo)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = size * 1.35
    direction = Vector((1.0, -1.0, 0.85)).normalized()
    cam.location = centre + direction * size * 4
    cam.rotation_euler = (centre - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.clip_end = size * 20
    scene.collection.objects.link(cam)
    scene.camera = cam
    for lname, energy, rot, colour in (("key", 3.5, (50, 0, 35), (1.0, 0.92, 0.82)), ("rim", 2.5, (120, 0, 210), (0.8, 0.85, 1.0)),
                                       ("fill", 1.0, (60, 0, 250), (1.0, 1.0, 1.0))):
        light = bpy.data.objects.new(lname, bpy.data.lights.new(lname, "SUN"))
        light.data.energy = energy
        light.data.color = colour
        light.rotation_euler = [math.radians(a) for a in rot]
        scene.collection.objects.link(light)
    scene.render.filepath = os.path.join(OUT, "T_Icon_{}.png".format(name))
    bpy.ops.render.render(write_still=True)
    print("[ErzIcon]", scene.render.filepath)


for n in NAMEN:
    render(n)
