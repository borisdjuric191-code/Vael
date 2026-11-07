# Graubox of the Glutkammer, the refugium of the Aschenmark, after the room profile Docs/Karte/glutkammer_raumprofil.png
# (Lore-Bibel tab Refugium). Builds the playable level /Game/Vael/Maps/Refugium/L_Glutkammer from the pieces of the look test
# (BuildGlutkammerProbe.py must have run first: it imports the pieces and makes the materials).
#
# Rooms (plan in profile pixels, 140 px = 10 m, north up = world +X, east = world +Y):
#   1 arrival on a root knot in the north, root ramp down        2 root table with the Hexenkueche in the middle of the hall
#   3 four clutch beds (stage I) and empty clutches               4 bed overview (Wurzeltafel) at the foot of the ramp
#   5 Glutbecken in the south of the hall                         6 four plots, Narbensaum between them (marked on the floor)
#   7 Prophet shrine   8 helper nest   9 Tamer den, Engineer bench, Mage glow well
#   10 Wurzeltor (stage IV)   11 Glutgalerie behind rubble (stage IV)   12 brood tunnels (pests come in)   13 stage V opening
# The camera looks north-east: walls there are high (ribs, rock), walls in the south-west stay low so the room stays visible.
# Everything sits in Outliner folders by stage ("Stufe I", "Stufe II", "Stufe IV", "Stufe V") for the stage layers later.
# The level is rebuilt from scratch on every run; it only creates its own level.
#
# Run with the editor closed:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/BuildGlutkammer.py" -unattended -nosplash -AllowCommandletRendering

import math
import random

import unreal

KIT = "/Game/Vael/Refugium/Glutkammer/Probe"
LEVEL_PATH = "/Game/Vael/Maps/Refugium/L_Glutkammer"
ROCKS = ["/Game/Rock_Collection_04/Meshes/Rock_0{0}/StaticMeshes/SM_Rock_0{0}".format(i) for i in range(1, 8)]
PLANTS = ["/Game/Vael/Requisiten/Aschenmark/{0}/SM_{0}".format(n) for n in
          ("Glutdistel", "Glimmklee", "Russrose", "Funkenmohn", "Taublatt", "Milchstern", "Windhalm", "Schlotwinde")]
# Beds by stage (Lore-Bibel, Refugium: 40 beds in the end, so even the rarest seed turns up now and then):
# stage -> (hall, egg chamber, Glutgalerie). Stage I has beds, the later ones lie there as sleeping clutches.
BED_PLAN = {"I": (8, 0, 0), "II": (2, 6, 0), "III": (8, 0, 0), "IV": (2, 0, 6), "V": (4, 4, 0)}
STAGES = ["I", "II", "III", "IV", "V"]
PX = 100.0 / 14.0          # cm per profile pixel
SEED = 31

assets = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(SEED)
notes = []


def log(message):
    unreal.log_warning("[Glutkammer] " + message)


def at(px, py, z=0.0):
    """Point of the room profile (pixels) in the world (cm)"""
    return unreal.Vector((560.0 - py) * PX, (px - 520.0) * PX, z)


def load(name):
    asset = unreal.load_asset("{}/{}".format(KIT, name))
    if asset is None:
        raise RuntimeError(name + " fehlt - zuerst Scripts/BuildGlutkammerProbe.py laufen lassen")
    return asset


# ---------------------------------------------------------------- actors

def spawn(actor_class, location, label, folder, rotation=None):
    actor = actors.spawn_actor_from_class(actor_class, location, rotation or unreal.Rotator(0.0, 0.0, 0.0))
    actor.set_folder_path(folder)
    actor.set_actor_label(label)
    return actor


def mesh_actor(mesh, location, label, folder, yaw=0.0, scale=1.0, pitch=0.0, roll=0.0, material=None, collision=False):
    actor = spawn(unreal.StaticMeshActor, location, label, folder, unreal.Rotator(roll, pitch, yaw))
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    if material is not None:
        component.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(*scale) if isinstance(scale, tuple) else unreal.Vector(scale, scale, scale))
    if not collision:
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return actor


def point_light(location, label, folder, color, intensity, radius, shadows=False):
    light = spawn(unreal.PointLight, location, label, folder)
    c = light.light_component
    c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    c.set_editor_property("intensity", intensity)
    c.set_light_color(unreal.LinearColor(color[0], color[1], color[2], 1.0))
    c.set_editor_property("attenuation_radius", radius)
    c.set_editor_property("cast_shadows", shadows)
    light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    return light


BEAM = []            # the beam mesh, loaded in build()


