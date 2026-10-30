"""
Vael – Kreaturenfabrik, Kleidung: spitze Lederstiefel (Blender)
===============================================================

Baut ein Paar Stiefel für einen MetaHuman ohne Tripo: Ringe laufen von der Wade über den Knöchel bis in eine aufgebogene Spitze.
Jeder Ring misst den Körper dort aus, so passt der Stiefel genau; tiefe, schiefe Knautschfalten machen ihn zu altem, weichem Leder.
Dazu entstehen die Texturen (Farbe, Normal, Rauheit) mit Flicken und Nähten direkt aus Rauschen und denselben Falten.

Ergebnis: 03_blender/SK_<Name>_Stiefel_MH.blend, 04_export/SK_<Name>_Stiefel_MH.fbx und T_Stiefel_basecolor|normal|roughness.png

Aufruf:
  blender -b -P vael_stiefel_bauen.py -- "C:\\Projekte\\Vael_Quellen\\MagierMann" MH_Magier
"""
import bpy, bmesh, math, os, sys
import numpy as np
from mathutils import Vector

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from vael_mensch_aufbereiten import CM, aktiv, exportiere, log
from vael_kleidung_anpassen import lade_metahuman

PUNKTE = 72             # Punkte je Ring
RINGE_SCHAFT = 46
RINGE_FUSS = 44
RINGE_SPITZE = 22
RINGE_RAND = 3          # nach innen umgeschlagener Rand oben

HOEHE_SCHAFT = 0.46     # Anteil der Wade, den der Schaft bedeckt
ABSTAND = 1.0           # Luft zur Haut in Zentimetern
SOHLE = 0.7             # So weit reicht die Sohle unter den Fuß
SPITZE_LAENGE = 11.0    # So weit läuft die Spitze über die Zehen hinaus
SPITZE_HOCH = 6.5       # So hoch biegt sie sich auf

GROESSE = 1024          # Kantenlänge der Texturen

# Knautschfalten: Lage entlang des Stiefels (0 oben, 1 Spitze), Tiefe in Zentimetern, Schiefe, Wellen um den Ring
FALTEN = ((.14, 1.0, .05, 1), (.27, 1.5, -.06, 2), (.38, 1.25, .08, 1), (.47, 1.0, -.04, 2), (.62, .55, .03, 1), (.78, .5, -.03, 1), (.9, .55, .02, 1))


def bezier(p0, p1, p2, t):
    return p0 * (1 - t) ** 2 + p1 * 2 * t * (1 - t) + p2 * t * t


def bahn(fuss, ball, wade, boden):
    """Die Mittellinie des Stiefels von der Wade bis zur Spitze, mit Länge entlang des Wegs."""
    vor = Vector((ball.x - fuss.x, ball.y - fuss.y, 0)).normalized()
    oben = fuss.lerp(wade, HOEHE_SCHAFT)
    knoechel = fuss + Vector((0, 0, 2.0))
    fuss_mitte = Vector((*(ball - vor * 2.5).xy, boden + 3.8))
    spitze_mitte = Vector((*(ball + vor * (SPITZE_LAENGE + 7.0)).xy, boden + SPITZE_HOCH))
    punkte = []
    for i in range(RINGE_SCHAFT):
        punkte.append(oben.lerp(knoechel, i / RINGE_SCHAFT))
    for i in range(RINGE_FUSS):
        punkte.append(bezier(knoechel, Vector((*(fuss - vor * .6).xy, boden + 3.5)), fuss_mitte, i / RINGE_FUSS))
    wende = ball + vor * 5.0 + Vector((0, 0, boden + 2.6 - ball.z))
    for i in range(RINGE_SPITZE + 1):
        punkte.append(bezier(fuss_mitte, wende, spitze_mitte, i / RINGE_SPITZE))
    return punkte, vor


def rahmen(punkte, vor):
    """Zu jedem Ring: Tangente, Seitenachse und die Achse, die am Fuß nach oben zeigt."""
    seite = vor.cross(Vector((0, 0, 1))).normalized()
    tangenten = []
    for i in range(len(punkte)):
        tangenten.append((punkte[min(i + 1, len(punkte) - 1)] - punkte[max(i - 1, 0)]).normalized())
    for _ in range(6):
        tangenten = [(tangenten[max(i - 1, 0)] + tangenten[i] * 2 + tangenten[min(i + 1, len(tangenten) - 1)]).normalized() for i in range(len(tangenten))]
    return [(t, seite, t.cross(seite).normalized()) for t in tangenten]


