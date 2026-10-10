"""
Vael – Kreaturenfabrik, Station 4 + 5 (Blender)
================================================

Bereitet ein Tripo-Modell einheitlich für Vael auf und gibt ihm das Skelett seiner Körperfamilie.

Zwei Schritte, jeweils ein Aufruf:

  1) vorbereiten  – importiert das Tripo-Modell, reinigt es, setzt Maßstab/Ausrichtung/Namen,
                    reduziert auf das Polygonbudget und legt das Vorlage-Skelett der Familie an
                    (falls Tripo kein brauchbares Rig mitliefert). Speichert eine .blend-Datei.
                    → Danach in Blender kurz prüfen und die Knochen bei Bedarf zurechtrücken.

  2) binden       – bindet das Mesh mit automatischer Gewichtung an das Skelett und exportiert
                    SK_<Name>.fbx für Unreal.

Aufruf (Windows, im Ordner der Kreatur):
  blender -b -P vael_kreatur_aufbereiten.py -- vorbereiten "D:\\Vael_Quellen\\Spannhornkaefer"
  blender -b -P vael_kreatur_aufbereiten.py -- binden     "D:\\Vael_Quellen\\Spannhornkaefer"

Der Kreatur-Ordner enthält:
  kreatur.json      Steckbrief-Daten (Name, Familie, Klasse, Länge …)
  02_tripo\\        der Tripo-Export (.fbx oder .glb)
Das Skript legt an:
  03_blender\\<Name>.blend
  04_export\\SK_<Name>.fbx

Getestet mit Blender 5.1 (Python-API ist seit 4.x gleich geblieben für alles, was hier benutzt wird).
"""
import bpy, bmesh, json, math, os, sys
from mathutils import Vector, Matrix

# ---------------------------------------------------------------- Fabrikregeln
BUDGET = {"normal": 15000, "elite": 30000, "boss": 60000}   # max. Dreiecke

# Vorlage-Skelett Vielbeiner, normiert auf die Bounding Box des Modells:
#   x: -1 … +1  = halbe Breite (links/rechts)
#   y:  0 … 1   = von der vordersten Spitze (Hörner) bis ganz hinten
#   z:  0 … 1   = vom Boden bis zur höchsten Stelle
# Werte abgeleitet von der Bestiarium-Zeichnung des Spannhornkäfers.
VIELBEINER = {
    "body":   [(0, .40, .55), (0, .86, .55)],
    "head":   [(0, .36, .55), (0, .27, .55)],
    "horn_M": [(0, .29, .62), (0, .08, .70)],
    # Die Kreatur schaut in Blender nach -Y: ihre linke Seite ist +X
    "horn_L": [(.12, .27, .60), (.75, .02, .80)],
    "horn_R": [(-.12, .27, .60), (-.75, .02, .80)],
    # je Bein: Hüfte, Knie, Fußgelenk, Fußspitze
    "legs": {
        "1": [(.37, .41, .45), (.65, .38, .75), (.78, .29, .30), (.82, .23, 0)],
        "2": [(.51, .58, .45), (.83, .60, .75), (.93, .70, .30), (1.0, .76, 0)],
        "3": [(.46, .75, .45), (.76, .83, .75), (.79, .94, .30), (.85, 1.0, 0)],
    },
}
FAMILIEN_SKELETT = {"Vielbeiner": VIELBEINER}

# ---------------------------------------------------------------- Hilfen
def log(*a): print("[Vael]", *a)

def lade_steckbrief(ordner):
    with open(os.path.join(ordner, "kreatur.json"), encoding="utf-8") as f:
        return json.load(f)

def finde_tripo_datei(ordner):
    q = os.path.join(ordner, "02_tripo")
    for n in sorted(os.listdir(q)):
        if n.lower().endswith((".fbx", ".glb", ".gltf")):
            return os.path.join(q, n)
    raise FileNotFoundError(f"Keine .fbx/.glb in {q}")

def leere_szene():
    bpy.ops.wm.read_factory_settings(use_empty=True)

def importiere(pfad):
    if pfad.lower().endswith(".fbx"):
        bpy.ops.import_scene.fbx(filepath=pfad)
    else:
        bpy.ops.import_scene.gltf(filepath=pfad)

def meshes():   return [o for o in bpy.data.objects if o.type == "MESH"]
def armaturen(): return [o for o in bpy.data.objects if o.type == "ARMATURE"]

def aktiv(obj, auch=()):
    bpy.ops.object.select_all(action="DESELECT")
    for o in (obj, *auch):
        o.select_set(True)
    bpy.context.view_layer.objects.active = obj

