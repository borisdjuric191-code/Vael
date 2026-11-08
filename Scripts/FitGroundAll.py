# Checks every older static mesh in L_Vael against the ground (goal step 11): ported places, the first dressing,
# settlement buildings, blockouts. The ground is the height map the landscape was built from (Scripts/Data/Welt/AM_Hoehen.r16;
# in a commandlet traces only meet the landscape anyway).
# - floating: the lowest point of the piece lies more than 12 cm above the lowest ground under its footprint, so a gap shows
#   -> lowered until that lowest point sits 4 cm in the ground (unless that would swallow it on a steep slope: then only listed)
# - buried: the whole piece is under the ground -> raised to stand on it
# The new scenery (tag VaelFuellung) is fitted when it is planned and is left out. Big pieces (over 15 m) are left out too
# (cliffs, walls of the land, floors of the places).
# Writes Saved/Boden_Pruefung.json; VAEL_NUR_PRUEFEN=1 only lists.
#
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/FitGroundAll.py" -EnablePlugins=PythonScriptPlugin -unattended

import array
import json
import os

import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WELT = os.path.join(ROOT, "Scripts", "Data", "Welt")
LEVEL = "/Game/Vael/Maps/L_Vael"
M = 100.0
SIZE_X, SIZE_Y = 2017, 2521
SKIP_TAGS = {"VaelFuellung", "VaelBodenAbdruck"}
ONLY_CHECK = os.environ.get("VAEL_NUR_PRUEFEN") == "1"

heights = array.array("H")
with open(os.path.join(WELT, "AM_Hoehen.r16"), "rb") as f:
    heights.frombytes(f.read())


def log(message):
    unreal.log_warning("[FitGroundAll] " + message)


def ground(x, y):
    x = min(max(x, 0.0), SIZE_X - 1.001)
    y = min(max(y, 0.0), SIZE_Y - 1.001)
    ix, iy = int(x), int(y)
    tx, ty = x - ix, y - iy

    def h(i, j):
        return (heights[j * SIZE_X + i] - 32768) / 128.0
    return (h(ix, iy) * (1 - tx) + h(ix + 1, iy) * tx) * (1 - ty) + (h(ix, iy + 1) * (1 - tx) + h(ix + 1, iy + 1) * tx) * ty


def main():
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL):
        log("could not load " + LEVEL)
        return
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    report = {"schwebend": [], "vergraben": [], "zu_steil": [], "steht_auf_etwas": 0}
    checked = 0
    # Every visible mesh as a box, so a piece standing on a table, a shrine or a floor is left as it is
    boxes = {}
    level_actors = actors.get_all_level_actors()
    for other in level_actors:
        if not isinstance(other, unreal.StaticMeshActor) or other.is_hidden_ed():
            continue
        o, e = other.get_actor_bounds(False)
        box = (other, (o.x - e.x) / M, (o.y - e.y) / M, (o.x + e.x) / M, (o.y + e.y) / M, (o.z - e.z) / M, (o.z + e.z) / M)
        for cx in range(int(box[1] // 10), int(box[3] // 10) + 1):
            for cy in range(int(box[2] // 10), int(box[4] // 10) + 1):
                boxes.setdefault((cx, cy), []).append(box)

    def resting(actor, x, y, bottom):
        for other, x0, y0, x1, y1, z0, z1 in boxes.get((int(x // 10), int(y // 10)), []):
            if other != actor and x0 <= x <= x1 and y0 <= y <= y1 and z0 < bottom - 0.05 and bottom - 0.4 <= z1 <= bottom + 0.6:
                return True
        return False

    for actor in level_actors:
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        tags = {str(t) for t in actor.get_editor_property("tags")}
        if tags & SKIP_TAGS or actor.is_hidden_ed() or not actor.static_mesh_component.get_editor_property("visible"):
            continue
        if actor.static_mesh_component.static_mesh is None:
            continue
        origin, extent = actor.get_actor_bounds(False)
        if max(extent.x, extent.y) * 2 > 1500.0 or extent.z < 2.0:
            continue
        x, y = origin.x / M, origin.y / M
        if x < 0 or y < 0 or x > SIZE_X or y > SIZE_Y:
            continue
        checked += 1
        bottom = (origin.z - extent.z) / M
        top = (origin.z + extent.z) / M
        g = [ground(x + ex * extent.x / M * 0.8, y + ey * extent.y / M * 0.8) for ex in (-1, 0, 1) for ey in (-1, 0, 1)]
        low, high = min(g), max(g)
        label = "{} ({})".format(actor.get_actor_label(), actor.get_folder_path())
        delta = 0.0
        if bottom - low > 0.12 and resting(actor, x, y, bottom):
            report["steht_auf_etwas"] += 1
            continue
        natural = actor.get_actor_label() in ("Fels", "Totholz", "Toter Baum", "Gestruepp")
        if top < low + 0.05:
            delta = (low - 0.04) - bottom
            report["vergraben"].append([label, round(x, 1), round(y, 1), round(delta, 2)])
        elif bottom - low > 0.12:
            delta = (low - 0.04) - bottom
            if natural and top + delta < high + 0.1:
                delta = max(delta, -0.6 * (top - bottom))          # rocks and dead wood may sink deeper on a slope
                report["schwebend"].append([label, round(x, 1), round(y, 1), round(-delta, 2)])
            elif top + delta < high + 0.1:
                report["zu_steil"].append([label, round(x, 1), round(y, 1), round(bottom - low, 2)])
                delta = 0.0
            else:
                report["schwebend"].append([label, round(x, 1), round(y, 1), round(-delta, 2)])
        if delta != 0.0 and not ONLY_CHECK:
            actor.modify()
            loc = actor.get_actor_location()
            actor.set_actor_location(unreal.Vector(loc.x, loc.y, loc.z + delta * M), False, False)
    out = os.path.join(ROOT, "Saved", "Boden_Pruefung.json")
    json.dump(report, open(out, "w", encoding="utf-8"), ensure_ascii=False, indent=0)
    log("{} pieces checked: {} floating, {} buried, {} on slopes too steep to lower (listed only), {} standing on something -> {}".format(
        checked, len(report["schwebend"]), len(report["vergraben"]), len(report["zu_steil"]), report["steht_auf_etwas"], out))
    if not ONLY_CHECK:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL)
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
        log("saved")


main()
