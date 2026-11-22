# Kein Wetter im Eisenschlund (Boris 2026-11-22: Regen, Dürre usw. sind in den Schluchten und Höhlen nicht relevant): stellt die
# Region "Eisenschlund" in L_Vael auf dauerhaft klar, ohne den ganzen Eisenschlund neu zu bauen (BuildEisenschlund.py setzt dasselbe
# beim nächsten vollen Lauf). Mit geschlossenem Editor:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script=".../Scripts/EisenschlundOhneWetter.py" -unattended -nosplash
import unreal

WORLD = "/Game/Vael/Maps/L_Vael"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level(WORLD)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
try:
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
except Exception as error:  # noqa: BLE001
    unreal.log_warning("[OhneWetter] konnte nicht alle Actors laden: {}".format(error))
W = unreal.VaelWeather
n = 0
for actor in editor.get_all_level_actors():
    if isinstance(actor, unreal.VaelRegion) and actor.get_actor_label() == "Region Eisenschlund":
        actor.set_editor_property("start_weather", W.CLEAR)
        actor.set_editor_property("available_weathers", [W.CLEAR])
        actor.set_editor_property("changing_weather", False)
        actor.set_editor_property("weather_weights_pure", {W.CLEAR: 1.0})
        actor.set_editor_property("weather_weights_corrupted", {W.CLEAR: 1.0})
        n += 1
unreal.EditorLoadingAndSavingUtils.save_map(world, WORLD)
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log_warning("[OhneWetter] {} Region(en) Eisenschlund ohne Wetter".format(n))
