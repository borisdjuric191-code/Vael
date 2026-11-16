# The engineer's corner of the first camp (goal 16.11., Boris: "die Schmiedesachen im ersten Lager, eine Inventarbox daneben,
# schön Platz, nicht den Ausgang versperren, die Leute neu auslegen"). The same layout in both maps that hold the Lager der
# Ueberlebenden: L_Aschenmark (the map the game starts in) and L_Vael (the open world, the camp in Kesselgrund). Everything is
# placed relative to the campfire, so both look the same.
#
# The camp is round (palisade ~8.7 m round the fire), its gate is the open arc in the north-west. The engineer gets the east side
# along the palisade: forge, bellows, quenching barrel, anvil with hammer and tongs, the store chest (C++ VaelStorageChest), the
# workbench (C++ VaelWorkbench) and two crates; the forge is usable (C++ VaelForge). What stood there moves: the horse trough and
# its hitching post to the free north side, the bucket and the barrel on its stand of tent 1 next to that tent. The barrel with the
# lantern and the crates at the gate move to the palisade beside the southern gate post, so the way in stays wide.
# In the Glutkammer the forge of his workshop is usable (VaelForge at SM_IW_Esse).
# Idempotent: pieces carry the tag VaelLagerSchmiede / VaelEsse, moved props go to fixed places.
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PlaceLagerSchmiede.py" -unattended -nosplash
import math

import unreal

LEVELS = [
    # level, folder of the camp (where the ground area "Lagerfeuer" lies), world partition
    ("/Game/Vael/Maps/L_Aschenmark", "Lager", False),
    ("/Game/Vael/Maps/L_Vael", "Kesselgrund (aus L_Aschenmark)/Lager", True),
]
GLUTKAMMER = "/Game/Vael/Maps/Refugium/L_Glutkammer"
TAG = "VaelLagerSchmiede"
ESSE_TAG = "VaelEsse"
WORKSHOP = "/Game/Vael/Refugium/Glutkammer/Werkstatt"
CHEST_MESH = "/Game/Megascans/3D_Assets/OldWoodenChest/SM_OldWoodenChest"

# The engineer's corner, cm from the fire (+X east, +Y north); every piece faces the fire unless a yaw is given
CORNER = [
    ("SM_IW_Esse", 720.0, 150.0, None, "Esse"),
    ("SM_IW_Blasebalg", 680.0, 310.0, None, "Blasebalg"),
    ("SM_IW_Loeschfass", 600.0, 255.0, None, "Loeschfass"),
    ("SM_IW_Amboss", 540.0, 100.0, None, "Amboss"),
    ("SM_IW_Munitionskiste", 570.0, -45.0, None, "Munitionskiste"),
    ("/Game/Vael/Refugium/Glutkammer/Probe/SM_GK_Werkbank", 700.0, -150.0, None, "Werkbank"),
    ("SM_IW_Schrottkiste", 520.0, -190.0, None, "Schrottkiste"),
]
CHEST = (750.0, 10.0)
# Props that move: labels (the first is the anchor, the others keep their offset to it), folder ending, new place of the anchor
MOVES = [
    (("Trog",), "Traenke", (-30.0, 690.0), 0.0),
    (("Anbindebalken",), "Traenke", (-30.0, 560.0), 0.0),
    (("Eimer flach",), "Zelt 1", (330.0, 610.0), None),
    (("Fass auf Bock",), "Zelt 1", (470.0, 570.0), None),
    # the barrel at the gate with its lantern and its light, and the crates: to the palisade beside the southern gate post
    (("Fass am Weg", "Laterne auf dem Fass", "Laternenschein Tor"), "", (-730.0, 205.0), None),
    (("Kiste am Weg", "Kiste am Weg oben", "Kiste 2"), "", (-705.0, 45.0), None),
    # Edda stood on the way from the gate to the fire: she waits behind her table now, looking at the fire
    (("Edda Krell",), "Lager", (-495.0, 95.0), -10.0),
]

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def log(message):
    unreal.log_warning("[LagerSchmiede] " + message)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def ground(x, y, z_hint, ignore):
    hit = unreal.SystemLibrary.line_trace_single(world(), unreal.Vector(x, y, z_hint + 400.0), unreal.Vector(x, y, z_hint - 800.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore, unreal.DrawDebugTrace.NONE, True)
    if hit is not None and hit.to_tuple()[0]:
        return hit.to_tuple()[4].z
    return z_hint


def facing(x, y):
    """Yaw that turns +X of a piece at (x, y) round the fire towards the fire"""
    return math.degrees(math.atan2(-y, -x))


def tag(actor, folder, label):
    actor.set_folder_path(folder)
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(TAG)])


