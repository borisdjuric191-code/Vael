# Creates the potions of the Rezeptbuch (Lore-Bibel, tab Rezeptbuch, Tränke der Aschenmark) as materials of the bag, so the
# witch's kitchen at the root table of the Glutkammer (FVaelBrewing, Source/Vael/Items/VaelBrewing.cpp) has something to brew.
#
# - DA_Material_Trank_<Id> in /Game/Vael/Items/Materials: name, what it does and how long, way, colour of the liquid, potion
#   category, drunk from the bag (use DRINK; effects other than healing come later). The Heiltrank of the guild stays as it is.
# - The model is the bottle of the loot kit (SM_Loot_Flasche) with its liquid in an own instance MI_Loot_Trank_<Id>.
# - Only creates its own assets and updates them on every run; the list must match MakeRecipes() in VaelBrewing.cpp.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/CreatePotionAssets.py" -unattended -nosplash

import unreal

MATERIALS = "/Game/Vael/Items/Materials"
LOOT = "/Game/Vael/Items/Loot"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.MaterialEditingLibrary
W = unreal.VaelWay
C = unreal.VaelMaterialCategory
R = unreal.VaelRarity
U = unreal.VaelMaterialUse


def color(hex_code):
    def to_linear(channel):
        value = int(channel, 16) / 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4
    return unreal.LinearColor(r=to_linear(hex_code[0:2]), g=to_linear(hex_code[2:4]), b=to_linear(hex_code[4:6]), a=1.0)


# id, name, way, tier (rarity of the bottle), colour, what it does, how long; same order as in VaelBrewing.cpp
POTIONS = [
    ("Manatrank", "Manatrank", W.NEUTRAL, 0, "4a8fe0", "Füllt 40 % Mana, danach schnellere Erholung.", "sofort, Erholung 5 Min."),
    ("Nachtsicht", "Nachtsicht", W.NEUTRAL, 0, "9fd0a0", "Sieht im Dunkeln, zeigt versteckte Funde.", "15 Min."),
    ("Loeschtrank", "Löschtrank", W.NEUTRAL, 0, "8fb8c8", "Löscht Brennen, danach weniger Feuerschaden.", "sofort, Schutz 5 Min."),
    ("Ascheschleier", "Ascheschleier", W.NEUTRAL, 0, "8a8580", "Gegner bemerken dich später; 2 Ladungen: 10 Sekunden unsichtbar.", "15 Min."),
    ("Leichtfuss", "Leichtfuß", W.LIGHT, 0, "bfe6ff", "20 % schneller.", "15 Min."),
    ("Rindenhaut", "Rindenhaut", W.LIGHT, 0, "9c7a4a", "+20 % Rüstung.", "15 Min."),
    ("Feuerschutz", "Feuerschutz", W.LIGHT, 0, "ffb04a", "30 % weniger Feuerschaden.", "15 Min."),
    ("Giftphiole", "Giftphiole", W.SHADOW, 0, "6fae3a", "Giftwolke, Gegner bluten.", "sofort"),
    ("Ermattungsfluch", "Ermattungsfluch", W.SHADOW, 0, "6a4a8a", "Gegner langsamer und verwirrt.", "sofort, Fluch 1 Min."),
    ("Blutoel", "Blutöl", W.SHADOW, 0, "8f1620", "Waffe raubt 10 % Leben.", "15 Min."),
    ("Schwarzbrand", "Schwarzbrand", W.SHADOW, 0, "3a1a2e", "Feuer, das sich schwer löschen lässt.", "sofort"),
    ("Gruppenheilung", "Gruppenheilung", W.LIGHT, 1, "ff8f8f", "Heilwolke für alle Verbündeten darin.", "sofort"),
    ("Saeurewolke", "Säurewolke", W.SHADOW, 1, "b8d23a", "Zerfrisst Rüstung in einer Fläche.", "sofort"),
    ("Reinigungstrank", "Reinigungstrank", W.LIGHT, 1, "f2f0d0", "Senkt die eigene Verderbnis um 10 %.", "sofort"),
    ("Daemmertrank", "Dämmertrank", W.TWILIGHT, 1, "b06a8a", "Heilt bei Treffern ein wenig und wirft Schaden auf Angreifer zurück.", "15 Min."),
    ("Hainruf", "Hainruf", W.LIGHT, 2, "6fcf5a", "Pflanzen gehorchen dir: alle 20 Sekunden umschlingt eine Ranke einen Gegner, Schlafpollen betäuben.", "20 Min."),
    ("Wuergherrschaft", "Würgherrschaft", W.SHADOW, 2, "4a2a3a", "Pflanzen kriechen auf Gegner zu, umschlingen und vergiften sie; 3 Ladungen: einen Gegner unter die Erde ziehen.", "20 Min."),
    ("Feenleib", "Feenleib", W.LIGHT, 2, "e8f5a0", "Du schwebst über Schlamm, alle 5 Sekunden trifft ein Funke der Hain-Geister einen nahen Gegner.", "30 Min."),
    ("Glutblut", "Glutblut", W.NEUTRAL, 2, "ff5a1a", "Halber Feuerschaden, du hinterlässt eine Glutspur, Feuerquellen füllen dein Mana.", "30 Min."),
    ("Aschegestalt", "Aschegestalt", W.NEUTRAL, 2, "6e6a66", "15 % ausweichen; 3 Ladungen: als Rauch durch Gegner gleiten, 3 Sekunden unverwundbar.", "20 Min."),
    ("Sternenblick", "Sternenblick", W.LIGHT, 2, "c8c0ff", "Du siehst verborgene Wege, Schätze und die Fundorte versiegelter Formeln.", "30 Min."),
    ("Blutrausch", "Blutrausch", W.SHADOW, 2, "a01a2a", "Jeder Treffer heilt ein wenig und macht schneller, bis zu 5-fach; im Kampf verlierst du langsam Leben.", "15 Min."),
    ("Daemmergarten", "Dämmergarten", W.TWILIGHT, 2, "8a5a7a", "Die Pflanzen wechseln alle 10 Sekunden zwischen Heilen und Würgen.", "20 Min."),
]
TIER_TEXT = ["Grundtrank", "Stärkerer Trank", "Krafttrank"]


