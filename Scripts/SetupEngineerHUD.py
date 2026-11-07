# The engineer's HUD bar (DA_HUDKlasse_Ingenieur): his real controls instead of the placeholder slots, the vessel as his pouch.
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/SetupEngineerHUD.py" -unattended -nosplash
import unreal

ASSET = "/Game/Vael/UI/HUD/Klassen/DA_HUDKlasse_Ingenieur"
data = unreal.load_asset(ASSET)
old = list(data.get_editor_property("abilities"))
icon = old[0].get_editor_property("icon") if old else None

B = unreal.VaelHUDButton
A = unreal.VaelHUDAbility
slots = [
    ("Schuss", B.R2, "LMT", A.ENG_FIRE),
    ("Vorrichtung", B.L2, "RMT", A.ENG_PLACE),
    ("Zünden", B.SQUARE, "X", A.ENG_IGNITE),
    ("Ladung", B.R1, "Rad", A.ENG_AMMO),
    ("Waffe", B.D_PAD, "R", A.ENG_WEAPON),
]
new = []
for index, (name, button, key, ability) in enumerate(slots):
    slot = unreal.VaelHUDAbilitySlot()
    slot.set_editor_property("name", name)
    slot.set_editor_property("button", button)
    slot.set_editor_property("keyboard_key", key)
    slot.set_editor_property("ability", ability)
    # keep the picture each slot had, else the class symbol
    previous = old[index] if index < len(old) else None
    slot.set_editor_property("icon", previous.get_editor_property("icon") if previous else icon)
    if previous is not None:
        slot.set_editor_property("painting", previous.get_editor_property("painting"))
    new.append(slot)
data.set_editor_property("abilities", new)
data.set_editor_property("resource", unreal.VaelHUDResource.AMMO)
data.set_editor_property("resource_name", "Ladung")
unreal.EditorAssetLibrary.save_loaded_asset(data, only_if_is_dirty=False)
unreal.log_warning("[EngineerHUD] {} slots".format(len(new)))
