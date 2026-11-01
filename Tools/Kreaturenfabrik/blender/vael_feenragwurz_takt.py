"""Prepares the Feenragwurz and her fairy for their movement without a skeleton.

Run it after every 'requisite' step (that one exports the FBX without the map). It reads <Ordner>/04_export/SM_<Name>.fbx
(origin bottom centre, metres) and exports it again with a second UV map "Takt"; floats in UVs, unlike vertex colours,
arrive in Unreal unchanged. The positions below were measured on the 2026-11-01 Tripo models.

    fee:      U = wing (0 at the root, 1 from a few cm out), V = leg (0 at the hip, 1 towards the foot)
    orchidee: U = phase of the blossom, V = how much of the blossom's nodding this vertex takes
              and writes 03_analyse/blueten.json: blossom middles and looks in Unreal space (cm, Y mirrored as the FBX
              import does), read by Scripts/BuildFeenragwurz.py for the places the fairies fly to

    blender -b --factory-startup -P vael_feenragwurz_takt.py -- fee <Ordner>
    blender -b --factory-startup -P vael_feenragwurz_takt.py -- orchidee <Ordner>
"""
import json
import os
import sys

import bpy
import numpy as np

# Fairy (looks along -Y): the wings stand on one flat plane per side, y = FLUEGEL_Y + FLUEGEL_NEIGUNG * |x|
FLUEGEL_Y = -0.027
FLUEGEL_NEIGUNG = 0.46
FLUEGEL_DICKE = 0.016    # m around the plane that belong to a wing
FLUEGEL_WOELBUNG = (0.08, 0.16, 0.05)  # the wing tips curve away: from |x| to |x| the band grows by this much
FLUEGEL_INNEN = 0.03     # m from the middle: the wing root
FLUEGEL_WEICH = 0.04     # m over which the wing takes up the full beat
FLUEGEL_Z = (0.26, 0.58)
HAAR = (0.49, 0.125)     # above this height and within this |x| the fine hair stands close to the plane
HUEFTE_Z = 0.31          # m: the legs bend from here
BEIN_WEICH = 0.13        # m below the hip at which a leg takes up the full swing
BEIN_BREITE = 0.06       # m, |x| of the legs; further out hang the arms

# Orchid, read off the front view (x, z in m)
# third value: stretches the distance to that blossom, so a big petal of its neighbour below stays with its own blossom
BLUETEN = ((-0.36, 1.21, 1.0), (0.30, 1.40, 1.0), (-0.43, 1.89, 2.6), (0.32, 1.93, 1.6), (-0.55, 0.82, 1.0), (0.416, 0.95, 1.0))
BLUETE_RADIUS = 0.30     # m around the middle in the front view
STAENGEL = ((0.0, 0.27), (0.02, 0.80), (0.0, 1.18), (-0.04, 1.63), (-0.02, 1.86))  # the main stem stays still
NICKEN_AB = 0.06         # m beside the main stem where the nodding starts
NICKEN_VOLL = 0.36       # m beside the main stem where a blossom nods fully
NICKEN_Z = (0.50, 0.60)  # the leaves and the moss below stay still
SAAT = 11


def unreal(p):
    return [round(float(p[0]) * 100, 1), round(float(-p[1]) * 100, 1), round(float(p[2]) * 100, 1)]


def laden(ordner):
    with open(os.path.join(ordner, "requisite.json"), encoding="utf-8") as f:
        name = json.load(f)["name"]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=os.path.join(ordner, "04_export", f"SM_{name}.fbx"))
    obj = next(o for o in bpy.data.objects if o.type == "MESH")
    return name, obj, np.array([obj.matrix_world @ v.co for v in obj.data.vertices])


def speichern(ordner, name, obj, u, v):
    mesh = obj.data
    takt = mesh.uv_layers.get("Takt") or mesh.uv_layers.new(name="Takt")
    for schleife in mesh.loops:
        takt.data[schleife.index].uv = (u[schleife.vertex_index], v[schleife.vertex_index])
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


