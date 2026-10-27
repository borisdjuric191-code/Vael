"""Measures where a requisite with a glowing face looks (Flüsterkappe and the like).

Reads <Ordner>/04_export/SM_<Name>.fbx (origin bottom centre, metres, from the 'requisite' step) and its base colour
texture, finds the strongly blue or cyan texels (eyes and mouth, the pale skin around them is barely saturated) and
writes <Ordner>/03_analyse/gesicht.json: the direction the face looks (yaw in Blender and in Unreal), its height, the
height where the cap starts (lowest point of the cap rim, for the breathing in the material) and the stem radius.

    blender -b --factory-startup -P vael_gesicht_vermessen.py -- <Ordner>
"""
import json
import math
import os
import sys

import bpy
import numpy as np

SCHWELLE = 0.30   # blue minus red above this counts as eye or mouth


def main(ordner):
    with open(os.path.join(ordner, "requisite.json"), encoding="utf-8") as f:
        name = json.load(f)["name"]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=os.path.join(ordner, "04_export", f"SM_{name}.fbx"))
    obj = next(o for o in bpy.data.objects if o.type == "MESH")
    mesh = obj.data

    bild = None
    for slot in obj.material_slots:
        for node in (slot.material.node_tree.nodes if slot.material and slot.material.node_tree else []):
            if node.type == "TEX_IMAGE" and node.image and "basecolor" in node.image.name.lower():
                bild = node.image
    if bild is None:
        raise RuntimeError("no base colour texture found")
    w, h = bild.size
    pixel = np.empty(w * h * 4, dtype=np.float32)
    bild.pixels.foreach_get(pixel)
    pixel = pixel.reshape(h, w, 4)

    uv = mesh.uv_layers.active.data
    punkte = np.array([obj.matrix_world @ v.co for v in mesh.vertices])
    treffer = np.zeros(len(mesh.vertices), dtype=bool)
    for schleife in mesh.loops:
        u, v = uv[schleife.index].uv
        x = min(max(int(u * w), 0), w - 1)
        y = min(max(int(v * h), 0), h - 1)
        r, g, b = pixel[y, x, :3]
        if b - r > SCHWELLE:
            treffer[schleife.vertex_index] = True

    hoehe = punkte[:, 2].max()
    # The face sits on the cap: ignore blue texels low on the stem
    gesicht = punkte[treffer & (punkte[:, 2] > hoehe * 0.45)]
    if len(gesicht) == 0:
        raise RuntimeError("no face texels found")
    mitte = gesicht.mean(axis=0)
    yaw_blender = math.degrees(math.atan2(mitte[1], mitte[0]))

    # Stem radius from a slice at a third of the height; cap starts where the slices get much wider than that
    def radius(z0, z1):
        lage = punkte[(punkte[:, 2] >= z0) & (punkte[:, 2] < z1)]
        if len(lage) == 0:
            return 0.0
        m = lage[:, :2].mean(axis=0)
        return float(np.percentile(np.linalg.norm(lage[:, :2] - m, axis=1), 90))

    band = hoehe / 40.0
    stiel = radius(hoehe * 0.3, hoehe * 0.3 + band)
    hut_unten = hoehe * 0.5
    for i in range(12, 40):
        if radius(i * band, (i + 1) * band) > stiel * 3.0:
            hut_unten = i * band
            break

    daten = {
        "hoehe_cm": round(float(hoehe) * 100, 1),
        "hut_unterseite_cm": round(float(hut_unten) * 100, 1),
        "stiel_radius_cm": round(stiel * 100, 1),
        "gesicht_hoehe_cm": round(float(mitte[2]) * 100, 1),
        "gesicht_abstand_cm": round(float(math.hypot(mitte[0], mitte[1])) * 100, 1),
        "gesicht_yaw_blender": round(yaw_blender, 1),
        # FBX import into Unreal mirrors Y: a direction at +a degrees in Blender points to -a degrees in Unreal
        "gesicht_yaw_unreal": round(-yaw_blender, 1),
        "gesicht_texel_punkte": int(len(gesicht)),
    }
    os.makedirs(os.path.join(ordner, "03_analyse"), exist_ok=True)
    with open(os.path.join(ordner, "03_analyse", "gesicht.json"), "w", encoding="utf-8") as f:
        json.dump(daten, f, indent=2)
    print("[gesicht]", json.dumps(daten))


if __name__ == "__main__":
    main(sys.argv[sys.argv.index("--") + 1])
