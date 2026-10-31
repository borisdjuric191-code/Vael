"""
Vael – gemalte Zauber-Symbole (Blender, ohne Oberfläche, nur als Rechner)
=========================================================================

Malt die Symbole der Elemente und Zauber: Zungen mit schwarzer Tuschekontur, Farbverlauf und Pinselstrichen entlang des Strichs,
auf dunklem Grimoire-Pergament mit Rissen und einem Siegelring in Silbertinte. Eigene Motive, nichts aus fremden Spielen.

Ein Motiv ist eine Liste von Zungen, von hinten nach vorn: jede eine Linie aus Punkten (x, y, Breite) im Bild von 0 bis 1.

Ergebnis: Scripts/Data/HUD/Zauber/T_Zauber_<Name>.png (256 Pixel), dazu eine Übersicht _Uebersicht.png.

Aufruf:
  blender -b --factory-startup -P vael_symbole_malen.py -- "C:/Projekte/Vael/Scripts/Data/HUD/Zauber" [Name ...]
"""
import math
import os
import sys

import bpy
import numpy as np

SIZE = 512          # Gemalt wird doppelt so groß, das Ergebnis hat 256 Pixel
SEED = 5

# Farbverläufe von kalt (0, Spitzen) bis heiß (1, Kern): Farbe eines Elements in Tinte
FIRE = ((0.0, (0.16, 0.012, 0.01)), (0.26, (0.46, 0.04, 0.012)), (0.48, (0.86, 0.17, 0.02)), (0.68, (1.0, 0.45, 0.05)), (0.86, (1.0, 0.76, 0.24)), (1.0, (1.0, 0.95, 0.72)))
WATER = ((0.0, (0.006, 0.02, 0.09)), (0.3, (0.012, 0.07, 0.26)), (0.52, (0.03, 0.22, 0.52)), (0.72, (0.12, 0.5, 0.76)), (0.9, (0.45, 0.82, 0.92)), (1.0, (0.9, 0.98, 1.0)))
EARTH = ((0.0, (0.018, 0.011, 0.007)), (0.3, (0.06, 0.036, 0.018)), (0.52, (0.15, 0.09, 0.04)), (0.72, (0.3, 0.2, 0.095)), (0.9, (0.5, 0.37, 0.2)), (1.0, (0.7, 0.57, 0.36)))
DUST = ((0.0, (0.06, 0.04, 0.02)), (0.5, (0.22, 0.15, 0.08)), (1.0, (0.55, 0.42, 0.25)))
AIR = ((0.0, (0.3, 0.4, 0.5)), (0.5, (0.6, 0.74, 0.84)), (1.0, (0.95, 0.98, 1.0)))
AIR_LIGHT = (0.62, 0.76, 0.88)
NID = ((0.0, (0.02, 0.0, 0.008)), (0.3, (0.075, 0.003, 0.014)), (0.52, (0.19, 0.006, 0.024)), (0.72, (0.36, 0.014, 0.04)), (0.88, (0.42, 0.05, 0.13)), (1.0, (0.56, 0.24, 0.52)))
NID_ROCK = ((0.0, (0.008, 0.002, 0.012)), (0.4, (0.035, 0.008, 0.045)), (0.7, (0.13, 0.03, 0.17)), (1.0, (0.36, 0.14, 0.46)))
DEPTH = ((0.0, (0.1, 0.01, 0.2)), (0.45, (0.42, 0.14, 0.82)), (0.8, (0.72, 0.5, 1.0)), (1.0, (0.96, 0.9, 1.0)))
IRIS = ((0.0, (0.012, 0.0, 0.008)), (0.3, (0.07, 0.002, 0.012)), (0.55, (0.2, 0.008, 0.024)), (0.78, (0.4, 0.04, 0.06)), (1.0, (0.62, 0.22, 0.3)))
FOAM = ((0.0, (0.5, 0.8, 0.92)), (0.5, (0.8, 0.95, 1.0)), (1.0, (1.0, 1.0, 1.0)))


def noise(rng, steep):
    """Kachelbares Rauschen, -1 bis etwa 1; je steiler, desto gröber"""
    fx = np.fft.fftfreq(SIZE)[:, None]
    fy = np.fft.fftfreq(SIZE)[None, :]
    f = np.sqrt(fx * fx + fy * fy)
    f[0, 0] = 1.0
    field = np.fft.ifft2(np.fft.fft2(rng.normal(size=(SIZE, SIZE))) / f ** steep).real
    return (field - field.mean()) / (field.std() * 2.2)


def sample(field, x, y):
    """Liest ein Rauschfeld an Pixelkoordinaten, gekachelt"""
    return field[np.mod(y.astype(np.int32), SIZE), np.mod(x.astype(np.int32), SIZE)]


def ramp(stops, value):
    """Farbe aus einem Verlauf"""
    value = np.clip(value, 0.0, 1.0)
    positions = [s[0] for s in stops]
    return np.stack([np.interp(value, positions, [s[1][k] for s in stops]) for k in range(3)], axis=-1)


def smooth_line(points, steps=18):
    """Weiche Linie durch (x, y, Breite)"""
    p = np.array(points, dtype=float)
    p = np.vstack((2 * p[0] - p[1], p, 2 * p[-1] - p[-2]))
    out = []
    for i in range(1, len(p) - 2):
        for t in np.linspace(0.0, 1.0, steps, endpoint=False):
            out.append(0.5 * (2 * p[i] + (p[i + 1] - p[i - 1]) * t + (2 * p[i - 1] - 5 * p[i] + 4 * p[i + 1] - p[i + 2]) * t * t
                              + (3 * p[i] - p[i - 1] - 3 * p[i + 1] + p[i + 2]) * t ** 3))
    out.append(p[-2])
    return np.array(out)