def fee(ordner):
    name, obj, punkte = laden(ordner)
    x, y, z = punkte[:, 0], punkte[:, 1], punkte[:, 2]
    aussen = np.abs(x)
    woelbung = np.clip((aussen - FLUEGEL_WOELBUNG[0]) / (FLUEGEL_WOELBUNG[1] - FLUEGEL_WOELBUNG[0]), 0.0, 1.0) * FLUEGEL_WOELBUNG[2]
    auf_ebene = np.abs(y - (FLUEGEL_Y + FLUEGEL_NEIGUNG * aussen)) < FLUEGEL_DICKE + woelbung
    haar = (z > HAAR[0]) & (aussen < HAAR[1])
    fluegel = auf_ebene & (aussen > FLUEGEL_INNEN) & (z > FLUEGEL_Z[0]) & (z < FLUEGEL_Z[1]) & ~haar
    u = np.where(fluegel, np.clip((aussen - FLUEGEL_INNEN) / FLUEGEL_WEICH, 0.0, 1.0), 0.0)
    bein = (aussen < BEIN_BREITE) & (z < HUEFTE_Z)
    v = np.where(bein, np.clip((HUEFTE_Z - z) / BEIN_WEICH, 0.0, 1.0), 0.0)
    speichern(ordner, name, obj, u, v)
    print("[takt] {} wing and {} leg vertices of {}".format(int(fluegel.sum()), int(bein.sum()), len(punkte)))


def abstand_zum_staengel(x, z):
    """Distance in the front view from each vertex to the main stem"""
    beste = np.full(len(x), 1e9)
    for (ax, az), (bx, bz) in zip(STAENGEL, STAENGEL[1:]):
        dx, dz = bx - ax, bz - az
        t = np.clip(((x - ax) * dx + (z - az) * dz) / (dx * dx + dz * dz), 0.0, 1.0)
        beste = np.minimum(beste, np.hypot(x - ax - t * dx, z - az - t * dz))
    return beste


def orchidee(ordner):
    name, obj, punkte = laden(ordner)
    x, y, z = punkte[:, 0], punkte[:, 1], punkte[:, 2]
    mitten = np.array(BLUETEN)
    abstand = np.hypot(x[:, None] - mitten[:, 0], z[:, None] - mitten[:, 1])
    naechste = (abstand * mitten[:, 2]).argmin(axis=1)
    phasen = np.random.default_rng(SAAT).random(len(BLUETEN))
    u = phasen[naechste]
    v = np.clip((abstand_zum_staengel(x, z) - NICKEN_AB) / (NICKEN_VOLL - NICKEN_AB), 0.0, 1.0)
    v = v * np.clip((z - NICKEN_Z[0]) / (NICKEN_Z[1] - NICKEN_Z[0]), 0.0, 1.0)

    blueten = []
    for index in range(len(BLUETEN)):
        teil = punkte[(naechste == index) & (abstand[np.arange(len(x)), naechste] < BLUETE_RADIUS) & (z > NICKEN_Z[0])]
        mitte = teil.mean(axis=0)
        # All blossoms open to the front (-Y), a little away from the stem
        blick = np.array([0.35 * np.sign(mitte[0]), -1.0, 0.0])
        blick = blick / np.linalg.norm(blick)
        blueten.append({"ort": unreal(mitte), "blick": [round(float(blick[0]), 3), round(float(-blick[1]), 3), 0.0],
                        "punkte": len(teil)})
    speichern(ordner, name, obj, u, v)
    os.makedirs(os.path.join(ordner, "03_analyse"), exist_ok=True)
    with open(os.path.join(ordner, "03_analyse", "blueten.json"), "w", encoding="utf-8") as f:
        json.dump({"hoehe_cm": round(float(z.max()) * 100, 1), "blueten": blueten}, f, indent=1)
    print("[takt] {} blossoms: {}".format(len(blueten), [(b["ort"], b["punkte"]) for b in blueten]))


if __name__ == "__main__":
    argumente = sys.argv[sys.argv.index("--") + 1:]
    {"fee": fee, "orchidee": orchidee}[argumente[0]](argumente[1])
