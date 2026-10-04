# Creates the formula data assets of Vael that don't exist yet. Existing assets are never touched,
# so values tuned in the editor stay as they are.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="Scripts/CreateFormulaAssets.py" -EnablePlugins=PythonScriptPlugin
#
# Values come from the browser prototype. Its distances are in tiles, one tile is 140 cm here
# (the prototype mage walks 4.3 tiles per second, the Unreal character 600 cm per second).

import unreal

FOLDER = "/Game/Vael/Magic/Formulas"
TILE = 140.0

Element = unreal.VaelElement
Source = unreal.VaelFormulaSource
Delivery = unreal.VaelSpellDelivery

FORMULAS = [
    {
        "asset": "DA_Formula_Funke",
        "display_name": unreal.Text("Funke"),
        "description": unreal.Text("Schnelles Feuergeschoss, setzt in Brand"),
        "elements": [Element.FIRE],
        "source": Source.START,
        "delivery": Delivery.PROJECTILE,
        "damage_element": Element.FIRE,
        "damage": 14.0,
        "knockback": 0.0,
        "projectile_speed": 14.0 * TILE,
        "projectile_radius": 0.22 * TILE,
        "projectile_lifetime": 1.1,
    },
    {
        "asset": "DA_Formula_Wassergeschoss",
        "display_name": unreal.Text("Wassergeschoss"),
        "description": unreal.Text("Stößt zurück, macht nass, löscht Glut"),
        "elements": [Element.WATER],
        "source": Source.START,
        "delivery": Delivery.PROJECTILE,
        "damage_element": Element.WATER,
        "damage": 7.0,
        "knockback": 4.0 * TILE,
        "projectile_speed": 11.0 * TILE,
        "projectile_radius": 0.22 * TILE,
        "projectile_lifetime": 1.1,
    },
    {
        "asset": "DA_Formula_Steinbrocken",
        "display_name": unreal.Text("Steinbrocken"),
        "description": unreal.Text("Schwerer Treffer, zerschmettert Gefrorenes"),
        "elements": [Element.EARTH],
        "source": Source.START,
        "delivery": Delivery.PROJECTILE,
        "damage_element": Element.EARTH,
        "damage": 24.0,
        "knockback": 4.5 * TILE,
        "projectile_speed": 9.5 * TILE,
        "projectile_radius": 0.3 * TILE,
        "projectile_lifetime": 1.2,
    },
    {
        "asset": "DA_Formula_Windstoss",
        "display_name": unreal.Text("Windstoß"),
        "description": unreal.Text("Kegel, schleudert Gegner weg, facht Feuer an"),
        "elements": [Element.AIR],
        "source": Source.START,
        "delivery": Delivery.CONE,
        "damage_element": Element.AIR,
        "damage": 6.0,
        "knockback": 7.0 * TILE,
        "cone_range": 3.4 * TILE,
        "cone_half_angle": 43.0,
    },
]


def create_formulas():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    created = 0

    for definition in FORMULAS:
        values = dict(definition)
        name = values.pop("asset")
        path = "{}/{}".format(FOLDER, name)

        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.log("Vael: formula {} already exists, left untouched".format(path))
            continue

        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.VaelFormula)

        formula = asset_tools.create_asset(name, FOLDER, unreal.VaelFormula, factory)
        if formula is None:
            unreal.log_error("Vael: formula {} could not be created".format(path))
            continue

        for key, value in values.items():
            formula.set_editor_property(key, value)

        if unreal.EditorAssetLibrary.save_loaded_asset(formula, False):
            created += 1
            unreal.log("Vael: created formula {}".format(path))
        else:
            unreal.log_error("Vael: formula {} could not be saved".format(path))

    unreal.log("Vael: {} formula assets created".format(created))


create_formulas()
