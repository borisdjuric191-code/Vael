# Fügt den Wurzelforst (Region II) in die offene Welt L_Vael ein, nördlich an den Aschenpass (Boris 13.11.2026: erst alle
# Karten grob zusammenfügen, die Details kommen später).
#
# - L_Wurzelforst_Blockout bleibt die Werkstatt: dort wird weiter gebaut, und dieses Skript wird danach einfach neu
#   ausgeführt. Der Blockout wird nur gelesen, nie gespeichert.
# - Neue Landschaft "Wurzelforst Gelaende" aus Scripts/Data/Welt/WF_Welt_Hoehen.r16 (Tools/Welt/wurzelforst_welt.py):
#   gedreht, so dass der Norden des Blockouts der Norden der Welt ist, an der Naht weich an den Rand der Aschenmark angeglichen.
#   Ihr Boden ist MI_WF_Zonenkarte_Welt: der Waldboden des Blockouts mit gedrehten Kartenachsen.
# - Alle Actors des Blockouts als Text kopiert (wie PortAschenmarkToWelt.py), gedreht und verschoben; wer im Übergangsband
#   steht, geht mit dem Boden mit. Nid-Adern und Pfuhlnatter-Nester rechnen ihre Weltpunkte mit um, Moorwasser bekommt
#   Kopien seiner Materialien mit gedrehten Achsen (*_Welt).
# - Nicht mitgenommen: Landschaft, Licht und Himmel (L_Vael hat eigene), Spielerstarts, Beschriftungen, das Meer des
#   Blockouts (das Meer der Aschenmark wird über beide Karten vergrößert).
# - Die Stimmung des Wurzelforsts (Post Process) gilt nur über dem Wurzelforst, dazu eine Region "Wurzelforst".
# - Naht: die unsichtbaren Randwände der Aschenmark im Norden und das verschlossene Passtor werden ausgeschaltet (nicht
#   gelöscht; Tag VaelWurzelforstOffen).
#
# Alles, was es anlegt, trägt den Tag "VaelWurzelforst" (Ordner "Wurzelforst (aus L_Wurzelforst_Blockout)/..."); ein neuer
# Lauf entfernt das erst. Vorher, wenn die Materialien neu sind: DressWurzelforst.py und BuildMoorWasser.py (Kartenachsen).
#
#   VAEL_PORT_DRY=1   zählt nur, was mitkäme, und ändert nichts
#
# Mit geschlossenem Editor:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PortWurzelforstToWelt.py" -AllowCommandletRendering -unattended -nosplash

import array
import json
import os

import unreal

OLD_LEVEL = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
NEW_LEVEL = "/Game/Vael/Maps/L_Vael"
DATA = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", "Welt")
GROUND = "/Game/Vael/Wurzelforst/Blockout/Materialien/M_WF_Zonenkarte"
GROUND_WORLD = "/Game/Vael/Wurzelforst/Blockout/Materialien/MI_WF_Zonenkarte_Welt"
TAG = "VaelWurzelforst"
OPEN_TAG = "VaelWurzelforstOffen"
FOLDER = "Wurzelforst (aus L_Wurzelforst_Blockout)"
BATCH = 200
SKIP_CLASSES = (unreal.Landscape, unreal.LandscapeProxy, unreal.DirectionalLight, unreal.SkyLight, unreal.SkyAtmosphere,
                unreal.ExponentialHeightFog, unreal.VolumetricCloud, unreal.PlayerStart, unreal.TextRenderActor)
SKIP_LABELS = ("Wasserspiegel_Ringsee_Westmeer",)

with open(os.path.join(DATA, "WF_Welt.json"), encoding="utf-8") as f:
    INFO = json.load(f)
NX, NY = INFO["punkte"]
STEP = INFO["schritt_cm"]
ORIGIN = INFO["ursprung_cm"]
AXES = INFO["material_achsen"]
NORTH_END, EAST_END = INFO["ende_nord_cm"], INFO["ende_ost_cm"]

editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary


def log(message):
    unreal.log_warning("[PortWurzelforst] " + message)


def read_raw(name):
    values = array.array("H")
    with open(os.path.join(DATA, name), "rb") as f:
        values.frombytes(f.read())
    return values


NEW_H = read_raw("WF_Welt_Hoehen.r16")
OLD_H = read_raw("WF_Welt_Alt.r16")


def lift(x, y):
    """cm, die der Boden an dieser Weltstelle gegenüber dem Blockout höher liegt (nur im Übergangsband nicht 0)"""
    fx = min(max((x - ORIGIN[0]) / STEP, 0.0), NX - 1.001)
    fy = min(max((y - ORIGIN[1]) / STEP, 0.0), NY - 1.001)
    ix, iy = int(fx), int(fy)
    tx, ty = fx - ix, fy - iy

    def d(i, j):
        k = j * NX + i
        return (NEW_H[k] - OLD_H[k]) / 128.0
    m = (d(ix, iy) * (1 - tx) + d(ix + 1, iy) * tx) * (1 - ty) + (d(ix, iy + 1) * (1 - tx) + d(ix + 1, iy + 1) * tx) * ty
    return m * 100.0


