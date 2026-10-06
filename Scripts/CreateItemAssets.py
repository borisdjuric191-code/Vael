# Creates the handmade item data assets of Vael, like the legendary Sturmmantel.
#
# - An item that doesn't exist yet is created with the values below.
# - Existing items are never touched, so tuning done in the editor stays.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="Scripts/CreateItemAssets.py" -EnablePlugins=PythonScriptPlugin

import unreal

FOLDER = "/Game/Vael/Items/Gear"

Stat = unreal.VaelItemStat
Slot = unreal.VaelItemSlot
Rarity = unreal.VaelRarity


def stat(kind, value):
    return unreal.VaelItemStatValue(stat=kind, value=value)


def tags(*names):
    return unreal.VaelScriptStatics.make_tag_container([unreal.Name(name) for name in names])


ITEMS = [
    {
        # Decided by Boris on 2026-10-06: immune to rain and lightning, water spells and the chain lightning stronger; lies in a chest
        "asset": "DA_Item_Sturmmantel",
        "display_name": unreal.Text("Sturmmantel"),
        "slot": Slot.CHEST,
        "rarity": Rarity.LEGENDARY,
        "stats": [stat(Stat.MAX_HEALTH, 15.0), stat(Stat.MANA_REGEN, 2.0)],
        "legendary_ability": unreal.VaelLegendaryAbility(
            description=unreal.Text("Regen und Blitze können dir nichts anhaben. Wasserzauber und Kettenblitz treffen um 25 % härter."),
            granted_tags=tags("Gear.WeatherWard"),
            bonus_stats=[stat(Stat.WATER_DAMAGE, 25.0), stat(Stat.LIGHTNING_DAMAGE, 25.0)],
        ),
    },
]


def create_items():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    created = 0

    for definition in ITEMS:
        values = dict(definition)
        name = values.pop("asset")
        path = "{}/{}".format(FOLDER, name)

        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.log("Vael: item {} exists, left untouched".format(path))
            continue

        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.VaelItemData)

        item = asset_tools.create_asset(name, FOLDER, unreal.VaelItemData, factory)
        if item is None:
            unreal.log_error("Vael: item {} could not be created".format(path))
            continue

        for key, value in values.items():
            item.set_editor_property(key, value)

        if unreal.EditorAssetLibrary.save_loaded_asset(item, False):
            created += 1
            unreal.log("Vael: created item {}".format(path))
        else:
            unreal.log_error("Vael: item {} could not be saved".format(path))

    unreal.log("Vael: {} item assets created".format(created))


create_items()
