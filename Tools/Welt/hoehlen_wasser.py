"""
Real water for the caves instead of square tiles (Boris 2026-11-22: "Simple Formen wie das Wasser zu richtigem Wasser ausbauen statt
der viereckigen Form"). Reads the layout (Scripts/Data/Hoehlen/<Name>.json), blurs the water cells (~ shallow, O deep) into a smooth
field and fills it with marching squares at 0.5 m: one mesh with soft, round shores. The vertex colour (red) is how far a point
lies from the shore (0 at the edge, 1 in the deep middle): the material darkens the deep water with it.

Exports C:/Projekte/Vael_Quellen/Hoehlen/export/SM_HK_Wasser_<Name>.fbx in world centimetres of the cave (cell (r, c) at
X = (H-1-r)*S, Y = c*S; Y is written mirrored because Blender pieces arrive mirrored in Y in Unreal). BuildHoehle.py places it at
the origin.

    blender -b --factory-startup -P Tools/Welt/hoehlen_wasser.py -- <Name>
"""
import json
import os
import sys

import bmesh
import bpy
import numpy as np

DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Scripts", "Data", "Hoehlen")
EXPORT = "C:/Projekte/Vael_Quellen/Hoehlen/export"
FEIN = 8                 # samples per cell (4 m / 8 = 0.5 m)
SCHWELLE = 0.32          # a little beyond the water cells: the water laps over the edge of the dry floor and hides the square pits


def blur(a, radius):
    """Box blur in both directions, twice (close to a gaussian)"""
    for _ in range(2):
        for axis in (0, 1):
            pad = [(0, 0), (0, 0)]
            pad[axis] = (radius, radius)
            p = np.pad(a, pad, mode="edge")
            c = np.cumsum(p, axis=axis)
            c = np.concatenate([np.zeros_like(np.take(c, [0], axis=axis)), c], axis=axis)
            n = a.shape[axis]
            a = (np.take(c, range(2 * radius + 1, 2 * radius + 1 + n), axis=axis) - np.take(c, range(0, n), axis=axis)) / (2 * radius + 1)
    return a


def main():
    name = sys.argv[sys.argv.index("--") + 1]
    d = json.load(open(os.path.join(DATA, name + ".json"), encoding="utf-8"))
    grid = d["grundriss"]
    H, W = len(grid), max(len(z) for z in grid)
    S = d.get("zelle_m", 4.0) * 100.0
    mask = np.zeros((H, W))
    for r in range(H):
        for c, ch in enumerate(grid[r]):
            if ch in "~O":
                mask[r, c] = 1.0
    if mask.sum() == 0:
        print("[Wasser] {}: kein Wasser".format(name))
        return
    # fine field: every cell is FEIN x FEIN samples, then blurred over about one cell
    feld = np.kron(mask, np.ones((FEIN, FEIN)))
    feld = np.pad(feld, FEIN, mode="constant")
    feld = blur(feld, FEIN // 2 + 1)
    tief = blur((feld > 0.75).astype(float), FEIN)          # depth: far from the shore
    n_r, n_c = feld.shape
    step = S / FEIN

    def welt(i, j):
        # sample (i, j) sits at the centre of a sub-cell; the cell (r, c) centre is at sample (r*FEIN + FEIN/2 - 0.5 + FEIN)
        r = (i - FEIN - FEIN / 2 + 0.5) / FEIN
        c = (j - FEIN - FEIN / 2 + 0.5) / FEIN
        return (H - 1 - r) * S, c * S

    bm = bmesh.new()
    farbe = bm.loops.layers.color.new("Tiefe")
    for i in range(n_r - 1):
        for j in range(n_c - 1):
            ecken = [(i, j), (i, j + 1), (i + 1, j + 1), (i + 1, j)]
            werte = [feld[p] for p in ecken]
            if max(werte) < SCHWELLE:
                continue
            poly = []
            for k in range(4):
                a, b = ecken[k], ecken[(k + 1) % 4]
                va, vb = werte[k], werte[(k + 1) % 4]
                if va >= SCHWELLE:
                    poly.append((a[0], a[1], tief[a]))
                if (va >= SCHWELLE) != (vb >= SCHWELLE):
                    t = (SCHWELLE - va) / (vb - va)
                    poly.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, 0.0))
            if len(poly) < 3:
                continue
            verts = []
            for pi, pj, dpt in poly:
                x, y = welt(pi, pj)
                verts.append((bm.verts.new((x / 100.0, -y / 100.0, 0.0)), dpt))
            try:
                face = bm.faces.new([v for v, _ in verts])
            except ValueError:
                continue
            for loop, (_, dpt) in zip(face.loops, verts):
                loop[farbe] = (min(1.0, dpt), 0.0, 0.0, 1.0)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.002)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    for f in bm.faces:
        if f.normal.z < 0:
            f.normal_flip()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    me = bpy.data.meshes.new("SM_HK_Wasser_" + name)
    bm.to_mesh(me)
    bm.free()
    # UVs in metres of the world, for the ripples
    uv = me.uv_layers.new(name="UV")
    for loop in me.loops:
        co = me.vertices[loop.vertex_index].co
        uv.data[loop.index].uv = (co.x / 4.0, co.y / 4.0)
    obj = bpy.data.objects.new("SM_HK_Wasser_" + name, me)
    bpy.context.scene.collection.objects.link(obj)
    for o in bpy.context.scene.objects:
        o.select_set(o == obj)
    bpy.context.view_layer.objects.active = obj
    os.makedirs(EXPORT, exist_ok=True)
    pfad = os.path.join(EXPORT, "SM_HK_Wasser_{}.fbx".format(name))
    # the same settings as the other cave pieces (Tools/Welt/eisenschlund_fels.py)
    bpy.ops.export_scene.fbx(filepath=pfad, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_ALL", bake_anim=False, mesh_smooth_type="FACE")
    print("[Wasser] {}: {} Dreiecke -> {}".format(name, len(me.polygons), pfad))


main()
