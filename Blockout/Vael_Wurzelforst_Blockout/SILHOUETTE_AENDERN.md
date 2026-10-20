# Silhouette der Urdrachin ändern

Die Form des Wurzelforsts ist **nicht** fest in Unreal eingebaut. Sie steht in `WF_Silhouette.json`, und alles andere wird daraus erzeugt: Höhenkarte, Zonenfarben und die Lage aller Platzhalter.

## Ablauf bei einer Änderung

1. **Form ändern** in `WF_Silhouette.json`. Die Koordinaten sind die Pixel des 2D-Kartenentwurfs; 1 Pixel = 2 m.
   - `kueste_wurzelforst`: Umriss von Rücken und Schwanz
   - `kiefer_sturmzinnen`: der Kiefer gegenüber den Fängen
   - `lagunen_pfuhlmoor`, `flachwasser_buchten`, `watt_die_faenge`: Wasserflächen
   - `gratlinie`, `kampagnenweg`: Grat und Weg

   Einfacher: Sagt Claude, was sich ändern soll, zum Beispiel „Schwanz 300 m länger“ oder „Hain breiter“. Claude passt die Datei an.
2. **Neu erzeugen:** Im Ordner `python wf_generator.py` ausführen. Beim ersten Mal vorher `pip install numpy scipy matplotlib pillow`, das kann auch Claude Code erledigen. In `WF_Vorschau.png` seht ihr sofort das Ergebnis.
3. **In Unreal aktualisieren:**
   - Landschaft austauschen: Entweder Modus „Landschaft“ › Verwalten › Werkzeug „Importieren“, die neue `WF_Hoehenkarte_2017.png` wählen und auf die bestehende Landschaft importieren. Oder die alte Landschaft löschen und neu importieren, mit denselben Werten 0/0/0 und 200/200/100.
   - Danach `wf_blockout.py` erneut ausführen. Es entfernt die alten Platzhalter und setzt sie passend zur neuen Form.

## Wichtig, solange die Grenzen nicht final sind

- **Nicht von Hand im Gelände modellieren.** Jeder neue Import überschreibt die Höhen in Unreal. Erst modellieren, wenn die Silhouette feststeht.
- Eigene, von Hand gesetzte Objekte ohne den Tag `WF_Blockout` bleiben erhalten. Nach einer Formänderung können sie aber im Wasser stehen.
- Feste Orte wie Urstamm, Eihöhle, Feenkreis und Nid-Quelle stehen in `wf_generator.py` im Abschnitt „Platzhalter“. Wandert die Küste stark, verschiebt man sie dort mit.
