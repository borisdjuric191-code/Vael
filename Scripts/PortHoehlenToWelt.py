# Brings the joined caves of the Eisenschlund (L_Eisenschlund) into the open world L_Vael, so the whole of Vael is one map
# (Boris 2026-11-23: "Alle Teile sollen eins ergeben", the circle of the regions closed, later the Herz below).
#
# - L_Eisenschlund stays the workshop: BuildHoehle.py builds there, this script copies it in again (like PortWurzelforstToWelt.py).
# - The caves lie beside the land, south of it, at 100 m height, not under it: World Partition streams by X/Y only, so caves under
#   the Eisenschlund would load all their 60 000 pieces while one walks the surface above them.
# - The mouths in the world and the exits in the caves become ways inside the level (AVaelCaveEntrance::bInLevel): no loading.
# - Not copied: sun, sky, fog and player start of the workshop (L_Vael has its own). The cave light: the post process only over
#   the caves, a dark roof high above them keeps the sun and the sky out (above the camera, so never seen).
# - The indoor marker becomes a box (bWholeLevel off) with the cave map in world coordinates (DA_Karte_Eisenschlund_Welt);
#   the cave regions and the marker are always loaded, the dwellers come when someone is near (AVaelIndoorLevel).
#
# Everything it makes carries the tag "VaelHoehlenVerbund" (folder "Eisenschlund/Hoehlen (aus L_Eisenschlund)/..."); a new run
# removes that first. With the editor closed, after BuildHoehle.py (VAEL_HOEHLE=Eisenschlund):
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PortHoehlenToWelt.py" -AllowCommandletRendering -unattended -nosplash
import json
import os

import unreal

OLD_LEVEL = "/Game/Vael/Maps/Hoehlen/L_Eisenschlund"
NEW_LEVEL = "/Game/Vael/Maps/L_Vael"
TAG = "VaelHoehlenVerbund"
FOLDER = "Eisenschlund/Hoehlen (aus L_Eisenschlund)"
BATCH = 300
GAP = 150000.0          # cm between the land and the caves
HEIGHT = 10000.0        # cm: the cave floor (0 in the workshop) lies at this height
ROOF = 4500.0           # cm above the floor
SKIP_CLASSES = (unreal.Landscape, unreal.LandscapeProxy, unreal.DirectionalLight, unreal.SkyLight, unreal.SkyAtmosphere,
                unreal.ExponentialHeightFog, unreal.VolumetricCloud, unreal.PlayerStart, unreal.TextRenderActor)
DATA = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", "Hoehlen", "Eisenschlund.json")

editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assets = unreal.EditorAssetLibrary


def log(message):
    unreal.log_warning("[HoehlenWelt] " + str(message))


def wanted(actor):
    return not isinstance(actor, SKIP_CLASSES) and actor.get_attach_parent_actor() is None


def load_all():
    try:
        descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
        unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
    except Exception as error:  # noqa: BLE001
        log("konnte nicht alle Actors laden: {}".format(error))


