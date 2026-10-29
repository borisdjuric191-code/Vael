"""Step 2: dumps the prepared Kelchreisser for measuring outside Blender: arbeit/analyse_roh.npz with

  punkte   every vertex (m, after step 1)
  farbe    mean base colour texel of each vertex (linear RGB)
  kanten   the mesh edges (vertex index pairs)
  normale  vertex normals

    blender -b --factory-startup -P Tools/Kelchreisser/02_analyse.py
"""
import os
import sys

import bpy
import numpy as np

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import vorschau  # noqa: E402


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
    obj = bpy.data.objects["Kelchreisser"]
    mesh = obj.data
    pixel = textur(obj)
    h, w = pixel.shape[:2]
    n = len(mesh.vertices)
    punkte = np.empty(n * 3)
    mesh.vertices.foreach_get("co", punkte)
    normale = np.empty(n * 3)
    mesh.vertices.foreach_get("normal", normale)
    kanten = np.empty(len(mesh.edges) * 2, dtype=np.int64)
    mesh.edges.foreach_get("vertices", kanten)
    schleifen = np.empty(len(mesh.loops), dtype=np.int64)
    mesh.loops.foreach_get("vertex_index", schleifen)
    uv = np.empty(len(mesh.loops) * 2)
    mesh.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    x = np.clip((uv[:, 0] * w).astype(int), 0, w - 1)
    y = np.clip((uv[:, 1] * h).astype(int), 0, h - 1)
    farbe = np.zeros((n, 3))
    np.add.at(farbe, schleifen, pixel[y, x, :3])
    farbe /= np.maximum(np.bincount(schleifen, minlength=n), 1)[:, None]
    np.savez(os.path.join(vorschau.ARBEIT, "analyse_roh.npz"), punkte=punkte.reshape(-1, 3), farbe=farbe,
             kanten=kanten.reshape(-1, 2), normale=normale.reshape(-1, 3))
    print("[Kelchreisser] %d Vertices, %d Kanten ausgelesen" % (n, len(mesh.edges)))


main()
