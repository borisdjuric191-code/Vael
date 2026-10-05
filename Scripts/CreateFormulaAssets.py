# Creates the formula data assets of Vael and keeps them complete.
#
# - A formula that doesn't exist yet is created with the values below.
# - A formula that exists only gets values filled in that are still at the class default,
#   for example fields that were added to the code after the asset was made.
#   Values that differ from the default are never overwritten, so tuning done in the editor stays.
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
Status = unreal.VaelStatus
GroundEffect = unreal.VaelGroundEffect

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
        "applied_status": Status.BURNING,
        "status_duration": 2.5,
        "status_damage_per_second": 6.0,
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
        "applied_status": Status.WET,
        "status_duration": 5.0,
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
    {
        "asset": "DA_Formula_Kettenblitz",
        "display_name": unreal.Text("Kettenblitz"),
        "description": unreal.Text("Springt über; doppelter Schaden an Nassen"),
        "elements": [Element.FIRE, Element.AIR],
        "source": Source.FREE,
        "delivery": Delivery.CHAIN,
        "damage_element": Element.AIR,
        "damage": 24.0,
        "lightning": True,
        "chain_range": 9.5 * TILE,
        "chain_jump_range": 4.0 * TILE,
        "chain_max_targets": 5,
        "chain_aim_half_angle": 29.0,
    },
    {
        "asset": "DA_Formula_Frostlanze",
        "display_name": unreal.Text("Frostlanze"),
        "description": unreal.Text("Durchbohrt und friert ein"),
        "hint": unreal.Text("Eine Schriftrolle liegt am alten Dorfbrunnen."),
        "elements": [Element.WATER, Element.AIR],
        "source": Source.SEALED,
        "delivery": Delivery.PROJECTILE,
        "damage_element": Element.WATER,
        "damage": 21.0,
        "applied_status": Status.FROZEN,
        "status_duration": 2.3,
        "projectile_speed": 17.0 * TILE,
        "projectile_radius": 0.2 * TILE,
        "projectile_lifetime": 0.9,
        "projectile_pierce": 3,
    },
    {
        "asset": "DA_Formula_Feuerball",
        "display_name": unreal.Text("Feuerball"),
        "description": unreal.Text("Explodiert in einem Flammenkreis"),
        "elements": [Element.FIRE, Element.FIRE],
        "source": Source.FREE,
        "delivery": Delivery.EXPLOSION,
        "damage_element": Element.FIRE,
        "damage": 20.0,
        "applied_status": Status.BURNING,
        "status_duration": 3.0,
        "status_damage_per_second": 8.0,
        "projectile_speed": 12.0 * TILE,
        "projectile_radius": 0.3 * TILE,
        "projectile_lifetime": 1.1,
        "explosion_radius": 1.9 * TILE,
        "explosion_damage": 34.0,
        "explosion_knockback": 4.0 * TILE,
    },
    {
        "asset": "DA_Formula_Lavaball",
        "display_name": unreal.Text("Lavaball"),
        "description": unreal.Text("Hinterlässt brennenden Boden"),
        "elements": [Element.FIRE, Element.EARTH],
        "source": Source.FREE,
        "delivery": Delivery.EXPLOSION,
        "damage_element": Element.FIRE,
        "damage": 16.0,
        "projectile_speed": 10.5 * TILE,
        "projectile_radius": 0.3 * TILE,
        "projectile_lifetime": 1.2,
        "explosion_radius": 1.5 * TILE,
        "explosion_damage": 28.0,
        "explosion_knockback": 4.0 * TILE,
        "area_radius": 1.35 * TILE,
        "area_lifetime": 5.5,
        "area_damage_per_second": 15.0,
    },
    {
        "asset": "DA_Formula_Dampfwolke",
        "display_name": unreal.Text("Dampfwolke"),
        "description": unreal.Text("Gegner im Dampf sind blind"),
        "elements": [Element.FIRE, Element.WATER],
        "source": Source.FREE,
        "delivery": Delivery.GROUND_AREA,
        "damage_element": Element.WATER,
        "area_range": 7.0 * TILE,
        "area_radius": 2.4 * TILE,
        "area_lifetime": 4.8,
        "area_damage_per_second": 7.0,
        "area_effect": GroundEffect.BLIND,
    },
    {
        "asset": "DA_Formula_Schlammfeld",
        "display_name": unreal.Text("Schlammfeld"),
        "description": unreal.Text("Verlangsamt alles im Schlamm"),
        "elements": [Element.WATER, Element.EARTH],
        "source": Source.FREE,
        "delivery": Delivery.GROUND_AREA,
        "damage_element": Element.EARTH,
        "area_range": 7.0 * TILE,
        "area_radius": 2.7 * TILE,
        "area_lifetime": 7.5,
        "area_effect": GroundEffect.SLOW,
    },
]


def comparable(value):
    """Brings Unreal values into a form that can be compared with =="""
    if isinstance(value, unreal.Text):
        return str(value)
    if isinstance(value, (list, tuple, unreal.Array)):
        return [comparable(item) for item in value]
    if isinstance(value, float):
        return round(value, 3)
    return value


def create_or_complete_formulas():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    defaults = unreal.get_default_object(unreal.VaelFormula)
    created = 0
    completed = 0

    for definition in FORMULAS:
        values = dict(definition)
        name = values.pop("asset")
        path = "{}/{}".format(FOLDER, name)

        if unreal.EditorAssetLibrary.does_asset_exist(path):
            formula = unreal.EditorAssetLibrary.load_asset(path)
            filled = []

            for key, value in values.items():
                current = comparable(formula.get_editor_property(key))
                untouched = current == comparable(defaults.get_editor_property(key))

                if untouched and current != comparable(value):
                    formula.set_editor_property(key, value)
                    filled.append(key)

            if filled and unreal.EditorAssetLibrary.save_loaded_asset(formula, False):
                completed += 1
                unreal.log("Vael: completed formula {}: {}".format(path, ", ".join(filled)))
            elif not filled:
                unreal.log("Vael: formula {} is complete, left untouched".format(path))
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

    unreal.log("Vael: {} formula assets created, {} completed".format(created, completed))


create_or_complete_formulas()
