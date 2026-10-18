"""
Vael – Kreaturenfabrik, Menschen (Blender)
==========================================

Bringt ein Tripo-Modell eines Menschen in T-Pose auf das Unreal-Skelett (SK_Mannequin), damit alle Menschen dieselben
Animationen benutzen, und teilt es in austauschbare Teile: Körper und Kopf.

Ablauf:
  1) Das Unreal-Skelett kommt aus C:\\Projekte\\Vael_Quellen\\_Skelett\\SKM_Manny_Simple.fbx (aus Unreal exportiert).
  2) Das Tripo-Modell wird auf die Schulterhöhe des Skeletts skaliert und mittig auf den Boden gestellt.
  3) Eine Kopie des Skeletts wird in die T-Pose des Modells gebracht, das Modell daran gebunden und mit ihr zurück
     in die A-Pose des Skeletts gebogen. Danach hängt es am unveränderten Skelett.
  4) Am Hals wird geteilt: 04_export/SK_<Name>_Koerper.fbx und SK_<Name>_Kopf.fbx.

Aufruf:
  blender -b -P vael_mensch_aufbereiten.py -- "C:\\Projekte\\Vael_Quellen\\MagierMann"

Der Ordner enthält mensch.json (name, optional drehung_z_grad, hals_z_cm, kopf_farbtest) und 02_tripo\\ mit dem Tripo-Export.
kopf_farbtest (Standard true): am Hals entscheidet die Farbe, blauer Stoff bleibt am Körper. Für Figuren ohne blauen
Stoff (Floromant) false: dann teilt nur die Höhe.
"""
import bpy, bmesh, json, math, os, sys
from mathutils import Vector, Matrix

SKELETT = r"C:\Projekte\Vael_Quellen\_Skelett\SKM_Manny_Simple.fbx"

# Gerechnet wird in Zentimetern wie in Unreal (Szenen-Einheit 0,01 m): nur so kommen die Knochen ohne Maßstab im FBX an
# und passen zum vorhandenen Unreal-Skelett. Mit Knochen in Metern schrumpft das Modell beim Animieren auf ein Hundertstel.
CM = 100.0

# Knochen, die keine Haut bewegen: Hilfsknochen des Skeletts, die Dreh-Zwischenknochen (die bewegt bei uns niemand)
# und die Finger (die Hände der Tripo-Modelle sind zu grob dafür; die ganze Hand folgt dem Handknochen)
OHNE_HAUT = ("ik_", "interaction", "center_of_mass", "_twist_", "index_", "middle_", "ring_", "pinky_", "thumb_")


def log(*a): print("[Vael]", *a)


def aktiv(obj, auch=()):
    bpy.ops.object.select_all(action="DESELECT")
    for o in (obj, *auch):
        o.select_set(True)
    bpy.context.view_layer.objects.active = obj


def punkte(obj):
    return [obj.matrix_world @ v.co for v in obj.data.vertices]


def mittel(ps):
    return sum(ps, Vector()) / len(ps)