class Canvas:
    def __init__(self, seed):
        self.rng = np.random.default_rng(seed)
        self.rough, self.middle, self.fine = noise(self.rng, 1.7), noise(self.rng, 1.1), noise(self.rng, 0.6)
        self.warp_x, self.warp_y = noise(self.rng, 1.5), noise(self.rng, 1.5)
        self.y, self.x = np.mgrid[0:SIZE, 0:SIZE].astype(np.float32)
        self.image = np.zeros((SIZE, SIZE, 3), np.float32)
        self.heat = np.zeros((SIZE, SIZE), np.float32)
        self.cover = np.zeros((SIZE, SIZE), bool)
        self.edge = np.zeros((SIZE, SIZE), bool)

    def background(self, glow, glow_at=(0.5, 0.5), ring=True):
        """Dunkles Pergament des Grimoires, Risse, ein Schein hinter dem Motiv, ein Siegelring in Silbertinte"""
        u, v = self.x / SIZE, self.y / SIZE
        rim = np.hypot(u - 0.5, v - 0.5) / 0.71
        base = np.array((0.005, 0.008, 0.02))[None, None] * (1.0 + 0.9 * self.rough[..., None] + 0.4 * self.middle[..., None])
        heat = np.exp(-(np.hypot(u - glow_at[0], v - glow_at[1]) / 0.42) ** 2)[..., None]
        image = base + np.array(glow)[None, None] * heat * (0.3 + 0.2 * self.rough[..., None])
        image *= (1.0 - 0.75 * rim ** 2.5)[..., None]

        # Risse: Grenzen zwischen zufälligen Feldern, wie gesprungener Lack auf altem Leder
        points = self.rng.random((16, 2))
        distances = np.sort(np.stack([np.hypot(u + 0.03 * self.warp_x - px, v + 0.03 * self.warp_y - py) for px, py in points]), axis=0)
        crack = np.clip(1.0 - (distances[1] - distances[0]) / 0.008, 0.0, 1.0)
        image = image * (1.0 - 0.75 * crack[..., None]) + np.array((0.05, 0.05, 0.06))[None, None] * crack[..., None] * 0.2

        # Siegelring: dünne Doppellinie mit kleinen Marken, leicht von Hand gezogen
        if not ring:
            self.image = image
            return self
        ring = np.hypot(u - 0.5 + 0.006 * self.warp_x, v - 0.5 + 0.006 * self.warp_y)
        line = np.exp(-((ring - 0.455) / 0.0052) ** 2) + 0.7 * np.exp(-((ring - 0.43) / 0.0032) ** 2)
        angle = np.arctan2(v - 0.5, u - 0.5)
        ticks = (np.abs(np.mod(angle / (math.tau / 24.0) + 0.5, 1.0) - 0.5) < 0.05) & (ring > 0.43) & (ring < 0.455)
        line = np.clip(line + ticks * 0.8, 0.0, 1.0) * (0.62 + 0.38 * self.middle)
        self.image = image + np.array((0.5, 0.56, 0.66))[None, None] * line[..., None] * 0.13
        return self

    def tongue(self, points, stops, heat=0.0, ink=0.0062, streak=0.52, wobble=9.0):
        """Eine Zunge: Tuschekontur, Verlauf von der Wurzel (heiß) zur Spitze (kalt), Pinselstriche längs, heller Kern in der Mitte"""
        line = smooth_line(points)
        wx = self.x + wobble * self.warp_x
        wy = self.y + wobble * self.warp_y
        lx, ly, lw = line[:, 0] * SIZE, line[:, 1] * SIZE, np.maximum(line[:, 2], 0.0) * SIZE

        margin = lw.max() + 20.0
        x0, x1 = int(max(lx.min() - margin, 0)), int(min(lx.max() + margin, SIZE))
        y0, y1 = int(max(ly.min() - margin, 0)), int(min(ly.max() + margin, SIZE))
        wx_c, wy_c = wx[y0:y1, x0:x1], wy[y0:y1, x0:x1]
        dx = wx_c[..., None] - lx
        dy = wy_c[..., None] - ly
        dist = np.hypot(dx, dy)
        reach = dist / np.maximum(lw, 0.5)
        nearest = np.argmin(reach, axis=-1)
        rows, cols = np.indices(nearest.shape)
        r = lw[nearest]
        d = dist[rows, cols, nearest]
        t = nearest / (len(line) - 1)

        # Quer zur Linie: -1 und 1 sind die Ränder
        tangent_x = np.gradient(lx)[nearest]
        tangent_y = np.gradient(ly)[nearest]
        size = np.maximum(np.hypot(tangent_x, tangent_y), 1e-3)
        side = (dx[rows, cols, nearest] * -tangent_y + dy[rows, cols, nearest] * tangent_x) / size / np.maximum(r, 0.5)

        inside = d < r + ink * SIZE
        body = d < r
        along = t * 260.0
        across = side * 34.0
        brush = sample(self.rough, along * 1.6, across * 2.0) * 0.55 + sample(self.middle, along * 3.0, across * 4.0) * 0.45

        value = (1.0 - t) ** 0.8 * 0.46 + heat + 0.27 * (1.0 - np.abs(side)) ** 1.5 + streak * 0.42 * brush

        # Jede Zunge fließt weich in die gemeinsame Hitzefläche: Kontur bekommt erst die ganze Flamme in merge()
        soft = np.clip((1.0 - np.abs(side)) / 0.6, 0.0, 1.0) ** 1.4
        hot = self.heat[y0:y1, x0:x1]
        covered = self.cover[y0:y1, x0:x1]
        mixed = np.where(covered, hot * (1.0 - soft) + value * soft, value)
        hot[body] = mixed[body]
        covered[body] = True
        self.edge[y0:y1, x0:x1][inside] = True
        return self

    def disc(self, center, radius, heat, ink=0.0062):
        """Eine runde Fläche in der gemeinsamen Hitzefläche: heller Kern oben links, dunkler zum Rand, wie eine Kugel"""
        cx, cy = center[0] * SIZE, center[1] * SIZE
        r = radius * SIZE
        d = np.hypot(self.x + 2.0 * self.warp_x - cx, self.y + 2.0 * self.warp_y - cy)
        light = np.hypot(self.x - (cx - r * 0.3), self.y - (cy - r * 0.35)) / r
        value = heat + 0.5 * np.clip(1.0 - light / 1.4, 0.0, 1.0) + 0.1 * self.rough
        body = d < r
        self.heat[body] = value[body]
        self.cover[body] = True
        self.edge[d < r + ink * SIZE] = True
        return self

    def ink_shape(self, points, color=(0.004, 0.006, 0.016)):
        """Eine kleine Form in dunkler Tusche, wie ein Scherenschnitt: ein Punkt ist im Inneren, wenn ein Strahl die Kante ungerade oft kreuzt"""
        x, y = self.x / SIZE, self.y / SIZE
        inside = np.zeros((SIZE, SIZE), bool)
        for (x0, y0), (x1, y1) in zip(points, points[1:] + points[:1]):
            crosses = (y0 > y) != (y1 > y)
            inside ^= crosses & (x < (x1 - x0) * (y - y0) / ((y1 - y0) or 1e-6) + x0)
        self.image[inside] = color
        return self

    def polygon(self, points):
        """Fläche innerhalb eines Umrisses: ein Punkt ist drinnen, wenn ein Strahl die Kante ungerade oft kreuzt"""
        x, y = self.x / SIZE, self.y / SIZE
        inside = np.zeros((SIZE, SIZE), bool)
        for (x0, y0), (x1, y1) in zip(points, points[1:] + points[:1]):
            crosses = (y0 > y) != (y1 > y)
            inside ^= crosses & (x < (x1 - x0) * (y - y0) / ((y1 - y0) or 1e-6) + x0)
        return inside

    def rock(self, outline, facets, tone, tall=False, stops=None, ink=(0.02, 0.012, 0.007)):
        """Fels: flache Bruchflächen, jede mit eigenem Ton nach ihrer Lage zum Licht, weiche Risse dazwischen, dünne Kontur außen,
        eine helle Kante oben links wo das Licht auftrifft"""
        # Der Umriss bricht: jede Kante bekommt Zacken, zweimal halbiert und seitlich versetzt
        jagged = [(float(a), float(b)) for a, b in outline]
        for depth in (0.1, 0.055):
            finer = []
            for (ax, ay), (bx, by) in zip(jagged, jagged[1:] + jagged[:1]):
                length = math.hypot(bx - ax, by - ay)
                offset = self.rng.uniform(-depth, depth) * length
                finer += [(ax, ay), ((ax + bx) / 2 - (by - ay) / max(length, 1e-4) * offset, (ay + by) / 2 + (bx - ax) / max(length, 1e-4) * offset)]
            jagged = finer
        outline = jagged
        points = np.array(outline)
        x0, y0, x1, y1 = points[:, 0].min(), points[:, 1].min(), points[:, 0].max(), points[:, 1].max()
        wx = self.x / SIZE + 0.006 * self.warp_x
        wy = self.y / SIZE + 0.006 * self.warp_y
        inside = self.polygon(outline)

        # Bruchflächen: Zellen um zufällige Punkte; bei hohen Pfeilern längs gestreckt
        seeds = self.rng.random((facets, 2)) * (x1 - x0, y1 - y0) + (x0, y0)
        stretch = 2.6 if tall else 1.0
        # Abstand als schiefes Maß mit eigener Richtung je Fläche: so werden die Grenzen zu geraden Bruchkanten statt zu Waben
        turns = self.rng.uniform(0.0, math.pi, facets)
        distance = np.stack([np.abs((wx - sx) * math.cos(a) + (wy - sy) / stretch * math.sin(a)) + 0.62 * np.abs(-(wx - sx) * math.sin(a) + (wy - sy) / stretch * math.cos(a))
                             for (sx, sy), a in zip(seeds, turns)])
        order = np.argsort(distance, axis=0)[:2]
        nearest = order[0]
        rows, cols = np.indices(nearest.shape)
        gap = distance[order[1], rows, cols] - distance[nearest, rows, cols]

        # Jede Fläche ist schräg zum Licht von links oben: ihr Ton hängt von ihrem Platz ab und bekommt einen eigenen Zufall
        lit = 1.0 - ((seeds[:, 0] - x0) / max(x1 - x0, 1e-3) * 0.55 + (seeds[:, 1] - y0) / max(y1 - y0, 1e-3) * 0.45)
        tones = 0.06 + 0.5 * lit + self.rng.normal(0.0, 0.1, facets) + tone
        value = tones[nearest] + 0.09 * self.rough + 0.055 * self.middle + 0.03 * self.fine

        # Innerhalb einer Fläche ein leichter Verlauf, damit sie nicht wie ausgeschnitten wirkt
        value += 0.1 * np.clip(gap / 0.05, 0.0, 1.0) - 0.05

        # Nicht jede Grenze ist ein Riss: das Rauschen lässt sie stellenweise verschwinden
        crease = np.exp(-(gap / 0.0042) ** 2) * np.clip(0.55 + 0.9 * self.rough, 0.0, 1.0)
        color = ramp(stops or EARTH, value) * (1.0 - 0.8 * crease)[..., None]

        # Kontur und Lichtkante aus dem weich gezeichneten Umriss
        self.cover[inside] = True
        soft = self.blurred_cover(2.2)
        self.cover[:] = False
        edge = (soft > 0.05) & ~inside
        blurred_shift = np.roll(np.roll(inside, 3, axis=0), 3, axis=1)
        light_edge = inside & ~blurred_shift
        color[light_edge] = color[light_edge] * 0.7 + np.array((0.5, 0.4, 0.27)) * 0.3
        self.image[edge] = ink
        self.image[inside] = color[inside]
        return self

    def crack(self, start, end, width, turn, depth=0, color=None):
        """Ein Riss im gemalten Stein: ein zackiger, schwarzer Weg, der ausläuft, mit heller Bruchkante an einer Seite und Abzweigen"""
        count = 7
        px_, py = float(start[0]), float(start[1])
        ax, ay = end[0] - start[0], end[1] - start[1]
        length = math.hypot(ax, ay)
        nx, ny = -ay / length, ax / length
        for i in range(count):
            t0, t1 = i / count, (i + 1) / count
            jitter = self.rng.uniform(-0.05, 0.05) * length * math.sin(math.pi * t1)
            qx, qy = start[0] + ax * t1 + nx * jitter, start[1] + ay * t1 + ny * jitter
            w0, w1 = width * (1.0 - 0.62 * t0), width * (1.0 - 0.62 * t1)
            sx, sy = qx - px_, qy - py
            size = max(sx * sx + sy * sy, 1e-6)
            x, y = self.x / SIZE, self.y / SIZE
            along = np.clip(((x - px_) * sx + (y - py) * sy) / size, 0.0, 1.0)
            dist = np.hypot(x - (px_ + along * sx), y - (py + along * sy))
            wide = w0 + (w1 - w0) * along
            lip = (dist < wide + 0.0028) & (dist > wide * 0.8) & ((x - px_) * sy - (y - py) * sx > 0.0)
            if color is None:
                self.image[lip] = self.image[lip] * 0.72 + np.array((0.5, 0.4, 0.27)) * 0.28
            self.image[dist < wide] = color if color is not None else (0.008, 0.005, 0.004)
            if depth == 0 and 0 < i < count - 1 and i % 2 == 1:
                branch = 0.42 * length * (1.0 - t1 * 0.5)
                angle = math.atan2(ay, ax) + turn * (1.0 if i % 4 == 1 else -1.0)
                self.crack((qx, qy), (qx + math.cos(angle) * branch, qy + math.sin(angle) * branch), w1 * 0.72, turn, 1, color)
            px_, py = qx, qy
        return self

    def dust(self, start, end, width, strength=0.5, color=(0.34, 0.26, 0.17)):
        """Eine Staubfahne ohne Form und Kante: dünnt zum Ende aus und wird dort breiter, zerfasert durch Rauschen"""
        x = self.x / SIZE + 0.05 * self.warp_x
        y = self.y / SIZE + 0.05 * self.warp_y
        ax, ay = end[0] - start[0], end[1] - start[1]
        size = ax * ax + ay * ay
        along = ((x - start[0]) * ax + (y - start[1]) * ay) / size
        t = np.clip(along, 0.0, 1.0)
        dist = np.hypot(x - (start[0] + t * ax), y - (start[1] + t * ay))
        wide = width * (0.45 + 1.1 * t)
        amount = np.exp(-(dist / wide) ** 2) * (1.0 - t) ** 1.3 * np.clip(0.42 + 0.9 * self.rough + 0.7 * self.middle, 0.0, 1.0) * strength
        amount = np.where((along > 0.0) & (along < 1.0), amount, 0.0)[..., None]
        self.image = self.image * (1.0 - amount) + np.array(color)[None, None] * amount
        return self

    def wisp(self, points, strength=0.8, color=AIR_LIGHT, core=0.6):
        """Ein Luftband: keine Kontur und kein Körper, nur feine Fasern längs der Strömung, in der Mitte heller, an beiden Enden aus dem Nichts"""
        line = smooth_line(points, 16)
        lx, ly, lw = line[:, 0] * SIZE, line[:, 1] * SIZE, np.maximum(line[:, 2], 0.002) * SIZE
        margin = lw.max() * 2.2 + 8.0
        x0, x1 = int(max(lx.min() - margin, 0)), int(min(lx.max() + margin, SIZE))
        y0, y1 = int(max(ly.min() - margin, 0)), int(min(ly.max() + margin, SIZE))
        if x1 <= x0 or y1 <= y0:
            return self
        wx = self.x[y0:y1, x0:x1] + 4.0 * self.warp_x[y0:y1, x0:x1]
        wy = self.y[y0:y1, x0:x1] + 4.0 * self.warp_y[y0:y1, x0:x1]
        dx = wx[..., None] - lx
        dy = wy[..., None] - ly
        nearest = np.argmin(np.hypot(dx, dy), axis=-1)
        rows, cols = np.indices(nearest.shape)
        r = lw[nearest]
        t = nearest / (len(line) - 1)
        tangent_x, tangent_y = np.gradient(lx)[nearest], np.gradient(ly)[nearest]
        size = np.maximum(np.hypot(tangent_x, tangent_y), 1e-3)
        side = (dx[rows, cols, nearest] * -tangent_y + dy[rows, cols, nearest] * tangent_x) / size / r

        fibres = 0.5 + 1.1 * sample(self.middle, t * 40.0, side * 70.0) + 0.7 * sample(self.fine, t * 70.0, side * 150.0)
        amount = np.exp(-(side * 1.5) ** 2) * np.clip(fibres, 0.0, 1.4) + core * np.exp(-(side * 5.0) ** 2)
        amount *= np.clip(t / 0.28, 0.0, 1.0) ** 1.6 * np.sin(np.clip(t, 0.0, 1.0) * math.pi) ** 0.5 * strength
        self.image[y0:y1, x0:x1] = self.image[y0:y1, x0:x1] + np.array(color)[None, None] * amount[..., None]
        return self

    def glow(self, center, radius, color, strength=1.0):
        """Weicher Schein, der sich zum Bild addiert: die Aura des Nid"""
        d = np.hypot(self.x / SIZE - center[0], self.y / SIZE - center[1]) / radius
        amount = np.exp(-d * d) * (0.75 + 0.4 * self.rough) * strength
        self.image = self.image + np.array(color)[None, None] * amount[..., None]
        return self

    def bubble(self, center, radius):
        """Eine Blase auf der dicken Flüssigkeit: dunkler Rand, violette Haut, ein Lichtpunkt"""
        x, y = self.x / SIZE, self.y / SIZE
        d = np.hypot(x - center[0], y - center[1])
        skin = np.clip(d / radius, 0.0, 1.0)[..., None]
        self.image[d < radius * 1.22] = (0.02, 0.0, 0.015)
        inside = d < radius
        self.image[inside] = ((np.array((0.3, 0.02, 0.14))[None, None] * (1.0 - skin) + np.array((0.5, 0.2, 0.8))[None, None] * skin))[inside]
        self.image[np.hypot(x - center[0] + radius * 0.35, y - center[1] + radius * 0.35) < radius * 0.28] = (0.9, 0.82, 0.95)
        return self

    def rift(self, outline, heat):
        """Eine eingerissene FlÃ¤che in der gemeinsamen HitzeflÃ¤che, in der Mitte heiÃŸer"""
        inside = self.polygon(outline)
        self.cover[inside] = True
        self.heat[inside] = (heat + 0.3 * self.blurred_cover(9.0) + 0.14 * self.rough)[inside]
        self.edge[self.blurred_cover(1.6) > 0.04] = True
        return self
    def nid_ground(self, seed_center=(0.5, 0.5)):
        """Das Nid vergiftet das Bild selbst: der Siegelring wird stellenweise schwarz, feine Adern ziehen sich durch das Pergament"""
        u, v = self.x / SIZE, self.y / SIZE
        ring = np.hypot(u - 0.5, v - 0.5)
        band = np.exp(-((ring - 0.445) / 0.04) ** 2)
        eaten = np.clip(0.75 + 1.6 * self.rough, 0.0, 1.0) * band
        self.image *= (1.0 - 0.93 * eaten)[..., None]
        for angle in np.linspace(0.0, 360.0, 11, endpoint=False):
            turn = float(angle + self.rng.uniform(-14.0, 14.0))
            reach = self.rng.uniform(0.34, 0.6)
            end = (seed_center[0] + math.cos(math.radians(turn)) * reach, seed_center[1] + math.sin(math.radians(turn)) * reach)
            self.crack(seed_center, end, 0.0052, 0.6, color=(0.06, 0.006, 0.085))
        return self

    def eye(self, center, radius, squash=1.0):
        """Das Auge der Schlafenden: tief liegend und halb geschlossen, nicht ganz rund, die Iris aus dunklen Fasern, ein dünner Schlitz"""
        dx = (self.x / SIZE - center[0]) / radius + 0.1 * self.warp_x + 0.04 * self.middle
        dy = (self.y / SIZE - center[1]) / (radius * squash) + 0.1 * self.warp_y + 0.04 * self.rough
        d = np.hypot(dx, dy)
        angle = np.arctan2(dy, dx)
        fibres = 0.5 * np.sin(angle * 31.0 + 4.0 * self.middle) + 0.5 * np.sin(angle * 53.0 + 7.0 * self.fine)
        slit = np.abs(dx + 0.04 * np.sin(dy * 3.0)) / np.maximum(0.135 * np.sqrt(np.clip(1.0 - (dy / 1.0) ** 2, 0.0, 1.0)), 1e-3)
        iris = ramp(IRIS, np.clip(0.96 - 0.66 * d + 0.15 * fibres + 0.1 * self.middle + 0.26 * np.exp(-((slit - 2.0) / 1.6) ** 2), 0.0, 1.0))
        iris *= np.clip((slit - 0.75) / 0.4, 0.0, 1.0)[..., None]
        # Die Lider legen sich schwer von oben und unten darüber
        lid = np.clip((np.abs(dy + 0.06) - 0.76) / 0.22, 0.0, 1.0)
        amount = (np.clip((1.04 - d) / 0.22, 0.0, 1.0) * (1.0 - lid))[..., None]
        self.image = self.image * (1.0 - amount) * (1.0 - 0.75 * np.clip((1.3 - d) / 0.3, 0.0, 1.0))[..., None] + iris * amount
        light = np.hypot(dx + 0.3, dy + 0.2)
        spot = (np.clip((0.07 - light) / 0.03, 0.0, 1.0) * 0.7)[..., None]
        self.image = self.image * (1.0 - spot) + np.array((0.72, 0.6, 0.66))[None, None] * spot
        return self

    def spray(self, center, spread, count, direction=None):
        """Feiner Blutnebel: viele winzige, unregelmäßige Spritzer ohne Kontur, dicht am Ursprung, nach außen dünner"""
        for _ in range(count):
            reach = abs(self.rng.normal(0.0, spread))
            angle = self.rng.uniform(0.0, math.tau) if direction is None else math.radians(direction) + self.rng.normal(0.0, 0.5)
            x = (center[0] + math.cos(angle) * reach) * SIZE
            y = (center[1] + math.sin(angle) * reach) * SIZE
            r = self.rng.uniform(0.8, 2.6)
            x0, x1, y0, y1 = int(x - 5), int(x + 6), int(y - 5), int(y + 6)
            if x0 < 0 or y0 < 0 or x1 >= SIZE or y1 >= SIZE:
                continue
            d = np.hypot(self.x[y0:y1, x0:x1] - x, (self.y[y0:y1, x0:x1] - y) * self.rng.uniform(0.7, 1.4))
            amount = np.clip((r - d) / 1.2, 0.0, 1.0)[..., None] * 0.85
            self.image[y0:y1, x0:x1] = self.image[y0:y1, x0:x1] * (1.0 - amount) + np.array((0.14, 0.006, 0.02))[None, None] * amount
        return self

    def oil(self):
        """Nimmt die Schärfe aus dem Bild: wie dicke Ölfarbe verlaufen die Kanten leicht, Korn liegt darüber, der Rand versinkt im Dunkel"""
        fx = np.fft.fftfreq(SIZE)[:, None]
        fy = np.fft.fftfreq(SIZE)[None, :]
        soften = np.exp(-2.0 * (math.pi * 0.6) ** 2 * (fx * fx + fy * fy))
        blurred = np.stack([np.fft.ifft2(np.fft.fft2(self.image[..., k]) * soften).real for k in range(3)], axis=-1)
        u, v = self.x / SIZE, self.y / SIZE
        vignette = 1.0 - 0.55 * np.clip((np.hypot(u - 0.5, v - 0.5) - 0.3) / 0.42, 0.0, 1.0) ** 1.6
        self.image = np.maximum(blurred, 0.0) * (0.85 + 0.2 * self.middle[..., None] + 0.12 * self.fine[..., None]) * vignette[..., None]
        return self

    def ring_piece(self, center, radius, from_deg, to_deg, push):
        """Ein Stück des gesprengten Siegelrings, das nach außen fliegt und dabei geschwärzt ist"""
        middle = math.radians((from_deg + to_deg) / 2.0)
        cx, cy = center[0] + math.cos(middle) * push, center[1] + math.sin(middle) * push
        u, v = self.x / SIZE - cx, self.y / SIZE - cy
        ring = np.hypot(u, v)
        angle = np.degrees(np.arctan2(v, u))
        angle = np.mod(angle - from_deg, 360.0)
        span = (to_deg - from_deg) % 360.0
        cut = angle + 7.0 * self.middle
        mask = (np.abs(ring - radius) < 0.016) & (cut > 0.0) & (cut < span)
        edge = (np.abs(ring - radius) < 0.022) & (cut > -1.5) & (cut < span + 1.5)
        self.image[edge] = (0.012, 0.0, 0.012)
        shade = np.clip(0.5 - 3.0 * (ring - radius) * 10.0 + 0.5 * self.middle, 0.1, 1.2)[..., None]
        self.image[mask] = (np.array((0.17, 0.16, 0.19))[None, None] * shade)[mask]
        return self

    def blurred_cover(self, sigma):
        """Die gemalte Fläche, weich gezeichnet"""
        spectrum = np.fft.fft2(self.cover.astype(np.float32))
        fx = np.fft.fftfreq(SIZE)[:, None]
        fy = np.fft.fftfreq(SIZE)[None, :]
        return np.fft.ifft2(spectrum * np.exp(-2.0 * (math.pi * sigma) ** 2 * (fx * fx + fy * fy))).real

    def merge(self, stops, rim_dark=True, shift=0.0, ink=(0.022, 0.006, 0.006), gloss=False):
        """Malt alle Zungen seit dem letzten Mal als eine Flamme: ein Verlauf, nur außen eine Kontur, zum Rand hin dunkler.
        Ohne dunklen Rand (Lichter, Schaum) läuft die Fläche weich in das aus, was darunter liegt."""
        if not rim_dark:
            soft = np.clip(self.blurred_cover(3.0) * 1.3, 0.0, 1.0)[..., None] * 0.8
            self.image = self.image * (1.0 - soft) + ramp(stops, self.heat) * soft
            self.heat[:] = 0.0
            self.cover[:] = False
            self.edge[:] = False
            return self

        rim = np.clip((self.blurred_cover(11.0) - 0.42) / 0.5, 0.0, 1.0)
        color = ramp(stops, self.heat + shift - 0.26 * (1.0 - rim))
        color *= (0.5 + 0.5 * rim ** 0.8)[..., None]
        if gloss:
            # Nass: oben links ein scharfes Licht auf der WÃ¶lbung, unten rechts violettes Gegenlicht
            height = self.blurred_cover(4.5)
            slope_y, slope_x = np.gradient(height)
            slope = (slope_x + slope_y) / max(float(np.abs(slope_x + slope_y).max()), 1e-5)
            shine = np.clip((slope - 0.42) / 0.5, 0.0, 1.0) ** 2.6 * np.clip(0.2 + 1.6 * self.middle, 0.0, 1.0)
            back = np.clip((-slope - 0.22) / 0.6, 0.0, 1.0) ** 1.4
            color = color + np.array((1.0, 0.86, 0.96))[None, None] * shine[..., None] * 0.26 + np.array((0.2, 0.03, 0.36))[None, None] * back[..., None] * 0.34
        self.image[self.edge] = ink
        self.image[self.cover] = color[self.cover]
        self.heat[:] = 0.0
        self.cover[:] = False
        self.edge[:] = False
        return self

    def embers(self, stops, count, area):
        """Glutfunken mit kleiner Tuschekante"""
        for _ in range(count):
            x = self.rng.uniform(area[0], area[2]) * SIZE
            y = self.rng.uniform(area[1], area[3]) * SIZE
            r = self.rng.uniform(2.5, 5.5)
            d = np.hypot(self.x - x, self.y - y)
            self.image[d < r + 2.2] = (0.012, 0.01, 0.012)
            self.image[d < r] = ramp(stops, 0.85 - d[d < r] / r * 0.5)
        return self

    def finish(self):
        """Lasur über das Ganze, damit es wie eine Malerei wirkt und nicht wie Vektorgrafik"""
        grain = 1.0 + 0.1 * self.middle[..., None] + 0.06 * self.fine[..., None]
        return np.clip(self.image * grain, 0.0, 1.0)


