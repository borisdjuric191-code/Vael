# Brings the places of today's L_Aschenmark into the open world L_Vael, each at its place of the zone profile: the survivors'
# camp with Edda, Albrun and the survivors, the village with its houses and well, the chapel, the Gildenkontor with Maren Holt,
# the Gildenspeicher with the cassette, the pond, the Orden der Narbe with its preachers, the Nid spring, the crater of the
# Glutkönigin and the fields with the chest. Quest markers, scrolls, plants and lights go with the place they are nearest to.
#
# L_Aschenmark is only read, never saved. Its actors are copied as text (the editor's copy and paste, unreal.VaelWorldBuilder) and
# moved: every place keeps its layout,
# the whole group is shifted to its new place and each actor set onto the new ground (Scripts/Data/Welt/AM_Hoehen.r16).
# Left behind: its light and sky, ground, scattering, roads and edge, and the free creatures (the clearings of L_Vael have their own).
# Its post process (exposure and colour of the Aschenmark) comes along with the camp.
#
#   VAEL_PORT_DRY=1   only prints the groups (centre, radius, actors) and changes nothing
#
# Run after BuildVaelWelt.py, with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PortAschenmarkToWelt.py" -AllowCommandletRendering

import array
import json
import math
import os

import unreal

OLD_LEVEL = "/Game/Vael/Maps/L_Aschenmark"
NEW_LEVEL = "/Game/Vael/Maps/L_Vael"
DATA = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Data", "Welt")
SIZE_X, SIZE_Y = 2017, 2521
M = 100.0
NEW_FOLDER = "Kesselgrund (aus L_Aschenmark)"

# Folder of the old level → group; the group's new place in AM_Orte.json
GROUPS = {
    "Lager": ("Lager der Ueberlebenden", ("camps", "Lager der Überlebenden")),
    "Dorf": ("Dorf", ("places", "Ruinen von Kesselgrund")),
    "Gildenspeicher": ("Gildenspeicher", ("places", "Gildenspeicher")),
    "Kontor": ("Gildenkontor", ("camps", "Gildenkontor")),
    "Teich": ("Teich", ("places", "Teich")),
    "Orden der Narbe": ("Orden der Narbe", ("places", "Kathedrale des Ordens")),
    "Mark-Quelle": ("Nid-Quelle", ("places", "Nid-Quelle")),
    "Krater": ("Krater", ("places", "Krater der Glutkönigin")),
    "Felder": ("Felder", ("places", "Ascheäcker")),
}
# Actors of these folders stay behind
SKIP_FOLDERS = ("Streuung", "Wege", "Rand", "Boden", "Spiel")
SKIP_CREATURES = ("Gegner/Glutkriecher", "Gegner/Aschharpyien", "Gegner/Aschenroller", "Gegner/Schleieraffen")
SKIP_CLASSES = ("DirectionalLight", "SkyLight", "SkyAtmosphere", "ExponentialHeightFog", "VaelRegion", "WorldSettings",
                "Brush", "DefaultPhysicsVolume", "WorldDataLayers", "WorldPartitionMiniMap")

editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def log(message):
    unreal.log_warning("[PortAschenmark] " + message)


def load_heights():
    heights = array.array("H")
    with open(os.path.join(DATA, "AM_Hoehen.r16"), "rb") as f:
        heights.frombytes(f.read())
    return heights


def ground(heights, x_m, y_m):
    """Bilinear height of the new land in m at a world place in m"""
    x = min(max(x_m, 0.0), SIZE_X - 1.001)
    y = min(max(y_m, 0.0), SIZE_Y - 1.001)
    ix, iy = int(x), int(y)
    tx, ty = x - ix, y - iy

    def h(i, j):
        return (heights[j * SIZE_X + i] - 32768) / 128.0
    return (h(ix, iy) * (1 - tx) + h(ix + 1, iy) * tx) * (1 - ty) + (h(ix, iy + 1) * (1 - tx) + h(ix + 1, iy + 1) * tx) * ty


def follow_mask(component, dx, dy):
    """Water of the pond lays its shore mask over the world (MaskOrigin): a copy of its material with the mask moved along"""
    library = unreal.MaterialEditingLibrary
    for index in range(component.get_num_materials()):
        material = component.get_material(index)
        if not isinstance(material, unreal.MaterialInstanceConstant) or "MaskOrigin" not in [str(n) for n in library.get_vector_parameter_names(material)]:
            continue
        source = material.get_path_name().split(".")[0]
        path = source + "_Welt"
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            moved = unreal.load_asset(path)
        else:
            moved = unreal.EditorAssetLibrary.duplicate_asset(source, path)
            origin = library.get_material_instance_vector_parameter_value(material, "MaskOrigin")
            library.set_material_instance_vector_parameter_value(moved, "MaskOrigin", unreal.LinearColor(origin.r + dx, origin.g + dy, origin.b, origin.a))
            library.update_material_instance(moved)
            unreal.EditorAssetLibrary.save_loaded_asset(moved, False)
        component.set_material(index, moved)


