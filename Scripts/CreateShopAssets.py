# Creates the shops of traders, like the Gildenkontor of Maren Holt. A shop that exists already is left untouched,
# so prices tuned in the editor stay.
#
# Boris' decision (2026-10-10): guild coins only come from contracts; Maren sells healing potions and rare materials,
# no gear and no formula hints.
#
# Run with the editor closed, after Scripts/CreateMaterialAssets.py:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreateShopAssets.py" -EnablePlugins=PythonScriptPlugin

import unreal

FOLDER = "/Game/Vael/Items/Shops"
MATERIALS = "/Game/Vael/Items/Materials"


def offer(material, price):
    result = unreal.VaelShopOffer()
    result.set_editor_property("material", unreal.load_asset("{}/{}".format(MATERIALS, material)))
    result.set_editor_property("price", price)
    return result


SHOPS = [
    {
        "asset": "DA_Shop_Gildenkontor",
        "title": "Gildenkontor",
        "offers": [
            ("DA_Material_Heiltrank", 4),
            ("DA_Material_Kelchtau", 3),
            ("DA_Material_Glockenglut", 5),
            ("DA_Material_Russquarz", 6),
        ],
    },
]


def create_shops():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    for definition in SHOPS:
        path = "{}/{}".format(FOLDER, definition["asset"])
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.log("Vael: shop {} exists, left untouched".format(path))
            continue

        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.VaelShop)
        shop = asset_tools.create_asset(definition["asset"], FOLDER, unreal.VaelShop, factory)
        shop.set_editor_property("title", unreal.Text(definition["title"]))
        shop.set_editor_property("offers", [offer(material, price) for material, price in definition["offers"]])
        unreal.EditorAssetLibrary.save_loaded_asset(shop, False)
        unreal.log_warning("Vael: created shop {}".format(path))


create_shops()
