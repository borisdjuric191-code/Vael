"""
Vael – Kreaturenfabrik, Kleidung (Blender)
==========================================

Setzt ein Kleidungsstück vom Mannequin-Skelett auf den Körper eines MetaHuman um, ohne neues Tripo-Modell:
  1) Öffnet 03_blender/<Name>.blend (aus vael_mensch_aufbereiten.py und vael_mensch_haende.py) und lädt den Körper
     aus _Skelett/SKM_<MetaHuman>_Body.fbx (vael_metahuman_koerper_export.py).
  2) Biegt jeden Punkt mit seinen Gewichten auf die Gelenke des MetaHuman: Arme und Beine drehen und strecken sich mit,
     Rumpf und Hals verschieben sich nur (ihre Knochen sind zu kurz für eine verlässliche Richtung).
  3) Rückt die Ärmel auf die Arme: die Tripo-Modelle haben keine Arme in den Ärmeln, die hängen daher neben dem echten Arm.
  4) Schiebt alles, was im Körper steckt oder ihm zu nahe ist, nach außen und glättet diesen Schub.
  5) Hängt das Teil an das Skelett des MetaHuman (gleiche Knochennamen, die Gewichte bleiben) und exportiert
     04_export/SK_<Name>_<Teil>_MH.fbx. Die Vorlage 03_blender/<Name>.blend bleibt unverändert.

Aufruf:
  blender -b -P vael_kleidung_anpassen.py -- "C:\\Projekte\\Vael_Quellen\\MagierMann" Koerper Robe MH_Magier
"""
import bpy, os, re, sys
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from vael_mensch_aufbereiten import aktiv, exportiere, log

# Jeder Knochen, der sich drehen und strecken darf, und der Knochen, auf den er zeigt
KETTEN = {}
for seite in ("l", "r"):
    KETTEN.update({
        f"clavicle_{seite}": f"upperarm_{seite}", f"upperarm_{seite}": f"lowerarm_{seite}", f"lowerarm_{seite}": f"hand_{seite}",
        f"thigh_{seite}": f"calf_{seite}", f"calf_{seite}": f"foot_{seite}", f"foot_{seite}": f"ball_{seite}",
    })

# Abstand der Kleidung zur Haut in Zentimetern, und wie weit um den Körper herum überhaupt geschoben wird
MARGE = 1.2
REICHWEITE = 12.0
GLAETTEN = 8

# In so viele Abschnitte wird ein Ärmel entlang des Arms geteilt
ABSCHNITTE = 12


def lade_metahuman(pfad):
    """Lädt Skelett und Körper des MetaHuman in Zentimetern, ohne das Elternobjekt mit Maßstab 0,01."""
    vorher = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=pfad, automatic_bone_orientation=False)
    neu = [o for o in bpy.data.objects if o not in vorher]
    arm = next(o for o in neu if o.type == "ARMATURE")
    koerper = [o for o in neu if o.type == "MESH"]
    for o in (arm, *koerper):
        welt = o.matrix_world.copy()
        o.parent = None
        o.matrix_world = welt
    for o in neu:
        if o is not arm and o not in koerper:
            bpy.data.objects.remove(o, do_unlink=True)
    for o in (arm, *koerper):
        for mod in list(o.modifiers):
            o.modifiers.remove(mod)
        aktiv(o)
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return arm, koerper


def verschiebung(ziel):
    m = np.eye(4)
    m[:3, 3] = ziel
    return m


def knochen_matrix(name, alt, neu):
    """Bringt den Knochen des Mannequins an seinen Platz im MetaHuman: als Matrix im Raum des Skeletts."""
    h_alt = np.array(alt[name].head_local)
    h_neu = np.array(neu[name].head_local)
    kind = KETTEN.get(name)
    if kind is None or kind not in alt or kind not in neu:
        return verschiebung(h_neu - h_alt)

    d_alt = alt[kind].head_local - alt[name].head_local
    d_neu = neu[kind].head_local - neu[name].head_local
    dreh = np.array(d_alt.rotation_difference(d_neu).to_matrix())
    achse = np.array(d_alt.normalized())
    strecken = np.eye(3) + (d_neu.length / d_alt.length - 1.0) * np.outer(achse, achse)

    m = np.eye(4)
    m[:3, :3] = dreh @ strecken
    return verschiebung(h_neu) @ m @ verschiebung(-h_alt)


def verwandte_matrix(name, matrizen, alt):
    """Dreh-Zwischenknochen und Finger folgen dem nächsten Vorfahren, der eine eigene Matrix hat."""
    b = alt[name]
    while b is not None:
        if b.name in matrizen:
            return matrizen[b.name]
        b = b.parent
    return np.eye(4)


