"""
Vael – Kreaturenfabrik, Kleidung: verdeckten Körper entfernen (Blender)
=======================================================================

Löscht am Körper eines MetaHuman, was unter einem Kleidungsstück liegt; so kann bei keiner Bewegung Haut durch den Stoff stechen.
Von jedem Punkt gehen fünf Strahlen schräg nach außen. Nur wenn alle fünf die Kleidung treffen, gilt er als verdeckt:
an Kragen, Ärmelsaum und Schlitz bleibt so von selbst ein Rand stehen. Gelöscht werden Flächen, deren Punkte alle verdeckt sind.

Vorlagen sind die Dateien SK_<Name>_<Teil>_MH.blend, mehrere Teile mit Komma. Ergebnis: 04_export/SK_<MetaHuman>_Koerper_<Teil>_<Teil>.fbx

Aufruf:
  blender -b -P vael_koerper_verdecken.py -- "C:\\Projekte\\Vael_Quellen\\MagierMann" Robe,Stiefel MH_Magier
"""
import bpy, bmesh, math, os, sys
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from vael_mensch_aufbereiten import CM, aktiv, exportiere, log
from vael_kleidung_anpassen import lade_metahuman

# Wie weit die Strahlen reichen (Zentimeter) und wie schräg die vier äußeren zur Hautnormale stehen (Grad)
REICHWEITE = 30.0
SCHRAEG = 40.0

# Ein Punkt gilt als Insel, wenn so viele seiner Nachbarn verdeckt sind; so oft wird nachgefasst
INSEL_ANTEIL = 0.6
INSEL_RUNDEN = 3


def strahlen(normale):
    """Die Normale und vier Richtungen schräg um sie herum."""
    seite = normale.orthogonal().normalized()
    richtungen = [normale]
    for winkel in (0.0, 90.0, 180.0, 270.0):
        schraeg = Matrix.Rotation(math.radians(SCHRAEG), 3, seite) @ normale
        richtungen.append(Matrix.Rotation(math.radians(winkel), 3, normale) @ schraeg)
    return richtungen


def main(ordner, kleidung, metahuman):
    name = os.path.basename(ordner.rstrip("\\/"))

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.scale_length = 1.0 / CM
    punkte, flaechen = [], []
    for stueck in kleidung.split(","):
        fertig = f"SK_{name}_{stueck}_MH"
        with bpy.data.libraries.load(os.path.join(ordner, "03_blender", f"{fertig}.blend")) as (quelle, ziel):
            ziel.meshes = [fertig]
        versatz = len(punkte)
        punkte.extend(v.co.copy() for v in ziel.meshes[0].vertices)
        flaechen.extend([versatz + i for i in p.vertices] for p in ziel.meshes[0].polygons)
    baum = BVHTree.FromPolygons(punkte, flaechen)

    arm, koerper = lade_metahuman(rf"C:\Projekte\Vael_Quellen\_Skelett\SKM_{metahuman}_Body.fbx")
    if len(koerper) != 1:
        raise RuntimeError(f"Erwartet ein Körperobjekt, gefunden {len(koerper)}")
    koerper = koerper[0]

    verdeckt = [all(baum.ray_cast(v.co + r * .2, r, REICHWEITE)[0] is not None for r in strahlen(v.normal.normalized())) for v in koerper.data.vertices]

    aktiv(koerper)
    bpy.ops.object.mode_set(mode="EDIT")
    bm = bmesh.from_edit_mesh(koerper.data)
    bm.verts.ensure_lookup_table()

    # Kleine Löcher im Stoff lassen einzelne Strahlen durch: Inseln mitten im Verdeckten gelten mit als verdeckt
    for _ in range(INSEL_RUNDEN):
        neu = list(verdeckt)
        for v in bm.verts:
            nachbarn = [e.other_vert(v).index for e in v.link_edges]
            if not verdeckt[v.index] and nachbarn and sum(verdeckt[n] for n in nachbarn) / len(nachbarn) >= INSEL_ANTEIL:
                neu[v.index] = True
        verdeckt = neu

    weg = [f for f in bm.faces if all(verdeckt[v.index] for v in f.verts)]
    gesamt = len(bm.faces)
    bmesh.ops.delete(bm, geom=weg, context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bmesh.update_edit_mesh(koerper.data)
    bpy.ops.object.mode_set(mode="OBJECT")
    log(f"verdeckt: {sum(verdeckt)} von {len(verdeckt)} Punkten, {len(weg)} von {gesamt} Flächen gelöscht")

    koerper.parent = arm
    mod = koerper.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    ergebnis = f"SK_{metahuman}_Koerper_{kleidung.replace(',', '_')}"
    koerper.name = koerper.data.name = ergebnis
    exportiere(arm, koerper, os.path.join(ordner, "04_export", f"{ergebnis}.fbx"))


if __name__ == "__main__":
    main(*sys.argv[sys.argv.index("--") + 1:][:3])
