# Brings the existing data assets in line with the Lore-Bibel, tabs Herbarium and Rezeptbuch (Boris, 2026-10-14:
# "the Rezeptbuch is the guide, every name must be the same everywhere").
#
# - Materials get the names of the Herbarium and Rezeptbuch; asset names keep the old ones, only what players read changes.
# - The Heiltrank of the guild follows its recipe: heals 40 %, then regenerates for 5 minutes.
# - Wild plants give what the Herbarium lists under "Ernte".
# - Compendium texts use the new names.
#
# The Create...Assets.py scripts never overwrite values that differ from the class default, so this one-off script
# carries the change into assets that already exist. Safe to run again.
#
# Run with the editor closed, after CreateMaterialAssets.py:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/UnifyNamesWithRezeptbuch.py"

import unreal

eal = unreal.EditorAssetLibrary
MATERIALS = "/Game/Vael/Items/Materials/"
K = unreal.VaelIngredientKind
E = unreal.VaelElement
W = unreal.VaelWay


def change(path, values):
    asset = eal.load_asset(path)
    if asset is None:
        unreal.log_error("Vael: {} is missing".format(path))
        return

    for key, value in values.items():
        asset.set_editor_property(key, value)

    if eal.save_loaded_asset(asset, False):
        unreal.log("Vael: updated {}: {}".format(path, ", ".join(values.keys())))
    else:
        unreal.log_error("Vael: {} could not be saved".format(path))


def loot(*entries):
    """Loot entries from (material asset name, min count, max count)"""
    result = []
    for name, min_count, max_count in entries:
        material = eal.load_asset(MATERIALS + name)
        if material is None:
            unreal.log_error("Vael: {} is missing, run CreateMaterialAssets.py first".format(name))
            continue
        entry = unreal.VaelLootEntry()
        entry.set_editor_property("material", material)
        entry.set_editor_property("chance", 1.0)
        entry.set_editor_property("min_count", min_count)
        entry.set_editor_property("max_count", max_count)
        result.append(entry)
    return result


# Names of the Herbarium and Rezeptbuch
change(MATERIALS + "DA_Material_Rauchblase", {
    "display_name": unreal.Text("Rauchstaub"),
    "description": unreal.Text("Grauer Staub aus einer heilen Aschblase. Für Rauch und Tarnung."),
})
change(MATERIALS + "DA_Material_Pfeifensporen", {
    "display_name": unreal.Text("Pfeifenmorchel-Sporen"),
    "description": unreal.Text("Rostroter Staub aus den Röhren der Pfeifenmorchel. Katalysator: macht aus einem Trank Räucherwerk für eine Fläche."),
})
change(MATERIALS + "DA_Material_Staeublingspulver", {
    "display_name": unreal.Text("Aschenstäubling"),
    "description": unreal.Text("Graues Pulver des Aschenstäublings. Katalysator: macht aus einem Trank eine Wolke und löscht Brennen."),
})
change(MATERIALS + "DA_Material_Myzelprobe", {
    "display_name": unreal.Text("Adernflechte"),
    "description": unreal.Text("Feine weiße Fäden der Adernflechte, die sich violett färben, wenn Mark nah ist. Katalysator: Schatten-Rezepte schlagen seltener fehl."),
})
change(MATERIALS + "DA_Material_Ueberraschungssaat", {
    "display_name": unreal.Text("Keimsaat"),
    "description": unreal.Text("Samenkapseln vom Keimpelz. Niemand weiß, was daraus wird, bevor Albrun sie im Topf gezogen hat."),
})
change(MATERIALS + "DA_Material_Russmoos", {
    "display_name": unreal.Text("Keimpelzflocken"),
    "description": unreal.Text("Flocken vom Keimpelz. Tamer-Futter, mit dem Tiere rein wachsen; auch Polster und Atemfilter."),
})

# The Heiltrank as the Rezeptbuch brews it
change(MATERIALS + "DA_Material_Heiltrank", {
    "description": unreal.Text("Blatttau, Rußblütenblatt und Glimmstaub, gebraut und mit Gildensiegel verkorkt. Heilt vier Zehntel des Lebens, danach heilt er fünf Minuten lang langsam nach."),
    "heal_fraction": 0.4,
    "regen_fraction": 0.2,
    "regen_seconds": 300.0,
    "way": W.LIGHT,
})