# ---------------------------------------------------------------- Motive

def feuer(c):
    """Das Element: eine aufrechte Flamme, die aus einem Punkt wächst, in Zungen wie Federn"""
    c.background((0.2, 0.06, 0.02), (0.5, 0.6))
    for pts, heat in (
        # Ein Körper, der sich hochwindet; kleine Zungen lösen sich seitlich von ihm, keine gleicht der anderen
        ([(0.47, 0.86, 0.09), (0.38, 0.7, 0.115), (0.33, 0.54, 0.07), (0.25, 0.37, 0.002)], -0.08),
        ([(0.53, 0.86, 0.09), (0.62, 0.7, 0.11), (0.66, 0.52, 0.065), (0.61, 0.34, 0.03), (0.68, 0.2, 0.002)], -0.06),
        ([(0.5, 0.88, 0.115), (0.48, 0.66, 0.12), (0.53, 0.44, 0.08), (0.47, 0.27, 0.04), (0.52, 0.07, 0.002)], 0.04),
        ([(0.5, 0.86, 0.065), (0.51, 0.7, 0.07), (0.48, 0.54, 0.04), (0.51, 0.4, 0.002)], 0.42),
    ):
        c.tongue(pts, FIRE, heat)
    c.merge(FIRE)
    c.embers(FIRE, 5, (0.22, 0.14, 0.8, 0.5))