def dreiecke(obj):
    obj.data.calc_loop_triangles()
    return len(obj.data.loop_triangles)

def bbox_welt(obj):
    pts = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi

# ---------------------------------------------------------------- Schritt 1
def vorbereiten(ordner):
    sb = lade_steckbrief(ordner)
    name = sb["name_datei"]
    leere_szene()
    importiere(finde_tripo_datei(ordner))
    log("importiert:", [o.name for o in bpy.data.objects])

    # Tripo-Rig behalten oder verwerfen
    tripo_rig = armaturen()
    behalte_rig = sb.get("tripo_rig_verwenden", False) and tripo_rig
    for a in tripo_rig:
        if not behalte_rig:
            for m in meshes():
                for mod in [x for x in m.modifiers if x.type == "ARMATURE"]:
                    m.modifiers.remove(mod)
                if m.parent == a:
                    mw = m.matrix_world.copy(); m.parent = None; m.matrix_world = mw
            bpy.data.objects.remove(a, do_unlink=True)
    for o in [o for o in bpy.data.objects if o.type not in ("MESH", "ARMATURE")]:
        bpy.data.objects.remove(o, do_unlink=True)

    # alle Mesh-Teile zu einem Objekt vereinen
    ms = meshes()
    aktiv(ms[0], ms[1:])
    if len(ms) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = obj.data.name = f"SK_{name}"
    if not behalte_rig:
        obj.vertex_groups.clear()

    # Transformationen anwenden, Drehung aus dem Steckbrief (falls Tripo anders ausrichtet)
    obj.rotation_euler.z += math.radians(sb.get("drehung_z_grad", 0))
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    # Maßstab: Gesamtlänge (vordere Spitze bis hinten, entlang Y) = laenge_cm
    lo, hi = bbox_welt(obj)
    laenge_bu = hi.y - lo.y
    faktor = (sb["laenge_cm"] / 100.0) / laenge_bu       # Blender-Einheit = 1 m
    obj.scale = (faktor,) * 3
    bpy.ops.object.transform_apply(scale=True)

    # Ursprung unten mittig, Kreatur steht auf dem Boden
    lo, hi = bbox_welt(obj)
    unten_mitte = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
    obj.data.transform(Matrix.Translation(-unten_mitte))
    obj.location = (0, 0, 0)

    # Polygonbudget
    ziel = BUDGET[sb.get("klasse", "normal")]
    n = dreiecke(obj)
    if n > ziel:
        mod = obj.modifiers.new("Budget", "DECIMATE")
        mod.ratio = ziel / n
        bpy.ops.object.modifier_apply(modifier=mod.name)
        log(f"reduziert: {n} → {dreiecke(obj)} Dreiecke (Budget {ziel})")
    else:
        log(f"Dreiecke: {n} (Budget {ziel}) – passt")

    # Materialnamen vereinheitlichen; Mark-Slot für das gemeinsame Mark-Material anlegen
    for i, slot in enumerate(obj.material_slots):
        if slot.material:
            slot.material.name = f"M_{name}" if i == 0 else f"M_{name}_{i}"
    if "M_Mark" not in [s.material.name for s in obj.material_slots if s.material]:
        mark = bpy.data.materials.get("M_Mark") or bpy.data.materials.new("M_Mark")
        obj.data.materials.append(mark)   # leer zugewiesen: Mark-Adern werden in Unreal per Maske gesetzt

    # Skelett
    if behalte_rig:
        log("Tripo-Rig wird behalten – Knochen bitte in Blender auf die Familiennamen prüfen.")
    else:
        familie = sb["familie"]
        if familie not in FAMILIEN_SKELETT:
            log(f"Für die Familie '{familie}' gibt es noch kein Vorlage-Skelett. Mesh ist trotzdem aufbereitet.")
        else:
            baue_vorlage_skelett(obj, name, FAMILIEN_SKELETT[familie], sb.get("hoerner", True))

    os.makedirs(os.path.join(ordner, "03_blender"), exist_ok=True)
    ziel_blend = os.path.join(ordner, "03_blender", f"{name}.blend")
    bpy.ops.wm.save_as_mainfile(filepath=ziel_blend)
    log("gespeichert:", ziel_blend)
    log("Nächster Schritt: Datei in Blender öffnen, Knochen prüfen/zurechtrücken, dann 'binden' ausführen.")