def armachse(ps):
    """Mitte eines Arms in einem Streifen: die weiten Ärmel hängen nach unten, darum zählt nur das obere Drittel der Punkte,
    ein Stück unter der Oberkante."""
    oben = sorted(ps, key=lambda p: -p.z)[:max(4, len(ps) // 3)]
    m = mittel(oben)
    return Vector((m.x, m.y, max(p.z for p in ps) - .045 * CM))


def lade_skelett():
    bpy.ops.import_scene.fbx(filepath=SKELETT, automatic_bone_orientation=False)
    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    # Das Skelett aus Unreal hängt an einem Elternobjekt mit Maßstab 0,01: lösen und den Maßstab in die Knochen schreiben
    welt = arm.matrix_world.copy()
    arm.parent = None
    arm.matrix_world = welt
    for o in [o for o in bpy.data.objects if o is not arm]:
        bpy.data.objects.remove(o, do_unlink=True)
    aktiv(arm)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for b in arm.data.bones:
        if any(k in b.name for k in OHNE_HAUT):
            b.use_deform = False
    log(f"Skelett geladen: {len(arm.data.bones)} Knochen, Objekt '{arm.name}'")
    return arm


def lade_modell(ordner, sb):
    q = os.path.join(ordner, "02_tripo")
    datei = next(os.path.join(q, n) for n in sorted(os.listdir(q)) if n.lower().endswith((".fbx", ".glb")))
    vorher = set(bpy.data.objects)
    if datei.lower().endswith(".fbx"):
        bpy.ops.import_scene.fbx(filepath=datei)
    else:
        bpy.ops.import_scene.gltf(filepath=datei)
    neu = [o for o in bpy.data.objects if o not in vorher]
    ms = [o for o in neu if o.type == "MESH"]
    for o in [o for o in neu if o.type != "MESH"]:
        for m in ms:
            if m.parent == o:
                mw = m.matrix_world.copy(); m.parent = None; m.matrix_world = mw
        bpy.data.objects.remove(o, do_unlink=True)
    aktiv(ms[0], ms[1:])
    if len(ms) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    for mod in list(obj.modifiers):
        obj.modifiers.remove(mod)
    obj.vertex_groups.clear()
    obj.rotation_euler.z += math.radians(sb.get("drehung_z_grad", 0))
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return obj


def einpassen(obj, arm):
    """Skaliert das Modell so, dass seine ausgestreckten Arme auf Schulterhöhe des Skeletts liegen, und stellt es mittig hin."""
    V = punkte(obj)
    lo = Vector((min(p.x for p in V), min(p.y for p in V), min(p.z for p in V)))
    hi = Vector((max(p.x for p in V), max(p.y for p in V), max(p.z for p in V)))
    hw = (hi.x - lo.x) / 2
    cx = (hi.x + lo.x) / 2
    # Oberarm: ein Streifen zwischen Schulter und Ellbogen, auf beiden Seiten
    mitte_z = lo.z + .55 * (hi.z - lo.z)
    oberarm = [p for p in V if .32 * hw < abs(p.x - cx) < .42 * hw and p.z > mitte_z]
    arm_z = max(p.z for p in oberarm) - .025 * (hi.z - lo.z) - lo.z   # Armmitte: ein Stück unter der Oberkante des Ärmels
    schulter_z = arm.data.bones["upperarm_l"].head_local.z
    faktor = schulter_z / arm_z
    # Rumpf: Punkte nahe der Mitte auf Brusthöhe geben die Tiefe
    rumpf = [p for p in V if abs(p.x - cx) < .1 * hw and lo.z + .55 * (hi.z - lo.z) < p.z < lo.z + .75 * (hi.z - lo.z)]
    cy = mittel(rumpf).y
    wirbel_y = arm.data.bones["spine_03"].head_local.y
    obj.data.transform(Matrix.Translation(Vector((-cx, -cy, -lo.z))))
    obj.data.transform(Matrix.Scale(faktor, 4))
    obj.data.transform(Matrix.Translation(Vector((0, wirbel_y, 0))))
    log(f"eingepasst: Faktor {faktor:.3f}, Größe {(hi.z - lo.z) * faktor / CM:.2f} m, Spannweite {(hi.x - lo.x) * faktor / CM:.2f} m")


def drehe(pb, von, nach):
    """Dreht einen Pose-Knochen um seinen Anfang, sodass die Richtung 'von' auf 'nach' fällt (Weltkoordinaten)."""
    bpy.context.view_layer.update()
    m = pb.matrix.copy()
    kopf = m.to_translation()
    q = von.normalized().rotation_difference(nach.normalized())
    pb.matrix = Matrix.Translation(kopf) @ q.to_matrix().to_4x4() @ Matrix.Translation(-kopf) @ m
    bpy.context.view_layer.update()


def in_modell_pose(kopie, obj):
    """Biegt die Arme der Skelett-Kopie in die Richtung, in die das Modell seine Arme hält."""
    V = punkte(obj)
    hw = max(abs(p.x) for p in V)
    V = [p for p in V if p.z > .55 * max(p.z for p in V)]       # nur die obere Hälfte: der Rock reicht unten weit nach außen
    aktiv(kopie)
    bpy.ops.object.mode_set(mode="POSE")
    pb = kopie.pose.bones
    for seite, s in (("l", 1), ("r", -1)):
        innen = [p for p in V if .33 * hw < p.x * s < .43 * hw]
        aussen = [p for p in V if .70 * hw < p.x * s < .80 * hw]
        richtung = armachse(aussen) - armachse(innen)
        drehe(pb[f"upperarm_{seite}"], pb[f"lowerarm_{seite}"].head - pb[f"upperarm_{seite}"].head, richtung)
        drehe(pb[f"lowerarm_{seite}"], pb[f"hand_{seite}"].head - pb[f"lowerarm_{seite}"].head, richtung)
        drehe(pb[f"hand_{seite}"], pb[f"middle_01_{seite}"].head - pb[f"hand_{seite}"].head, richtung)
        # Handfläche nach unten wie im Modell: der Daumen zeigt nach vorn (-Y)
        hand = pb[f"hand_{seite}"]
        bpy.context.view_layer.update()
        daumen = (pb[f"thumb_01_{seite}"].head - hand.head)
        achse = richtung.normalized()
        quer = daumen - achse * daumen.dot(achse)
        ziel = Vector((0, -1, 0)) - achse * Vector((0, -1, 0)).dot(achse)
        if quer.length > 1e-4 and ziel.length > 1e-4:
            drehe(hand, quer, ziel)
        log(f"Arm {seite}: Richtung {[round(c, 2) for c in richtung.normalized()]}")
    bpy.ops.pose.armature_apply(selected=False)
    bpy.ops.object.mode_set(mode="OBJECT")


def zurueck_in_ruhepose(kopie, arm):
    """Stellt die Kopie so hin, wie das echte Skelett ruht: das gebundene Modell biegt sich mit in die A-Pose."""
    aktiv(kopie)
    bpy.ops.object.mode_set(mode="POSE")

    def setze(knochen):
        kopie.pose.bones[knochen.name].matrix = arm.data.bones[knochen.name].matrix_local.copy()
        bpy.context.view_layer.update()
        for kind in knochen.children:
            setze(kind)

    for wurzel in [b for b in kopie.data.bones if b.parent is None]:
        setze(wurzel)
    bpy.ops.object.mode_set(mode="OBJECT")


def binde(obj, kopie):
    aktiv(kopie, (obj,))
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    benutzt = {g.group for v in obj.data.vertices for g in v.groups if g.weight > 0.01}
    haut = [b.name for b in kopie.data.bones if b.use_deform]
    ohne = [n for n in haut if obj.vertex_groups.get(n) is None or obj.vertex_groups[n].index not in benutzt]
    log(f"gebunden: {len(haut) - len(ohne)} von {len(haut)} Knochen tragen Gewicht" + (f"; ohne: {ohne[:8]}" if ohne else ""))
    if len(ohne) > len(haut) * .5:
        raise RuntimeError("Die automatische Gewichtung ist gescheitert (zu viele Knochen ohne Gewicht).")


def finde_hals(obj, arm, sb):
    """Höhe, auf der Kopf und Körper geteilt werden: am Halsansatz, unter Kinn und Bart."""
    if sb.get("hals_z_cm"):
        return sb["hals_z_cm"] / 100.0 * CM
    return arm.data.bones["neck_01"].head_local.z + .01 * CM


def farbe_lesen(obj):
    """Gibt eine Funktion zurück, die die Farbe der Textur an einer UV-Stelle liefert (r, g, b), oder None ohne Textur."""
    import numpy
    for slot in obj.material_slots:
        baum = slot.material.node_tree if slot.material and slot.material.use_nodes else None
        for knoten in (baum.nodes if baum else []):
            if knoten.type == "TEX_IMAGE" and knoten.image and any(l.to_socket.name == "Base Color" for l in knoten.outputs[0].links):
                bild = knoten.image
                w, h = bild.size
                px = numpy.empty(w * h * 4, dtype=numpy.float32)
                bild.pixels.foreach_get(px)
                px = px.reshape(h, w, 4)
                return lambda uv: px[int(uv.y % 1.0 * (h - 1)), int(uv.x % 1.0 * (w - 1)), :3]
    return None


def teile(obj, hals_z, name, farbtest=True):
    """Trennt den Kopf als eigenes Objekt ab. Gibt (Körper, Kopf) zurück.
    Weit über dem Halsansatz ist alles Kopf, weit darunter alles Körper. Dazwischen entscheidet die Farbe:
    Haut und Bart gehören zum Kopf, der Stoff von Kragen und zurückgeschlagener Kapuze bleibt am Körper."""
    farbe = farbe_lesen(obj) if farbtest else None
    V = [obj.matrix_world @ v.co for v in obj.data.vertices]
    schaedel = [p for p in V if p.z > hals_z + .10 * CM]
    ax, ay = sum(p.x for p in schaedel) / len(schaedel), sum(p.y for p in schaedel) / len(schaedel)

    aktiv(obj)
    bpy.ops.object.mode_set(mode="EDIT")
    bm = bmesh.from_edit_mesh(obj.data)
    uv = bm.loops.layers.uv.active

    def am_kopf(f):
        p = obj.matrix_world @ f.calc_center_median()
        if p.z > hals_z + .09 * CM:
            return True
        if p.z < hals_z - .07 * CM or math.hypot(p.x - ax, p.y - ay) > .13 * CM:
            return False
        if farbe is None or uv is None:
            return p.z > hals_z
        mitte = sum((l[uv].uv for l in f.loops), Vector((0, 0))) / len(f.loops)
        r, g, b = farbe(mitte)
        return bool(b < r * 1.08)    # der Stoff ist blau, Haut und Haar sind es nicht

    for f in bm.faces:
        f.select = am_kopf(f)
    bmesh.update_edit_mesh(obj.data)
    vorher = set(bpy.data.objects)
    bpy.ops.mesh.separate(type="SELECTED")
    bpy.ops.object.mode_set(mode="OBJECT")
    kopf = next(o for o in bpy.data.objects if o not in vorher)
    obj.name = obj.data.name = f"SK_{name}_Koerper"
    kopf.name = kopf.data.name = f"SK_{name}_Kopf"
    log(f"geteilt auf {hals_z / CM * 100:.0f} cm Höhe: Körper {len(obj.data.vertices)} Punkte, Kopf {len(kopf.data.vertices)} Punkte")
    return obj, kopf


def exportiere(arm, teil, ziel):
    aktiv(arm, (teil,))
    bpy.ops.export_scene.fbx(
        filepath=ziel, use_selection=True, object_types={"ARMATURE", "MESH"},
        apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE",
        add_leaf_bones=False, bake_anim=False, path_mode="COPY", embed_textures=True,
        mesh_smooth_type="FACE", use_armature_deform_only=False,
    )
    log("exportiert:", ziel)


def main(ordner):
    with open(os.path.join(ordner, "mensch.json"), encoding="utf-8") as f:
        sb = json.load(f)
    name = sb["name"]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.scale_length = 1.0 / CM      # der FBX-Import rechnet damit alles von selbst in Zentimeter um

    arm = lade_skelett()
    obj = lade_modell(ordner, sb)
    einpassen(obj, arm)

    # Kopie des Skeletts in der Pose des Modells; daran binden und mit ihr zurück in die Ruhepose des echten Skeletts
    aktiv(arm)
    bpy.ops.object.duplicate()
    kopie = bpy.context.view_layer.objects.active
    in_modell_pose(kopie, obj)
    binde(obj, kopie)
    zurueck_in_ruhepose(kopie, arm)
    aktiv(obj)
    for mod in [m for m in obj.modifiers if m.type == "ARMATURE"]:
        bpy.ops.object.modifier_apply(modifier=mod.name)
    welt = obj.matrix_world.copy()
    obj.parent = None
    obj.matrix_world = welt
    bpy.data.objects.remove(kopie, do_unlink=True)

    # Jetzt ans echte Skelett: die Gewichte heißen schon wie seine Knochen
    obj.parent = arm
    mod = obj.modifiers.new("Armature", "ARMATURE")
    mod.object = arm

    for i, slot in enumerate(obj.material_slots):
        if slot.material:
            slot.material.name = f"M_{name}" if i == 0 else f"M_{name}_{i}"

    hals = finde_hals(obj, arm, sb)
    koerper, kopf = teile(obj, hals, name, sb.get("kopf_farbtest", True))

    os.makedirs(os.path.join(ordner, "03_blender"), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ordner, "03_blender", f"{name}.blend"))
    os.makedirs(os.path.join(ordner, "04_export"), exist_ok=True)
    exportiere(arm, koerper, os.path.join(ordner, "04_export", f"SK_{name}_Koerper.fbx"))
    exportiere(arm, kopf, os.path.join(ordner, "04_export", f"SK_{name}_Kopf.fbx"))


if __name__ == "__main__":
    main(sys.argv[sys.argv.index("--") + 1])
