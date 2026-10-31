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

    def background(self, glow, glow_at=(0.5, 0.5)):
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
        ring = np.hypot(u - 0.5 + 0.006 * self.warp_x, v - 0.5 + 0.006 * self.warp_y)
        line = np.exp(-((ring - 0.455) / 0.0052) ** 2) + 0.7 * np.exp(-((ring - 0.43) / 0.0032) ** 2)
        angle = np.arctan2(v - 0.5, u - 0.5)
        ticks = (np.abs(np.mod(angle / (math.tau / 24.0) + 0.5, 1.0) - 0.5) < 0.05) & (ring > 0.43) & (ring < 0.455)
        line = np.clip(line + ticks * 0.8, 0.0, 1.0) * (0.62 + 0.38 * self.middle)
        self.image = image + np.array((0.5, 0.56, 0.66))[None, None] * line[..., None] * 0.13
        return self

    def tongue(self, points, stops, heat=0.0, ink=0.0062, streak=0.52):
        """Eine Zunge: Tuschekontur, Verlauf von der Wurzel (heiß) zur Spitze (kalt), Pinselstriche längs, heller Kern in der Mitte"""
        line = smooth_line(points)
        wx = self.x + 9.0 * self.warp_x
        wy = self.y + 9.0 * self.warp_y
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

    def blurred_cover(self, sigma):
        """Die gemalte Fläche, weich gezeichnet"""
        spectrum = np.fft.fft2(self.cover.astype(np.float32))
        fx = np.fft.fftfreq(SIZE)[:, None]
        fy = np.fft.fftfreq(SIZE)[None, :]
        return np.fft.ifft2(spectrum * np.exp(-2.0 * (math.pi * sigma) ** 2 * (fx * fx + fy * fy))).real

    def merge(self, stops, rim_dark=True, shift=0.0, ink=(0.022, 0.006, 0.006)):
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


def wassergeschoss(c):
    """Das Wassergeschoss: eine Kugel aus Wasser, die einen Schweif aus Spritzern hinter sich herzieht"""
    c.background((0.02, 0.07, 0.2), (0.62, 0.4))
    for pts, heat in (
        ([(0.68, 0.33, 0.1), (0.48, 0.5, 0.075), (0.3, 0.64, 0.04), (0.08, 0.76, 0.002)], -0.12),
        ([(0.66, 0.36, 0.1), (0.52, 0.58, 0.07), (0.38, 0.74, 0.035), (0.22, 0.92, 0.002)], -0.12),
        ([(0.66, 0.34, 0.1), (0.46, 0.42, 0.06), (0.26, 0.42, 0.028), (0.1, 0.5, 0.002)], -0.16),
        ([(0.64, 0.33, 0.135), (0.68, 0.32, 0.165), (0.72, 0.31, 0.14), (0.76, 0.3, 0.05)], 0.1),
        ([(0.58, 0.44, 0.03), (0.68, 0.42, 0.075), (0.78, 0.34, 0.05), (0.76, 0.24, 0.002)], 0.26),
    ):
        c.tongue(pts, WATER, heat)
    c.merge(WATER, shift=-0.2, ink=(0.003, 0.012, 0.05))
    glints(c, ([(0.57, 0.3, 0.002), (0.61, 0.22, 0.016), (0.7, 0.19, 0.012), (0.77, 0.21, 0.002)],))
    c.embers(WATER, 7, (0.08, 0.3, 0.55, 0.92))


def welle(c):
    """Die Welle: ein breiter Kegel, der sich aufbäumt und oben in Schaum bricht"""
    c.background((0.02, 0.07, 0.2), (0.48, 0.6))
    for pts, heat in (
        ([(0.04, 0.82, 0.1), (0.3, 0.76, 0.13), (0.58, 0.62, 0.14), (0.76, 0.4, 0.09), (0.7, 0.22, 0.05), (0.52, 0.2, 0.002)], -0.02),
        ([(0.02, 0.92, 0.07), (0.34, 0.88, 0.09), (0.66, 0.82, 0.08), (0.94, 0.86, 0.002)], -0.06),
        ([(0.5, 0.72, 0.05), (0.64, 0.56, 0.06), (0.72, 0.4, 0.04), (0.66, 0.3, 0.002)], 0.3),
    ):
        c.tongue(pts, WATER, heat)
    c.merge(WATER, shift=-0.2, ink=(0.003, 0.012, 0.05))
    glints(c, ([(0.5, 0.2, 0.002), (0.62, 0.17, 0.02), (0.76, 0.24, 0.024), (0.83, 0.4, 0.014), (0.8, 0.5, 0.002)],
               [(0.62, 0.3, 0.002), (0.7, 0.3, 0.008), (0.74, 0.38, 0.002)], [(0.14, 0.74, 0.002), (0.3, 0.68, 0.009), (0.46, 0.62, 0.002)]))
    c.embers(WATER, 6, (0.42, 0.06, 0.92, 0.32))


def flutwelle(c):
    """Die Flutwelle: eine Wand aus Wasser, höher als alles, zwei Wellen übereinander, die sich nach rechts überschlagen"""
    c.background((0.02, 0.07, 0.2), (0.45, 0.5))
    for pts, heat in (
        ([(0.03, 0.94, 0.17), (0.22, 0.6, 0.2), (0.44, 0.3, 0.14), (0.68, 0.14, 0.08), (0.8, 0.22, 0.04), (0.72, 0.34, 0.002)], -0.08),
        ([(0.1, 0.96, 0.15), (0.4, 0.76, 0.17), (0.64, 0.52, 0.11), (0.86, 0.46, 0.05), (0.88, 0.58, 0.002)], 0.06),
        ([(0.02, 0.96, 0.09), (0.4, 0.92, 0.11), (0.72, 0.88, 0.08), (0.98, 0.9, 0.02)], 0.16),
    ):
        c.tongue(pts, WATER, heat)
    c.merge(WATER, shift=-0.2, ink=(0.003, 0.012, 0.05))
    glints(c, ([(0.5, 0.17, 0.002), (0.66, 0.08, 0.02), (0.82, 0.13, 0.026), (0.88, 0.27, 0.012), (0.83, 0.34, 0.002)],
               [(0.64, 0.46, 0.002), (0.78, 0.4, 0.016), (0.92, 0.46, 0.016), (0.93, 0.56, 0.002)],
               [(0.26, 0.52, 0.002), (0.36, 0.36, 0.01), (0.48, 0.24, 0.002)]))
    c.embers(WATER, 7, (0.5, 0.05, 0.96, 0.4))


MOTIVES = {"Feuer": feuer, "Funke": funke, "Feuerball": feuerball, "Feuerstrahl": feuerstrahl, "Feuerwalze": feuerwalze,
           "Wasser": wasser, "Wassergeschoss": wassergeschoss, "Welle": welle, "Flutwelle": flutwelle}


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
