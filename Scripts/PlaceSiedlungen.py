# The people of the Aschenmark in L_Vael (Docs/Plan_NPCs_Siedlungen.md), from Scripts/Data/Welt/AM_Siedlungen.json.
#
# - People already standing in L_Vael ("bestehend" and the story people Edda, Albrun, Maren) get their identity
#   (name, origin, role, settlement, age, build, attitude to the Prophet) and the gossip of their settlement.
# - Everybody else is spawned on their work place, robed in the colour of their faction.
# - New settlements get their buildings (huts, tents, workbench, pen, field, drying rack) and a fire.
# - Every spot is tested against the bounds of what stands in the level: where a house, a table or a barrel stands,
#   the person moves to the nearest free ground (the generator does not know the houses of the ported places).
# Everything spawned carries the tag VaelSiedlung and is removed again on the next run (the people that were there
# before are only updated). Run CreateSettlementGossip.py and ImportSiedlungsBauten.py first.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PlaceSiedlungen.py" -EnablePlugins=PythonScriptPlugin -unattended

import array
import json
import math
import os
import random

import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WELT = os.path.join(ROOT, "Scripts", "Data", "Welt")
LEVEL = "/Game/Vael/Maps/L_Vael"
GOSSIP = "/Game/Vael/Story/Gerede/DA_Gossip_Siedlung_"
BAUTEN = "/Game/Vael/Maps/Aschenmark/Bauten/"
TAG = "VaelSiedlung"
M = 100.0
SIZE_X, SIZE_Y = 2017, 2521

PROPHET = {"Anhaenger": unreal.VaelProphetAttitude.FOLLOWER, "Gegner": unreal.VaelProphetAttitude.ENEMY,
           "Zweifler": unreal.VaelProphetAttitude.DOUBTER, "Gleichgueltig": unreal.VaelProphetAttitude.INDIFFERENT,
           "Story": unreal.VaelProphetAttitude.STORY}
AGE = {"erwachsen": unreal.VaelNpcAge.ADULT, "kind": unreal.VaelNpcAge.CHILD, "jung": unreal.VaelNpcAge.YOUNG, "alt": unreal.VaelNpcAge.OLD}
BUILD = {"mittel": unreal.VaelNpcBuild.MEDIUM, "schmal": unreal.VaelNpcBuild.SLIM, "kraeftig": unreal.VaelNpcBuild.STRONG,
         "klein": unreal.VaelNpcBuild.SLIM}
# Robe colour per faction; every person gets a slightly different shade
ROBES = {"Ueberlebende": (0.3, 0.24, 0.18), "Gilde": (0.1, 0.13, 0.22), "Orden": (0.2, 0.09, 0.17), "Passwache": (0.15, 0.18, 0.12),
         "Freie": (0.36, 0.27, 0.15)}
# Building: (mesh, height in m or None for the mesh's own size, collision)
MESHES = {"Huette": ("SM_AM_Huette", None, True), "Zelt": ("SM_AM_Zelt", 2.3, True), "Werkbank": ("SM_AM_Werkbank", None, True),
          "Gehege": ("SM_AM_Gehege", None, True), "Acker": ("SM_AM_Acker", None, True), "Trockengestell": ("SM_AM_Trockengestell", None, True)}

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(1108)
counts = {}

heights = array.array("H")
with open(os.path.join(WELT, "AM_Hoehen.r16"), "rb") as f:
    heights.frombytes(f.read())


def log(message):
    unreal.log_warning("[PlaceSiedlungen] " + message)


def ground(x, y):
    x = min(max(x, 0.0), SIZE_X - 1.001)
    y = min(max(y, 0.0), SIZE_Y - 1.001)
    ix, iy = int(x), int(y)
    tx, ty = x - ix, y - iy

    def h(i, j):
        return (heights[j * SIZE_X + i] - 32768) / 128.0
    return (h(ix, iy) * (1 - tx) + h(ix + 1, iy) * tx) * (1 - ty) + (h(ix, iy + 1) * (1 - tx) + h(ix + 1, iy + 1) * tx) * ty


def identity(settlement, person):
    return unreal.VaelNpcIdentity(
        name=unreal.Text(person["name"]), culture=unreal.Name(person.get("kultur", "")), role=unreal.Name(person["rolle"]),
        faction=unreal.Name(settlement["fraktion"]), settlement=unreal.Name(settlement["id"]),
        settlement_name=unreal.Text(settlement["name"]), age=AGE[person["alter"]], build=BUILD[person["koerper"]],
        prophet=PROPHET[person["prophet"]])


# ---------------------------------------------------------------- free ground

