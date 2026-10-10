"""
Vael – Kreaturenfabrik: baut die Testmap L_Kreaturentest für die Abnahme aus (Station 8).
Spielerstart, Rampe mit Plateau (Laufen am Hang), Unebenheiten, Spawner mit zwei Spannhornkäfern auf dem Plateau.
Läuft nur einmal: gibt es den Ordner "Abnahme" schon, ändert es nichts.

  UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="<Repo>/Tools/Kreaturenfabrik/unreal/vael_testmap_ausbauen.py"
"""
import math
import unreal

MAP = "/Game/Vael/Maps/Test/L_Kreaturentest"
FOLDER = "Abnahme"

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")


def block(label, location, size, rotation=unreal.Rotator(0, 0, 0)):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
    actor.static_mesh_component.set_static_mesh(cube)
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    actor.set_folder_path(FOLDER)
    actor.set_actor_label(label)
    return actor


def ausbauen():
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    level_actors = actors.get_all_level_actors()
    if any(str(a.get_folder_path()) == FOLDER for a in level_actors):
        unreal.log_warning("[Vael] Testmap ist schon ausgebaut")
        return

    # The display beetle from the first import moves aside
    for a in level_actors:
        if a.get_actor_label() == "Spannhornkaefer":
            a.set_actor_location(unreal.Vector(-400, 700, 0), False, False)
        if a.get_actor_label() == "Referenz 1 m":
            a.set_actor_location(unreal.Vector(-400, 900, 50), False, False)

    start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-450, 0, 100), unreal.Rotator(0, 0, 0))
    start.set_folder_path(FOLDER)
    start.set_actor_label("Spielerstart")

    # Plateau 1.2 m high, reached by a ramp of about 17 degrees
    height, ramp_length = 120.0, 400.0
    block("Plateau", unreal.Vector(1100, 0, height / 2), (800, 900, height))
    slope = math.degrees(math.atan2(height, ramp_length))
    run = math.hypot(height, ramp_length)
    block("Rampe", unreal.Vector(700 - ramp_length / 2, 0, height / 2 - 12), (run, 900, 24), unreal.Rotator(0, slope, 0))

    # Uneven ground in front of the ramp
    for index, (x, y, yaw, h) in enumerate([(-100, -250, 20, 18), (60, 180, 50, 25), (180, -60, 10, 14), (-250, 120, 70, 22)]):
        block("Buckel {}".format(index + 1), unreal.Vector(x, y, h / 2 - 4), (90, 120, h), unreal.Rotator(0, 0, yaw))

    spawner = actors.spawn_actor_from_class(unreal.VaelCreatureSpawner, unreal.Vector(1000, 0, height + 100), unreal.Rotator(0, 180, 0))
    spawner.set_editor_property("creature_kind", unreal.VaelHornBeetleData.static_class())
    spawner.set_editor_property("count", 2)
    spawner.set_editor_property("spread", 300.0)
    spawner.set_folder_path(FOLDER)
    spawner.set_actor_label("Spannhornkaefer (2)")

    unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    unreal.log_warning("[Vael] Testmap ausgebaut")


ausbauen()