# The Kelchkapsel of the Erntekelch, as entered in the Rezeptbuch
change(MATERIALS + "DA_Material_Kelchkapsel", {
    "kind": K.SEED,
    "has_element": True,
    "element": E.WATER,
    "way": W.NEUTRAL,
})

# Wild plants give what the Herbarium lists
change("/Game/Vael/Nature/DA_Harvest_Glutdistel", {"loot": loot(("DA_Material_Glutsamen", 1, 2), ("DA_Material_Distelflaum", 1, 1))})
change("/Game/Vael/Nature/DA_Harvest_Traenenkelch", {"loot": loot(("DA_Material_Kelchtau", 1, 2), ("DA_Material_Traenenglitzer", 1, 1))})
change("/Game/Vael/Nature/DA_Harvest_Russmoos", {"loot": loot(("DA_Material_Russmoos", 1, 3), ("DA_Material_Pelzwasser", 1, 1))})
change("/Game/Vael/Nature/DA_Harvest_Fleischkelch", {"loot": loot(("DA_Material_Kelchschleim", 1, 1), ("DA_Material_Verdauungssaeure", 1, 1))})

# Compendium texts with the new names
COMPENDIUM = "/Game/Vael/Compendium/"
change(COMPENDIUM + "DA_Compendium_Aschblase", {
    "weakness": unreal.Text("Platzt bei jedem Treffer, egal womit. Vorsichtig gepflückt bleibt eine Blase heil und gibt Rauchstaub."),
    "uses": unreal.Text("Luftzauber in ihrer Nähe kosten weniger. Der Ingenieur füllt Rauchstaub in Wurfgeschosse, um Gegner zu blenden."),
})
change(COMPENDIUM + "DA_Compendium_Russmoos", {
    "weakness": unreal.Text("Brennt nicht. Gepflückt gibt er Keimpelzflocken und Pelzwasser, und wenn er Kapseln trägt, auch Keimsaat."),
})
change(COMPENDIUM + "DA_Compendium_Traenenkelch", {
    "weakness": unreal.Text("Verträgt keine Verderbnis: Steigt sie, welkt der Kelch. Gepflückt gibt er Kelchtau und Tränenglitzer."),
    "uses": unreal.Text("Kelchtau ist die Basis starker Heiltränke, Tränenglitzer heilt die ganze Gruppe. In seiner Nähe kosten Wasserzauber weniger."),
})
change(COMPENDIUM + "DA_Compendium_Glutdistel", {
    "weakness": unreal.Text("Wasser löscht sie für eine Weile. Gepflückt gibt sie Glutsamen, die noch Stunden warm bleiben, und Distelflaum."),
})
change(COMPENDIUM + "DA_Compendium_Fleischkelch", {
    "weakness": unreal.Text("Feuer lässt sie zusammenschrumpfen. Geschnitten gibt sie Kelchschleim und Verdauungssäure."),
    "uses": unreal.Text("Kelchschleim ist eine Schattenzutat für Gift und Fluch, Verdauungssäure zerfrisst Rüstung. Wo Fleischkelche wachsen, ist die Region krank."),
})
change(COMPENDIUM + "DA_Compendium_Pfeifenmorchel", {
    "weakness": unreal.Text("Bei Windstille stumm und leicht zu übersehen. Gesammelt gibt sie Pfeifenmorchel-Sporen."),
})
change(COMPENDIUM + "DA_Compendium_Aschenstaeubling", {
    "weakness": unreal.Text("Einmal geplatzt, ist er leer, bis er nachwächst. Gesammelt gibt er sein Pulver, den Aschenstäubling."),
    "uses": unreal.Text("Der Aschenstäubling ist der Katalysator für den Löschtrank. Klug ist, wer brennend auf einen Stäubling zuläuft."),
})
change(COMPENDIUM + "DA_Compendium_Adernflechte", {
    "weakness": unreal.Text("Reißt leicht. Gesammelt gibt sie ein paar Fäden, die Adernflechte."),
})
