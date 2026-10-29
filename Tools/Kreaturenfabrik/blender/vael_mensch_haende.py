"""
Vael – Kreaturenfabrik, Menschen: Hände (Blender)
=================================================

Die Hände der Tripo-Modelle sind flache Klumpen ohne getrennte Finger; daran lässt sich keine Fingerpose biegen.
Dieses Skript ersetzt sie durch die Hände des Unreal-Mannequins (gleiches Skelett, jeder Finger schon gewichtet):
  1) Öffnet 03_blender/<Name>.blend (aus vael_mensch_aufbereiten.py).
  2) Schneidet am Körper die Haut hinter dem Handgelenk weg (Höhe und Farbe: der blaue Ärmel bleibt).
  3) Nimmt aus SKM_Manny_Simple (LOD1) alle Flächen, die an Hand und Fingern hängen, als eigenes Teil.
  4) Färbt sie in der mittleren Hautfarbe der weggeschnittenen Tripo-Hände (eine kleine Textur T_<Name>_Haut).
  5) Exportiert 04_export/SK_<Name>_Koerper.fbx neu und SK_<Name>_Haende.fbx.

Aufruf (nach vael_mensch_aufbereiten.py):
  blender -b -P vael_mensch_haende.py -- "C:\\Projekte\\Vael_Quellen\\MagierMann"
"""
import bpy, bmesh, json, os, sys
from mathutils import Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from vael_mensch_aufbereiten import CM, SKELETT, aktiv, exportiere, farbe_lesen, log

# Knochen der Hand: was überwiegend an ihnen hängt, gehört zur Mannequin-Hand
HANDKNOCHEN = ("hand_", "index_", "middle_", "ring_", "pinky_", "thumb_")

# So weit vor dem Handgelenk (Richtung Ellbogen) wird Tripo-Haut gelöscht: der Ansatz der alten Hand steht sonst als Zacken aus dem Ärmel
SCHNITT_VOR_GELENK = .12 * CM


def handgelenke(arm):
    """Je Seite: Gelenkpunkt und Richtung vom Ellbogen zur Hand (Ruhepose)."""
    erg = {}
    for seite in ("l", "r"):
        gelenk = arm.data.bones[f"hand_{seite}"].head_local.copy()
        erg[seite] = (gelenk, (gelenk - arm.data.bones[f"lowerarm_{seite}"].head_local).normalized())
    return erg


def schneide_haende(koerper, arm):
    """Löscht die Haut hinter dem Handgelenk und gibt die mittlere Farbe der gelöschten Flächen zurück."""
    farbe = farbe_lesen(koerper)
    gelenke = handgelenke(arm)
    aktiv(koerper)
    bpy.ops.object.mode_set(mode="EDIT")
    bm = bmesh.from_edit_mesh(koerper.data)
    uv = bm.loops.layers.uv.active
    weg, farben = [], []
    for f in bm.faces:
        p = koerper.matrix_world @ f.calc_center_median()
        for gelenk, richtung in gelenke.values():
            if (p - gelenk).dot(richtung) < -SCHNITT_VOR_GELENK or (p - gelenk).length > .3 * CM:
                continue
            if farbe is not None and uv is not None:
                mitte = sum((l[uv].uv for l in f.loops), Vector((0, 0))) / len(f.loops)
                r, g, b = farbe(mitte)
                if b >= r * 1.08:       # blauer Ärmel bleibt
                    continue
                farben.append(Vector((r, g, b)))
            weg.append(f)
            break
    bmesh.ops.delete(bm, geom=weg, context="FACES")
    bmesh.update_edit_mesh(koerper.data)
    bpy.ops.object.mode_set(mode="OBJECT")
    haut = sum(farben, Vector()) / len(farben) if farben else Vector((.62, .45, .36))
    log(f"Tripo-Hände entfernt: {len(weg)} Flächen, Hautfarbe {[round(c, 3) for c in haut]}")
    return haut


def manny_haende(arm, name):
    """Holt die Hände aus dem Mannequin (LOD1) als eigenes Objekt am Skelett."""
    vorher = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=SKELETT, automatic_bone_orientation=False)
    neu = [o for o in bpy.data.objects if o not in vorher]
    hand = next(o for o in neu if o.type == "MESH" and o.name.endswith("_LOD1"))
    welt = hand.matrix_world.copy()
    hand.parent = None
    hand.matrix_world = welt
    for o in neu:
        if o is not hand:
            bpy.data.objects.remove(o, do_unlink=True)
    for mod in list(hand.modifiers):
        hand.modifiers.remove(mod)
    aktiv(hand)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    namen = {g.index: g.name for g in hand.vertex_groups}

    def an_der_hand(v):
        if not v.groups:
            return False
        stark = max(v.groups, key=lambda g: g.weight)
        return namen[stark.group].startswith(HANDKNOCHEN)

    drin = [an_der_hand(v) for v in hand.data.vertices]
    bpy.ops.object.mode_set(mode="EDIT")
    bm = bmesh.from_edit_mesh(hand.data)
    bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if not all(drin[v.index] for v in f.verts)], context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bmesh.update_edit_mesh(hand.data)
    bpy.ops.object.mode_set(mode="OBJECT")

    hand.name = hand.data.name = f"SK_{name}_Haende"
    hand.parent = arm
    mod = hand.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    log(f"Mannequin-Hände: {len(hand.data.vertices)} Punkte")
    return hand


def haut_material(hand, haut, name, ordner):
    """Ein Material mit einer kleinen einfarbigen Textur, damit der Import es wie die anderen Teile behandelt."""
    pfad = os.path.join(ordner, "04_export", f"T_{name}_Haut.png")
    bild = bpy.data.images.new(f"T_{name}_Haut", 8, 8)
    bild.pixels = [*haut, 1.0] * 64
    bild.filepath_raw = pfad
    bild.file_format = "PNG"
    bild.save()
    mat = bpy.data.materials.new(f"M_{name}_Haut")
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
    tex.image = bild
    mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    hand.data.materials.clear()
    hand.data.materials.append(mat)


def main(ordner):
    with open(os.path.join(ordner, "mensch.json"), encoding="utf-8") as f:
        name = json.load(f)["name"]
    bpy.ops.wm.open_mainfile(filepath=os.path.join(ordner, "03_blender", f"{name}.blend"))
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    koerper = bpy.data.objects[f"SK_{name}_Koerper"]
    if f"SK_{name}_Haende" in bpy.data.objects:
        raise RuntimeError("Die Hände sind in dieser Datei schon ersetzt – zuerst vael_mensch_aufbereiten.py neu laufen lassen.")

    # Die Finger bewegten bisher keine Haut (vael_mensch_aufbereiten.py schaltet sie ab); für die neuen Hände wieder an
    for b in arm.data.bones:
        if b.name.startswith(HANDKNOCHEN[1:]):
            b.use_deform = True

    haut = schneide_haende(koerper, arm)
    hand = manny_haende(arm, name)
    haut_material(hand, haut, name, ordner)

    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ordner, "03_blender", f"{name}.blend"))
    exportiere(arm, koerper, os.path.join(ordner, "04_export", f"SK_{name}_Koerper.fbx"))
    exportiere(arm, hand, os.path.join(ordner, "04_export", f"SK_{name}_Haende.fbx"))


if __name__ == "__main__":
    main(sys.argv[sys.argv.index("--") + 1])