def place_camp(level, camp_folder, partitioned):
    if not levels.load_level(level):
        log("could not load " + level)
        return
    if partitioned:
        descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
        unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
    everything = actors.get_all_level_actors()
    old = [a for a in everything if unreal.Name(TAG) in a.get_editor_property("tags")]
    for actor in old:
        actors.destroy_actor(actor)
    everything = [a for a in everything if a not in old]
    fires = [a for a in everything if a.get_actor_label() == "Lagerfeuer" and str(a.get_folder_path()) == camp_folder]
    if not fires:
        log("{}: campfire not found".format(level))
        return
    fire = fires[0].get_actor_location()
    folder = camp_folder + "/Kulisse/Ingenieur"
    near = [a for a in everything if (a.get_actor_location() - fire).length() < 1400.0]

    # 1. Make room: move what stood where the corner goes, and clear the gate
    moved = 0
    for labels, folder_end, target, yaw in MOVES:
        group = [a for a in near if a.get_actor_label() in labels and str(a.get_folder_path()).endswith(folder_end)]
        if not group:
            log("{}: nothing to move for {}".format(level, labels[0]))
            continue
        anchor_actor = next((a for a in group if a.get_actor_label() == labels[0]), group[0])
        anchor = anchor_actor.get_actor_location()
        floor = ground(fire.x + target[0], fire.y + target[1], fire.z, group)
        old_floor = ground(anchor.x, anchor.y, fire.z, group)
        delta = unreal.Vector(fire.x + target[0] - anchor.x, fire.y + target[1] - anchor.y, floor - old_floor)
        for a in group:
            a.modify()
            a.set_actor_location(a.get_actor_location() + delta, False, False)
            if yaw is not None:
                a.set_actor_rotation(unreal.Rotator(0.0, 0.0, yaw), False)
            moved += 1

    # 2. The corner
    placed = {}
    for name, x, y, yaw, label in CORNER:
        mesh = unreal.load_asset(name if name.startswith("/Game/") else "{}/{}".format(WORKSHOP, name))
        if mesh is None:
            log("missing mesh " + name)
            continue
        z = ground(fire.x + x, fire.y + y, fire.z, [])
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(fire.x + x, fire.y + y, z - 2.0),
                                              unreal.Rotator(0.0, 0.0, facing(x, y) if yaw is None else yaw))
        actor.static_mesh_component.set_static_mesh(mesh)
        tag(actor, folder, "Ingenieur " + label)
        placed[label] = actor
    if "Amboss" in placed:
        anvil = placed["Amboss"].get_actor_location()
        for name, ox, oy, oz, turn in (("SM_IW_Hammer", 0.0, 0.0, 80.0, 70.0), ("SM_IW_Zange", -10.0, 15.0, 81.0, -50.0)):
            mesh = unreal.load_asset("{}/{}".format(WORKSHOP, name))
            if mesh is not None:
                tool = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(anvil.x + ox, anvil.y + oy, anvil.z + oz),
                                                     unreal.Rotator(0.0, 0.0, facing(540.0, 100.0) + turn))
                tool.static_mesh_component.set_static_mesh(mesh)
                tag(tool, folder, "Ingenieur " + name.replace("SM_IW_", ""))
    if "Esse" in placed:
        esse = placed["Esse"]
        forge = actors.spawn_actor_from_class(unreal.VaelForge, esse.get_actor_location(), esse.get_actor_rotation())
        tag(forge, folder, "Esse (Ingenieur)")
        if "Amboss" in placed:
            local = esse.get_actor_transform().inverse_transform_location(placed["Amboss"].get_actor_location())
            forge.set_editor_property("spark_offset", unreal.Vector(local.x, local.y, 85.0))
    if "Werkbank" in placed:
        bench = actors.spawn_actor_from_class(unreal.VaelWorkbench, placed["Werkbank"].get_actor_location(), placed["Werkbank"].get_actor_rotation())
        tag(bench, folder, "Werkbank (Ingenieur, Lager)")
    x, y = CHEST
    z = ground(fire.x + x, fire.y + y, fire.z, [])
    chest = actors.spawn_actor_from_class(unreal.VaelStorageChest, unreal.Vector(fire.x + x, fire.y + y, z - 1.0), unreal.Rotator(0.0, 0.0, facing(x, y) + 90.0))
    chest.get_component_by_class(unreal.StaticMeshComponent).set_static_mesh(unreal.load_asset(CHEST_MESH))
    tag(chest, folder, "Vorratskiste (Lager)")

    unreal.EditorLoadingAndSavingUtils.save_map(world(), level)
    if partitioned:
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
    log("{}: {} old removed, {} props moved, {} pieces, chest at the fire + {}".format(level, len(old), moved, len(placed), CHEST))


def place_glutkammer():
    """The forge of his workshop becomes usable: a VaelForge where SM_IW_Esse stands, sparks off the anvil"""
    if not levels.load_level(GLUTKAMMER):
        log("could not load " + GLUTKAMMER)
        return
    everything = actors.get_all_level_actors()
    for actor in [a for a in everything if unreal.Name(ESSE_TAG) in a.get_editor_property("tags")]:
        actors.destroy_actor(actor)
    by_mesh = lambda n: [a for a in everything if isinstance(a, unreal.StaticMeshActor) and a.static_mesh_component.static_mesh is not None  # noqa: E731
                         and a.static_mesh_component.static_mesh.get_name() == n]
    esse, anvil = by_mesh("SM_IW_Esse"), by_mesh("SM_IW_Amboss")
    if not esse:
        log("no SM_IW_Esse in the Glutkammer (run Scripts/BuildWerkstatt.py first)")
        return
    forge = actors.spawn_actor_from_class(unreal.VaelForge, esse[0].get_actor_location(), esse[0].get_actor_rotation())
    forge.set_folder_path("Stufe I/Werkstatt Ingenieur")
    forge.set_actor_label("Esse (Ingenieur)")
    forge.set_editor_property("tags", [unreal.Name(ESSE_TAG)])
    forge.get_component_by_class(unreal.PointLightComponent).set_intensity(25.0)
    if anvil:
        local = esse[0].get_actor_transform().inverse_transform_location(anvil[0].get_actor_location())
        forge.set_editor_property("spark_offset", unreal.Vector(local.x, local.y, 85.0))
    unreal.EditorLoadingAndSavingUtils.save_map(world(), GLUTKAMMER)
    log("forge in the Glutkammer at {}".format(esse[0].get_actor_location()))


place_glutkammer()
for level_path, camp, wp in LEVELS:
    place_camp(level_path, camp, wp)
