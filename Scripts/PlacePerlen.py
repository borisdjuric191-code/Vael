# Setzt die wilden Saftperlen und Eiterperlen (DA_Harvest_Saftperle / DA_Harvest_Eiterperle) in L_Wurzelforst_Blockout.
#
# - Saftperle, die reine Form: kleine Gruppen am Forstsaum und im Ewigen Hain, auf freiem Waldboden.
# - Eiterperle, die gezeichnete Form: im Pfuhlmoor am trockenen Ufer, dichter rund um die Nid-Quelle und auf den Nid-Flecken.
# - Nie im Wasser, auf Fels, in Bäumen oder auf Wegen; jede Pflanze wird einzeln auf die Landschaft gesetzt.
# - Größe, Neigung und Drehung wählt AVaelHarvestable selbst je Ort; Wanken und Tropfen kommen aus den Daten.
# Alles bekommt den Tag WF_Perlen (Ordner Wurzelforst/Perlen/<Art>); ein neuer Lauf ersetzt nur das.
#
# Mit geschlossenem Editor, nach CreateMaterialAssets.py, CreateHarvestableAssets.py und CreateCompendiumAssets.py:
#   UnrealEditor-Cmd.exe Vael.uproject -run=pythonscript -script="C:/Projekte/Vael/Scripts/PlacePerlen.py" -EnablePlugins=PythonScriptPlugin

import json
import math
import os
import random
import struct
import zlib

import unreal

LEVEL_PATH = "/Game/Vael/Maps/L_Wurzelforst_Blockout"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KIT = os.path.join(ROOT, "Blockout", "Vael_Wurzelforst_Blockout")
TAG = "WF_Perlen"
FOLDER = "Wurzelforst/Perlen"
DRY_Z = 60.0             # tiefster Boden in cm (Meer und Moorwasser stehen bei 0-6 cm)
PIXEL = 200.0            # 1 Pixel der Zonenkarte = 2 m (wie in ScatterWurzelforst.py)
ZONE_COLORS = {          # Zonenfarben von T_WF_Zonenkarte.png (wie in ScatterWurzelforst.py)
    "saum": "6e6448", "moor": "3e6b55", "hain": "3f7f45", "kamm": "3b6670", "grat": "6a6d60", "asche": "7a4a2a",
    "jaw": "4a5f78", "grat_hoch": "9a9c90", "lagune": "2f6f74", "nid": "5a3a7a", "weg": "b9a77a",
}
# Art, Zonen, Gruppen, Pflanzen je Gruppe, Abstand der Gruppen (cm)
PLAN = [
    ("Saftperle", ("saum", "hain"), 14, (2, 4), 3500.0),
    ("Eiterperle", ("moor", "nid"), 10, (2, 3), 3000.0),
]
NID_QUELLE = (-83200.0, -26400.0)   # WF_Karte.json, Beschriftung "Nid-Quelle"
NID_GRUPPEN = 3                     # Eiterperlen-Gruppen 8 bis 25 m um die Quelle

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(1311)
WORLD = [None]


def log(message):
    unreal.log_warning("[PlacePerlen] " + message)


