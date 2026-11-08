# Materials of the engineer's hunt (2026-11-08): the break part a creature leaves when the engineer breaks its head or armour.
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateEngineerMaterials.py" -unattended -nosplash
import unreal

MATERIALS = "/Game/Vael/Items/Materials"
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

PIECES = [
    # asset, name, description, colour, rarity, look from
    ("Bruchstueck", "Bruchstück", "Ein abgebrochenes Stück Horn, Panzer oder Klaue. Nur wer gezielt schießt, bricht es heraus. Der Ingenieur baut daraus seine besten Stücke.",
     (0.78, 0.52, 0.22), unreal.VaelRarity.RARE, "DA_Material_Rollpanzer"),
]

for ident, name, text, rgb, rarity, look in PIECES:
    path = "{}/DA_Material_{}".format(MATERIALS, ident)
    asset = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset("DA_Material_" + ident, MATERIALS, unreal.VaelMaterial, unreal.DataAssetFactory())
    source = unreal.load_asset("{}/{}".format(MATERIALS, look))
    asset.set_editor_property("display_name", unreal.Text(name))
    asset.set_editor_property("description", unreal.Text(text))
    asset.set_editor_property("color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    asset.set_editor_property("category", unreal.VaelMaterialCategory.MATERIAL)
    asset.set_editor_property("rarity", rarity)
    try:
        asset.set_editor_property("kind", unreal.VaelIngredientKind.PART)
    except Exception as error:
        unreal.log_warning("[Ingenieur] kind: {}".format(error))
    if source is not None:
        for prop in ("mesh", "icon"):
            try:
                asset.set_editor_property(prop, source.get_editor_property(prop))
            except Exception:
                pass
    assets.save_loaded_asset(asset, False)
    unreal.log_warning("[Ingenieur] Material {} angelegt".format(ident))