def passe_vielbeiner_an(obj, tpl, hoerner=True):
    """Sucht Füße, Knie, Horn-Spitzen und Kopf direkt im Mesh und schreibt die Vorlage darauf um.
    Funktioniert für jeden Sechsbeiner, der auf dem Boden steht und nach -Y schaut."""
    import copy
    lo, hi = bbox_welt(obj)
    hw, L, H = (hi.x - lo.x) / 2, hi.y - lo.y, hi.z - lo.z
    V = [obj.matrix_world @ v.co for v in obj.data.vertices]
    n = lambda p: ((p.x) / hw, (p.y - lo.y) / L, (p.z - lo.z) / H)   # Welt → normiert
    t = copy.deepcopy(tpl)

    # 1) Füße: Punkte nahe am Boden, je Seite in drei Gruppen entlang Y
    boden = [p for p in V if p.z < lo.z + .05 * H and abs(p.x) > .15 * hw]
    beine = {}
    for s, seite in ((-1, "L"), (1, "R")):
        ys = sorted((p for p in boden if p.x * s > 0), key=lambda p: p.y)
        if len(ys) < 6:
            log("Füße nicht sicher gefunden – nehme die Vorlage."); return tpl
        # Gruppen dort trennen, wo entlang Y eine deutliche Lücke ist (Füße liegen auseinander)
        gruppen, akt = [], [ys[0]]
        for a_, b_ in zip(ys, ys[1:]):
            if b_.y - a_.y > .1 * L:
                gruppen.append(akt); akt = []
            akt.append(b_)
        gruppen.append(akt)
        gruppen = sorted(gruppen, key=len, reverse=True)[:3]
        gruppen.sort(key=lambda g: g[0].y)
        beine[seite] = [Vector((sum(q.x for q in g) / len(g), sum(q.y for q in g) / len(g), lo.z)) for g in gruppen]

    anzahl = min(len(beine["L"]), len(beine["R"]))
    nummern = {3: ("1", "2", "3"), 2: ("1", "3"), 1: ("2",)}[anzahl]
    if anzahl < 3:
        log(f"WARNUNG: nur {anzahl * 2} Beine am Boden gefunden statt 6. "
            f"Skelett bekommt nur die Beine {', '.join(nummern)} – Modell ggf. neu erzeugen.")
    t["legs"] = {}

    # 2) Hüfte am Körperrand, Knie = höchster Punkt des Beins zwischen Hüfte und Fuß,
    #    aber unterhalb der Körperoberkante (sonst landen wir in Hörnern/Geweih)
    for i, nr in enumerate(nummern):
        fuss = beine["R"][i]
        huefte = Vector((max(fuss.x * .6, hw * .2), fuss.y * .8 + (lo.y + L * .5) * .2, lo.z + .40 * H))
        mitte = (huefte + fuss) / 2
        kand = [p for p in V if huefte.x * .9 < p.x < fuss.x * 1.35 and abs(p.y - mitte.y) < .12 * L
                and lo.z + .15 * H < p.z < lo.z + .6 * H]
        knie = max(kand, key=lambda p: p.z + .3 * p.x) if kand else Vector((fuss.x * 1.05, mitte.y, lo.z + .5 * H))
        gelenk = knie.lerp(fuss, .6); gelenk.z = max(lo.z + .12 * H, min(gelenk.z, lo.z + .3 * H))
        t["legs"][nr] = [n(huefte), n(knie), n(gelenk), n(fuss)]

    # 3) Geweih-Hörner: äußerste hohe Punkte links/rechts; Ansatz an der Körperflanke
    for s, k in (() if not hoerner else ((1, "horn_L"), (-1, "horn_R"))):
        hoch = [p for p in V if p.z > lo.z + .45 * H and p.x * s > .4 * hw]
        if hoch:
            spitze = max(hoch, key=lambda p: abs(p.x) + .6 * (p.z - lo.z))
            ansatz = Vector((s * .33 * hw, spitze.y, lo.z + .62 * H))
            t[k] = [n(ansatz), n(spitze)]
    # 4) Mittelhorn: höchster Punkt nahe der Mitte in der vorderen Hälfte
    mitte_v = [p for p in V if abs(p.x) < .12 * hw and p.y < lo.y + .55 * L]
    if mitte_v and hoerner:
        sp = max(mitte_v, key=lambda p: p.z)
        t["horn_M"] = [n(Vector((0, sp.y + .08 * L, lo.z + .62 * H))), n(sp)]
    # 5) Kopf: vorderster Punkt in der Mitte
    vorn = [p for p in V if abs(p.x) < .15 * hw]
    if vorn:
        kp = min(vorn, key=lambda p: p.y)
        t["head"] = [n(Vector((0, kp.y + .22 * L, kp.z))), n(kp)]
        t["body"] = [n(Vector((0, kp.y + .24 * L, lo.z + .5 * H))), n(Vector((0, hi.y - .08 * L, lo.z + .5 * H)))]
    log("Skelett an das Mesh angepasst (Füße, Knie, Hörner, Kopf gefunden).")
    return t

