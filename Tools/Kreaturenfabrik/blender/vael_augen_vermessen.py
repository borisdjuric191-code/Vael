"""Finds the eyes on a requisite covered with small eyes (Wachbeere and the like).

Reads <Ordner>/04_export/SM_<Name>.fbx (origin bottom centre, metres, from the 'requisite' step) and its base colour
texture. Eye whites are bright, barely saturated texels; berries are strongly red. Vertices on such texels are grouped
into eyes and berries (texels close in space belong to one); an eye looks along the mean surface normal of its
vertices and belongs to the nearest berry. Writes <Ordner>/03_analyse/augen.json with eye positions, look directions
and blink phases in Unreal space (cm, Y mirrored as the FBX import does), the berry middles and the height of the
mesh, and exports 04_export/SM_<Name>.fbx again with a second UV map "Takt" (U = blink phase, V = lid zone).
Run it after every 'requisite' step (that one exports the FBX without the map).

    blender -b --factory-startup -P vael_augen_vermessen.py -- <Ordner>
"""
import json
import os
import sys

import bpy
import numpy as np

WEISS_HELL = 0.62     # darkest channel of an eye white at least this bright
WEISS_SATT = 0.25     # and saturation below this
AUGE_RADIUS = 0.06    # m: eye-white vertices closer than this form one eye
BEERE_RADIUS = 0.14   # m: red vertices closer than this form one berry
MIN_AUGE = 2          # vertices an eye needs at least
MIN_BEERE = 25        # vertices a berry needs at least
LID_RADIUS = 0.07     # m around an eye's middle that its lid covers when it closes
SAAT = 7              # seed for the blink phases


def gruppen(punkte, radius):
    """Groups points that lie within radius of each other (flood fill over a grid)"""
    zelle = {}
    schluessel = np.floor(punkte / radius).astype(int)
    for i, k in enumerate(map(tuple, schluessel)):
        zelle.setdefault(k, []).append(i)
    frei = np.ones(len(punkte), dtype=bool)
    ergebnis = []
    for start in range(len(punkte)):
        if not frei[start]:
            continue
        frei[start] = False
        offen, gruppe = [start], []
        while offen:
            i = offen.pop()
            gruppe.append(i)
            kx, ky, kz = schluessel[i]
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    for dz in (-1, 0, 1):
                        for j in zelle.get((kx + dx, ky + dy, kz + dz), ()):
                            if frei[j] and np.sum((punkte[j] - punkte[i]) ** 2) < radius * radius:
                                frei[j] = False
                                offen.append(j)
        ergebnis.append(gruppe)
    return ergebnis


def unreal(p):
    return [round(float(p[0]) * 100, 1), round(float(-p[1]) * 100, 1), round(float(p[2]) * 100, 1)]


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
    drehung = obj.matrix_world.to_3x3()
    normalen = np.array([(drehung @ v.normal).normalized() for v in mesh.vertices])
    weiss = np.zeros(len(mesh.vertices), dtype=bool)
    rot = np.zeros(len(mesh.vertices), dtype=bool)
    for schleife in mesh.loops:
        u, v = uv[schleife.index].uv
        x = min(max(int(u * w), 0), w - 1)
        y = min(max(int(v * h), 0), h - 1)
        r, g, b = pixel[y, x, :3]
        hell, dunkel = max(r, g, b), min(r, g, b)
        if dunkel > WEISS_HELL and (hell - dunkel) / (hell + 1e-5) < WEISS_SATT:
            weiss[schleife.vertex_index] = True
        if r > 0.45 and r > g * 2.2 and r > b * 1.4:
            rot[schleife.vertex_index] = True

    beeren = []
    rote = punkte[rot]
    for gruppe in gruppen(rote, BEERE_RADIUS):
        if len(gruppe) >= MIN_BEERE:
            beeren.append(rote[gruppe].mean(axis=0))
    if not beeren:
        raise RuntimeError("no berries found")
    beeren = np.array(beeren)

    augen = []
    weisse = punkte[weiss]
    weisse_normalen = normalen[weiss]
    for gruppe in gruppen(weisse, AUGE_RADIUS):
        if len(gruppe) < MIN_AUGE:
            continue
        mitte = weisse[gruppe].mean(axis=0)
        abstand = np.linalg.norm(beeren - mitte, axis=1)
        beere = int(abstand.argmin())
        # Eyes far from every berry are stray white texels (dew drops on leaves and the like)
        if abstand[beere] > 0.35:
            continue
        blick = weisse_normalen[gruppe].sum(axis=0)
        laenge = np.linalg.norm(blick)
        blick = blick / laenge if laenge > 1e-4 else np.array([0.0, 0.0, -1.0])
        augen.append({"ort": unreal(mitte), "blick": [round(float(blick[0]), 3), round(float(-blick[1]), 3), round(float(blick[2]), 3)],
                      "beere": beere})

    # Blink rhythm: every eye its own phase, written into a second UV map "Takt" (U = phase, V = 1 on the eye and
    # its lid, fading out around it), so the material blinks each eye on its own; floats in UVs, unlike vertex
    # colours, arrive in Unreal unchanged. The light beams read the same phase from augen.json.
    zufall = np.random.default_rng(SAAT)
    mitten = np.array([[a["ort"][0] / 100, -a["ort"][1] / 100, a["ort"][2] / 100] for a in augen])
    for auge in augen:
        auge["takt"] = round(float(zufall.random()), 3)
    takt = mesh.uv_layers.get("Takt") or mesh.uv_layers.new(name="Takt")
    zone = np.zeros(len(mesh.vertices))
    phase = np.zeros(len(mesh.vertices))
    if len(augen):
        abstand = np.linalg.norm(punkte[:, None, :] - mitten[None, :, :], axis=2)
        naechstes = abstand.argmin(axis=1)
        zone = np.clip((LID_RADIUS - abstand[np.arange(len(punkte)), naechstes]) / (LID_RADIUS * 0.4), 0.0, 1.0)
        phase = np.array([augen[i]["takt"] for i in naechstes])
    for schleife in mesh.loops:
        takt.data[schleife.index].uv = (phase[schleife.vertex_index], zone[schleife.vertex_index])
    mesh.uv_layers.active_index = 0
    for ebene in mesh.uv_layers:
        ebene.active_render = ebene.name != "Takt"
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=os.path.join(ordner, "04_export", f"SM_{name}.fbx"), use_selection=True, object_types={"MESH"},
        apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
        bake_anim=False, path_mode="COPY", embed_textures=True, mesh_smooth_type="FACE",
    )

    daten = {
        "hoehe_cm": round(float(punkte[:, 2].max()) * 100, 1),
        "beeren": [unreal(b) for b in beeren],
        "augen": augen,
    }
    os.makedirs(os.path.join(ordner, "03_analyse"), exist_ok=True)
    with open(os.path.join(ordner, "03_analyse", "augen.json"), "w", encoding="utf-8") as f:
        json.dump(daten, f, indent=1)
    print("[augen] {} berries, {} eyes, {} cm high".format(len(beeren), len(augen), daten["hoehe_cm"]))


if __name__ == "__main__":
    main(sys.argv[sys.argv.index("--") + 1])