def group_of(actor):
    """Group by folder (the Gildenspeicher lives in the village folder), None if the actor stays behind"""
    folder = str(actor.get_folder_path())
    label = actor.get_actor_label()
    cls = actor.get_class().get_name()
    if cls in ("PlayerStart", "PostProcessVolume"):
        return "Lager"
    if cls in SKIP_CLASSES or folder.startswith(SKIP_FOLDERS) or folder.startswith(SKIP_CREATURES):
        return None
    if folder.startswith("Dorf/Gildenspeicher") or "Gildenspeicher" in label or label == "Kassette der Gilde":
        return "Gildenspeicher"
    top = folder.split("/")[0]
    if top in GROUPS:
        return top
    if top == "Schutzzonen":
        return "Kontor" if "Kontor" in label else "Lager"
    if top == "Gegner":
        return {"Gegner/Prediger": "Orden der Narbe", "Gegner/Glutkoenigin": "Krater", "Gegner/Sichelpuppen": "Felder"}.get(folder)
    if top == "Quests":
        for key, word in (("Dorf", "Dorf"), ("Dorf", "Brunnen"), ("Orden der Narbe", "Orden"), ("Krater", "Krater"), ("Kontor", "Kontor")):
            if word in label:
                return key
    return "nearest"


def main():
    with open(os.path.join(DATA, "AM_Orte.json"), encoding="utf-8") as f:
        places = json.load(f)
    heights = load_heights()

    if not levels.load_level(OLD_LEVEL):
        log("could not open " + OLD_LEVEL)
        return
    old_actors = editor.get_all_level_actors()

    members = {key: [] for key in GROUPS}
    nearest = []
    for actor in old_actors:
        key = group_of(actor)
        if key is None:
            continue
        if key == "nearest":
            nearest.append(actor)
        else:
            members[key].append(actor)

    # Centre and size of each group from its own actors
    centres = {}
    for key, group in members.items():
        if not group:
            continue
        xs = [a.get_actor_location().x / M for a in group]
        ys = [a.get_actor_location().y / M for a in group]
        cx, cy = sum(xs) / len(xs), sum(ys) / len(ys)
        radius = max(math.hypot(x - cx, y - cy) for x, y in zip(xs, ys))
        centres[key] = (cx, cy, radius)
    for actor in nearest:
        l = actor.get_actor_location()
        key = min(centres, key=lambda k: math.hypot(l.x / M - centres[k][0], l.y / M - centres[k][1]))
        members[key].append(actor)
    # Names stay valid when the old level is opened again, the actor objects do not
    members = {key: [a.get_name() for a in group] for key, group in members.items()}

    for key, (cx, cy, radius) in centres.items():
        target = places[GROUPS[key][1][0]][GROUPS[key][1][1]]
        log("group {:16s} {:4d} actors, centre {:6.1f} {:6.1f}, radius {:5.1f} m -> {} ({:.0f}, {:.0f})".format(
            key, len(members[key]), cx, cy, radius, GROUPS[key][1][1], target["x"], target["y"]))

    if os.environ.get("VAEL_PORT_DRY") == "1":
        return

    # Copy each group, paste it into the open world and move it to its place
    moved = 0
    for key, group in members.items():
        if not group:
            continue
        if not levels.load_level(OLD_LEVEL):
            log("could not open " + OLD_LEVEL)
            return
        old = {a.get_name(): a for a in editor.get_all_level_actors()}
        selection = [old[name] for name in group if name in old]
        text = unreal.VaelWorldBuilder.copy_actors_as_text(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), selection)
        log("{}: {} of {} actors found, {} characters copied".format(key, len(selection), len(group), len(text)))

        if not levels.load_level(NEW_LEVEL):
            log("could not open " + NEW_LEVEL)
            return
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        pasted = list(unreal.VaelWorldBuilder.paste_actors_from_text(world, text))

        cx, cy, _ = centres[key]
        target = places[GROUPS[key][1][0]][GROUPS[key][1][1]]
        for actor in pasted:
            l = actor.get_actor_location()
            x = target["x"] + (l.x / M - cx)
            y = target["y"] + (l.y / M - cy)
            actor.set_actor_location(unreal.Vector(x * M, y * M, l.z + ground(heights, x, y) * M), False, False)
            if isinstance(actor, unreal.StaticMeshActor):
                follow_mask(actor.static_mesh_component, (target["x"] - cx) * M, (target["y"] - cy) * M)
            folder = str(actor.get_folder_path())
            actor.set_folder_path("{}/{}".format(NEW_FOLDER, folder if folder and folder != "None" else GROUPS[key][0]))
        moved += len(pasted)
        log("pasted {} actors of {} at {}".format(len(pasted), key, GROUPS[key][1][1]))

        unreal.EditorLoadingAndSavingUtils.save_map(world, NEW_LEVEL)
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    log("done, {} actors brought into {}".format(moved, NEW_LEVEL))


main()
