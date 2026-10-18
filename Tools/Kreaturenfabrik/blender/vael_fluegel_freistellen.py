"""
Vael – Kreaturenfabrik, Flügel freistellen (Blender, ohne Oberfläche)
====================================================================

Schneidet aus einem Tripo-Bild eines Flügelpaars (von hinten, auf hellgrauem Grund) den rechten Flügel aus und gibt ihm
einen Alphakanal: wo das Bild dem Hintergrund gleicht (Rand, Löcher in der Haut), wird es durchsichtig.
Den linken Flügel spiegelt das Spiel. Der Ansatz des Flügels liegt am linken Bildrand auf halber Höhe des Ansatzes.

Aufruf:
  blender -b -P vael_fluegel_freistellen.py -- <bild.png> <ziel.png> <x0> <y0> <x1> <y1>
x0 y0 x1 y1: Ausschnitt in Pixeln des Quellbilds, von oben links gezählt.
"""
import sys
import bpy
import numpy


def main(quelle, ziel, x0, y0, x1, y1):
    bild = bpy.data.images.load(quelle)
    w, h = bild.size
    px = numpy.empty(w * h * 4, dtype=numpy.float32)
    bild.pixels.foreach_get(px)
    # Blender zählt die Zeilen von unten
    px = px.reshape(h, w, 4)[::-1]

    # Hintergrund je Zeile: der Mittelwert der äußersten Spalten, denn der Grund wird nach unten etwas dunkler
    rand = numpy.concatenate([px[:, :24, :3], px[:, -24:, :3]], axis=1)
    grund = numpy.median(rand, axis=1)[:, None, :]

    teil = px[y0:y1, x0:x1, :3]
    abstand = numpy.abs(teil - grund[y0:y1]).max(axis=2)
    # Weich von 0,03 bis 0,12 Abstand: Ränder bleiben glatt, kaum sichtbares Grau wird ganz durchsichtig
    alpha = numpy.clip((abstand - 0.03) / 0.09, 0.0, 1.0)
    alpha = alpha * alpha * (3.0 - 2.0 * alpha)

    aus = numpy.dstack([teil, alpha]).astype(numpy.float32)
    th, tw = aus.shape[:2]
    neu = bpy.data.images.new("Fluegel", tw, th, alpha=True)
    neu.pixels.foreach_set(aus[::-1].ravel())
    neu.filepath_raw = ziel
    neu.file_format = "PNG"
    neu.save()
    print(f"[Vael] Flügel freigestellt: {tw}x{th} -> {ziel}")


if __name__ == "__main__":
    a = sys.argv[sys.argv.index("--") + 1:]
    main(a[0], a[1], *[int(v) for v in a[2:6]])
