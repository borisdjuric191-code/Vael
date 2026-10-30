# Assembles a MetaHuman Character asset (UE Optimized, medium) into its own folder under Menschen.
# Needs the real editor (baking textures needs its render loop, a commandlet leaves them missing), editor closed:
#   UnrealEditor.exe Vael.uproject /Engine/Maps/Entry -unattended -nosplash -ExecutePythonScript=<this file>
# The character is chosen with the environment variable VAEL_MH_NAME (default MH_Magier).
# With VAEL_MH_QUIT set the editor closes itself at the end.
import os

import unreal

NAME = os.environ.get("VAEL_MH_NAME", "MH_Magier")
ROOT = "/Game/Vael/Characters/Menschen"
ASSET = f"{ROOT}/{NAME}.{NAME}"
BUILD_PATH = f"{ROOT}/{NAME}_Build"
COMMON_PATH = f"{ROOT}/MH_Common"

character = unreal.load_asset(ASSET)
if character is None:
    raise RuntimeError(f"Vael MetaHuman: {ASSET} not found")

subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not subsystem.try_add_object_to_edit(character=character):
    raise RuntimeError("Vael MetaHuman: could not open the character for edit")

try:
    if hasattr(subsystem, "get_rigging_state"):
        unreal.log(f"Vael MetaHuman: rig state = {subsystem.get_rigging_state(character=character)}")
    unreal.log(f"Vael MetaHuman: high res textures = {character.has_high_resolution_textures}")
    if not character.has_high_resolution_textures:
        # Comes from Epic's service, so this needs the login of the machine
        texture_params = unreal.MetaHumanCharacterTextureRequestParams()
        texture_params.blocking = True
        texture_params.report_progress = False
        subsystem.request_texture_sources(character=character, params=texture_params)
        unreal.log(f"Vael MetaHuman: after download high res textures = {character.has_high_resolution_textures}")
        if character.has_high_resolution_textures:
            unreal.EditorAssetLibrary.save_loaded_asset(character, only_if_is_dirty=False)
    can_build = subsystem.can_build_meta_human(character=character)
    unreal.log(f"Vael MetaHuman: can build = {can_build}")
    if not can_build:
        raise RuntimeError("Vael MetaHuman: character cannot be built (rig or textures missing)")

    params = unreal.MetaHumanCharacterEditorBuildParameters()
    params.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    params.pipeline_quality = unreal.MetaHumanQualityLevel.MEDIUM
    params.absolute_build_path = BUILD_PATH
    params.common_folder_path = COMMON_PATH
    params.enable_wardrobe_item_validation = False
    try:
        subsystem.build_meta_human(character=character, params=params)
    except RuntimeError as error:
        # Control Rig reroute warnings arrive as exceptions although the assembly goes on and succeeds
        unreal.log_warning(f"Vael MetaHuman: build reported {str(error)[:200]}")
finally:
    if subsystem.is_object_added_for_editing(character=character):
        subsystem.remove_object_to_edit(character=character)

for folder in (BUILD_PATH, COMMON_PATH):
    unreal.EditorAssetLibrary.save_directory(folder, only_if_is_dirty=True, recursive=True)
    for path in unreal.EditorAssetLibrary.list_assets(folder, recursive=True):
        data = unreal.EditorAssetLibrary.find_asset_data(path)
        if str(data.asset_class_path.asset_name) in ("SkeletalMesh", "Skeleton", "Blueprint", "AnimBlueprint", "MetaHumanCollection"):
            unreal.log(f"Vael MetaHuman: built {data.asset_class_path.asset_name} {path}")

unreal.log("Vael MetaHuman: build finished")

if os.environ.get("VAEL_MH_QUIT"):
    unreal.SystemLibrary.quit_editor()
