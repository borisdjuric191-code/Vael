# Auftrag für Claude Code: Rankenluchs einrichten

Kopiere den Text unter der Linie in Claude Code. Vorher in Tripo das fertige Modell als **GLB** herunterladen, mit Texturen und PBR, und als `Rankenluchs_tripo.glb` in den Ordner `Desktop\Vael_Rankenluchs` legen.

---

Du hilfst beim Unreal-5.8-Projekt **Vael**, einem isometrischen Action-RPG. Die Oberfläche ist auf Deutsch. Ziel: Der **Rankenluchs**, ein Mischwesen aus Luchs und Pflanze aus der Region Wurzelforst, wird spielfertig. Er braucht ein Skelett, einen beweglichen Blätterkamm auf dem Rücken, eine Tarnpose, in der er wie ein Busch aussieht, und **ein** Material mit Licht- und Schattenform.

Ausgangsmodell: `%USERPROFILE%\Desktop\Vael_Rankenluchs\Rankenluchs_tripo.glb`. Es ist ein Tripo-Modell mit Quad-Topologie, 4K-PBR-Texturen und neutraler Standpose. Der Rücken ist bewusst glatt, ohne Kamm.

## Regeln

- Frag, bevor du Software installierst. Blender wird gebraucht, zum Beispiel per `winget install BlenderFoundation.Blender`.
- Arbeite mit Blender im Hintergrund per Python-Skript (`blender -b -P skript.py`). Leg alle Skripte unter `Desktop\Vael_Rankenluchs\skripte\` ab, damit man sie erneut ausführen kann.
- **Wichtig, Lizenz:** Megascans- oder andere Fab-Inhalte dürfen nicht in KI-Systeme gelangen. Lade sie also nicht in Blender und rendere sie nicht als Vorschau, die du ansiehst. Die Blätter für den Kamm baust du in Blender **prozedural** als eigene Geometrie. Die Megascans-Blatttextur wird erst in Unreal zugewiesen.
- In Unreal nur unter `/Game/Vael/Kreaturen/Rankenluchs/` anlegen. Nichts Bestehendes löschen oder verändern. Keine Commits ohne Rückfrage.
- Nach jedem größeren Schritt Vorschaubilder aus Blender rendern (Seite, Draufsicht schräg von oben wie die Spielkamera), selbst prüfen und mir zeigen.

## Schritte

1. **Vorbereiten:** GLB in Blender importieren. Ausrichten mit Kopf in +X wie in Unreal. Größe auf 0,70 m Schulterhöhe skalieren. Ursprung zwischen den Pfoten auf den Boden legen, Transformationen anwenden.
2. **Skelett für einen Vierbeiner** (Namen auf Deutsch oder Englisch, aber einheitlich):
   - Becken, 3 Wirbel, Hals, Kopf, Kiefer
   - 2 Knochen je Ohr
   - **Schweif mit 10 Knochen**, denn er ist sehr lang
   - je Bein Ober-, Unterschenkel, Mittelfuß und Pfote
   - Gewichte automatisch zuweisen, dann prüfen: Beine dürfen beim Beugen nicht in den Körper knicken, der Schweif muss sich weich biegen.
3. **Blätterkamm:**
   - Ein breites, lanzettförmiges Blatt prozedural modellieren: etwa 18 cm lang, 5 cm breit, leicht gewölbt, mit Mittelrippen-Knick.
   - UV-Layout so, dass V von der Basis (0) zur Spitze (1) läuft.
   - 10 Kopien vom Nacken bis zur Hüfte setzen, nach hinten oben geneigt. In der Mitte des Rückens am größten, vorn und hinten kleiner.
   - Jede Kopie bekommt einen eigenen Knochen (`kamm_01` bis `kamm_10`), am nächsten Wirbel befestigt. Starre Gewichte, 100 % auf den eigenen Knochen.
   - Eigene Materialposition `Kamm`.
4. **Augenleuchten:** Zwei kleine, flache Leucht-Körper exakt auf den Augen, am Kopfknochen befestigt, mit eigener Materialposition `Augen`. Position anhand der Textur und der Vorschaubilder finden und mir zur Bestätigung zeigen.
5. **Animationen** als einzelne Actions:
   - `Stehen_Idle`: Atmen, leichtes Schweifpendeln, Ohrzucken, 4 s, Schleife.
   - `Tarn_Ruhe`: Der Luchs liegt flach, Beine untergeschlagen, Kopf zwischen den Pfoten, Schweif um den Körper gelegt. **Der Kamm fächert sich nach allen Seiten auf**, sodass ein Busch entsteht. Ganz leichtes Atmen, Schleife.
   - `Erwachen`: 1,0 s von Tarn_Ruhe zu Stehen. Der Kamm klappt an den Rücken, der Kopf hebt sich zuerst.
   - `Sprung_Angriff`: 0,8 s, Ansprung nach vorn.
   - `Laufen` und `Rennen` als Schleifen.
6. **Export:** FBX mit Skelett, Mesh und allen Actions nach `Desktop\Vael_Rankenluchs\export\`. Achse und Skalierung für Unreal einstellen.
7. **Unreal-Import** per Python nach `/Game/Vael/Kreaturen/Rankenluchs/`: Skeletal Mesh, Skelett, Animationen und Texturen.
8. **Material mit Licht- und Schattenform**, per Python in Unreal gebaut:
   - `M_Rankenluchs` mit Skalar-Parameter **`Verderbnis`** (0 bis 1). Auf der Tripo-Textur aufbauen (BaseColor, Normal, Roughness).
   - Bei Verderbnis 1: Fell entsättigt und dunkel aschgrau. Grüne Bereiche, also Moos und Blätter (Maske, wo Grün stärker ist als Rot und Blau), werden schwarzviolett. Dazu feine violett leuchtende Adern (Emissive, über ein prozedurales Riss- oder Voronoi-Muster).
   - `M_Rankenkamm` mit demselben Parameter: Licht = sattes Grün mit Adern, Schatten = schwarzviolett mit **tiefroten Spitzen** über die V-Koordinate. Einen Textur-Parameter `Blatt_Textur` vorsehen, in den ich später selbst eine Megascans-Blatttextur einsetze.
   - `M_Rankenluchs_Augen`: Emissive. Licht = Bernstein, Schatten = Violett, ebenfalls über `Verderbnis`.
   - Materialinstanzen: `MI_Rankenluchs_Licht` (Verderbnis 0) und `MI_Rankenluchs_Schatten` (Verderbnis 1), jeweils auch für Kamm und Augen.
9. **Melde am Ende:**
   - Polygonzahl, Knochenzahl und Liste der Animationen
   - Vorschaubilder: Stehen, Tarnpose, beide Materialformen (Blender-Render ohne Megascans-Textur)
   - Was noch offen ist oder unsauber aussieht

Die Spiel-Logik (Tarnung, Erwachen bei Annäherung, Rudel zu dritt) noch **nicht** einbauen. Schlage mir zuerst vor, wie sie in das bestehende Kreaturen-System des Projekts passt.
