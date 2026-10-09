# Creates the data of the plants, fungi and stones players can gather, and keeps them complete.
#
# - A harvestable that doesn't exist yet is created with the values below.
# - One that exists only gets values filled in that are still at the class default (an empty loot list counts as default).
#   Values that differ from the default are never overwritten, so tuning done in the editor stays.
#
# Run with the editor closed, after Scripts/CreateMaterialAssets.py (the loot points at its materials):
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateHarvestableAssets.py" -EnablePlugins=PythonScriptPlugin
#
# The kinds follow the Lore-Bibel, tab Kompendium, section "Arten der Regionen". Region gives the base color, element the glow.

import unreal

FOLDER = "/Game/Vael/Nature"
MATERIALS = "/Game/Vael/Items/Materials"

Kind = unreal.VaelHarvestKind


def color(hex_code):
    """Linear color from an sRGB hex code like 'ff7a2e'"""
    def to_linear(channel):
        value = int(channel, 16) / 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4

    return unreal.LinearColor(r=to_linear(hex_code[0:2]), g=to_linear(hex_code[2:4]), b=to_linear(hex_code[4:6]), a=1.0)


def loot(material, chance=1.0, min_count=1, max_count=1):
    """One loot entry; the material is the asset name in the materials folder"""
    return (material, chance, min_count, max_count)


HARVESTABLES = [
    # Plants
    {"asset": "DA_Harvest_Glutdistel", "display_name": unreal.Text("Glutdistel"), "compendium_id": "Glutdistel", "kind": Kind.PLANT,
     "loot": [loot("DA_Material_Glutsamen", 1.0, 1, 2)], "regrow_seconds": 90.0, "height": 80.0, "base_color": color("1a1512"), "glow_color": color("ff7a2e")},
    {"asset": "DA_Harvest_Traenenkelch", "display_name": unreal.Text("Tränenkelch"), "compendium_id": "Traenenkelch", "kind": Kind.PLANT,
     "loot": [loot("DA_Material_Kelchtau", 1.0, 1, 2)], "regrow_seconds": 120.0, "height": 90.0, "base_color": color("4a4640"), "glow_color": color("bfe3ff")},
    {"asset": "DA_Harvest_Aschblase", "display_name": unreal.Text("Aschblase"), "compendium_id": "Aschblase", "kind": Kind.PLANT,
     "loot": [loot("DA_Material_Rauchblase", 1.0, 1, 1)], "regrow_seconds": 90.0, "height": 70.0, "base_color": color("2a2622"), "glow_color": color("9aa3a8")},
    {"asset": "DA_Harvest_Laternenglocke", "display_name": unreal.Text("Laternenglocke"), "compendium_id": "Laternenglocke", "kind": Kind.PLANT,
     "loot": [loot("DA_Material_Glockenglut", 1.0, 1, 1)], "regrow_seconds": 120.0, "height": 100.0, "base_color": color("5e5852"), "glow_color": color("ffb259")},
    {"asset": "DA_Harvest_Russmoos", "display_name": unreal.Text("Rußmoos"), "compendium_id": "Russmoos", "kind": Kind.PLANT,
     "loot": [loot("DA_Material_Russmoos", 1.0, 1, 3)], "regrow_seconds": 60.0, "height": 25.0, "base_color": color("2b2b27"), "glow_color": color("4d5a3a")},
    {"asset": "DA_Harvest_Fleischkelch", "display_name": unreal.Text("Fleischkelch"), "compendium_id": "Fleischkelch", "kind": Kind.PLANT,
     "loot": [loot("DA_Material_Kelchfleisch", 1.0, 1, 1)], "regrow_seconds": 180.0, "height": 110.0, "base_color": color("6b2e3a"), "glow_color": color("a24dff")},
    # Fungi
    {"asset": "DA_Harvest_Pfeifenmorchel", "display_name": unreal.Text("Pfeifenmorchel"), "compendium_id": "Pfeifenmorchel", "kind": Kind.FUNGUS,
     "loot": [loot("DA_Material_Pfeifensporen", 1.0, 1, 2)], "regrow_seconds": 120.0, "height": 60.0, "base_color": color("5a3a2a"), "glow_color": color("b0442a")},
    {"asset": "DA_Harvest_Aschenstaeubling", "display_name": unreal.Text("Aschenstäubling"), "compendium_id": "Aschenstaeubling", "kind": Kind.FUNGUS,
     "loot": [loot("DA_Material_Staeublingspulver", 1.0, 1, 2)], "regrow_seconds": 90.0, "height": 30.0, "base_color": color("6e6862"), "glow_color": color("9c958d")},
    {"asset": "DA_Harvest_Adernflechte", "display_name": unreal.Text("Adernflechte"), "compendium_id": "Adernflechte", "kind": Kind.FUNGUS,
     "loot": [loot("DA_Material_Myzelprobe", 1.0, 1, 1)], "regrow_seconds": 150.0, "height": 15.0, "base_color": color("d8d2c4"), "glow_color": color("eae4d6")},
    # Stones
    {"asset": "DA_Harvest_Schlacke", "display_name": unreal.Text("Schlacke"), "compendium_id": "Schlacke", "kind": Kind.STONE,
     "loot": [loot("DA_Material_Schlacke", 1.0, 1, 3)], "regrow_seconds": 240.0, "height": 50.0, "base_color": color("1c1a1c"), "glow_color": color("3a3640")},
    {"asset": "DA_Harvest_Glutstein", "display_name": unreal.Text("Glutstein"), "compendium_id": "Glutstein", "kind": Kind.STONE,
     "loot": [loot("DA_Material_Glutstein", 1.0, 1, 2)], "regrow_seconds": 240.0, "height": 90.0, "base_color": color("3a302a"), "glow_color": color("ff6a1a")},
    {"asset": "DA_Harvest_Russquarz", "display_name": unreal.Text("Rußquarz"), "compendium_id": "Russquarz", "kind": Kind.STONE,
     "loot": [loot("DA_Material_Russquarz", 1.0, 1, 2)], "regrow_seconds": 240.0, "height": 60.0, "base_color": color("222020"), "glow_color": color("8c8a90")},
    {"asset": "DA_Harvest_Markdruse", "display_name": unreal.Text("Markdruse"), "compendium_id": "Markdruse", "kind": Kind.STONE,
     "loot": [loot("DA_Material_Markkristall", 1.0, 1, 1)], "regrow_seconds": 300.0, "height": 110.0, "base_color": color("2a2430"), "glow_color": color("a24dff")},
]


