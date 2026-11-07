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
PLANTS = ["/Game/Vael/Requisiten/Aschenmark/{0}/SM_{0}".format(n) for n in ("Glutdistel", "Glimmklee", "Russrose", "Funkenmohn")]
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


def shaft(location, label, folder, intensity, cone):
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
    return light


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
# sleeping clutches in the hall (profile pixels, stage that wakes them): 4 beds at the start, 11 after stage IV
SLEEPING = [(330, 541, "II"), (712, 551, "II"), (599, 391, "III"), (459, 741, "III"), (440, 375, "III"),
            (360, 700, "IV"), (690, 735, "IV")]
# places the scattered things keep away from (profile pixels and radius): table, pool, beds, corners, gate
KEEP_FREE = [(520, 560, 120), (520, 760, 95), (395, 470, 45), (651, 480, 45), (391, 655, 45), (649, 661, 45), (226, 350, 45),
             (355, 370, 45), (855, 420, 70), (745, 600, 50), (865, 690, 50), (717, 400, 40), (112, 560, 60)]
KEEP_FREE += [(x, y, 40) for x, y, _ in SLEEPING]
for tunnel in TUNNELS:
    CORRIDORS += [Room("Gang", x, y, 16, 16) for x, y in path_points(tunnel)]
OPEN = ROOMS + CORRIDORS


def open_at(p, margin=0.0, skip=None):
    return any(room.inside(p, margin) for room in OPEN if room is not skip)


