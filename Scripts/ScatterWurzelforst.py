# Scatters rocks, stones, roots, moss beds, dead wood and dead bushes over the Wurzelforst blockout (region II),
# by zone, with the packs we have (Megascans, Fab, Rock Collection). Small plants (fern, sorrel, balsam, reeds,
# grass) are not placed here: the landscape grows them itself near the camera (DressWurzelforst.py, GT_WF_*).
#
# - Reads the zones and heights from the blockout kit (T_WF_Zonenkarte.png, WF_Hoehenkarte_2017.png) and keeps
#   clear of the grey box placeholders (WF_Platzhalter.json), the campaign path and the player start.
# - One AVaelScatterField per mesh (many instances in one actor), folder "Wurzelforst/_Kulisse/<kind>", tag WF_Streu.
#   Rerunning first removes its own WF_Streu actors and scatters anew (same seed = same result).
#   The placeholders and everything else in the level stay as they are.
#
# Run with the editor closed, after DressWurzelforst.py:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/ScatterWurzelforst.py" -EnablePlugins=PythonScriptPlugin

import json
import math
import os
import random
import struct
import zlib

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
KIT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Blockout", "Vael_Wurzelforst_Blockout")
PACK_FOLDERS = ["/Game/Megascans", "/Game/Rock_Collection_04", "/Game/Fab", "/Game/Megaplant_Library"]
TAG = "WF_Streu"
FOLDER = "Wurzelforst/_Kulisse"
SEED = 2017

CELL = 600.0          # one candidate spot per 6 x 6 m
PIXEL = 200.0         # 1 map pixel = 2 m
HEIGHT_SCALE = 100.0 / 128.0  # heightmap value to cm (landscape Z scale 100)
DRY = 25.0            # lowest ground in cm for things that don't stand in water
START_CLEAR = 1200.0  # nothing that blocks within 12 m of the player start

# Kind: meshes, scale range, sunk by this share of the height, tilted with the slope, blocks, cull distance (0 = never),
# shadow, steepest slope in degrees, distance to the campaign path in map pixels, may stand in shallow moor water
KINDS = {
    "Felsen": (["SM_Rock_0{}".format(i) for i in range(1, 8)] + ["SM_IcelandicBoulder", "SM_GraniteRockAssembly", "SM_HovsBeachRock"],
               (0.5, 1.5), 0.2, False, True, 0, True, 55, 9, False),
    "Moosfelsen": (["SM_MossyRocksA", "SM_MossyRocksB", "SM_IcelandicMossyRock", "SM_IcelandicMossyRocks", "SM_MossyRock"],
                   (0.8, 1.7), 0.15, True, True, 0, True, 40, 7, False),
    "Steine": (["SM_MossyStonesPack_0{}".format(i) for i in range(4)], (1.5, 4.0), 0.1, True, False, 6000, False, 45, 2, False),
    "Wurzeln": (["SM_ForestRootsA", "SM_ForestRootsB"], (1.0, 2.2), 0.05, True, False, 12000, True, 30, 5, True),
    "Moospolster": (["SM_IcelandicMossClusters_0{}".format(i) for i in (0, 1, 2, 4, 5, 6, 7, 8)],
                    (0.6, 1.4), 0.1, True, False, 12000, False, 35, 4, False),
    "Staemme": (["SM_MossyLog", "SM_TreeLog", "SM_FallenTreeAssembly", "SM_FallenPineTree", "SM_FallenFirTree",
                 "SM_FallenPineAssembly", "SM_BrokenPineAssembly"], (0.8, 1.4), 0.1, True, True, 0, True, 20, 8, True),
    "Aeste": (["SM_WoodBranchA", "SM_WoodBranchB", "SM_WoodBranchC", "SM_DeadBranches_00", "SM_DeadBranches_01", "SM_DeadBranches_02"],
              (0.8, 1.6), 0.05, True, False, 6000, False, 30, 3, False),
    "Stuempfe": (["SM_OldStump", "SM_LichenStump", "SM_TreeStumpB", "SM_BrokenTreeStump", "SM_BrokenPineStump", "SM_FirTreeStump",
                  "SM_WoodStump"], (0.8, 1.5), 0.08, False, True, 0, True, 25, 7, True),
    "Totstaemme": (["SM_TreeStumpA_00"], (0.35, 0.6), 0.03, False, True, 0, True, 20, 10, True),
    "Boeschungen": (["SM_MossyEmbankmentA_00", "SM_MossyEmbankmentA_01", "SM_MossyEmbankmentB", "SM_MossyEmbankmentC",
                     "SM_MossyEmbankmentD", "SM_MossyEmbankmentE"], (0.8, 1.5), 0.25, True, False, 0, True, 20, 8, False),
    "Totbusch": (["SM_DeadBroomBush_V{}".format(i) for i in range(1, 10)], (0.8, 1.4), 0.03, False, False, 8000, False, 35, 3, False),
}

