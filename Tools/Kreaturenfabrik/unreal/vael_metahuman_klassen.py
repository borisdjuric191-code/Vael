# MetaHumans of the other classes from the mage's MetaHuman (Boris 2026-11-08): the engineer stocky and strong, the Floromantin
# slender and light. MH_Magier is copied (it is never changed), the body is set by measurements, the skin tone and, for the
# Floromantin, the face are moved, then the character is built like vael_metahuman_build.py does.
# Needs the real editor (textures and rig come from Epic's service with the login of the machine), editor closed:
#   set VAEL_MH_KLASSE=Ingenieur (or Floromantin), VAEL_MH_QUIT=1
#   UnrealEditor.exe Vael.uproject /Engine/Maps/Entry -unattended -nosplash -ExecutePythonScript=<this file>
import os
import random

import unreal

KLASSE = os.environ.get("VAEL_MH_KLASSE", "Ingenieur")
NAME = "MH_" + KLASSE
ROOT = "/Game/Vael/Characters/Menschen"
SOURCE = ROOT + "/MH_Magier"
ASSET = "{}/{}".format(ROOT, NAME)
BUILD_PATH = "{}/{}_Build".format(ROOT, NAME)
COMMON_PATH = ROOT + "/MH_Common"

# Body measurements per class (cm, and the -2..2 sliders); the mage is 187.5 tall, Fat -0.9, Muscularity 0.2, Masculine -1.0
BODIES = {
    "Ingenieur": {"Height": 179.0, "Fat": 0.4, "Muscularity": 1.5, "Masculine/Feminine": -1.4, "Across Shoulder": 41.0, "Chest": 112.0,
                  "Waist": 92.0, "Bicep": 37.0, "Forearm": 32.0, "Neck": 43.0, "Thigh": 61.0},
    "Floromantin": {"Height": 163.0, "Fat": -1.3, "Muscularity": -0.7, "Masculine/Feminine": 1.7, "Waist": 62.0, "Hip": 92.0,
                    "Across Shoulder": 31.0, "Chest": 84.0, "Bicep": 24.0, "Neck": 31.5},
}
# Skin tone (u, v on the skin palette): the engineer weathered and darker from the forge, the Floromantin pale with a green cast
SKIN = {"Ingenieur": (0.52, 0.55), "Floromantin": (0.18, 0.30)}


def log(text):
    unreal.log_warning("Vael MetaHuman Klasse: " + text)


lib = unreal.EditorAssetLibrary
character = unreal.load_asset(ASSET)
if character is None:
    character = lib.duplicate_asset(SOURCE, ASSET)
    log("{} aus MH_Magier kopiert".format(NAME))
if character is None:
    raise RuntimeError("could not make " + ASSET)

subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not subsystem.try_add_object_to_edit(character=character):
    raise RuntimeError("could not open the character for edit")

try:
    # Body
    wanted = BODIES[KLASSE]
    constraints = subsystem.get_body_constraints(character=character)
    for constraint in constraints:
        name = str(constraint.name)
        if name in wanted:
            constraint.is_active = True
            constraint.target_measurement = max(constraint.min_measurement, min(constraint.max_measurement, wanted[name]))
    subsystem.set_body_constraints(character=character, body_constraints=constraints)
    log("Koerper gesetzt: " + ", ".join("{}={}".format(k, v) for k, v in wanted.items()))

    # Skin tone
    skin_settings = character.get_editor_property("skin_settings")
    skin = skin_settings.get_editor_property("skin")
    skin.set_editor_property("u", SKIN[KLASSE][0])
    skin.set_editor_property("v", SKIN[KLASSE][1])
    skin_settings.set_editor_property("skin", skin)
    subsystem.commit_skin_settings(character=character, skin_settings=skin_settings)
    log("Haut u={} v={}".format(*SKIN[KLASSE]))

    # Face: the Floromantin gets a softer, smaller face than the mage's (a move in the face model, seeded so it stays the same)
    if KLASSE == "Floromantin" and os.environ.get("VAEL_MH_GESICHT", "1") == "1":
        coefficients = list(subsystem.get_face_model_coefficients(character=character))
        log("Gesicht: {} Koeffizienten, erste {}".format(len(coefficients), [round(c, 3) for c in coefficients[:8]]))
        rng = random.Random(1107)
        scale = float(os.environ.get("VAEL_MH_GESICHT_STAERKE", "0.6"))
        moved = [c * (1.0 - scale * 0.5) + rng.gauss(0.0, scale * 0.35) for c in coefficients]
        subsystem.set_face_model_coefficients(character=character, coefficients=moved)
        log("Gesicht verschoben (Staerke {})".format(scale))

    unreal.EditorAssetLibrary.save_loaded_asset(character, only_if_is_dirty=False)

    if not subsystem.can_build_meta_human(character=character):
        log("braucht ein neues Rig: Auto-Rigging anfordern")
        try:
            params = unreal.MetaHumanCharacterAutoRiggingRequestParams()
            params.blocking = True
            params.report_progress = False
            subsystem.request_auto_rigging(character=character, params=params)
        except Exception as error:
            log("Auto-Rigging: {}".format(error))
    if not character.has_high_resolution_textures:
        texture_params = unreal.MetaHumanCharacterTextureRequestParams()
        texture_params.blocking = True
        texture_params.report_progress = False
        subsystem.request_texture_sources(character=character, params=texture_params)
    unreal.EditorAssetLibrary.save_loaded_asset(character, only_if_is_dirty=False)
    log("kann bauen: {}".format(subsystem.can_build_meta_human(character=character)))

    params = unreal.MetaHumanCharacterEditorBuildParameters()
    params.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    params.pipeline_quality = unreal.MetaHumanQualityLevel.MEDIUM
    params.absolute_build_path = BUILD_PATH
    params.common_folder_path = COMMON_PATH
    params.enable_wardrobe_item_validation = False
    try:
        subsystem.build_meta_human(character=character, params=params)
    except RuntimeError as error:
        log("Build meldet {}".format(str(error)[:200]))
finally:
    if subsystem.is_object_added_for_editing(character=character):
        subsystem.remove_object_to_edit(character=character)

for folder in (BUILD_PATH, COMMON_PATH):
    unreal.EditorAssetLibrary.save_directory(folder, only_if_is_dirty=True, recursive=True)
for path in unreal.EditorAssetLibrary.list_assets(BUILD_PATH, recursive=True):
    data = unreal.EditorAssetLibrary.find_asset_data(path)
    if str(data.asset_class_path.asset_name) in ("SkeletalMesh", "Blueprint"):
        log("gebaut {} {}".format(data.asset_class_path.asset_name, path))
log("fertig")

if os.environ.get("VAEL_MH_QUIT"):
    unreal.SystemLibrary.quit_editor()
