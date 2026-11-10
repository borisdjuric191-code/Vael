"""Step 6: exports single new actions of a night creature that is already in Unreal (export/A_<Name>_Nachtrag.fbx:
the skeleton with only the named actions), so the creature's other assets are left alone.

    blender -b --factory-startup -P Tools/Nachtwesen/06_export_nachtrag.py -- <Name> <Aktion> [<Aktion> ...]
"""
import os
import sys

import bpy

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bau  # noqa: E402
import gemein  # noqa: E402

args = sys.argv[sys.argv.index("--") + 1:]
name, aktionen = args[0], args[1:]
obj, arm = gemein.oeffnen(name, "03_animationen.blend")
arm.name = "Armature"
obj.name = "SK_" + name
bau.ruhe(arm)
for aktion in list(bpy.data.actions):
    if aktion.name not in aktionen:
        bpy.data.actions.remove(aktion)
for o in bpy.context.scene.objects:
    o.select_set(o in (arm, obj))
bpy.context.view_layer.objects.active = arm
ziel = gemein.ordner(name, "export", "A_%s_Nachtrag.fbx" % name)
bpy.ops.export_scene.fbx(
    filepath=ziel, use_selection=True, object_types={"ARMATURE", "MESH"},
    apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
    add_leaf_bones=False, mesh_smooth_type="FACE", path_mode="STRIP", embed_textures=False, colors_type="LINEAR",
    bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
    bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0,
)
gemein.log(name, "Nachtrag exportiert:", ziel, "Aktionen:", [a.name for a in bpy.data.actions])
