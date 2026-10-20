# Caps all textures under /Game/Fab to a maximum size (default 2048) and saves only the changed ones.
# Run headless with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script=Scripts/CapFabTextures.py -unattended -nosplash
import unreal

ROOT = "/Game/Fab"
MAX_SIZE = 2048

registry = unreal.AssetRegistryHelpers.get_asset_registry()
assets = registry.get_assets_by_path(ROOT, recursive=True)

changed, skipped, small = [], 0, 0
for data in assets:
    if str(data.asset_class_path.asset_name) not in ("Texture2D", "TextureCube"):
        continue
    tex = unreal.EditorAssetLibrary.load_asset(str(data.package_name))
    if tex is None:
        continue
    width = tex.blueprint_get_size_x() if hasattr(tex, "blueprint_get_size_x") else 0
    if 0 < width <= MAX_SIZE:
        small += 1
        continue
    if tex.get_editor_property("max_texture_size") == MAX_SIZE:
        skipped += 1
        continue
    tex.set_editor_property("max_texture_size", MAX_SIZE)
    changed.append(str(data.package_name))

for path in changed:
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)

for path in changed:
    unreal.log_warning("[FabCap] %s" % path)
unreal.log_warning("[FabCap] %d textures capped to %d, %d already capped, %d already small enough" % (len(changed), MAX_SIZE, skipped, small))