def funke(c):
    """Der Funke: eine scharfe Pfeilspitze aus Licht, die aus zwei Fingern schießt, mit dünnem Schweif"""
    c.background((0.16, 0.05, 0.02), (0.68, 0.34))
    for pts, heat in (
        ([(0.2, 0.84, 0.002), (0.34, 0.66, 0.018), (0.5, 0.5, 0.036), (0.62, 0.38, 0.03)], -0.25),
        ([(0.3, 0.8, 0.002), (0.42, 0.68, 0.016), (0.55, 0.52, 0.03), (0.64, 0.4, 0.026)], -0.2),
        ([(0.14, 0.72, 0.002), (0.32, 0.58, 0.016), (0.48, 0.47, 0.03), (0.62, 0.37, 0.026)], -0.2),
    ):
        c.tongue(pts, FIRE, heat)
    # Pfeilspitze: zwei Flügel und der Kern
    c.tongue([(0.56, 0.5, 0.012), (0.66, 0.46, 0.04), (0.74, 0.32, 0.03), (0.84, 0.15, 0.002)], FIRE, 0.1)
    c.tongue([(0.5, 0.44, 0.012), (0.56, 0.34, 0.04), (0.7, 0.26, 0.03), (0.84, 0.15, 0.002)], FIRE, 0.1)
    c.tongue([(0.58, 0.42, 0.036), (0.68, 0.33, 0.032), (0.77, 0.23, 0.016), (0.84, 0.15, 0.002)], FIRE, 0.42)
    c.merge(FIRE)
    c.embers(FIRE, 4, (0.2, 0.34, 0.5, 0.8))