def ground_at(world, x, y, ignore=()):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 60000.0), unreal.Vector(x, y, -20000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, list(ignore), unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit is not None else None


def world_map(offset):
    """DA_Karte_Eisenschlund moved into world coordinates (the minimap shows it inside the caves)"""
    src = "/Game/Vael/UI/Karten/DA_Karte_Eisenschlund"
    dst = src + "_Welt"
    if not assets.does_asset_exist(src):
        log("DA_Karte_Eisenschlund fehlt")
        return None
    source = unreal.load_asset(src)
    copy = unreal.load_asset(dst) if assets.does_asset_exist(dst) else assets.duplicate_asset(src, dst)
    shift = lambda v: unreal.Vector2D(v.x + offset.x, v.y + offset.y)
    # always from the workshop map, so a new run doesn't shift twice
    for key in ("Title", "Picture", "WorldSize", "bFogOfWar"):
        copy.set_editor_property(key, source.get_editor_property(key))
    for key in ("world_min", "focus_min", "focus_max"):
        copy.set_editor_property(key, shift(source.get_editor_property(key)))
    for key in ("labels", "camps"):
        entries = list(source.get_editor_property(key))
        for entry in entries:
            entry.set_editor_property("location", shift(entry.get_editor_property("location")))
        copy.set_editor_property(key, entries)
    assets.save_loaded_asset(copy, False)
    return copy


def main():
    with open(DATA, encoding="utf-8") as f:
        D = json.load(f)
    grid = D["grundriss"]
    rows, cols = len(grid), max(len(r) for r in grid)
    S = 400.0
    size_x, size_y = rows * S, cols * S

    # ---------------------------------------------------------------- read the workshop
    if not levels.load_level(OLD_LEVEL):
        log("konnte " + OLD_LEVEL + " nicht oeffnen")
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    old_actors = [a for a in editor.get_all_level_actors() if wanted(a)]
    log("{} Actors kommen mit".format(len(old_actors)))
    texts = [unreal.VaelWorldBuilder.copy_actors_as_text(world, old_actors[i:i + BATCH]) for i in range(0, len(old_actors), BATCH)]

    # ---------------------------------------------------------------- L_Vael
    if not levels.load_level(NEW_LEVEL):
        log("konnte " + NEW_LEVEL + " nicht oeffnen")
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    load_all()
    actors = editor.get_all_level_actors()
    old = [a for a in actors if unreal.Name(TAG) in a.get_editor_property("tags")]
    for actor in old:
        editor.destroy_actor(actor)
    log("{} Actors des letzten Laufs entfernt".format(len(old)))
    lands = [a for a in editor.get_all_level_actors() if isinstance(a, unreal.LandscapeProxy)]
    min_x = min(a.get_actor_bounds(False)[0].x - a.get_actor_bounds(False)[1].x for a in lands)
    min_y = min(a.get_actor_bounds(False)[0].y - a.get_actor_bounds(False)[1].y for a in lands)
    max_y = max(a.get_actor_bounds(False)[0].y + a.get_actor_bounds(False)[1].y for a in lands)
    # south of the land (X is north), in the middle of it from west to east
    offset = unreal.Vector(min_x - GAP - size_x, (min_y + max_y) / 2 - size_y / 2, HEIGHT)
    log("Land ab X {:.0f} m; Hoehlen bei {:.0f} / {:.0f} / {:.0f} m, {:.0f} x {:.0f} m".format(min_x / 100, offset.x / 100, offset.y / 100,
                                                                                         offset.z / 100, size_x / 100, size_y / 100))
    centre = unreal.Vector(offset.x + size_x / 2, offset.y + size_y / 2, offset.z)
    cave_map = world_map(offset)

    placed = 0
    exits = []
    for text in texts:
        for actor in unreal.VaelWorldBuilder.paste_actors_from_text(world, text):
            if actor.get_attach_parent_actor() is None:
                loc = actor.get_actor_location()
                actor.set_actor_location(unreal.Vector(loc.x + offset.x, loc.y + offset.y, loc.z + offset.z), False, True)
            folder = str(actor.get_folder_path())
            actor.set_folder_path("{}/{}".format(FOLDER, folder if folder and folder != "None" else "Sonstiges"))
            actor.set_editor_property("tags", list(actor.get_editor_property("tags")) + [unreal.Name(TAG)])
            if isinstance(actor, unreal.PostProcessVolume):
                actor.set_editor_property("unbound", False)
                actor.set_actor_location(unreal.Vector(centre.x, centre.y, centre.z + 1000.0), False, True)
                actor.set_actor_scale3d(unreal.Vector(size_x / 200.0 + 20.0, size_y / 200.0 + 20.0, 120.0))
                actor.set_editor_property("blend_radius", 2000.0)
                # before the world's own post process (made for daylight): with its exposure the lamps of the caves were black
                actor.set_editor_property("priority", 50.0)
                settings = actor.get_editor_property("settings")
                settings.set_editor_property("override_auto_exposure_method", True)
                settings.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_HISTOGRAM)
                # without the workshop's cool fill light from above (L_Vael has one sun) a little more exposure
                settings.set_editor_property("auto_exposure_bias", settings.get_editor_property("auto_exposure_bias") + 0.7)
                actor.set_editor_property("settings", settings)
            elif isinstance(actor, unreal.VaelIndoorLevel):
                actor.set_editor_property("bWholeLevel", False)
                actor.set_actor_location(centre, False, True)
                actor.set_editor_property("extent", unreal.Vector(size_x / 2 + 2000.0, size_y / 2 + 2000.0, 8000.0))
                if cave_map is not None:
                    actor.set_editor_property("place_map", cave_map)
                actor.set_editor_property("is_spatially_loaded", False)
            elif isinstance(actor, unreal.VaelRegion):
                actor.set_editor_property("is_spatially_loaded", False)
            elif isinstance(actor, unreal.VaelCaveEntrance):
                exits.append(actor)
            placed += 1
    log("{} Actors eingefuegt".format(placed))

    # the exits lead up into the world in front of the mouth they belong to
    for exit_ in exits:
        place = exit_.get_editor_property("return_place")
        z = ground_at(world, place.x, place.y)
        exit_.set_editor_property("bInLevel", True)
        exit_.set_editor_property("destination", unreal.Vector(place.x, place.y, z if z is not None else 2500.0))
        exit_.set_editor_property("target_level", unreal.Name("None"))
        # deep in its pocket against the rock, a smaller mouth, and a word first (Boris 23.11.: suddenly beamed out in a tunnel)
        exit_.set_editor_property("MouthRadius", 140.0)
        exit_.set_editor_property("HintRadius", 900.0)
        exit_.set_editor_property("Hint", unreal.Text("Weg nach oben"))
        exit_.set_editor_property("Message", unreal.Text("Hinauf ins Freie: Eisenschlund"))
        log("Ausgang {:.0f}/{:.0f} m -> Welt {:.0f}/{:.0f} m, Boden {}".format(exit_.get_actor_location().x / 100, exit_.get_actor_location().y / 100,
                                                                              place.x / 100, place.y / 100, z))
    # the mouths in the world lead down into the caves (they pointed at L_Eisenschlund)
    mouths = 0
    for actor in editor.get_all_level_actors():
        if isinstance(actor, unreal.VaelCaveEntrance) and str(actor.get_editor_property("target_level")) == "L_Eisenschlund":
            arrive = actor.get_editor_property("return_place")
            actor.modify()
            actor.set_editor_property("bInLevel", True)
            actor.set_editor_property("destination", unreal.Vector(arrive.x + offset.x, arrive.y + offset.y, offset.z))
            mouths += 1
    log("{} Eingaenge in der Welt fuehren jetzt in die Hoehlen der Karte".format(mouths))

    # the roof: keeps sun, moon and sky out; high above the floor, above the camera
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    dark = unreal.load_asset("/Game/Vael/Maps/Blockout/MI_Blockout_Dunkel")
    roof = editor.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(centre.x, centre.y, offset.z + ROOF + 100.0), unreal.Rotator(0, 0, 0))
    roof.static_mesh_component.set_static_mesh(cube)
    if dark is not None:
        roof.static_mesh_component.set_material(0, dark)
    roof.set_actor_scale3d(unreal.Vector(size_x / 100.0 + 120.0, size_y / 100.0 + 120.0, 2.0))
    roof.static_mesh_component.set_collision_profile_name("NoCollision")
    roof.set_actor_label("Hoehlendach")
    roof.set_folder_path(FOLDER)
    roof.set_editor_property("tags", [unreal.Name(TAG)])

    unreal.EditorLoadingAndSavingUtils.save_map(world, NEW_LEVEL)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    info = {"offset_cm": [offset.x, offset.y, offset.z], "groesse_cm": [size_x, size_y]}
    with open(os.path.join(os.path.dirname(DATA), "Eisenschlund_Welt.json"), "w", encoding="utf-8") as f:
        json.dump(info, f, indent=1)
    log("fertig: {}".format(info))


main()
