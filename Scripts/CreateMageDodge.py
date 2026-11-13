# Die Ausweichrolle des Magiers ohne Zurückspringen (Boris 13.11.2026: "springt sehr weit, wird dann zurückgezogen").
#
# Die bisherige Montage AM_Mage_Dodge spielt MM_Dash der Mannequin-Vorlage. Dessen Wurzelknochen läuft 8,75 m nach vorn,
# ohne Root Motion: die Figur schießt der Kapsel (5,6 m, AVaelCharacter::DodgeSpeed x DodgeDuration) davon und springt am
# Ende auf sie zurück. Hier entstehen eigene Kopien, die Vorlage und AM_Mage_Dodge bleiben unberührt:
# - A_Mage_Ausweichen: MM_Dash mit festgehaltener Wurzel (Force Root Lock), die Figur geht genau mit der Kapsel.
# - AM_Mage_Ausweichen: Montage darauf, kurzes Einblenden, weiches Ausblenden. AVaelCharacter::DodgeMontage zeigt darauf.
#
# Mit geschlossenem Editor:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateMageDodge.py" -EnablePlugins=PythonScriptPlugin

import unreal

SOURCE = "/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Dash"
FOLDER = "/Game/Vael/Characters/Mage"
SEQUENCE = FOLDER + "/A_Mage_Ausweichen"
MONTAGE = FOLDER + "/AM_Mage_Ausweichen"

assets = unreal.EditorAssetLibrary


def log(message):
    unreal.log_warning("[CreateMageDodge] " + message)


SOFTEN = 0.65      # wie viel der Bewegung von MM_Dash bleibt (Boris: zu überstreckt, wie ein Hechtsprung)
HOP = 0.5          # wie viel des Hüpfers (Becken nach oben) bleibt


def nlerp(a, b, t):
    """Zwischen zwei Drehungen (normiert, kürzester Weg; für die Winkel einer Pose genau genug)"""
    sign = 1.0 if a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w >= 0.0 else -1.0
    x, y, z, w = (a.x + (b.x * sign - a.x) * t, a.y + (b.y * sign - a.y) * t, a.z + (b.z * sign - a.z) * t, a.w + (b.w * sign - a.w) * t)
    n = (x * x + y * y + z * z + w * w) ** 0.5 or 1.0
    return unreal.Quat(x / n, y / n, z / n, w / n)


def soften(sequence):
    """Jeder Knochen nur SOFTEN so weit von seiner Startpose weg, das Becken hüpft nur HOP so hoch"""
    controller = sequence.controller
    frames = unreal.AnimationLibrary.get_num_frames(sequence)
    bones = [str(n) for n in unreal.AnimationLibrary.get_animation_track_names(sequence)]
    changed = 0
    for bone in bones:
        poses = [unreal.AnimationLibrary.get_bone_pose_for_frame(sequence, bone, f, False) for f in range(frames)]
        start = poses[0]
        positions, rotations, scales = [], [], []
        for pose in poses:
            rotation = nlerp(start.rotation, pose.rotation, SOFTEN)
            position = pose.translation
            if bone == "pelvis":
                position = unreal.Vector(start.translation.x + (position.x - start.translation.x) * SOFTEN,
                                         start.translation.y + (position.y - start.translation.y) * SOFTEN,
                                         start.translation.z + (position.z - start.translation.z) * HOP)
            positions.append(position)
            rotations.append(rotation)
            scales.append(pose.scale3d)
        controller.set_bone_track_keys(bone, positions, rotations, scales, True)
        changed += 1
    return changed


def main():
    if assets.does_asset_exist(SEQUENCE):
        assets.delete_asset(SEQUENCE)
    assets.duplicate_asset(SOURCE, SEQUENCE)
    sequence = unreal.load_asset(SEQUENCE)
    log("{} Knochen abgemildert".format(soften(sequence)))
    sequence.set_editor_property("enable_root_motion", False)
    sequence.set_editor_property("force_root_lock", True)
    sequence.set_editor_property("root_motion_root_lock", unreal.RootMotionRootLock.REF_POSE)
    assets.save_loaded_asset(sequence, False)

    montage = unreal.load_asset(MONTAGE) if assets.does_asset_exist(MONTAGE) else None
    if montage is None:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("source_animation", sequence)
        factory.set_editor_property("target_skeleton", sequence.get_editor_property("skeleton"))
        name = MONTAGE.rsplit("/", 1)[1]
        montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, unreal.AnimMontage, factory)
    else:
        # Die Montage bleibt, ihr Abschnitt zeigt auf die neu angelegte Animation
        tracks = montage.get_editor_property("slot_anim_tracks")
        for track in tracks:
            anim_track = track.get_editor_property("anim_track")
            segments = anim_track.get_editor_property("anim_segments")
            for segment in segments:
                segment.set_editor_property("anim_reference", sequence)
            anim_track.set_editor_property("anim_segments", segments)
            track.set_editor_property("anim_track", anim_track)
        montage.set_editor_property("slot_anim_tracks", tracks)
    blend_in = montage.get_editor_property("blend_in")
    blend_in.set_editor_property("blend_time", 0.06)
    montage.set_editor_property("blend_in", blend_in)
    blend_out = montage.get_editor_property("blend_out")
    blend_out.set_editor_property("blend_time", 0.22)
    montage.set_editor_property("blend_out", blend_out)
    assets.save_loaded_asset(montage, False)

    root = [unreal.AnimationLibrary.get_bone_pose_for_time(sequence, "root", t, False).translation for t in (0.0, sequence.get_play_length())]
    log("{} und {} angelegt, Länge {:.2f} s; Wurzel (Rohdaten) {:.0f} cm -> mit Root Lock fest".format(
        SEQUENCE, MONTAGE, sequence.get_play_length(), (root[1] - root[0]).length()))


main()