def feuerball(c):
    """Der Feuerball: ein glühender Kern, der einen Schweif aus Zungen hinter sich herzieht, von unten rechts nach links oben"""
    c.background((0.2, 0.07, 0.02), (0.38, 0.38))
    for pts, heat in (
        ([(0.42, 0.42, 0.11), (0.62, 0.48, 0.09), (0.76, 0.63, 0.05), (0.9, 0.72, 0.002)], -0.2),
        ([(0.42, 0.42, 0.11), (0.54, 0.6, 0.09), (0.66, 0.76, 0.05), (0.72, 0.93, 0.002)], -0.2),
        ([(0.42, 0.42, 0.13), (0.64, 0.56, 0.1), (0.7, 0.74, 0.055), (0.86, 0.86, 0.002)], -0.08),
        ([(0.42, 0.42, 0.11), (0.6, 0.36, 0.075), (0.76, 0.4, 0.04), (0.92, 0.52, 0.002)], -0.16),
        ([(0.42, 0.42, 0.11), (0.42, 0.62, 0.075), (0.46, 0.78, 0.04), (0.54, 0.92, 0.002)], -0.16),
    ):
        c.tongue(pts, FIRE, heat)
    c.tongue([(0.34, 0.33, 0.13), (0.42, 0.42, 0.17), (0.54, 0.52, 0.12), (0.62, 0.6, 0.03)], FIRE, 0.02, streak=0.6)
    # The core is a whirl of three tongues round a hot middle
    c.tongue([(0.22, 0.42, 0.012), (0.27, 0.28, 0.05), (0.4, 0.22, 0.05), (0.52, 0.3, 0.012)], FIRE, 0.08)
    c.tongue([(0.27, 0.54, 0.012), (0.24, 0.42, 0.045), (0.32, 0.3, 0.06), (0.46, 0.3, 0.012)], FIRE, 0.04)
    c.tongue([(0.46, 0.58, 0.012), (0.34, 0.56, 0.05), (0.3, 0.44, 0.06), (0.4, 0.34, 0.012)], FIRE, -0.02)
    c.tongue([(0.28, 0.44, 0.03), (0.34, 0.39, 0.085), (0.42, 0.39, 0.08), (0.52, 0.46, 0.01)], FIRE, 0.5, streak=0.3)
    c.merge(FIRE)
    c.embers(FIRE, 5, (0.5, 0.25, 0.92, 0.9))


def feuerstrahl(c):
    """Der Feuerstrahl: ein gebündelter Strom, der sich zur Mitte verjüngt, mit Zungen, die sich um ihn winden"""
    c.background((0.2, 0.06, 0.02), (0.55, 0.48))
    for pts, heat in (
        ([(0.1, 0.86, 0.03), (0.34, 0.74, 0.05), (0.56, 0.46, 0.04), (0.92, 0.1, 0.002)], -0.22),
        ([(0.2, 0.92, 0.03), (0.4, 0.66, 0.05), (0.66, 0.5, 0.036), (0.94, 0.16, 0.002)], -0.22),
    ):
        c.tongue(pts, FIRE, heat)
    c.tongue([(0.08, 0.92, 0.1), (0.34, 0.66, 0.085), (0.62, 0.38, 0.055), (0.93, 0.08, 0.008)], FIRE, 0.06)
    c.tongue([(0.1, 0.9, 0.05), (0.36, 0.64, 0.044), (0.62, 0.38, 0.026), (0.9, 0.11, 0.004)], FIRE, 0.42, streak=0.3)
    c.tongue([(0.2, 0.68, 0.002), (0.3, 0.56, 0.024), (0.44, 0.5, 0.016), (0.52, 0.4, 0.002)], FIRE, -0.1)
    c.tongue([(0.44, 0.8, 0.002), (0.5, 0.66, 0.022), (0.6, 0.6, 0.014), (0.66, 0.5, 0.002)], FIRE, -0.1)
    c.merge(FIRE)
    c.embers(FIRE, 5, (0.2, 0.1, 0.9, 0.9))


def feuerwalze(c):
    """Die Feuerwalze: eine rollende Wand aus Flammen, die sich überschlägt, flach über dem Boden nach rechts"""
    c.background((0.2, 0.06, 0.02), (0.5, 0.56))
    for pts, heat in (
        ([(0.1, 0.8, 0.07), (0.26, 0.52, 0.09), (0.46, 0.3, 0.06), (0.74, 0.27, 0.03), (0.82, 0.42, 0.002)], -0.16),
        ([(0.2, 0.82, 0.08), (0.42, 0.58, 0.1), (0.6, 0.4, 0.06), (0.78, 0.42, 0.03), (0.83, 0.56, 0.002)], -0.04),
        ([(0.34, 0.84, 0.08), (0.54, 0.68, 0.09), (0.72, 0.56, 0.05), (0.84, 0.62, 0.002)], 0.1),
        ([(0.1, 0.84, 0.05), (0.32, 0.78, 0.06), (0.6, 0.8, 0.05), (0.9, 0.78, 0.002)], 0.2),
    ):
        c.tongue(pts, FIRE, heat)
    for pts in ([(0.14, 0.62, 0.002), (0.13, 0.46, 0.03), (0.22, 0.3, 0.002)], [(0.3, 0.36, 0.002), (0.32, 0.22, 0.022), (0.42, 0.14, 0.002)]):
        c.tongue(pts, FIRE, -0.1)
    c.merge(FIRE)
    c.embers(FIRE, 6, (0.1, 0.12, 0.9, 0.6))


def glints(c, strokes):
    """Scharfe helle Lichter und Schaum auf dem Wasser, ohne Kontur, über der fertigen Fläche"""
    for pts in strokes:
        c.tongue(pts, FOAM, 0.3, ink=0.0, streak=0.5)
    c.merge(FOAM, rim_dark=False)


def wasser(c):
    """Das Element: ein schwerer Tropfen, in dem das Wasser kreist"""
    c.background((0.02, 0.07, 0.2), (0.5, 0.62))
    for pts, heat in (
        ([(0.5, 0.7, 0.21), (0.5, 0.56, 0.17), (0.5, 0.38, 0.075), (0.5, 0.1, 0.002)], -0.06),
        ([(0.38, 0.74, 0.03), (0.52, 0.78, 0.06), (0.63, 0.68, 0.05), (0.57, 0.55, 0.002)], 0.3),
        ([(0.6, 0.6, 0.02), (0.47, 0.54, 0.05), (0.41, 0.63, 0.04), (0.48, 0.69, 0.002)], 0.14),
    ):
        c.tongue(pts, WATER, heat)
    c.merge(WATER, shift=-0.2, ink=(0.003, 0.012, 0.05))
    glints(c, ([(0.33, 0.74, 0.002), (0.35, 0.62, 0.011), (0.42, 0.47, 0.008), (0.46, 0.36, 0.002)],))
    c.embers(WATER, 4, (0.15, 0.15, 0.88, 0.5))


def curl(center, radius, start_deg, turns, width, shrink=0.92, end_width=0.002, count=9):
    """Punkte einer Spirale, die sich nach innen rollt: auf dem Bild im Uhrzeigersinn, Breite läuft zur Spitze aus"""
    points = []
    for i in range(count):
        t = i / (count - 1)
        angle = math.radians(start_deg + 360.0 * turns * t)
        r = radius * (1.0 - shrink * t)
        points.append((center[0] + r * math.cos(angle), center[1] + r * math.sin(angle), width * (1.0 - t) ** 1.1 + end_width))
    return points


def crest(center, radius, start_deg, turns, width, shrink, part, foam, count=12):
    """Schaum auf der Außenkante einer curl()-Spirale: part sagt, welcher Abschnitt der Spirale (0 bis 1) ihn trägt"""
    points = []
    for i in range(count):
        s = i / (count - 1)
        t = part[0] + (part[1] - part[0]) * s
        angle = math.radians(start_deg + 360.0 * turns * t)
        r = radius * (1.0 - shrink * t) + width * (1.0 - t) ** 1.1 * 0.8
        points.append((center[0] + r * math.cos(angle), center[1] + r * math.sin(angle), foam * math.sin(s * math.pi) ** 0.7 + 0.001))
    return points


def arc(center, radius, from_deg, to_deg, width, count=7):
    """Ein Bogen um einen Punkt, in der Mitte am breitesten"""
    points = []
    for i in range(count):
        t = i / (count - 1)
        angle = math.radians(from_deg + (to_deg - from_deg) * t)
        points.append((center[0] + radius * math.cos(angle), center[1] + radius * math.sin(angle), width * math.sin(t * math.pi) ** 0.8 + 0.001))
    return points