def baue_vorlage_skelett(obj, name, tpl, hoerner=True):
    if tpl is VIELBEINER:
        tpl = passe_vielbeiner_an(obj, tpl, hoerner)
    lo, hi = bbox_welt(obj)
    hw, L, H = (hi.x - lo.x) / 2, hi.y - lo.y, hi.z - lo.z
    vorne = lo.y   # Blender: Kreatur schaut nach -Y (Blender-Vorderansicht)

    def p(n):  # normiert → Welt
        x, y, z = n
        return Vector((x * hw, vorne + y * L, z * H))

    arm_data = bpy.data.armatures.new(f"SKEL_{name}")
    arm = bpy.data.objects.new(f"SKEL_{name}", arm_data)
    bpy.context.collection.objects.link(arm)
    aktiv(arm)
    bpy.ops.object.mode_set(mode="EDIT")
    eb = arm_data.edit_bones

    def knochen(nm, a, b, eltern=None, verbunden=False):
        k = eb.new(nm); k.head = a; k.tail = b
        if eltern: k.parent = eltern; k.use_connect = verbunden
        return k

    root = knochen("root", Vector((0, 0, 0)), Vector((0, 0, H * .2)))
    body = knochen("body", p(tpl["body"][0]), p(tpl["body"][1]), root)
    head = knochen("head", p(tpl["head"][0]), p(tpl["head"][1]), body)
    for h in ("horn_M", "horn_L", "horn_R") if hoerner else ():
        knochen(h, p(tpl[h][0]), p(tpl[h][1]), head)
    for nr, (huefte, knie, gelenk, spitze) in tpl["legs"].items():
        # Blick nach -Y: links ist +X
        for seite, s in (("L", 1), ("R", -1)):
            m = lambda v: (v[0] * s, v[1], v[2])
            ansatz = p((m(huefte)[0] * .55, huefte[1], huefte[2]))
            c = knochen(f"leg_{seite}{nr}_coxa", ansatz, p(m(huefte)), body)
            f = knochen(f"leg_{seite}{nr}_femur", p(m(huefte)), p(m(knie)), c, True)
            t = knochen(f"leg_{seite}{nr}_tibia", p(m(knie)), p(m(gelenk)), f, True)
            knochen(f"leg_{seite}{nr}_foot", p(m(gelenk)), p(m(spitze)), t, True)
    bpy.ops.object.mode_set(mode="OBJECT")
    log(f"Vorlage-Skelett angelegt: {len(arm_data.bones)} Knochen")

def abstands_gewichtung(obj, arm):
    """Jeder Punkt gehört zu den nächstgelegenen Knochen (Abstand zur Knochenstrecke), weich überblendet.
    Robust für jedes Mesh, egal wie sauber es ist."""
    mw = arm.matrix_world
    segs = [(b.name, mw @ b.head_local, mw @ b.tail_local) for b in arm.data.bones if b.name != "root"]
    obj.vertex_groups.clear()
    gruppen = {n: obj.vertex_groups.new(name=n) for n, _, _ in segs}
    obj.vertex_groups.new(name="root")

    def abstand(p, a, b):
        ab = b - a; t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-9)))
        return (p - (a + ab * t)).length

    owm = obj.matrix_world
    for v in obj.data.vertices:
        p = owm @ v.co
        d = sorted((abstand(p, a, b), n) for n, a, b in segs)
        nah = [(dist, n) for dist, n in d[:3] if dist <= d[0][0] * 1.6 + 1e-4]
        ws = [1.0 / (dist * dist + 1e-6) for dist, _ in nah]; s = sum(ws)
        for (dist, n), w in zip(nah, ws):
            gruppen[n].add([v.index], w / s, "REPLACE")
    log("Abstands-Gewichtung fertig:", len(obj.data.vertices), "Punkte auf", len(segs), "Knochen verteilt.")