def farness(direction_x, direction_y):
    """1 for walls on the far side of the camera (north-east), -1 for walls next to it"""
    length = math.hypot(direction_x, direction_y) or 1.0
    return (direction_x + direction_y) / (length * math.sqrt(2.0))


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
    m_floor, m_tissue, m_roots, m_pool = (load(n) for n in ("M_GK_Boden", "M_GK_Gewebe", "M_GK_Wurzel", "M_GK_Becken"))
    mi_membrane, mi_eggs = load("MI_GK_Membran"), load("MI_GK_Eier")
    rocks = [r for r in (unreal.load_asset(p) for p in ROCKS) if r is not None]
    plants = [unreal.load_asset(p) for p in PLANTS]

    spawn(unreal.VaelIndoorLevel, at(520, 560, 400.0), "Innenraum", "Spiel")

    # Floor: one slab under everything, its top at 0
    mesh_actor(cube, at(520, 600, -50.0), "Boden", "Raum", scale=(95.0, 95.0, 1.0), material=m_floor, collision=True)

    # Rock along the edge of every room, high in the north-east, low in the south-west
    count = 0
    for room in ROOMS + CORRIDORS:
        circumference = 2 * math.pi * math.sqrt((room.rx ** 2 + room.ry ** 2) / 2)
        steps = max(8, int(circumference / 330.0))
        for k in range(steps):
            a = 2 * math.pi * k / steps + rng.uniform(-0.05, 0.05)
            edge = room.edge(a)
            out_x, out_y = math.cos(a) / max(room.ry, 1.0), math.sin(a) / max(room.rx, 1.0)
            norm = math.hypot(out_x, out_y)
            out_x, out_y = out_x / norm, out_y / norm
            place = unreal.Vector(edge.x + out_x * 170.0, edge.y + out_y * 170.0, -40.0)
            if open_at(place, margin=60.0):
                continue
            far = farness(out_x, out_y)
            height = 160.0 + 620.0 * max(0.0, far) ** 1.3 + (90.0 if far > -0.3 else 0.0) + rng.uniform(-40, 50)
            if room in CORRIDORS:
                height = min(height, 380.0)
            rock = rocks[count % len(rocks)]
            box = rock.get_bounding_box()
            size = box.max - box.min
            width = max(size.x, size.y, 1.0)
            s = min(height / max(size.z, 1.0), 520.0 / width)
            mesh_actor(rock, unreal.Vector(place.x + out_x * width * s * 0.3, place.y + out_y * width * s * 0.3, place.z),
                       "Fels {}".format(count + 1), "Raum/Fels", yaw=rng.uniform(0, 360), scale=s, collision=True)
            count += 1
    log("{} Felsen".format(count))

    # Rib walls with membranes and egg sacs on the far half of the hall and in the den
    ribs = 0
    for room, start, end, spacing, scale in ((HALL, -20.0, 110.0, 560.0, 1.0), (ROOMS[2], -10.0, 100.0, 520.0, 0.75)):
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
    mesh_actor(cylinder, unreal.Vector(table.x, table.y, 45.0), "Wurzeltisch", "Stufe I/Hexenkueche", scale=(3.6, 2.6, 0.9),
               material=m_roots, collision=True)
    for k, (name, dx, dy) in enumerate((("Kessel", 0, -260), ("Moerser", 230, -120), ("Destille", 230, 140), ("Trockengestell", -230, 140),
                                        ("Harzpresse", -230, -120), ("Staubmuehle", 0, 270))):
        mesh_actor(cube if k % 2 else cylinder, unreal.Vector(table.x + dx, table.y + dy, 40.0), "Station " + name, "Stufe I/Hexenkueche",
                   scale=(0.8, 0.8, 0.8), material=m_tissue, collision=True)
    shaft(table, "Hauptschacht", "Licht/Lichtschaechte", 2200.0, 10.0)
    label_text(table, "2 Wurzeltisch", "Stufe I/Hexenkueche")

    # 3 Clutch beds (stage I) with plants, empty clutches that become beds later
    for k, (px, py) in enumerate(((395, 470), (651, 480), (391, 655), (649, 661))):
        p = at(px, py, -6.0)
        mesh_actor(bed, p, "Gelegebeet {}".format(k + 1), "Stufe I/Beete", yaw=rng.uniform(0, 360), collision=True)
        if plants[k] is not None:
            mesh_actor(plants[k], unreal.Vector(p.x, p.y, p.z + 30.0), "Pflanze {}".format(k + 1), "Stufe I/Beete", yaw=rng.uniform(0, 360), scale=0.9)
        shaft(p, "Schacht Beet {}".format(k + 1), "Licht/Lichtschaechte", 700.0, 5.0)
    label_text(at(395, 470), "3 Beete", "Stufe I/Beete")

    # Sleeping clutches: closed sacs on the floor of the hall that hatch into beds over the story (see SLEEPING)
    for k, (px, py, stage) in enumerate(SLEEPING):
        p = at(px, py, -10.0)
        folder = "Stufe {}/Beete Halle".format(stage)
        mesh_actor(sac, p, "Schlafendes Gelege {} ({})".format(k + 1, stage), folder, yaw=rng.uniform(0, 360),
                   pitch=rng.uniform(70, 88), scale=rng.uniform(0.75, 0.9), collision=True)
        point_light(unreal.Vector(p.x, p.y, 70.0), "Gelege-Glimmen {}".format(k + 1), folder, (1.0, 0.4, 0.1), 12.0, 220.0)
        label_text(p, "schlaeft (Stufe {})".format(stage), folder, color=(1.0, 0.7, 0.4))

    # 4 Bed overview at the foot of the ramp
    mesh_actor(cube, at(717, 400, 70.0), "Wurzeltafel", "Stufe I/Hexenkueche", yaw=225.0, scale=(0.15, 1.4, 1.4), material=m_roots, collision=True)
    label_text(at(717, 400), "4 Wurzeltafel", "Stufe I/Hexenkueche")

    # 5 Glutbecken: cooled in the light, lava in the shadow, the Brunnen in the twilight
    p = at(520, 760)
    mesh_actor(pool, p, "Glutbecken", "Stufe I/Glutbecken", yaw=90.0, scale=(1.1, 0.7, 1.0))
    point_light(unreal.Vector(p.x, p.y, 110.0), "Becken-Licht", "Licht", (1.0, 0.4, 0.1), 140.0, 1000.0, shadows=True)
    label_text(p, "5 Glutbecken", "Stufe I/Glutbecken")

    # 6 Plots: four quarters of the hall, the Narbensaum between them (thin dark lines on the floor for now)
    for k, (ax, ay, bx, by) in enumerate(((520, 330, 520, 480), (520, 640, 520, 820), (300, 560, 440, 560), (600, 560, 800, 560))):
        a, b = at(ax, ay, 1.0), at(bx, by, 1.0)
        mesh_actor(cube, unreal.Vector((a.x + b.x) / 2, (a.y + b.y) / 2, 1.0), "Narbensaum {}".format(k + 1), "Stufe I/Parzellen",
                   yaw=math.degrees(math.atan2(b.y - a.y, b.x - a.x)), scale=(math.hypot(b.x - a.x, b.y - a.y) / 100.0, 0.12, 0.02), material=m_tissue)
    label_text(at(600, 650), "6 Parzellen", "Stufe I/Parzellen", color=(0.8, 0.6, 0.9))

    # 7-9 Corners of the classes and the helper nest (stage II)
    corners = [
        ("7 Prophet: Schrein", (226, 350), cone, (1.2, 1.2, 2.2)),
        ("8 Helfernest", (355, 370), sphere, (1.4, 1.4, 0.7)),
        ("9 Tamer: Bau", (855, 420), sphere, (3.0, 3.0, 1.4)),
        ("9 Ingenieur: Werkbank", (745, 600), cube, (2.2, 1.0, 0.9)),
        ("9 Magier: Glutbrunnen", (865, 690), cylinder, (1.6, 1.6, 0.5)),
    ]
    for name, (px, py), shape, size in corners:
        p = at(px, py)
        mesh_actor(shape, unreal.Vector(p.x, p.y, size[2] * 50.0), name, "Stufe II/Klassenecken", scale=size,
                   material=m_pool if "Glutbrunnen" in name else m_tissue, collision=True)
        label_text(p, name, "Stufe II/Klassenecken", color=(0.6, 0.95, 0.55))
    for k in range(3):
        point_light(at(355 + rng.uniform(-30, 30), 370 + rng.uniform(-30, 30), 140.0), "Fee {}".format(k + 1), "Stufe II/Klassenecken",
                    (0.85, 1.0, 0.6), 25.0, 260.0)
    point_light(at(865, 690, 120.0), "Glutbrunnen-Licht", "Stufe II/Klassenecken", (1.0, 0.45, 0.12), 90.0, 600.0)

    # Stage II: more clutch beds in the egg chamber
    for k, (px, py) in enumerate(((180, 442), (250, 422), (170, 542), (270, 542), (190, 632), (275, 642))):
        mesh_actor(bed, at(px, py, -6.0), "Gelegebeet Eierkammer {}".format(k + 1), "Stufe II/Eierkammer", yaw=rng.uniform(0, 360), collision=True)
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
        mesh_actor(rock, unreal.Vector(p.x, p.y, -30.0), "Schutt {}".format(k + 1), "Stufe IV/Schutt", yaw=rng.uniform(0, 360), scale=s, collision=True)
    for k, (px, py, yaw) in enumerate(((420, 930, 10), (560, 905, -15), (700, 940, 20), (480, 990, 5), (650, 1000, -10))):
        mesh_actor(pool, at(px, py, -2.0), "Glutader {}".format(k + 1), "Stufe IV/Glutgalerie", yaw=yaw, scale=(0.18, 1.5, 0.6))
        point_light(at(px, py, 60.0), "Glutader-Licht {}".format(k + 1), "Stufe IV/Glutgalerie", (1.0, 0.35, 0.08), 60.0, 500.0)
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
            light = shaft(p, "Deckenloch {}".format(spots + 1), "Licht/Deckenloecher", rng.uniform(260.0, 520.0), size + rng.uniform(-0.8, 0.8))
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
                    math.hypot(p.x - at(x, y).x, p.y - at(x, y).y) < r * PX for x, y, r in KEEP_FREE):
                continue
            kind = rng.random()
            folder = "Stufe IV/Glutgalerie" if room is ROOMS[4] else "Stufe I/Kleinkram"
            if kind < 0.5:
                rock = rocks[k % len(rocks)]
                size = rock.get_bounding_box().max - rock.get_bounding_box().min
                s_ = rng.uniform(40.0, 110.0) / max(size.z, 1.0)
                mesh_actor(rock, unreal.Vector(p.x, p.y, -15.0), "Stein {}".format(props + 1), folder, yaw=rng.uniform(0, 360), scale=s_, collision=True)
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
                       ("bloom_intensity", 0.9), ("vignette_intensity", 0.45)):
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