def wassergeschoss(c):
    """Das Wassergeschoss: eine klare Kugel, in der das Wasser kreist, mit Druckringen und Tropfen hinter sich"""
    c.background((0.02, 0.07, 0.2), (0.58, 0.42))
    middle = (0.58, 0.42)
    # Zwei Druckringe hinter der Kugel, sie läuft nach rechts oben
    for radius, width in ((0.3, 0.02), (0.385, 0.014)):
        c.tongue(arc(middle, radius, 95.0, 175.0, width), WATER, 0.1, streak=0.1, wobble=2.0)
    c.merge(WATER, shift=-0.2, ink=(0.003, 0.012, 0.05))
    c.disc(middle, 0.2, 0.3)
    c.tongue(curl(middle, 0.15, 150.0, 1.25, 0.05, shrink=0.75), WATER, 0.28, streak=0.15, wobble=1.5)
    c.merge(WATER, shift=-0.2, ink=(0.003, 0.012, 0.05))
    glints(c, ([(0.45, 0.36, 0.002), (0.49, 0.28, 0.011), (0.58, 0.25, 0.008), (0.64, 0.25, 0.002)],))
    # Tropfen auf der Bahn hinter der Kugel
    for x, y, size in ((0.26, 0.74, 0.022), (0.18, 0.82, 0.016), (0.32, 0.8, 0.012), (0.2, 0.68, 0.011)):
        c.tongue([(x, y, size), (x + 0.012, y - 0.012, size * 0.8), (x + 0.04, y - 0.04, 0.001)], WATER, 0.25, streak=0.1, wobble=1.0)
    c.merge(WATER, shift=-0.2, ink=(0.003, 0.012, 0.05))


def welle(c):
    """Die Welle: eine einzelne flache Woge, die unten durch den Ring läuft und nach rechts bricht, mit viel Luft darüber"""
    c.background((0.02, 0.07, 0.2), (0.5, 0.66))
    center = (0.6, 0.57)
    body = [(0.12, 0.76, 0.03), (0.28, 0.73, 0.055), (0.42, 0.66, 0.07)] + curl(center, 0.155, 170.0, 0.9, 0.07, shrink=0.6)
    c.tongue(body, WATER, 0.0, streak=0.12, wobble=2.0)
    band = [(0.18, 0.75, 0.008), (0.3, 0.72, 0.016), (0.42, 0.655, 0.024)] + curl(center, 0.148, 172.0, 0.88, 0.024, shrink=0.6)
    c.tongue(band, WATER, 0.3, streak=0.08, wobble=2.0)
    c.tongue([(0.1, 0.78, 0.002), (0.3, 0.77, 0.035), (0.6, 0.76, 0.04), (0.88, 0.77, 0.002)], WATER, 0.0, streak=0.12, wobble=2.0)
    c.merge(WATER, shift=-0.2, ink=(0.003, 0.012, 0.05))
    glints(c, (crest(center, 0.155, 170.0, 0.9, 0.07, 0.6, (0.08, 0.72), 0.013),))
    c.embers(WATER, 3, (0.66, 0.3, 0.82, 0.42))


def flutwelle(c):
    """Die Flutwelle: eine Wand aus Wasser, die das ganze Bild füllt und über den Ring hinausgeht. Zwei Kämme übereinander;
    der winzige Turm vor ihr zeigt, wie groß sie ist."""
    c.background((0.02, 0.07, 0.2), (0.45, 0.42))
    # Hinterer Kamm, dunkler, er steht höher als alles
    back = (0.5, 0.36)
    c.tongue([(0.0, 0.8, 0.22), (0.1, 0.5, 0.2)] + curl(back, 0.3, 175.0, 0.8, 0.17, shrink=0.5, count=10), WATER, -0.2, streak=0.12, wobble=2.5)
    c.merge(WATER, shift=-0.2, ink=(0.003, 0.012, 0.05))
    glints(c, (crest(back, 0.3, 175.0, 0.8, 0.17, 0.5, (0.12, 0.7), 0.016),))

    # Vorderer Kamm und die Wassermasse, sie füllt das Bild bis an den Rand
    center = (0.62, 0.56)
    body = [(0.0, 0.98, 0.26), (0.14, 0.82, 0.26), (0.3, 0.7, 0.2)] + curl(center, 0.24, 165.0, 0.85, 0.19, shrink=0.55, count=11)
    c.tongue(body, WATER, -0.04, streak=0.12, wobble=2.5)
    band = [(0.12, 0.88, 0.02), (0.26, 0.74, 0.044), (0.36, 0.66, 0.052)] + curl(center, 0.23, 167.0, 0.83, 0.052, shrink=0.55, count=11)
    c.tongue(band, WATER, 0.28, streak=0.08, wobble=2.5)
    c.tongue([(0.0, 1.0, 0.16), (0.4, 0.98, 0.17), (0.75, 0.96, 0.14), (1.0, 0.98, 0.1)], WATER, -0.02, streak=0.12, wobble=2.5)
    c.merge(WATER, shift=-0.2, ink=(0.003, 0.012, 0.05))
    glints(c, (crest(center, 0.24, 165.0, 0.85, 0.19, 0.55, (0.1, 0.72), 0.022),))

    # Der winzige Turm im Weg der Welle
    tower = [(0.84, 0.86), (0.84, 0.72), (0.825, 0.72), (0.825, 0.695), (0.84, 0.695), (0.84, 0.71), (0.852, 0.71), (0.852, 0.695), (0.866, 0.695), (0.866, 0.71),
                 (0.878, 0.71), (0.878, 0.695), (0.893, 0.695), (0.893, 0.72), (0.878, 0.72), (0.878, 0.86)]
    c.ink_shape([(0.56 + (x - 0.86) * 1.5, 0.93 + (y - 0.86) * 1.5) for x, y in tower])
    c.embers(WATER, 5, (0.8, 0.1, 0.98, 0.4))


def round_outline(center, radius, count=14):
    """Umriss eines runden Steins"""
    return [(center[0] + radius * math.cos(a), center[1] + radius * math.sin(a)) for a in np.linspace(0.0, math.tau, count, endpoint=False)]


def erde(c):
    """Das Element: ein runder Stein, der unten zerfällt, Brocken lösen sich und fallen"""
    c.background((0.1, 0.065, 0.025), (0.5, 0.48))
    c.rock([(0.3, 0.2), (0.46, 0.13), (0.6, 0.17), (0.7, 0.14), (0.8, 0.3), (0.78, 0.46), (0.82, 0.56), (0.7, 0.68), (0.56, 0.64), (0.44, 0.7), (0.3, 0.66), (0.22, 0.56), (0.2, 0.38)], 20, 0.06)
    for shard, tone in (
        ([(0.26, 0.68), (0.33, 0.64), (0.37, 0.72), (0.3, 0.77)], -0.04),
        ([(0.43, 0.75), (0.5, 0.72), (0.54, 0.79), (0.46, 0.83)], 0.0),
        ([(0.62, 0.69), (0.7, 0.66), (0.72, 0.74), (0.65, 0.77)], -0.04),
        ([(0.36, 0.84), (0.41, 0.82), (0.43, 0.87), (0.38, 0.89)], -0.08),
        ([(0.56, 0.87), (0.6, 0.85), (0.62, 0.9), (0.57, 0.91)], -0.08),
    ):
        c.rock(shard, 4, tone)
    c.embers(DUST, 6, (0.26, 0.7, 0.74, 0.94))


def steinbrocken(c):
    """Das Steingeschoss: ein spitzer Stein in voller Bewegung nach rechts oben, hinter ihm Staubstreifen und Splitter"""
    c.background((0.1, 0.065, 0.025), (0.62, 0.38))
    # Weicher Staub statt Streifen: die Fahne wird nach hinten breiter und lÃ¶st sich auf
    c.dust((0.52, 0.5), (0.06, 0.92), 0.08, 0.95)
    c.dust((0.46, 0.46), (0.04, 0.68), 0.055, 0.65)
    c.dust((0.56, 0.58), (0.26, 0.98), 0.055, 0.65)
    c.rock([(0.9, 0.09), (0.73, 0.2), (0.53, 0.34), (0.4, 0.5), (0.45, 0.58), (0.58, 0.62), (0.72, 0.5), (0.83, 0.3)], 13, 0.08)
    c.embers(DUST, 9, (0.1, 0.55, 0.42, 0.92))
    for shard, tone in (([(0.2, 0.74), (0.26, 0.7), (0.29, 0.76), (0.23, 0.8)], -0.06), ([(0.34, 0.84), (0.39, 0.81), (0.41, 0.86), (0.36, 0.89)], -0.1), ([(0.12, 0.58), (0.17, 0.55), (0.19, 0.6), (0.14, 0.62)], -0.12)):
        c.rock(shard, 4, tone)


def felswand(c):
    """Die Felswand: Pfeiler aus Stein stehen dicht an dicht aus dem Boden, ihre Spitzen sind gebrochen"""
    c.background((0.1, 0.065, 0.025), (0.5, 0.6))
    for shape, tone in (
        ([(0.06, 0.86), (0.08, 0.5), (0.14, 0.42), (0.23, 0.47), (0.25, 0.86)], -0.12),
        ([(0.76, 0.86), (0.77, 0.44), (0.84, 0.36), (0.93, 0.46), (0.95, 0.86)], -0.12),
        ([(0.22, 0.86), (0.23, 0.32), (0.31, 0.2), (0.4, 0.3), (0.42, 0.86)], -0.02),
        ([(0.6, 0.86), (0.61, 0.26), (0.68, 0.16), (0.77, 0.27), (0.79, 0.86)], -0.02),
        ([(0.4, 0.86), (0.41, 0.2), (0.5, 0.08), (0.6, 0.18), (0.62, 0.86)], 0.08),
    ):
        c.rock(shape, 13, tone, tall=True)
    c.rock([(0.03, 0.92), (0.2, 0.84), (0.5, 0.87), (0.8, 0.83), (0.97, 0.92), (0.8, 0.95), (0.2, 0.95)], 14, -0.14)
    c.embers(DUST, 4, (0.1, 0.12, 0.9, 0.32))