def to_world(v):
    """Punkt des Blockouts (cm) in die Welt: gedreht und verschoben, mit dem Boden angehoben"""
    x, y = 250000.0 - v.y, v.x + 175100.0
    return unreal.Vector(x, y, v.z + lift(x, y))


def world_material(source, folder_suffix="_Welt"):
    """Kopie eines Materials, das Karten nach Weltkoordinaten aufträgt, mit den gedrehten Kartenachsen"""
    names = [str(n) for n in library.get_vector_parameter_names(source)]
    if "KarteAchseU" not in names:
        return None
    path = source.get_path_name().split(".")[0]
    if isinstance(source, unreal.MaterialInstanceConstant):
        target = path + folder_suffix
        copy = unreal.load_asset(target) if assets.does_asset_exist(target) else assets.duplicate_asset(path, target)
    else:
        target = GROUND_WORLD if path == GROUND else path + folder_suffix
        if assets.does_asset_exist(target):
            copy = unreal.load_asset(target)
        else:
            folder, name = target.rsplit("/", 1)
            copy = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.MaterialInstanceConstant,
                                                                           unreal.MaterialInstanceConstantFactoryNew())
            copy.set_editor_property("parent", source)
    for key, value in AXES.items():
        library.set_material_instance_vector_parameter_value(copy, key, unreal.LinearColor(value[0], value[1], 0.0, 0.0))
    library.update_material_instance(copy)
    assets.save_loaded_asset(copy, False)
    return copy


moved_materials = {}


def follow_axes(component):
    """Wasser des Moors legt seine Maske nach Weltkoordinaten: Kopie mit gedrehten Achsen"""
    for index in range(component.get_num_materials()):
        material = component.get_material(index)
        if not isinstance(material, (unreal.MaterialInstanceConstant, unreal.Material)):
            continue
        key = material.get_path_name()
        if key not in moved_materials:
            moved_materials[key] = world_material(material)
        if moved_materials[key] is not None:
            component.set_material(index, moved_materials[key])


def wanted(actor):
    if isinstance(actor, SKIP_CLASSES):
        return False
    if actor.get_actor_label() in SKIP_LABELS or str(actor.get_folder_path()) == "Lighting":
        return False
    return actor.get_attach_parent_actor() is None


def place(actor):
    """Ein eingefügter Actor an seinen Platz in der Welt"""
    old = actor.get_actor_location()
    new = to_world(old)
    if isinstance(actor, unreal.VaelNidVeins):
        # Seine Punkte stehen in Weltkoordinaten, das Netz hängt ungedreht am Actor: nur verschieben, die Punkte umrechnen
        points = list(actor.get_editor_property("points"))
        for point in points:
            point.set_editor_property("location", to_world(point.get_editor_property("location")))
        actor.set_editor_property("points", points)
        actor.set_actor_location(new, False, True)
        return
    if isinstance(actor, unreal.VaelPfuhlnatterPopulation):
        actor.set_editor_property("nests", [to_world(n) for n in actor.get_editor_property("nests")])
    actor.set_actor_location(new, False, True)
    actor.add_actor_world_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=INFO["drehung_gier"]), False, True)
    if isinstance(actor, unreal.PostProcessVolume) and actor.get_editor_property("unbound"):
        # Die Stimmung des Waldes nur über dem Wald
        actor.set_editor_property("unbound", False)
        actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), True)
        actor.set_actor_location(unreal.Vector((ORIGIN[0] + NORTH_END) * 0.5, (ORIGIN[1] + EAST_END) * 0.5, 5000.0), False, True)
        actor.set_actor_scale3d(unreal.Vector((NORTH_END - ORIGIN[0]) / 200.0, (EAST_END - ORIGIN[1]) / 200.0, 150.0))
        actor.set_editor_property("blend_radius", 6000.0)
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        follow_axes(component)