def shaft(location, label, folder, intensity, cone, dust=True):
    light = spawn(unreal.SpotLight, unreal.Vector(location.x, location.y, 1600.0), label, folder, unreal.Rotator(0.0, -90.0, 0.0))
    c = light.light_component
    c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    c.set_editor_property("intensity", intensity)
    c.set_light_color(unreal.LinearColor(1.0, 0.86, 0.62, 1.0))
    c.set_editor_property("inner_cone_angle", cone * 0.55)
    c.set_editor_property("outer_cone_angle", cone)
    c.set_editor_property("attenuation_radius", 2800.0)
    c.set_editor_property("volumetric_scattering_intensity", 6.0)
    light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    # the beam you see: as wide as the light spot on the floor, 9 m high, fading out towards the top
    radius = 1600.0 * math.tan(math.radians(cone))
    mesh_actor(BEAM[0], unreal.Vector(location.x, location.y, 0.0), label + " Strahl", "Licht/Strahlen",
               yaw=rng.uniform(0, 360), scale=(radius / 100.0, radius / 100.0, 6.5))
    if dust:
        motes(unreal.Vector(location.x, location.y, 0.0), label + " Staub", "Licht/Staub", unreal.VaelCaveMote.DUST,
              int(min(140, 12 + radius * 0.35)), radius * 0.8, 800.0)
    return light


def motes(location, label, folder, kind, count, radius, height, brightness=1.0):
    actor = spawn(unreal.VaelCaveMotes, location, label, folder)
    actor.set_editor_property("kind", kind)
    actor.set_editor_property("count", count)
    actor.set_editor_property("radius", radius)
    actor.set_editor_property("height", height)
    actor.set_editor_property("brightness", brightness)
    return actor


def label_text(location, text, folder, color=(1.0, 0.85, 0.55)):
    """Floating name for the grey box, turned towards the camera (it looks north-east from above)"""
    actor = spawn(unreal.TextRenderActor, unreal.Vector(location.x, location.y, location.z + 260.0), "Schild " + text, folder + "/Schilder",
                  unreal.Rotator(0.0, 40.0, 225.0))
    t = actor.text_render
    t.set_text(unreal.Text(text))
    t.set_editor_property("world_size", 55.0)
    t.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
    t.set_text_render_color(unreal.Color(r=int(color[0] * 255), g=int(color[1] * 255), b=int(color[2] * 255), a=255))
    return actor


# ---------------------------------------------------------------- rooms

class Room:
    def __init__(self, name, px, py, rx, ry):
        self.name = name
        self.c = at(px, py)
        self.rx = rx * PX   # east-west half size (world Y)
        self.ry = ry * PX   # north-south half size (world X)

    def inside(self, p, margin=0.0):
        dx = (p.x - self.c.x) / (self.ry + margin)
        dy = (p.y - self.c.y) / (self.rx + margin)
        return dx * dx + dy * dy < 1.0

    def edge(self, angle):
        return unreal.Vector(self.c.x + math.cos(angle) * self.ry, self.c.y + math.sin(angle) * self.rx, 0.0)


HALL = Room("Haupthalle", 530, 566, 255, 262)
ROOMS = [
    HALL,
    Room("Eierkammer", 235, 520, 115, 185),
    Room("Bau", 850, 400, 95, 100),
    Room("Glutbrunnen", 862, 690, 75, 70),
    Room("Glutgalerie", 580, 915, 285, 115),
    Room("Wurzelknoten", 560, 175, 55, 55),
]
RAMP = [(560, 190), (600, 262), (650, 272), (690, 300), (720, 322), (735, 380)]
TUNNELS = [[(150, 400), (110, 300), (70, 190)], [(850, 900), (910, 950), (985, 1040)], [(905, 470), (950, 520), (990, 600)]]


def path_points(points, step_px=18):
    out = []
    for (ax, ay), (bx, by) in zip(points, points[1:]):
        n = max(1, int(math.hypot(bx - ax, by - ay) / step_px))
        for k in range(n):
            t = k / n
            out.append((ax + (bx - ax) * t, ay + (by - ay) * t))
    out.append(points[-1])
    return out


CORRIDORS = [Room("Rampe", x, y, 34, 34) for x, y in path_points(RAMP)]
# places the scattered things keep away from (profile pixels and radius): table, pool, beds, corners, gate
KEEP_FREE = [(520, 560, 120), (520, 760, 95), (395, 470, 45), (651, 480, 45), (391, 655, 45), (649, 661, 45), (226, 350, 45),
             (355, 370, 45), (855, 420, 70), (745, 600, 50), (865, 690, 50), (717, 400, 40), (112, 560, 60)]
for tunnel in TUNNELS:
    CORRIDORS += [Room("Gang", x, y, 16, 16) for x, y in path_points(tunnel)]
OPEN = ROOMS + CORRIDORS


def open_at(p, margin=0.0, skip=None):
    return any(room.inside(p, margin) for room in OPEN if room is not skip)


def farness(direction_x, direction_y):
    """1 for walls on the far side of the camera (north-east), -1 for walls next to it"""
    length = math.hypot(direction_x, direction_y) or 1.0
    return (direction_x + direction_y) / (length * math.sqrt(2.0))


# ---------------------------------------------------------------- beds

_slots = []
first_slot = {}


def near_line(p, ax, ay, bx, by, distance):
    a, b = at(ax, ay), at(bx, by)
    dx, dy = b.x - a.x, b.y - a.y
    t = max(0.0, min(1.0, ((p.x - a.x) * dx + (p.y - a.y) * dy) / max(dx * dx + dy * dy, 1.0)))
    return math.hypot(p.x - a.x - dx * t, p.y - a.y - dy * t) < distance