class Ground:
    """Free ground by the bounds of what stands in the level. (Traces do not work here: in a commandlet the meshes have
    no physics, a trace only meets the landscape.) Obstacles: visible actors with collision, at least 0.5 m high and
    less than 12 m across (big floors and the landscape count as ground), sorted into 10 m cells."""

    CELL = 10.0

    def __init__(self, level_actors):
        self.taken = []
        self.cells = {}
        self.obstacles = 0
        self.blocked = 0
        for actor in level_actors:
            if isinstance(actor, (unreal.VaelNpc, unreal.Volume, unreal.Light)) or actor.get_class().get_name().startswith("Landscape"):
                continue
            if not actor.get_actor_enable_collision() or actor.is_hidden_ed():
                continue
            if isinstance(actor, unreal.StaticMeshActor) and not actor.static_mesh_component.get_editor_property("visible"):
                continue
            origin, extent = actor.get_actor_bounds(True)
            if extent.z * 2 < 50.0 or max(extent.x, extent.y) * 2 > 1200.0 or max(extent.x, extent.y) < 5.0:
                continue
            box = (origin.x / M - extent.x / M, origin.y / M - extent.y / M, origin.x / M + extent.x / M, origin.y / M + extent.y / M,
                   actor.get_actor_label())
            for cx in range(int(box[0] // self.CELL), int(box[2] // self.CELL) + 1):
                for cy in range(int(box[1] // self.CELL), int(box[3] // self.CELL) + 1):
                    self.cells.setdefault((cx, cy), []).append(box)
            self.obstacles += 1

    def standing(self, x, y, margin=0.45):
        """What stands on the spot, or None if the ground is free"""
        for box in self.cells.get((int(x // self.CELL), int(y // self.CELL)), []):
            if box[0] - margin <= x <= box[2] + margin and box[1] - margin <= y <= box[3] + margin:
                return box[4]
        return None

    def free_spot(self, x, y, room=1.0, max_r=7.0):
        """The spot itself if free, else the nearest free one on rings round it; None if there is none"""
        r = 0.0
        while r <= max_r:
            steps = 1 if r == 0 else max(6, int(2 * math.pi * r / 0.7))
            start = rng.uniform(0, 2 * math.pi)
            for k in range(steps):
                a = start + 2 * math.pi * k / steps
                px, py = x + math.cos(a) * r, y + math.sin(a) * r
                if any(math.hypot(px - tx, py - ty) < room for tx, ty in self.taken):
                    continue
                what = self.standing(px, py)
                if what is None:
                    self.taken.append((px, py))
                    if r > 0:
                        self.blocked += 1
                    return px, py
            r += 0.75
        return None


# ---------------------------------------------------------------- spawning

def tag(actor, folder, label):
    actor.set_folder_path(folder)
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(TAG)])
    counts[folder.split("/")[0]] = counts.get(folder.split("/")[0], 0) + 1


def place_building(art, spot, folder):
    if art == "Feuer":
        z = ground(spot["x"], spot["y"]) * M
        fire = actors.spawn_actor_from_class(unreal.VaelFireSpot, unreal.Vector(spot["x"] * M, spot["y"] * M, z + 10.0), unreal.Rotator(0, 0, 0))
        tag(fire, folder, "Feuer")
        light = actors.spawn_actor_from_class(unreal.VaelFireLight, unreal.Vector(spot["x"] * M, spot["y"] * M, z + 100.0), unreal.Rotator(0, 0, 0))
        tag(light, folder, "Feuerschein")
        return
    name, height, collision = MESHES[art]
    mesh = unreal.load_asset(BAUTEN + name)
    if mesh is None:
        log("mesh missing: " + name)
        return
    box = mesh.get_bounding_box()
    scale = height * M / max(box.max.z - box.min.z, 1.0) if height else 1.0
    # The lowest corner of the footprint decides the height, so nothing floats on a slope
    half_x, half_y = (box.max.x - box.min.x) * scale / 2 / M, (box.max.y - box.min.y) * scale / 2 / M
    z = min(ground(spot["x"] + dx, spot["y"] + dy) for dx in (-half_x, 0, half_x) for dy in (-half_y, 0, half_y)) * M - box.min.z * scale - 5.0
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(spot["x"] * M, spot["y"] * M, z), unreal.Rotator(0, 0, spot["yaw"]))
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    if not collision:
        actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    tag(actor, folder, art)


def person_height(person):
    return {"kind": 0.62, "jung": 0.94, "alt": 0.92}.get(person["alter"], 1.0)


def day_place(kind, x, y, yaw, person):
    """One place of the day of a person: the actor centre stands 92 cm (scaled) above the ground"""
    place = unreal.VaelNpcPlace()
    place.set_editor_property("kind", kind)
    place.set_editor_property("location", unreal.Vector(x * M, y * M, ground(x, y) * M + 92.0 * person_height(person)))
    place.set_editor_property("yaw", yaw)
    return place


def day_places(person, free, work):
    """Work (already found free), fire and sleep, or the stops of a carter; fire spots and stops get the same free-ground
    check as the work spots, the sleeping place not (a sleeping person is inside, hidden)"""
    places = []
    plaetze = person.get("plaetze", {})
    if work is not None and "arbeit" in plaetze:
        places.append(day_place(unreal.VaelNpcPlaceKind.WORK, work[0], work[1], work[2], person))
    for index, stop in enumerate(person.get("halte", [])):
        spot = work if index == 0 and work is not None else free.free_spot(stop["x"], stop["y"])
        if spot is not None:
            places.append(day_place(unreal.VaelNpcPlaceKind.STOP, spot[0], spot[1], stop["yaw"], person))
    if "feuer" in plaetze:
        fire = plaetze["feuer"]
        spot = free.free_spot(fire["x"], fire["y"])
        if spot is not None:
            places.append(day_place(unreal.VaelNpcPlaceKind.FIRE, spot[0], spot[1], fire["yaw"], person))
        else:
            log("no free ground at the fire for " + person["name"])
    if "schlaf" in plaetze:
        sleep = plaetze["schlaf"]
        places.append(day_place(unreal.VaelNpcPlaceKind.SLEEP, sleep["x"], sleep["y"], sleep["yaw"], person))
    return places


def place_person(settlement, person, spot, gossip, folder, places):
    shade = ROBES.get(settlement["fraktion"], (0.3, 0.25, 0.2))
    robe = unreal.LinearColor(*(min(1.0, c * rng.uniform(0.8, 1.25)) for c in shade), 1.0)
    z = ground(spot[0], spot[1]) * M + 92.0 * person_height(person)
    npc = actors.spawn_actor_from_class(unreal.VaelNpc, unreal.Vector(spot[0] * M, spot[1] * M, z), unreal.Rotator(0, 0, spot[2]))
    npc.set_editor_property("identity", identity(settlement, person))
    npc.set_editor_property("robe_color", robe)
    npc.set_editor_property("places", places)
    if gossip is not None:
        npc.set_editor_property("gossip", gossip)
    tag(npc, folder, person["name"])
    return npc


def main():
    data = json.load(open(os.path.join(WELT, "AM_Siedlungen.json"), encoding="utf-8"))
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL):
        log("could not load " + LEVEL)
        return
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

    level_actors = actors.get_all_level_actors()
    old = [a for a in level_actors if unreal.Name(TAG) in a.get_editor_property("tags")]
    for actor in old:
        actors.destroy_actor(actor)
    log("{} actors of the last run removed".format(len(old)))

    npcs = {}
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.VaelNpc):
            npcs.setdefault(actor.get_actor_label(), []).append(actor)
    updated = 0
    schedules = 0
    moved = []
    # Buildings first, so the people keep clear of them
    for settlement in data["siedlungen"]:
        folder = "Siedlungen/" + settlement["name"]
        for building in settlement.get("gebaeude", []):
            place_building(building["art"], building, folder + "/Bauten")
    free = Ground(actors.get_all_level_actors())
    # The people already there keep their spot; nobody else may stand on it
    for group in npcs.values():
        for actor in group:
            loc = actor.get_actor_location()
            free.taken.append((loc.x / M, loc.y / M))

    for settlement in data["siedlungen"]:
        folder = "Siedlungen/" + settlement["name"]
        gossip = unreal.load_asset(GOSSIP + settlement["gerede"])
        for person in settlement["leute"]:
            label = person.get("bestehend") or (person["name"] if person["prophet"] == "Story" else None)
            if label is not None:
                for actor in npcs.get(label, []):
                    actor.modify()
                    actor.set_editor_property("identity", identity(settlement, person))
                    if person.get("bestehend") and gossip is not None:
                        actor.set_editor_property("gossip", gossip)
                    updated += 1
                if label not in npcs:
                    log("not found in the level: " + label)
                continue
            start = person["plaetze"]["arbeit"] if "arbeit" in person.get("plaetze", {}) else person["halte"][0]
            spot = free.free_spot(start["x"], start["y"])
            if spot is None:
                log("no free ground for {} near {:.0f} {:.0f}".format(person["name"], start["x"], start["y"]))
                continue
            if math.hypot(spot[0] - start["x"], spot[1] - start["y"]) > 0.1:
                moved.append("{} {:.1f} m".format(person["name"], math.hypot(spot[0] - start["x"], spot[1] - start["y"])))
            work = (spot[0], spot[1], start["yaw"])
            places = day_places(person, free, work)
            place_person(settlement, person, work, gossip, folder + "/Leute", places)
            schedules += 1

    log("{} obstacles known; {} people moved off something: {}".format(free.obstacles, len(moved), ", ".join(moved)))
    log("{} people updated, {} with a day schedule, spawned: {}".format(updated, schedules, counts))
    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
    log("saved")


main()
