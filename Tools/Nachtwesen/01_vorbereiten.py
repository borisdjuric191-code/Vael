"""Step 1: prepares one of the night creatures (budget, facing +X, size, origin) and renders measuring views
(front, side, top; orthographic, picture centre and width are printed so joints can be read off).

    blender -b --factory-startup -P Tools/Nachtwesen/01_vorbereiten.py -- <Name>
"""
import os
import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import gemein  # noqa: E402

# name: triangles, turn round Z so it looks to +X, height or length in metres
WESEN = {
    "Moderglocke": dict(dreiecke=30000, drehung_z=90.0, hoehe=2.7),
    "Kahlschwinge": dict(dreiecke=30000, drehung_z=90.0, hoehe=1.9),
    "Peitschenunke": dict(dreiecke=26000, drehung_z=90.0, laenge=1.7),
    "Schnappwurz": dict(dreiecke=32000, drehung_z=0.0, hoehe=2.4),
}


def main():
    name = sys.argv[sys.argv.index("--") + 1]
    obj = gemein.laden(name, **WESEN[name])
    lo, hi = gemein.grenzen(obj)
    g = hi - lo
    gemein.speichern(name, "01_vorbereitet.blend")
    weit = max(g) * 1.1
    mitte = (0.0, 0.0, g.z / 2)
    gemein.log(name, "Messbilder: Mitte %s, Breite %.3f m" % (mitte, weit))
    gemein.ansichten(name, "01", mitte, weit, ("vorne", "seite", "oben", "spiel"), 800)


main()