# Pieces per hectare in each zone (zone colours as in wf_generator.py)
GRAT = {"Felsen": 25, "Moosfelsen": 6, "Steine": 70, "Moospolster": 6, "Totbusch": 5}
DENSITY = {
    "hain": {"Moosfelsen": 10, "Steine": 40, "Wurzeln": 30, "Moospolster": 35, "Staemme": 6, "Aeste": 30, "Stuempfe": 4,
             "Boeschungen": 3, "Felsen": 2},
    "saum": {"Felsen": 3, "Moosfelsen": 4, "Steine": 30, "Wurzeln": 6, "Moospolster": 8, "Staemme": 6, "Aeste": 30, "Stuempfe": 8,
             "Totstaemme": 1.5, "Totbusch": 12},
    "asche": {"Totbusch": 30, "Stuempfe": 6, "Totstaemme": 2, "Felsen": 3, "Aeste": 20, "Steine": 20},
    "moor": {"Moosfelsen": 3, "Wurzeln": 15, "Moospolster": 25, "Staemme": 8, "Aeste": 25, "Stuempfe": 10, "Totstaemme": 2,
             "Boeschungen": 2},
    "kamm": {"Felsen": 10, "Moosfelsen": 12, "Steine": 50, "Wurzeln": 8, "Moospolster": 20, "Staemme": 3, "Aeste": 15, "Stuempfe": 3},
    "grat": GRAT, "grat_hoch": GRAT,
    "jaw": {"Felsen": 15, "Steine": 40},
    # Shallow water of the moor lagoons: drowned trunks and stumps (only next to the moor, not in the open sea)
    "lagune": {"Stuempfe": 6, "Totstaemme": 2, "Staemme": 3, "Wurzeln": 4},
}
ZONE_COLORS = {
    "saum": "6e6448", "moor": "3e6b55", "hain": "3f7f45", "kamm": "3b6670", "grat": "6a6d60", "asche": "7a4a2a",
    "jaw": "4a5f78", "grat_hoch": "9a9c90", "lagune": "2f6f74", "nid": "5a3a7a", "weg": "b9a77a",
}


def log(message):
    unreal.log_warning("[ScatterWurzelforst] " + message)


# ---------------------------------------------------------------- Map data

