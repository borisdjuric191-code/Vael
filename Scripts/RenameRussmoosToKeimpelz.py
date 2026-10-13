# Renames the Russmoos to Keimpelz in the existing data assets (Boris, 2026-10-13) and gives it its new behaviour:
# it sprouts with every rain, and about half of them carry seeds (model SM_KeimpelzSamen, loot Ueberraschungssaat).
#
# The Create...Assets.py scripts never overwrite values that differ from the class default, so this one-off script
# carries the change into assets that already exist. Asset names and ids keep "Russmoos", only what players read changes.
# Safe to run again.
#
# Run with the editor closed, after CreateMaterialAssets.py and after importing Keimpelz and KeimpelzSamen:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="Scripts/RenameRussmoosToKeimpelz.py"

import unreal

eal = unreal.EditorAssetLibrary


def color(hex_code):
    """Linear color from an sRGB hex code like 'ff7a2e'"""
    def to_linear(channel):
        value = int(channel, 16) / 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4

    return unreal.LinearColor(r=to_linear(hex_code[0:2]), g=to_linear(hex_code[2:4]), b=to_linear(hex_code[4:6]), a=1.0)


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


def seed_loot():
    material = eal.load_asset("/Game/Vael/Items/Materials/DA_Material_Ueberraschungssaat")
    if material is None:
        unreal.log_error("Vael: DA_Material_Ueberraschungssaat is missing, run CreateMaterialAssets.py first")
        return []

    entry = unreal.VaelLootEntry()
    entry.set_editor_property("material", material)
    entry.set_editor_property("chance", 1.0)
    entry.set_editor_property("min_count", 1)
    entry.set_editor_property("max_count", 2)
    return [entry]


harvest = {
    "display_name": unreal.Text("Keimpelz"),
    "trait": unreal.VaelHarvestTrait.RAIN_SPROUT,
    "trait_amount": 0.5,
    "trait_duration": 120.0,
    "trait_color": color("2f4a26"),
    "seed_loot": seed_loot(),
}

for key, path in (("mesh", "/Game/Vael/Requisiten/Aschenmark/Keimpelz/SM_Keimpelz"), ("seed_mesh", "/Game/Vael/Requisiten/Aschenmark/KeimpelzSamen/SM_KeimpelzSamen")):
    model = eal.load_asset(path)
    if model is None:
        unreal.log_error("Vael: {} is missing, import it first".format(path))
    else:
        harvest[key] = model

change("/Game/Vael/Nature/DA_Harvest_Russmoos", harvest)

change("/Game/Vael/Items/Materials/DA_Material_Russmoos", {
    "display_name": unreal.Text("Keimpelz"),
    "description": unreal.Text("Grüner Moospelz, der mit dem Regen aus der Asche kommt. Futter, mit dem Tiere rein wachsen."),
})

change("/Game/Vael/Compendium/DA_Compendium_Russmoos", {
    "display_name": unreal.Text("Keimpelz"),
    "glimpse": unreal.Text("Ein dunkelgrüner Pelz, der mit dem Regen aus der Asche kommt und bald danach wieder vergeht. Früher hieß er Rußmoos."),
    "behaviour": unreal.Text("Frisst Asche und Ruß und wächst darauf. Er sprießt mit jedem Regen; etwa jeder zweite trägt dann dünne Stiele mit blassgrün glimmenden Samenkapseln."),
    "weakness": unreal.Text("Brennt nicht. Gepflückt gibt er eine Handvoll Pelz, und wenn er Kapseln trägt, auch die Überraschungssaat."),
    "uses": unreal.Text("Tiere, die damit gefüttert werden, wachsen rein, auch wenn das Land ringsum verdorben ist. Aus seiner Saat zieht Albrun im Topf die Pflanzen, mit denen die Aschenmark wieder grün wird."),
    "marked_form": unreal.Text("Gezeichneter Keimpelz wird nie grün, auch nicht im Regen. Tiere meiden ihn, und das zu Recht."),
    "albrun_note": unreal.Text("Das bescheidenste Gewächs der Aschenmark und das fleißigste. Es räumt hinter allen anderen auf und legt dann noch die Saat für die nächsten. Ich mag es sehr."),
})