def main():
    # ---------------------------------------------------------------- lesen
    if not levels.load_level(OLD_LEVEL):
        log("konnte " + OLD_LEVEL + " nicht öffnen")
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    old_actors = [a for a in editor.get_all_level_actors() if wanted(a)]
    counts = {}
    for a in old_actors:
        counts[a.get_class().get_name()] = counts.get(a.get_class().get_name(), 0) + 1
    log("{} Actors kommen mit: {}".format(len(old_actors), ", ".join("{} {}".format(k, v) for k, v in sorted(counts.items(), key=lambda kv: -kv[1]))))
    if os.environ.get("VAEL_PORT_DRY") == "1":
        return
    texts = []
    for start in range(0, len(old_actors), BATCH):
        texts.append(unreal.VaelWorldBuilder.copy_actors_as_text(world, old_actors[start:start + BATCH]))
    log("{} Pakete kopiert, {} Zeichen".format(len(texts), sum(len(t) for t in texts)))

    ground = unreal.load_asset(GROUND)
    ground_world = world_material(ground) if ground is not None else None
    if ground_world is None:
        log("M_WF_Zonenkarte hat noch keine Kartenachsen: zuerst DressWurzelforst.py ausführen")
        return

    # ---------------------------------------------------------------- L_Vael
    if not levels.load_level(NEW_LEVEL):
        log("konnte " + NEW_LEVEL + " nicht öffnen")
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    try:
        descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
        unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
    except Exception as error:  # noqa: BLE001
        log("konnte nicht alle Actors laden: {}".format(error))

    level_actors = editor.get_all_level_actors()
    # Die Landschaft legt in der offenen Welt eigene Teilstücke an (ohne Tag): alle Landschaftsteile nördlich der Naht gehören dazu
    old = [a for a in level_actors if unreal.Name(TAG) in a.get_editor_property("tags")
           or (isinstance(a, unreal.LandscapeProxy) and a.get_actor_location().x >= ORIGIN[0] - 10.0)]
    for actor in old:
        editor.destroy_actor(actor)
    log("{} Actors des letzten Laufs entfernt".format(len(old)))

    # Landschaft
    land = unreal.VaelWorldBuilder.create_landscape_from_raw(world, os.path.join(DATA, "WF_Welt_Hoehen.r16"), NX, NY,
                                                             unreal.Vector(ORIGIN[0], ORIGIN[1], 0.0), unreal.Vector(*INFO["skala"]),
                                                             ground_world, 2)
    if land is None:
        log("die Landschaft konnte nicht angelegt werden")
        return
    land.set_actor_label("Wurzelforst Gelaende")
    land.set_folder_path(FOLDER)
    land.set_editor_property("tags", [unreal.Name(TAG)])

    # Actors
    placed = 0
    for text in texts:
        for actor in unreal.VaelWorldBuilder.paste_actors_from_text(world, text):
            if actor.get_attach_parent_actor() is None:
                place(actor)
            folder = str(actor.get_folder_path())
            actor.set_folder_path("{}/{}".format(FOLDER, folder if folder and folder != "None" else "Sonstiges"))
            actor.set_editor_property("tags", list(actor.get_editor_property("tags")) + [unreal.Name(TAG)])
            placed += 1
    log("{} Actors eingefügt, {} Materialien mit gedrehten Achsen".format(placed, sum(1 for m in moved_materials.values() if m)))

    # Region und Meer
    region = editor.spawn_actor_from_class(unreal.VaelRegion, unreal.Vector((ORIGIN[0] + NORTH_END) * 0.5, (ORIGIN[1] + EAST_END) * 0.5, 0.0),
                                           unreal.Rotator(0.0, 0.0, 0.0))
    region.set_actor_label("Region Wurzelforst")
    region.set_folder_path(FOLDER + "/Spiel")
    region.set_editor_property("region_name", unreal.Text("Wurzelforst"))
    region.set_editor_property("is_spatially_loaded", False)
    region.set_editor_property("tags", [unreal.Name(TAG)])
    box = region.get_component_by_class(unreal.BoxComponent)
    box.set_box_extent(unreal.Vector((NORTH_END - ORIGIN[0]) * 0.5, (EAST_END - ORIGIN[1]) * 0.5, 40000.0))

    opened = 0
    for actor in level_actors:
        if unreal.Name(TAG) in actor.get_editor_property("tags"):
            continue
        label = actor.get_actor_label()
        folder = str(actor.get_folder_path())
        l = actor.get_actor_location()
        if label == "Meer" and isinstance(actor, unreal.StaticMeshActor):
            # Ein Meer für beide Karten
            south, west = -60000.0, ORIGIN[1] - 60000.0
            north, east = NORTH_END + 60000.0, EAST_END + 60000.0
            actor.set_actor_location(unreal.Vector((south + north) * 0.5, (west + east) * 0.5, l.z), False, True)
            actor.set_actor_scale3d(unreal.Vector((north - south) / 100.0, (east - west) / 100.0, 1.0))
        elif (folder.startswith("Grenzen/Rand") and l.x > 195000.0) or label.startswith("Tor zum Forstsaum"):
            if unreal.Name(OPEN_TAG) not in actor.get_editor_property("tags"):
                actor.set_actor_hidden_in_game(True)
                actor.set_actor_enable_collision(False)
                actor.set_editor_property("tags", list(actor.get_editor_property("tags")) + [unreal.Name(OPEN_TAG)])
            opened += 1
    log("{} Randwände und Tor an der Naht ausgeschaltet".format(opened))

    unreal.EditorLoadingAndSavingUtils.save_map(world, NEW_LEVEL)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("fertig")


main()