def read_png(path):
    """Plain PNG reader (8 or 16 bit, grey or RGB, not interlaced); returns width, height, channels, depth, rows of samples"""
    data = open(path, "rb").read()
    pos, chunks = 8, []
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if kind == b"IHDR":
            width, height, depth, color = struct.unpack(">IIBB", body[:10])
        elif kind == b"IDAT":
            chunks.append(body)
        elif kind == b"IEND":
            break
        pos += 12 + length
    channels = {0: 1, 2: 3, 4: 2, 6: 4}[color]
    bpp = max(1, channels * depth // 8)
    stride = width * channels * depth // 8
    raw = zlib.decompress(b"".join(chunks))
    rows, previous = [], bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        kind = raw[start]
        line = bytearray(raw[start + 1:start + 1 + stride])
        if kind == 1:
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 255
        elif kind == 2:
            for i in range(stride):
                line[i] = (line[i] + previous[i]) & 255
        elif kind == 3:
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((left + previous[i]) >> 1)) & 255
        elif kind == 4:
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = previous[i]
                c = previous[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(bytes(line))
        previous = line
    return width, height, channels, depth, rows


class Terrain:
    """Heights and zones of the landscape, looked up in world cm"""

    def __init__(self, origin):
        self.origin = origin
        self.size, _, _, _, rows = read_png(os.path.join(KIT, "WF_Hoehenkarte_2017.png"))
        self.heights = [struct.unpack(">{}H".format(self.size), row) for row in rows]
        _, _, self.zone_channels, _, self.zone_rows = read_png(os.path.join(KIT, "T_WF_Zonenkarte.png"))
        self.zone_rgb = {name: [int(c[i:i + 2], 16) for i in (0, 2, 4)] for name, c in ZONE_COLORS.items()}
        self.zone_cache = {}

    def pixel(self, x, y):
        return (x - self.origin[0]) / PIXEL, (y - self.origin[1]) / PIXEL

    def height(self, x, y):
        """Ground height in cm, bilinear between the landscape vertices"""
        px, py = self.pixel(x, y)
        j = min(max(int(px), 0), self.size - 2)
        i = min(max(int(py), 0), self.size - 2)
        fx, fy = min(max(px - j, 0.0), 1.0), min(max(py - i, 0.0), 1.0)
        top = self.heights[i][j] * (1 - fx) + self.heights[i][j + 1] * fx
        bottom = self.heights[i + 1][j] * (1 - fx) + self.heights[i + 1][j + 1] * fx
        return ((top * (1 - fy) + bottom * fy) - 32768.0) * HEIGHT_SCALE

    def normal(self, x, y):
        step = PIXEL
        dx = (self.height(x + step, y) - self.height(x - step, y)) / (2 * step)
        dy = (self.height(x, y + step) - self.height(x, y - step)) / (2 * step)
        length = math.sqrt(dx * dx + dy * dy + 1.0)
        return unreal.Vector(-dx / length, -dy / length, 1.0 / length)

    def zone_at_pixel(self, j, i):
        key = (j, i)
        if key in self.zone_cache:
            return self.zone_cache[key]
        j = min(max(j, 0), self.size - 1)
        i = min(max(i, 0), self.size - 1)
        row = self.zone_rows[i]
        rgb = [row[j * self.zone_channels + k] for k in range(3)]
        best, best_error = None, None
        for name, color in self.zone_rgb.items():
            # The map is hill-shaded (x 0.65 .. 1.25), so each colour is compared at its best brightness
            scale = sum(a * b for a, b in zip(rgb, color)) / max(1, sum(b * b for b in color))
            scale = min(max(scale, 0.6), 1.3)
            error = sum((a - b * scale) ** 2 for a, b in zip(rgb, color))
            if best_error is None or error < best_error:
                best, best_error = name, error
        self.zone_cache[key] = best
        return best

    def zone(self, x, y):
        px, py = self.pixel(x, y)
        return self.zone_at_pixel(int(round(px)), int(round(py)))

    def near_zone(self, x, y, name, reach_px=30):
        px, py = self.pixel(x, y)
        for dj, di in ((reach_px, 0), (-reach_px, 0), (0, reach_px), (0, -reach_px), (reach_px, reach_px), (-reach_px, -reach_px),
                       (reach_px, -reach_px), (-reach_px, reach_px)):
            if self.zone_at_pixel(int(px) + dj, int(py) + di) == name:
                return True
        return False


def placeholder_cells(terrain, prims, offset):
    """Map pixels covered by the low parts of the grey box (trunks, roots, huts, rocks, the path stripes ...)"""
    taken = set()
    for p in prims:
        location = unreal.Vector(float(p["l"][0]) + offset[0], float(p["l"][1]) + offset[1], float(p["l"][2]) + offset[2])
        rotation = unreal.Rotator(roll=float(p["r"][2]), pitch=float(p["r"][0]), yaw=float(p["r"][1]))
        scale = [float(v) for v in p["s"]]
        transform = unreal.Transform(location, rotation, unreal.Vector(*scale))
        # Points through the shape about every 1.5 m
        steps = [min(40, max(1, int(s * 100 / 150) + 1)) for s in scale]
        for a in range(steps[0] + 1):
            for b in range(steps[1] + 1):
                for c in range(steps[2] + 1):
                    local = unreal.Vector(-50 + 100.0 * a / steps[0], -50 + 100.0 * b / steps[1], -50 + 100.0 * c / steps[2])
                    point = transform.transform_location(local)
                    if point.z < terrain.height(point.x, point.y) + 250.0:
                        px, py = terrain.pixel(point.x, point.y)
                        taken.add((int(round(px)), int(round(py))))
    return taken


def path_distance(px, py, route):
    """Distance in map pixels to the campaign path (route in grid pixels)"""
    best = 1e9
    for (ax, ay), (bx, by) in zip(route, route[1:]):
        dx, dy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / max(1e-6, dx * dx + dy * dy)))
        best = min(best, math.hypot(px - ax - t * dx, py - ay - t * dy))
    return best