def erdbeben(c):
    """Das Erdbeben: ein großer Stein, über den sich von der Mitte aus Risse ziehen; er ist kurz davor zu zerspringen, Stücke heben ab"""
    c.background((0.1, 0.065, 0.025), (0.5, 0.56))
    for radius, width in ((0.42, 0.012), (0.47, 0.008)):
        c.tongue(arc((0.5, 0.56), radius, 200.0, 340.0, width), DUST, 0.05, streak=0.3, wobble=3.0)
    c.merge(DUST, ink=(0.02, 0.012, 0.006))
    c.rock([(0.08, 0.9), (0.1, 0.62), (0.24, 0.4), (0.42, 0.3), (0.62, 0.3), (0.8, 0.42), (0.92, 0.64), (0.92, 0.9), (0.5, 0.94)], 22, 0.0)
    # Die Risse laufen von der Mitte bis an den Rand, dicke Hauptrisse mit feinen Abzweigen
    middle = (0.5, 0.62)
    for end, width, turn in (((0.34, 0.31), 0.02, 0.5), ((0.68, 0.32), 0.02, -0.5), ((0.1, 0.56), 0.019, 0.7), ((0.92, 0.6), 0.019, -0.7),
                             ((0.2, 0.9), 0.019, -0.5), ((0.8, 0.9), 0.019, 0.5), ((0.5, 0.93), 0.016, 0.3)):
        c.crack(middle, end, width, turn)
    for shard, tone in (([(0.3, 0.17), (0.36, 0.13), (0.41, 0.2), (0.34, 0.25)], 0.04), ([(0.6, 0.14), (0.66, 0.11), (0.69, 0.19), (0.62, 0.21)], 0.0), ([(0.47, 0.08), (0.52, 0.05), (0.55, 0.12), (0.49, 0.15)], -0.04)):
        c.rock(shard, 4, tone)
    c.embers(DUST, 5, (0.2, 0.06, 0.8, 0.28))


def luft(c):
    """Das Element: drei Strömungen, die sich um eine ruhige Mitte einrollen"""
    c.background((0.05, 0.08, 0.12), (0.5, 0.5))
    middle = (0.5, 0.5)
    for start, radius, width, strength in ((10.0, 0.36, 0.05, 0.62), (130.0, 0.36, 0.055, 0.7), (250.0, 0.36, 0.05, 0.62), (70.0, 0.27, 0.03, 0.42), (190.0, 0.27, 0.03, 0.42), (310.0, 0.27, 0.03, 0.42)):
        c.wisp(curl(middle, radius, start, 0.72, width, shrink=0.78, end_width=0.008, count=11), strength)
    c.embers(AIR, 5, (0.16, 0.16, 0.86, 0.86))


def wind_band(c, origin, angle_deg, length, width, strength, hand):
    """Ein Band, das von einem Punkt ausgeht und sich am Ende einrollt"""
    angle = math.radians(angle_deg)
    end = (origin[0] + math.cos(angle) * length, origin[1] + math.sin(angle) * length)
    side = angle + hand * math.pi / 2.0
    center = (end[0] + math.cos(side) * 0.075, end[1] + math.sin(side) * 0.075)
    start = math.degrees(math.atan2(end[1] - center[1], end[0] - center[0]))
    middle = (origin[0] + math.cos(angle) * length * 0.55 - math.cos(side) * 0.03, origin[1] + math.sin(angle) * length * 0.55 - math.sin(side) * 0.03)
    tail = curl(center, 0.075, start, hand * 0.85, width, shrink=0.8, end_width=0.006, count=8)
    c.wisp([(*origin, 0.006), (*middle, width * 0.8)] + tail, strength)


def windstoss(c):
    """Der Windstoß: ein Kegel aus Strömungen, der von unten links ausgeht, sich öffnet und an den Enden einrollt"""
    c.background((0.05, 0.08, 0.12), (0.42, 0.6))
    origin = (0.13, 0.84)
    for angle, length, width, strength, hand in ((-82.0, 0.62, 0.03, 0.46, 1), (-12.0, 0.66, 0.03, 0.46, -1), (-66.0, 0.74, 0.042, 0.62, 1), (-28.0, 0.76, 0.042, 0.62, -1), (-47.0, 0.84, 0.05, 0.8, 1)):
        wind_band(c, origin, angle, length, width, strength, hand)
    c.embers(AIR, 6, (0.4, 0.14, 0.9, 0.7))


def boee(c):
    """Die Böe: ein gestreckter Sprint auf dem Wind, schnelle Linien dicht beieinander, am Ziel platzt die Luft in Bögen auf"""
    c.background((0.05, 0.08, 0.12), (0.72, 0.34))
    target = (0.74, 0.32)
    for pts, strength in (
        ([(0.04, 0.86, 0.008), (0.3, 0.74, 0.03), (0.54, 0.54, 0.042), (0.72, 0.34, 0.03)], 0.95),
        ([(0.1, 0.94, 0.004), (0.36, 0.8, 0.014), (0.6, 0.6, 0.018), (0.74, 0.44, 0.008)], 0.5),
        ([(0.04, 0.72, 0.004), (0.26, 0.62, 0.014), (0.46, 0.48, 0.018), (0.62, 0.32, 0.008)], 0.5),
        ([(0.2, 0.96, 0.003), (0.44, 0.82, 0.01), (0.66, 0.66, 0.012), (0.8, 0.5, 0.005)], 0.32),
        ([(0.03, 0.6, 0.003), (0.2, 0.52, 0.01), (0.38, 0.4, 0.012), (0.5, 0.26, 0.005)], 0.32),
    ):
        c.wisp(pts, strength)
    for radius, width, strength in ((0.11, 0.022, 0.8), (0.18, 0.018, 0.5), (0.25, 0.014, 0.3)):
        c.wisp(arc(target, radius, -130.0, 50.0, width, count=9), strength)
    c.embers(AIR, 6, (0.56, 0.1, 0.94, 0.56))


def wirbelsturm(c):
    """Der Wirbelsturm: ein Trichter aus liegenden Ringen, oben weit, unten dünn, er reißt Brocken mit"""
    c.background((0.05, 0.08, 0.12), (0.5, 0.44))
    levels = 11
    rings = []
    for k in range(levels):
        s = k / (levels - 1)
        y = 0.13 + 0.74 * s
        rings.append((0.5 + 0.035 * math.sin(s * 5.0), y, 0.035 + 0.31 * (1.0 - s) ** 1.5))
    # Hinten dunkel, damit der Trichter Tiefe hat
    for cx, y, r in rings:
        back = [(cx + r * math.cos(a), y - 0.2 * r * math.sin(a), 0.012 + r * 0.05) for a in np.radians(np.linspace(-20.0, 200.0, 9))]
        c.wisp(back, 0.22)
    c.wisp([(0.52, 0.14, 0.2), (0.49, 0.4, 0.11), (0.53, 0.66, 0.045), (0.5, 0.9, 0.012)], 0.2, core=0.15)
    for k, (cx, y, r) in enumerate(rings):
        front = [(cx + r * math.cos(a), y + 0.26 * r * math.sin(a), 0.014 + r * 0.06) for a in np.radians(np.linspace(-25.0, 205.0, 9))]
        c.wisp(front, 0.72 - 0.02 * k)
    for shard, tone in (([(0.16, 0.42), (0.21, 0.39), (0.23, 0.45), (0.18, 0.47)], -0.02), ([(0.78, 0.54), (0.82, 0.51), (0.85, 0.56), (0.8, 0.59)], -0.06), ([(0.3, 0.72), (0.34, 0.7), (0.35, 0.74), (0.31, 0.76)], -0.1)):
        c.rock(shard, 4, tone)
    c.embers(AIR, 6, (0.14, 0.16, 0.88, 0.8))


NID_INK = (0.016, 0.0, 0.012)


def vessel(c, start, angle_deg, length, width, depth=2, heat=0.0):
    """Ein Blutgefäß: es schlängelt sich, wird dünner und gabelt sich wie die Adern der Schlafenden"""
    points = [(start[0], start[1], width)]
    x, y = start
    angle = angle_deg
    steps = 5
    for i in range(1, steps + 1):
        angle += c.rng.uniform(-30.0, 30.0)
        x += math.cos(math.radians(angle)) * length / steps
        y += math.sin(math.radians(angle)) * length / steps
        points.append((x, y, width * (1.0 - 0.8 * i / steps) + 0.002))
    c.tongue(points, NID, heat, ink=0.0014, streak=0.85, wobble=7.0)
    if depth > 0:
        fork = points[2]
        for turn in (-1, 1):
            vessel(c, fork[:2], angle + turn * c.rng.uniform(26.0, 46.0), length * 0.55, fork[2] * 0.72, depth - 1, heat)


def drip(c, x, y0, y1, width):
    """Nid rinnt zäh über das Bild: eine lange, dünner werdende Spur mit dunklem Schleier daneben, kein Tropfen am Ende"""
    c.dust((x, y0), (x + 0.012, y1 + 0.04), width * 1.7, 0.55, (0.08, 0.002, 0.016))
    c.tongue([(x, y0, width), (x + 0.007, y0 + (y1 - y0) * 0.35, width * 0.55), (x - 0.004, y0 + (y1 - y0) * 0.7, width * 0.4), (x + 0.003, y1, 0.0012)], NID, 0.0, ink=0.0012, streak=0.7, wobble=3.0)