def trace(start, end):
    hit = unreal.SystemLibrary.line_trace_single(WORLD[0], start, end,
                                                 unreal.TraceTypeQuery.ECC_VISIBILITY, True, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    values = hit.to_tuple()
    if not values[0]:
        return None
    return values[4], values[7], values


def on_land(hit):
    """Nur die Landschaft zählt (Wasserflächen haben keine Kollision, die Höhe hält die Pflanzen aus dem Wasser)"""
    values = hit[2]
    actor = values[9] if len(values) > 9 else None
    return isinstance(actor, unreal.LandscapeProxy) and hit[0].z > DRY_Z and hit[1].z >= 0.85


class ZoneMap:
    """Zones of the forest from T_WF_Zonenkarte.png, looked up in world cm (reader as in ScatterWurzelforst.py)"""

    def __init__(self, origin):
        self.origin = origin
        data = open(os.path.join(KIT, "T_WF_Zonenkarte.png"), "rb").read()
        pos, chunks = 8, []
        while pos < len(data):
            length, kind = struct.unpack(">I4s", data[pos:pos + 8])
            body = data[pos + 8:pos + 8 + length]
            if kind == b"IHDR":
                self.width, self.height, depth, color = struct.unpack(">IIBB", body[:10])
            elif kind == b"IDAT":
                chunks.append(body)
            elif kind == b"IEND":
                break
            pos += 12 + length
        self.channels = {0: 1, 2: 3, 4: 2, 6: 4}[color]
        bpp = max(1, self.channels * depth // 8)
        stride = self.width * self.channels * depth // 8
        raw = zlib.decompress(b"".join(chunks))
        self.rows, previous = [], bytearray(stride)
        for y in range(self.height):
            start = y * (stride + 1)
            kind = raw[start]
            line = bytearray(raw[start + 1:start + 1 + stride])
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                up = previous[i]
                corner = previous[i - bpp] if i >= bpp else 0
                if kind == 1:
                    line[i] = (line[i] + left) & 255
                elif kind == 2:
                    line[i] = (line[i] + up) & 255
                elif kind == 3:
                    line[i] = (line[i] + ((left + up) >> 1)) & 255
                elif kind == 4:
                    guess = left + up - corner
                    pa, pb, pc = abs(guess - left), abs(guess - up), abs(guess - corner)
                    line[i] = (line[i] + (left if pa <= pb and pa <= pc else up if pb <= pc else corner)) & 255
            self.rows.append(bytes(line))
            previous = line
        self.colours = {name: [int(c[i:i + 2], 16) for i in (0, 2, 4)] for name, c in ZONE_COLORS.items()}

    def zone(self, x, y):
        j = min(max(int(round((x - self.origin[0]) / PIXEL)), 0), self.width - 1)
        i = min(max(int(round((y - self.origin[1]) / PIXEL)), 0), self.height - 1)
        rgb = [self.rows[i][j * self.channels + k] for k in range(3)]
        best, best_error = None, None
        for name, colour in self.colours.items():
            # The map is hill-shaded, so each colour is compared at its best brightness
            scale = min(max(sum(a * b for a, b in zip(rgb, colour)) / max(1, sum(b * b for b in colour)), 0.6), 1.3)
            error = sum((a - b * scale) ** 2 for a, b in zip(rgb, colour))
            if best_error is None or error < best_error:
                best, best_error = name, error
        return best


def frei_vom_weg(zones, x, y):
    """Kein Weg an der Stelle und 4 m drum herum"""
    return all(zones.zone(x + dx, y + dy) != "weg" for dx, dy in ((0, 0), (400, 0), (-400, 0), (0, 400), (0, -400)))


def boden(x, y, top, bottom):
    hit = trace(unreal.Vector(x, y, top), unreal.Vector(x, y, bottom))
    return hit[0].z if hit is not None and on_land(hit) else None


def gruppe(zones, art, zonen, x, y, anzahl, top, bottom):
    """Erste Pflanze am Punkt, die übrigen 1,6 bis 3,5 m drum herum"""
    pflanzen = []
    versuche = 0
    while len(pflanzen) < anzahl and versuche < 60:
        versuche += 1
        if pflanzen:
            a, d = rng.uniform(0, 2 * math.pi), rng.uniform(160.0, 350.0)
            px, py = x + math.cos(a) * d, y + math.sin(a) * d
        else:
            px, py = x, y
        if zones.zone(px, py) not in zonen or not frei_vom_weg(zones, px, py):
            if not pflanzen:
                return []
            continue
        if any((px - qx) ** 2 + (py - qy) ** 2 < 140.0 ** 2 for qx, qy, _ in pflanzen):
            continue
        z = boden(px, py, top, bottom)
        if z is None:
            if not pflanzen:
                return []
            continue
        pflanzen.append((px, py, z))
    return pflanzen


def main():
    world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    WORLD[0] = world
    level_actors = actors.get_all_level_actors()
    removed = 0
    for actor in level_actors:
        if unreal.Name(TAG) in actor.tags:
            actors.destroy_actor(actor)
            removed += 1

    landscape = next((a for a in level_actors if isinstance(a, unreal.LandscapeProxy)), None)
    if landscape is None:
        log("keine Landschaft im Level, nichts gesetzt")
        return
    centre, extent = landscape.get_actor_bounds(False)
    corner = landscape.get_actor_location()
    zones = ZoneMap((corner.x, corner.y))
    top, bottom = centre.z + extent.z + 5000, centre.z - extent.z - 5000

    gruppen = []          # (art, x, y)
    pflanzen = {}
    for art, zonen, ziel, stueck, abstand in PLAN:
        data = unreal.load_asset("/Game/Vael/Nature/DA_Harvest_" + art)
        if data is None:
            log("fehlt: DA_Harvest_{} (zuerst CreateHarvestableAssets.py)".format(art))
            continue
        orte = []
        if art == "Eiterperle":
            for _ in range(NID_GRUPPEN * 40):
                if len(orte) >= NID_GRUPPEN:
                    break
                a, d = rng.uniform(0, 2 * math.pi), rng.uniform(800.0, 2500.0)
                orte.append((NID_QUELLE[0] + math.cos(a) * d, NID_QUELLE[1] + math.sin(a) * d, "Quelle"))
        versuche = 0
        n = 0
        gesetzt = []
        while n < ziel and versuche < ziel * 600:
            versuche += 1
            if orte:
                x, y, grund = orte.pop(0)
            else:
                x = rng.uniform(centre.x - extent.x, centre.x + extent.x)
                y = rng.uniform(centre.y - extent.y, centre.y + extent.y)
                grund = "Wald" if art == "Saftperle" else "Moor"
            if any((x - gx) ** 2 + (y - gy) ** 2 < abstand ** 2 for _, gx, gy in gruppen):
                continue
            teile = gruppe(zones, art, zonen, x, y, rng.randint(*stueck), top, bottom)
            if not teile:
                continue
            gruppen.append((art, x, y))
            n += 1
            for i, (px, py, z) in enumerate(teile):
                actor = actors.spawn_actor_from_class(unreal.VaelHarvestable, unreal.Vector(px, py, z), unreal.Rotator(0, 0, rng.uniform(0, 360)))
                actor.set_editor_property("data", data)
                actor.set_folder_path("{}/{}".format(FOLDER, art))
                actor.set_actor_label("{}_{}_{:02d}".format(art, grund, n))
                actor.set_editor_property("tags", [unreal.Name(TAG), unreal.Name("VaelNatur")])
                gesetzt.append(actor)
        pflanzen[art] = (n, len(gesetzt), versuche)

    unreal.EditorLoadingAndSavingUtils.save_map(world, LEVEL_PATH)
    log("{} alte entfernt; gesetzt (Gruppen, Pflanzen, Versuche): {}".format(removed, pflanzen))


main()