# ---------------------------------------------------------------- Scattering

def load_meshes():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    wanted = {name for kind in KINDS.values() for name in kind[0]}
    found = {}
    for folder in PACK_FOLDERS:
        for data in registry.get_assets_by_path(folder, recursive=True):
            name = str(data.asset_name)
            if name in wanted and name not in found and str(data.asset_class_path.asset_name) == "StaticMesh":
                found[name] = data.get_asset()
    return found


def allow_instancing(meshes):
    """A master material without the instancing usage shows the grey default material on scattered pieces in the game"""
    library = unreal.MaterialEditingLibrary
    changed = []
    for mesh in meshes:
        for slot in mesh.static_materials:
            if slot.material_interface is None:
                continue
            base = slot.material_interface.get_base_material()
            if base.get_editor_property("used_with_instanced_static_meshes"):
                continue
            library.set_material_usage(base, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
            library.recompile_material(base)
            unreal.EditorAssetLibrary.save_loaded_asset(base, False)
            changed.append(base.get_name())
    return changed


def scatter(terrain, taken, route, start, meshes, rng):
    """Returns {(kind, mesh name): [transforms]}"""
    sizes = {}
    for name, mesh in meshes.items():
        box = mesh.get_bounding_box()
        sizes[name] = (max(box.max.x - box.min.x, box.max.y - box.min.y) * 0.5, box.max.z - box.min.z, box.min.z)

    placed = {}
    lo_x, lo_y = terrain.origin
    span = terrain.size * PIXEL
    count = int(span / CELL)
    share = CELL * CELL / 1e8  # one cell of a hectare
    for gy in range(count):
        for gx in range(count):
            x = lo_x + (gx + rng.random()) * CELL
            y = lo_y + (gy + rng.random()) * CELL
            zone = terrain.zone(x, y)
            table = DENSITY.get(zone)
            if not table:
                continue
            roll = rng.random()
            kind_name = None
            for name, density in table.items():
                roll -= density * share
                if roll < 0:
                    kind_name = name
                    break
            if kind_name is None:
                continue

            names, scale_range, sink, tilt, blocks, cull, shadow, max_slope, path_clear, in_water = KINDS[kind_name]
            ground = terrain.height(x, y)
            if zone == "lagune":
                if ground < -70.0 or not terrain.near_zone(x, y, "moor"):
                    continue
            elif ground < DRY:
                if not (in_water and ground > -70.0 and zone == "moor"):
                    continue
            normal = terrain.normal(x, y)
            if math.degrees(math.acos(max(-1.0, min(1.0, normal.z)))) > max_slope:
                continue
            px, py = terrain.pixel(x, y)
            if path_distance(px, py, route) < path_clear:
                continue
            if blocks and math.hypot(x - start[0], y - start[1]) < START_CLEAR:
                continue

            mesh_name = rng.choice([n for n in names if n in meshes] or [None])
            if mesh_name is None:
                continue
            scale = rng.uniform(*scale_range)
            radius, tall, bottom = sizes[mesh_name]
            reach = max(1, int(math.ceil(radius * scale / PIXEL)))
            ci, cj = int(round(px)), int(round(py))
            if any((ci + a, cj + b) in taken for a in range(-reach, reach + 1) for b in range(-reach, reach + 1)):
                continue

            yaw = rng.uniform(0.0, 360.0)
            if tilt:
                rotation = unreal.MathLibrary.compose_rotators(unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw),
                                                               unreal.MathLibrary.make_rot_from_z(normal))
            else:
                rotation = unreal.Rotator(roll=rng.uniform(-3, 3), pitch=rng.uniform(-3, 3), yaw=yaw)
            z = ground - bottom * scale - sink * tall * scale
            transform = unreal.Transform(unreal.Vector(x, y, z), rotation, unreal.Vector(scale, scale, scale))
            placed.setdefault((kind_name, mesh_name), []).append(transform)
            # Blocking pieces keep the others off their spot
            if blocks:
                for a in range(-reach, reach + 1):
                    for b in range(-reach, reach + 1):
                        taken.add((ci + a, cj + b))
    return placed


