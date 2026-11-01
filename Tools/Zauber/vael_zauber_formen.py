# Builds meshes of spells that were engine shapes until now: a block of the Narbenwall and a lump of clay of the Lehmgolem.
#
#   blender -b --factory-startup -P Tools/Zauber/vael_zauber_formen.py -- C:/Projekte/Vael/Scripts/Data/Zauber
#
# Scripts/CreateSpellSurfaceKit.py imports them. Sizes as the engine shapes they replace, so the code keeps its scales:
# the wall block fills a cube of 100 cm around its middle, the lump a ball of 100 cm.

import math
import os
import sys

import bmesh
import bpy
import mathutils
from mathutils import noise, Vector


def out_folder():
    arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    return os.path.abspath(arguments[0]) if arguments else os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")


def material(name, color):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (*color, 1.0)
    return mat


def lumpy_ball(name, mat, radius, squash_to_cube, amount, scale, seed):
    """A ball pushed in and out by noise; with squash_to_cube it fills a cube instead"""
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=5, radius=1.0)
    offset = Vector((seed, seed * 1.7, seed * 0.3))
    for vert in bm.verts:
        p = vert.co.normalized()
        if squash_to_cube:
            biggest = max(abs(p.x), abs(p.y), abs(p.z))
            p = (p / biggest).lerp(p, 0.25)
        push = noise.fractal(p * scale + offset, 1.0, 2.0, 4) * amount
        push += max(0.0, noise.ridged_multi_fractal(p * scale * 0.7 + offset, 1.0, 2.0, 3, 0.0, 2.0) - 0.75) * amount * 0.8
        vert.co = p * radius * (1.0 + push)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    mesh.materials.append(mat)
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    return obj


def bone(name, mat, start, control, end, radius):
    """A rib: a tube along a bend that thins out to a point"""
    curve = bpy.data.curves.new(name, "CURVE")
    curve.dimensions = "3D"
    curve.bevel_depth = radius
    curve.bevel_resolution = 2
    spline = curve.splines.new("BEZIER")
    spline.bezier_points.add(2)
    for point, co in zip(spline.bezier_points, (start, control, end)):
        point.co = co
        point.handle_left_type = "AUTO"
        point.handle_right_type = "AUTO"
    spline.bezier_points[0].radius = 1.1
    spline.bezier_points[1].radius = 0.8
    spline.bezier_points[2].radius = 0.12
    obj = bpy.data.objects.new(name, curve)
    bpy.context.collection.objects.link(obj)
    curve.materials.append(mat)
    return obj


def join(parts, name):
    bpy.ops.object.select_all(action="DESELECT")
    meshes = []
    for part in parts:
        part.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.convert(target="MESH")
    bpy.ops.object.join()
    joined = bpy.context.view_layer.objects.active
    joined.name = name
    return joined


def export(obj, folder):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=os.path.join(folder, obj.name + ".fbx"), use_selection=True, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
                             mesh_smooth_type="FACE", colors_type="NONE", add_leaf_bones=False, axis_forward="-Z", axis_up="Y", use_tspace=False)
    print("exported", obj.name)


def main():
    folder = out_folder()
    os.makedirs(folder, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = 0.01

    flesh = material("Fleisch", (0.3, 0.05, 0.06))
    bones = material("Knochen", (0.8, 0.72, 0.58))
    clay = material("Lehm", (0.4, 0.26, 0.15))

    # A block of the wall: raw flesh with ribs breaking out of it on both long sides, curving up and in
    parts = [lumpy_ball("Fleisch", flesh, 52.0, True, 0.2, 2.6, 3.0)]
    for side in (-1.0, 1.0):
        for index, y in enumerate((-34.0, -12.0, 12.0, 34.0)):
            low = -22.0 + index * 6.0
            parts.append(bone("Rippe", bones, Vector((side * 42.0, y, low)), Vector((side * 66.0, y + 3.0, low + 26.0)), Vector((side * 40.0, y - 2.0, low + 62.0)), 3.4))
    wall = join(parts, "SM_Narbenwall")
    export(wall, folder)

    lump = lumpy_ball("SM_Lehmklumpen", clay, 46.0, False, 0.3, 2.2, 8.0)
    export(lump, folder)
    print("done", folder)


main()