def comparable(value):
    """Brings Unreal values into a form that can be compared with =="""
    if isinstance(value, unreal.Text):
        return str(value)
    if isinstance(value, unreal.LinearColor):
        return (round(value.r, 3), round(value.g, 3), round(value.b, 3), round(value.a, 3))
    if isinstance(value, float):
        return round(value, 3)
    if isinstance(value, unreal.Name):
        return str(value)
    return value


def make_loot(entries):
    """Loot entries as Unreal structs; a missing material is reported and left out"""
    result = []
    for material, chance, min_count, max_count in entries:
        asset = unreal.load_asset("{}/{}".format(MATERIALS, material))
        if asset is None:
            unreal.log_error("Vael: material {} is missing, run CreateMaterialAssets.py first".format(material))
            continue

        entry = unreal.VaelLootEntry()
        entry.set_editor_property("material", asset)
        entry.set_editor_property("chance", chance)
        entry.set_editor_property("min_count", min_count)
        entry.set_editor_property("max_count", max_count)
        result.append(entry)
    return result


def create_or_complete_harvestables():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    defaults = unreal.get_default_object(unreal.VaelHarvestableData)
    created = 0
    completed = 0

    for definition in HARVESTABLES:
        values = dict(definition)
        name = values.pop("asset")
        loot_entries = values.pop("loot")
        values["compendium_id"] = unreal.Name(values["compendium_id"])
        path = "{}/{}".format(FOLDER, name)

        if unreal.EditorAssetLibrary.does_asset_exist(path):
            data = unreal.EditorAssetLibrary.load_asset(path)
            filled = []

            for key, value in values.items():
                current = comparable(data.get_editor_property(key))
                untouched = current == comparable(defaults.get_editor_property(key))

                if untouched and current != comparable(value):
                    data.set_editor_property(key, value)
                    filled.append(key)

            if len(data.get_editor_property("loot")) == 0:
                data.set_editor_property("loot", make_loot(loot_entries))
                filled.append("loot")

            if filled and unreal.EditorAssetLibrary.save_loaded_asset(data, False):
                completed += 1
                unreal.log("Vael: completed harvestable {}: {}".format(path, ", ".join(filled)))
            continue

        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.VaelHarvestableData)

        data = asset_tools.create_asset(name, FOLDER, unreal.VaelHarvestableData, factory)
        if data is None:
            unreal.log_error("Vael: harvestable {} could not be created".format(path))
            continue

        for key, value in values.items():
            data.set_editor_property(key, value)
        data.set_editor_property("loot", make_loot(loot_entries))

        if unreal.EditorAssetLibrary.save_loaded_asset(data, False):
            created += 1
            unreal.log("Vael: created harvestable {}".format(path))
        else:
            unreal.log_error("Vael: harvestable {} could not be saved".format(path))

    unreal.log("Vael: {} harvestable assets created, {} completed".format(created, completed))


create_or_complete_harvestables()
