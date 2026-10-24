# Puts the veins of the Nid (AVaelNidVeins) into L_Wurzelforst_Blockout and grows their network (Boris 2026-10-22: like the
# vessels of a living body, the dragon's: a few thick trunks at the Nid spring forking again and again into ever thinner
# veins, uneven, winding, never straight; mostly open, now and then under the ground; creeping slowly away from the spring).
#
# The network grows here on the height map of the kit (WF_Hoehenkarte_2017.png): the landscape streams in cells, so neither
# this commandlet nor the game at its start could trace it everywhere. Each vein winds (it keeps turning a little the way it
# turned, pulled gently away from the spring), swells and narrows, and forks like a vessel: both arms together carry what
# it carried (their widths cubed add up to its width cubed), the side arm leaving at a wide angle. Under the standing water
# of the Pfuhlmoor a vein runs hidden and comes out at the shore. The points (every metre) are stored in the actor.
#
# The angular placeholder veins of the blockout (cubes NidAder_*) are hidden, not deleted, and keep their collision.
# Everything it adds is tagged WF_NidAdern (Outliner folder Wurzelforst/NidAdern); rerunning replaces only that.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PlaceNidVeins.py"

import math
import os
import random
import struct
import zlib

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
KIT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Blockout", "Vael_Wurzelforst_Blockout")
TAG = "WF_NidAdern"
FOLDER = "Wurzelforst/NidAdern"

PIXEL = 200.0                 # 1 map pixel = 2 m
HEIGHT_SCALE = 100.0 / 128.0  # heightmap value to cm (landscape Z scale 100)
STEP = 100.0                  # a point every metre (AVaelNidVeins expects this)

# Water level of the Pfuhlmoor in cm (WF_Moor.json wasserspiegel_cm, plus the lift of the water plane)
WATER_LEVEL = 10.0

# The Nid spring: where the blockout put its basin (placeholder NidQuelle_Becken; the blockout moved everything with the
# landscape when Unreal shifted it on import)
SPRING_LABEL = "NidQuelle_Becken"

# One network: seed, trunks, width of the trunks and the thinnest vein in cm, reach of a trunk in cm, distance between forks
# in cm (on a trunk; thinner veins fork sooner), how restlessly they wind (degrees per metre), share of the way that lies
# open, how often a vein may fork into forks
NETWORKS = [
    {"name": "NidAdern_Quelle", "at": SPRING_LABEL, "seed": 7, "trunks": 5, "trunk_width": 240.0, "min_width": 40.0,
     "reach": 24000.0, "fork_every": 3800.0, "wander": 6.0, "open_share": 0.75, "max_depth": 4},
]

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(message):
    unreal.log_warning("[PlaceNidVeins] " + message)


def read_png(path):
    """Plain PNG reader (8 or 16 bit, grey or RGB, not interlaced), the same as ScatterWurzelforst.py's"""
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
    """Ground height in world cm, bilinear between the landscape vertices"""

    def __init__(self, origin):
        self.origin = origin
        self.size, _, _, _, rows = read_png(os.path.join(KIT, "WF_Hoehenkarte_2017.png"))
        self.heights = [struct.unpack(">{}H".format(self.size), row) for row in rows]

    def inside(self, x, y):
        px, py = (x - self.origin[0]) / PIXEL, (y - self.origin[1]) / PIXEL
        return 1 <= px < self.size - 2 and 1 <= py < self.size - 2

    def height(self, x, y):
        px, py = (x - self.origin[0]) / PIXEL, (y - self.origin[1]) / PIXEL
        j = min(max(int(px), 0), self.size - 2)
        i = min(max(int(py), 0), self.size - 2)
        fx, fy = min(max(px - j, 0.0), 1.0), min(max(py - i, 0.0), 1.0)
        top = self.heights[i][j] * (1 - fx) + self.heights[i][j + 1] * fx
        bottom = self.heights[i + 1][j] * (1 - fx) + self.heights[i + 1][j + 1] * fx
        return self.origin[2] + ((top * (1 - fy) + bottom * fy) - 32768.0) * HEIGHT_SCALE


