"""Step 8: the Pfuhlnatter's poison as a source of the water kit, under /Game/Vael/Kreaturen/Pfuhlnatter/:

  MI_Water_Gift            instance of M_Water (as the mage's water) in poison green: deep bottle green, shallow toxic
                           green, fairly murky, a faint glow, thick and slow
  DA_ElementSource_Gift    the source (bPoison): its balls are the mage's Wassergeschoss in this water and leave poison
                           puddles (AVaelPoisonPuddle, drawn like the water puddles in these colours)

Run with the editor closed:
  UnrealEditor-Cmd.exe <Vael.uproject> -run=pythonscript -script="<this file>"
"""
import unreal

ZIEL = "/Game/Vael/Kreaturen/Pfuhlnatter"
MAGIER = "/Game/VFX/Water/Materials/MI_Water_Spell_Mage"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

FARBEN = {"DeepColor": (0.004, 0.035, 0.008), "ShallowColor": (0.12, 0.45, 0.05)}
WERTE = {"Murkiness": 0.6, "EmissiveStrength": 0.35, "WobbleStrength": 3.0, "WobbleSpeed": 0.5, "Refraction": 0.12,
         "FresnelPower": 4.0, "FoamAmount": 0.12, "Wetness": 1.0}


def log(text):
    unreal.log_warning("[Pfuhlnatter] " + text)


def instanz():
    pfad = ZIEL + "/MI_Water_Gift"
    mi = eal.load_asset(pfad) if eal.does_asset_exist(pfad) else tools.create_asset(
        "MI_Water_Gift", ZIEL, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    magier = eal.load_asset(MAGIER)
    mel.set_material_instance_parent(mi, magier.get_editor_property("parent"))
    for name, (r, g, b) in FARBEN.items():
        mel.set_material_instance_vector_parameter_value(mi, name, unreal.LinearColor(r, g, b, 1.0))
    for name, wert in WERTE.items():
        mel.set_material_instance_scalar_parameter_value(mi, name, wert)
    eal.save_asset(pfad, only_if_is_dirty=False)
    log("MI_Water_Gift auf " + magier.get_editor_property("parent").get_name())
    return mi


def quelle(mi):
    pfad = ZIEL + "/DA_ElementSource_Gift"
    da = eal.load_asset(pfad) if eal.does_asset_exist(pfad) else tools.create_asset(
        "DA_ElementSource_Gift", ZIEL, unreal.VaelElementSource, unreal.DataAssetFactory())
    da.set_editor_property("source_name", unreal.Text("Pfuhlnatter-Gift"))
    da.set_editor_property("element", unreal.VaelElement.WATER)
    da.set_editor_property("poison", True)
    da.set_editor_property("material", mi)
    da.set_editor_property("glow_color", unreal.LinearColor(0.12, 0.45, 0.06, 1.0))
    da.set_editor_property("particle_color", unreal.LinearColor(0.22, 0.55, 0.1, 0.85))
    da.set_editor_property("viscosity", 0.5)
    eal.save_asset(pfad, only_if_is_dirty=False)
    log("DA_ElementSource_Gift angelegt")


quelle(instanz())
