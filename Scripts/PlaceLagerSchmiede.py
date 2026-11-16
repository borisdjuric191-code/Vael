# The forge of the camp (goal 16.11.: "Die Esse und das Lager sollen einen Platz haben, wo gebaut und geschmiedet werden kann"):
# beside the campfire of the Lager der Ueberlebenden in L_Vael the engineer gets a small open-air forge with the pieces of his
# workshop (Tools/Ingenieur/vael_werkstatt.py): forge, bellows, anvil with hammer and tongs, quenching barrel, crate, the
# workbench of the Glutkammer kit; and the two places to use (C++ VaelForge at the forge, VaelWorkbench at the bench).
# In the Glutkammer the forge of his workshop becomes usable too (VaelForge at SM_IW_Esse, tag VaelWerkstatt).
# Idempotent (tag VaelLagerSchmiede / VaelEsse).
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PlaceLagerSchmiede.py" -unattended -nosplash
import math

import unreal

WORLD_LEVEL = "/Game/Vael/Maps/L_Vael"
GLUTKAMMER = "/Game/Vael/Maps/Refugium/L_Glutkammer"
TAG = "VaelLagerSchmiede"
ESSE_TAG = "VaelEsse"
FOLDER = "/Game/Vael/Refugium/Glutkammer/Werkstatt"
CAMP_FOLDER_WORD = "Lager der Ueberlebenden"
# the forge stands this far from the fire, in a ring of pieces round the anvil (cm, own frame: +X away from the fire)
DISTANCE = 900.0
PIECES = [
    # mesh, x, y, yaw turn, label
    ("SM_IW_Esse", 130.0, -40.0, 180.0, "Esse"),
    ("SM_IW_Blasebalg", 150.0, 110.0, 160.0, "Blasebalg"),
    ("SM_IW_Amboss", -30.0, 0.0, 90.0, "Amboss"),
    ("SM_IW_Loeschfass", 20.0, -190.0, 30.0, "Loeschfass"),
    ("SM_IW_Munitionskiste", -120.0, -170.0, 70.0, "Munitionskiste"),
    ("SM_IW_Schrottkiste", 190.0, -180.0, 120.0, "Schrottkiste"),
    ("/Game/Vael/Refugium/Glutkammer/Probe/SM_GK_Werkbank", -60.0, 190.0, 0.0, "Werkbank"),
]

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def log(message):
    unreal.log_warning("[LagerSchmiede] " + message)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def ground(x, y, z_hint):
    hit = unreal.SystemLibrary.line_trace_single(world(), unreal.Vector(x, y, z_hint + 1500.0), unreal.Vector(x, y, z_hint - 3000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    if hit is not None and hit.to_tuple()[0]:
        return hit.to_tuple()[4].z
    return z_hint


def mesh_of(name):
    path = name if name.startswith("/Game/") else "{}/{}".format(FOLDER, name)
    return unreal.load_asset(path)


def place_camp():
    if not levels.load_level(WORLD_LEVEL):
        log("could not load " + WORLD_LEVEL)
        return
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
    everything = actors.get_all_level_actors()
    old = [a for a in everything if unreal.Name(TAG) in a.get_editor_property("tags")]
    for actor in old:
        actors.destroy_actor(actor)
    # The campfire of the Lager der Ueberlebenden (the ground area "Lagerfeuer"); the forge goes on the freest spot inside the
    # palisade: a grid round the fire, each point scored by its distance to the nearest thing standing there
    fires = [a for a in everything if a.get_actor_label() == "Lagerfeuer" and str(a.get_folder_path()) == "Kesselgrund (aus L_Aschenmark)/Lager"]
    if not fires:
        log("campfire not found")
        return
    at = fires[0].get_actor_location()
    skip = ("VaelGroundArea", "VaelSafeZone", "VaelFireLight", "PostProcessVolume", "VaelRegion", "Landscape", "WorldPartition", "Brush")
    obstacles = []
    for a in everything:
        if a in old or any(k in a.get_class().get_name() for k in skip):
            continue
        where = a.get_actor_location()
        if abs(where.x - at.x) < 1600.0 and abs(where.y - at.y) < 1600.0:
            origin, extent = a.get_actor_bounds(False)
            if extent.x < 1500.0 and extent.y < 1500.0:
                obstacles.append((unreal.Vector(origin.x, origin.y, 0.0), max(extent.x, extent.y) * 0.6))
    palisade = [o for o, r in obstacles]
    best = None
    for gx in range(-900, 901, 50):
        for gy in range(-900, 901, 50):
            spot = unreal.Vector(at.x + gx, at.y + gy, 0.0)
            from_fire = math.hypot(gx, gy)
            if from_fire < 480.0 or from_fire > 850.0:
                continue
            clear = min(((spot - o).length() - r for o, r in obstacles), default=1000.0)
            if best is None or clear > best[0]:
                best = (clear, spot)
    clear, spot = best
    spot = unreal.Vector(spot.x, spot.y, at.z)
    # the anvil faces the fire, the forge stands behind it
    frame_yaw = math.degrees(math.atan2(spot.y - at.y, spot.x - at.x))
    log("campfire at {}, forge spot {} ({:.0f} cm clear, yaw {:.0f}), {} obstacles".format(at, spot, clear, frame_yaw, len(obstacles)))

    c, s = math.cos(math.radians(frame_yaw)), math.sin(math.radians(frame_yaw))
    placed = {}
    for name, dx, dy, turn, label in PIECES:
        mesh = mesh_of(name)
        if mesh is None:
            log("missing mesh " + name)
            continue
        x, y = spot.x + dx * c - dy * s, spot.y + dx * s + dy * c
        z = ground(x, y, spot.z)
        # pieces face the anvil in the middle
        face = math.degrees(math.atan2(spot.y - y, spot.x - x)) + turn * 0.0
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z - 2.0), unreal.Rotator(0.0, 0.0, face if label != "Werkbank" else frame_yaw + 180.0))
        actor.static_mesh_component.set_static_mesh(mesh)
        actor.set_folder_path("Lager der Ueberlebenden/Lagerschmiede")
        actor.set_actor_label("Lagerschmiede " + label)
        actor.set_editor_property("tags", [unreal.Name(TAG)])
        placed[label] = actor
    # hammer and tongs on the anvil
    if "Amboss" in placed:
        anvil = placed["Amboss"].get_actor_location()
        for name, ox, oy, oz, turn in (("SM_IW_Hammer", 0.0, 0.0, 80.0, 70.0), ("SM_IW_Zange", -10.0, 15.0, 81.0, -50.0)):
            mesh = mesh_of(name)
            if mesh is not None:
                tool = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(anvil.x + ox, anvil.y + oy, anvil.z + oz), unreal.Rotator(0.0, 0.0, frame_yaw + turn))
                tool.static_mesh_component.set_static_mesh(mesh)
                tool.set_folder_path("Lager der Ueberlebenden/Lagerschmiede")
                tool.set_actor_label("Lagerschmiede " + name.replace("SM_IW_", ""))
                tool.set_editor_property("tags", [unreal.Name(TAG)])
    # the places to use: the forge at the forge (sparks fly off the anvil), the bench at the bench
    if "Esse" in placed:
        esse = placed["Esse"]
        forge = actors.spawn_actor_from_class(unreal.VaelForge, esse.get_actor_location(), esse.get_actor_rotation())
        forge.set_folder_path("Lager der Ueberlebenden/Lagerschmiede")
        forge.set_actor_label("Esse (Ingenieur)")
        forge.set_editor_property("tags", [unreal.Name(TAG)])
        if "Amboss" in placed:
            local = esse.get_actor_transform().inverse_transform_location(placed["Amboss"].get_actor_location())
            forge.set_editor_property("spark_offset", unreal.Vector(local.x, local.y, 85.0))
    if "Werkbank" in placed:
        bench = actors.spawn_actor_from_class(unreal.VaelWorkbench, placed["Werkbank"].get_actor_location(), placed["Werkbank"].get_actor_rotation())
        bench.set_folder_path("Lager der Ueberlebenden/Lagerschmiede")
        bench.set_actor_label("Werkbank (Ingenieur, Lager)")
        bench.set_editor_property("tags", [unreal.Name(TAG)])
    unreal.EditorLoadingAndSavingUtils.save_map(world(), WORLD_LEVEL)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
    log("{} old removed, {} pieces at the camp".format(len(old), len(placed)))