def biege(teil, alt, neu):
    """Rechnet jeden Punkt mit seinen Gewichten um (lineares Skinning)."""
    matrizen = {b.name: knochen_matrix(b.name, alt, neu) for b in alt if b.name in neu and (b.name in KETTEN or not re.search(r"_twist_|^(index|middle|ring|pinky|thumb)_|^ik_", b.name))}
    gruppen = {g.index: g.name for g in teil.vertex_groups}
    nach_name = {name: verwandte_matrix(name, matrizen, alt) for name in gruppen.values() if name in alt}

    ohne_gewicht = 0
    for v in teil.data.vertices:
        summe = np.zeros((4, 4))
        gesamt = 0.0
        for g in v.groups:
            m = nach_name.get(gruppen[g.group])
            if m is not None and g.weight > 0.0:
                summe += g.weight * m
                gesamt += g.weight
        if gesamt <= 0.0:
            ohne_gewicht += 1
            continue
        p = summe / gesamt @ np.array((*v.co, 1.0))
        v.co = Vector(p[:3])
    log(f"gebogen: {len(teil.data.vertices)} Punkte, ohne Gewicht {ohne_gewicht}")


def aermel_auf_arme(teil, neu):
    """Die weiten Tripo-Ärmel hängen neben dem Arm: jeder Abschnitt rückt so, dass der Arm unter seiner Oberkante liegt."""
    gruppen = {g.index: g.name for g in teil.vertex_groups}
    for seite in ("l", "r"):
        schulter = neu[f"upperarm_{seite}"].head_local
        ellbogen = neu[f"lowerarm_{seite}"].head_local
        hand = neu[f"hand_{seite}"].head_local
        strecken = [(schulter, ellbogen), (ellbogen, hand)]
        laengen = [(b - a).length for a, b in strecken]
        gesamt = sum(laengen)

        def arm_punkt(s):
            """Punkt auf dem Arm bei s Zentimetern ab der Schulter."""
            if s <= laengen[0]:
                return schulter.lerp(ellbogen, max(s, 0.0) / laengen[0])
            return ellbogen.lerp(hand, min((s - laengen[0]) / laengen[1], 1.0))

        def arm_lage(p):
            """Lage eines Punkts entlang des Arms, in Zentimetern ab der Schulter."""
            auf_ober = (p - schulter).dot((ellbogen - schulter).normalized())
            if auf_ober <= laengen[0]:
                return auf_ober
            return laengen[0] + (p - ellbogen).dot((hand - ellbogen).normalized())

        # Punkte, die überwiegend am Arm hängen, und wie stark
        am_arm = {}
        for v in teil.data.vertices:
            gewicht = sum(g.weight for g in v.groups if gruppen[g.group].startswith((f"upperarm_", f"lowerarm_")) and gruppen[g.group].endswith(f"_{seite}"))
            gewicht += sum(g.weight for g in v.groups if gruppen[g.group] == f"hand_{seite}")
            if gewicht > 0.05:
                am_arm[v.index] = gewicht

        # Abschnitte entlang des Arms: die obere Hälfte des Ärmels soll den Arm umschließen
        faecher = [[] for _ in range(ABSCHNITTE)]
        for index in am_arm:
            p = teil.data.vertices[index].co
            s = arm_lage(p)
            if 0.0 <= s <= gesamt:
                faecher[min(int(s / gesamt * ABSCHNITTE), ABSCHNITTE - 1)].append(p)

        ruecken = [None] * ABSCHNITTE
        for i, fach in enumerate(faecher):
            if len(fach) >= 6:
                oben = sorted(fach, key=lambda p: -p.z)[:max(3, len(fach) // 2)]
                mitte = sum(oben, Vector()) / len(oben)
                ruecken[i] = arm_punkt((i + 0.5) / ABSCHNITTE * gesamt) - mitte
                # Nur quer zum Arm rücken, die Länge des Ärmels bleibt
                achse = (ellbogen - schulter if i < ABSCHNITTE / 2 else hand - ellbogen).normalized()
                ruecken[i] -= achse * ruecken[i].dot(achse)
        bekannt = [i for i, r in enumerate(ruecken) if r is not None]
        for i in range(ABSCHNITTE):
            if ruecken[i] is None:
                ruecken[i] = ruecken[min(bekannt, key=lambda k: abs(k - i))].copy()
        for _ in range(2):
            ruecken = [(ruecken[max(i - 1, 0)] + ruecken[i] * 2 + ruecken[min(i + 1, ABSCHNITTE - 1)]) / 4 for i in range(ABSCHNITTE)]

        for index, gewicht in am_arm.items():
            v = teil.data.vertices[index]
            lage = np.clip(arm_lage(v.co) / gesamt * ABSCHNITTE - 0.5, 0, ABSCHNITTE - 1)
            links = int(lage)
            rechts = min(links + 1, ABSCHNITTE - 1)
            v.co += ruecken[links].lerp(ruecken[rechts], lage - links) * min(gewicht, 1.0)
        log(f"Ärmel {seite}: größter Versatz {max(r.length for r in ruecken):.1f} cm")


def schiebe_raus(teil, koerper):
    """Schiebt Punkte aus dem Körper nach außen; der Schub wird über die Kanten geglättet, damit keine Beulen bleiben."""
    bm_quellen = []
    for k in koerper:
        bm_quellen.append(k)
    punkte, flaechen = [], []
    for k in bm_quellen:
        versatz = len(punkte)
        punkte.extend(k.matrix_world @ v.co for v in k.data.vertices)
        flaechen.extend([versatz + i for i in p.vertices] for p in k.data.polygons)
    baum = BVHTree.FromPolygons(punkte, flaechen)

    n = len(teil.data.vertices)
    schub = np.zeros((n, 3))
    aktiv_punkte = 0
    for v in teil.data.vertices:
        ort, normale, _, _ = baum.find_nearest(v.co, REICHWEITE)
        if ort is None:
            continue
        abstand = (v.co - ort).dot(normale)
        if abstand < MARGE:
            schub[v.index] = np.array(normale) * (MARGE - abstand)
            aktiv_punkte += 1

    kanten = np.array([e.vertices for e in teil.data.edges])
    for _ in range(GLAETTEN):
        summe = np.zeros_like(schub)
        anzahl = np.zeros(n)
        np.add.at(summe, kanten[:, 0], schub[kanten[:, 1]])
        np.add.at(summe, kanten[:, 1], schub[kanten[:, 0]])
        np.add.at(anzahl, kanten[:, 0], 1)
        np.add.at(anzahl, kanten[:, 1], 1)
        mittel = summe / np.maximum(anzahl, 1)[:, None]
        # Der stärkere von eigenem und mittlerem Schub: so bleibt die Kleidung außen und läuft weich aus
        groesser = np.linalg.norm(mittel, axis=1) > np.linalg.norm(schub, axis=1)
        schub[groesser] = mittel[groesser]

    for v in teil.data.vertices:
        v.co += Vector(schub[v.index])
    log(f"nach außen geschoben: {aktiv_punkte} Punkte direkt, größter Schub {np.linalg.norm(schub, axis=1).max():.1f} cm")


def main(ordner, teil_name, kleidung, metahuman):
    name = os.path.basename(ordner.rstrip("\\/"))
    bpy.ops.wm.open_mainfile(filepath=os.path.join(ordner, "03_blender", f"{name}.blend"))
    arm_alt = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    teil = bpy.data.objects[f"SK_{name}_{teil_name}"]

    arm_neu, koerper = lade_metahuman(rf"C:\Projekte\Vael_Quellen\_Skelett\SKM_{metahuman}_Body.fbx")
    alt, neu = arm_alt.data.bones, arm_neu.data.bones
    groesse = max((k.matrix_world @ Vector(ecke)).z for k in koerper for ecke in k.bound_box)
    log(f"Skelett {len(alt)} -> {len(neu)} Knochen, Körper {sum(len(k.data.vertices) for k in koerper)} Punkte, {groesse:.1f} cm hoch")
    log(f"Kopf Mannequin {alt['head'].head_local.z:.1f}, MetaHuman {neu['head'].head_local.z:.1f} cm")

    biege(teil, alt, neu)
    aermel_auf_arme(teil, neu)
    schiebe_raus(teil, koerper)
    log(f"Kleidung reicht von {min(v.co.z for v in teil.data.vertices):.1f} bis {max(v.co.z for v in teil.data.vertices):.1f} cm")

    for mod in list(teil.modifiers):
        teil.modifiers.remove(mod)
    teil.parent = arm_neu
    mod = teil.modifiers.new("Armature", "ARMATURE")
    mod.object = arm_neu
    fertig = f"SK_{name}_{kleidung}_MH"
    teil.name = teil.data.name = fertig

    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ordner, "03_blender", f"{fertig}.blend"))
    exportiere(arm_neu, teil, os.path.join(ordner, "04_export", f"{fertig}.fbx"))


if __name__ == "__main__":
    main(*sys.argv[sys.argv.index("--") + 1:][:4])