def main():
    world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level_actors = actors.get_all_level_actors()

    landscape = next((a for a in level_actors if isinstance(a, unreal.LandscapeProxy)), None)
    if landscape is None:
        log("no landscape in the level, nothing done")
        return
    origin = landscape.get_actor_location()

    removed = 0
    for actor in level_actors:
        if unreal.Name(TAG) in actor.tags:
            actors.destroy_actor(actor)
            removed += 1

    terrain = Terrain((origin.x, origin.y))
    data = json.load(open(os.path.join(KIT, "WF_Platzhalter.json"), encoding="utf-8"))
    taken = placeholder_cells(terrain, data["prims"], (origin.x, origin.y, origin.z))
    silhouette = json.load(open(os.path.join(KIT, "WF_Silhouette.json"), encoding="utf-8"))
    route = [(px + data["OX"], py + data["OY"]) for px, py in silhouette["kampagnenweg"]]
    start = (data["spawn"]["l"][0] + origin.x, data["spawn"]["l"][1] + origin.y)

    meshes = load_meshes()
    missing = sorted({n for kind in KINDS.values() for n in kind[0]} - set(meshes))
    allowed = allow_instancing(meshes.values())
    placed = scatter(terrain, taken, route, start, meshes, random.Random(SEED))

    total = 0
    per_kind = {}
    for (kind_name, mesh_name), transforms in sorted(placed.items()):
        _, _, _, _, blocks, cull, shadow, _, _, _ = KINDS[kind_name]
        field = actors.spawn_actor_from_class(unreal.VaelScatterField, transforms[0].translation, unreal.Rotator(0.0, 0.0, 0.0))
        field.setup_field(meshes[mesh_name], float(cull), shadow, blocks)
        field.add_scatter_instances(transforms)
        field.set_actor_label("Streu_{}_{}".format(kind_name, mesh_name.replace("SM_", "")))
        field.set_folder_path("{}/{}".format(FOLDER, kind_name))
        field.set_editor_property("tags", [unreal.Name(TAG)])
        total += len(transforms)
        per_kind[kind_name] = per_kind.get(kind_name, 0) + len(transforms)

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log("{} pieces in {} fields ({} old fields removed): {}".format(
        total, len(placed), removed, ", ".join("{} {}".format(k, v) for k, v in sorted(per_kind.items()))))
    if missing:
        log("NOTE missing meshes: {}".format(", ".join(missing)))
    if allowed:
        log("NOTE instancing allowed on: {}".format(", ".join(allowed)))


main()