def log(message):
    unreal.log_warning("[Traenke] " + message)


def liquid(name, hex_code, way):
    """Own instance of the bottle liquid of the loot kit in the colour of the potion; shadow brews are murky and pulse"""
    template = unreal.load_asset(LOOT + "/Materialien/MI_Loot_Klartrunk")
    parent = template.get_editor_property("parent") if template is not None else None
    if parent is None:
        return None
    path = LOOT + "/Materialien/MI_Loot_Trank_" + name
    instance = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
        "MI_Loot_Trank_" + name, LOOT + "/Materialien", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(instance, parent)
    c = color(hex_code)
    shadow = way in (W.SHADOW, W.TWILIGHT)
    library.set_material_instance_vector_parameter_value(instance, "Farbe", unreal.LinearColor(c.r, c.g, c.b, 1.0))
    library.set_material_instance_vector_parameter_value(instance, "AderFarbe", unreal.LinearColor(min(1.0, c.r * 1.6 + 0.2), min(1.0, c.g * 1.6 + 0.2), min(1.0, c.b * 1.6 + 0.2), 1.0))
    for key, value in (("Leuchten", 0.8), ("Truebung", 0.6 if shadow else 0.1), ("Adern", 0.0), ("Puls", 0.25 if shadow else 0.08), ("PulsTempo", 0.5 if shadow else 0.3)):
        library.set_material_instance_scalar_parameter_value(instance, key, value)
    library.update_material_instance(instance)
    assets.save_loaded_asset(instance, False)
    return instance


def build():
    bottle = unreal.load_asset(LOOT + "/Modelle/SM_Loot_Flasche")
    inner = unreal.load_asset(LOOT + "/Modelle/SM_Loot_Flasche_Fluessigkeit")
    made = 0
    for ident, name, way, tier, hex_code, effect, duration in POTIONS:
        asset_name = "DA_Material_Trank_" + ident
        path = MATERIALS + "/" + asset_name
        asset = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(asset_name, MATERIALS, unreal.VaelMaterial, unreal.DataAssetFactory())
        if asset is None:
            log("konnte nicht anlegen: " + path)
            continue
        asset.set_editor_property("display_name", unreal.Text(name))
        asset.set_editor_property("description", unreal.Text("{} aus der Hexenküche. {} Dauer: {}.".format(TIER_TEXT[tier], effect, duration)))
        asset.set_editor_property("region", unreal.Text("Aschenmark"))
        asset.set_editor_property("color", color(hex_code))
        asset.set_editor_property("category", C.POTION)
        asset.set_editor_property("rarity", [R.COMMON, R.MAGIC, R.RARE][tier])
        asset.set_editor_property("way", way)
        asset.set_editor_property("has_element", False)
        asset.set_editor_property("stack_size", 5)
        asset.set_editor_property("use", U.DRINK)
        asset.set_editor_property("mesh", bottle)
        asset.set_editor_property("inner_mesh", inner)
        asset.set_editor_property("inner_material", liquid(ident, hex_code, way))
        assets.save_loaded_asset(asset, False)
        made += 1
    log("{} Traenke angelegt oder aktualisiert".format(made))


build()
