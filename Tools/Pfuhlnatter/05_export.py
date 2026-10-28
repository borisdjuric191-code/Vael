"""Step 5: export for Unreal.

  export/SK_Pfuhlnatter.fbx      skeleton, mesh and all eight actions
  export/T_Pfuhlnatter_basecolor.jpg, _normal.png, _roughness.jpg, _metallic.jpg   the Tripo textures (4K), copied

Axes as in Tools/Rankenluchs (Blender defaults, FBX unit scale applied): head +X stays +X in Unreal.
The armature object is called "Armature" while exporting, so Unreal takes the bone "wurzel" as the root and adds no extra one.

    blender -b --factory-startup -P Tools/Pfuhlnatter/05_export.py
"""
import glob
import os
import shutil
import sys

import bpy

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import vorschau  # noqa: E402

EXPORT = os.path.join(vorschau.ORDNER, "export")


def main():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(vorschau.ARBEIT, "04_animationen.blend"))
    os.makedirs(EXPORT, exist_ok=True)
    # The actions are keyed at 30 frames per second (04_animationen.py); the scene's default 24 would stretch them
    bpy.context.scene.render.fps = 30
    natter = bpy.data.objects["Pfuhlnatter"]
    arm = bpy.data.objects["Pfuhlnatter_Skelett"]
    arm.name = "Armature"
    natter.name = "SK_Pfuhlnatter"
    if arm.animation_data:
        arm.animation_data.action = None
    for pb in arm.pose.bones:
        pb.location = (0, 0, 0)
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.scale = (1, 1, 1)

    # The Tripo textures lie next to its FBX
    for datei in glob.glob(os.path.join(vorschau.ORDNER, "02_tripo", "*.fbm", "*")):
        name = os.path.basename(datei).lower()
        art = next((a for a in ("basecolor", "normal", "roughness", "metallic") if a in name), None)
        if art:
            ziel = os.path.join(EXPORT, "T_Pfuhlnatter_%s%s" % (art, ".png" if name.endswith(".png") else ".jpg"))
            shutil.copyfile(datei, ziel)
            print("[Pfuhlnatter] Textur", ziel)

    for o in bpy.context.scene.objects:
        o.select_set(o in (arm, natter))
    bpy.context.view_layer.objects.active = arm
    ziel = os.path.join(EXPORT, "SK_Pfuhlnatter.fbx")
    bpy.ops.export_scene.fbx(
        filepath=ziel, use_selection=True, object_types={"ARMATURE", "MESH"},
        apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
        add_leaf_bones=False, mesh_smooth_type="FACE", path_mode="STRIP", embed_textures=False, colors_type="LINEAR",
        bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
        bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0,
    )
    print("[Pfuhlnatter] exportiert:", ziel, "Aktionen:", [a.name for a in bpy.data.actions])
    print("[Pfuhlnatter] Knochen: %d, Dreiecke: %d, Materialplaetze: %s" % (
        len(arm.data.bones), sum(len(p.vertices) - 2 for p in natter.data.polygons), [m.name for m in natter.data.materials]))


main()