# ---------------------------------------------------------------- Schritt 2
def binden(ordner):
    sb = lade_steckbrief(ordner)
    name = sb["name_datei"]
    bpy.ops.wm.open_mainfile(filepath=os.path.join(ordner, "03_blender", f"{name}.blend"))
    obj = bpy.data.objects[f"SK_{name}"]
    arm = armaturen()[0]
    if not any(m.type == "ARMATURE" for m in obj.modifiers):
        aktiv(arm, (obj,))
        bpy.ops.object.parent_set(type="ARMATURE_AUTO")
        benutzt = {g.group for v in obj.data.vertices for g in v.groups if g.weight > 0.01}
        leer = [g.name for g in obj.vertex_groups if g.index not in benutzt and g.name != "root"]
        if len(leer) > len(obj.vertex_groups) * .3:
            # Blenders Wärme-Gewichtung scheitert oft an KI-Meshes (überlappende, offene Teile).
            log(f"Automatische Gewichtung unvollständig ({len(leer)} Knochen leer) – nehme Abstands-Gewichtung.")
            abstands_gewichtung(obj, arm)
        else:
            log("automatisch gewichtet.", f"Knochen ohne Gewicht: {leer}" if leer else "Alle Knochen haben Gewicht.")
    bpy.ops.wm.save_mainfile()

    os.makedirs(os.path.join(ordner, "04_export"), exist_ok=True)
    ziel = os.path.join(ordner, "04_export", f"SK_{name}.fbx")
    aktiv(arm, (obj,))
    bpy.ops.export_scene.fbx(
        filepath=ziel, use_selection=True, object_types={"ARMATURE", "MESH"},
        apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
        add_leaf_bones=False, bake_anim=False, path_mode="COPY", embed_textures=True,
        mesh_smooth_type="FACE",
    )
    log("exportiert:", ziel)

def requisite(ordner):
    """Pflanzen, Pilze, Gesteine und andere Requisiten: ohne Skelett, in einem Schritt.
    Liest requisite.json (name, hoehe_cm), vereint die Teile, skaliert auf die Höhe, Ursprung unten mittig,
    exportiert 04_export/SM_<Name>.fbx."""
    with open(os.path.join(ordner, "requisite.json"), encoding="utf-8") as f:
        sb = json.load(f)
    name = sb["name"]
    leere_szene()
    importiere(finde_tripo_datei(ordner))
    for a in armaturen():
        bpy.data.objects.remove(a, do_unlink=True)
    for o in [o for o in bpy.data.objects if o.type != "MESH"]:
        bpy.data.objects.remove(o, do_unlink=True)

    ms = meshes()
    aktiv(ms[0], ms[1:])
    if len(ms) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = obj.data.name = f"SM_{name}"
    obj.rotation_euler.z += math.radians(sb.get("drehung_z_grad", 0))
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    # Maßstab: Höhe vom Boden bis zur höchsten Stelle = hoehe_cm
    lo, hi = bbox_welt(obj)
    obj.scale = ((sb["hoehe_cm"] / 100.0) / (hi.z - lo.z),) * 3
    bpy.ops.object.transform_apply(scale=True)
    lo, hi = bbox_welt(obj)
    obj.data.transform(Matrix.Translation(-Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))))
    obj.location = (0, 0, 0)

    ziel = sb.get("budget_dreiecke", 15000)
    n = dreiecke(obj)
    if n > ziel:
        mod = obj.modifiers.new("Budget", "DECIMATE")
        mod.ratio = ziel / n
        bpy.ops.object.modifier_apply(modifier=mod.name)
    log(f"{name}: {dreiecke(obj)} Dreiecke, {sb['hoehe_cm']} cm hoch")
    for i, slot in enumerate(obj.material_slots):
        if slot.material:
            slot.material.name = f"M_{name}" if i == 0 else f"M_{name}_{i}"

    os.makedirs(os.path.join(ordner, "04_export"), exist_ok=True)
    ziel_fbx = os.path.join(ordner, "04_export", f"SM_{name}.fbx")
    aktiv(obj)
    bpy.ops.export_scene.fbx(
        filepath=ziel_fbx, use_selection=True, object_types={"MESH"},
        apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL",
        bake_anim=False, path_mode="COPY", embed_textures=True, mesh_smooth_type="FACE",
    )
    log("exportiert:", ziel_fbx)

# ---------------------------------------------------------------- Einstieg
if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if len(args) != 2 or args[0] not in ("vorbereiten", "binden", "requisite"):
        print(__doc__); sys.exit(1)
    {"vorbereiten": vorbereiten, "binden": binden, "requisite": requisite}[args[0]](args[1])