def place_glutkammer():
    """The forge of his workshop becomes usable: a VaelForge where SM_IW_Esse stands, sparks off the anvil"""
    if not levels.load_level(GLUTKAMMER):
        log("could not load " + GLUTKAMMER)
        return
    everything = actors.get_all_level_actors()
    for actor in [a for a in everything if unreal.Name(ESSE_TAG) in a.get_editor_property("tags")]:
        actors.destroy_actor(actor)
    esse = [a for a in everything if isinstance(a, unreal.StaticMeshActor) and a.static_mesh_component.static_mesh is not None
            and a.static_mesh_component.static_mesh.get_name() == "SM_IW_Esse"]
    anvil = [a for a in everything if isinstance(a, unreal.StaticMeshActor) and a.static_mesh_component.static_mesh is not None
             and a.static_mesh_component.static_mesh.get_name() == "SM_IW_Amboss"]
    if not esse:
        log("no SM_IW_Esse in the Glutkammer (run Scripts/BuildWerkstatt.py first)")
        return
    forge = actors.spawn_actor_from_class(unreal.VaelForge, esse[0].get_actor_location(), esse[0].get_actor_rotation())
    forge.set_folder_path("Stufe I/Werkstatt Ingenieur")
    forge.set_actor_label("Esse (Ingenieur)")
    forge.set_editor_property("tags", [unreal.Name(ESSE_TAG)])
    # the workshop has its own glow light for the coals; the forge's light stays soft beside it
    forge.get_component_by_class(unreal.PointLightComponent).set_intensity(25.0)
    if anvil:
        local = esse[0].get_actor_transform().inverse_transform_location(anvil[0].get_actor_location())
        forge.set_editor_property("spark_offset", unreal.Vector(local.x, local.y, 85.0))
    unreal.EditorLoadingAndSavingUtils.save_map(world(), GLUTKAMMER)
    log("forge in the Glutkammer at {}".format(esse[0].get_actor_location()))


place_glutkammer()
place_camp()