def grow_network(terrain, net):
    """All veins of a network: lists of (x, y, z, width, along, open), each from the spring outwards"""
    rng = random.Random(net["seed"])
    spring = net["at"]
    veins = []
    # Where veins already run (cells of 4 m), so a vein never crosses another: it ends where it meets one, like a vessel
    # joining it
    taken = {}

    def cell(x, y):
        return int(math.floor(x / 400.0)), int(math.floor(y / 400.0))

    def crosses(x, y, width, vein, along):
        cx, cy = cell(x, y)
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for ox, oy, ow, ov, oa in taken.get((cx + dx, cy + dy), ()):
                    # Its own vein and the vein it forked from, near the fork, do not count
                    if ov == vein or abs(oa - along) < 600.0:
                        continue
                    if math.hypot(ox - x, oy - y) < (ow + width) * 0.5 + 60.0:
                        return True
        return False

    def run(x, y, yaw, width, along, budget, depth, is_open):
        points = []
        veins.append(points)
        vein = len(veins) - 1
        phase = rng.uniform(0.0, 100.0)
        # Besides its long bends a vein snakes in small waves (6 to 15 m long), as vessels do
        wave_length = rng.uniform(600.0, 1500.0)
        wave_reach = rng.uniform(0.4, 0.9)
        turn = 0.0
        stretch = rng.uniform(0.3, 1.0) * 1500.0
        next_fork = rng.uniform(0.4, 1.2) * net["fork_every"] * min(max(width / net["trunk_width"], 0.35), 1.0)
        walked = 0.0
        while walked < budget and width >= net["min_width"] and terrain.inside(x, y):
            # Snaking sideways, never more than about a width and a half
            snake = math.sin(along / wave_length * math.tau + phase) * wave_reach * min(width, 120.0) * 1.5
            px = x - math.sin(math.radians(yaw)) * snake
            py = y + math.cos(math.radians(yaw)) * snake
            if walked > 300.0 and crosses(px, py, width, vein, along):
                break
            z = terrain.height(px, py)
            # Uneven along the way: the vein swells and narrows a little
            drawn = width * (1.0 + 0.16 * math.sin(along * 0.011 + phase) + 0.08 * math.sin(along * 0.037 + phase * 2.3))
            # Under standing water a vein runs hidden
            points.append((px, py, z, drawn, along, is_open and z > WATER_LEVEL + 5.0))
            taken.setdefault(cell(px, py), []).append((px, py, width, vein, along))

            # Open stretches are long, the ones under the ground short
            stretch -= STEP
            if stretch <= 0.0:
                is_open = not is_open
                open_run = rng.uniform(0.5, 1.5) * 1500.0
                stretch = open_run if is_open else open_run * (1.0 - net["open_share"]) / max(net["open_share"], 0.05)

            # It winds: keeps turning a little the way it turned, and is pulled gently away from the spring, so it spreads
            turn = turn * 0.8 + rng.uniform(-1.0, 1.0) * net["wander"]
            away_x, away_y = x - spring[0], y - spring[1]
            if abs(away_x) + abs(away_y) > 1.0:
                away = math.degrees(math.atan2(away_y, away_x))
                turn += ((away - yaw + 180.0) % 360.0 - 180.0) * 0.03
            yaw += turn
            x += math.cos(math.radians(yaw)) * STEP
            y += math.sin(math.radians(yaw)) * STEP
            along += STEP
            walked += STEP
            width *= 0.997

            # It forks like a vessel; the side arm leaves at a wide angle, the vein bends a little the other way
            next_fork -= STEP
            if next_fork <= 0.0 and depth < net["max_depth"]:
                share = rng.uniform(0.15, 0.45)
                side = 1.0 if rng.random() < 0.5 else -1.0
                side_width = width * share ** (1.0 / 3.0)
                width *= (1.0 - share) ** (1.0 / 3.0)
                run(x, y, yaw + side * rng.uniform(35.0, 70.0), side_width, along, (budget - walked) * rng.uniform(0.45, 0.85), depth + 1, is_open)
                yaw -= side * rng.uniform(5.0, 15.0)
                next_fork = rng.uniform(0.5, 1.3) * net["fork_every"] * min(max(width / net["trunk_width"], 0.35), 1.0)

        # Its end runs out thin
        for back in range(min(4, len(points))):
            x0, y0, z0, w0, a0, o0 = points[-1 - back]
            points[-1 - back] = (x0, y0, z0, w0 * (back + 1) / 5.0, a0, o0)

    for trunk in range(net["trunks"]):
        yaw = trunk * 360.0 / net["trunks"] + rng.uniform(-25.0, 25.0)
        width = net["trunk_width"] * rng.uniform(0.75, 1.05)
        x = spring[0] + math.cos(math.radians(yaw)) * net["trunk_width"] * 0.4
        y = spring[1] + math.sin(math.radians(yaw)) * net["trunk_width"] * 0.4
        run(x, y, yaw, width, 0.0, net["reach"] * rng.uniform(0.6, 1.0), 0, True)
    return [vein for vein in veins if len(vein) >= 2]