def free_for_bed(p):
    table, pool_at = at(520, 560), at(520, 760)
    if math.hypot(p.x - table.x, p.y - table.y) < 560.0 or math.hypot(p.x - pool_at.x, p.y - pool_at.y) < 430.0:
        return False
    for x, y in ((226, 350), (355, 370), (855, 420), (745, 600), (865, 690), (717, 400), (112, 560)):
        q = at(x, y)
        if math.hypot(p.x - q.x, p.y - q.y) < 280.0:
            return False
    # paths: the Narbensaum lines between the plots stay walkable
    for line in ((520, 300, 520, 830), (280, 560, 800, 560)):
        if near_line(p, *line, distance=170.0):
            return False
    return not any(c.inside(p, margin=150.0) for c in CORRIDORS)


def grid_in(room, spacing, margin):
    found = []
    steps = int(max(room.rx, room.ry) / spacing) + 2
    for j in range(-steps, steps + 1):
        for i in range(-steps, steps + 1):
            p = unreal.Vector(room.c.x + j * spacing * 0.866, room.c.y + (i + 0.5 * (j % 2)) * spacing, 0.0)
            if room.inside(p, margin=-margin) and free_for_bed(p):
                found.append(p)
    return found


def bed_slots():
    """(stage, room name, location) of every bed place; worked out once"""
    if _slots:
        return _slots
    # Hall: per plot the nearest places to the root table, then dealt out rank by rank over the four plots
    table = at(520, 560)
    quarters = [[], [], [], []]
    for p in grid_in(HALL, 330.0, 330.0):
        quarters[(p.x > table.x) * 2 + (p.y > table.y)].append(p)
    per_plot = sum(plan[0] for plan in BED_PLAN.values()) // 4
    for q in quarters:
        q.sort(key=lambda p: math.hypot(p.x - table.x, p.y - table.y))
    order = [q[rank] for rank in range(per_plot) for q in quarters if rank < len(q)]
    hall_stages = [stage for stage in STAGES for _ in range(BED_PLAN[stage][0])]
    # within a rank the plots must get the same stage where possible: the plan above fills rank by rank
    for stage, p in zip(hall_stages, order):
        _slots.append((stage, "Halle", p))
    for room, index, name in ((ROOMS[1], 1, "Eierkammer"), (ROOMS[4], 2, "Glutgalerie")):
        places = grid_in(room, 300.0 if index == 1 else 420.0, 250.0)
        if index == 2:
            veins = [at(x, y) for x, y in ((420, 930), (560, 905), (700, 940), (480, 990), (650, 1000))]
            places = [p for p in places if min(math.hypot(p.x - v.x, p.y - v.y) for v in veins) > 170.0]
        places.sort(key=lambda p: math.hypot(p.x - room.c.x, p.y - room.c.y))
        stages = [stage for stage in STAGES for _ in range(BED_PLAN[stage][index])]
        for stage, p in zip(stages, places):
            _slots.append((stage, name, p))
    for stage, _, p in _slots:
        first_slot.setdefault(stage, p)
    counts = {stage: sum(1 for st, _, _ in _slots if st == stage) for stage in STAGES}
    log("Beetplaetze {} von {}: {}".format(len(_slots), sum(sum(v) for v in BED_PLAN.values()), counts))
    return _slots


# ---------------------------------------------------------------- build