def miss_koerper(punkte, rahmen_, verts):
    """Halbachsen und Versatz jedes Rings aus den Körperpunkten nahe seiner Ebene."""
    werte = []
    for mitte, (t, s, h) in zip(punkte, rahmen_):
        d = verts - np.array(mitte)
        nah = np.abs(d @ np.array(t)) < 2.6
        nah &= np.linalg.norm(d, axis=1) < 16.0
        if nah.sum() >= 6:
            x, y = d[nah] @ np.array(s), d[nah] @ np.array(h)
            werte.append(((x.max() + x.min()) / 2, (y.max() + y.min()) / 2, (x.max() - x.min()) / 2, (y.max() - y.min()) / 2))
        else:
            werte.append(None)
    bekannt = [i for i, w in enumerate(werte) if w is not None]
    letzter = bekannt[-1]
    for i, w in enumerate(werte):
        if w is None:
            q = werte[min(bekannt, key=lambda k: abs(k - i))]
            if i > letzter:     # über die Zehen hinaus läuft alles zur Spitze aus
                rest = 1.0 - (i - letzter) / (len(werte) - letzter)
                q = (q[0] * rest, q[1] * rest, q[2] * rest ** .85 + .15, q[3] * rest ** .85 + .15)
            werte[i] = q
    w = np.array(werte)
    for _ in range(4):
        w[1:-1] = (w[:-2] + 2 * w[1:-1] + w[2:]) / 4
    return w


def falte(t, theta, seite_phase):
    """Verschiebung in Zentimetern: schiefe Rillen mit Wülsten daneben, auf beiden Stiefeln etwas verschieden."""
    wert = 0.0
    for lage, tiefe, schief, wellen in FALTEN:
        mitte = lage + schief * math.sin(theta + seite_phase) + .018 * math.sin(theta * wellen * 2 + lage * 9 + seite_phase)
        x = (t - mitte) / .024
        wert += tiefe * (.75 * math.exp(-((x - 1.7) ** 2) / 2.2) - math.exp(-x * x))
    return wert


def falten_bild(u, v, seite_phase):
    """Dasselbe wie falte(), auf das Texturgitter gerechnet."""
    theta = u * math.tau
    wert = np.zeros_like(u)
    for lage, tiefe, schief, wellen in FALTEN:
        mitte = lage + schief * np.sin(theta + seite_phase) + .018 * np.sin(theta * wellen * 2 + lage * 9 + seite_phase)
        x = (v - mitte) / .024
        wert += tiefe * (.75 * np.exp(-((x - 1.7) ** 2) / 2.2) - np.exp(-x * x))
    return wert


