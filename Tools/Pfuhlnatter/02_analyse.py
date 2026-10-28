"""Step 2: measures the Pfuhlnatter for its skeleton and writes arbeit/analyse.npz:

  punkte   every vertex (m, after step 1)
  flosse   True for vertices on fin membranes: pale, little saturated texels (the body is crimson, black-petrol,
           turquoise and sulfur yellow, all far darker or far more saturated)
  linie    the centre line of the body from the tail root to the snout, evenly spaced: along X for the part lying
           on the ground, along Z where the neck rears up, then forward to the snout
  radius   body radius at each point of the line

Renders vorschau/02_analyse_*.png (fins pink, body grey, centre line red) with matplotlib from the venv in
C:/Users/boris/Tools/wf_generator_venv (run 02_analyse_bild.py after this).

    blender -b --factory-startup -P Tools/Pfuhlnatter/02_analyse.py
"""
import os
import sys

import bpy
import numpy as np

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import vorschau  # noqa: E402

FLOSSE_HELL = 0.30   # darkest channel of a fin texel at least this bright


def textur(obj):
    for slot in obj.material_slots:
        for node in (slot.material.node_tree.nodes if slot.material and slot.material.node_tree else []):
            if node.type == "TEX_IMAGE" and node.image and "basecolor" in node.image.name.lower():
                w, h = node.image.size
                pixel = np.empty(w * h * 4, dtype=np.float32)
                node.image.pixels.foreach_get(pixel)
                return pixel.reshape(h, w, 4)
    raise RuntimeError("no base colour texture")


def main():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(vorschau.ARBEIT, "01_vorbereitet.blend"))
    obj = bpy.data.objects["Pfuhlnatter"]
    mesh = obj.data
    pixel = textur(obj)
    h, w = pixel.shape[:2]
    punkte = np.array([v.co[:] for v in mesh.vertices])
    farbe = np.zeros((len(punkte), 3))
    anzahl = np.zeros(len(punkte))
    uv = mesh.uv_layers.active.data
    for schleife in mesh.loops:
        u, v = uv[schleife.index].uv
        x = min(max(int(u * w), 0), w - 1)
        y = min(max(int(v * h), 0), h - 1)
        farbe[schleife.vertex_index] += pixel[y, x, :3]
        anzahl[schleife.vertex_index] += 1
    farbe /= np.maximum(anzahl, 1)[:, None]
    flosse = farbe.min(axis=1) > FLOSSE_HELL
    print("[Pfuhlnatter] Flossen-Vertices: %d von %d" % (flosse.sum(), len(punkte)))
    np.savez(os.path.join(vorschau.ARBEIT, "analyse_roh.npz"), punkte=punkte, flosse=flosse, farbe=farbe)


main()
