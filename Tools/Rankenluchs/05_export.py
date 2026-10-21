"""Step 6: export for Unreal.

  export/SK_Rankenluchs.fbx         skeleton, mesh (material slots: the Tripo body, Kamm, Augen) and all six actions
  export/T_Rankenluchs_basecolor.jpg, _normal.png, _rm.png   the Tripo textures (4K), for the Unreal materials

Axes as in the project's creature factory (Blender defaults, FBX unit scale applied): head +X stays +X in Unreal.
The armature object is called "Armature" while exporting, so Unreal takes the bone "wurzel" as the root and adds no extra one.

    blender -b --factory-startup -P skripte/05_export.py
"""
import os
import sys

import bpy

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import vorschau  # noqa: E402

ORDNER = vorschau.ORDNER
EXPORT = os.path.join(ORDNER, "export")


def main():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(ORDNER, "arbeit", "04_animationen.blend"))
    os.makedirs(EXPORT, exist_ok=True)
    luchs = bpy.data.objects["Rankenluchs"]
    arm = bpy.data.objects["Rankenluchs_Skelett"]
    arm.name = "Armature"
    luchs.name = "SK_Rankenluchs"
    if arm.animation_data:
        arm.animation_data.action = None
    for pb in arm.pose.bones:
        pb.location = (0, 0, 0)
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.scale = (1, 1, 1)

    # The Tripo textures out of the GLB, byte for byte as they were packed (JPG or PNG)
    for img in bpy.data.images:
        name = img.name.lower()
        art = next((a for a in ("basecolor", "normal", "rm") if a in name), None)
        if art is None or img.packed_file is None:
            continue
        endung = os.path.splitext(img.name)[1].lower() or ".png"
        ziel = os.path.join(EXPORT, "T_Rankenluchs_%s%s" % (art, endung))
        with open(ziel, "wb") as f:
            f.write(img.packed_file.data)
        print("[Rankenluchs] Textur", ziel)

    for o in bpy.context.scene.objects:
        o.select_set(o in (arm, luchs))
    bpy.context.view_layer.objects.active = arm
    ziel = os.path.join(EXPORT, "SK_Rankenluchs.fbx")
    bpy.ops.export_scene.fbx(
        filepath=ziel, use_selection=True, object_types={"ARMATURE", "MESH"},
        apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
        add_leaf_bones=False, mesh_smooth_type="FACE", path_mode="STRIP", embed_textures=False,
        bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
        bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0,
    )
    aktionen = [a.name for a in bpy.data.actions]
    print("[Rankenluchs] exportiert:", ziel, "Aktionen:", aktionen)
    print("[Rankenluchs] Knochen: %d, Dreiecke: %d, Materialplaetze: %s" % (
        len(arm.data.bones), sum(len(p.vertices) - 2 for p in luchs.data.polygons), [m.name for m in luchs.data.materials]))


main()