def build():
    if assets.does_asset_exist(LEVEL_PATH):
        assets.delete_asset(LEVEL_PATH)
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)

    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    cylinder = unreal.load_asset("/Engine/BasicShapes/Cylinder.Cylinder")
    cone = unreal.load_asset("/Engine/BasicShapes/Cone.Cone")
    sphere = unreal.load_asset("/Engine/BasicShapes/Sphere.Sphere")
    rib, sac, bed, roots, pool = (load(n) for n in ("SM_GK_Rippenwand", "SM_GK_Eisack", "SM_GK_Gelegebeet", "SM_GK_Wurzelvorhang", "SM_GK_Glutbecken"))
    sleeping = load("SM_GK_Gelege_Schlafend")
    m_rock = load("MI_GK_Fels_Brocken")
    BEAM[:] = [load("SM_GK_Lichtstrahl")]
    m_floor, m_tissue, m_roots, m_pool = (load(n) for n in ("M_GK_Boden", "M_GK_Gewebe", "M_GK_Wurzel", "M_GK_Becken"))
    mi_membrane, mi_eggs = load("MI_GK_Membran"), load("MI_GK_Eier")
    rocks = [r for r in (unreal.load_asset(p) for p in ROCKS) if r is not None]
    plants = [m for m in (unreal.load_asset(p) for p in PLANTS) if m is not None] or [None]

    spawn(unreal.VaelIndoorLevel, at(520, 560, 400.0), "Innenraum", "Spiel")

    # Floor: one slab under everything, its top at 0
    mesh_actor(cube, at(520, 600, -50.0), "Boden", "Raum", scale=(95.0, 95.0, 1.0), material=m_floor, collision=True)

    # Walls of cooled lava along the edge of every room: high in the north-east, low in the south-west, boulders at their foot
    walls = {name: load(name) for name in ("SM_GK_Felswand_Hoch", "SM_GK_Felswand_Mittel", "SM_GK_Felswand_Niedrig")}
    count = boulders = 0
    for room in ROOMS + CORRIDORS[::3]:
        circumference = 2 * math.pi * math.sqrt((room.rx ** 2 + room.ry ** 2) / 2)
        piece = 560.0 if room in ROOMS else 420.0
        steps = max(6, int(circumference / piece))
        for k in range(steps):
            a = 2 * math.pi * k / steps + rng.uniform(-0.03, 0.03)
            edge = room.edge(a)
            out_x, out_y = math.cos(a) / max(room.ry, 1.0), math.sin(a) / max(room.rx, 1.0)
            norm = math.hypot(out_x, out_y)
            out_x, out_y = out_x / norm, out_y / norm
            test = unreal.Vector(edge.x + out_x * 140.0, edge.y + out_y * 140.0, 0.0)
            if open_at(test, margin=40.0):
                continue
            far = farness(out_x, out_y)
            if room in CORRIDORS:
                name, scale = ("SM_GK_Felswand_Mittel" if far > 0.2 else "SM_GK_Felswand_Niedrig"), 0.7
            else:
                name = "SM_GK_Felswand_Hoch" if far > 0.3 else "SM_GK_Felswand_Mittel" if far > -0.45 else "SM_GK_Felswand_Niedrig"
                scale = rng.uniform(0.9, 1.15)
            place = unreal.Vector(edge.x + out_x * 40.0, edge.y + out_y * 40.0, -10.0)
            yaw = math.degrees(math.atan2(-out_y, -out_x)) + rng.uniform(-7, 7)
            mesh_actor(walls[name], place, "Felswand {}".format(count + 1), "Raum/Fels", yaw=yaw, scale=(scale, scale * rng.uniform(0.9, 1.1), scale),
                       collision=True)
            count += 1
            # warm glow from the cracks at the foot of the high walls, so they read against the dark
            if name != "SM_GK_Felswand_Niedrig" and room in ROOMS and count % 2 == 0:
                point_light(unreal.Vector(edge.x - out_x * 120.0, edge.y - out_y * 120.0, 70.0), "Felsglut {}".format(count), "Licht/Felsglut",
                            (1.0, 0.42, 0.12), 28.0, 650.0)
            # the high walls on the far side get grazing light from in front, as if from the holes in the ceiling, so their
            # ledges and cracks read from the game camera instead of sinking into black
            if name == "SM_GK_Felswand_Hoch" and room is HALL:
                wash = spawn(unreal.SpotLight, unreal.Vector(edge.x - out_x * 520.0, edge.y - out_y * 520.0, 900.0),
                             "Wandlicht {}".format(count), "Licht/Wandlicht",
                             unreal.Rotator(0.0, -38.0, math.degrees(math.atan2(out_y, out_x))))
                c = wash.light_component
                c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
                c.set_editor_property("intensity", 260.0)
                c.set_light_color(unreal.LinearColor(1.0, 0.78, 0.55, 1.0))
                c.set_editor_property("inner_cone_angle", 18.0)
                c.set_editor_property("outer_cone_angle", 34.0)
                c.set_editor_property("attenuation_radius", 1400.0)
                c.set_editor_property("cast_shadows", False)
                c.set_editor_property("volumetric_scattering_intensity", 0.0)
                wash.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
            if room in ROOMS and rng.random() < 0.55:
                rock = rocks[boulders % len(rocks)]
                box = rock.get_bounding_box()
                size = box.max - box.min
                s = rng.uniform(80.0, 190.0) / max(size.z, 1.0)
                s = min(s, 260.0 / max(size.x, size.y, 1.0))
                side = rng.uniform(-200.0, 200.0)
                spot = unreal.Vector(edge.x - out_x * 60.0 - out_y * side, edge.y - out_y * 60.0 + out_x * side, -25.0)
                if not open_at(spot, margin=-120.0, skip=room) and not any(c.inside(spot, margin=60.0) for c in CORRIDORS):
                    mesh_actor(rock, spot, "Brocken {}".format(boulders + 1), "Raum/Fels", yaw=rng.uniform(0, 360), scale=s, collision=True,
                               material=m_rock)
                    boulders += 1
    log("{} Felswaende, {} Brocken".format(count, boulders))

    # Rib walls with membranes and egg sacs on the far half of the hall and in the den
    ribs = 0
    for room, start, end, spacing, scale in ((HALL, -20.0, 110.0, 980.0, 1.0), (ROOMS[2], -10.0, 100.0, 620.0, 0.75)):
        a = math.radians(start)
        while a <= math.radians(end):
            edge = room.edge(a)
            inward = unreal.Vector(room.c.x - edge.x, room.c.y - edge.y, 0.0)
            length = math.hypot(inward.x, inward.y)
            pos = unreal.Vector(edge.x + inward.x / length * 90.0, edge.y + inward.y / length * 90.0, 0.0)
            # not where the ramp, a tunnel or a neighbouring room opens the wall
            if not any(other.inside(pos, margin=180.0) for other in OPEN if other is not room):
                yaw = math.degrees(math.atan2(inward.y, inward.x))
                mesh_actor(rib, pos, "Rippenwand {}".format(ribs + 1), "Stufe I/Rippen", yaw=yaw, scale=scale)
                for gy in (-180.0, 0.0, 180.0):
                    if rng.random() < 0.5:
                        continue
                    z = rng.uniform(80.0, 280.0) * scale
                    c, s_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
                    lx, ly = -12.0 + z * 0.02, gy * scale
                    loc = unreal.Vector(pos.x + lx * c - ly * s_, pos.y + lx * s_ + ly * c, z - 60.0)
                    mesh_actor(sac, loc, "Eisack {}-{}".format(ribs + 1, int(gy)), "Stufe I/Eisaecke", yaw=yaw + rng.uniform(-20, 20),
                               pitch=rng.uniform(-10, 10), scale=rng.uniform(0.8, 1.1) * scale)
                    point_light(unreal.Vector(loc.x + c * 70.0, loc.y + s_ * 70.0, loc.z + 80.0), "Eisack-Licht", "Licht/Eisaecke",
                                (1.0, 0.45, 0.12), 30.0, 420.0)
                ribs += 1
            a += spacing / max(room.rx, room.ry)
    log("{} Rippenwaende".format(ribs))

    # 1 Arrival: root knot on a ledge, root curtain from the crack above it, root ramp down into the hall
    knot = at(560, 175)
    mesh_actor(cube, unreal.Vector(knot.x, knot.y, 140.0), "Wurzelknoten Sockel", "Stufe I/Ankunft", scale=(7.5, 7.5, 3.0),
               material=m_tissue, collision=True)
    mesh_actor(roots, unreal.Vector(knot.x, knot.y, 280.0), "Wurzelriss (Ankunft)", "Stufe I/Ankunft", yaw=30.0, scale=1.15)
    point_light(unreal.Vector(knot.x, knot.y, 420.0), "Wurzelknoten-Licht", "Licht", (1.0, 0.85, 0.5), 70.0, 800.0)
    heights = [290.0, 230.0, 175.0, 120.0, 60.0, 0.0]
    for k, ((ax, ay), (bx, by)) in enumerate(zip(RAMP, RAMP[1:])):
        a, b = at(ax, ay, heights[k]), at(bx, by, heights[k + 1])
        dx, dy, dz = b.x - a.x, b.y - a.y, b.z - a.z
        run = math.hypot(dx, dy)
        mid = unreal.Vector((a.x + b.x) / 2, (a.y + b.y) / 2, (a.z + b.z) / 2 - 20.0)
        mesh_actor(cube, mid, "Wurzelrampe {}".format(k + 1), "Stufe I/Ankunft", yaw=math.degrees(math.atan2(dy, dx)),
                   pitch=-math.degrees(math.atan2(dz, run)) * -1.0, scale=((math.hypot(run, dz) + 60.0) / 100.0, 3.0, 0.4),
                   material=m_roots, collision=True)
    spawn(unreal.PlayerStart, unreal.Vector(knot.x - 150.0, knot.y, 420.0), "Spielerstart (Ankunft)", "Spiel",
          unreal.Rotator(0.0, 0.0, 225.0))
    label_text(knot, "1 Ankunft", "Stufe I/Ankunft")

    # 2 Root table with the stations of the Hexenkueche under the main shaft
    table = at(520, 560)
    # the front of the table (book, mortar) faces the camera in the south-west, the herb arch stands at its back
    facing = 225.0
    mesh_actor(load("SM_GK_Wurzeltisch"), unreal.Vector(table.x, table.y, 0.0), "Wurzeltisch", "Stufe I/Hexenkueche", yaw=facing, collision=True)
    mesh_actor(load("SM_GK_Tischschmuck"), unreal.Vector(table.x, table.y, 0.0), "Tischschmuck", "Stufe I/Hexenkueche", yaw=facing)
    mesh_actor(load("SM_GK_Kraeuterbogen"), unreal.Vector(table.x, table.y, 0.0), "Kraeuterbogen", "Stufe I/Hexenkueche", yaw=facing, collision=True)
    # two stools at the front corners of the table, a little turned, so someone can sit and read the book
    for k, (side, turn) in enumerate(((-1.0, 25.0), (1.0, -35.0))):
        local = unreal.Vector(70.0, side * 118.0, 0.0)
        c, s_ = math.cos(math.radians(facing)), math.sin(math.radians(facing))
        p = unreal.Vector(table.x + local.x * c - local.y * s_, table.y + local.x * s_ + local.y * c, 0.0)
        mesh_actor(load("SM_GK_Hocker"), p, "Hocker {}".format(k + 1), "Stufe I/Hexenkueche", yaw=facing + turn + rng.uniform(-10, 10),
                   scale=rng.uniform(0.95, 1.08), collision=True)
    spawn(unreal.VaelBrewTable, unreal.Vector(table.x, table.y, 0.0), "Brautisch (Hexenkueche)", "Spiel", unreal.Rotator(0.0, 0.0, facing))
    point_light(unreal.Vector(table.x, table.y, 260.0), "Tischlicht", "Stufe I/Hexenkueche", (1.0, 0.75, 0.45), 6.0, 480.0)
    shaft(table, "Hauptschacht", "Licht/Lichtschaechte", 520.0, 10.0)
    label_text(table, "2 Wurzeltisch", "Stufe I/Hexenkueche")

    # 3 Beds: 40 places in the end (BED_PLAN), the hall shared out evenly over the four plots, inner ones first.
    #   Stage I: planted beds. Later stages: sleeping clutches that wake when the story reaches their stage.
    #   Every place is a VaelRefugiumClutch: it wakes with the story flag of its stage and hatches at dawn (C++).
    for k, (stage, folder_room, p) in enumerate(bed_slots()):
        folder = "Stufe {}/Beete {}".format(stage, folder_room)
        number = STAGES.index(stage) + 1
        clutch = spawn(unreal.VaelRefugiumClutch, unreal.Vector(p.x, p.y, -6.0 if number == 1 else -4.0),
                       "Beetplatz {:02d} (Stufe {})".format(k + 1, stage), folder, unreal.Rotator(0.0, 0.0, rng.uniform(0, 360)))
        clutch.set_editor_property("refugium", "Glutkammer")
        clutch.set_editor_property("stage", number)
        clutch.set_editor_property("place_id", "{}{:02d}".format(folder_room, k + 1))
        if number == 1:
            # empty to start with: the players sow their own seeds (VaelRefugiumClutch::SowFrom)
            shaft(p, "Schacht Beet {}".format(k + 1), "Licht/Lichtschaechte", 650.0, 4.5, dust=False)
    for stage in STAGES:
        hall, chamber, gallery = BED_PLAN[stage]
        label_text(first_slot[stage], "Beete Stufe {} (+{})".format(stage, hall + chamber + gallery), "Stufe {}/Schilder".format(stage),
                   color=(1.0, 0.75, 0.4))
    label_text(at(395, 470), "3 Beete", "Stufe I/Beete Halle")

    # 4 Bed overview at the foot of the ramp
    mesh_actor(cube, at(717, 400, 70.0), "Wurzeltafel", "Stufe I/Hexenkueche", yaw=225.0, scale=(0.15, 1.4, 1.4), material=m_roots, collision=True)
    label_text(at(717, 400), "4 Wurzeltafel", "Stufe I/Hexenkueche")
    # the board of Die Waage: its page, Albrun's first explanation, the guided first bed (C++ VaelBalanceBoard)
    spawn(unreal.VaelBalanceBoard, at(717, 400, 0.0), "Wurzeltafel (Waage)", "Spiel", unreal.Rotator(0.0, 0.0, 225.0))
    # a place to rest by the Glutbecken: 8 hours, heals and purifies with a Klartrunk or Lichttropfen
    spawn(unreal.VaelRestSpot, at(470, 815, 0.0), "Rastplatz am Glutbecken", "Spiel")

    # 5 Glutbecken: cooled in the light, lava in the shadow, the Brunnen in the twilight
    p = at(520, 760)
    mesh_actor(pool, p, "Glutbecken", "Stufe I/Glutbecken", yaw=90.0, scale=(1.1, 0.7, 1.0))
    point_light(unreal.Vector(p.x, p.y, 110.0), "Becken-Licht", "Licht", (1.0, 0.4, 0.1), 140.0, 1000.0, shadows=True)
    label_text(p, "5 Glutbecken", "Stufe I/Glutbecken")
    motes(unreal.Vector(p.x, p.y, 0.0), "Funken Glutbecken", "Licht/Funken", unreal.VaelCaveMote.EMBER, 45, 220.0, 520.0)

    # 6 Plots: four quarters of the hall, the Narbensaum between them (thin dark lines on the floor for now)
    for k, (ax, ay, bx, by) in enumerate(((520, 330, 520, 480), (520, 640, 520, 820), (300, 560, 440, 560), (600, 560, 800, 560))):
        a, b = at(ax, ay, 1.0), at(bx, by, 1.0)
        mesh_actor(cube, unreal.Vector((a.x + b.x) / 2, (a.y + b.y) / 2, 1.0), "Narbensaum {}".format(k + 1), "Stufe I/Parzellen",
                   yaw=math.degrees(math.atan2(b.y - a.y, b.x - a.x)), scale=(math.hypot(b.x - a.x, b.y - a.y) / 100.0, 0.12, 0.02), material=m_tissue)
    label_text(at(600, 650), "6 Parzellen", "Stufe I/Parzellen", color=(0.8, 0.6, 0.9))

    # 7-9 Corners of the classes and the helper nest (stage II)
    corners = [
        ("7 Prophet: Schrein", (226, 350), "SM_GK_Schrein"),
        ("8 Helfernest", (355, 370), "SM_GK_Helfernest"),
        ("9 Tamer: Bau", (855, 420), "SM_GK_Bau"),
        ("9 Ingenieur: Werkbank", (745, 600), "SM_GK_Werkbank"),
        ("9 Magier: Glutbrunnen", (865, 690), "SM_GK_Glutbrunnen"),
    ]
    for name, (px, py), piece in corners:
        p = at(px, py)
        # each corner turns its front (+X) to the root table
        yaw = math.degrees(math.atan2(table.y - p.y, table.x - p.x))
        mesh_actor(load(piece), unreal.Vector(p.x, p.y, -2.0), name, "Stufe II/Klassenecken", yaw=yaw, collision=True)
        label_text(p, name, "Stufe II/Klassenecken", color=(0.6, 0.95, 0.55))
    for k, (px, py) in enumerate(((226, 350), (745, 600))):
        point_light(at(px, py, 180.0), "Eckenlicht {}".format(k + 1), "Stufe II/Klassenecken", (1.0, 0.7, 0.4), 30.0, 450.0)
    for k in range(3):
        point_light(at(355 + rng.uniform(-30, 30), 370 + rng.uniform(-30, 30), 140.0), "Fee {}".format(k + 1), "Stufe II/Klassenecken",
                    (0.85, 1.0, 0.6), 25.0, 260.0)
    point_light(at(865, 690, 120.0), "Glutbrunnen-Licht", "Stufe II/Klassenecken", (1.0, 0.45, 0.12), 90.0, 600.0)
    motes(at(865, 690), "Funken Glutbrunnen", "Licht/Funken", unreal.VaelCaveMote.EMBER, 20, 90.0, 400.0)

    # Stage II: more clutch beds in the egg chamber
    shaft(at(230, 520), "Schacht Eierkammer", "Licht/Lichtschaechte", 900.0, 9.0)
    label_text(at(232, 700), "Eierkammer (Stufe II)", "Stufe II/Eierkammer", color=(0.6, 0.95, 0.55))

    # 10 Wurzeltor to the other refugia (stage IV): a ring of root in the west wall
    gate = at(112, 560)
    mesh_actor(cylinder, unreal.Vector(gate.x, gate.y, 170.0), "Wurzeltor", "Stufe IV/Wurzeltor", pitch=90.0, scale=(3.0, 1.8, 0.25), material=mi_membrane)
    point_light(unreal.Vector(gate.x, gate.y + 120.0, 220.0), "Wurzeltor-Licht", "Stufe IV/Wurzeltor", (0.6, 0.8, 1.0), 60.0, 500.0)
    label_text(gate, "10 Wurzeltor (Stufe IV)", "Stufe IV/Wurzeltor", color=(0.55, 0.75, 1.0))

    # 11 Glutgalerie behind rubble (stage IV): rubble wall now, glow veins in its floor for fire plants
    for k in range(13):
        t = k / 12
        p = at(340 + 490 * t, 832 + 12 * math.sin(t * 6.0))
        rock = rocks[(k + 3) % len(rocks)]
        size = rock.get_bounding_box().max - rock.get_bounding_box().min
        s = min(260.0 / max(size.z, 1.0), 300.0 / max(size.x, size.y, 1.0))
        mesh_actor(rock, unreal.Vector(p.x, p.y, -30.0), "Schutt {}".format(k + 1), "Stufe IV/Schutt", yaw=rng.uniform(0, 360), scale=s, collision=True,
                   material=m_rock)
    for k, (px, py, yaw) in enumerate(((420, 930, 10), (560, 905, -15), (700, 940, 20), (480, 990, 5), (650, 1000, -10))):
        mesh_actor(pool, at(px, py, -2.0), "Glutader {}".format(k + 1), "Stufe IV/Glutgalerie", yaw=yaw, scale=(0.18, 1.5, 0.6))
        point_light(at(px, py, 60.0), "Glutader-Licht {}".format(k + 1), "Stufe IV/Glutgalerie", (1.0, 0.35, 0.08), 60.0, 500.0)
        motes(at(px, py), "Funken Glutader {}".format(k + 1), "Licht/Funken", unreal.VaelCaveMote.EMBER, 14, 120.0, 380.0)
    label_text(at(580, 960), "11 Glutgalerie (hinter Schutt)", "Stufe IV/Glutgalerie", color=(0.55, 0.75, 1.0))

    # 12 Brood tunnels: points where the pests come in
    for k, tunnel in enumerate(TUNNELS):
        end = at(*tunnel[-1], z=60.0)
        spawn(unreal.TargetPoint, end, "Befall {}".format(k + 1), "Spiel/Befall")
        label_text(at(*tunnel[1]), "12 Brutgang", "Spiel/Befall", color=(1.0, 0.45, 0.4))

    # 13 Stage V: where the ceiling breaks open (only a marker for now)
    spawn(unreal.TargetPoint, at(520, 560, 1500.0), "Deckendurchbruch (Stufe V)", "Stufe V")

    # The ceiling full of holes (Designbibel): many small light spots on the floor of the hall, no shadows, cheap
    spots = 0
    for ring, count, size in ((70, 6, 4.5), (130, 10, 3.8), (195, 14, 3.2)):
        for k in range(count):
            a = 2 * math.pi * (k + 0.5 * (ring // 150 % 2)) / count + rng.uniform(-0.12, 0.12)
            p = unreal.Vector(HALL.c.x + math.cos(a) * ring * PX + rng.uniform(-60, 60), HALL.c.y + math.sin(a) * ring * PX + rng.uniform(-60, 60), 0.0)
            if not HALL.inside(p, margin=-150.0):
                continue
            light = shaft(p, "Deckenloch {}".format(spots + 1), "Licht/Deckenloecher", rng.uniform(260.0, 520.0), size + rng.uniform(-0.8, 0.8),
                          dust=spots % 3 == 0)
            light.light_component.set_editor_property("cast_shadows", False)
            spots += 1
    log("{} Deckenloecher".format(spots))

    # Things along the walls, the middle stays free for fights: small rocks, loose sacs, empty clutches, root strands
    props = 0
    for room in ROOMS[:5]:
        for k in range(int(10 + (room.rx + room.ry) / 120.0)):
            a = rng.uniform(0, 2 * math.pi)
            f = rng.uniform(0.74, 0.93)
            p = unreal.Vector(room.c.x + math.cos(a) * room.ry * f, room.c.y + math.sin(a) * room.rx * f, 0.0)
            if any(c.inside(p, margin=80.0) for c in CORRIDORS) or any(
                    math.hypot(p.x - at(x, y).x, p.y - at(x, y).y) < r * PX for x, y, r in KEEP_FREE) or any(
                    math.hypot(p.x - b.x, p.y - b.y) < 260.0 for _, _, b in bed_slots()):
                continue
            kind = rng.random()
            folder = "Stufe IV/Glutgalerie" if room is ROOMS[4] else "Stufe I/Kleinkram"
            if kind < 0.5:
                rock = rocks[k % len(rocks)]
                size = rock.get_bounding_box().max - rock.get_bounding_box().min
                s_ = rng.uniform(40.0, 110.0) / max(size.z, 1.0)
                mesh_actor(rock, unreal.Vector(p.x, p.y, -15.0), "Stein {}".format(props + 1), folder, yaw=rng.uniform(0, 360), scale=s_, collision=True,
                           material=m_rock)
            elif kind < 0.72:
                mesh_actor(sac, unreal.Vector(p.x, p.y, -20.0), "Eisack lose {}".format(props + 1), folder, yaw=rng.uniform(0, 360),
                           pitch=rng.uniform(55, 85), scale=rng.uniform(0.55, 0.8))
            elif kind < 0.88:
                mesh_actor(bed, unreal.Vector(p.x, p.y, 5.0), "Gelege leer {}".format(props + 1), folder, yaw=rng.uniform(0, 360),
                           pitch=rng.uniform(50, 85), scale=rng.uniform(0.55, 0.75))
            else:
                mesh_actor(roots, unreal.Vector(p.x, p.y, -620.0), "Wurzelstrang {}".format(props + 1), folder, yaw=rng.uniform(0, 360),
                           scale=rng.uniform(0.7, 0.9))
            props += 1
    log("{} Kleinteile".format(props))

    # Light and air of the cave
    point_light(at(520, 560, 1100.0), "Fuelllicht", "Licht", (1.0, 0.7, 0.5), 120.0, 5200.0)
    fog = spawn(unreal.ExponentialHeightFog, at(520, 560), "Hoehlennebel", "Licht")
    f = fog.component
    f.set_editor_property("fog_density", 0.035)
    f.set_editor_property("fog_height_falloff", 0.25)
    f.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.06, 0.035, 0.025, 1.0))
    f.set_volumetric_fog(True)
    for key, value in (("volumetric_fog_scattering_distribution", 0.65), ("volumetric_fog_extinction_scale", 1.4)):
        try:
            f.set_editor_property(key, value)
        except Exception as error:
            notes.append("Nebel {}: {}".format(key, error))
    post = spawn(unreal.PostProcessVolume, at(520, 560), "Nachbearbeitung", "Licht")
    post.set_editor_property("unbound", True)
    settings = post.get_editor_property("settings")
    for key, value in (("auto_exposure_min_brightness", 0.0), ("auto_exposure_max_brightness", 0.0), ("auto_exposure_bias", 0.3),
                       ("bloom_intensity", 1.1), ("vignette_intensity", 0.5)):
        settings.set_editor_property("override_" + key, True)
        settings.set_editor_property(key, value)
    post.set_editor_property("settings", settings)

    if unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH):
        log("gespeichert: " + LEVEL_PATH)
    else:
        unreal.log_error("[Glutkammer] konnte nicht gespeichert werden")
    for note in notes:
        log("Hinweis: " + note)
    log("fertig")


build()
