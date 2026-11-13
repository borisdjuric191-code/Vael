# Setzt die Erze der Aschenmark in L_Vael nach Scripts/Data/Welt/AM_Erze.json (Tools/Welt/aschenmark_erze.py).
#
# - Rußquarz und Schlacke-Erz in Vorkommen wie geplant, die Niddrusen rund um die Mark-Quelle des Levels (sie wachsen
#   nur bei offener Quelle, 7 m).
# - Alles bekommt den Tag "VaelErze" im Ordner "Erze/<Zone>/<Art>"; ein neuer Lauf entfernt erst die eigenen Erze.
# - Die alten zufälligen Erze von AusbauAschenmark.py (Ordner "Ausbau/Natur/<Zone>/Russquarz|Markdruse|Schlacke")
#   werden entfernt; AusbauAschenmark.py setzt sie nicht mehr. Sonst wird nichts gelöscht.
#
# Mit geschlossenem Editor:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PlaceErze.py" -AllowCommandletRendering -unattended -nosplash

import array
import json
import math
import os
import random

import unreal

LEVEL = "/Game/Vael/Maps/L_Vael"
DATA = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", "Welt")
TAG = "VaelErze"
ALTE_ERZE = ("Russquarz", "Markdruse", "Schlacke")
SIZE_X, SIZE_Y = 2017, 2521
M = 100.0

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
rng = random.Random(1311)


def log(message):
    unreal.log_warning("[PlaceErze] " + message)


heights = array.array("H")
with open(os.path.join(DATA, "AM_Hoehen.r16"), "rb") as f:
    heights.frombytes(f.read())
with open(os.path.join(DATA, "AM_Erze.json"), encoding="utf-8") as f:
    PLAN = json.load(f)


def ground(x, y):
    x = min(max(x, 0.0), SIZE_X - 1.001)
    y = min(max(y, 0.0), SIZE_Y - 1.001)
    ix, iy = int(x), int(y)
    tx, ty = x - ix, y - iy

    def h(i, j):
        return (heights[j * SIZE_X + i] - 32768) / 128.0
    return (h(ix, iy) * (1 - tx) + h(ix + 1, iy) * tx) * (1 - ty) + (h(ix, iy + 1) * (1 - tx) + h(ix + 1, iy + 1) * tx) * ty


data_cache = {}


def data(art):
    if art not in data_cache:
        data_cache[art] = unreal.load_asset("/Game/Vael/Nature/DA_Harvest_" + art)
        if data_cache[art] is None:
            log("fehlt: DA_Harvest_" + art)
    return data_cache[art]


def setze(art, x, y, z, yaw, ordner, label):
    d = data(art)
    if d is None:
        return False
    actor = actors.spawn_actor_from_class(unreal.VaelHarvestable, unreal.Vector(x * M, y * M, z * M), unreal.Rotator(0, 0, yaw))
    actor.set_editor_property("data", d)
    actor.set_folder_path(ordner)
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(TAG), unreal.Name("VaelNatur")])
    return True


def main():
    if not levels.load_level(LEVEL):
        log("konnte " + LEVEL + " nicht öffnen")
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    try:
        descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
        unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
        log("{} Actor-Beschreibungen geladen".format(len(descs)))
    except Exception as error:  # noqa: BLE001
        log("konnte nicht alle Actors laden: {}".format(error))

    weg = []
    quellen = []
    for actor in actors.get_all_level_actors():
        tags = actor.get_editor_property("tags")
        ordner = str(actor.get_folder_path())
        if unreal.Name(TAG) in tags:
            weg.append(actor)
        elif unreal.Name("VaelAusbau") in tags and ordner.startswith("Ausbau/Natur/") and ordner.rsplit("/", 1)[-1] in ALTE_ERZE:
            weg.append(actor)
        elif isinstance(actor, unreal.VaelMarkSource):
            quellen.append(actor)
    for actor in weg:
        actors.destroy_actor(actor)
    log("{} alte Erze entfernt".format(len(weg)))

    gesetzt = {}
    for v in PLAN["vorkommen"]:
        for i, s in enumerate(v["stuecke"]):
            label = "{}_{}".format(v["art"], v["grund"])
            if setze(v["art"], s["x"], s["y"], s["z"], s["yaw"], "Erze/{}/{}".format(v["zone"], v["art"]), label):
                gesetzt[v["art"]] = gesetzt.get(v["art"], 0) + 1

    # Niddrusen um die Mark-Quelle (die in Kesselgrund, falls es mehrere gibt: die nächste zur Nid-Quelle des Plans)
    nid = PLAN["niddrusen"]
    if quellen:
        quelle = quellen[0]
        l = quelle.get_actor_location()
        qx, qy = l.x / M, l.y / M
        punkte = []
        versuche = 0
        while len(punkte) < nid["anzahl"] and versuche < 500:
            versuche += 1
            a, d = rng.uniform(0, 2 * math.pi), rng.uniform(nid["abstand_min"], nid["abstand_max"])
            x, y = qx + math.cos(a) * d, qy + math.sin(a) * d
            if all(math.hypot(x - px, y - py) > 1.4 for px, py in punkte):
                punkte.append((x, y))
        for x, y in punkte:
            if setze("Markdruse", x, y, ground(x, y), rng.uniform(0, 360), "Erze/Kesselgrund/Niddruse", "Niddruse_Quelle"):
                gesetzt["Markdruse"] = gesetzt.get("Markdruse", 0) + 1
        log("Mark-Quelle bei {:.0f} / {:.0f} m ({} im Level)".format(qx, qy, len(quellen)))
    else:
        log("keine Mark-Quelle im Level: keine Niddrusen gesetzt")

    log("gesetzt: " + ", ".join("{} {}".format(k, n) for k, n in sorted(gesetzt.items())))
    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("fertig")


main()