def baue_stiefel(mesh_bm, uv, gruppe_ringe, punkte, rahmen_, masse, boden, seite_phase):
    """Legt die Ringe an und verbindet sie; gibt je Punkt seine Lage entlang des Stiefels zurück."""
    n = len(punkte)
    ringe = []
    lagen = {}

    def ring(i, zusatz, v_tex):
        t = i / (n - 1)
        mitte, (_, s, h) = punkte[i], rahmen_[i]
        cx, cy, a, b = masse[i]
        punkte_ring = []
        for k in range(PUNKTE + 1):
            theta = k % PUNKTE / PUNKTE * math.tau
            # Der Rand oben ist nicht gerade: das Leder sackt auf einer Seite ab
            versatz_rand = max(0.0, 1.0 - i / 9.0) * (2.2 * math.sin(theta + seite_phase) + 1.0 * math.sin(2 * theta))
            r = ABSTAND + falte(t, theta, seite_phase) * (1.0 if i < n - RINGE_SPITZE // 2 else .4) + zusatz
            r = max(r, .35 if i < n - 4 else 0.0)
            schmal = 1.0 if i < n - 8 else (n - 1 - i) / 7 + .12
            p = mitte + s * (cx + (a + r) * math.cos(theta) * schmal) + h * (cy + (b + r) * math.sin(theta) * schmal)
            p += Vector((0, 0, versatz_rand))
            if i >= RINGE_SCHAFT:
                p.z = max(p.z, boden - SOHLE)
            v = mesh_bm.verts.new(p)
            lagen[v] = i
            punkte_ring.append(v)
        ringe.append((punkte_ring, v_tex))

    for i in range(RINGE_RAND):
        ring(0, -.45, -.02 * (RINGE_RAND - i) / RINGE_RAND)
    for i in range(n):
        ring(i, 0.0, i / (n - 1))

    for (unten, v0), (oben, v1) in zip(ringe, ringe[1:]):
        for k in range(PUNKTE):
            f = mesh_bm.faces.new((unten[k], unten[k + 1], oben[k + 1], oben[k]))
            for loop, (uu, vv) in zip(f.loops, ((k / PUNKTE, v0), ((k + 1) / PUNKTE, v0), ((k + 1) / PUNKTE, v1), (k / PUNKTE, v1))):
                loop[uv].uv = (uu, 1.0 - max(vv, 0.0))
    # Die Spitze schließt in einem Punkt
    ende = mesh_bm.verts.new(punkte[-1])
    lagen[ende] = n - 1
    letzter, v1 = ringe[-1]
    for k in range(PUNKTE):
        f = mesh_bm.faces.new((letzter[k], letzter[k + 1], ende))
        for loop in f.loops:
            loop[uv].uv = (k / PUNKTE, 0.0)
    return lagen


def gewichte(ring):
    """Knochengewichte nach dem Ring: Wade, Knöchel, Fuß, Ballen und Spitze."""
    knoechel = RINGE_SCHAFT - 6
    fuss = RINGE_SCHAFT + 10
    ballen_a = RINGE_SCHAFT + RINGE_FUSS - 8
    ballen_b = RINGE_SCHAFT + RINGE_FUSS + 2
    if ring <= knoechel:
        return (("calf", 1.0),)
    if ring < fuss:
        t = (ring - knoechel) / (fuss - knoechel)
        return (("calf", 1 - t), ("foot", t))
    if ring <= ballen_a:
        return (("foot", 1.0),)
    if ring < ballen_b:
        t = (ring - ballen_a) / (ballen_b - ballen_a)
        return (("foot", 1 - t), ("ball", t))
    return (("ball", 1.0),)


def rauschen(rng, steil):
    """Kachelbares Rauschen: gefiltertes Weißrauschen, je steiler desto grober."""
    fx = np.fft.fftfreq(GROESSE)[:, None]
    fy = np.fft.fftfreq(GROESSE)[None, :]
    f = np.sqrt(fx * fx + fy * fy)
    f[0, 0] = 1.0
    bild = np.fft.ifft2(np.fft.fft2(rng.normal(size=(GROESSE, GROESSE))) / f ** steil).real
    return (bild - bild.mean()) / bild.std()


def speichere(pfad, daten, srgb):
    h, w = daten.shape[:2]
    bild = bpy.data.images.new(os.path.basename(pfad), w, h, alpha=True, float_buffer=False)
    if not srgb:
        bild.colorspace_settings.name = "Non-Color"
    rgba = np.ones((h, w, 4), dtype=np.float32)
    rgba[..., :3] = np.clip(daten, 0, 1)
    bild.pixels.foreach_set(rgba[::-1].ravel())
    bild.filepath_raw = pfad
    bild.file_format = "PNG"
    bild.save()


def texturen(ordner):
    """Altes braunes Leder: Flecken, helle Kanten an den Faltenrücken, dunkle Rillen, Flicken mit Naht, dunkle Sohle."""
    rng = np.random.default_rng(7)
    v, u = np.mgrid[0:GROESSE, 0:GROESSE] / GROESSE
    grob, mittel, fein = rauschen(rng, 1.6), rauschen(rng, 1.1), rauschen(rng, .55)
    fein_kurz = np.clip(fein, -2, 2)

    wulst = falten_bild(u, v, 0.0)
    wulst += falten_bild(u, v, 2.0) * .5
    rille = np.clip(-wulst, 0, 1.5) / 1.5
    ruecken = np.clip(wulst, 0, 1.0)

    farbe = np.array((.112, .062, .034)) * (1.0 + .28 * grob[..., None] + .14 * mittel[..., None])
    farbe = farbe * (1.0 - .62 * rille[..., None]) + np.array((.19, .125, .075)) * (.5 * ruecken[..., None])
    hoehe = .9 * wulst + .10 * mittel + .05 * fein_kurz

    # Flicken: etwas helleres Leder mit dunkler Kante und Stichen
    for (mu, mv, bu, bv) in ((.10, .22, .07, .07), (.46, .36, .055, .09), (.62, .72, .06, .05), (.30, .08, .05, .045)):
        du = np.abs((u - mu + .5) % 1.0 - .5) / bu
        dv = np.abs(v - mv) / bv
        kasten = np.maximum(du, dv)
        drin = kasten < 1.0
        farbe[drin] = farbe[drin] * .55 + np.array((.17, .10, .055)) * (1.0 + .2 * grob[drin][:, None]) * .45
        kante = (kasten > .97) & (kasten < 1.05)
        farbe[kante] *= .35
        hoehe[drin] += .55
        stiche = (kasten > .86) & (kasten < .92) & (np.sin((u + v) * 340.0) > .35)
        farbe[stiche] = np.array((.62, .52, .36))
        hoehe[stiche] += .8

    # Die Naht über den Schaft und die Sohle: u 0,25 liegt am Fuß unten, u 0,75 oben
    naht = (np.abs(u - .75) < .004) & (np.sin(v * 420.0) > .1)
    farbe[naht] = np.array((.42, .35, .24))
    sohle = (np.abs(u - .25) < .17) & (v > .50)
    farbe[sohle] = np.array((.035, .026, .021)) * (1.0 + .3 * mittel[sohle][:, None])
    rand_sohle = (np.abs(np.abs(u - .25) - .17) < .006) & (v > .50) & (np.sin(v * 380.0) > 0)
    farbe[rand_sohle] = np.array((.40, .33, .23))

    dy, dx = np.gradient(hoehe)
    staerke = 9.0
    normal = np.dstack((-dx * staerke, dy * staerke, np.ones_like(hoehe)))
    normal /= np.linalg.norm(normal, axis=2, keepdims=True)
    rauh = np.clip(.86 + .07 * mittel - .10 * ruecken, .6, 1.0)

    export = os.path.join(ordner, "04_export")
    speichere(os.path.join(export, "T_Stiefel_basecolor.png"), np.clip(farbe, 0, 1) ** (1 / 2.2), True)
    speichere(os.path.join(export, "T_Stiefel_normal.png"), normal * .5 + .5, False)
    speichere(os.path.join(export, "T_Stiefel_roughness.png"), np.dstack((rauh, rauh, rauh)), False)


def main(ordner, metahuman):
    name = os.path.basename(ordner.rstrip("\\/"))
    fertig = f"SK_{name}_Stiefel_MH"
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.scale_length = 1.0 / CM

    arm, koerper = lade_metahuman(rf"C:\Projekte\Vael_Quellen\_Skelett\SKM_{metahuman}_Body.fbx")
    verts = np.array([k.matrix_world @ v.co for k in koerper for v in k.data.vertices])
    bones = arm.data.bones

    mesh = bpy.data.meshes.new(fertig)
    obj = bpy.data.objects.new(fertig, mesh)
    bpy.context.collection.objects.link(obj)
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")

    zuordnung = []
    for seite, phase in (("l", .0), ("r", 2.0)):
        fuss, ball, wade = (bones[f"{k}_{seite}"].head_local.copy() for k in ("foot", "ball", "calf"))
        nah = verts[np.linalg.norm(verts[:, :2] - np.array(fuss.xy), axis=1) < 22.0]
        boden = float(nah[:, 2].min())
        punkte, vor = bahn(fuss, ball, wade, boden)
        rahmen_ = rahmen(punkte, vor)
        masse = miss_koerper(punkte, rahmen_, verts)
        lagen = baue_stiefel(bm, uv, None, punkte, rahmen_, masse, boden, phase)
        zuordnung.append((seite, lagen))
        log(f"Stiefel {seite}: Boden {boden:.1f}, Wade oben {punkte[0].z:.1f}, Spitze {punkte[-1].x:.1f} {punkte[-1].y:.1f} {punkte[-1].z:.1f}, Ringe {len(punkte)}")

    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    # Die Punkte bekommen ihre Nummern erst beim Schreiben
    bm.verts.index_update()
    verteilung = [(v.index, seite, ring) for seite, lagen in zuordnung for v, ring in lagen.items()]
    bm.to_mesh(mesh)
    bm.free()

    for p in mesh.polygons:
        p.use_smooth = True

    for index, seite, ring in verteilung:
        for knochen, gewicht in gewichte(ring):
            gruppe_name = f"{knochen}_{seite}"
            gruppe = obj.vertex_groups.get(gruppe_name) or obj.vertex_groups.new(name=gruppe_name)
            gruppe.add([index], gewicht, "REPLACE")

    mat = bpy.data.materials.new("M_Stiefel")
    mesh.materials.append(mat)
    obj.parent = arm
    mod = obj.modifiers.new("Armature", "ARMATURE")
    mod.object = arm

    for k in koerper:
        bpy.data.objects.remove(k, do_unlink=True)
    texturen(ordner)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ordner, "03_blender", f"{fertig}.blend"))
    log(f"{fertig}: {len(mesh.vertices)} Punkte, {len(mesh.polygons)} Flächen")
    exportiere(arm, obj, os.path.join(ordner, "04_export", f"{fertig}.fbx"))


if __name__ == "__main__":
    main(*sys.argv[sys.argv.index("--") + 1:][:2])