def main():
    world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    landscape = None
    places = {}
    removed = 0
    hidden = 0
    for actor in actors.get_all_level_actors():
        label = actor.get_actor_label()
        if unreal.Name(TAG) in actor.tags:
            actors.destroy_actor(actor)
            removed += 1
        elif label.startswith("NidAder_"):
            actor.set_actor_hidden_in_game(True)
            try:
                # Also hidden in the editor, saved with the level
                actor.set_editor_property("hidden_ed", True)
            except Exception:
                actor.set_is_temporarily_hidden_in_editor(True)
            hidden += 1
        elif landscape is None and isinstance(actor, unreal.LandscapeProxy):
            landscape = actor
        if label == SPRING_LABEL:
            places[label] = (actor.get_actor_location().x, actor.get_actor_location().y)
    if landscape is None:
        log("NOTE no landscape in " + LEVEL_PATH)
        return
    origin = landscape.get_actor_location()
    terrain = Terrain((origin.x, origin.y, origin.z))

    for net in NETWORKS:
        if net["at"] not in places:
            log("NOTE no {} in the level, {} not placed".format(net["at"], net["name"]))
            continue
        net = dict(net, at=places[net["at"]])
        veins = grow_network(terrain, net)
        points = []
        open_metres = 0.0
        for index, vein in enumerate(veins):
            for at, (x, y, z, width, along, is_open) in enumerate(vein):
                # The ground at both edges of the ridge (1.3 widths), so the vein follows a slope across (AVaelNidVeins: Side is
                # up x forward, left is Y = -1)
                bx, by = vein[max(at - 1, 0)][:2]
                ax, ay = vein[min(at + 1, len(vein) - 1)][:2]
                forward = math.hypot(ax - bx, ay - by) or 1.0
                side_x, side_y = -(ay - by) / forward, (ax - bx) / forward
                reach = width * 0.65
                ground_left = terrain.height(x - side_x * reach, y - side_y * reach) - z
                ground_right = terrain.height(x + side_x * reach, y + side_y * reach) - z
                point = unreal.VaelNidVeinPoint()
                point.set_editor_property("location", unreal.Vector(x, y, z))
                point.set_editor_property("width", width)
                point.set_editor_property("along", along)
                point.set_editor_property("open", is_open)
                point.set_editor_property("vein", index)
                point.set_editor_property("ground_left", ground_left)
                point.set_editor_property("ground_right", ground_right)
                points.append(point)
                open_metres += STEP / 100.0 if is_open else 0.0
        x, y = net["at"]
        actor = actors.spawn_actor_from_class(unreal.VaelNidVeins, unreal.Vector(x, y, terrain.height(x, y)), unreal.Rotator(0.0, 0.0, 0.0))
        actor.set_editor_property("points", points)
        actor.set_actor_label(net["name"])
        actor.set_folder_path(FOLDER)
        actor.set_editor_property("tags", [unreal.Name(TAG)])
        log("{}: {} veins, {} points, {:.0f} m open".format(net["name"], len(veins), len(points), open_metres))

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log("{} networks placed; {} old ones removed; {} placeholder veins hidden".format(len(NETWORKS), removed, hidden))


main()