def nid(c):
    """Das Element: das Blut der Schlafenden, und sie sieht dich an. Ein Auge im Knoten, die Adern wachsen über den Ring hinaus, Blut rinnt herab"""
    c.background((0.05, 0.004, 0.06), (0.5, 0.46))
    c.nid_ground((0.5, 0.46))
    c.glow((0.5, 0.46), 0.26, (0.2, 0.02, 0.34), 0.34)
    middle = (0.5, 0.46)
    for angle, length, width in ((-118.0, 0.5, 0.03), (-52.0, 0.46, 0.024), (8.0, 0.52, 0.03), (152.0, 0.48, 0.026), (218.0, 0.44, 0.03)):
        vessel(c, (middle[0] + math.cos(math.radians(angle)) * 0.08, middle[1] + math.sin(math.radians(angle)) * 0.08), angle, length, width, depth=2)
    c.tongue([(0.42, 0.5, 0.1), (0.47, 0.46, 0.17), (0.53, 0.46, 0.17), (0.58, 0.42, 0.1)], NID, -0.1, ink=0.0015, streak=0.8, wobble=9.0)
    drip(c, 0.43, 0.56, 0.96, 0.017)
    drip(c, 0.55, 0.56, 0.82, 0.012)
    drip(c, 0.5, 0.58, 1.0, 0.021)
    c.merge(NID, ink=NID_INK, shift=-0.14, gloss=True)
    c.eye(middle, 0.135)
    c.spray((0.5, 0.46), 0.2, 60)
    c.oil()


def marksplitter(c):
    """Der Marksplitter: ein Dorn aus geronnenem Nid durchschlägt den Ring, nur von innen glimmt die Ader, hinter ihm hängt Blutnebel"""
    c.background((0.05, 0.004, 0.06), (0.62, 0.38))
    c.nid_ground((0.62, 0.38))
    c.glow((0.62, 0.38), 0.3, (0.2, 0.02, 0.34), 0.34)
    c.dust((0.46, 0.58), (0.05, 0.98), 0.085, 0.9, (0.13, 0.004, 0.03))
    c.dust((0.42, 0.62), (0.12, 0.78), 0.05, 0.6, (0.09, 0.003, 0.024))
    c.spray((0.34, 0.72), 0.17, 140, 135.0)
    c.rock([(0.97, 0.02), (0.72, 0.2), (0.52, 0.4), (0.33, 0.62), (0.42, 0.7), (0.6, 0.62), (0.78, 0.4)], 12, 0.1, stops=NID_ROCK, ink=(0.06, 0.012, 0.09))
    for pts, heat in (([(0.38, 0.65, 0.002), (0.54, 0.5, 0.012), (0.7, 0.33, 0.009), (0.93, 0.05, 0.002)], 0.34), ([(0.52, 0.5, 0.002), (0.6, 0.52, 0.006), (0.68, 0.46, 0.002)], 0.24), ([(0.48, 0.52, 0.002), (0.47, 0.44, 0.006), (0.55, 0.38, 0.002)], 0.24)):
        c.tongue(pts, NID, heat, ink=0.0, streak=0.6, wobble=4.0)
    c.merge(NID, rim_dark=False)
    c.oil()


def markstoss(c):
    """Der Markstoß: die Druckwelle zerreißt den Siegelring, Blut bricht unregelmäßig aus der Mitte, ein dichter Nebel aus Spritzern fliegt mit"""
    c.background((0.05, 0.004, 0.06), (0.5, 0.5), ring=False)
    c.nid_ground((0.5, 0.5))
    middle = (0.5, 0.5)
    c.glow(middle, 0.3, (0.2, 0.02, 0.34), 0.34)
    for turn, reach, width in ((-152.0, 0.3, 0.03), (-108.0, 0.42, 0.036), (-61.0, 0.24, 0.022), (-24.0, 0.38, 0.032), (19.0, 0.2, 0.02), (58.0, 0.44, 0.038), (101.0, 0.26, 0.024), (146.0, 0.36, 0.03), (188.0, 0.22, 0.02)):
        a = math.radians(turn)
        sway = c.rng.uniform(-0.05, 0.05)
        pts = [(0.5 + math.cos(a) * s * reach - math.sin(a) * sway * s, 0.5 + math.sin(a) * s * reach + math.cos(a) * sway * s, w * width) for s, w in ((0.2, 1.0), (0.55, 0.62), (0.82, 0.24), (1.0, 0.05))]
        c.tongue(pts, NID, 0.04, ink=0.0014, streak=0.9, wobble=9.0)
    c.tongue([(0.44, 0.54, 0.09), (0.48, 0.51, 0.13), (0.53, 0.48, 0.13), (0.57, 0.45, 0.08)], NID, -0.1, ink=0.0014, streak=0.8, wobble=9.0)
    c.merge(NID, ink=NID_INK, shift=-0.12, gloss=True)
    c.spray(middle, 0.26, 220)
    for start, end, push in ((-160.0, -100.0, 0.05), (-84.0, -30.0, 0.075), (-16.0, 48.0, 0.045), (62.0, 118.0, 0.08), (134.0, 196.0, 0.055)):
        c.ring_piece(middle, 0.4, start, end, push)
    c.eye(middle, 0.095)
    c.oil()


def offene_wunde(c):
    """Die Offene Wunde: die Welt reißt auf. Aus der Tiefe des gezackten Risses sieht das Auge heraus, Adern fressen den Fels"""
    c.background((0.05, 0.004, 0.06), (0.5, 0.5))
    c.nid_ground((0.5, 0.5))
    c.rock([(0.05, 0.5), (0.13, 0.2), (0.4, 0.06), (0.7, 0.07), (0.92, 0.26), (0.97, 0.58), (0.84, 0.84), (0.56, 0.96), (0.26, 0.9), (0.08, 0.74)], 20, -0.2, stops=NID_ROCK, ink=NID_INK)
    top = [(0.06, 0.62), (0.2, 0.5), (0.25, 0.53), (0.36, 0.33), (0.45, 0.38), (0.55, 0.25), (0.62, 0.32), (0.74, 0.28), (0.76, 0.36), (0.94, 0.3)]
    low = [(0.86, 0.42), (0.75, 0.56), (0.7, 0.53), (0.6, 0.72), (0.52, 0.66), (0.42, 0.78), (0.36, 0.7), (0.26, 0.74), (0.24, 0.66)]
    c.ink_shape(top + low, (0.004, 0.0, 0.007))
    c.glow((0.5, 0.5), 0.11, (0.16, 0.02, 0.26), 0.3)
    c.eye((0.5, 0.5), 0.115)
    for start, angle in (((0.36, 0.33), -120.0), ((0.55, 0.25), -80.0), ((0.74, 0.28), -40.0), ((0.2, 0.5), 200.0), ((0.86, 0.42), 20.0), ((0.6, 0.72), 70.0), ((0.42, 0.78), 110.0), ((0.26, 0.74), 150.0)):
        vessel(c, start, angle, 0.3, 0.013, depth=2)
    drip(c, 0.42, 0.78, 1.0, 0.014)
    drip(c, 0.6, 0.72, 0.95, 0.012)
    drip(c, 0.3, 0.74, 0.92, 0.009)
    c.merge(NID, ink=NID_INK, shift=-0.12, gloss=True)
    c.spray((0.5, 0.56), 0.26, 90)
    c.oil()


MOTIVES = {"Feuer": feuer, "Funke": funke, "Feuerball": feuerball, "Feuerstrahl": feuerstrahl, "Feuerwalze": feuerwalze,
           "Wasser": wasser, "Wassergeschoss": wassergeschoss, "Welle": welle, "Flutwelle": flutwelle,
           "Erde": erde, "Steinbrocken": steinbrocken, "Felswand": felswand, "Erdbeben": erdbeben,
           "Luft": luft, "Windstoss": windstoss, "Boee": boee, "Wirbelsturm": wirbelsturm,
           "Nid": nid, "Marksplitter": marksplitter, "Markstoss": markstoss, "OffeneWunde": offene_wunde}


def save(path, image):
    h, w = image.shape[:2]
    # Das Bild wird über vier Pixel gemittelt, so bleibt die Kontur ruhig
    small = image.reshape(h // 2, 2, w // 2, 2, 3).mean(axis=(1, 3)) if h == SIZE else image
    h, w = small.shape[:2]
    target = bpy.data.images.new(os.path.basename(path), w, h, alpha=True)
    rgba = np.ones((h, w, 4), np.float32)
    rgba[..., :3] = small ** (1.0 / 2.2)
    target.pixels.foreach_set(rgba[::-1].ravel())
    target.filepath_raw = path
    target.file_format = "PNG"
    target.save()
    print("[Vael] gemalt:", path)
    return small


def main(folder, wanted):
    os.makedirs(folder, exist_ok=True)
    painted = []
    for index, (name, motive) in enumerate(MOTIVES.items()):
        if wanted and name not in wanted:
            continue
        canvas = Canvas(SEED + index)
        motive(canvas)
        painted.append(save(os.path.join(folder, "T_Zauber_%s.png" % name), canvas.finish()))
    if len(painted) > 1:
        strip = np.concatenate(painted, axis=1)
        save(os.path.join(folder, "_Uebersicht.png"), strip)


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:]
    main(args[0], args[1:])
